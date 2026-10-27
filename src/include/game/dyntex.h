#ifndef _IN_GAME_DYNTEX_H
#define _IN_GAME_DYNTEX_H
#include <ultra64.h>
#include "data.h"
#include "types.h"

void dyntexTickRoom(s32 roomnum, Vtx *vertices);
void dyntexAddVertex(Vtx *vertex);
void dyntexSetCurrentType(s16 type);
void dyntexSetCurrentRoom(RoomNum roomnum);
void dyntexReset(void);
#ifndef PLATFORM_N64
/** Drop per-room dyntex offsets so rooms can reload from another copy (XBLA F6). */
void dyntexForgetRooms(void);
#endif

bool dyntexHasRoom(void);

#endif
