#ifndef sx_h
#define sx_h

#include "sxxxxxxx.h"

#include <stdlib.h>
#include <pthread.h>

#include <libspotify/api.h>

#include "sx_server.h"

#include "audio.h"

typedef enum {
	STOPPED,
	PAUSED,
	LOADING,
	BUFFERING,
	PLAYING,
} sx_state;

typedef struct monitor_list_item monitor_list_item;

typedef struct sx_session {
	sxxxxxxx_session_config *config;
	sx_state state;
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
} sx_session;

void sx_log(sx_session *s, const char *message, ...);

void sx_notify_monitors(sx_session *s, const char *message, size_t len);

void sx_monitor(sx_client *c);
void sx_monitor_end(sx_client *c);
void sx_play(sx_session *session, char *id);
void sx_send_event(sx_session *s, char *event);
void sx_try_to_play(sx_session *session);

int sx_waitfor(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, int ms_to_wait);

#endif
