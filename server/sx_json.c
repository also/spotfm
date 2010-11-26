#include "sx_json.h"

#include "string.h"

yajl_gen_status yajl_gen_string0(yajl_gen g, const char * str) {
	return yajl_gen_string(g, (unsigned char *) str, strlen(str));
}
