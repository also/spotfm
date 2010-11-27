#ifndef sx_spotify_h
#define sx_spotify_h

#include "sx.h"

void sx_spotify_init(sx_session *session);
void *sx_spotify_run(void *session);
sp_track *sx_spotify_track_for_url(sx_session *session, const char *url);

#endif
