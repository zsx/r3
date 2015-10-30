#include <stdlib.h>
#include <string.h>

#include "reb-host.h"
#include "host-renderer.h"

REBRDR *rebol_renderer;

extern REBRDR rebrdr_agg;
extern REBRDR rebrdr_nanovg;

static REBRDR* renderers[] = {
	&rebrdr_nanovg,
	&rebrdr_agg,
};

REBRDR *init_renderer()
{
	REBRDR *r;
	int i;

	const char *r_env = SDL_getenv("R3_RENDERER");
	for(i = 0; i < sizeof(renderers) / sizeof(REBRDR*); i ++) {
		r = renderers[i];
		if (r_env) {
			if (r->name && !strcmp(r_env, r->name)) {
				/* only try the one specified by env */
				if (!r->init(r)) return r;
				else return NULL;
			}
		} else {
			if (!r->init(r)) return r;
		}
	}
	return NULL;
}
