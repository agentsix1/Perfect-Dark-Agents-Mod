#ifndef _IN_HEADFIT_H
#define _IN_HEADFIT_H

#include <PR/ultratypes.h>

struct modeldef;

static inline s32 headfitWasMeasured(const struct modeldef *modeldef)
{
	(void)modeldef;
	return 0;
}

static inline s32 headfitAppliedOffset(const struct modeldef *modeldef)
{
	(void)modeldef;
	return 0;
}

#endif
