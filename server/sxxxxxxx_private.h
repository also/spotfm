#include "sxxxxxxx.h"

#include <stdlib.h>

#include <pthread.h>
#include <libspotify/api.h>

#include "audio.h"

typedef struct sxxxxxxx_session {
	sp_session *spotify_session;
	sp_track *track;
	audio_fifo_t audiofifo;
	// synchronize on libspotify method calls
	pthread_mutex_t spotify_mutex;
	/// Synchronization mutex for the main thread
	pthread_mutex_t notify_mutex;
	/// Synchronization condition variable for the main thread
	pthread_cond_t notify_cond;
	/// Synchronization variable telling the main thread to process events
	int notify_do;
} _sxxxxxxx_session;

void sxxxxxxx_play(sxxxxxxx_session *session, char *id);
void sxxxxxxx_pause(sxxxxxxx_session *session);
void sxxxxxxx_resume(sxxxxxxx_session *session);
static void sxxxxxxx_stop(sxxxxxxx_session *session);

static void try_to_play(sxxxxxxx_session *session);
