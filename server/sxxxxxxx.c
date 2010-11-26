#include "sxxxxxxx_private.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
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

static void send_event(sxxxxxxx_session *s, char *event);

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
	.user_agent = "spotfm",
	.callbacks = &session_callbacks,
	NULL,
};

// we have to keep a global because spotify doesn't give us a way to reference this from its callbacks
static sxxxxxxx_session *g_session;

static void* main_loop(void *sess);
static void* watchdog_loop(void *sess);
static void try_to_play(sxxxxxxx_session *session);

static yajl_gen begin_event(sxxxxxxx_session *s, const char *event);

static void finish_event_notify_monitors(sxxxxxxx_session *s, yajl_gen g);
static void finish_event_send(client *c, yajl_gen g);

static yajl_gen begin_track_info_event(sxxxxxxx_session *session);
yajl_gen_status yajl_gen_string0(yajl_gen g, const char * str) {
	return yajl_gen_string(g, (unsigned char *) str, strlen(str));
}

static double get_position(sxxxxxxx_session *session);


static void handle_player_stop(audio_player_t *player) {
	sxxxxxxx_session *session = (sxxxxxxx_session *) player->data;
	session->state = STOPPED;
	if (session->track_ending) {
		session->track_ending = false;
		send_event(g_session, "end_of_track");
	}
	send_event(session, "stopped");
	sxxxxxxx_log(session, "handle_player_stop");
}

# pragma mark Spotify Callbacks

static void logged_in(sp_session *sess, sp_error error) {
	if (SP_ERROR_OK != error) {
		sxxxxxxx_log(g_session, "Login failed: %s\n", sp_error_message(error));
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
	audio_fifo_t *af = &g_session->player.af;
	if (g_session->state != PLAYING) {
		g_session->state = PLAYING;
		audio_start(&g_session->player);
		send_event(g_session, "start_of_track");
		send_event(g_session, "playing");
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
	
	audio_enqueue(&g_session->player, afd);
	
	pthread_cond_signal(&af->cond);
	pthread_mutex_unlock(&af->mutex);

	g_session->frames_since_seek += num_frames;
	
	return num_frames;
}

static void metadata_updated(sp_session *sess) {
	try_to_play(g_session);
}

static void play_token_lost(sp_session *sess) {
	// we can lose the play token even if there is no track
	audio_finish(&g_session->player);
}

static void log_message(sp_session *session, const char *data) {
	sxxxxxxx_log(g_session, "libspotify log: %s", data);
}

static void message_to_user(sp_session *session, const char *data) {
	sxxxxxxx_log(g_session, "libspotify message_to_user: %s", data);
}

static void end_of_track(sp_session *sess) {
	g_session->track_ending = true;
	audio_finish(&g_session->player);
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

	if (session->track && session->track != session->next_track) {
		sp_track_release(session->track);
	}

	session->track = session->next_track;
	session->next_track = NULL;
	error = sp_session_player_load(session->spotify_session, session->track);
	if (SP_ERROR_OK != error) {
		sxxxxxxx_log(session, "failed loading player: %s", sp_error_message(error));
		send_event(session, "playback_failed");
		pthread_mutex_unlock(&session->spotify_mutex);
		return;
	}
	else {
		audio_fifo_flush(&session->player.af);
		audio_reset(&session->player);
		session->seek_position = 0;
		session->frames_since_seek = 0;
		sp_session_player_play(session->spotify_session, true);

		yajl_gen g = begin_track_info_event(session);
		finish_event_notify_monitors(session, g);

		g = begin_event(session, "position");
		yajl_gen_string0(g, "position");
		yajl_gen_integer(g, 0);
		finish_event_notify_monitors(session, g);
	}
	pthread_mutex_unlock(&session->spotify_mutex);
}

void sxxxxxxx_init(sxxxxxxx_session **session, sxxxxxxx_session_config * config, const char *username, const char *password) {
	sp_error err;
	sxxxxxxx_session *s;
	
	s = *session = (_sxxxxxxx_session *) malloc(sizeof(_sxxxxxxx_session));
	bzero(s, sizeof(_sxxxxxxx_session));
	s->config = malloc(sizeof(sxxxxxxx_session_config));
	memcpy(s->config, config, sizeof(sxxxxxxx_session_config));
	s->track = NULL;
	s->state = STOPPED;
	send_event(s, "stopped");
	g_session = s;
	
	spconfig.application_key_size = g_appkey_size;
	
	err = sp_session_init(&spconfig, &s->spotify_session);
	
	if (SP_ERROR_OK != err) {
		sxxxxxxx_log(s, "Unable to create session: %s", sp_error_message(err));
		exit(1);
	}
	
	// set up locks
	pthread_mutex_init(&s->spotify_mutex, NULL);
	pthread_mutex_init(&s->notify_mutex, NULL);
	pthread_cond_init(&s->notify_cond, NULL);
	
	sp_session_login(s->spotify_session, username, password);
	
	s->player.data = s;
	s->player.on_stop = handle_player_stop;

	audio_init(&s->player);
}

void sxxxxxxx_log(sxxxxxxx_session *s, const char *message, ...) {
	if (!s->config->log) return;

	va_list argptr;
	va_start(argptr, message);
	char *formatted_message;
	vasprintf(&formatted_message, message, argptr);
	va_end(argptr);
	s->config->log(NULL, formatted_message);
}

void sxxxxxxx_run(sxxxxxxx_session *session, bool thread) {
	pthread_t server_thread, main_thread, watchdog_thread;
	pthread_create(&server_thread, NULL, server_loop, session);
	pthread_create(&watchdog_thread, NULL, watchdog_loop, session);
	if (thread) {
		pthread_create(&main_thread, NULL, main_loop, session);
	}
	else {
		main_loop(session);
	}
}

static int waitfor(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, int ms_to_wait) {
	struct timespec ts;

#if _POSIX_TIMERS > 0
	clock_gettime(CLOCK_REALTIME, &ts);
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	TIMEVAL_TO_TIMESPEC(&tv, &ts);
#endif
	ts.tv_sec += ms_to_wait / 1000;
	ts.tv_nsec += (ms_to_wait % 1000) * 1000000;
	pthread_cond_timedwait(cond, mutex, &ts);
	return 0;
}

static void* main_loop(void *s) {
	sxxxxxxx_session *session = (sxxxxxxx_session *) s;
	int next_timeout = 0;
	pthread_mutex_lock(&session->notify_mutex);
	
	for (;;) {
		if (next_timeout == 0) {
			while(!session->notify_do) {
				pthread_cond_wait(&session->notify_cond, &session->notify_mutex);
			}
		} else {
			waitfor(&session->notify_cond, &session->notify_mutex, next_timeout);
		}
		
		session->notify_do = 0;
		pthread_mutex_unlock(&session->notify_mutex);
		
		do {
			sp_session_process_events(session->spotify_session, &next_timeout);
		} while (next_timeout == 0);
		
		pthread_mutex_lock(&session->notify_mutex);
	}
}

static void* watchdog_loop(void *sess) {
	sxxxxxxx_session *session = (sxxxxxxx_session *) sess;

	pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

	int previous_frames_since_seek = 0;
	int previous_seek_position = 0;

	// TODO seems like a dumb way to sleep
	pthread_mutex_lock(&lock);
	for(;;) {
		waitfor(&cond, &lock, 1000);
		int position;
		audio_get_position(&g_session->player, &position);
		pthread_mutex_unlock(&lock);
		if (previous_frames_since_seek != session->frames_since_seek || previous_seek_position != session->seek_position) {
			double position = get_position(session);
			yajl_gen g = begin_event(session, "position");
			yajl_gen_string0(g, "position");
			yajl_gen_double(g, position);
			finish_event_notify_monitors(session, g);
			previous_frames_since_seek = session->frames_since_seek;
			previous_seek_position = session->seek_position;
		}
	}
	pthread_mutex_unlock(&lock);

	return NULL;
}

static double get_position(sxxxxxxx_session *session) {
	return (double) session->frames_since_seek / 44.100 + session->seek_position;
}

static yajl_gen begin_track_info_event(sxxxxxxx_session *session) {
	yajl_gen g = begin_event(session, "track_info");

	yajl_gen_string0(g, "track_name");
	const char *track_name = sp_track_name(session->track);
	yajl_gen_string0(g, track_name);

	sp_album* album = sp_track_album(session->track);
	yajl_gen_string0(g, "album_name");
	const char *album_name = sp_album_name(album);
	yajl_gen_string0(g, album_name);

	sp_artist *artist = sp_track_artist(session->track, 0);
	yajl_gen_string0(g, "artist_name");
	const char *artist_name = sp_artist_name(artist);
	yajl_gen_string0(g, artist_name);

	int duration = sp_track_duration(session->track);
	yajl_gen_string0(g, "track_duration");
	yajl_gen_integer(g, duration);

	return g;
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

	double position = 0.0;
	yajl_gen g;

	if (c->session->track) {
		g = begin_track_info_event(c->session);
		finish_event_send(c, g);
		position = get_position(c->session);
	}
	g = begin_event(c->session, "position");
	yajl_gen_string0(g, "position");
	yajl_gen_double(g, position);
	finish_event_send(c, g);
	if (c->session->state == PLAYING) {
		send_event(c->session, "playing");
	}
	else {
		send_event(c->session, "stopped");
	}

	sxxxxxxx_log(c->session, "client %p added to monitor list", c);
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
	sxxxxxxx_log(c->session, "client %p removed from monitor list", c);
	free(item);
}

void sxxxxxxx_notify_monitors(sxxxxxxx_session *s, const char *message, size_t len) {
	monitor_list_item *monitor = s->monitors;

	while (monitor) {
		ws_send(monitor->c->ws_client, message, len);
		monitor = monitor->next;
	}
}

static yajl_gen begin_event(sxxxxxxx_session *s, const char *event) {
	yajl_gen_config conf = { false };
	yajl_gen g;
	g = yajl_gen_alloc(&conf, NULL);
	yajl_gen_map_open(g);
	yajl_gen_string0(g, "event");
	yajl_gen_string0(g, event);
	return g;
}

static void finish_event_notify_monitors(sxxxxxxx_session *s, yajl_gen g) {
	yajl_gen_map_close(g);
	const unsigned char *buf;
	unsigned int len;
	yajl_gen_get_buf(g, &buf, &len);
	sxxxxxxx_notify_monitors(s, (char *) buf, len);
	yajl_gen_free(g);
}

static void finish_event_send(client *c, yajl_gen g) {
	yajl_gen_map_close(g);
	const unsigned char *buf;
	unsigned int len;
	yajl_gen_get_buf(g, &buf, &len);
	ws_send(c->ws_client, (char *) buf, len);
	yajl_gen_free(g);
}

static void send_event(sxxxxxxx_session *s, char *event) {
	yajl_gen g = begin_event(s, event);
	finish_event_notify_monitors(s, g);
}

void sxxxxxxx_play(sxxxxxxx_session *session, char *id) {
	sp_session_player_play(session->spotify_session, false);
	// TODO what is necessary?
	audio_fifo_flush(&session->player.af);
	audio_reset(&session->player);

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
		send_event(session, "buffering");
		try_to_play(session);
	}
	
	sp_link_release(link);
}

void sxxxxxxx_resume(sxxxxxxx_session *session) {

	if (session->track) {
		sp_session_player_play(session->spotify_session, true);
	}
	else {
		send_event(session, "play");
	}

}

void sxxxxxxx_stop(sxxxxxxx_session *session) {
	// TODO what is necessary?
	audio_pause(&session->player);
	sp_session_player_play(session->spotify_session, false);
	session->state = PAUSED;
}

void sxxxxxxx_toggle_play(sxxxxxxx_session *session) {
	if (session->state == STOPPED || session->state == PAUSED) {
		sxxxxxxx_resume(session);
	}
	else {
		sxxxxxxx_stop(session);
	}
}

void sxxxxxxx_seek(sxxxxxxx_session *session, int offset) {
	// TODO what is necessary?
	audio_fifo_flush(&session->player.af);
	audio_reset(&session->player);
	sp_session_player_seek(session->spotify_session, offset);
	session->frames_since_seek = 0;
	session->seek_position = offset;
	yajl_gen g = begin_event(session, "position");
	yajl_gen_string0(g, "position");
	yajl_gen_integer(g, offset);
	finish_event_notify_monitors(session, g);
}

void sxxxxxxx_previous(sxxxxxxx_session *session) {
	if (get_position(session) < 3000) {
		send_event(session, "previous");
	}
	else {
		sxxxxxxx_seek(session, 0);
	}
}

void sxxxxxxx_next(sxxxxxxx_session *session) {
	send_event(session, "next");
}
