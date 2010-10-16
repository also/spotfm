#include "sxxxxxxx_private.h"

#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "appkey.c"

#include "server.h"

static void logged_in(sp_session *sess, sp_error error);
static void notify_main_thread(sp_session *sess);
static int music_delivery(sp_session *sess, const sp_audioformat *format, const void *frames, int num_frames);
static void metadata_updated(sp_session *sess);
static void play_token_lost(sp_session *sess);
static void log_message(sp_session *session, const char *data);
static void message_to_user(sp_session *session, const char *data);
static void end_of_track(sp_session *sess);

static sp_session_callbacks session_callbacks = {
	.logged_in = &logged_in,
	.notify_main_thread = &notify_main_thread,
	.music_delivery = &music_delivery,
	.metadata_updated = &metadata_updated,
	.play_token_lost = &play_token_lost,
	.log_message = &log_message,
	.end_of_track = &end_of_track,
	.message_to_user = &message_to_user,
};

static sp_session_config spconfig = {
	.api_version = SPOTIFY_API_VERSION,
	.cache_location = "tmp",
	.settings_location = "tmp",
	.application_key = g_appkey,
	.application_key_size = 0, // Set in main()
	.user_agent = "test",
	.callbacks = &session_callbacks,
	NULL,
};

// we have to keep a global because spotify doesn't give us a way to reference this from its callbacks
static sxxxxxxx_session *g_session;
static void* main_loop(void *sess);
static void try_to_play(sxxxxxxx_session *session);

# pragma mark Spotify Callbacks

static void logged_in(sp_session *sess, sp_error error) {
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "Login failed: %s\n", sp_error_message(error));
		exit(2);
	}
}

static void notify_main_thread(sp_session *sess) {
	pthread_mutex_lock(&g_session->notify_mutex);
	g_session->notify_do = 1;
	pthread_cond_signal(&g_session->notify_cond);
	pthread_mutex_unlock(&g_session->notify_mutex);
}

static int music_delivery(sp_session *sess, const sp_audioformat *format, const void *frames, int num_frames) {
	audio_fifo_t *af = &g_session->audiofifo;
	audio_fifo_data_t *afd;
	size_t s;
	
	if (num_frames == 0)
		return 0; // Audio discontinuity, do nothing
	
	pthread_mutex_lock(&af->mutex);
	
	/* Buffer one second of audio */
	if (af->qlen > format->sample_rate) {
		pthread_mutex_unlock(&af->mutex);
		
		return 0;
	}
	
	s = num_frames * sizeof(int16_t) * format->channels;
	
	afd = malloc(sizeof(audio_fifo_data_t) + s);
	memcpy(afd->samples, frames, s);
	
	afd->nsamples = num_frames;
	
	afd->rate = format->sample_rate;
	afd->channels = format->channels;
	
	TAILQ_INSERT_TAIL(&af->q, afd, link);
	af->qlen += num_frames;
	
	pthread_cond_signal(&af->cond);
	pthread_mutex_unlock(&af->mutex);
	
	return num_frames;
}

static void metadata_updated(sp_session *sess) {
	try_to_play(g_session);
}

static void play_token_lost(sp_session *sess) {
	end_of_track(sess);
}

static void log_message(sp_session *session, const char *data) {
	fprintf(stderr, "log: %s", data);
}

static void message_to_user(sp_session *session, const char *data) {
	fprintf(stderr, "Spotify Says: %s\n", data);
}

static void end_of_track(sp_session *sess) {
	sp_track_release(g_session->track);
	sp_session_player_unload(sess);
}

static void try_to_play(sxxxxxxx_session *session) {
	if (session->track == NULL) {
		return;
	}
	if (sp_track_is_loaded(session->track) == 0) {
		return;
	}
	sp_error error;
	
	pthread_mutex_lock(&session->spotify_mutex);
	error = sp_session_player_load(session->spotify_session, session->track);
	if (SP_ERROR_OK != error) {
		return;
	}
	
	sp_session_player_play(session->spotify_session, true);
	fprintf(stderr, "playing!");
	pthread_mutex_unlock(&session->spotify_mutex);
}

void sxxxxxxx_init(sxxxxxxx_session **session, const char *username, const char *password) {
	sp_error err;
	sxxxxxxx_session *s;
	
	s = *session = (_sxxxxxxx_session *) malloc(sizeof(_sxxxxxxx_session));
	s->track = NULL;
	g_session = s;
	
	spconfig.application_key_size = g_appkey_size;
	
	err = sp_session_init(&spconfig, &s->spotify_session);
	
	if (SP_ERROR_OK != err) {
		fprintf(stderr, "Unable to create session: %s\n", sp_error_message(err));
		exit(1);
	}
	
	// set up locks
	pthread_mutex_init(&s->spotify_mutex, NULL);
	pthread_mutex_init(&s->notify_mutex, NULL);
	pthread_cond_init(&s->notify_cond, NULL);
	
	sp_session_login(s->spotify_session, username, password);
	
	audio_init(&s->audiofifo);
}

void sxxxxxxx_run(sxxxxxxx_session *session, bool thread) {
	pthread_t server_thread, main_thread;
	pthread_create(&server_thread, NULL, server_loop, session);
	if (thread) {
		pthread_create(&main_thread, NULL, main_loop, session);
	}
	else {
		main_loop(session);
	}
}

void* main_loop(void *s) {
	sxxxxxxx_session *session = (sxxxxxxx_session *) s;
	int next_timeout = 0;
	pthread_mutex_lock(&session->notify_mutex);
	
	for (;;) {
		if (next_timeout == 0) {
			while(!session->notify_do) {
				pthread_cond_wait(&session->notify_cond, &session->notify_mutex);
			}
		} else {
			struct timespec ts;
			
#if _POSIX_TIMERS > 0
			clock_gettime(CLOCK_REALTIME, &ts);
#else
			struct timeval tv;
			gettimeofday(&tv, NULL);
			TIMEVAL_TO_TIMESPEC(&tv, &ts);
#endif
			ts.tv_sec += next_timeout / 1000;
			ts.tv_nsec += (next_timeout % 1000) * 1000000;
			pthread_cond_timedwait(&session->notify_cond, &session->notify_mutex, &ts);
		}
		
		session->notify_do = 0;
		pthread_mutex_unlock(&session->notify_mutex);
		
		do {
			sp_session_process_events(session->spotify_session, &next_timeout);
		} while (next_timeout == 0);
		
		pthread_mutex_lock(&session->notify_mutex);
	}
}

void sxxxxxxx_play(sxxxxxxx_session *session, char *id) {
	char url[] = "spotify:track:XXXXXXXXXXXXXXXXXXXXXX";
	memcpy(url + 14, id, 22);
	sp_link *link = sp_link_create_from_string(url);
	if (!link) {
		return;
	}
	sp_track *track = sp_link_as_track(link);
	if (track) {
		sp_track_add_ref(track);
		session->track = track;
		try_to_play(session);
	}
	
	sp_link_release(link);
	
}

void sxxxxxxx_resume(sxxxxxxx_session *session) {
	sp_session_player_play(session->spotify_session, true);
}

void sxxxxxxx_stop(sxxxxxxx_session *session) {
	sp_session_player_play(session->spotify_session, false);
}
