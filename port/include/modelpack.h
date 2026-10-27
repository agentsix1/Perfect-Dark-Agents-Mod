#ifndef _IN_MODELPACK_H
#define _IN_MODELPACK_H
#include <PR/ultratypes.h>

#define MODELPACK_PREFER_N64  0
#define MODELPACK_PREFER_XBLA 1

struct objmesh;

static inline s32 modelpackHaveMesh(s32 fileid) { (void)fileid; return 0; }
static inline s32 modelpackHavePacks(void) { return 0; }
static inline struct objmesh *modelpackGetMesh(s32 fileid) { (void)fileid; return NULL; }
static inline void *modelpackFindN64(s32 fileid) { (void)fileid; return NULL; }
static inline void *modelpackFindXbla(s32 fileid) { (void)fileid; return NULL; }
static inline s32 modelpackGetPrefer(void) { return MODELPACK_PREFER_XBLA; }
static inline s32 modelpackGetGeneration(void) { return 0; }
struct objmaterial;
static inline const void *modelpackBindMaterial(const struct objmaterial *mat, s32 *outAlpha, s32 *outSoft)
{
	(void)mat;
	if (outAlpha) {
		*outAlpha = 0;
	}
	if (outSoft) {
		*outSoft = 0;
	}
	return NULL;
}

static inline u8 *modelpackDecodeN64Texture(s32 texturenum, s32 *outW, s32 *outH)
{
	(void)texturenum;
	(void)outW;
	(void)outH;
	return NULL;
}

#endif

