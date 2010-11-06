#include "sxxxxxxx_private.h"

#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include <yajl/yajl_gen.h>

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

static void set_state(sxxxxxxx_session *s, char *state);

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
	if (g_session->state != PLAYING) {
		g_session->state = PLAYING;
		set_state(g_session, "playing");
	}

	audio_fifo_data_t *afd;
	size_t s;
	
	if (num_frames == 0) {
		return 0; // Audio discontinuity, do nothing
	}
	
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
	
	audio_enqueue(af, afd);
	
	pthread_cond_signal(&af->cond);
	pthread_mutex_unlock(&af->mutex);
	
	return num_frames;
}

static void metadata_updated(sp_session *sess) {
	try_to_play(g_session);
}

static void play_token_lost(sp_session *sess) {
	// we can lose the play token even if there is no track
	g_session->state = STOPPED;
	set_state(g_session, "stopped");
}

static void log_message(sp_session *session, const char *data) {
	fprintf(stderr, "log: %s", data);
}

static void message_to_user(sp_session *session, const char *data) {
	fprintf(stderr, "Spotify Says: %s\n", data);
}

static void end_of_track(sp_session *sess) {
	g_session->state = STOPPED;
	set_state(g_session, "end_of_track");
	set_state(g_session, "stopped");
	g_session->track = NULL;
	// TODO better to leak than to crash...
	//sp_session_player_unload(sess);
}

static void try_to_play(sxxxxxxx_session *session) {
	if (!session->next_track) {
		return;
	}
	if (sp_track_is_loaded(session->next_track) == 0) {
		return;
	}
	sp_error error;
	
	pthread_mutex_lock(&session->spotify_mutex);

	session->track = session->next_track;
	session->next_track = NULL;
	error = sp_session_player_load(session->spotify_session, session->track);
	if (SP_ERROR_OK != error) {
		fprintf(stderr, "failed loading player: %s\n", sp_error_message(error));
		set_state(g_session, "playback_failed");
		pthread_mutex_unlock(&session->spotify_mutex);
		return;
	}
	else {
		audio_fifo_flush(&g_session->audiofifo);
		audio_reset(&g_session->audiofifo);
		sp_session_player_play(session->spotify_session, true);

		yajl_gen_config conf = { false };
		yajl_gen g;
		g = yajl_gen_alloc(&conf, NULL);
		yajl_gen_map_open(g);

		yajl_gen_string(g, (unsigned char *) "event", 5);
		yajl_gen_string(g, (unsigned char *) "playing", 7);

		yajl_gen_string(g, (unsigned char *) "track_name", 10);
		const char *track_name = sp_track_name(session->track);
		yajl_gen_string(g, track_name, strlen(track_name));

		sp_album* album = sp_track_album(session->track);
		yajl_gen_string(g, (unsigned char *) "album_name", 10);
		const char *album_name = sp_album_name(album);
		yajl_gen_string(g, album_name, strlen(album_name));

		sp_artist *artist = sp_track_artist(session->track, 0);
		yajl_gen_string(g, (unsigned char *) "artist_name", 11);
		const char *artist_name = sp_artist_name(artist);
		yajl_gen_string(g, artist_name, strlen(artist_name));
		
		int duration = sp_track_duration(session->track);
		yajl_gen_string(g, (unsigned char *) "track_duration", 14);
		yajl_gen_integer(g, duration);

		yajl_gen_map_close(g);
		const unsigned char *buf;
		unsigned int len;
		yajl_gen_get_buf(g, &buf, &len);
		sxxxxxxx_notify_monitors(g_session, buf, len);
		yajl_gen_clear(g);
	}
	pthread_mutex_unlock(&session->spotify_mutex);
}

void sxxxxxxx_init(sxxxxxxx_session **session, const char *username, const char *password) {
	sp_error err;
	sxxxxxxx_session *s;
	
	s = *session = (_sxxxxxxx_session *) malloc(sizeof(_sxxxxxxx_session));
	bzero(s, sizeof(_sxxxxxxx_session));
	s->track = NULL;
	s->state = STOPPED;
	set_state(s, "stopped");
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

void sxxxxxxx_monitor(client *c) {
	monitor_list_item *item = malloc(sizeof(monitor_list_item));
	bzero(item, sizeof(monitor_list_item));
	item->c = c;
	c->data = item;
	monitor_list_item *last = c->session->monitors;

	if (!last) {
		c->session->monitors = item;
	}
	else {
		while (last->next) {
			last = last->next;
		}
		last->next = item;
		item->previous = last;
	}

	if (c->session->last_monitor_message) {
		ws_send(c->ws_client, c->session->last_monitor_message, c->session->last_monitor_message_len);
	}
	fprintf(stderr, "client %p added to monitor list\n", c);
}

void sxxxxxxx_monitor_end(client *c) {
	monitor_list_item *item = c->data;

	if (item->previous) {
		item->previous->next = item->next;
	}
	else {
		c->session->monitors = item->next;
	}
	if (item->next) {
		item->next->previous = item->previous;
	}
	fprintf(stderr, "client %p removed from monitor list\n", c);
	free(item);
}

void sxxxxxxx_notify_monitors(sxxxxxxx_session *s, const char *message, size_t len) {
	monitor_list_item *monitor = s->monitors;

	s->last_monitor_message = realloc(s->last_monitor_message, strlen(message) + 1);
	memcpy(s->last_monitor_message, message, strlen(message) + 1);

	while (monitor) {
		ws_send(monitor->c->ws_client, message, len);
		fprintf(stderr, "notified %p \"%s\"\n", monitor->c, message);
		monitor = monitor->next;
	}
}

yajl_gen begin_event_json(sxxxxxxx_session *s, const char *event) {
	yajl_gen_config conf = { false };
	yajl_gen g;
	g = yajl_gen_alloc(&conf, NULL);
	yajl_gen_map_open(g);
	yajl_gen_string(g, "event", 5);
	yajl_gen_string(g, event, strlen(event));
	return g;
}

void finish_send_event(sxxxxxxx_session *s, yajl_gen g) {
	yajl_gen_map_close(g);
	const unsigned char *buf;
	unsigned int len;
	yajl_gen_get_buf(g, &buf, &len);
	sxxxxxxx_notify_monitors(s, buf, len);
	yajl_gen_clear(g);	
}

void set_state(sxxxxxxx_session *s, char *state) {
	yajl_gen g = begin_event_json(s, "state_change");
	yajl_gen_string(g, "state", 5);
	yajl_gen_string(g, state, strlen(state));
	finish_send_event(s, g);
}

void sxxxxxxx_play(sxxxxxxx_session *session, char *id) {
	sxxxxxxx_stop(session);

	char url[] = "spotify:track:XXXXXXXXXXXXXXXXXXXXXX";
	memcpy(url + 14, id, 22);
	sp_link *link = sp_link_create_from_string(url);
	if (!link) {
		return;
	}
	sp_track *track = sp_link_as_track(link);
	if (track) {
		sp_track_add_ref(track);
		session->next_track = track;
		session->state = BUFFERING;
		set_state(session, "buffering");
		fprintf(stderr, "loading\n");
		try_to_play(session);
	}
	
	sp_link_release(link);
}

void sxxxxxxx_resume(sxxxxxxx_session *session) {
	// TODO what is necessary?
	audio_fifo_flush(&g_session->audiofifo);
	audio_reset(&g_session->audiofifo);
	sp_session_player_play(session->spotify_session, true);
}

void sxxxxxxx_stop(sxxxxxxx_session *session) {
	// TODO what is necessary?
	// TODO drops a buffer full of audio
	audio_fifo_flush(&g_session->audiofifo);
	audio_reset(&g_session->audiofifo);
	sp_session_player_play(session->spotify_session, false);
	session->state = STOPPED;
	set_state(session, "stopped");
}

void sxxxxxxx_toggle_play(sxxxxxxx_session *session) {
	if (session->state == STOPPED) {
		sxxxxxxx_resume(session);
	}
	else {
		sxxxxxxx_stop(session);
	}
}

void sxxxxxxx_seek(sxxxxxxx_session *session, int offset) {
	// TODO what is necessary?
	audio_fifo_flush(&g_session->audiofifo);
	audio_reset(&g_session->audiofifo);
	sp_session_player_seek(session->spotify_session, offset);
	yajl_gen g = begin_event_json(session, "seek");
	yajl_gen_string(g, "offset", 6);
	yajl_gen_integer(g, offset);
	finish_send_event(session, g);
}

void sxxxxxxx_previous(sxxxxxxx_session *session) {
	sxxxxxxx_seek(session, 0);
	// TODO if at beginning of song, send "previous track" event
}

void sxxxxxxx_next(sxxxxxxx_session *session) {
	// TODO
}

