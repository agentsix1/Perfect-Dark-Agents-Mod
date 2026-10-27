#ifndef _IN_ROMDATA_H
#define _IN_ROMDATA_H

#include <PR/ultratypes.h>

extern u8 *g_RomFile;
extern u32 g_RomFileSize;
extern const char *g_RomName;

s32 romdataInit(void);

u8 *romdataFileLoad(s32 fileNum, u32 *outSize);
void romdataFilePreprocess(s32 fileNum, s32 loadType, u8 *data, u32 size, u32 *outSize);
void romdataFileFree(s32 fileNum);
const char *romdataFileGetName(s32 fileNum);

u8 *romdataFileGetData(s32 fileNum);
s32 romdataFileGetSize(s32 fileNum);

s32 romdataFileGetNumForName(const char *name);

u8 *romdataSegGetData(const char *segName);
u8 *romdataSegGetDataEnd(const char *segName);
u32 romdataSegGetSize(const char *segName);
u32 romdataFileGetEstimatedSize(const u32 size, const u32 loadtype);

s32 romdataCheckGbcRom(void);

#ifndef PLATFORM_N64
/** Bumps when the loaded file set changes (mod swap); XBLA caches key off it. */
s32 romdataFilesGeneration(void);
s32 romdataFileIsStock(s32 fileNum);
/** Dab: XBLA mesh id for a stock file, or 0. */
s32 romdataFileGetXblaId(s32 fileNum);
/** Dab: which overlay mod directory owns a replaced file, or -1. */
s32 romdataFileGetModDir(s32 fileNum);
#endif

#endif

