#include "sx_spotify.h"

#include "sx.h"

#import "string.h"

#include "appkey.c"

#define SX_S(sp_session) ((sx_session *) sp_session_userdata(sp_session))

struct spotify_metadata_listener_list_item {
	sx_spotify_metadata_listener *listener;
	void *data;
	spotify_metadata_listener_list_item *previous;
	spotify_metadata_listener_list_item *next;
};

typedef struct sxp_loader {
	sx_session *session;
	sx_callback *callback;
	pthread_cond_t cond;
	void *sp_obj;
	void *data;
	bool free;
} sxp_loader;

static spotify_metadata_listener_list_item *remove_metadata_listener_list_item(sx_session *session, spotify_metadata_listener_list_item *item);
static bool track_loaded_metadata_callback(sx_session *session, void *data);

static void logged_in(sp_session *sess, sp_error error);
static void notify_main_thread(sp_session *sess);
static int music_delivery(sp_session *sess, const sp_audioformat *format, const void *frames, int num_frames);
static void metadata_updated(sp_session *sess);
static void play_token_lost(sp_session *sess);
static void log_message(sp_session *session, const char *data);
static void message_to_user(sp_session *session, const char *data);
static void end_of_track(sp_session *sess);

static void image_loaded(sp_image *image, void *userdata);

static sxp_loader *sxp_create_loader();
static void sxp_release_loader(sxp_loader *loader);

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

void sxp_lock(sx_session *session) {
	if (!pthread_equal(pthread_self(), session->spotify_thread)) {
		pthread_mutex_lock(&session->spotify_mutex);
	}

}

void sxp_unlock(sx_session *session) {
	if (!pthread_equal(pthread_self(), session->spotify_thread)) {
		pthread_mutex_unlock(&session->spotify_mutex);
	}
}

void sx_spotify_init(sx_session *session) {
	sp_error err;

	// set up locks
	pthread_mutex_init(&session->spotify_mutex, NULL);
	pthread_mutex_init(&session->notify_mutex, NULL);
	pthread_cond_init(&session->notify_cond, NULL);
	pthread_mutex_init(&session->spotify_metadata_listeners_lock, NULL);

	session->sp_session_config.api_version = SPOTIFY_API_VERSION;
	session->sp_session_config.cache_location = "tmp";
	session->sp_session_config.settings_location = "tmp";
	session->sp_session_config.application_key = g_appkey;
	session->sp_session_config.application_key_size = g_appkey_size;
	session->sp_session_config.user_agent = "sx";
	session->sp_session_config.userdata = session;
	session->sp_session_config.callbacks = &session_callbacks;
	
	err = sp_session_create(&session->sp_session_config, &session->spotify_session);

	if (SP_ERROR_OK != err) {
		sx_log(session, "Unable to create session: %s", sp_error_message(err));
		exit(1);
	}
}

static void logged_in(sp_session *sess, sp_error error) {
	if (SP_ERROR_OK != error) {
		sx_log(SX_S(sess), "Login failed: %s\n", sp_error_message(error));
		exit(2);
	}
}

static void notify_main_thread(sp_session *sess) {
	sx_session *session = SX_S(sess);
	pthread_mutex_lock(&session->notify_mutex);
	session->notify_do = 1;
	pthread_cond_signal(&session->notify_cond);
	pthread_mutex_unlock(&session->notify_mutex);
}

static int music_delivery(sp_session *sess, const sp_audioformat *format, const void *frames, int num_frames) {
	sx_session *session = SX_S(sess);
	audio_fifo_t *af = &session->player.af;
	if (session->state != PLAYING) {
		session->state = PLAYING;
		audio_start(&session->player);
		sx_send_event(session, "start_of_track");
		sx_send_event(session, "playing");
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

	audio_enqueue(&session->player, afd);

	pthread_cond_signal(&af->cond);
	pthread_mutex_unlock(&af->mutex);

	session->frames_since_seek += num_frames;

	return num_frames;
}

static void metadata_updated(sp_session *sess) {
	sx_session *session = SX_S(sess);
	pthread_mutex_lock(&session->spotify_metadata_listeners_lock);

	spotify_metadata_listener_list_item *item = session->spotify_metadata_listeners;

	while (item) {
		bool remove = item->listener(session, item->data);
		if (remove) {
			item = remove_metadata_listener_list_item(session, item);
		}
		else {
			item = item->next;
		}
	}

	pthread_mutex_unlock(&session->spotify_metadata_listeners_lock);
}

static void play_token_lost(sp_session *sess) {
	// we can lose the play token even if there is no track
	audio_finish(&SX_S(sess)->player);
}

static void log_message(sp_session *sess, const char *data) {
	sx_log(SX_S(sess), "libspotify log: %s", data);
}

static void message_to_user(sp_session *sess, const char *data) {
	sx_log(SX_S(sess), "libspotify message_to_user: %s", data);
}

static void end_of_track(sp_session *sess) {
	sx_session *session = SX_S(sess);
	session->track_ending = true;
	audio_finish(&session->player);
	session->track = NULL;
	// TODO better to leak than to crash...
	//sp_session_player_unload(sess);
}

void* sx_spotify_run(void *s) {
	pthread_setname_np("sx_spotify main");
	sx_session *session = (sx_session *) s;

	session->spotify_thread = pthread_self();

	int next_timeout = 0;
	pthread_mutex_lock(&session->notify_mutex);

	for (;;) {
		if (next_timeout == 0) {
			while(!session->notify_do) {
				pthread_cond_wait(&session->notify_cond, &session->notify_mutex);
			}
		} else {
			sx_waitfor(&session->notify_cond, &session->notify_mutex, next_timeout);
		}

		session->notify_do = 0;
		pthread_mutex_unlock(&session->notify_mutex);

		do {
			pthread_mutex_lock(&session->spotify_mutex);
			sp_session_process_events(session->spotify_session, &next_timeout);
			pthread_mutex_unlock(&session->spotify_mutex);
		} while (next_timeout == 0);

		pthread_mutex_lock(&session->notify_mutex);
	}
}

sp_track *sx_spotify_track_for_url(sx_session *session, const char *url) {
	sp_track *track = NULL;

	pthread_mutex_lock(&session->spotify_mutex);
	sp_link *link = sp_link_create_from_string(url);

	if (link) {
		track = sp_link_as_track(link);
		if (track) {
			sp_track_add_ref(track);
		}

		sp_link_release(link);
	}

	pthread_mutex_unlock(&session->spotify_mutex);

	return track;
}

sp_album *sx_sp_album_for_url(sx_session *session, const char *url) {
	sp_album *album = NULL;
	
	pthread_mutex_lock(&session->spotify_mutex);
	sp_link *link = sp_link_create_from_string(url);
	
	if (link) {
		album = sp_link_as_album(link);
		if (album) {
			sp_album_add_ref(album);
		}
		
		sp_link_release(link);
	}
	
	pthread_mutex_unlock(&session->spotify_mutex);
	
	return album;
}

void sx_spotify_add_metadata_listener(sx_session *session, sx_spotify_metadata_listener *listener, void *data) {
	spotify_metadata_listener_list_item *item = malloc(sizeof(spotify_metadata_listener_list_item));
	item->listener = listener;
	item->data = data;
	item->previous = NULL;
	item->next = NULL;

	pthread_mutex_lock(&session->spotify_metadata_listeners_lock);

	if (!session->spotify_metadata_listeners) {
		session->spotify_metadata_listeners = item;
	}
	else {
		item->next = session->spotify_metadata_listeners;
		session->spotify_metadata_listeners->previous = item;
		session->spotify_metadata_listeners = item;
	}

	pthread_mutex_unlock(&session->spotify_metadata_listeners_lock);
}

static spotify_metadata_listener_list_item *remove_metadata_listener_list_item(sx_session *session, spotify_metadata_listener_list_item *item) {
	spotify_metadata_listener_list_item *next = item->next;
	if (item->previous) {
		item->previous->next = item->next;
	}
	else {
		session->spotify_metadata_listeners = item->next;
	}
	if (next) {
		next->previous = item->previous;
	}
	free(item);
	return next;
}

static sxp_loader *sxp_create_loader() {
	sxp_loader *loader = malloc(sizeof(sxp_loader));
	bzero(loader, sizeof(sxp_loader));
	return loader;
}

static void sxp_release_loader(sxp_loader *loader) {
	if (loader->free) {
		free(loader);
	}
	else {
		loader->free = true;
	}
}


void sx_spotify_load_track(sx_session *session, sp_track *track, sx_callback *callback, void *data) {
	pthread_mutex_lock(&session->spotify_mutex);
	bool is_loaded = sp_track_is_loaded(track);
	pthread_mutex_unlock(&session->spotify_mutex);

	if (is_loaded) {
		callback(session, data);
	}
	else {
		sxp_loader *loader = sxp_create_loader();
		loader->callback = callback;
		loader->sp_obj = track;
		loader->data = data;
		sx_spotify_add_metadata_listener(session, track_loaded_metadata_callback, loader);
	}
}

static bool track_loaded_metadata_callback(sx_session *session, void *data) {
	sxp_loader *loader = (sxp_loader *) data;
	sp_track *track = (sp_track *) loader->sp_obj;

	bool is_loaded = sp_track_is_loaded(track);

	if (is_loaded) {
		loader->callback(session, loader->data);
		free(loader);
	}

	return is_loaded;
}

static void image_loaded(sp_image *image, void *userdata) {
	sxp_loader *loader = (sxp_loader *) userdata;
	pthread_cond_signal(&loader->cond);
	sxp_release_loader(loader);
}

sx_spotify_image *sx_spotify_get_album_cover(sx_session *session, sp_album *album) {
	sp_error err;
	sx_spotify_image *result = malloc(sizeof(sx_spotify_image));

	sxp_loader *loader = sxp_create_loader();
	loader->session = session;
	pthread_cond_init(&loader->cond, NULL);

	pthread_mutex_lock(&session->spotify_mutex);

	result->sp_image = sp_image_create(session->spotify_session, sp_album_cover(album));

	bool is_loaded = sp_image_is_loaded(result->sp_image);

	if (!is_loaded) {
		sp_image_add_load_callback(result->sp_image, image_loaded, loader);

		sx_waitfor(&loader->cond, &session->spotify_mutex, 2000);
		sp_image_remove_load_callback(result->sp_image, image_loaded, loader);
	}

	err = sp_image_error(result->sp_image);
	if (err != SP_ERROR_OK) {
		sx_log(session, "error loading image: %s", sp_error_message(err));
		sp_image_release(result->sp_image);
		free(result);
		result = NULL;
	}
	else {
		result->data = sp_image_data(result->sp_image, &result->size);
	}

	pthread_mutex_unlock(&session->spotify_mutex);
	pthread_cond_destroy(&loader->cond);
	sxp_release_loader(loader);

	return result;
}

void sx_spotify_free_image(sx_session *session, sx_spotify_image *image) {
	pthread_mutex_lock(&session->spotify_mutex);
	// FIXME this causes crashes. leaking.
	sp_image_release(image->sp_image);
	free(image);
	pthread_mutex_unlock(&session->spotify_mutex);
}

sx_spotify_image *sx_spotify_get_current_album_cover(sx_session *session) {
	if (!session->track) {
		return NULL;
	}

	sp_album *album = sp_track_album(session->track);
	if (!album) {
		// TODO handle metadata not loaded
		return NULL;
	}
	sx_spotify_load_album_sync(session, album, -1);
	return sx_spotify_get_album_cover(session, album);
}

bool album_loaded_sync_metadata_callback(sx_session *session, void *data) {
	sxp_loader *loader = (sxp_loader *) data;
	sp_album *album = loader->sp_obj;
	pthread_mutex_lock(&session->spotify_mutex);
	bool is_loaded = sp_album_is_loaded(album);
	if (is_loaded) {
		pthread_cond_signal(&loader->cond);
	}
	pthread_mutex_unlock(&session->spotify_mutex);
	sxp_release_loader(loader);
	return is_loaded;
}

bool sx_spotify_load_album_sync(sx_session *session, sp_album *album, int timeout) {
	pthread_mutex_lock(&session->spotify_mutex);

	bool is_loaded = sp_album_is_loaded(album);
	if (!is_loaded) {
		sxp_loader *loader = sxp_create_loader();
		loader->sp_obj = album;
		pthread_cond_init(&loader->cond, NULL);
		sx_spotify_add_metadata_listener(session, album_loaded_sync_metadata_callback, loader);
		do {
			if (timeout > 0) {
				sx_waitfor(&loader->cond, &session->spotify_mutex, timeout);
			}
			else {
				pthread_cond_wait(&loader->cond, &session->spotify_mutex);
			}
			is_loaded = sp_album_is_loaded(album);
		} while (timeout < 0 && !is_loaded);
		sxp_release_loader(loader);
	}
	pthread_mutex_unlock(&session->spotify_mutex);
	return is_loaded;
}

bool track_loaded_sync_metadata_callback(sx_session *session, void *data) {
	sxp_loader *loader = (sxp_loader *) data;
	sp_track *track = loader->sp_obj;
	bool is_loaded = sp_track_is_loaded(track);
	if (is_loaded) {
		pthread_cond_signal(&loader->cond);
	}
	sxp_release_loader(loader);
	return is_loaded;
}

bool sx_spotify_load_track_sync(sx_session *session, sp_track *track, int timeout) {
	pthread_mutex_lock(&session->spotify_mutex);

	bool is_loaded = sp_track_is_loaded(track);
	if (!is_loaded) {
		sxp_loader *loader = sxp_create_loader();
		loader->sp_obj = track;
		pthread_cond_init(&loader->cond, NULL);
		sx_spotify_add_metadata_listener(session, track_loaded_sync_metadata_callback, loader);
		do {
			if (timeout > 0) {
				sx_waitfor(&loader->cond, &session->spotify_mutex, timeout);
			}
			else {
				pthread_cond_wait(&loader->cond, &session->spotify_mutex);
			}
			is_loaded = sp_track_is_loaded(track);
		} while (timeout < 0 && !is_loaded);
		sxp_release_loader(loader);
	}
	pthread_mutex_unlock(&session->spotify_mutex);

	return is_loaded;
}

sp_album *sx_spotify_load_album_for_track_sync(sx_session *session, sp_track *track, int timeout) {
	if (!sx_spotify_load_track_sync(session, track, timeout)) {
		sx_log(session, "track failed to load");
		return NULL;
	}
	sp_album *album = sp_track_album(track);
	if (album) {
		if (!sx_spotify_load_album_sync(session, album, timeout)) {
			sx_log(session, "album failed to load");
			return NULL;
		}
	}
	return album;
}
