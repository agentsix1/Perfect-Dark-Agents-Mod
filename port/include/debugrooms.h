#ifndef _IN_PORT_DEBUGROOMS_H
#define _IN_PORT_DEBUGROOMS_H

#include <ultra64.h>
#include "types.h"

/**
 * Testing aid: tint Skedar's registered KOH hill rooms (not every BG room)
 * in any gamemode so hill zones are obvious while debugging occupancy.
 *
 * Hill 1 purple, 2 red, 3 orange, 4 green, 5 yellow, 6 brown, else blue.
 * Toggle: Debug.SkedarRoomColours=1 in pd.ini (default on).
 */
extern s32 g_DebugSkedarRoomColours;

bool debugSkedarRoomColoursActive(void);
void debugSkedarApplyRoomColour(RoomNum room, s32 *r, s32 *g, s32 *b);
Gfx *debugSkedarRenderPadHud(Gfx *gdl);

#endif
