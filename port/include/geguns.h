#ifndef _IN_GEGUNS_H
#define _IN_GEGUNS_H

#include <PR/ultratypes.h>

struct model;

static inline s32 gegunsIsActive(void) { return 0; }

static inline s32 gegunsHandIsSpent(const struct model *model)
{
	(void)model;
	return 0;
}

#endif
