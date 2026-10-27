/**
 * No-op stubs for Dab-only GoldenEye / sheen / GPU-mesh helpers.
 * Perfect Dark XBLA textures/meshes/rooms do not need these paths at runtime.
 */
#include <PR/ultratypes.h>
#include <PR/gbi.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "gebean.h"
#include "gfxmesh.h"
#include "roomsheen.h"
#include "romdata.h"
#include "xblaagent4.h"
#include "game/hudmsg.h"
#include "game/title.h"
#include "lib/main.h"
#include "constants.h"

#ifndef PLATFORM_N64

s32 gebeanGetEnabled(void) { return 0; }
s32 gebeanIsAvailable(void) { return 0; }
s32 gebeanFindRow(u16 fileid, struct modeldef *modeldef)
{
	(void)fileid;
	(void)modeldef;
	return -1;
}
const char *gebeanRowName(s32 row)
{
	(void)row;
	return "bean";
}
s32 gebeanRowIsPool(s32 row) { (void)row; return 0; }
s32 gebeanRowIsFemale(s32 row) { (void)row; return 0; }
s32 gebeanRowIsFirstPerson(s32 row) { (void)row; return 0; }
s32 gebeanRowIsProp(s32 row) { (void)row; return 0; }
s32 gebeanRowIsChr(s32 row) { (void)row; return 0; }
s32 gebeanRowKeepsHood(s32 bodyrow, s32 headrow)
{
	(void)bodyrow;
	(void)headrow;
	return 0;
}
void gebeanBuildFailed(s32 row, u16 fileid)
{
	(void)row;
	(void)fileid;
}
void gebeanMeshesSwitched(void) {}
s32 gebeanPrepare(void) { return 0; }
s32 gebeanListNodeMatrix(const struct modelnode *node)
{
	(void)node;
	return 0;
}
s32 gebeanListLoadedMatrix(const struct modelnode *node)
{
	(void)node;
	return -1;
}
u8 *gebeanBuild(s32 row, s32 original, struct modeldef *modeldef, struct modelnode **nodes, s32 numnodes,
		struct gebeanmats *mats, u64 *outAbsent, u32 *outLen)
{
	(void)row;
	(void)original;
	(void)modeldef;
	(void)nodes;
	(void)numnodes;
	(void)mats;
	if (outAbsent) {
		*outAbsent = 0;
	}
	if (outLen) {
		*outLen = 0;
	}
	return NULL;
}

s32 gfxMeshGpuAvailable(void) { return 0; }
void gfxMeshForget(struct gfxmesh *mesh)
{
	(void)mesh;
}

Gfx *roomSheenTexgenShift(Gfx *gdl) { return gdl; }
Gfx *roomSheenTexgenTurn(Gfx *gdl) { return gdl; }
s32 roomSheenGetStockFollow(void) { return 0; }
void roomSheenSetStockFollow(s32 on) { (void)on; }
Gfx *roomSheenStockBegin(Gfx *gdl) { return gdl; }
Gfx *roomSheenStockEnd(Gfx *gdl) { return gdl; }
Gfx *roomSheenStockResume(Gfx *gdl) { return gdl; }

s32 romdataFilesGeneration(void) { return 1; }
s32 romdataFileIsStock(s32 fileNum)
{
	(void)fileNum;
	return 1;
}
s32 romdataFileGetXblaId(s32 fileNum)
{
	(void)fileNum;
	return 0;
}
s32 romdataFileGetModDir(s32 fileNum)
{
	(void)fileNum;
	return -1;
}

s32 netLookData(u32 *out)
{
	if (out) {
		*out = 0;
	}
	return 0;
}
void netStageHashNote(s32 loadtype, u32 crc, u32 len)
{
	(void)loadtype;
	(void)crc;
	(void)len;
}


void hudmsgSayToggle(const char *ontext, const char *offtext, s32 on)
{
	const char *text = on ? ontext : offtext;

	if (text) {
		hudmsgCreate((char *)text, 0);
	}
}

bool titleIsBootSequence(void)
{
	return !STAGE_IS_LEVEL(mainGetStageNum());
}

#endif /* PLATFORM_N64 */
