#include <stdio.h>
#include <ultra64.h>
#include "constants.h"
#include "bss.h"
#include "data.h"
#include "types.h"
#include "platform.h"
#include "config.h"
#include "system.h"
#include "game/pad.h"
#include "game/game_1531a0.h"
#include "game/options.h"
#include "lib/vi.h"
#include "pads.h"
#include "debugrooms.h"

s32 g_DebugSkedarRoomColours = 1;

/* Skedar MP hill pads (same order as mp_setupoat intro / scenarioReset). */
static const s16 s_SkedarHillPads[] = {
	PAD_OAT_0093,
	PAD_OAT_00D5,
	PAD_OAT_008D,
	PAD_OAT_00DA,
	PAD_OAT_0076,
};

static RoomNum s_HillRooms[ARRAYCOUNT(s_SkedarHillPads)];
static s32 s_HillRoomCount;
static s32 s_ResolvedStage = -1;

/**
 * Resolve hill pad → room once per Skedar load so we only tint KOH hill rooms,
 * not the whole arena.
 */
static void debugSkedarEnsureHillRooms(void)
{
	s32 i;
	struct pad pad;

	if (s_ResolvedStage == g_Vars.stagenum && s_HillRoomCount > 0) {
		return;
	}

	s_ResolvedStage = g_Vars.stagenum;
	s_HillRoomCount = 0;

	if (g_Vars.stagenum != STAGE_MP_SKEDAR) {
		return;
	}

	for (i = 0; i < ARRAYCOUNT(s_SkedarHillPads); i++) {
		padUnpack(s_SkedarHillPads[i], PADFIELD_ROOM, &pad);
		if (pad.room >= 0) {
			s_HillRooms[s_HillRoomCount++] = pad.room;
		}
	}
}

/**
 * Hill-list index for this BG room, or -1 if it is not a Skedar hill room.
 * First hill = 0 (purple), second = 1 (red), …
 */
static s32 debugSkedarHillIndexForRoom(RoomNum room)
{
	s32 i;

	debugSkedarEnsureHillRooms();

	for (i = 0; i < s_HillRoomCount; i++) {
		if (s_HillRooms[i] == room) {
			return i;
		}
	}

	return -1;
}

bool debugSkedarRoomColoursActive(void)
{
	if (!g_DebugSkedarRoomColours || g_Vars.stagenum != STAGE_MP_SKEDAR) {
		return false;
	}

	debugSkedarEnsureHillRooms();
	return s_HillRoomCount > 0;
}

/**
 * Tint only registered Skedar hill rooms:
 * 1 purple, 2 red, 3 orange, 4 green, 5 yellow, 6 brown, further hills blue.
 * Non-hill rooms are left unchanged.
 */
void debugSkedarApplyRoomColour(RoomNum room, s32 *r, s32 *g, s32 *b)
{
	s32 hillindex;
	s32 max;
	f32 br;
	f32 fr;
	f32 fg;
	f32 fb;

	if (!r || !g || !b) {
		return;
	}

	hillindex = debugSkedarHillIndexForRoom(room);
	if (hillindex < 0) {
		return;
	}

	max = *r;
	if (*g > max) {
		max = *g;
	}
	if (*b > max) {
		max = *b;
	}
	br = (f32)max;
	if (br < 48.0f) {
		br = 48.0f;
	}

	switch (hillindex) {
	case 0: /* hill 1 — purple */
		fr = 0.70f; fg = 0.18f; fb = 1.00f;
		break;
	case 1: /* hill 2 — red */
		fr = 1.00f; fg = 0.14f; fb = 0.14f;
		break;
	case 2: /* hill 3 — orange */
		fr = 1.00f; fg = 0.48f; fb = 0.10f;
		break;
	case 3: /* hill 4 — green */
		fr = 0.14f; fg = 1.00f; fb = 0.20f;
		break;
	case 4: /* hill 5 — yellow */
		fr = 1.00f; fg = 0.95f; fb = 0.14f;
		break;
	case 5: /* hill 6 — brown */
		fr = 0.62f; fg = 0.32f; fb = 0.10f;
		break;
	default: /* extra hills */
		fr = 0.18f; fg = 0.48f; fb = 1.00f;
		break;
	}

	*r = (s32)(br * fr);
	*g = (s32)(br * fg);
	*b = (s32)(br * fb);

	if (*r > 255) {
		*r = 255;
	}
	if (*g > 255) {
		*g = 255;
	}
	if (*b > 255) {
		*b = 255;
	}
}

/**
 * Top-center HUD (same spot as the KOH hill timer): 4-digit hex of the pad
 * nearest the local player. Active with Debug.SkedarRoomColours on Skedar.
 */
Gfx *debugSkedarRenderPadHud(Gfx *gdl)
{
	struct prop *prop;
	struct coord *pos;
	struct pad pad;
	s32 padnum;
	s32 bestpad;
	s32 numpads;
	f32 bestdist;
	f32 dx;
	f32 dy;
	f32 dz;
	f32 dist;
	s32 x;
	s32 y;
	s32 textwidth;
	s32 textheight;
	char text[16];
	u32 colour;
	s32 playercount;

	if (!g_DebugSkedarRoomColours
			|| g_Vars.stagenum != STAGE_MP_SKEDAR
			|| !g_Vars.normmplayerisrunning
			|| !g_PadsFile
			|| !g_Vars.currentplayer
			|| !g_Vars.currentplayer->prop) {
		return gdl;
	}

	prop = g_Vars.currentplayer->prop;
	pos = &prop->pos;
	numpads = g_PadsFile->numpads;
	bestpad = -1;
	bestdist = 1e12f;

	for (padnum = 0; padnum < numpads; padnum++) {
		padUnpack(padnum, PADFIELD_POS, &pad);
		dx = pos->x - pad.pos.x;
		dy = pos->y - pad.pos.y;
		dz = pos->z - pad.pos.z;
		dist = dx * dx + dy * dy + dz * dz;
		if (dist < bestdist) {
			bestdist = dist;
			bestpad = padnum;
		}
	}

	if (bestpad < 0) {
		return gdl;
	}

	sprintf(text, "%04X", bestpad);

	x = viGetViewLeft() + viGetViewWidth() / 2;
	y = viGetViewTop() + 10;
	colour = 0xffffffff;

	playercount = PLAYERCOUNT();
	if (playercount < 2 || (playercount == 2 && optionsGetScreenSplit() == SCREENSPLIT_HORIZONTAL)) {
		gSPSetExtraGeometryModeEXT(gdl++, G_ASPECT_CENTER_EXT);
	}

	gDPSetTextureFilter(gdl++, G_TF_POINT);
	gDPSetColorDither(gdl++, G_CD_DISABLE);
	gSPClearGeometryMode(gdl++, G_ZBUFFER);
	gDPPipeSync(gdl++);
	gDPSetTexturePersp(gdl++, G_TP_NONE);
	gDPSetCycleType(gdl++, G_CYC_1CYCLE);
	gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);

	gdl = text0f153628(gdl);
	textMeasure(&textheight, &textwidth, text, g_CharsHandelGothicXs, g_FontHandelGothicXs, 0);
	x -= textwidth / 2;
	textwidth += x;
	textheight += y;
	gdl = text0f153990(gdl, x, y, textwidth, textheight);
	gdl = textRender(gdl, &x, &y, text, g_CharsHandelGothicXs, g_FontHandelGothicXs,
			colour, 0xa0, viGetWidth(), viGetHeight(), 0, 0);
	gdl = text0f153780(gdl);

	gSPClearExtraGeometryModeEXT(gdl++, G_ASPECT_MODE_EXT);

	return gdl;
}

PD_CONSTRUCTOR static void debugRoomsConfigInit(void)
{
	configRegisterInt("Debug.SkedarRoomColours", &g_DebugSkedarRoomColours, 0, 1);
}
