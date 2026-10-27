#ifndef _IN_TEXPACK_H
#define _IN_TEXPACK_H

#include <PR/ultratypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TEXPACK_PACKS_DIR "texture-packs"

/** Create texture-packs/ (and cache parent) once the filesystem is ready. */
void texpackInit(void);

/**
 * Texture identity registry — pairs decompressed texel addresses with their
 * texture numbers so the renderer can look up pack replacements.
 */
void texpackRegisterTexture(const void *data, s32 texturenum);
void texpackForgetTexture(const void *data);
void texpackForgetAll(void);
s32 texpackGetTextureNum(const void *data);

/**
 * Load an RGBA32 replacement for the texture at data (bottom-up N64 row order).
 * Returns NULL when packs are off / Default / no file — caller keeps ROM path.
 */
u8 *texpackLoadReplacement(const void *data, s32 *outWidth, s32 *outHeight);
void texpackFreeReplacement(u8 *rgba);

s32 texpackHaveReplacements(void);
s32 texpackHaveReplacementFor(s32 texturenum);

/** Decode by texture number on the calling thread (no registry address). */
u8 *texpackDecodeReplacementNow(s32 texturenum, s32 *outWidth, s32 *outHeight);

/* Pack picker (Extended Options) — index 0 is always "Default" (ROM). */
void texpackRefreshPacks(void);
s32 texpackGetNumPacks(void); /* count of installed packs (excludes Default) */
const char *texpackGetPackName(s32 index); /* 0..numPacks-1 */
s32 texpackGetSelectedPack(void); /* -1 = Default */
void texpackSetSelectedPack(s32 index); /* -1 = Default */
s32 texpackSelectPackByName(const char *name);
const char *texpackGetSelectedPackName(void); /* "" when Default */
const char *texpackGetPacksDirPath(void);

void texpackReload(void);
s32 texpackGetNumReplacements(void);

/**
 * Phase 2 XBLA stubs — return "no replacement" until XBLA support lands.
 * Pack scan still indexes pd-xbla/ and xbla/ into a separate table so Phase 2
 * is additive.
 */
s32 texpackHaveXblaReplacement(s32 record);
s32 texpackXblaReplacementIsSelectedPack(s32 record);
u8 *texpackLoadXblaReplacement(s32 record, s32 *outWidth, s32 *outHeight);
u8 *texpackDecodeXblaReplacementNow(s32 record, s32 *outWidth, s32 *outHeight);
s32 texpackGetNumXblaReplacements(void);
s32 texpackXblaRecordFromId(s32 id);

/** Dab-compat: packs are "enabled" when a non-Default pack is selected. */
s32 texpackLoadEnabled(void);
void texpackSetLoadEnabled(s32 enabled);

/** Bumps when the pack index/selection changes (XBLA matte scan). */
s32 texpackGetIndexSerial(void);

/** Dab dump-to-pack helpers (unused unless Mod dump path is on). */
static inline s32 texpackDumpEnabled(void) { return 0; }
static inline void texpackDumpXblaRecord(const u8 *rgba, u32 w, u32 h, u32 record)
{
	(void)rgba;
	(void)w;
	(void)h;
	(void)record;
}


#ifdef __cplusplus
}
#endif

#endif


