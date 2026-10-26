#include <ultra64.h>
#include "lib/sched.h"
#include "constants.h"
#include "game/bondmove.h"
#include "game/cheats.h"
#include "game/chraction.h"
#include "game/debug.h"
#include "game/chr.h"
#include "game/prop.h"
#include "game/propsnd.h"
#include "game/objectives.h"
#include "game/game_096360.h"
#include "game/bondgun.h"
#include "game/gunfx.h"
#include "game/game_0b0fd0.h"
#include "game/modelmgr.h"
#include "game/tex.h"
#include "game/camera.h"
#include "game/player.h"
#include "game/playermgr.h"
#include "game/vtxstore.h"
#include "game/gfxmemory.h"
#include "game/explosions.h"
#include "game/smoke.h"
#include "game/sparks.h"
#include "game/bg.h"
#include "game/file.h"
#include "game/mplayer/setup.h"
#include "game/bot.h"
#include "game/botact.h"
#include "game/mplayer/mplayer.h"
#include "game/pad.h"
#include "game/propobj.h"
#include "game/splat.h"
#include "game/wallhit.h"
#include "bss.h"
#include "lib/vi.h"
#include "lib/main.h"
#include "lib/model.h"
#include "lib/rng.h"
#include "lib/mtx.h"
#include "lib/anim.h"
#include "lib/collision.h"
#include "data.h"
#include "types.h"
#ifndef PLATFORM_N64
#include "net/net.h"
#include "net/netdbg.h"
#endif

void propsTick(void)
{
	s32 i;
	struct prop *prop;
	struct prop *next;
	struct prop *next2;
	s32 done;
	s32 tickop;

	for (i = 0; i < PLAYERCOUNT(); i++) {
		g_Vars.players[i]->bondextrapos.x = 0;
		g_Vars.players[i]->bondextrapos.y = 0;
		g_Vars.players[i]->bondextrapos.z = 0;
	}

	shieldhitsTick();
	chraTickBg();

#ifndef PLATFORM_N64
	// Equip flush may defer_delete; reap after so the list walk stays clean.
	if (g_NetMode != NETMODE_NONE) {
		if (g_NetMode == NETMODE_CLIENT) {
			netClientSimFlushEquips();
		}
		netReapDeferredDeletes();
	}
#endif

	prop = g_Vars.activeprops;
#ifndef PLATFORM_N64
	{
		s32 safety = g_Vars.maxprops + 8;
#endif

	do {
#ifndef PLATFORM_N64
		if (!prop || --safety < 0) {
			netdbgEvent("warn", "\"action\":\"propsTick_walk_safety\",\"prop_null\":%d",
					prop ? 0 : 1);
			propsRepairActiveList(prop ? "proptick_cycle" : "proptick_null");
			break;
		}
#endif
#ifndef PLATFORM_N64
		if (!propIsInPool(prop)) {
			netdbgEvent("warn", "\"action\":\"propsTick_cur_oob\"");
			propsRepairActiveList("proptick_cur_oob");
			break;
		}
#endif
		next = prop->next;
#ifndef PLATFORM_N64
		if (next && next != g_Vars.pausedprops && !propIsInPool(next)) {
			netdbgEvent("warn",
					"\"action\":\"propsTick_next_oob\",\"syncid\":%u",
					(unsigned)prop->syncid);
			propsRepairActiveList("proptick_next_oob");
			break;
		}
#endif
		done = next == g_Vars.pausedprops;
		tickop = TICKOP_NONE;

		if (prop->type == PROPTYPE_CHR) {
			tickop = chrTickBeams(prop);
		} else if (prop->type == PROPTYPE_OBJ || prop->type == PROPTYPE_WEAPON || prop->type == PROPTYPE_DOOR) {
#ifndef PLATFORM_N64
			/* Freed/deferred guns can still sit on a repaired activeprops chain. */
			if (!prop->obj) {
				netdbgEvent("warn",
						"\"action\":\"propsTick_obj_null\",\"syncid\":%u,\"type\":%u",
						(unsigned)prop->syncid, (unsigned)prop->type);
				tickop = TICKOP_FREE;
			} else
#endif
			tickop = objTick(prop);
		} else if (prop->type == PROPTYPE_EXPLOSION) {
			tickop = explosionTick(prop);
		} else if (prop->type == PROPTYPE_SMOKE) {
			tickop = smokeTick(prop);
		} else if (prop->type == PROPTYPE_PLAYER) {
			tickop = playerTickBeams(prop);
		}

		// FREE/DISABLE/GIVE must keep the pre-tick next — re-reading after
		// objTick/objFree can pick up a sibling that was just reaped.
		if (tickop == TICKOP_CHANGEDLIST
				|| tickop == TICKOP_FREE
				|| tickop == TICKOP_DISABLE
				|| tickop == TICKOP_GIVETOPLAYER) {
			next2 = next;
			done = next2 == g_Vars.pausedprops;
			propExecuteTickOperation(prop, tickop);
		} else {
			next2 = prop->next;
#ifndef PLATFORM_N64
			if (next2 && next2 != g_Vars.pausedprops && !propIsInPool(next2)) {
				propsRepairActiveList("proptick_next2_oob");
				break;
			}
#endif
			done = next2 == g_Vars.pausedprops;

			if (tickop == TICKOP_RETICK) {
				propDelist(prop);
				propActivateThisFrame(prop);

				if (done) {
					next2 = prop;
					done = false;
				}
			} else {
				propExecuteTickOperation(prop, tickop);
			}
		}

		prop = next2;
	} while (!done);
#ifndef PLATFORM_N64
	}
#endif
}
