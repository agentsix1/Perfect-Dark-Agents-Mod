#ifndef _IN_ARCHIVE_H
#define _IN_ARCHIVE_H

#include <PR/ultratypes.h>

#ifdef __cplusplus
extern "C" {
#endif

s32 archiveIsSupported(const char *path);
s32 archiveIsZip(const char *path);
s32 archiveExtract(const char *path, const char *destDir);
s32 archiveFindEntry(const char *path, const char *needle);

typedef s32 (*archivefilter)(const char *name, void *arg);
s32 archiveExtractMatching(const char *path, const char *destDir, archivefilter filter, void *arg);
void archiveGetProgress(s32 *done, s32 *total);

/** Zip-only helper used by texture packs. */
s32 archiveExtractZip(const char *path, const char *destDir);

#ifdef __cplusplus
}
#endif

#endif
