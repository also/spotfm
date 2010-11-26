#include "spotify.h"

#include "sxxxxxxx_private.h"

#import "string.h"

#include "appkey.c"

static void logged_in(sp_session *sess, sp_error error);
static void notify_main_thread(sp_session *sess);
static int music_delivery(sp_session *sess, const sp_audioformat *format, const void *frames, int num_frames);
static void metadata_updated(sp_session *sess);
static void play_token_lost(sp_session *sess);
static void log_message(sp_session *session, const char *data);
static void message_to_user(sp_session *session, const char *data);
static void end_of_track(sp_session *sess);

// we have to keep a global because spotify doesn't give us a way to reference this from its callbacks
static sxxxxxxx_session *g_session;

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

void sx_spotify_init(sxxxxxxx_session *s) {
	sp_error err;

	g_session = s;
	spconfig.application_key_size = g_appkey_size;

	err = sp_session_init(&spconfig, &s->spotify_session);

	if (SP_ERROR_OK != err) {
		sxxxxxxx_log(s, "Unable to create session: %s", sp_error_message(err));
		exit(1);
	}
}

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
