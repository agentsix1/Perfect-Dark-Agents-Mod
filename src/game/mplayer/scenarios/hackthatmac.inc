/**
 * Hack that Mac (also known as Hacker Central)
 *
 * A data uplink and a terminal are spawned into a random location in the arena.
 * Players must pick up the data uplink then go to the terminal and download
 * data from it to score points. Each download takes 20 seconds and the player
 * cannot use weapons while uplinking.
 */

#define HTM_NUM_TERMINALS 1

struct menuitem g_HtmOptionsMenuItems[] = {
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_222, // "One-Hit Kills"
		MPOPTION_ONEHITKILLS,
		menuhandlerMpOneHitKills,
	},
	{
		MENUITEMTYPE_DROPDOWN,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_223, // "Slow Motion"
		0,
		menuhandlerMpSlowMotion,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_224, // "Fast Movement"
		MPOPTION_FASTMOVEMENT,
		menuhandlerMpCheckboxOption,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_225, // "Display Team"
		MPOPTION_DISPLAYTEAM,
		menuhandlerMpDisplayTeam,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_226, // "No Radar"
		MPOPTION_NORADAR,
		menuhandlerMpCheckboxOption,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_227, // "No Auto-Aim"
		MPOPTION_NOAUTOAIM,
		menuhandlerMpCheckboxOption,
	},
#ifndef PLATFORM_N64
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR | MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"Start Armed",
		MPOPTION_SPAWNWITHWEAPON,
		menuhandlerMpCheckboxOption,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR | MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"No Drug Blur",
		MPOPTION_NODRUGBLUR,
		menuhandlerMpCheckboxOption,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_OPTIONS_257, // "Friendly Fire"
		MPOPTION_FRIENDLYFIRE,
		menuhandlerMpDisplayTeam,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR | MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"No Player on Radar",
		MPOPTION_NOPLAYERONRADAR,
		menuhandlerMpCheckboxOption,
	},
#endif
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_OPTIONS_493, // "Kills Score"
		MPOPTION_KILLSSCORE,
		menuhandlerMpCheckboxOption,
	},
	{
		MENUITEMTYPE_SEPARATOR,
		0,
		0,
		0,
		0,
		NULL,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_231, // "Highlight Terminal"
		MPOPTION_HTM_HIGHLIGHTTERMINAL,
		menuhandlerMpCheckboxOption,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_238, // "Show on Radar"
		MPOPTION_HTM_SHOWONRADAR,
		menuhandlerMpCheckboxOption,
	},
	{
		MENUITEMTYPE_SLIDER,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_235, // "Time"
		0x0000006e,
		menuhandlerMpHillTime,
	},
#ifndef PLATFORM_N64
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR | MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"Mobile Computer",
		MPOPTION_HTM_MOBILECOMPUTER,
		menuhandlerMpCheckboxOption,
	},
#endif
	{
		MENUITEMTYPE_SEPARATOR,
		0,
		0,
		0,
		0,
		NULL,
	},
	{
		MENUITEMTYPE_SELECTABLE,
		0,
		MENUITEMFLAG_SELECTABLE_CLOSESDIALOG,
		L_MPMENU_239, // "Back"
		0,
		NULL,
	},
	{ MENUITEMTYPE_END },
};

struct menudialogdef g_HtmOptionsMenuDialog = {
	MENUDIALOGTYPE_DEFAULT,
	L_MPMENU_217, // "Hacker Options"
	g_HtmOptionsMenuItems,
	mpOptionsMenuDialog,
	MENUDIALOGFLAG_MPLOCKABLE,
	NULL,
};

struct weaponobj g_HtmUplinkObj;

void htmInit(void)
{
	g_ScenarioData.htm.uplink = NULL;
}

/**
 * Add one for the data uplink.
 */
s32 htmNumProps(void)
{
	return HTM_NUM_TERMINALS + 1;
}

void htmAddPad(s16 padnum)
{
	struct scenariodata_htm *data = &g_ScenarioData.htm;

#if VERSION >= VERSION_NTSC_1_0
	if (data->numpads < ARRAYCOUNT(g_ScenarioData.htm.padnums))
#endif
	{
		osSyncPrintf("HackThatMacAddBankPad -> Adding New Pad %d  - Pad Id = %d-> Saving Pad\n", data->numpads, padnum);

		data->padnums[data->numpads] = padnum;
		data->numpads++;
	}
}

void htmReset(void)
{
	s32 i;

	osSyncPrintf("HackThatMacReset -> Working\n");

	g_ScenarioData.htm.numpads = 0;
	g_ScenarioData.htm.numterminals = 0;
	g_ScenarioData.htm.unk138 = 0;
	g_ScenarioData.htm.dlplayernum = -1;
	g_ScenarioData.htm.playernuminrange = -1;
	g_ScenarioData.htm.dlterminalnum = -1;
	g_ScenarioData.htm.unk140 = 0;

	for (i = 0; i < ARRAYCOUNT(g_ScenarioData.htm.numpoints); i++) {
		g_ScenarioData.htm.numpoints[i] = 0;
		g_ScenarioData.htm.dltime240[i] = 0;
	}

	for (i = 0; i < ARRAYCOUNT(g_ScenarioData.htm.padnums); i++) {
		g_ScenarioData.htm.padnums[i] = -1;
	}

	for (i = 0; i < HTM_NUM_TERMINALS; i++) {
		g_ScenarioData.htm.terminals[i].unk00 = 0;
		g_ScenarioData.htm.terminals[i].prop = NULL;
		g_ScenarioData.htm.terminals[i].padnum = -1;
		g_ScenarioData.htm.terminals[i].team = 255;
		g_ScenarioData.htm.terminals[i].unk0b = 255;
	}
}

void htbCreateUplink(void)
{
	struct weaponobj template = {
		512,                    // extrascale
		0,                      // hidden2
		OBJTYPE_WEAPON,         // type
		MODEL_CHRDATATHIEF,     // modelnum
		0,                      // pad
		OBJFLAG_FALL | OBJFLAG_INVINCIBLE | OBJFLAG_FORCENOBOUNCE,
		OBJFLAG2_IMMUNETOGUNFIRE | OBJFLAG2_IMMUNETOEXPLOSIONS,
		0,                      // flags3
		NULL,                   // prop
		NULL,                   // model
		1, 0, 0,                // realrot
		0, 1, 0,
		0, 0, 1,
		0,                      // hidden
		NULL,                   // geo
		NULL,                   // projectile
		0,                      // damage
		1000,                   // maxdamage
		0xff, 0xff, 0xff, 0x00, // shadecol
		0xff, 0xff, 0xff, 0x00, // nextcol
		0x0fff,                 // floorcol
		0,                      // tiles
		WEAPON_DATAUPLINK,      // weaponnum
		0,                      // unk5d
		0,                      // unk5e
		FUNC_PRIMARY,           // gunfunc
		0,                      // fadeouttimer60
		-1,                     // dualweaponnum
		-1,                     // timer240
		NULL,                   // dualweapon
	};

	struct prop *prop = g_Vars.activeprops;
	struct defaultobj *obj;
	s32 padnum;
	s32 count = 0;
	struct defaultobj *candidates[20];

#ifndef PLATFORM_N64
	{
		s32 walksafety = g_Vars.maxprops + 8;
#endif
	while (prop && count < 20) {
#if VERSION >= VERSION_NTSC_1_0
		if (prop->type == PROPTYPE_OBJ)
#endif
		{
			obj = prop->obj;

			if (obj && obj->type == OBJTYPE_MULTIAMMOCRATE) {
				candidates[count] = obj;
				count++;
			}
		}

#ifndef PLATFORM_N64
		prop = propListNext(prop, &walksafety, "htm_uplink_crates");
#else
		prop = prop->next;
#endif
	}
#ifndef PLATFORM_N64
	}
#endif

	if (count > 0) {
		count = rngRandom() % count;
		var800869ec = candidates[count];
		var800869ec->hidden |= OBJHFLAG_DELETING;
		var800869ec->hidden2 |= OBJH2FLAG_CANREGEN;
		scenarioNetHideReplacedCrate(var800869ec);
		padnum = var800869ec->pad;
	} else if (g_ScenarioData.htm.numpads > 0) {
		padnum = g_ScenarioData.htm.padnums[rngRandom() % g_ScenarioData.htm.numpads];
	} else {
		padnum = 0;
	}

	g_HtmUplinkObj = template;
	g_HtmUplinkObj.base.pad = padnum;

	setupPlaceWeapon(&g_HtmUplinkObj, 999);

	g_HtmUplinkObj.base.hidden2 &= ~OBJH2FLAG_CANREGEN;

	g_ScenarioData.htm.uplink = g_HtmUplinkObj.base.prop;

	if (g_ScenarioData.htm.uplink) {
		g_ScenarioData.htm.uplink->forcetick = true;
	}
}

void htmInitProps(void)
{
	struct scenariodata_htm *data = &g_ScenarioData.htm;
	struct prop *prop = g_Vars.activeprops;
	s32 i = 0;
	s32 rand;

	osSyncPrintf("HackThatMacInitProps -> Start : %d Bank Pads\n", data->numterminals);

#ifndef PLATFORM_N64
	{
		s32 walksafety = g_Vars.maxprops + 8;
#endif
	while (prop) {
		if (prop->type == PROPTYPE_OBJ && prop->obj) {
			struct defaultobj *obj = prop->obj;

			if (obj->type == OBJTYPE_AMMOCRATE || obj->type == OBJTYPE_MULTIAMMOCRATE) {
				if (obj->modelnum == MODEL_MULTI_AMMO_CRATE) {
					osSyncPrintf("HackThatMacInitProps -> Adding prop %d (%x)\n", i, obj->pad);
					htmAddPad(obj->pad);
				}
			}
		}

#ifndef PLATFORM_N64
		prop = propListNext(prop, &walksafety, "htmInitProps");
#else
		prop = prop->next;
#endif
		i++;
	}
#ifndef PLATFORM_N64
	}
#endif

	osSyncPrintf("HackThatMacInitProps -> Mid : %d Bank Pads\n", data->numpads);
	osSyncPrintf("HackThatMacInitProps -> Generating %d random box pads from %d in the bank\n", htmNumProps(), data->numpads);

	data->numterminals = 0;

#ifndef PLATFORM_N64
	/* Empty pad bank → % 0 UB / hang; skip terminal create. */
	if (data->numpads <= 0) {
		osSyncPrintf("HackThatMacInitProps -> no pads in bank, skipping terminals\n");
	} else
#endif
	// @bug: This should be < htmNumProps() - 1 to account for the data uplink,
	// or just < HTM_NUM_TERMINALS. If HTM_NUM_TERMINALS is set to 7 then it
	// will overflow the array.
	// Keep padnums intact (do not zero used slots) so Mobile Computer can
	// pick a different pad after each successful download.
	while (data->numterminals < htmNumProps()) {
		s32 padnum;
		s32 already;
		s32 j;
#ifndef PLATFORM_N64
		s32 padtries = data->numpads + 8;
#endif

		do {
			already = false;
			rand = rngRandom() % data->numpads;
			padnum = data->padnums[rand];
			if (padnum > 0) {
				for (j = 0; j < data->numterminals; j++) {
					if (data->terminals[j].padnum == padnum) {
						already = true;
						break;
					}
				}
			}
#ifndef PLATFORM_N64
			if (--padtries <= 0) {
				padnum = -1;
				break;
			}
#endif
		} while (padnum <= 0 || already);

#ifndef PLATFORM_N64
		if (padnum <= 0) {
			break;
		}
#endif
		data->terminals[data->numterminals].padnum = padnum;
		data->numterminals++;
	}

	osSyncPrintf("HackThatMacInitProps -> %d/%d Random box pads generated - Listing\n", data->numterminals, htmNumProps());

	for (i = 0; i < data->numterminals; i++) {
		osSyncPrintf("Pad %d -> Pad Id = %d\n", i, data->terminals[i].padnum);
	}

	for (i = 0; i < data->numterminals && i < HTM_NUM_TERMINALS; i++) {
		data->terminals[i].prop = scenarioCreateObj(MODEL_GOODPC, data->terminals[i].padnum, 0.2f,
				OBJFLAG_FALL | OBJFLAG_INVINCIBLE | OBJFLAG_FORCENOBOUNCE,
				OBJFLAG2_IMMUNETOGUNFIRE | OBJFLAG2_IMMUNETOEXPLOSIONS,
				OBJFLAG3_HTMTERMINAL | OBJFLAG3_INTERACTABLE);
		osSyncPrintf("HackThatMacInitProps -> Building and adding custom prop %d - Pad=%d, Ptr=%08x\n",
				i, data->terminals[i].padnum, data->terminals[i].prop);
		htbRemoveAmmoCrateAtPad(data->terminals[i].padnum);
	}

	var800869ec = NULL;

	htbCreateUplink();

	osSyncPrintf("HackThatMacInitProps -> End\n");
}

void htmTick(void)
{
	u8 stack[8];
	s32 i;
	u32 prevplayernum = g_Vars.currentplayernum;
	struct prop *prop;

	if (var800869ec && var800869ec->prop) {
		if (g_ScenarioData.htm.uplink == NULL || g_ScenarioData.htm.uplink->type != PROPTYPE_WEAPON) {
			var800869ec = NULL;
		} else {
			var800869ec->prop->timetoregen = TICKS(1200);
		}
	}

	g_ScenarioData.htm.uplink = NULL;

	// Check if uplink is on the ground
	prop = g_Vars.activeprops;
#ifndef PLATFORM_N64
	{
		s32 walksafety = g_Vars.maxprops + 8;
#endif

	while (prop) {
		if (prop->type == PROPTYPE_WEAPON && prop->weapon) {
			struct weaponobj *weapon = prop->weapon;

			if (weapon->weaponnum == WEAPON_DATAUPLINK) {
#ifndef PLATFORM_N64
				/* DELETING/GONE = mid-pickup reap. Skip or we recreate into
				 * g_HtmUplinkObj while deferred free still owns it → AV. */
				if (prop->obj && (prop->obj->hidden & (OBJHFLAG_DELETING | OBJHFLAG_GONE))) {
					/* ignore */
				} else if (prop->parent) {
					g_ScenarioData.htm.uplink = prop->parent;
				} else {
					g_ScenarioData.htm.uplink = prop;
				}
#else
				g_ScenarioData.htm.uplink = prop;
#endif
			}
		}

#ifndef PLATFORM_N64
		prop = propListNext(prop, &walksafety, "htmTick_uplink");
#else
		prop = prop->next;
#endif
	}
#ifndef PLATFORM_N64
	}
#endif

	// Check if a player is holding it
	if (g_ScenarioData.htm.uplink == NULL) {
		for (i = 0; i < PLAYERCOUNT(); i++) {
			setCurrentPlayerNum(i);

#ifndef PLATFORM_N64
			if (g_Vars.currentplayer->client && g_Vars.currentplayer->client->leftmatch) {
				continue;
			}
#endif

			if (invHasSingleWeaponExcAllGuns(WEAPON_DATAUPLINK)) {
				g_ScenarioData.htm.uplink = g_Vars.currentplayer->prop;
				break;
			}
		}
	}

	setCurrentPlayerNum(prevplayernum);

	// Check if a simulant is holding it
	if (g_ScenarioData.htm.uplink == NULL) {
		for (i = PLAYERCOUNT(); i < g_MpNumChrs; i++) {
			if (g_MpAllChrPtrs[i]->aibot->hasuplink) {
				g_ScenarioData.htm.uplink = g_MpAllChrPtrs[i]->prop;
				break;
			}
		}
	}

	if (g_ScenarioData.htm.uplink == NULL) {
#ifndef PLATFORM_N64
		/*
		 * Do not respawn while the previous static uplink is still queued for
		 * deferred free — setupPlaceWeapon would clobber g_HtmUplinkObj.
		 */
		if (g_NetMode != NETMODE_NONE
				&& g_HtmUplinkObj.base.prop
				&& (g_HtmUplinkObj.base.hidden & OBJHFLAG_DELETING)) {
			/* wait for netReapDeferredDeletes */
		} else
#endif
		{
			htbCreateUplink();
			scenarioNetBroadcastProp(g_ScenarioData.htm.uplink);
		}
	}
}

/**
 * Pick a pad from the HTM bank that is not the current terminal pad.
 * Returns -1 if no alternate pad exists.
 */
static s16 htmChooseMobilePad(s16 excludepad)
{
	struct scenariodata_htm *data = &g_ScenarioData.htm;
	s16 candidates[ARRAYCOUNT(g_ScenarioData.htm.padnums)];
	s32 count = 0;
	s32 i;
	s32 j;

	for (i = 0; i < data->numpads; i++) {
		s16 padnum = data->padnums[i];
		bool used = false;

		if (padnum <= 0 || padnum == excludepad) {
			continue;
		}

		for (j = 0; j < data->numterminals && j < HTM_NUM_TERMINALS; j++) {
			if (data->terminals[j].prop && data->terminals[j].padnum == padnum) {
				used = true;
				break;
			}
		}

		if (!used) {
			candidates[count++] = padnum;
		}
	}

	if (count <= 0) {
		return -1;
	}

	return candidates[rngRandom() % count];
}

/**
 * Move an existing terminal prop to a new pad (host). Syncs MOVE so clients
 * update radar / highlight without a new SPAWN.
 *
 * Use func0f06a730 (same as CTC home / first spawn settle) so the PC sits on
 * the floor — padGetCentre + func0f06a580 left later pads floating in Y.
 */
static void htmMoveTerminalToPad(s32 terminalindex, s16 padnum)
{
	struct scenariodata_htm *data = &g_ScenarioData.htm;
	struct prop *prop;
	struct defaultobj *obj;
	struct pad pad;
	Mtxf mtx;
	RoomNum rooms[2];

	if (terminalindex < 0 || terminalindex >= HTM_NUM_TERMINALS) {
		return;
	}

	prop = data->terminals[terminalindex].prop;

	if (!prop || !prop->obj || padnum <= 0) {
		return;
	}

#ifndef PLATFORM_N64
	if (!propIsInPool(prop)) {
		return;
	}
#endif

	obj = prop->obj;
	padUnpack(padnum, PADFIELD_POS | PADFIELD_LOOK | PADFIELD_UP | PADFIELD_ROOM, &pad);

	if (pad.room <= 0) {
		return;
	}

	rooms[0] = pad.room;
	rooms[1] = -1;

	mtx00016d58(&mtx, 0, 0, 0,
			-pad.look.x, -pad.look.y, -pad.look.z,
			pad.up.x, pad.up.y, pad.up.z);

	if (obj->model) {
		mtx00015f04(obj->model->scale, &mtx);
	}

	func0f06a730(obj, &pad.pos, &mtx, rooms, &pad.pos);

	obj->pad = padnum;
	data->terminals[terminalindex].padnum = padnum;

	htbRemoveAmmoCrateAtPad(padnum);

#ifndef PLATFORM_N64
	if (g_NetMode == NETMODE_SERVER) {
		netPropEnsureSyncId(prop);
		prop->forcetick = true;
		netmsgSvcPropMoveWrite(&g_NetMsgRel, prop, NULL);
		netmsgSvcScenarioWrite(&g_NetMsgRel);
	}
#endif
}

static void htmMaybeRelocateTerminal(s32 terminalindex)
{
	s16 oldpad;
	s16 newpad;

	if ((g_MpSetup.options & MPOPTION_HTM_MOBILECOMPUTER) == 0) {
		return;
	}

	if (terminalindex < 0 || terminalindex >= HTM_NUM_TERMINALS) {
		return;
	}

	oldpad = g_ScenarioData.htm.terminals[terminalindex].padnum;
	newpad = htmChooseMobilePad(oldpad);

	if (newpad > 0) {
		htmMoveTerminalToPad(terminalindex, newpad);
	}
}

void htmTickChr(struct chrdata *chr)
{
	struct scenariodata_htm *data = &g_ScenarioData.htm;
	bool hasuplink;
	s32 playernum;
	s32 i;
	s32 *time;

	if (chr) {
		hasuplink = chr->aibot->hasuplink;
		playernum = mpPlayerGetIndex(chr);
	} else {
		hasuplink = invHasDataUplink();
		playernum = g_Vars.currentplayernum;
	}

	time = &data->dltime240[playernum];

	for (i = 0; i < HTM_NUM_TERMINALS; i++) {
		if (data->terminals[i].prop) {
			struct prop *prop = data->terminals[i].prop;
			struct defaultobj *obj;
			s32 activatedbyplayernum = -1;

#ifndef PLATFORM_N64
			if (!propIsInPool(prop) || !prop->obj) {
				data->terminals[i].prop = NULL;
				continue;
			}
#endif
			obj = prop->obj;

			if (chr) {
				if (hasuplink) {
					activatedbyplayernum = playernum;
				}
			} else {
				if (obj->hidden & OBJHFLAG_ACTIVATED_BY_BOND) {
					activatedbyplayernum = (obj->hidden & 0xf0000000) >> 28;
				}
			}

			if (playernum == activatedbyplayernum) {
				obj->hidden &= ~OBJHFLAG_ACTIVATED_BY_BOND;
				obj->hidden &= ~0xf0000000;

				if (hasuplink) {
					if (data->dlterminalnum == -1) {
						data->dlterminalnum = i;
						data->dlplayernum = playernum;
						data->playernuminrange = playernum;
						*time = 0;

						if (chr == NULL) {
#if VERSION >= VERSION_JPN_FINAL
							scenarioNetHudmsg(langGet(L_MPWEAPONS_018), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE | HUDMSGFLAG_NOWRAP, playernum); // "Starting download."
#else
							scenarioNetHudmsg(langGet(L_MPWEAPONS_018), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE, playernum); // "Starting download."
#endif
							scenarioNetPsCreate(data->terminals[data->dlterminalnum].prop, SFX_01BF, true);
						}
					}
				} else {
					if (chr == NULL) {
#if VERSION >= VERSION_JPN_FINAL
						scenarioNetHudmsg(langGet(L_MPWEAPONS_019), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE | HUDMSGFLAG_NOWRAP, playernum); // "You need to use the Data Uplink."
#else
						scenarioNetHudmsg(langGet(L_MPWEAPONS_019), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE, playernum); // "You need to use the Data Uplink."
#endif
						scenarioNetSnd(SFX_01CC);
					}
				}
			}
		}
	}

	if (playernum == data->dlplayernum && data->dlterminalnum != -1
#ifndef PLATFORM_N64
			&& data->dlterminalnum < HTM_NUM_TERMINALS
			&& data->terminals[data->dlterminalnum].prop
			&& propIsInPool(data->terminals[data->dlterminalnum].prop)
#endif
			) {
		struct coord *terminalpos = &data->terminals[data->dlterminalnum].prop->pos;
		f32 angle;
		f32 relangle;
		f32 rangexz;
		f32 rangey;
		struct coord *chrpos;
		struct coord dist;
		bool holdinguplink;
#ifndef PLATFORM_N64
		s32 anglesafety;
#endif

		if (chr) {
			if (!chr->prop) {
				return;
			}
			chrpos = &chr->prop->pos;
			angle = (M_BADTAU - chrGetInverseTheta(chr)) * 57.295776367188f;
			holdinguplink = chr->aibot->weaponnum == WEAPON_UNARMED;
		} else {
			if (!g_Vars.currentplayer || !g_Vars.currentplayer->prop) {
				return;
			}
			chrpos = &g_Vars.currentplayer->prop->pos;
			angle = g_Vars.currentplayer->vv_theta;
			holdinguplink = invHasDataUplink();
		}

		dist.x = terminalpos->x - chrpos->x;
		dist.y = terminalpos->y - chrpos->y;
		dist.z = terminalpos->z - chrpos->z;

		rangexz = sqrtf(dist.x * dist.x + dist.z * dist.z);

		rangey = ABS(dist.y);

		relangle = atan2f(dist.x, dist.z) * 57.295776367188f + angle;

#ifndef PLATFORM_N64
		/* NaN/Inf would hang the wrap loops forever mid-download. */
		if (!(relangle == relangle) || relangle > 1.0e10f || relangle < -1.0e10f) {
			relangle = 0;
		}
		anglesafety = 16;
#endif

		while (relangle < 180
#ifndef PLATFORM_N64
				&& --anglesafety >= 0
#endif
				) {
			relangle += 360;
		}

#ifndef PLATFORM_N64
		anglesafety = 16;
#endif
		while (relangle > 180
#ifndef PLATFORM_N64
				&& --anglesafety >= 0
#endif
				) {
			relangle -= 360;
		}

		if (relangle > 0) {
			// empty
		} else {
			relangle = -relangle;
		}

		osSyncPrintf("HTM : Player %d - Term Pos = (%d,%d,%d)", playernum, (s32)terminalpos->x, (s32)terminalpos->y, (s32)terminalpos->z);
		osSyncPrintf("HTM : Player %d - Play Pos = (%d,%d,%d)", playernum, (s32)chrpos->x, (s32)chrpos->y, (s32)chrpos->z);
		osSyncPrintf("HTM : Player %d - T/P  Rel = (%d,%d,%d)", playernum, (s32)dist.x, (s32)dist.y, (s32)dist.z);

		osSyncPrintf("HTM : Player %d - Range XZ = %d", playernum, rangexz);
		osSyncPrintf("HTM : Player %d - Range Y  = %d", playernum, rangey);
		osSyncPrintf("HTM : Player %d - Angle XZ = %d", playernum, relangle);

		{
			f32 maxxz = 250;
			f32 maxy = 200;
			f32 maxangle = 45;
			bool outofzone;
#ifndef PLATFORM_N64
			static s32 s_HtmOutGrace240[MAX_MPCHRS];
			bool remoteplayer = false;

			/*
			 * Host remotes: look angle and inv lag break mid-download even when
			 * the client is still at the terminal. Widen range, ignore facing,
			 * trust uplink token, and grace before "Connection broken."
			 */
			if (!chr && scenarioPlayerIsHostRemote(g_Vars.currentplayer)) {
				remoteplayer = true;
				maxxz = 320;
				maxy = 260;
				if (!holdinguplink
						&& !g_Vars.currentplayer->isdead
						&& data->uplink == g_Vars.currentplayer->prop) {
					holdinguplink = true;
				}
			}
#endif

			outofzone = (rangexz > maxxz || rangey > maxy || relangle > maxangle || !holdinguplink);

#ifndef PLATFORM_N64
			if (remoteplayer) {
				/* Facing is unreliable on remotes — do not break for angle alone. */
				outofzone = (rangexz > maxxz || rangey > maxy || !holdinguplink);

				if (outofzone) {
					if (playernum >= 0 && playernum < MAX_MPCHRS) {
						s_HtmOutGrace240[playernum] += g_Vars.lvupdate240;
						if (s_HtmOutGrace240[playernum] < SCENARIO_REMOTE_ZONE_GRACE240) {
							outofzone = false;
						}
					}
				} else if (playernum >= 0 && playernum < MAX_MPCHRS) {
					s_HtmOutGrace240[playernum] = 0;
				}
			}
#endif

			if (outofzone) {
				if (rangexz < maxxz && rangey < maxy) {
					data->playernuminrange = playernum;
				} else {
					data->playernuminrange = -1;
				}

				if (chr == NULL) {
					// "Connection broken."
#if VERSION >= VERSION_JPN_FINAL
					scenarioNetHudmsg(langGet(L_MPWEAPONS_017), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE | HUDMSGFLAG_NOWRAP, playernum);
#else
					scenarioNetHudmsg(langGet(L_MPWEAPONS_017), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE, playernum);
#endif
					scenarioNetPsStop(data->terminals[data->dlterminalnum].prop);
					scenarioNetSnd(SFX_01CC);
				}

				data->dlterminalnum = -1;
				data->dlplayernum = -1;
				*time = 0;
#ifndef PLATFORM_N64
				if (playernum >= 0 && playernum < MAX_MPCHRS) {
					s_HtmOutGrace240[playernum] = 0;
				}
#endif
			} else {
				*time += g_Vars.lvupdate240;

				if (*time > scenarioPointDuration240()) {
					s32 completedterminal = data->dlterminalnum;

					data->numpoints[playernum]++;
					data->playernuminrange = playernum;

					if (chr == NULL) {
						// "Download successful."
#if VERSION >= VERSION_JPN_FINAL
						scenarioNetHudmsg(langGet(L_MPWEAPONS_016), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE | HUDMSGFLAG_NOWRAP, playernum);
#else
						scenarioNetHudmsg(langGet(L_MPWEAPONS_016), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE, playernum);
#endif
						scenarioNetPsStop(data->terminals[completedterminal].prop);
						scenarioNetSnd(SFX_01C1);
					}

					data->dlterminalnum = -1;
					data->dlplayernum = -1;
					*time = 0;
#ifndef PLATFORM_N64
					if (playernum >= 0 && playernum < MAX_MPCHRS) {
						s_HtmOutGrace240[playernum] = 0;
					}
#endif

					/* Mobile Computer: move terminal so the uplink holder must relocate. */
					htmMaybeRelocateTerminal(completedterminal);
				}
			}
		}

		osSyncPrintf("HTM : Player %d - Dwnld Plr=%d, Dwnld Prop=%d\n", playernum, data->playernuminrange, data->terminals[data->dlterminalnum].prop);
		osSyncPrintf("HTM : Player %d - Download Time = %d", playernum, *time);
	}
}

Gfx *htmRenderHud(Gfx *gdl)
{
	struct scenariodata_htm *data = &g_ScenarioData.htm;
	s32 dltime;
	s32 viewleft;
	s32 viewright;
	s32 viewtop;
	s32 a0;
	s32 a1;
	s32 barleft;
	s32 barright;
	s32 t1;
	s32 t6;
	s32 v1;
	s32 s1;

	dltime = data->dltime240[g_Vars.currentplayernum];

	if (data->dlterminalnum != -1 && g_Vars.currentplayernum == data->dlplayernum) {
		viewleft = viGetViewLeft();
		viewright = viGetViewLeft() + viGetViewWidth();
		viewtop = viGetViewTop();
		t6 = (viewleft + viewright) / 2;
		a1 = viGetViewWidth() / 3;
		barleft = t6 - a1 / 2;
		barright = t6 + a1 / 2;
		{
			const f32 pointdur = (f32)scenarioPointDuration240();
			s1 = barleft + (s32)(a1 * (dltime / (pointdur > 0.0f ? pointdur : 1.0f)));
		}

		gdl = text0f153628(gdl);
		gdl = textSetPrimColour(gdl, 0x60000060);

		gDPFillRectangle(gdl++, barleft, viewtop + 8, barright, viewtop + 16);

		gdl = text0f153838(gdl);
		gdl = textSetPrimColour(gdl, 0xc00000d0);

		v1 = barleft + 1;
		a0 = barleft;

#ifndef PLATFORM_N64
		/* Corrupt/huge dltime240 made this spin for millions of fills (freeze). */
		if (s1 > barright) {
			s1 = barright;
		}
		if (s1 < barleft) {
			s1 = barleft;
		}
#endif

		while (v1 < s1) {
			gDPFillRectangle(gdl++, a0, viewtop + 8, v1, viewtop + 16);
			v1 += 2;
			a0 += 2;
		}

		gdl = text0f153838(gdl);
		gdl = text0f153780(gdl);
	}

	return gdl;
}

void htmCalculatePlayerScore(struct mpchrconfig *mpchr, s32 mpchrnum, s32 *score, s32 *deaths)
{
	struct mpchrconfig *loopmpchr;
	s32 i;
	s32 index;

	*score = 0;
	index = func0f18d0e8(mpchrnum);

	if (index >= 0) {
		*score += g_ScenarioData.htm.numpoints[index] * 2;
	}

	if (g_MpSetup.options & MPOPTION_KILLSSCORE) {
		for (i = 0; i != MAX_MPCHRS; i++) {
			if (i == mpchrnum) {
				*score -= mpchr->killcounts[i];
			} else if (g_MpSetup.options & MPOPTION_TEAMSENABLED) {
				loopmpchr = MPCHR(i);

				if (loopmpchr->team == mpchr->team) {
					*score -= mpchr->killcounts[i];
				} else {
					*score += mpchr->killcounts[i];
				}
			} else {
				*score += mpchr->killcounts[i];
			}
		}
	}

	*deaths = mpchr->numdeaths;
}

Gfx *htmRadarExtra(Gfx *gdl)
{
	struct scenariodata_htm *data = &g_ScenarioData.htm;
	struct coord dist;
	s32 i;
	struct coord sp88;

	// Red/green/blue/alpha as float and integer
	f32 rf;
	f32 gf;
	f32 bf;
	f32 af;

	u32 ri;
	u32 gi;
	u32 bi;
	u32 ai;

	if (g_MpSetup.options & MPOPTION_HTM_SHOWONRADAR) {
		// Show the uplink
		if (data->uplink && data->uplink->type != PROPTYPE_PLAYER && data->uplink->type != PROPTYPE_CHR) {
			dist.x = data->uplink->pos.x - g_Vars.currentplayer->prop->pos.x;
			dist.y = data->uplink->pos.y - g_Vars.currentplayer->prop->pos.y;
			dist.z = data->uplink->pos.z - g_Vars.currentplayer->prop->pos.z;

			gdl = radarDrawDot(gdl, data->uplink, &dist, 0x00ff0000, 0x00000000, true);
		}

		// Show the terminal
		for (i = 0; i < HTM_NUM_TERMINALS; i++) {
			if (data->terminals[i].prop) {
				sp88.x = data->terminals[i].prop->pos.x - g_Vars.currentplayer->prop->pos.x;
				sp88.y = data->terminals[i].prop->pos.y - g_Vars.currentplayer->prop->pos.y;
				sp88.z = data->terminals[i].prop->pos.z - g_Vars.currentplayer->prop->pos.z;

				if (data->terminals[i].team == 255) {
					rf = 0;
					gf = 255;
					bf = 0;
					af = 0;
				} else if (g_MpSetup.options & MPOPTION_TEAMSENABLED) {
					u32 colour = g_TeamColours[radarGetTeamIndex(data->terminals[i].team)];
					rf = (colour >> 24) & 0xff;
					gf = ((colour >> 16) & 0xff);
					bf = ((colour >> 8) & 0xff);
					af = colour & 0xff;
				} else {
					rf = 0;
					gf = 255;
					bf = 0;
					af = 0;
				}

				ri = rf;
				gi = gf;
				bi = bf;
				ai = af;

				if (ri > 255) {
					ri = 255;
				}

				if (gi > 255) {
					gi = 255;
				}

				if (bi > 255) {
					bi = 255;
				}

				if (ai > 255) {
					ai = 255;
				}

				gdl = radarDrawDot(gdl, data->terminals[i].prop, &sp88,
						(ri << 24) | (gi << 16) | (bi << 8) | ai,
						0x00000000, true);
			}
		}
	}

	return gdl;
}

bool htmRadarChr(Gfx **gdl, struct prop *prop)
{
	if ((g_MpSetup.options & MPOPTION_HTM_SHOWONRADAR) && g_ScenarioData.htm.uplink) {
		if (prop == g_ScenarioData.htm.uplink &&
				(prop->type == PROPTYPE_PLAYER || prop->type == PROPTYPE_CHR)) {
			struct coord dist;
			dist.x = prop->pos.x - g_Vars.currentplayer->prop->pos.x;
			dist.y = prop->pos.y - g_Vars.currentplayer->prop->pos.y;
			dist.z = prop->pos.z - g_Vars.currentplayer->prop->pos.z;

			if (g_MpSetup.options & MPOPTION_TEAMSENABLED) {
				u32 colour = g_TeamColours[radarGetTeamIndex(prop->chr->team)];
				*gdl = radarDrawDot(*gdl, g_ScenarioData.htm.uplink, &dist, colour, 0, 1);
			} else {
				*gdl = radarDrawDot(*gdl, g_ScenarioData.htm.uplink, &dist, 0x00ff0000, 0, 1);
			}

			return true;
		}
	}

	return false;
}

bool htmHighlightProp(struct prop *prop, s32 *colour)
{
	if (g_MpSetup.options & MPOPTION_HTM_HIGHLIGHTTERMINAL) {
		bool highlight = false;

		if (prop == g_ScenarioData.htm.uplink) {
			highlight = true;
		} else {
			s32 i;

			for (i = 0; i < HTM_NUM_TERMINALS; i++) {
				if (g_ScenarioData.htm.terminals[i].prop == prop) {
					highlight = true;
					break;
				}
			}
		}

		if (highlight) {
			colour[0] = 0;
			colour[1] = 0xff;
			colour[2] = 0;
			colour[3] = 0x40;

			return true;
		}
	}

	return false;
}
