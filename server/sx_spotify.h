#ifndef sx_spotify_h
#define sx_spotify_h

#include "sx.h"

typedef bool (sxp_metadata_listener) (sx_session *session, void *data);

typedef struct sxp_image {
	sp_image *sp_image;
	size_t size;
	const void * data;
} sxp_image;

void sxp_init(sx_session *session);
void *sxp_run(void *session);

sp_track *sxp_track_for_url(sx_session *session, const char *url);
sp_album *sx_sp_album_for_url(sx_session *session, const char *url);

void sxp_add_metadata_listener(sx_session *session, sxp_metadata_listener *listener, void *data);

void sxp_load_track(sx_session *session, sp_track *track, sx_callback *callback, void *data);

bool sxp_load_album_sync(sx_session *session, sp_album *album, int timeout);
bool sxp_load_track_sync(sx_session *session, sp_track *track, int timeout);

sp_album *sxp_load_album_for_track_sync(sx_session *session, sp_track *track, int timeout);

sxp_image *sxp_get_current_album_cover(sx_session *session);
sxp_image *sxp_get_album_cover(sx_session *session, sp_album *album);
void sxp_free_image(sx_session *session, sxp_image *image);

void sxp_lock(sx_session *session);
void sxp_unlock(sx_session *session);

#endif
