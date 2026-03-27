#include "tool_modules.h"

#include <errno.h>
#include <string.h>

#include "edt_time.h"

#if defined(__linux__)
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#if defined(__linux__)
static void edt_net_update_stats(edt_net_capture_t *capture, const uint8_t *packet, size_t len)
{
    const struct ethhdr *eth;
    uint16_t eth_type;

    if (len < sizeof(struct ethhdr)) {
        return;
    }

    eth = (const struct ethhdr *)packet;
    eth_type = ntohs(eth->h_proto);

    pthread_mutex_lock(&capture->lock);
    capture->stats.timestamp_ns = edt_now_ns();
    capture->stats.packets_total++;
    capture->stats.bytes_total += len;

    if (eth_type == ETH_P_ARP) {
        capture->stats.arp_packets++;
    } else if (eth_type == ETH_P_IP) {
        const struct iphdr *ip;
        capture->stats.ipv4_packets++;
        if (len >= sizeof(struct ethhdr) + sizeof(struct iphdr)) {
            ip = (const struct iphdr *)(packet + sizeof(struct ethhdr));
            switch (ip->protocol) {
            case IPPROTO_TCP:
                capture->stats.tcp_packets++;
                break;
            case IPPROTO_UDP:
                capture->stats.udp_packets++;
                break;
            case IPPROTO_ICMP:
                capture->stats.icmp_packets++;
                break;
            default:
                capture->stats.other_packets++;
                break;
            }
        }
    } else {
        capture->stats.other_packets++;
    }
    pthread_mutex_unlock(&capture->lock);
}

static void *edt_net_capture_thread(void *arg)
{
    edt_net_capture_t *capture = (edt_net_capture_t *)arg;
    uint8_t buffer[2048];

    while (atomic_load_explicit(&capture->running, memory_order_acquire) != 0) {
        ssize_t len = recv(capture->socket_fd, buffer, sizeof(buffer), 0);
        if (len <= 0) {
            if (errno == EINTR) {
                continue;
            }
            if (atomic_load_explicit(&capture->running, memory_order_acquire) == 0) {
                break;
            }
            continue;
        }
        edt_net_update_stats(capture, buffer, (size_t)len);
    }
    return NULL;
}
#endif

int edt_net_capture_init(edt_net_capture_t *capture, const edt_net_capture_config_t *config)
{
    if (!capture) {
        return -EINVAL;
    }
    memset(capture, 0, sizeof(*capture));
    capture->config.interface_name = (config && config->interface_name) ? config->interface_name : "eth0";
    capture->config.promiscuous = config ? config->promiscuous : 0;
    capture->socket_fd = -1;
    if (pthread_mutex_init(&capture->lock, NULL) != 0) {
        return -errno;
    }
    atomic_store(&capture->running, 0);
    return 0;
}

int edt_net_capture_start(edt_net_capture_t *capture)
{
#if !defined(__linux__)
    (void)capture;
    return -ENOSYS;
#else
    struct sockaddr_ll sll;
    unsigned int if_index;

    if (!capture) {
        return -EINVAL;
    }
    if (atomic_load(&capture->running) != 0) {
        return 0;
    }

    capture->socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (capture->socket_fd < 0) {
        return -errno;
    }

    if_index = if_nametoindex(capture->config.interface_name);
    if (if_index == 0U) {
        int err = errno;
        (void)close(capture->socket_fd);
        capture->socket_fd = -1;
        return -err;
    }

    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = (int)if_index;
    if (bind(capture->socket_fd, (const struct sockaddr *)&sll, sizeof(sll)) != 0) {
        int err = errno;
        (void)close(capture->socket_fd);
        capture->socket_fd = -1;
        return -err;
    }

    if (capture->config.promiscuous) {
        struct packet_mreq mr;
        memset(&mr, 0, sizeof(mr));
        mr.mr_ifindex = (int)if_index;
        mr.mr_type = PACKET_MR_PROMISC;
        (void)setsockopt(capture->socket_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr));
    }

    atomic_store(&capture->running, 1);
    if (pthread_create(&capture->thread, NULL, edt_net_capture_thread, capture) != 0) {
        int err = errno;
        atomic_store(&capture->running, 0);
        (void)close(capture->socket_fd);
        capture->socket_fd = -1;
        return -err;
    }
    return 0;
#endif
}

int edt_net_capture_stop(edt_net_capture_t *capture)
{
    if (!capture) {
        return -EINVAL;
    }
    if (atomic_load(&capture->running) == 0) {
        return 0;
    }
    atomic_store(&capture->running, 0);
#if defined(__linux__)
    if (capture->socket_fd >= 0) {
        (void)shutdown(capture->socket_fd, SHUT_RDWR);
        (void)close(capture->socket_fd);
        capture->socket_fd = -1;
    }
    (void)pthread_join(capture->thread, NULL);
#endif
    return 0;
}

void edt_net_capture_destroy(edt_net_capture_t *capture)
{
    if (!capture) {
        return;
    }
    (void)edt_net_capture_stop(capture);
    (void)pthread_mutex_destroy(&capture->lock);
}

int edt_net_capture_get_stats(edt_net_capture_t *capture, edt_net_capture_stats_t *out_stats)
{
    if (!capture || !out_stats) {
        return -EINVAL;
    }
    pthread_mutex_lock(&capture->lock);
    *out_stats = capture->stats;
    pthread_mutex_unlock(&capture->lock);
    return 0;
}
