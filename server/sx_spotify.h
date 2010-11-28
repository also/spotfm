#ifndef sx_spotify_h
#define sx_spotify_h

#include "sx.h"

typedef bool (sx_spotify_metadata_listener) (sx_session *session, void *data);

typedef struct sx_spotify_image {
	sp_image *sp_image;
	size_t size;
	const void * data;
} sx_spotify_image;

void sx_spotify_init(sx_session *session);
void *sx_spotify_run(void *session);

sp_track *sx_spotify_track_for_url(sx_session *session, const char *url);
sp_album *sx_sp_album_for_url(sx_session *session, const char *url);

void sx_spotify_add_metadata_listener(sx_session *session, sx_spotify_metadata_listener *listener, void *data);

void sx_spotify_load_track(sx_session *session, sp_track *track, sx_callback *callback, void *data);

bool sx_spotify_load_album_sync(sx_session *session, sp_album *album, int timeout);
bool sx_spotify_load_track_sync(sx_session *session, sp_track *track, int timeout);

sp_album *sx_spotify_load_album_for_track_sync(sx_session *session, sp_track *track, int timeout);

sx_spotify_image *sx_spotify_get_current_album_cover(sx_session *session);
sx_spotify_image *sx_spotify_get_album_cover(sx_session *session, sp_album *album);
void sx_spotify_free_image(sx_session *session, sx_spotify_image *image);

void sxp_lock(sx_session *session);
void sxp_unlock(sx_session *session);

#endif
