#ifndef _IN_FS_H
#define _IN_FS_H

#include <stdio.h>
#include <PR/ultratypes.h>

#define FS_MAXPATH 1024

s32 fsInit(void);

const char *fsFullPath(const char *relPath);

s32 fsPathIsAbsolute(const char *path);
s32 fsPathIsCwdRelative(const char *path);

void *fsFileLoad(const char *name, u32 *outSize);
s32 fsFileLoadTo(const char *name, void *dst, u32 dstSize);
s32 fsFileSize(const char *name);

FILE *fsFileOpenWrite(const char *name);
FILE *fsFileOpenRead(const char *name);
void fsFileFree(FILE *f);

const char *fsGetModDir(void);
s32 fsCreateDir(const char *path);

typedef void (*fsScanCallback)(const char *name, void *arg);
s32 fsScanDir(const char *path, fsScanCallback cb, void *arg);
s32 fsRemoveFile(const char *path);
s32 fsChooseOutputDir(const char *name, char *dst, u32 dstSize);

#define FS_ADDED_CONTENT_DIR "added-content"
#define FS_ADDED_CONTENT_SEARCH \
	"$E/" FS_ADDED_CONTENT_DIR, \
	"$H/" FS_ADDED_CONTENT_DIR, \
	"./" FS_ADDED_CONTENT_DIR, \
	"$S/" FS_ADDED_CONTENT_DIR

s32 fsAddedContentDir(char *dst, u32 dstSize);

#endif

