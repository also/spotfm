#include "sx.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/time.h>
#include <arpa/inet.h>

#include "sx_server.h"
#include "sx_spotify.h"
#include "sx_json.h"

struct monitor_list_item {
	sx_client *c;
	monitor_list_item *previous;
	monitor_list_item *next;
};

static void* watchdog_loop(void *sess);

static yajl_gen begin_event(sx_session *s, const char *event);
static yajl_gen begin_track_info_event(sx_session *session);

static void finish_event_notify_monitors(sx_session *s, yajl_gen g);
static void finish_event_send(sx_client *c, yajl_gen g);

static double get_position(sx_session *session);
static void handle_player_stop(audio_player_t *player);

static void next_track_ready(sx_session *session, void *data);

static void next_track_ready(sx_session *session, void *data) {
	sp_error error;

	sp_track *next_track = (sp_track *) data;

	audio_fifo_flush(&session->player.af);
	audio_reset(&session->player);
	session->seek_position = 0;
	session->frames_since_seek = 0;

	// TODO probably unnecessary; called from main thread
	sxp_lock(session);

	if (session->track && session->track != next_track) {
		sp_track_release(session->track);
	}

	session->track = next_track;

	error = sp_session_player_load(session->spotify_session, session->track);
	if (SP_ERROR_OK != error) {
		sxp_unlock(session);
		sx_log(session, "failed loading player: %s", sp_error_message(error));
		sx_send_event(session, "playback_failed");
	}
	else {
		sp_session_player_play(session->spotify_session, true);
		sxp_unlock(session);

		yajl_gen g = begin_track_info_event(session);
		finish_event_notify_monitors(session, g);

		g = begin_event(session, "position");
		yajl_gen_string0(g, "position");
		yajl_gen_integer(g, 0);
		finish_event_notify_monitors(session, g);
	}
}

void sxxxxxxx_init(sx_session **session, sxxxxxxx_session_config * config, const char *username, const char *password) {
	sx_session *s;
	
	s = *session = (sx_session *) malloc(sizeof(sx_session));
	bzero(s, sizeof(sx_session));
	s->config = malloc(sizeof(sxxxxxxx_session_config));
	memcpy(s->config, config, sizeof(sxxxxxxx_session_config));
	s->track = NULL;
	s->state = STOPPED;
	sx_send_event(s, "stopped");

	sx_spotify_init(s);
	
	sp_session_login(s->spotify_session, username, password);
	
	s->player.data = s;
	s->player.on_stop = handle_player_stop;

	audio_init(&s->player);
}

void sx_log(sx_session *s, const char *message, ...) {
	if (!s->config->log) return;

	va_list argptr;
	va_start(argptr, message);
	char *formatted_message;
	vasprintf(&formatted_message, message, argptr);
	va_end(argptr);
	s->config->log(NULL, formatted_message);
	free(formatted_message);
}

void sxxxxxxx_run(sx_session *session, bool thread) {
	pthread_t server_thread, main_thread, watchdog_thread;
	pthread_create(&server_thread, NULL, sx_server_loop, session);
	pthread_create(&watchdog_thread, NULL, watchdog_loop, session);
	if (thread) {
		pthread_create(&main_thread, NULL, sx_spotify_run, session);
	}
	else {
		sx_spotify_run(session);
	}
}

int sx_waitfor(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, int ms_to_wait) {
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

static void* watchdog_loop(void *sess) {
	pthread_setname_np("sx_watchdog main");
	sx_session *session = (sx_session *) sess;

	pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

	int previous_frames_since_seek = 0;
	int previous_seek_position = 0;

	pthread_mutex_lock(&lock);
	for(;;) {
		sx_waitfor(&cond, &lock, 1000);
		int position;
		audio_get_position(&session->player, &position);
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

static double get_position(sx_session *session) {
	return (double) session->frames_since_seek / 44.100 + session->seek_position;
}

static void handle_player_stop(audio_player_t *player) {
	sx_session *session = (sx_session *) player->data;
	session->state = STOPPED;
	if (session->track_ending) {
		session->track_ending = false;
		sx_send_event(session, "end_of_track");
	}
	sx_send_event(session, "stopped");
	sx_log(session, "handle_player_stop");
}

static yajl_gen begin_track_info_event(sx_session *session) {
	sxp_lock(session);
	const char *track_name = sp_track_name(session->track);
	sp_album* album = sp_track_album(session->track);
	const char *album_name = sp_album_name(album);
	sp_artist *artist = sp_track_artist(session->track, 0);
	const char *artist_name = sp_artist_name(artist);
	int duration = sp_track_duration(session->track);
	sxp_unlock(session);

	yajl_gen g = begin_event(session, "track_info");

	yajl_gen_string0(g, "track_name");
	yajl_gen_string0(g, track_name);

	yajl_gen_string0(g, "album_name");
	yajl_gen_string0(g, album_name);

	yajl_gen_string0(g, "artist_name");
	yajl_gen_string0(g, artist_name);

	yajl_gen_string0(g, "track_duration");
	yajl_gen_integer(g, duration);

	return g;
}

void sx_monitor(sx_client *c) {
	// FIXME lock
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
		sx_send_event(c->session, "playing");
	}
	else {
		sx_send_event(c->session, "stopped");
	}

	sx_log(c->session, "client %p added to monitor list", c);
}

void sx_monitor_end(sx_client *c) {
	// FIXME lock
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
	sx_log(c->session, "client %p removed from monitor list", c);
	free(item);
}

void sxxxxxxx_notify_monitors(sx_session *s, const char *message, size_t len) {
	// FIXME lock
	monitor_list_item *monitor = s->monitors;

	while (monitor) {
		ws_send(monitor->c->ws_client, message, len);
		monitor = monitor->next;
	}
}

static yajl_gen begin_event(sx_session *s, const char *event) {
	yajl_gen_config conf = { false };
	yajl_gen g;
	g = yajl_gen_alloc(&conf, NULL);
	yajl_gen_map_open(g);
	yajl_gen_string0(g, "event");
	yajl_gen_string0(g, event);
	return g;
}

static void finish_event_notify_monitors(sx_session *s, yajl_gen g) {
	yajl_gen_map_close(g);
	const unsigned char *buf;
	unsigned int len;
	yajl_gen_get_buf(g, &buf, &len);
	sxxxxxxx_notify_monitors(s, (char *) buf, len);
	yajl_gen_free(g);
}

static void finish_event_send(sx_client *c, yajl_gen g) {
	yajl_gen_map_close(g);
	const unsigned char *buf;
	unsigned int len;
	yajl_gen_get_buf(g, &buf, &len);
	ws_send(c->ws_client, (char *) buf, len);
	yajl_gen_free(g);
}

void sx_send_event(sx_session *s, char *event) {
	yajl_gen g = begin_event(s, event);
	finish_event_notify_monitors(s, g);
}

void sx_play(sx_session *session, char *id) {
	sxp_lock(session);
	sp_session_player_play(session->spotify_session, false);
	sxp_unlock(session);

	// TODO what is necessary?
	audio_fifo_flush(&session->player.af);
	audio_reset(&session->player);

	char url[] = "spotify:track:XXXXXXXXXXXXXXXXXXXXXX";
	memcpy(url + 14, id, 22);
	sp_track *track = sx_spotify_track_for_url(session, url);
	if (track) {
		session->state = BUFFERING;
		sx_send_event(session, "buffering");
		sx_spotify_load_track(session, track, &next_track_ready, track);
	}
}

void sxxxxxxx_resume(sx_session *session) {
	if (session->track) {
		sxp_lock(session);
		sp_session_player_play(session->spotify_session, true);
		sxp_unlock(session);
	}
	else {
		sx_send_event(session, "play");
	}
}

void sxxxxxxx_stop(sx_session *session) {
	// TODO what is necessary?
	audio_pause(&session->player);
	sxp_lock(session);
	sp_session_player_play(session->spotify_session, false);
	sxp_unlock(session);
	session->state = PAUSED;
}

void sxxxxxxx_toggle_play(sx_session *session) {
	if (session->state == STOPPED || session->state == PAUSED) {
		sxxxxxxx_resume(session);
	}
	else {
		sxxxxxxx_stop(session);
	}
}

void sxxxxxxx_seek(sx_session *session, int offset) {
	// TODO what is necessary?
	audio_fifo_flush(&session->player.af);
	audio_reset(&session->player);
	sxp_lock(session);
	sp_session_player_seek(session->spotify_session, offset);
	sxp_unlock(session);
	session->frames_since_seek = 0;
	session->seek_position = offset;
	yajl_gen g = begin_event(session, "position");
	yajl_gen_string0(g, "position");
	yajl_gen_integer(g, offset);
	finish_event_notify_monitors(session, g);
}

void sxxxxxxx_previous(sx_session *session) {
	if (get_position(session) < 3000) {
		sx_send_event(session, "previous");
	}
	else {
		sxxxxxxx_seek(session, 0);
	}
}

void sxxxxxxx_next(sx_session *session) {
	sx_send_event(session, "next");
}
