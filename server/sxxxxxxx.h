#include <stdbool.h>

typedef struct sxxxxxxx_session sxxxxxxx_session;

typedef struct sxxxxxxx_session_config {
	void (*log)(void * data, const char *message);
} sxxxxxxx_session_config;

void sxxxxxxx_init(struct sxxxxxxx_session **session, sxxxxxxx_session_config * config, const char *username, const char *password);
void sxxxxxxx_run(struct sxxxxxxx_session *session, bool thread);
void sxxxxxxx_stop(sxxxxxxx_session *session);
void sxxxxxxx_resume(sxxxxxxx_session *session);
void sxxxxxxx_previous(sxxxxxxx_session *session);
void sxxxxxxx_seek(sxxxxxxx_session *session, int offset);
void sxxxxxxx_next(sxxxxxxx_session *session);
void sxxxxxxx_toggle_play(sxxxxxxx_session *session);
