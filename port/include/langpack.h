#ifndef _IN_LANGPACK_H
#define _IN_LANGPACK_H

#include <PR/ultratypes.h>

static inline const char *langTr(const char *en) { return en; }
static inline const char *langTrCtx(const char *ctx, const char *en)
{
	(void)ctx;
	return en;
}

#endif
