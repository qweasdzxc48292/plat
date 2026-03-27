#include "tool_modules.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "edt_time.h"

#if defined(__linux__)
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

int edt_v4l2_probe(const char *device, edt_v4l2_probe_result_t *result)
{
    if (!device || !result) {
        return -EINVAL;
    }
    memset(result, 0, sizeof(*result));
    result->timestamp_ns = edt_now_ns();
    (void)snprintf(result->device, sizeof(result->device), "%s", device);

#if !defined(__linux__)
    return -ENOSYS;
#else
    {
        int fd = open(device, O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            return -errno;
        }

        struct v4l2_capability cap;
        memset(&cap, 0, sizeof(cap));
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) != 0) {
            int err = errno;
            (void)close(fd);
            return -err;
        }

        result->available = 1;
        result->capabilities = cap.capabilities;
        (void)snprintf(result->driver, sizeof(result->driver), "%s", cap.driver);
        (void)snprintf(result->card, sizeof(result->card), "%s", cap.card);
        (void)close(fd);
    }
    return 0;
#endif
}

int edt_v4l2_dump_formats(const char *device, char *buffer, size_t buffer_size)
{
    if (!device || !buffer || buffer_size == 0U) {
        return -EINVAL;
    }

#if !defined(__linux__)
    (void)snprintf(buffer, buffer_size, "V4L2 format listing is only supported on Linux.");
    return -ENOSYS;
#else
    int fd;
    struct v4l2_fmtdesc fmtdesc;
    size_t pos = 0U;

    fd = open(device, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        int err = errno;
        (void)snprintf(buffer, buffer_size, "open(%s) failed: %s", device, strerror(err));
        return -err;
    }

    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0) {
        int written = snprintf(buffer + pos,
                               buffer_size - pos,
                               "#%u fourcc=0x%08X desc=%s\n",
                               fmtdesc.index,
                               fmtdesc.pixelformat,
                               fmtdesc.description);
        if (written < 0) {
            break;
        }
        if ((size_t)written >= buffer_size - pos) {
            pos = buffer_size - 1U;
            break;
        }
        pos += (size_t)written;
        fmtdesc.index++;
    }
    if (pos == 0U) {
        (void)snprintf(buffer, buffer_size, "No capture formats found on %s", device);
    }
    (void)close(fd);
    return 0;
#endif
}
