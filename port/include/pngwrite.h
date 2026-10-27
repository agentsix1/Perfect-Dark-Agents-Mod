#ifndef _IN_PNGWRITE_H
#define _IN_PNGWRITE_H

#include <PR/ultratypes.h>

static inline s32 pngWriteFile(const char *path, const void *rgba, s32 w, s32 h)
{
	(void)path;
	(void)rgba;
	(void)w;
	(void)h;
	return 0;
}

static inline s32 pngWrite(const char *path, const u8 *pixels, s32 width, s32 height, s32 channels, s32 bottomRowFirst)
{
	(void)channels;
	(void)bottomRowFirst;
	return pngWriteFile(path, pixels, width, height);
}


#endif

