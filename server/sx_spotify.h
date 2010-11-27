#ifndef sx_spotify_h
#define sx_spotify_h

#include "sx.h"

typedef bool (sx_spotify_metadata_listener) (sx_session *session, void *data);

void sx_spotify_init(sx_session *session);
void *sx_spotify_run(void *session);
sp_track *sx_spotify_track_for_url(sx_session *session, const char *url);

void sx_spotify_add_metadata_listener(sx_session *session, sx_spotify_metadata_listener *listener, void *data);

void sx_spotify_load_track(sx_session *session, sp_track *track, sx_callback *callback, void *data);

#endif
