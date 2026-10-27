#ifndef _IN_GEBEANSTAGE_H
#define _IN_GEBEANSTAGE_H

#include <PR/ultratypes.h>
#include <PR/gbi.h>

static inline void gebeanStageLevelReset(void) {}
static inline s32 gebeanStageDrawsEveryRoom(void) { return 0; }
static inline u32 gebeanStageRoomSize(s32 roomnum) { (void)roomnum; return 0; }
static inline s32 gebeanStageRoomRead(s32 roomnum, void *dst, u32 len)
{
	(void)roomnum;
	(void)dst;
	(void)len;
	return 0;
}
static inline s32 gebeanStageOwnsRecord(s32 record) { (void)record; return 0; }
static inline void *gebeanStageTile(s32 record) { (void)record; return NULL; }
static inline s32 gebeanStageRecordIsWater(s32 record) { (void)record; return 0; }
static inline Gfx *gebeanStageScrollSlot(s32 record, s32 w, s32 h)
{
	(void)record;
	(void)w;
	(void)h;
	return NULL;
}

#endif
