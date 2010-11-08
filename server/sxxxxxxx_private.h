#ifndef sxxxxxxx_private_h
#define sxxxxxxx_private_h

#include "sxxxxxxx.h"

#include <stdlib.h>

#include <pthread.h>
#include <libspotify/api.h>
#include "server.h"

#include "audio.h"

typedef enum {
	STOPPED,
	PAUSED,
	LOADING,
	BUFFERING,
	PLAYING,
} sxxxxxxx_state;

typedef struct monitor_list_item monitor_list_item;

struct monitor_list_item {
	client *c;
	monitor_list_item *previous;
	monitor_list_item *next;
};

typedef struct sxxxxxxx_session {
	sxxxxxxx_state state;
	bool track_ending;
	sp_session *spotify_session;
	sp_track *track;
	sp_track *next_track;
	int seek_position;
	int frames_since_seek;
	audio_player_t player;
	// synchronize on libspotify method calls
	pthread_mutex_t spotify_mutex;
	/// Synchronization mutex for the main thread
	pthread_mutex_t notify_mutex;
	/// Synchronization condition variable for the main thread
	pthread_cond_t notify_cond;
	/// Synchronization variable telling the main thread to process events
	int notify_do;
	monitor_list_item *monitors;
} _sxxxxxxx_session;

void sxxxxxxx_notify_monitors(sxxxxxxx_session *s, const char *message, size_t len);

void sxxxxxxx_monitor(client *c);
void sxxxxxxx_monitor_end(client *c);
void sxxxxxxx_play(sxxxxxxx_session *session, char *id);

#endif
