#include "media_player.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "hub_log.h"

static void reap_if_exited(hub_media_player_t *mp)
{
    int status = 0;
    pid_t ret;

    if (!mp || mp->pid <= 0) {
        return;
    }

    ret = waitpid(mp->pid, &status, WNOHANG);
    if (ret == mp->pid) {
        mp->pid = -1;
    }
}

int hub_media_player_init(hub_media_player_t *mp, const char *default_file)
{
    if (!mp) {
        return -1;
    }

    memset(mp, 0, sizeof(*mp));
    mp->pid = -1;
    pthread_mutex_init(&mp->lock, NULL);

    if (default_file && default_file[0]) {
        snprintf(mp->default_file, sizeof(mp->default_file), "%s", default_file);
    }
    return 0;
}

void hub_media_player_deinit(hub_media_player_t *mp)
{
    if (!mp) {
        return;
    }

    (void)hub_media_player_stop(mp);
    pthread_mutex_destroy(&mp->lock);
}

int hub_media_player_stop(hub_media_player_t *mp)
{
    if (!mp) {
        return -1;
    }

    pthread_mutex_lock(&mp->lock);
    reap_if_exited(mp);
    if (mp->pid > 0) {
        (void)kill(mp->pid, SIGTERM);
        (void)waitpid(mp->pid, NULL, 0);
        mp->pid = -1;
    }
    pthread_mutex_unlock(&mp->lock);
    return 0;
}

int hub_media_player_play(hub_media_player_t *mp, const char *file)
{
    const char *play_file;
    pid_t pid;

    if (!mp) {
        return -1;
    }

    play_file = (file && file[0]) ? file : mp->default_file;
    if (!play_file || !play_file[0]) {
        HUB_LOGW("media play requested but no file configured");
        return -1;
    }

    pthread_mutex_lock(&mp->lock);
    reap_if_exited(mp);
    if (mp->pid > 0) {
        (void)kill(mp->pid, SIGTERM);
        (void)waitpid(mp->pid, NULL, 0);
        mp->pid = -1;
    }

    pid = fork();
    if (pid < 0) {
        pthread_mutex_unlock(&mp->lock);
        HUB_LOGE("fork for media play failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        execlp("aplay", "aplay", "-q", play_file, (char *)NULL);
        _exit(127);
    }

    mp->pid = pid;
    pthread_mutex_unlock(&mp->lock);
    HUB_LOGI("media play started: %s (pid=%d)", play_file, (int)pid);
    return 0;
}

