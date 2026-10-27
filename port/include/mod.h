#ifndef _IN_MOD_H
#define _IN_MOD_H

#include <PR/ultratypes.h>

#define MOD_CONFIG_FNAME "modconfig.txt"

struct animtableentry;

s32 modConfigLoad(const char *path);

s32 modTextureLoad(u16 num, void *dst, u32 dstSize);

s32 modAnimationLoadDescriptor(u16 num, struct animtableentry *anim);
void *modAnimationLoadData(u16 num);

void *modSequenceLoad(u16 num, u32 *outSize);

#ifndef PLATFORM_N64
static inline s32 modSetTextureFromStage(s32 on)
{
	(void)on;
	return 0;
}
static inline s32 modSetTextureSourceMod(s32 dir)
{
	(void)dir;
	return -1;
}
#endif

#endif

