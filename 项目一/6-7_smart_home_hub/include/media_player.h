#ifndef MEDIA_PLAYER_H
#define MEDIA_PLAYER_H

#include <pthread.h>
#include <sys/types.h>

typedef struct hub_media_player {
    pthread_mutex_t lock;
    pid_t pid;
    char default_file[256];
} hub_media_player_t;

int hub_media_player_init(hub_media_player_t *mp, const char *default_file);
void hub_media_player_deinit(hub_media_player_t *mp);

int hub_media_player_play(hub_media_player_t *mp, const char *file);
int hub_media_player_stop(hub_media_player_t *mp);

#endif /* MEDIA_PLAYER_H */

