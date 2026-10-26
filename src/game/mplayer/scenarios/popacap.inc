/**
 * Pop a Cap
 *
 * At the start of the match, a single player is randomly chosen as the victim.
 * Players must kill the victim in order to score, while the victim scores a
 * point for every "Time" seconds they remain alive (same slider as KOH).
 * Once killed, another living player/sim is chosen at random.
 *
 * victims[] holds host-stable g_MpAllChrPtrs indices (server playernum for
 * humans). Clients must not rewrite that array — local playernum 0 is the
 * joining client, so remapping in place made the client look like the cap
 * whenever the host actually was.
 */

#ifndef PLATFORM_N64
/**
 * Convert a host-stable PAC index into this peer's g_MpAllChrPtrs slot.
 */
static s32 pacVictimLocalIndex(s32 hostidx)
{
	if (g_NetMode == NETMODE_CLIENT && hostidx >= 0 && hostidx < PLAYERCOUNT()) {
		const s32 local = netPlayernumForMpChrIndex(hostidx);
		return (local >= 0) ? local : -1;
	}
	return hostidx;
}

static bool pacAmIVictim(void)
{
	struct scenariodata_pac *data = &g_ScenarioData.pac;
	s32 v;
	s32 mine;

	if (data->victimindex < 0 || data->victimindex >= g_MpNumChrs) {
		return false;
	}

	v = data->victims[data->victimindex];
	if (v < 0) {
		return false;
	}

	if (g_NetMode == NETMODE_CLIENT) {
		mine = netMpChrIndexForPlayernum(g_Vars.currentplayernum);
		return mine >= 0 && mine == v;
	}

	return g_Vars.currentplayernum == v;
}

static struct chrdata *pacGetVictimChr(void)
{
	struct scenariodata_pac *data = &g_ScenarioData.pac;
	s32 local;

	if (data->victimindex < 0 || data->victimindex >= g_MpNumChrs) {
		return NULL;
	}

	local = pacVictimLocalIndex(data->victims[data->victimindex]);
	if (local < 0 || local >= g_MpNumChrs || !g_MpAllChrPtrs[local]) {
		return NULL;
	}

	return g_MpAllChrPtrs[local];
}
#else
static bool pacAmIVictim(void)
{
	struct scenariodata_pac *data = &g_ScenarioData.pac;

	return data->victimindex >= 0
			&& g_Vars.currentplayernum == data->victims[data->victimindex];
}

static struct chrdata *pacGetVictimChr(void)
{
	struct scenariodata_pac *data = &g_ScenarioData.pac;

	if (data->victimindex < 0) {
		return NULL;
	}

	return g_MpAllChrPtrs[data->victims[data->victimindex]];
}
#endif

struct menuitem g_PacOptionsMenuItems[] = {
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
		L_MPMENU_230, // "Highlight Target"
		MPOPTION_PAC_HIGHLIGHTTARGET,
		menuhandlerMpCheckboxOption,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LOCKABLEMINOR,
		L_MPMENU_238, // "Show on Radar"
		MPOPTION_PAC_SHOWONRADAR,
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

struct menudialogdef g_PacOptionsMenuDialog = {
	MENUDIALOGTYPE_DEFAULT,
	L_MPMENU_218, // "Pop a Cap Options"
	g_PacOptionsMenuItems,
	mpOptionsMenuDialog,
	MENUDIALOGFLAG_MPLOCKABLE,
	NULL,
};

void pacReset(void)
{
	s32 i;
	s32 j;
	struct scenariodata_pac *data = &g_ScenarioData.pac;

	data->victimindex = -1;
	data->age240 = 0;

	osSyncPrintf("PopACapReset -> num_mplayers=%d : Working\n", g_MpNumChrs);

	for (i = 0; i != MAX_MPCHRS; i++) {
		data->killcounts[i] = 0;
		data->survivalcounts[i] = 0;
	}

	i = 0;

	while (i < g_MpNumChrs) {
		bool isnew;
		s32 victimplayernum = rngRandom() % g_MpNumChrs;

		for (j = 0, isnew = true; j < i; j++) {
			if (data->victims[j] == victimplayernum) {
				isnew = false;
				break;
			}
		}

		if (isnew) {
			data->victims[i] = victimplayernum;
			i++;
		}
	}

	osSyncPrintf("PopACapReset -> Generated %d victims for this game : Listing\n", i);

	for (j = 0; j < g_MpNumChrs; j++) {
		osSyncPrintf("PopACapReset -> Victim %d is player %d\n", j, data->victims[j]);
	}

	osSyncPrintf("PopACapReset -> Done\n");
}

void pacInit(void)
{
	pacReset();
}

void pacInitProps(void)
{
	pacReset();
}

bool pacHighlightProp(struct prop *prop, s32 *colour)
{
	struct chrdata *victimchr;

	if ((g_MpSetup.options & MPOPTION_PAC_HIGHLIGHTTARGET) == 0
			|| (prop->type != PROPTYPE_PLAYER && prop->type != PROPTYPE_CHR)) {
		return false;
	}

	victimchr = pacGetVictimChr();
	if (victimchr && prop->chr == victimchr) {
		colour[0] = 0;
		colour[1] = 0xff;
		colour[2] = 0;
		colour[3] = 0x40;
		return true;
	}

	return false;
}

/**
 * True if this host-stable mpchr index is allowed to become the new cap.
 * excludeplayernum / norepeat skip the player who just died / prior cap
 * (isdead may still be false when death is recorded).
 */
static bool pacVictimCandidateOk(s32 vplayernum, s32 excludeplayernum, s32 norepeat)
{
	struct chrdata *chr;

	if (vplayernum < 0 || vplayernum >= g_MpNumChrs) {
		return false;
	}

	if (vplayernum == excludeplayernum || vplayernum == norepeat) {
		return false;
	}

	if (vplayernum < PLAYERCOUNT()) {
		struct player *pl = g_Vars.players[vplayernum];

		if (!pl || pl->isdead) {
			return false;
		}
#ifndef PLATFORM_N64
		if (pl->client && pl->client->leftmatch) {
			return false;
		}
#endif
		return true;
	}

	chr = g_MpAllChrPtrs[vplayernum];
	if (!chr || chrIsDead(chr)) {
		return false;
	}

	return true;
}

/**
 * Map a host-stable playernum to its slot in victims[].
 */
static s32 pacVictimSlotForPlayernum(s32 playernum)
{
	struct scenariodata_pac *data = &g_ScenarioData.pac;
	s32 i;

	for (i = 0; i < g_MpNumChrs; i++) {
		if (data->victims[i] == playernum) {
			return i;
		}
	}

	return -1;
}

/**
 * Pick the next living human/sim as cap at random from everyone still alive.
 * Never hands the cap to the same playernum twice in a row when anyone else
 * is eligible (2 players trade off; with bots the whole lobby shares).
 * excludeplayernum skips the player who just died (isdead may still be false).
 */
static void pacChooseNextVictim(s32 excludeplayernum)
{
	struct scenariodata_pac *data = &g_ScenarioData.pac;
	s32 vplayernum = -1;
	char text[64];
	s32 i;
	s32 candidates[MAX_MPCHRS];
	s32 ncandidates = 0;
	s32 chosenplayernum = -1;
	s32 norepeat = -1;
	s32 slot;

#ifndef PLATFORM_N64
	if (g_NetMode == NETMODE_CLIENT) {
		return;
	}
#endif

	if (g_MpNumChrs <= 0) {
		return;
	}

	/* Prior cap — never consecutive when another living peer exists. */
	if (data->victimindex >= 0 && data->victimindex < g_MpNumChrs) {
		norepeat = data->victims[data->victimindex];
	}
	if (excludeplayernum >= 0) {
		norepeat = excludeplayernum;
	}

	for (i = 0; i < g_MpNumChrs; i++) {
		if (pacVictimCandidateOk(i, excludeplayernum, norepeat)) {
			candidates[ncandidates++] = i;
		}
	}

	if (ncandidates == 0) {
		/* Only the prior cap is left alive — allow them again. */
		for (i = 0; i < g_MpNumChrs; i++) {
			if (pacVictimCandidateOk(i, excludeplayernum, -1)) {
				candidates[ncandidates++] = i;
			}
		}
	}

	if (ncandidates == 0) {
		/* Everyone else dead — any roster slot except the excluded player. */
		for (i = 0; i < g_MpNumChrs; i++) {
			if (i != excludeplayernum) {
				candidates[ncandidates++] = i;
			}
		}
	}

	if (ncandidates > 0) {
		chosenplayernum = candidates[rngRandom() % ncandidates];
	}

	if (chosenplayernum < 0) {
		data->victimindex = -1;
		data->age240 = 0;
#ifndef PLATFORM_N64
		if (g_NetMode == NETMODE_SERVER) {
			netServerBroadcastScenarioImmediate();
		}
#endif
		return;
	}

	slot = pacVictimSlotForPlayernum(chosenplayernum);
	if (slot < 0) {
		slot = 0;
		data->victims[0] = (s16)chosenplayernum;
	}

	data->victimindex = slot;
	vplayernum = chosenplayernum;
	data->age240 = 0;

	for (i = 0; i < PLAYERCOUNT(); i++) {
		if (vplayernum == i) {
			sprintf(text, langGet(L_MPWEAPONS_013)); // "You are the victim!"
		} else if (vplayernum >= 0 && vplayernum < g_MpNumChrs && g_MpAllChrConfigPtrs[vplayernum]
				&& scenarioChrsAreSameTeam(vplayernum, i)) {
#if VERSION >= VERSION_JPN_FINAL
			sprintf(text, langGet(L_MPWEAPONS_014), scenarioRemoveLineBreaks(g_MpAllChrConfigPtrs[vplayernum]->name, 0)); // "Protect %s!"
#else
			sprintf(text, langGet(L_MPWEAPONS_014), g_MpAllChrConfigPtrs[vplayernum]->name); // "Protect %s!"
#endif
		} else if (vplayernum >= 0 && vplayernum < g_MpNumChrs && g_MpAllChrConfigPtrs[vplayernum]) {
#if VERSION >= VERSION_JPN_FINAL
			sprintf(text, langGet(L_MPWEAPONS_015), scenarioRemoveLineBreaks(g_MpAllChrConfigPtrs[vplayernum]->name, 0)); // "Get %s!"
#else
			sprintf(text, langGet(L_MPWEAPONS_015), g_MpAllChrConfigPtrs[vplayernum]->name); // "Get %s!"
#endif
		} else {
			continue;
		}

#ifndef PLATFORM_N64
		if (g_NetMode) {
			scenarioNetHudmsg(text, HUDMSGTYPE_MPSCENARIO, 0, i);
		} else
#endif
		{
			scenarioCreateHudmsg(i, text);
		}
	}

#ifndef PLATFORM_N64
	if (g_NetMode == NETMODE_SERVER) {
		netServerBroadcastScenarioImmediate();
	}
#endif
}

void pacApplyNextVictim(void)
{
	pacChooseNextVictim(-1);
}

void pacHandleDeath(s32 aplayernum, s32 vplayernum)
{
	struct scenariodata_pac *data = &g_ScenarioData.pac;

#ifndef PLATFORM_N64
	// Host owns killcounts / victim rotation; clients get SVC_SCENARIO.
	if (g_NetMode == NETMODE_CLIENT) {
		return;
	}
#endif

	if (data->victimindex < 0 || vplayernum != data->victims[data->victimindex]) {
		return;
	}

	/* Award pop points only for a non-suicide kill of the cap. */
	if (aplayernum >= 0 && aplayernum != vplayernum) {
		if (scenarioChrsAreSameTeam(aplayernum, vplayernum)) {
#ifndef PLATFORM_N64
			if (g_NetMode) {
				scenarioNetHudmsg(langGet(L_MPWEAPONS_008), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE, aplayernum);
				scenarioNetHudmsg(langGet(L_MPWEAPONS_009), HUDMSGTYPE_MPSCENARIO, HUDMSGFLAG_ONLYIFALIVE, aplayernum);
			} else
#endif
			{
				scenarioCreateHudmsg(aplayernum, langGet(L_MPWEAPONS_008)); // "You're supposed to look"
				scenarioCreateHudmsg(aplayernum, langGet(L_MPWEAPONS_009)); // "after your friends!"
			}
		} else {
			data->killcounts[aplayernum]++;
#ifndef PLATFORM_N64
			if (g_NetMode) {
				/* No ONLYIFALIVE — killer is alive; flag interacted badly with net HUD. */
				scenarioNetHudmsg(langGet(L_MPWEAPONS_010), HUDMSGTYPE_MPSCENARIO, 0, aplayernum);
				scenarioNetHudmsg(langGet(L_MPWEAPONS_011), HUDMSGTYPE_MPSCENARIO, 0, aplayernum);
				scenarioNetHudmsg(langGet(L_MPWEAPONS_012), HUDMSGTYPE_MPSCENARIO, 0, aplayernum);
			} else
#endif
			{
				scenarioCreateHudmsg(aplayernum, langGet(L_MPWEAPONS_010)); // "Well done!"
				scenarioCreateHudmsg(aplayernum, langGet(L_MPWEAPONS_011)); // "You popped a cap!"
				scenarioCreateHudmsg(aplayernum, langGet(L_MPWEAPONS_012)); // "Have 2 Points..."
			}
		}
	}

	/* Always rotate when the cap dies — including suicide. Never re-pick them. */
	pacChooseNextVictim(vplayernum);
}

void pacTick(void)
{
	struct scenariodata_pac *data = &g_ScenarioData.pac;

	if (data->victimindex == -1) {
		pacApplyNextVictim();
		osSyncPrintf("PopACapTick : Current Victim = %d (Player %d)\n",
				data->victimindex, data->victims[data->victimindex]);
	}

	if (data->victimindex >= 0) {
#if VERSION >= VERSION_NTSC_1_0
		if (data->victims[data->victimindex] >= PLAYERCOUNT() ||
				(g_Vars.players[data->victims[data->victimindex]]
					&& g_Vars.players[data->victims[data->victimindex]]->isdead == false))
#endif
		{
			data->age240 += g_Vars.lvupdate240;

			if (data->age240 > (u32)scenarioPointDuration240()) {
				data->age240 = 0;
				data->survivalcounts[data->victims[data->victimindex]]++;
#ifndef PLATFORM_N64
				if (g_NetMode) {
					scenarioNetHudmsg(langGet(L_MPWEAPONS_007), HUDMSGTYPE_MPSCENARIO, 0,
							data->victims[data->victimindex]);
				} else
#endif
				{
					scenarioCreateHudmsg(data->victims[data->victimindex], langGet(L_MPWEAPONS_007)); // "Have a point for living!"
				}
			}
		}
	}
}

Gfx *pacRenderHud(Gfx *gdl)
{
	struct scenariodata_pac *data = &g_ScenarioData.pac;
	s32 time240;
	s32 mins;
	s32 secs;
	s32 textwidth;
	s32 textheight;
	s32 x;
	s32 y;
	char text[64];
	const s32 pointdur = scenarioPointDuration240();

#if VERSION >= VERSION_NTSC_1_0
	if (pacAmIVictim() && !g_Vars.currentplayer->isdead)
#else
	if (pacAmIVictim())
#endif
	{
		time240 = pointdur - data->age240;
		x = viGetViewLeft() + viGetViewWidth() / 2;
		y = viGetViewTop() + 10;

		if (time240 < 0) {
			time240 = 0;
		}

		mins = time240 / TICKS(60 * 240);
		time240 -= TICKS(60 * 240) * mins;
		secs = (time240 + (TICKS(240) - 1)) / TICKS(240);
		sprintf(text, "%d:%02d", mins, secs);

#ifndef PLATFORM_N64
		const s32 playercount = PLAYERCOUNT();
		if (playercount < 2 || (playercount == 2 && optionsGetScreenSplit() == SCREENSPLIT_HORIZONTAL)) {
			gSPSetExtraGeometryModeEXT(gdl++, G_ASPECT_CENTER_EXT);
		}
#endif

		gdl = text0f153628(gdl);
		textMeasure(&textheight, &textwidth, text, g_CharsHandelGothicXs, g_FontHandelGothicXs, 0);

		x -= textwidth / 2;
		textwidth += x;
		textheight += y;

#if VERSION >= VERSION_NTSC_1_0
		gdl = text0f153990(gdl, x, y, textwidth, textheight);
		gdl = textRender(gdl, &x, &y, text, g_CharsNumeric, g_FontNumeric, 0x00ff00a0, 0xa0, viGetWidth(), viGetHeight(), 0, 0);
#else
		gdl = text0f153858(gdl, &x, &y, &textwidth, &textheight);
		gdl = textRender(gdl, &x, &y, text, g_CharsNumeric, g_FontNumeric, 0x00ff00a0, 0x88, viGetWidth(), viGetHeight(), 0, 0);
#endif
		gdl = text0f153780(gdl);

#ifndef PLATFORM_N64
		gSPClearExtraGeometryModeEXT(gdl++, G_ASPECT_MODE_EXT);
#endif
	}

	return gdl;
}

void pacCalculatePlayerScore(struct mpchrconfig *mpchr, s32 mpchrnum, s32 *score, s32 *arg3)
{
	struct mpchrconfig *loopmpchr;
	s32 i;
	s32 index;

	*score = 0;
	index = func0f18d0e8(mpchrnum);

	if (index >= 0) {
		*score += g_ScenarioData.pac.killcounts[index] * 2;
		*score += g_ScenarioData.pac.survivalcounts[index];
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

	*arg3 = mpchr->numdeaths;
}

Gfx *pacRadarExtra(Gfx *gdl)
{
	return gdl;
}

bool pacRadarChr(Gfx **gdl, struct prop *prop)
{
	struct coord dist;
	struct chrdata *victimchr;
	struct prop *vprop;

	if ((g_MpSetup.options & MPOPTION_PAC_SHOWONRADAR) == 0) {
		return false;
	}

	victimchr = pacGetVictimChr();
	if (!victimchr || !victimchr->prop) {
		return false;
	}

	vprop = victimchr->prop;
	if (vprop == prop) {
		dist.x = prop->pos.x - g_Vars.currentplayer->prop->pos.x;
		dist.y = prop->pos.y - g_Vars.currentplayer->prop->pos.y;
		dist.z = prop->pos.z - g_Vars.currentplayer->prop->pos.z;

		if (g_MpSetup.options & MPOPTION_TEAMSENABLED) {
			u32 colour = g_TeamColours[radarGetTeamIndex(prop->chr->team)];
			*gdl = radarDrawDot(*gdl, vprop, &dist, colour, 0, 1);
		} else {
			*gdl = radarDrawDot(*gdl, vprop, &dist, 0x00ff0000, 0, 1);
		}

		return true;
	}

	return false;
}
