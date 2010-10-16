#include <stdbool.h>

typedef struct sxxxxxxx_session sxxxxxxx_session;

void sxxxxxxx_init(struct sxxxxxxx_session **session, const char *username, const char *password);
void sxxxxxxx_run(struct sxxxxxxx_session *session, bool thread);