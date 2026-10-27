#ifndef _IN_LOADSPLASH_H
#define _IN_LOADSPLASH_H

#include <PR/ultratypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Perfect Dark–themed full-screen status while unpacking texture packs or the
 * XBLA package. No-ops if video is not up yet. Pump SDL events so the OS does
 * not mark the app hung.
 */
void loadSplashBegin(const char *title, const char *detail);
void loadSplashSetProgress(s32 done, s32 total);
void loadSplashEnd(void);

#ifdef __cplusplus
}
#endif

#endif
