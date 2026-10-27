#ifndef _IN_MODLOADER_H
#define _IN_MODLOADER_H

#include <PR/ultratypes.h>

static inline s32 modloaderDirIndexIsConversion(s32 i)
{
	(void)i;
	return 0;
}

static inline s32 modTextureExists(u16 num)
{
	(void)num;
	return 0;
}

static inline s32 modloaderStageIsRemake(s32 stage) { (void)stage; return 0; }
static inline s32 modloaderStageIsGexPlus(s32 stage) { (void)stage; return 0; }

#endif

