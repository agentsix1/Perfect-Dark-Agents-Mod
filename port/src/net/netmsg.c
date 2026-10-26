#include <string.h>
#include <stdio.h>
#include "types.h"
#include "data.h"
#include "bss.h"
#include "lib/main.h"
#include "lib/mtx.h"
#include "lib/model.h"
#include "lib/memp.h"
#include "game/mplayer/mplayer.h"
#include "game/mplayer/scenarios.h"
#include "game/challenge.h"
#include "game/bot.h"
#include "game/botinv.h"
#include "game/dlights.h"
#include "game/chr.h"
#include "game/chraction.h"
#include "game/prop.h"
#include "game/propobj.h"
#include "game/player.h"
#include "game/playermgr.h"
#include "game/bondgun.h"
#include "game/game_0b0fd0.h"
#include "game/inv.h"
#include "game/menu.h"
#include "game/setup.h"
#include "game/setuputils.h"
#include "game/modelmgr.h"
#include "game/propsnd.h"
#include "game/wallhit.h"
#include "game/explosions.h"
#include "game/bondgun.h"
#include "game/gunfx.h"
#include "game/gfxmemory.h"
#include "lib/mtx.h"
#include "lib/snd.h"
#include "game/hudmsg.h"
#include "system.h"
#include "romdata.h"
#include "fs.h"
#include "console.h"
#include "net/net.h"
#include "net/netbuf.h"
#include "net/netmsg.h"
#include "net/netdbg.h"

extern struct menudialogdef g_NetWaitingPlayersDialog;

static struct prop *netmsgFindPropBySyncId(u32 syncid);

/** Reject out-of-range client ids (NET_NULL_CLIENT is 0xFF). */
static s32 netmsgClientIdOk(u8 clid)
{
	return clid < NET_MAX_CLIENTS;
}

/**
 * Drain SVC_SCENARIO body after the scenario byte was already read.
 * Early-outs must call this or the multiplexed packet desyncs.
 */
static void netmsgSkipScenarioBody(struct netbuf *src, u8 scenario)
{
	s32 i;
	u8 numterminals;

	switch (scenario) {
	case MPSCENARIO_HOLDTHEBRIEFCASE:
		netbufReadU32(src);
		netbufReadS16(src); /* token ref */
		{
			struct coord tmp;
			netbufReadCoord(src, &tmp);
		}
		netbufReadS32(src);
		for (i = 0; i < MAX_MPCHRS; i++) {
			netbufReadS32(src);
		}
		break;
	case MPSCENARIO_HACKERCENTRAL:
		netbufReadU32(src);
		netbufReadS16(src); /* uplink token */
		netbufReadS16(src);
		netbufReadS16(src);
		netbufReadS32(src);
		numterminals = netbufReadU8(src);
		for (i = 0; i < numterminals && i < 7; i++) {
			netbufReadU32(src); /* prop syncid */
		}
		for (i = 0; i < MAX_MPCHRS; i++) {
			netbufReadS32(src);
			netbufReadS32(src);
		}
		break;
	case MPSCENARIO_CAPTURETHECASE:
		for (i = 0; i < 4; i++) {
			netbufReadU32(src);
			netbufReadS16(src);
			netbufReadS16(src);
			netbufReadS16(src);
		}
		for (i = 0; i < MAX_MPCHRS; i++) {
			netbufReadS32(src);
		}
		break;
	case MPSCENARIO_POPACAP:
		netbufReadS32(src);
		netbufReadU16(src);
		for (i = 0; i < MAX_MPCHRS; i++) {
			netbufReadS16(src);
			netbufReadS16(src);
			netbufReadS16(src);
		}
		break;
	case MPSCENARIO_KINGOFTHEHILL:
		netbufReadS16(src);
		netbufReadS16(src);
		netbufReadS16(src);
		netbufReadS16(src);
		netbufReadS16(src);
		netbufReadS16(src);
		netbufReadS16(src);
		{
			struct coord tmp;
			netbufReadCoord(src, &tmp);
		}
		netbufReadF32(src);
		netbufReadF32(src);
		netbufReadF32(src);
		for (i = 0; i < MAX_MPCHRS; i++) {
			netbufReadS32(src);
		}
		break;
	default:
		break;
	}
}

/** Apply host-ordered numpoints into local g_MpAllChrConfigPtrs (PAC-style remap). */
static void netmsgApplyRemappedNumpoints(struct netbuf *src)
{
	s32 hostpoints[MAX_MPCHRS];
	s32 i;

	for (i = 0; i < MAX_MPCHRS; i++) {
		hostpoints[i] = netbufReadS32(src);
	}
	if (src->error) {
		return;
	}
	for (i = 0; i < MAX_MPCHRS; i++) {
		s32 hosti = i;

		if (i < PLAYERCOUNT()) {
			hosti = netMpChrIndexForPlayernum(i);
			if (hosti < 0 || hosti >= MAX_MPCHRS) {
				hosti = i;
			}
		}
		if (g_MpAllChrConfigPtrs[i]) {
			g_MpAllChrConfigPtrs[i]->numpoints = hostpoints[hosti];
		}
	}
}

static void netmsgSkipStageStartPayload(struct netbuf *src)
{
	u8 stagenum;
	u8 n;
	s32 i;
	u8 weapons[NUM_MPWEAPONSLOTS];

	netbufReadU32(src);
	netbufReadU64(src);
	netbufReadU64(src);
	stagenum = netbufReadU8(src);
	if (stagenum == STAGE_TITLE || stagenum == STAGE_CITRAINING) {
		return;
	}
	netbufReadU8(src); /* mode */
	netbufReadU8(src); /* scenario */
	netbufReadU8(src);
	netbufReadU8(src);
	netbufReadU16(src);
	netbufReadU16(src);
	netbufReadU32(src);
	netbufReadData(src, weapons, sizeof(weapons));
	netbufReadU8(src); /* mphilltime */
	netMpSimulantsSkip(src);
	n = netbufReadU8(src);
	for (i = 0; i < n; i++) {
		netbufReadU8(src); /* id */
		netbufReadU8(src); /* playernum */
		netbufReadU8(src); /* team */
		netbufReadU16(src); /* options */
		netbufReadU8(src);
		netbufReadU8(src);
		netbufReadF32(src);
		netbufReadF32(src);
		netbufReadStr(src);
	}
}

static void netmsgSkipPropSpawnBody(struct netbuf *src, u8 msgflags, u8 type, u8 objtype)
{
	if (type == PROPTYPE_WEAPON) {
		netbufReadS16(src);
		netbufReadU8(src);
		netbufReadS8(src);
		netbufReadS8(src);
		netbufReadS8(src);
		netbufReadU8(src);
		netbufReadS16(src);
	} else if (type == PROPTYPE_OBJ) {
		netbufReadS16(src);
		if (objtype == OBJTYPE_AUTOGUN) {
			netbufReadU8(src);
			netbufReadU8(src);
			netbufReadU8(src);
			netbufReadU8(src);
		}
	}
	if (msgflags & (1 << 1)) {
		u32 hidden;
		u32 projflags;

		netbufReadU32(src);
		netbufReadU32(src);
		netbufReadU32(src);
		hidden = netbufReadU32(src); /* skip; no owner remap needed */
		netbufReadU8(src);
		netbufReadU16(src);
		netbufReadS16(src);
		for (s32 i = 0; i < 9; i++) {
			netbufReadF32(src);
		}
		if (hidden & OBJHFLAG_PROJECTILE) {
			struct coord tmp;
			Mtxf mtx;
			netbufReadCoord(src, &tmp);
			netbufReadU32(src);
			netbufReadU32(src);
			projflags = netbufReadU32(src);
			netbufReadF32(src);
			netbufReadS16(src);
			netbufReadS16(src);
			netbufReadMtxf(src, &mtx);
			if (projflags & PROJECTILEFLAG_POWERED) {
				netbufReadF32(src);
				netbufReadF32(src);
				netbufReadF32(src);
			}
		}
	}
}

/* utils */

static inline u32 netbufReadHidden(struct netbuf *buf)
{
	u32 hidden = netbufReadU32(buf);

	// swap owner player numbers to match server
	const u8 ownerclid = (hidden & 0xf0000000) >> 28;
	if (ownerclid < NET_MAX_CLIENTS) {
		const u8 ownerplayernum = g_NetClients[ownerclid].playernum;
		hidden = (hidden & 0x0fffffff) | (ownerplayernum << 28);
	}

	return hidden;
}

static inline u32 netbufWriteRooms(struct netbuf *buf, const s16 *rooms, const s32 num)
{
	// Always emit an in-bounds -1 terminator. A full 8-room list without
	// one freezes clients in roomsCopy / propsTickPlayer / collision walks.
	if (num <= 0) {
		return buf->error;
	}

	for (s32 i = 0; i < num; ++i) {
		if (i == num - 1) {
			netbufWriteS16(buf, -1);
			break;
		}
		netbufWriteS16(buf, rooms[i]);
		if (rooms[i] < 0) {
			break;
		}
	}
	return buf->error;
}

static inline u32 netbufReadRooms(struct netbuf *buf, s16 *rooms, const s32 num)
{
	s32 i;

	if (num <= 0) {
		return buf->error;
	}

	for (i = 0; i < num; ++i) {
		rooms[i] = netbufReadS16(buf);
		if (rooms[i] < 0) {
			break;
		}
	}

	// Truncate / terminate even if the peer sent a full non-terminated list.
	if (i >= num) {
		rooms[num - 1] = -1;
	}

	return buf->error;
}

static inline u32 netbufWriteGset(struct netbuf *buf, const struct gset *gset)
{
	netbufWriteData(buf, gset, sizeof(*gset));
	return buf->error;
}

static inline u32 netbufReadGset(struct netbuf *buf, struct gset *gset)
{
	netbufReadData(buf, gset, sizeof(*gset));
	return buf->error;
}

static inline u32 netbufWritePlayerMove(struct netbuf *buf, const struct netplayermove *in)
{
	netbufWriteU32(buf, in->tick);
	netbufWriteU32(buf, in->ucmd);
	netbufWriteF32(buf, in->leanofs);
	netbufWriteF32(buf, in->crouchofs);
	netbufWriteF32(buf, in->movespeed[0]);
	netbufWriteF32(buf, in->movespeed[1]);
	netbufWriteF32(buf, in->angles[0]);
	netbufWriteF32(buf, in->angles[1]);
	netbufWriteF32(buf, in->crosspos[0]);
	netbufWriteF32(buf, in->crosspos[1]);
	netbufWriteS8(buf, in->weaponnum);
	netbufWriteCoord(buf, &in->pos);
	if (in->ucmd & UCMD_AIMMODE) {
		netbufWriteF32(buf, in->zoomfov);
	}
	return buf->error;
}

static inline u32 netbufReadPlayerMove(struct netbuf *buf, struct netplayermove *in)
{
	in->tick = netbufReadU32(buf);
	in->ucmd = netbufReadU32(buf);
	in->leanofs = netbufReadF32(buf);
	in->crouchofs = netbufReadF32(buf);
	in->movespeed[0] = netbufReadF32(buf);
	in->movespeed[1] = netbufReadF32(buf);
	in->angles[0] = netbufReadF32(buf);
	in->angles[1] = netbufReadF32(buf);
	in->crosspos[0] = netbufReadF32(buf);
	in->crosspos[1] = netbufReadF32(buf);
	in->weaponnum = netbufReadS8(buf);
	netbufReadCoord(buf, &in->pos);
	if (in->ucmd & UCMD_AIMMODE) {
		in->zoomfov = netbufReadF32(buf);
	} else {
		in->zoomfov = 0.f;
	}
	return buf->error;
}

static inline u32 netbufWritePropPtr(struct netbuf *buf, const struct prop *prop)
{
	netbufWriteU32(buf, prop ? prop->syncid : 0);
	return buf->error;
}

static inline struct prop *netbufReadPropPtr(struct netbuf *buf)
{
	const u32 syncid = netbufReadU32(buf);
	if (syncid == 0) {
		return NULL;
	}

	// Missing syncids are common for ephemeral props; avoid log spam.
	return netPropFindBySyncId(syncid);
}

static inline s32 propRoomsEqual(const RoomNum *ra, const RoomNum *rb)
{
	for (s32 i = 0; i < 8; ++i) {
		if (ra[i] != rb[i]) {
			return 0;
		}
		if (ra[i] == -1) {
			break;
		}
	}
	return 1;
}

/* client -> server */

u32 netmsgClcAuthWrite(struct netbuf *dst)
{
	const char *modDir = fsGetModDir();
	if (!modDir) {
		modDir = "";
	}

	netbufWriteU8(dst, CLC_AUTH);
	netdbgTx(0, CLC_AUTH);
	netbufWriteStr(dst, g_NetLocalClient->settings.name);
	netbufWriteStr(dst, g_RomName); // TODO: use a CRC or something
	netbufWriteStr(dst, modDir);
	netbufWriteU8(dst, 1); // TODO: number of local players

	return dst->error;
}

u32 netmsgClcAuthRead(struct netbuf *src, struct netclient *srccl)
{
	if (srccl->state != CLSTATE_AUTH) {
		sysLogPrintf(LOG_WARNING, "NET: CLC_AUTH from client %u, who is not in CLSTATE_AUTH", srccl->id);
		return 1;
	}

	char *name = netbufReadStr(src);
	const char *romName = netbufReadStr(src);
	const char *modDir = netbufReadStr(src);
	const u8 players = netbufReadU8(src);

	if (src->error) {
		sysLogPrintf(LOG_WARNING, "NET: malformed CLC_AUTH from client %u", srccl->id);
		netServerKick(srccl, DISCONNECT_KICKED);
		return 1;
	}

	if (strcasecmp(romName, g_RomName) != 0) {
		sysLogPrintf(LOG_WARNING, "NET: CLC_AUTH: client %u has the wrong ROM, disconnecting", srccl->id);
		netServerKick(srccl, DISCONNECT_FILES);
		return src->error;
	}

	if (modDir[0] == '\0') {
		modDir = NULL;
	}

	const char *myModDir = fsGetModDir();
	if ((!myModDir != !modDir) || (myModDir && modDir && strcasecmp(modDir, myModDir) != 0)) {
		sysLogPrintf(LOG_WARNING, "NET: CLC_AUTH: client %u has the wrong mod, disconnecting", srccl->id);
		netServerKick(srccl, DISCONNECT_FILES);
		return src->error;
	}

	// for now use settings from our own client, remote is supposed to send CLC_SETTINGS after CLC_AUTH
	srccl->settings = g_NetLocalClient->settings;
	strncpy(srccl->settings.name, name, sizeof(srccl->settings.name) - 1);
	srccl->state = CLSTATE_LOBBY;

	sysLogPrintf(LOG_NOTE, "NET: CLC_AUTH from client %u (%s), responding", srccl->id, srccl->settings.name);

	netbufStartWrite(&srccl->out);
	netmsgSvcAuthWrite(&srccl->out, srccl);
	netSend(srccl, NULL, true, NETCHAN_CONTROL);

	sysLogPrintf(LOG_NOTE, "NET: %s (%u) joined", srccl->settings.name, srccl->id);

	netChatPrintf(NULL, "%s joined", name);

	// Refresh lobby roster/sims for everyone (joiner + existing peers).
	netLobbyRebuildHumanSlots();
	netMpSetupSend();

	return 0;
}

u32 netmsgClcChatWrite(struct netbuf *dst, const char *str)
{
	netbufWriteU8(dst, CLC_CHAT);
	netdbgTx(0, CLC_CHAT);
	netbufWriteStr(dst, str);
	return dst->error;
}

u32 netmsgClcChatRead(struct netbuf *src, struct netclient *srccl)
{
	char tmp[1024];
	const char *msg = netbufReadStr(src);
	if (msg && !src->error) {
		sysLogPrintf(LOG_CHAT, "%s", msg);
		netbufStartWrite(&g_NetMsgRel);
		netmsgSvcChatWrite(&g_NetMsgRel, msg);
		netSend(NULL, &g_NetMsgRel, true, NETCHAN_DEFAULT);
	}
	return src->error;
}

u32 netmsgClcMoveWrite(struct netbuf *dst)
{
	const struct netplayermove *m = &g_NetLocalClient->outmove[0];

	netbufWriteU8(dst, CLC_MOVE);
	netbufWriteU32(dst, g_NetLocalClient->inmove[0].tick);
	netbufWritePlayerMove(dst, m);
	netdbgEvent("tx",
			"\"msgid\":%u,\"name\":\"CLC_MOVE\",\"mtick\":%u,\"ucmd\":%u,\"weaponnum\":%d"
			",\"pos\":[%.2f,%.2f,%.2f]",
			(unsigned)CLC_MOVE, (unsigned)m->tick, (unsigned)m->ucmd, (int)m->weaponnum,
			m->pos.x, m->pos.y, m->pos.z);
	return dst->error;
}

u32 netmsgClcMoveRead(struct netbuf *src, struct netclient *srccl)
{
	struct netplayermove newmove;
	const u32 outmoveack = netbufReadU32(src);
	netbufReadPlayerMove(src, &newmove);

	netdbgEvent("fields",
			"\"msgid\":%u,\"name\":\"CLC_MOVE\",\"clid\":%u,\"ack\":%u,\"mtick\":%u,\"ucmd\":%u"
			",\"weaponnum\":%d,\"pos\":[%.2f,%.2f,%.2f]",
			(unsigned)CLC_MOVE, (unsigned)srccl->id, (unsigned)outmoveack,
			(unsigned)newmove.tick, (unsigned)newmove.ucmd, (int)newmove.weaponnum,
			newmove.pos.x, newmove.pos.y, newmove.pos.z);

	if (srccl->state != CLSTATE_GAME || srccl->leftmatch) {
		// silently ignore (left mid-match until they rejoin)
		return src->error;
	}

	srccl->outmoveack = outmoveack;

	if (!src->error) {
		// enforce teleports and such
		if (srccl->forcetick && srccl->player) {
			if (srccl->outmoveack >= srccl->forcetick) {
				// client has acknowledged our last sent move, clear the force flags
				srccl->forcetick = 0;
				srccl->player->ucmd &= ~UCMD_FL_FORCEMASK;
				sysLogPrintf(LOG_NOTE, "NET: client %u successfully forcemoved", srccl->id);
			} else {
				// client hasn't teleported yet, discard the new position from the input command
				if (srccl->player->ucmd & UCMD_FL_FORCEPOS) {
					newmove.pos = srccl->player->prop->pos;
				}
				if (srccl->player->ucmd & UCMD_FL_FORCEANGLE) {
					newmove.angles[0] = srccl->player->vv_theta;
					newmove.angles[1] = srccl->player->vv_verta;
				}
			}
		}
		// make space in the move stack
		memmove(srccl->inmove + 1, srccl->inmove, sizeof(srccl->inmove) - sizeof(*srccl->inmove));
		srccl->inmove[0] = newmove;
		srccl->lerpticks = 0;
	}

	return src->error;
}

u32 netmsgClcSettingsWrite(struct netbuf *dst)
{
	netbufWriteU8(dst, CLC_SETTINGS);
	netdbgTx(0, CLC_SETTINGS);
	netbufWriteU16(dst, g_NetLocalClient->settings.options);
	netbufWriteU8(dst, g_NetLocalClient->settings.bodynum);
	netbufWriteU8(dst, g_NetLocalClient->settings.headnum);
	netbufWriteF32(dst, g_NetLocalClient->settings.fovy);
	netbufWriteF32(dst, g_NetLocalClient->settings.fovzoommult);
	netbufWriteStr(dst, g_NetLocalClient->settings.name);
	return dst->error;
}

u32 netmsgClcSettingsRead(struct netbuf *src, struct netclient *srccl)
{
	const u16 options = netbufReadU16(src);
	const u8 bodynum = netbufReadU8(src);
	const u8 headnum = netbufReadU8(src);
	const f32 fovy = netbufReadF32(src);
	const f32 fovzoommult = netbufReadF32(src);
	char *name = netbufReadStr(src);

	if (src->error) {
		sysLogPrintf(LOG_WARNING, "NET: malformed CLC_SETTINGS from client %u", srccl->id);
		netServerKick(srccl, DISCONNECT_KICKED);
		return 1;
	}

	if (srccl->settings.name[0] && strncmp(srccl->settings.name, name, MAX_PLAYERNAME) != 0) {
		netChatPrintf(NULL, "%s is now known as %s", srccl->settings.name, name);
	}

	strncpy(srccl->settings.name, name, sizeof(srccl->settings.name) - 1);
	srccl->settings.options = options;
	srccl->settings.bodynum = bodynum;
	srccl->settings.headnum = headnum;
	srccl->settings.fovy = fovy;
	srccl->settings.fovzoommult = fovzoommult;

	// Keep live player configs in sync so mid-match paintball toggles apply
	// when the server evaluates wallhits for this shooter.
	if (srccl->state >= CLSTATE_GAME && srccl->playernum < MAX_PLAYERS) {
		struct mpplayerconfig *cfg = &g_PlayerConfigsArray[srccl->playernum];
		cfg->options = options;
		cfg->options &= ~(OPTION_AIMCONTROL | OPTION_LOOKAHEAD);
		cfg->options |= OPTION_FORWARDPITCH | OPTION_ASKEDSAVEPLAYER;
		cfg->base.mpbodynum = bodynum;
		cfg->base.mpheadnum = headnum;
	}

	return src->error;
}

extern s32 g_MpWeaponSetNum;
extern u32 g_NetMpSetupSeq;

static void netmsgWriteMpSetupPayload(struct netbuf *dst, u32 seq)
{
	netbufWriteU32(dst, seq);
	netbufWriteU8(dst, g_MpSetup.stagenum);
	netbufWriteU8(dst, g_MpSetup.scenario);
	netbufWriteU8(dst, g_MpSetup.scorelimit);
	netbufWriteU8(dst, g_MpSetup.timelimit);
	netbufWriteU16(dst, g_MpSetup.teamscorelimit);
	netbufWriteU16(dst, g_MpSetup.chrslots);
	netbufWriteU32(dst, g_MpSetup.options);
	netbufWriteData(dst, g_MpSetup.weapons, sizeof(g_MpSetup.weapons));
	netbufWriteU8(dst, (u8)g_MpWeaponSetNum);
	netbufWriteU8(dst, (u8)g_BossFile.locktype);
	netbufWriteU8(dst, (u8)g_MpChallengeIndex);
	netbufWriteU8(dst, (u8)g_Vars.mphilltime);
	netbufWriteStr(dst, g_MpSetup.name);
	netMpSimulantsWrite(dst);
	netMpPlayersWrite(dst);
}

static void netmsgSkipMpSetupPayload(struct netbuf *src)
{
	u8 weapons[NUM_MPWEAPONSLOTS];
	netbufReadU32(src); // seq
	netbufReadU8(src);
	netbufReadU8(src);
	netbufReadU8(src);
	netbufReadU8(src);
	netbufReadU16(src);
	netbufReadU16(src);
	netbufReadU32(src);
	netbufReadData(src, weapons, sizeof(weapons));
	netbufReadU8(src); // weaponset
	netbufReadU8(src); // locktype
	netbufReadU8(src); // challengeindex
	netbufReadU8(src); // mphilltime (HTB/HTM/PAC/KOH Time)
	netbufReadStr(src);
	netMpSimulantsSkip(src);
	netMpPlayersSkip(src);
}

u32 netmsgClcMpSetupWrite(struct netbuf *dst)
{
	netbufWriteU8(dst, CLC_MPSETUP);
	netdbgTx(0, CLC_MPSETUP);
	// seq 0 = client proposal; server assigns the real seq on rebroadcast
	netmsgWriteMpSetupPayload(dst, 0);
	return dst->error;
}

u32 netmsgClcMpSetupRead(struct netbuf *src, struct netclient *srccl)
{
	if (!g_NetAllowSetupChanges || srccl->state != CLSTATE_LOBBY) {
		sysLogPrintf(LOG_WARNING, "NET: ignoring CLC_MPSETUP from client %u", srccl->id);
		netmsgSkipMpSetupPayload(src);
		return src->error;
	}

	netMpSetupApply(src, srccl);

	// Rebroadcast authoritative copy to everyone (including the sender)
	netMpSetupSend();
	return src->error;
}

/* server -> client */

u32 netmsgSvcAuthWrite(struct netbuf *dst, struct netclient *authcl)
{
	netbufWriteU8(dst, SVC_AUTH);
	netdbgTx(1, SVC_AUTH);
	netbufWriteU8(dst, authcl - g_NetClients);
	netbufWriteU8(dst, g_NetMaxClients);
	netbufWriteU32(dst, g_NetTick);
	netbufWriteU8(dst, g_NetAllowSetupChanges ? 1 : 0);
	return dst->error;
}

u32 netmsgSvcAuthRead(struct netbuf *src, struct netclient *srccl)
{
	if (g_NetLocalClient->state != CLSTATE_AUTH) {
		sysLogPrintf(LOG_WARNING, "NET: SVC_AUTH from server but we're not in AUTH state");
		return 1;
	}

	const u8 id = netbufReadU8(src);
	const u8 maxclients = netbufReadU8(src);
	g_NetTick = netbufReadU32(src);
	const u8 allowsetup = netbufReadU8(src);
	if (g_NetLocalClient->in.error || id == NET_NULL_CLIENT || id == 0 || maxclients == 0) {
		sysLogPrintf(LOG_WARNING, "NET: malformed SVC_AUTH from server");
		return 1;
	}

	sysLogPrintf(LOG_NOTE, "NET: SVC_AUTH from server, our ID is %u", id);

	// there's at least one client, which is us, and we know maxclients as well
	g_NetMaxClients = maxclients;
	g_NetNumClients = 1;
	g_NetAllowSetupChanges = allowsetup ? 1 : 0;

	// we now know our proper ID, so move to the appropriate client slot and reset the old one
	g_NetLocalClient = &g_NetClients[id];
	g_NetClients[id] = g_NetClients[NET_MAX_CLIENTS];
	g_NetLocalClient->out.data = g_NetLocalClient->out_data;
	g_NetLocalClient->id = id;

	// clear out the old slot
	g_NetClients[NET_MAX_CLIENTS].id = NET_MAX_CLIENTS;
	g_NetClients[NET_MAX_CLIENTS].state = 0;
	g_NetClients[NET_MAX_CLIENTS].peer = NULL;

	// the server's client probably is in the lobby state by now
	g_NetClients[0].state = CLSTATE_LOBBY;

	g_NetLocalClient->state = CLSTATE_LOBBY;

	return src->error;
}

u32 netmsgSvcMpSetupWrite(struct netbuf *dst)
{
	netbufWriteU8(dst, SVC_MPSETUP);
	netdbgTx(1, SVC_MPSETUP);
	netbufWriteU8(dst, g_NetAllowSetupChanges ? 1 : 0);
	// Caller (netMpSetupSend) already bumped seq; write that value.
	netmsgWriteMpSetupPayload(dst, g_NetMpSetupSeq);
	return dst->error;
}

u32 netmsgSvcMpSetupRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 allowsetup = netbufReadU8(src);
	if (src->error) {
		return src->error;
	}
	g_NetAllowSetupChanges = allowsetup ? 1 : 0;
	netMpSetupApply(src, srccl);
	return src->error;
}

u32 netmsgClcEndscreenReadyWrite(struct netbuf *dst, u8 ready)
{
	netbufWriteU8(dst, CLC_ENDSCREEN_READY);
	netdbgTx(0, CLC_ENDSCREEN_READY);
	netbufWriteU8(dst, ready ? 1 : 0);
	return dst->error;
}

u32 netmsgClcEndscreenReadyRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 ready = netbufReadU8(src);
	if (src->error) {
		return src->error;
	}

	if (g_NetMode != NETMODE_SERVER || srccl->state < CLSTATE_LOBBY) {
		sysLogPrintf(LOG_WARNING, "NET: ignoring CLC_ENDSCREEN_READY from client %u", srccl->id);
		return src->error;
	}

	srccl->endscreenready = ready ? 1 : 0;
	if (ready) {
		netServerCheckEndscreenReady();
	}
	return src->error;
}

u32 netmsgSvcMatchProceedWrite(struct netbuf *dst)
{
	netbufWriteU8(dst, SVC_MATCH_PROCEED);
	netdbgTx(1, SVC_MATCH_PROCEED);
	return dst->error;
}

u32 netmsgSvcMatchProceedRead(struct netbuf *src, struct netclient *srccl)
{
	(void)src;
	(void)srccl;
	netEndscreenProceedToLobby();
	return 0;
}

u32 netmsgClcLeftMatchWrite(struct netbuf *dst)
{
	netbufWriteU8(dst, CLC_LEFT_MATCH);
	netdbgTx(0, CLC_LEFT_MATCH);
	return dst->error;
}

void netApplyLeftMatch(struct netclient *cl)
{
	if (!cl || cl->state < CLSTATE_GAME || !cl->player) {
		return;
	}

	cl->leftmatch = 1;
	cl->player->aborted = true;

	// Kill as a suicide (shooter == victim) so it counts on the scoreboard.
	const s32 prev = g_Vars.currentplayernum;
	setCurrentPlayerNum(cl->playernum);

	// Always release HTB/HTM/CTC carryables first. playerDieByShooter no-ops when
	// already dead, which left scenario tokens stuck on leavers / rejoins.
	scenarioNetReleaseCarriedItems();

	playerDieByShooter(cl->playernum, true);
	if (g_Vars.currentplayer->client) {
		netmsgSvcPlayerStatsWrite(&g_NetMsgRel, g_Vars.currentplayer->client);
	}
	setCurrentPlayerNum(prev);

	netServerCheckAllHumansLeftMatch();
}

u32 netmsgClcLeftMatchRead(struct netbuf *src, struct netclient *srccl)
{
	(void)src;

	if (g_NetMode != NETMODE_SERVER || !srccl) {
		sysLogPrintf(LOG_WARNING, "NET: ignoring CLC_LEFT_MATCH from client %u", srccl ? srccl->id : 0);
		return 0;
	}

	netApplyLeftMatch(srccl);
	sysLogPrintf(LOG_NOTE, "NET: client %u left match (suicide, awaiting rejoin)", srccl->id);
	return 0;
}

u32 netmsgClcRejoinMatchWrite(struct netbuf *dst)
{
	netbufWriteU8(dst, CLC_REJOIN_MATCH);
	netdbgTx(0, CLC_REJOIN_MATCH);
	return dst->error;
}

u32 netmsgClcRejoinMatchRead(struct netbuf *src, struct netclient *srccl)
{
	(void)src;

	if (g_NetMode != NETMODE_SERVER || srccl->state < CLSTATE_GAME) {
		sysLogPrintf(LOG_WARNING, "NET: ignoring CLC_REJOIN_MATCH from client %u", srccl ? srccl->id : 0);
		return 0;
	}

	srccl->leftmatch = 0;
	if (srccl->player) {
		srccl->player->aborted = false;

		// Leave can fail to retarget the token off this player; force another
		// release before resync so rejoiner is not SVC_SCENARIO'd as holder.
		const s32 prev = g_Vars.currentplayernum;
		setCurrentPlayerNum(srccl->playernum);
		scenarioNetReleaseCarriedItems();
		setCurrentPlayerNum(prev);
	}

	// Push ground tokens + scenario state so the rejoiner is not stuck
	// looking like a holder / missing the case (ammo crate at the pad).
	scenarioNetResyncGroundTokens();

	sysLogPrintf(LOG_NOTE, "NET: client %u rejoining match", srccl->id);
	return 0;
}

u32 netmsgSvcChatWrite(struct netbuf *dst, const char *str)
{
	netbufWriteU8(dst, SVC_CHAT);
	netdbgTx(1, SVC_CHAT);
	netbufWriteStr(dst, str);
	return dst->error;
}

u32 netmsgSvcChatRead(struct netbuf *src, struct netclient *srccl)
{
	char tmp[1024];
	const char *msg = netbufReadStr(src);
	if (msg && !src->error) {
		sysLogPrintf(LOG_CHAT, "%s", msg);
	}
	return src->error;
}

u32 netmsgSvcStageStartWrite(struct netbuf *dst)
{
	netbufWriteU8(dst, SVC_STAGE_START);
	netdbgTx(1, SVC_STAGE_START);

	netbufWriteU32(dst, g_NetTick);

	netbufWriteU64(dst, g_RngSeed);
	netbufWriteU64(dst, g_Rng2Seed);

	netbufWriteU8(dst, g_StageNum);

	if (g_StageNum == STAGE_TITLE || g_StageNum == STAGE_CITRAINING) {
		// going back to lobby, don't need anything else
		return dst->error;
	}

	// game settings
	netbufWriteU8(dst, 0); // 0 for combat sim TODO: coop/anti
	netbufWriteU8(dst, g_MpSetup.scenario);
	netbufWriteU8(dst, g_MpSetup.scorelimit);
	netbufWriteU8(dst, g_MpSetup.timelimit);
	netbufWriteU16(dst, g_MpSetup.teamscorelimit);
	netbufWriteU16(dst, g_MpSetup.chrslots);
	netbufWriteU32(dst, g_MpSetup.options);
	netbufWriteData(dst, g_MpSetup.weapons, sizeof(g_MpSetup.weapons));
	netbufWriteU8(dst, (u8)g_Vars.mphilltime);
	netMpSimulantsWrite(dst);

	// who the fuck is in the game
	netbufWriteU8(dst, g_NetNumClients);
	for (s32 i = 0; i < g_NetMaxClients; ++i) {
		struct netclient *ncl = &g_NetClients[i];
		if (ncl->state) {
			ncl->settings.team = ncl->config->base.team;
			netbufWriteU8(dst, ncl->id);
			netbufWriteU8(dst, ncl->playernum);
			netbufWriteU8(dst, ncl->settings.team);
			netbufWriteU16(dst, ncl->settings.options);
			netbufWriteU8(dst, ncl->settings.bodynum);
			netbufWriteU8(dst, ncl->settings.headnum);
			netbufWriteF32(dst, ncl->settings.fovy);
			netbufWriteF32(dst, ncl->settings.fovzoommult);
			netbufWriteStr(dst, ncl->settings.name);
			memset(ncl->inmove, 0, sizeof(ncl->inmove));
			memset(ncl->outmove, 0, sizeof(ncl->outmove));
			ncl->lerpticks = 0;
			ncl->outmoveack = 0;
			ncl->state = CLSTATE_GAME;
		}
	}

	return dst->error;
}

u32 netmsgSvcStageStartRead(struct netbuf *src, struct netclient *srccl)
{
	if (srccl->state != CLSTATE_LOBBY) {
		sysLogPrintf(LOG_WARNING, "NET: SVC_STAGE from server but we're not in LOBBY state");
		netmsgSkipStageStartPayload(src);
		return src->error ? src->error : 1;
	}

	g_NetTick = netbufReadU32(src);

	g_NetRngSeeds[0] = netbufReadU64(src);
	g_NetRngSeeds[1] = netbufReadU64(src);
	g_NetRngLatch = true;

	const u8 stagenum = netbufReadU8(src);

	if (stagenum == STAGE_TITLE || stagenum == STAGE_CITRAINING) {
		// server went back to lobby, we don't really care
		return src->error;
	}

	const u8 mode = netbufReadU8(src); // TODO: coop, anti
	g_MpSetup.stagenum = stagenum;
	g_MpSetup.scenario = netbufReadU8(src);
	g_MpSetup.scorelimit = netbufReadU8(src);
	g_MpSetup.timelimit = netbufReadU8(src);
	g_MpSetup.teamscorelimit = netbufReadU16(src);
	g_MpSetup.chrslots = netbufReadU16(src);
	g_MpSetup.options = netbufReadU32(src);
	netbufReadData(src, g_MpSetup.weapons, sizeof(g_MpSetup.weapons));
	g_Vars.mphilltime = netbufReadU8(src);
	strcpy(g_MpSetup.name, "server");

	if (src->error) {
		sysLogPrintf(LOG_WARNING, "NET: malformed SVC_STAGE from server");
		return 1;
	}

	netMpSimulantsApply(src);

	if (src->error) {
		sysLogPrintf(LOG_WARNING, "NET: malformed simulant block in SVC_STAGE");
		return 1;
	}

	// read players
	const u8 numplayers = netbufReadU8(src);
	if (src->error || !numplayers || numplayers > g_NetMaxClients + 1) {
		sysLogPrintf(LOG_WARNING, "NET: malformed SVC_STAGE from server");
		return 2;
	}

	for (u8 i = 0; i < numplayers; ++i) {
		const u8 id = netbufReadU8(src);
		struct netclient *ncl = &g_NetClients[id];
		ncl->playernum = netbufReadU8(src);
		ncl->settings.team = netbufReadU8(src);
		if (ncl != g_NetLocalClient) {
			ncl->id = id;
			ncl->settings.options = netbufReadU16(src);
			ncl->settings.bodynum = netbufReadU8(src);
			ncl->settings.headnum = netbufReadU8(src);
			ncl->settings.fovy = netbufReadF32(src);
			ncl->settings.fovzoommult = netbufReadF32(src);
			char *name = netbufReadStr(src);
			if (name) {
				strncpy(ncl->settings.name, name, sizeof(ncl->settings.name) - 1);
			} else {
				sysLogPrintf(LOG_WARNING, "NET: malformed SVC_STAGE from server");
				return 3;
			}
		} else {
			// skip our own settings except for the team and player number
			netbufReadU16(src);
			netbufReadU8(src);
			netbufReadU8(src);
			netbufReadF32(src);
			netbufReadF32(src);
			netbufReadStr(src);
		}
		ncl->state = CLSTATE_GAME;
		ncl->player = NULL;
	}

	if (src->error) {
		return src->error;
	}

	// set teams on the player configs, but swap teams with the server player
	// because we haven't swapped player numbers yet (see netPlayersAlloc)
	for (u32 i = 0; i < NET_MAX_CLIENTS; ++i) {
		struct netclient *ncl = &g_NetClients[i];
		if (ncl->state) {
			u32 playernum = 0;
			if (ncl->id == 0) {
				playernum = g_NetLocalClient->playernum;
			} else if (ncl == g_NetLocalClient) {
				playernum = g_NetClients[0].playernum;
			} else {
				playernum = ncl->playernum;
			}
			g_PlayerConfigsArray[playernum].base.team = ncl->settings.team;
		}
	}

	g_NetNumClients = numplayers;

	sysLogPrintf(LOG_NOTE, "NET: SVC_STAGE from server: going to stage 0x%02x with %u players", g_MpSetup.stagenum, numplayers);

	mpStartMatch();
	menuStop();

	return 0;
}

u32 netmsgSvcStageEndWrite(struct netbuf *dst)
{
	netbufWriteU8(dst, SVC_STAGE_END);
	netdbgTx(1, SVC_STAGE_END);

	for (s32 i = 0; i < g_NetMaxClients; ++i) {
		struct netclient *ncl = &g_NetClients[i];
		if (ncl->state) {
			ncl->state = CLSTATE_LOBBY;
			ncl->playernum = 0;
			if (ncl->player) {
				ncl->player->client = NULL;
				ncl->player->isremote = false;
				ncl->player = NULL;
			}
			if (ncl->config) {
				ncl->config->client = NULL;
				ncl->config = NULL;
			}
		}
	}

	return dst->error;
}

u32 netmsgSvcStageEndRead(struct netbuf *src, struct netclient *srccl)
{
	for (s32 i = 0; i < g_NetMaxClients; ++i) {
		struct netclient *ncl = &g_NetClients[i];
		if (ncl->state) {
			ncl->state = CLSTATE_DISCONNECTED;
			ncl->player = NULL;
			ncl->config = NULL;
		}
	}

	g_NetLocalClient->state = CLSTATE_LOBBY;
	g_NetAwaitingRejoin = 0;
	g_NetForceRespawn = 0;
	if (g_NetLocalClient) {
		g_NetLocalClient->leftmatch = 0;
	}

	// Mid-match wait uses MENUROOT_MPPAUSE; close it so stats (-6) can take over.
	{
		const u32 prevplayernum = g_MpPlayerNum;
		g_MpPlayerNum = 0;
		if (g_Menus[0].curdialog
				&& g_Menus[0].curdialog->definition == &g_NetWaitingPlayersDialog) {
			func0f0f8120();
			if (g_Menus[0].depth > 0 || g_Menus[0].curdialog) {
				menuClose();
			}
		}
		g_MpPlayerNum = prevplayernum;
	}

	g_NumReasonsToEndMpMatch = 1;
	mainEndStage();

	return src->error;
}

u32 netmsgSvcPlayerMoveWrite(struct netbuf *dst, struct netclient *movecl)
{
	if (movecl->state < CLSTATE_GAME || !movecl->player || !movecl->player->prop) {
		return dst->error;
	}

	netbufWriteU8(dst, SVC_PLAYER_MOVE);
	netbufWriteU8(dst, movecl->id);
	netbufWriteU32(dst, movecl->inmove[0].tick);
	netbufWritePlayerMove(dst, &movecl->outmove[0]);
	if (movecl->outmove[0].ucmd & UCMD_FL_FORCEMASK) {
		netbufWriteRooms(dst, movecl->player->prop->rooms, ARRAYCOUNT(movecl->player->prop->rooms));
	}

	{
		const struct netplayermove *m = &movecl->outmove[0];
		netdbgEvent("tx",
				"\"msgid\":%u,\"name\":\"SVC_PLAYER_MOVE\",\"clid\":%u,\"mtick\":%u,\"ucmd\":%u"
				",\"weaponnum\":%d,\"pos\":[%.2f,%.2f,%.2f]",
				(unsigned)SVC_PLAYER_MOVE, (unsigned)movecl->id, (unsigned)m->tick,
				(unsigned)m->ucmd, (int)m->weaponnum, m->pos.x, m->pos.y, m->pos.z);
	}

	return dst->error;
}

static struct chrdata *netmsgBotChrForSlot(u8 slot)
{
	if (slot >= MAX_BOTS) {
		return NULL;
	}

	for (s32 i = 0; i < g_BotCount; ++i) {
		struct chrdata *chr = g_MpBotChrPtrs[i];
		if (chr && chr->aibot && chr->aibot->config == &g_BotConfigsArray[slot]) {
			return chr;
		}
	}

	return NULL;
}

static u8 netmsgSimGunFlags(const struct chrdata *chr)
{
	u8 flags = 0;

	if (!chr || !chr->prop) {
		return 0;
	}

	if (chr->actiontype == ACT_ATTACK || chr->actiontype == ACT_ATTACKAMOUNT
			|| chr->actiontype == ACT_BOT_ATTACKSTAND || chr->actiontype == ACT_BOT_ATTACKKNEEL
			|| chr->actiontype == ACT_BOT_ATTACKSTRAFE) {
		if (chr->act_attack.firegun[HAND_RIGHT]) {
			flags |= NETGUN_FIRING_R | NETGUN_FLASH_R;
		}
		if (chr->act_attack.firegun[HAND_LEFT]) {
			flags |= NETGUN_FIRING_L | NETGUN_FLASH_L;
		}
	} else if (chr->actiontype == ACT_ATTACKWALK) {
		if (chr->act_attackwalk.firegun[HAND_RIGHT]) {
			flags |= NETGUN_FIRING_R | NETGUN_FLASH_R;
		}
		if (chr->act_attackwalk.firegun[HAND_LEFT]) {
			flags |= NETGUN_FIRING_L | NETGUN_FLASH_L;
		}
	}

	if (chr->hidden & CHRHFLAG_FIRINGRIGHT) {
		flags |= NETGUN_FIRING_R | NETGUN_FLASH_R;
	}
	if (chr->hidden & CHRHFLAG_FIRINGLEFT) {
		flags |= NETGUN_FIRING_L | NETGUN_FLASH_L;
	}

	// Host may have already toggled gunfire parts for this frame.
	{
		struct prop *r = chrGetHeldProp((struct chrdata *)chr, HAND_RIGHT);
		struct prop *l = chrGetHeldProp((struct chrdata *)chr, HAND_LEFT);
		if (r && weaponIsGunfireVisible(r)) {
			flags |= NETGUN_FLASH_R | NETGUN_FIRING_R;
		}
		if (l && weaponIsGunfireVisible(l)) {
			flags |= NETGUN_FLASH_L | NETGUN_FIRING_L;
		}
	}

	return flags;
}

u32 netmsgSvcSimMoveWrite(struct netbuf *dst, u8 slot, struct chrdata *chr)
{
	if (!chr || !chr->prop) {
		return dst->error;
	}

	const struct aibot *aibot = chr->aibot;
	struct model *model = chr->model;
	const f32 look_yaw = aibot ? aibot->lookangle : chrGetInverseTheta(chr);
	const f32 model_yaw = model ? modelGetChrRotY(model) : look_yaw;
	const s16 anim_id = model ? modelGetAnimNum(model) : 0;
	const u8 anim_flip = model && modelIsFlipped(model) ? 1 : 0;
	const u8 anim_looping = (model && model->anim && model->anim->looping) ? 1 : 0;
	const f32 anim_frame = model ? modelGetCurAnimFrame(model) : 0.0f;
	const f32 anim_speed = model ? modelGetAnimSpeed(model) : 1.0f;
	const f32 anim_loopframe = (model && model->anim) ? model->anim->loopframe : 0.0f;
	const u8 dualweapons = (chr->weapons_held[HAND_LEFT] != NULL) ? 1 : 0;
	const u8 crouchpos = aibot ? (u8)botGuessCrouchPos((struct chrdata *)chr) : CROUCHPOS_STAND;

	netbufWriteU8(dst, SVC_SIM_MOVE);
	netbufWriteU8(dst, slot);
	netbufWritePropPtr(dst, chr->prop);
	netbufWriteCoord(dst, &chr->prop->pos);
	netbufWriteRooms(dst, chr->prop->rooms, ARRAYCOUNT(chr->prop->rooms));
	netbufWriteF32(dst, model_yaw);
	netbufWriteF32(dst, look_yaw);
	netbufWriteS16(dst, anim_id);
	netbufWriteU8(dst, anim_flip);
	netbufWriteU8(dst, anim_looping);
	netbufWriteF32(dst, anim_frame);
	netbufWriteF32(dst, anim_speed);
	netbufWriteF32(dst, anim_loopframe);
	netbufWriteF32(dst, chr->aimendback);
	netbufWriteF32(dst, chr->aimendsideback);
	netbufWriteU8(dst, aibot ? (u8)aibot->weaponnum : WEAPON_NONE);
	netbufWriteU8(dst, aibot ? (u8)aibot->gunfunc : FUNC_PRIMARY);
	netbufWriteU8(dst, netmsgSimGunFlags(chr));
	netbufWriteU8(dst, dualweapons);
	netbufWriteU8(dst, crouchpos);
	netbufWriteF32(dst, chr->cshield);

	netdbgEvent("tx",
			"\"msgid\":%u,\"name\":\"SVC_SIM_MOVE\",\"slot\":%u,\"syncid\":%u"
			",\"pos\":[%.2f,%.2f,%.2f],\"anim_id\":%d,\"weaponnum\":%u,\"cshield\":%.2f",
			(unsigned)SVC_SIM_MOVE, (unsigned)slot, (unsigned)chr->prop->syncid,
			chr->prop->pos.x, chr->prop->pos.y, chr->prop->pos.z,
			(int)anim_id, (unsigned)(aibot ? aibot->weaponnum : WEAPON_NONE), chr->cshield);

	return dst->error;
}

u32 netmsgSvcSimMoveRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 slot = netbufReadU8(src);
	struct prop *prop = netbufReadPropPtr(src);
	struct coord pos;
	RoomNum rooms[8] = { -1 };

	netbufReadCoord(src, &pos);
	netbufReadRooms(src, rooms, ARRAYCOUNT(rooms));
	struct netsimsnap snap = { 0 };
	snap.model_yaw = netbufReadF32(src);
	snap.look_yaw = netbufReadF32(src);
	snap.anim_id = netbufReadS16(src);
	snap.anim_flip = netbufReadU8(src);
	snap.anim_looping = netbufReadU8(src);
	snap.anim_frame = netbufReadF32(src);
	snap.anim_speed = netbufReadF32(src);
	snap.anim_loopframe = netbufReadF32(src);
	snap.aim_pitch = netbufReadF32(src);
	snap.aim_yaw = netbufReadF32(src);
	snap.weaponnum = netbufReadU8(src);
	snap.gunfunc = netbufReadU8(src);
	snap.gunflags = netbufReadU8(src);
	snap.dualweapons = netbufReadU8(src);
	snap.crouchpos = netbufReadU8(src);
	snap.cshield = netbufReadF32(src);
	snap.pos = pos;
	/* Drop out-of-range room ids before roomsCopy / cdFindGround. */
	{
		s32 ri;
		for (ri = 0; ri < ARRAYCOUNT(rooms); ri++) {
			if (rooms[ri] < 0) {
				rooms[ri] = -1;
				break;
			}
			if (rooms[ri] >= g_Vars.roomcount) {
				rooms[ri] = -1;
				break;
			}
		}
		if (ri >= ARRAYCOUNT(rooms)) {
			rooms[ARRAYCOUNT(rooms) - 1] = -1;
		}
	}
	roomsCopy(rooms, snap.rooms);
	snap.valid = 1;

	netdbgEvent("fields",
			"\"msgid\":%u,\"name\":\"SVC_SIM_MOVE\",\"slot\":%u,\"syncid\":%u"
			",\"pos\":[%.2f,%.2f,%.2f],\"anim_id\":%d,\"weaponnum\":%u,\"cshield\":%.2f",
			(unsigned)SVC_SIM_MOVE, (unsigned)slot,
			(unsigned)(prop ? prop->syncid : 0),
			pos.x, pos.y, pos.z, (int)snap.anim_id,
			(unsigned)snap.weaponnum, snap.cshield);

	if (src->error || srccl->state < CLSTATE_GAME || g_NetMode != NETMODE_CLIENT) {
		return src->error;
	}

	if (slot >= MAX_BOTS) {
		netdbgEvent("warn", "\"action\":\"sim_move_bad_slot\",\"slot\":%u", (unsigned)slot);
		netdbgFlush();
		return src->error;
	}

	// Prefer slot mapping — wire prop can soft-fail syncid lookup without
	// killing the snapshot (invisible/frozen sims).
	struct chrdata *chr = netmsgBotChrForSlot(slot);
	if (!chr || !chr->prop) {
		if (!prop || prop->type != PROPTYPE_CHR || !prop->chr || !prop->chr->aibot) {
			return src->error;
		}
		chr = prop->chr;
	}

	// Host only sends living snaps. In-flight poses can arrive during ACT_DIE —
	// never abort the death anim. Only ACT_DEAD (anim finished / waiting) is
	// treated as a host respawn.
	if (chrIsDead(chr)) {
		if (chr->actiontype == ACT_DEAD) {
			netdbgEvent("client",
					"\"action\":\"sim_act_dead_to_respawn\",\"syncid\":%u,\"slot\":%u",
					(unsigned)(chr->prop ? chr->prop->syncid : 0), (unsigned)slot);
			netClientSimForceRespawnFromSnap(chr, &snap);
			netdbgFlush();
		} else {
			netdbgEvent("client",
					"\"action\":\"sim_move_during_die\",\"syncid\":%u,\"act\":%d,\"slot\":%u",
					(unsigned)(chr->prop ? chr->prop->syncid : 0),
					(int)chr->actiontype, (unsigned)slot);
			return src->error;
		}
	}

	netSimReceiveSnapshot(slot, &snap);

	return src->error;
}

u32 netmsgSvcPlayerMoveRead(struct netbuf *src, struct netclient *srccl)
{
	u8 id = 0;
	u32 outmoveack = 0;
	struct netplayermove newmove;
	RoomNum newrooms[8] = { -1 };

	id = netbufReadU8(src);
	outmoveack = netbufReadU32(src);
	netbufReadPlayerMove(src, &newmove);
	if (newmove.ucmd & UCMD_FL_FORCEMASK) {
		netbufReadRooms(src, newrooms, ARRAYCOUNT(newrooms));
	}

	netdbgEvent("fields",
			"\"msgid\":%u,\"name\":\"SVC_PLAYER_MOVE\",\"clid\":%u,\"ack\":%u,\"mtick\":%u"
			",\"ucmd\":%u,\"weaponnum\":%d,\"pos\":[%.2f,%.2f,%.2f]",
			(unsigned)SVC_PLAYER_MOVE, (unsigned)id, (unsigned)outmoveack,
			(unsigned)newmove.tick, (unsigned)newmove.ucmd, (int)newmove.weaponnum,
			newmove.pos.x, newmove.pos.y, newmove.pos.z);

	if (src->error || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	if (!netmsgClientIdOk(id)) {
		netdbgEvent("warn", "\"action\":\"player_move_bad_clid\",\"clid\":%u", (unsigned)id);
		return 1;
	}

	struct netclient *movecl = &g_NetClients[id];

	// make space in the move stack
	memmove(movecl->inmove + 1, movecl->inmove, sizeof(movecl->inmove) - sizeof(*movecl->inmove));
	movecl->inmove[0] = newmove;
	movecl->outmoveack = outmoveack;
	movecl->lerpticks = 0;

	if (movecl == g_NetLocalClient && (newmove.ucmd & UCMD_FL_FORCEMASK)) {
		// server wants to teleport us
		if (movecl->player && movecl->player->prop) {
			chrSetPos(movecl->player->prop->chr, &newmove.pos, newrooms, newmove.angles[0], (newmove.ucmd & UCMD_FL_FORCEGROUND) != 0);
		}
	}

	return src->error;
}

static u8 netmsgPlayerGunsFlags(const struct player *pl)
{
	u8 flags = 0;
	if (pl->hands[HAND_LEFT].flashon) {
		flags |= NETGUN_FLASH_L;
	}
	if (pl->hands[HAND_RIGHT].flashon) {
		flags |= NETGUN_FLASH_R;
	}
	if (pl->hands[HAND_LEFT].firing) {
		flags |= NETGUN_FIRING_L;
	}
	if (pl->hands[HAND_RIGHT].firing) {
		flags |= NETGUN_FIRING_R;
	}
	if (pl->insightaimmode) {
		flags |= NETGUN_AIMMODE;
	}
	return flags;
}

u32 netmsgSvcPlayerGunsWrite(struct netbuf *dst, struct netclient *actcl)
{
	if (actcl->state < CLSTATE_GAME || !actcl->player || !actcl->player->prop) {
		return dst->error;
	}

	struct player *pl = actcl->player;
	const u8 flags = netmsgPlayerGunsFlags(pl);

	// Remotes never run the FP shootrot pass; derive aim from look pitch.
	f32 shootrotx = pl->shootrotx;
	f32 shootroty = pl->shootroty;
	if (pl->isremote) {
		shootrotx = pl->vv_verta * (M_BADTAU / 360.0f);
		if (shootrotx >= M_PI) {
			shootrotx -= M_BADTAU;
		}
		shootroty = 0.f;
	}

	netbufWriteU8(dst, SVC_PLAYER_GUNS);
	netdbgTx(1, SVC_PLAYER_GUNS);
	netbufWriteU8(dst, actcl->id);
	netbufWriteU8(dst, flags);
	netbufWriteF32(dst, shootrotx);
	netbufWriteF32(dst, shootroty);
	return dst->error;
}

u32 netmsgSvcPlayerGunsRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 id = netbufReadU8(src);
	const u8 flags = netbufReadU8(src);
	const f32 shootrotx = netbufReadF32(src);
	const f32 shootroty = netbufReadF32(src);

	if (src->error || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	/* Array is NET_MAX_CLIENTS+1 (temp slot); reject anything past that. */
	if (id >= ARRAYCOUNT(g_NetClients)) {
		return 1;
	}

	struct netclient *actcl = &g_NetClients[id];
	// Never overwrite the local player's own gun — FP feel stays authoritative.
	if (actcl == g_NetLocalClient || !actcl->player) {
		return src->error;
	}

	actcl->netgunflags = flags;
	actcl->netshootrotx = shootrotx;
	actcl->netshootroty = shootroty;
	actcl->netgunvalid = 1;
	netApplyRemoteGunVisuals(actcl->player);

	return src->error;
}

u32 netmsgSvcPlayerStatsWrite(struct netbuf *dst, struct netclient *actcl)
{
	if (actcl->state < CLSTATE_GAME || !actcl->player || !actcl->player->prop
			|| !actcl->player->prop->chr) {
		return dst->error;
	}
	const struct player *pl = actcl->player;
	const u8 flags = (pl->isdead != 0) | (pl->gunctrl.dualwielding << 1) |
		(pl->hands[0].inuse << 2) | (pl->hands[1].inuse << 3);
	netbufWriteU8(dst, SVC_PLAYER_STATS);
	netdbgTx(1, SVC_PLAYER_STATS);
	netbufWriteU8(dst, actcl->id);
	netbufWriteU8(dst, flags);
	netbufWriteS8(dst, pl->gunctrl.weaponnum);
	netbufWriteF32(dst, pl->prop->chr->damage);
	netbufWriteF32(dst, pl->bondhealth);
	netbufWriteF32(dst, pl->prop->chr->cshield);
	netbufWriteCoord(dst, &pl->bondshotspeed);

	for (s32 i = 0; i < 2; ++i) {
		if (pl->hands[i].inuse) {
			netbufWriteS16(dst, pl->hands[i].loadedammo[0]);
			netbufWriteS16(dst, pl->hands[i].loadedammo[1]);
		}
	}

	// assemble a bitmask of all non-zero ammo entries below 32
	u32 mask = 0;
	for (u32 i = 0; i < 32; ++i) {
		if (pl->ammoheldarr[i]) {
			mask |= (1 << i);
		}
	}

	netbufWriteU32(dst, mask);

	// write all entries that are non-zero or have index above 32
	for (s32 i = 0; i < ARRAYCOUNT(pl->ammoheldarr); ++i) {
		if (pl->ammoheldarr[i] || i >= 32) {
			netbufWriteS16(dst, pl->ammoheldarr[i]);
		}
	}

	return dst->error;
}

u32 netmsgSvcPlayerStatsRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 clid = netbufReadU8(src);
	const u8 flags = netbufReadU8(src);
	const s8 newweaponnum = netbufReadS8(src);
	const f32 newdamage = netbufReadF32(src);
	const f32 newhealth = netbufReadF32(src);
	const f32 newshield = netbufReadF32(src);
	struct coord newshotspeed; netbufReadCoord(src, &newshotspeed);
	const bool handused[2] = { (flags & (1 << 2)) != 0, (flags & (1 << 3)) != 0 };

	if (src->error) {
		return src->error;
	}

	if (!netmsgClientIdOk(clid)) {
		netdbgEvent("warn", "\"action\":\"player_stats_bad_clid\",\"clid\":%u", (unsigned)clid);
		return 1;
	}

	struct netclient *actcl = g_NetClients + clid;
	if (actcl->state < CLSTATE_GAME) {
		return 1;
	}

	struct player *pl = actcl->player;
	if (!pl || !pl->prop || !pl->prop->chr) {
		return src->error;
	}

	pl->prop->chr->damage = newdamage;
	pl->prop->chr->cshield = newshield;
	pl->bondhealth = newhealth;
	pl->bondshotspeed = newshotspeed;

	for (s32 i = 0; i < 2; ++i) {
		if (handused[i]) {
			pl->hands[i].loadedammo[0] = netbufReadS16(src);
			pl->hands[i].loadedammo[1] = netbufReadS16(src);
		}
	}

	const u32 ammomask = netbufReadU32(src);
	for (s32 i = 0; i < ARRAYCOUNT(pl->ammoheldarr); ++i) {
		if (i >= 32 || (ammomask & (1 << i))) {
			pl->ammoheldarr[i] = netbufReadS16(src);
		} else {
			pl->ammoheldarr[i] = 0;
		}
	}

	const s32 prevplayernum = g_Vars.currentplayernum;
	setCurrentPlayerNum(actcl->playernum);

	const bool newisdead = (flags & (1 << 0)) != 0;
	if (!pl->isdead && newisdead) {
		s16 shooter;
		if (pl->prop->chr->lastshooter >= 0 && pl->prop->chr->timeshooter > 0) {
			shooter = pl->prop->chr->lastshooter;
		} else {
			shooter = g_Vars.currentplayernum;
		}
		playerDieByShooter(shooter, true);
	} else if (pl->isdead && !newisdead) {
		playerStartNewLife();
	}

	const bool dualwielding = (flags & (1 << 1)) != 0;
	// Never force-equip the local human from host stats — that resets
	// gset.weaponfunc to primary while gunfuncs still say secondary (disarm).
	if (!pl->isdead
			&& actcl != g_NetLocalClient
			&& (newweaponnum != pl->gunctrl.weaponnum || dualwielding != pl->gunctrl.dualwielding)) {
		pl->gunctrl.dualwielding = dualwielding;
		bgunEquipWeapon(newweaponnum);
	}

	setCurrentPlayerNum(prevplayernum);

	return src->error;
}

u32 netmsgSvcPropMoveWrite(struct netbuf *dst, struct prop *prop, struct coord *initrot)
{
	u8 flags = (prop->obj != NULL);

	struct projectile *projectile = NULL;
	if (prop->obj) {
		if (prop->obj->hidden & OBJHFLAG_EMBEDDED) {
			projectile = prop->obj->embedment->projectile;
		} else if (prop->obj->hidden & OBJHFLAG_PROJECTILE) {
			projectile = prop->obj->projectile;
		}
		if (projectile) {
			flags |= (1 << 1);
			if (initrot) {
				flags |= (1 << 2);
			}
			if (prop->obj->type == OBJTYPE_HOVERPROP || prop->obj->type == OBJTYPE_HOVERBIKE) {
				flags |= (1 << 3);
			}
		}
	}

	netbufWriteU8(dst, SVC_PROP_MOVE);
	netbufWriteU8(dst, flags);
	netbufWritePropPtr(dst, prop);
	netbufWriteCoord(dst, &prop->pos);
	netbufWriteRooms(dst, prop->rooms, ARRAYCOUNT(prop->rooms));
	if (projectile) {
		netbufWriteCoord(dst, &projectile->speed);
		netbufWriteF32(dst, projectile->unk0dc);
		netbufWriteU32(dst, projectile->flags);
		netbufWriteS8(dst, projectile->bouncecount);
		netbufWritePropPtr(dst, projectile->ownerprop);
		netbufWritePropPtr(dst, projectile->targetprop);
		if (initrot) {
			netbufWriteCoord(dst, initrot);
		}
		if (prop->obj->type == OBJTYPE_HOVERPROP || prop->obj->type == OBJTYPE_HOVERBIKE) {
			netbufWriteF32(dst, projectile->unk08c);
			netbufWriteF32(dst, projectile->unk098);
			netbufWriteF32(dst, projectile->unk0e0);
			netbufWriteF32(dst, projectile->unk0e4);
			netbufWriteF32(dst, projectile->unk0ec);
			netbufWriteF32(dst, projectile->unk0f0);
		}
	}

	netdbgEvent("tx",
			"\"msgid\":%u,\"name\":\"SVC_PROP_MOVE\",\"syncid\":%u,\"type\":%u,\"objtype\":%u"
			",\"room0\":%d,\"projectile\":%d",
			(unsigned)SVC_PROP_MOVE, (unsigned)prop->syncid, (unsigned)prop->type,
			(unsigned)(prop->obj ? prop->obj->type : 0),
			(int)prop->rooms[0], projectile ? 1 : 0);

	return dst->error;
}

u32 netmsgSvcPropMoveRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 flags = netbufReadU8(src);
	struct prop *prop = netbufReadPropPtr(src);

	netdbgEvent("fields",
			"\"msgid\":%u,\"name\":\"SVC_PROP_MOVE\",\"syncid\":%u,\"flags\":%u,\"projectile\":%d",
			(unsigned)SVC_PROP_MOVE, (unsigned)(prop ? prop->syncid : 0),
			(unsigned)flags, (flags & (1 << 1)) ? 1 : 0);
	struct coord pos; netbufReadCoord(src, &pos);
	RoomNum rooms[8] = { -1 }; netbufReadRooms(src, rooms, ARRAYCOUNT(rooms));

	if (src->error || !prop) {
		return src->error;
	}

	if (srccl->state < CLSTATE_GAME) {
		return 1;
	}

	prop->pos = pos;

	if (!propRoomsEqual(rooms, prop->rooms)) {
		if (prop->active) {
			propDeregisterRooms(prop);
		}
		roomsCopy(rooms, prop->rooms);
		if (prop->active) {
			propRegisterRooms(prop);
		}
	}

	if (!(flags & (1 << 0))) {
		return src->error;
	}

	if (!prop->obj) {
		sysLogPrintf(LOG_WARNING, "NET: SVC_PROP_MOVE: prop %u should have an obj, but doesn't", prop->syncid);
		return 1;
	}

	if (!(flags & (1 << 1))) {
		return src->error;
	}

	/* EMBEDDED without embedment AVs — clear flag or allocate before touch. */
	if ((prop->obj->hidden & OBJHFLAG_EMBEDDED) && !prop->obj->embedment) {
		prop->obj->hidden &= ~OBJHFLAG_EMBEDDED;
	}

	// create a projectile for this prop if it isn't already there
	func0f0685e4(prop);

	struct projectile *projectile = NULL;
	if ((prop->obj->hidden & OBJHFLAG_EMBEDDED) && prop->obj->embedment) {
		projectile = prop->obj->embedment->projectile;
	} else if (prop->obj->hidden & OBJHFLAG_PROJECTILE) {
		projectile = prop->obj->projectile;
	}

	if (!projectile) {
		sysLogPrintf(LOG_WARNING, "NET: SVC_PROP_MOVE: prop %u should have a projectile, but doesn't", prop->syncid);
		return 1;
	}

	netbufReadCoord(src, &projectile->speed);
	projectile->unk0dc = netbufReadF32(src);
	projectile->flags = netbufReadU32(src);
	projectile->bouncecount = netbufReadS8(src);
	projectile->ownerprop = netbufReadPropPtr(src);
	projectile->targetprop = netbufReadPropPtr(src);
	if (projectile->ownerprop && !propIsInPool(projectile->ownerprop)) {
		projectile->ownerprop = NULL;
	}
	if (projectile->targetprop && !propIsInPool(projectile->targetprop)) {
		projectile->targetprop = NULL;
	}

	if (flags & (1 << 2)) {
		struct coord initrot; netbufReadCoord(src, &initrot);
		mtx4LoadRotation(&initrot, &projectile->mtx);
	}

	if (flags & (1 << 3)) {
		projectile->unk08c = netbufReadF32(src);
		projectile->unk098 = netbufReadF32(src);
		projectile->unk0e0 = netbufReadF32(src);
		projectile->unk0e4 = netbufReadF32(src);
		projectile->unk0ec = netbufReadF32(src);
		projectile->unk0f0 = netbufReadF32(src);
	}

	prop->pos = pos;

	return src->error;
}

u32 netmsgSvcPropSpawnWrite(struct netbuf *dst, struct prop *prop)
{
	const u8 msgflags = (prop->active != 0) | ((prop->obj != NULL) << 1) | ((prop->forcetick != 0) << 2);
	const u8 objtype = prop->obj ? prop->obj->type : 0;

	netPropEnsureSyncId(prop);

	netbufWriteU8(dst, SVC_PROP_SPAWN);
	netbufWriteU8(dst, msgflags);
	netbufWriteU32(dst, prop->syncid);
	netbufWriteCoord(dst, &prop->pos);
	netbufWriteRooms(dst, prop->rooms, ARRAYCOUNT(prop->rooms));
	netbufWritePropPtr(dst, prop->parent);
	netbufWriteU8(dst, prop->type);
	netbufWriteU8(dst, objtype);
	netbufWriteU8(dst, prop->flags);

	switch (prop->type) {
		case PROPTYPE_WEAPON:
			// dropped gun or projectile
			if (!prop->weapon) {
				return 1;
			}
			netbufWriteS16(dst, prop->weapon->base.modelnum);
			netbufWriteU8(dst, prop->weapon->weaponnum);
			netbufWriteS8(dst, prop->weapon->dualweaponnum);
			netbufWriteS8(dst, prop->weapon->unk5d);
			netbufWriteS8(dst, prop->weapon->unk5e);
			netbufWriteU8(dst, prop->weapon->gunfunc);
			netbufWriteS16(dst, prop->weapon->timer240);
			break;
		case PROPTYPE_OBJ:
			if (!prop->obj) {
				return 1;
			}
			// we already send most of the important obj stuff below, so
			netbufWriteS16(dst, prop->obj->modelnum);
			if (objtype == OBJTYPE_AUTOGUN) {
				// thrown laptop probably — hidden>>28 is mpPlayerGetIndex, not players[]
				struct autogunobj *autogun = (struct autogunobj *)prop->obj;
				const u8 ownerplayernum = (prop->obj->hidden & 0xf0000000) >> 28;
				u8 clid = NET_NULL_CLIENT;
				struct chrdata *ownerchr = mpGetChrFromPlayerIndex(ownerplayernum);

				if (ownerchr && ownerchr->prop && ownerchr->prop->type == PROPTYPE_PLAYER) {
					const s32 pnum = playermgrGetPlayerNumByProp(ownerchr->prop);

					if (pnum >= 0 && pnum < PLAYERCOUNT()
							&& g_Vars.players[pnum] && g_Vars.players[pnum]->client) {
						clid = (u8)g_Vars.players[pnum]->client->id;
					}
				}

				netbufWriteU8(dst, autogun->ammoquantity);
				netbufWriteU8(dst, autogun->firecount);
				netbufWriteU8(dst, autogun->targetteam);
				netbufWriteU8(dst, clid);
			}
			break;
		default:
			break;
	}

	if (prop->obj) {
		netbufWriteU32(dst, prop->obj->flags);
		netbufWriteU32(dst, prop->obj->flags2);
		netbufWriteU32(dst, prop->obj->flags3);
		netbufWriteU32(dst, prop->obj->hidden);
		netbufWriteU8(dst, prop->obj->hidden2);
		netbufWriteU16(dst, prop->obj->extrascale);
		netbufWriteS16(dst, prop->obj->pad);
		for (s32 i = 0; i < 3; ++i) {
			for (s32 j = 0; j < 3; ++j) {
				netbufWriteF32(dst, prop->obj->realrot[i][j]);
			}
		}
		if ((prop->obj->hidden & OBJHFLAG_PROJECTILE) && prop->obj->projectile) {
			netbufWriteCoord(dst, &prop->obj->projectile->nextsteppos);
			netbufWritePropPtr(dst, prop->obj->projectile->ownerprop);
			netbufWritePropPtr(dst, prop->obj->projectile->targetprop);
			netbufWriteU32(dst, prop->obj->projectile->flags);
			netbufWriteF32(dst, prop->obj->projectile->unk08c);
			netbufWriteS16(dst, prop->obj->projectile->pickuptimer240);
			netbufWriteS16(dst, prop->obj->projectile->droptype);
			netbufWriteMtxf(dst, &prop->obj->projectile->mtx);
			if (prop->obj->projectile->flags & PROJECTILEFLAG_POWERED) {
				netbufWriteF32(dst, prop->obj->projectile->unk010);
				netbufWriteF32(dst, prop->obj->projectile->unk014);
				netbufWriteF32(dst, prop->obj->projectile->unk018);
			}
		}
	}

	if (objtype == OBJTYPE_AUTOGUN && prop->obj) {
		struct autogunobj *ag = (struct autogunobj *)prop->obj;
		netdbgEvent("laptop",
				"\"action\":\"spawn_tx\",\"syncid\":%u,\"ammo\":%u,\"firecount\":%u"
				",\"thrown\":%d,\"room0\":%d",
				(unsigned)prop->syncid, (unsigned)ag->ammoquantity,
				(unsigned)ag->firecount,
				(prop->obj->flags & OBJFLAG_THROWNLAPTOP) ? 1 : 0,
				(int)prop->rooms[0]);
	}

	netdbgEvent("tx",
			"\"msgid\":%u,\"name\":\"SVC_PROP_SPAWN\",\"syncid\":%u,\"type\":%u,\"objtype\":%u"
			",\"room0\":%d,\"projectile\":%d",
			(unsigned)SVC_PROP_SPAWN, (unsigned)prop->syncid, (unsigned)prop->type,
			(unsigned)objtype, (int)prop->rooms[0],
			(prop->obj && (prop->obj->hidden & OBJHFLAG_PROJECTILE)) ? 1 : 0);

	return dst->error;
}

u32 netmsgSvcPropSpawnRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 msgflags = netbufReadU8(src);
	const u32 syncid = netbufReadU32(src);
	struct coord pos; netbufReadCoord(src, &pos);
	RoomNum rooms[8] = { -1 }; netbufReadRooms(src, rooms, ARRAYCOUNT(rooms));
	struct prop *parent = netbufReadPropPtr(src);
	const u8 type = netbufReadU8(src);
	const u8 objtype = netbufReadU8(src);
	const u8 propflags = netbufReadU8(src);

	netdbgEvent("fields",
			"\"msgid\":%u,\"name\":\"SVC_PROP_SPAWN\",\"syncid\":%u,\"type\":%u,\"objtype\":%u"
			",\"room0\":%d,\"msgflags\":%u",
			(unsigned)SVC_PROP_SPAWN, (unsigned)syncid, (unsigned)type, (unsigned)objtype,
			(int)rooms[0], (unsigned)msgflags);

	if (src->error) {
		return src->error;
	}
	if (parent && !propIsInPool(parent)) {
		parent = NULL;
	}

	if (srccl->state < CLSTATE_GAME) {
		netmsgSkipPropSpawnBody(src, msgflags, type, objtype);
		return src->error ? src->error : 1;
	}

	// Rejoin/resync may re-send SPAWN for an existing token — reuse it.
	// Only reuse when type/obj match; a colliding syncid on a chr/map prop used
	// to AV here (write weaponnum into a dangling/wrong pointer).
	struct prop *existing = syncid ? netmsgFindPropBySyncId(syncid) : NULL;
	if (existing && (existing->type != type
			|| (type == PROPTYPE_WEAPON && (!existing->weapon || existing->weapon->base.type != OBJTYPE_WEAPON))
			|| (type == PROPTYPE_OBJ && !existing->obj))) {
		sysLogPrintf(LOG_WARNING, "NET: SPAWN syncid %u type mismatch (have %u) - allocating fresh",
				syncid, existing->type);
		/* Clear stale id so the new prop is the only match for ReadPropPtr. */
		netPropClearSyncId(existing);
		existing = NULL;
	}
	struct prop *prop = existing;
	struct prop *allocated = NULL;

	/*
	 * AUTOGUN/BASIC objs allocate via laptopDeploy / setupCreateObject —
	 * do not pre-allocate an empty prop for those.
	 */
	if (!prop && !(type == PROPTYPE_OBJ
			&& (objtype == OBJTYPE_AUTOGUN || objtype == OBJTYPE_BASIC))) {
		prop = propAllocate();
		if (!prop) {
			/* Second chance after free-list rebuild inside allocate failed. */
			propsRepairActiveList("spawn_alloc_fail");
			propsRebuildFreeList("spawn_alloc_fail");
			prop = propAllocate();
		}
		allocated = prop;
		if (!prop) {
			netdbgEvent("warn",
					"\"action\":\"spawn_alloc_exhausted\",\"syncid\":%u,\"type\":%u",
					(unsigned)syncid, (unsigned)type);
			netmsgSkipPropSpawnBody(src, msgflags, type, objtype);
			return src->error ? src->error : 1;
		}
	}

	g_NetApplyingPropSpawn = 1;

	if (type == PROPTYPE_WEAPON) {
		const s16 modelnum = netbufReadS16(src);
		const u8 weaponnum = netbufReadU8(src);
		const s8 dualweaponnum = netbufReadS8(src);
		const s8 unk5d = netbufReadS8(src);
		const s8 unk5e = netbufReadS8(src);
		const u8 gunfunc = netbufReadU8(src);
		const s16 timer240 = netbufReadS16(src);

		if (modelnum < 0) {
			sysLogPrintf(LOG_WARNING, "NET: SPAWN weapon with bad modelnum %d", modelnum);
			if (allocated) {
				propFree(allocated);
			}
			g_NetApplyingPropSpawn = 0;
			return 1;
		}

		if (existing && existing->weapon) {
			// Refresh an already-known ground token (e.g. after leave/rejoin).
			existing->weapon->weaponnum = weaponnum;
			existing->weapon->unk5d = unk5d;
			existing->weapon->unk5e = unk5e;
			existing->weapon->gunfunc = gunfunc;
			existing->weapon->timer240 = timer240;
			existing->timetoregen = 0;
			if (existing->obj) {
				existing->obj->hidden &= ~(OBJHFLAG_GONE | OBJHFLAG_DELETING);
			}
			prop = existing;
		} else {
			setupLoadModeldef(modelnum);
			struct modeldef *modeldef = g_ModelStates[modelnum].modeldef;
			if (!modeldef) {
				sysLogPrintf(LOG_WARNING, "NET: SPAWN weapon modeldef missing for %d", modelnum);
				if (allocated) {
					propFree(allocated);
				}
				g_NetApplyingPropSpawn = 0;
				return 1;
			}
			struct model *model = modelmgrInstantiateModelWithoutAnim(modeldef);
			struct weaponobj *weapon = weaponCreate(prop == NULL, model == NULL, modeldef);
			if (!weapon) {
				sysLogPrintf(LOG_WARNING, "NET: SPAWN weaponCreate failed (pool exhausted?)");
				if (model) {
					modelmgrFreeModel(model);
				}
				if (allocated) {
					propFree(allocated);
				}
				g_NetApplyingPropSpawn = 0;
				return 1;
			}
			struct weaponobj tmp = {
				256,                    // extrascale
				0,                      // hidden2
				OBJTYPE_WEAPON,         // type
				0,                      // modelnum
				-1,                     // pad
				OBJFLAG_FALL,           // flags
				0,                      // flags2
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
				0,                      // weaponnum
				0,                      // unk5d
				0,                      // unk5e
				0,                      // gunfunc
				0,                      // fadeouttimer60
				-1,                     // dualweaponnum
				-1,                     // timer240
				NULL,                   // dualweapon
			};
			*weapon = tmp;
			weapon->base.modelnum = modelnum;
			weapon->weaponnum = weaponnum;
			weapon->unk5d = unk5d;
			weapon->unk5e = unk5e;
			weapon->gunfunc = gunfunc;
			weapon->timer240 = timer240;
			prop = func0f08adc8(weapon, modeldef, prop, model);
			if (prop && (weaponnum == WEAPON_BRIEFCASE2 || weaponnum == WEAPON_DATAUPLINK)) {
				prop->forcetick = true;
			}
		}
	} else if (type == PROPTYPE_OBJ) {
		const s16 modelnum = netbufReadS16(src);
		if (objtype == OBJTYPE_BASIC) {
			/* HTM terminals (MODEL_GOODPC) — host-only InitProps + SPAWN. */
			if (existing && existing->obj && existing->obj->type == OBJTYPE_BASIC) {
				prop = existing;
			} else {
				struct defaultobj *obj;

				setupLoadModeldef(modelnum);
				obj = mempAlloc(ALIGN16(sizeof(struct defaultobj)), MEMPOOL_STAGE);
				if (!obj) {
					sysLogPrintf(LOG_WARNING, "NET: SPAWN basic obj OOM model %d", modelnum);
					g_NetApplyingPropSpawn = 0;
					return 1;
				}
				memset(obj, 0, sizeof(*obj));
				obj->type = OBJTYPE_BASIC;
				obj->modelnum = modelnum;
				obj->pad = -1;
				obj->flags = OBJFLAG_FALL | OBJFLAG_INVINCIBLE | OBJFLAG_FORCENOBOUNCE;
				obj->flags2 = OBJFLAG2_IMMUNETOGUNFIRE | OBJFLAG2_IMMUNETOEXPLOSIONS;
				obj->extrascale = 256;
				obj->maxdamage = 1000;
				setupCreateObject(obj, 123);
				if (!obj->prop) {
					sysLogPrintf(LOG_WARNING, "NET: SPAWN basic setupCreateObject failed");
					g_NetApplyingPropSpawn = 0;
					return 1;
				}
				prop = obj->prop;
			}
		} else if (objtype == OBJTYPE_AUTOGUN) {
			// thrown laptop — replica must not steal owner ammo (g_NetApplyingPropSpawn)
			const u8 ammocount = netbufReadU8(src);
			const u8 firecount = netbufReadU8(src);
			const u8 targetteam = netbufReadU8(src);
			const u8 clid = netbufReadU8(src);
			struct chrdata *ownerchr = NULL;
			if (clid < ARRAYCOUNT(g_NetClients) && g_NetClients[clid].player
					&& g_NetClients[clid].player->prop) {
				ownerchr = g_NetClients[clid].player->prop->chr;
			}
			if (!ownerchr) {
				sysLogPrintf(LOG_WARNING, "NET: SPAWN laptop owner clid %u missing", clid);
				g_NetApplyingPropSpawn = 0;
				return 1;
			}
			// Prefer the owner's thrown-laptop slot over a fresh deploy. A second
			// SPAWN with a new syncid used to call laptopDeploy, which explodes
			// the still-airborne replica and hard-locks the thrower's client.
			{
				const s32 thrownidx = mpPlayerGetIndex(ownerchr);
				struct autogunobj *slot = (thrownidx >= 0 && thrownidx < g_MaxThrownLaptops)
						? &g_ThrownLaptops[thrownidx] : NULL;

				/* Slot prop must still own the autogun — stale prop* after free
				 * left prop->obj NULL and AV'd on realrot write (laptop throw echo). */
				if (slot && slot->base.prop && slot->base.type == OBJTYPE_AUTOGUN
						&& slot->base.prop->obj == &slot->base) {
					const u8 prevammo = slot->ammoquantity;
					slot->ammoquantity = ammocount;
					slot->firecount = firecount;
					slot->targetteam = targetteam;
					prop = slot->base.prop;
					netdbgEvent("laptop",
							"\"action\":\"spawn_rx_adopt_slot\",\"syncid\":%u,\"slot_sync\":%u"
							",\"clid\":%u,\"ammo\":%u,\"prev_ammo\":%u,\"firecount\":%u",
							(unsigned)syncid, (unsigned)prop->syncid, (unsigned)clid,
							(unsigned)ammocount, (unsigned)prevammo, (unsigned)firecount);
				} else if (slot && slot->base.prop && slot->base.prop->obj != &slot->base) {
					netdbgEvent("warn",
							"\"action\":\"laptop_stale_slot\",\"syncid\":%u,\"clid\":%u,\"idx\":%d",
							(unsigned)syncid, (unsigned)clid, thrownidx);
					slot->base.prop = NULL;
					slot = NULL;
				}

				if (!prop && existing && existing->obj && existing->obj->type == OBJTYPE_AUTOGUN) {
					struct autogunobj *ag = (struct autogunobj *)existing->obj;
					const u8 prevammo = ag->ammoquantity;
					ag->ammoquantity = ammocount;
					ag->firecount = firecount;
					ag->targetteam = targetteam;
					prop = existing;
					netdbgEvent("laptop",
							"\"action\":\"spawn_rx_adopt_existing\",\"syncid\":%u,\"clid\":%u"
							",\"ammo\":%u,\"prev_ammo\":%u,\"firecount\":%u",
							(unsigned)syncid, (unsigned)clid,
							(unsigned)ammocount, (unsigned)prevammo, (unsigned)firecount);
				} else if (!prop) {
					struct autogunobj *obj = laptopDeploy(modelnum, NULL, ownerchr);
					if (!obj || !obj->base.prop || !obj->base.prop->obj) {
						sysLogPrintf(LOG_WARNING, "NET: SPAWN laptopDeploy failed");
						netdbgEvent("warn",
								"\"action\":\"laptop_deploy_fail\",\"syncid\":%u,\"clid\":%u",
								(unsigned)syncid, (unsigned)clid);
						g_NetApplyingPropSpawn = 0;
						return 1;
					}
					obj->ammoquantity = ammocount;
					obj->firecount = firecount;
					obj->targetteam = targetteam;
					prop = obj->base.prop;
					netdbgEvent("laptop",
							"\"action\":\"spawn_rx_deploy\",\"syncid\":%u,\"clid\":%u"
							",\"ammo\":%u,\"firecount\":%u",
							(unsigned)syncid, (unsigned)clid,
							(unsigned)ammocount, (unsigned)firecount);
				}
			}
		}
	}

	if (prop) {
		const s32 alreadyactive = prop->active != 0;

		prop->type = type;
		netPropSetSyncId(prop, syncid);
		prop->pos = pos;
		prop->forcetick = (msgflags & (1 << 2)) != 0;
		prop->flags = propflags;
		/* Move between rooms safely when echo-updating a live local throw. */
		propDeregisterRooms(prop);
		roomsCopy(rooms, prop->rooms);

		/*
		 * Attached children must not remain on the world active/paused list.
		 * (PC uses childnext/childprev for siblings; still delist before
		 * reparent so Pause/Activate cannot leave a world-list hole.)
		 */
		if (parent) {
			if (prop->active || (!prop->parent && (prop->next || prop->prev))) {
				propDelist(prop);
			}
			if (prop->parent && prop->parent != parent) {
				propDetach(prop);
			}
			if (prop->parent != parent) {
				propReparent(prop, parent);
			}
		} else {
			if (prop->parent) {
				propDetach(prop);
			}
			if (msgflags & (1 << 0)) {
				if (!alreadyactive) {
					propActivate(prop);
				}
				propRegisterRooms(prop);
			} else {
				propPause(prop);
			}
		}

		if (propflags & PROPFLAG_ENABLED) {
			propEnable(prop);
		} else {
			propDisable(prop);
		}
	} else {
		sysLogPrintf(LOG_WARNING, "NET: no prop allocated when spawning prop %u (%u)", syncid, type);
		g_NetApplyingPropSpawn = 0;
		return src->error;
	}

	if (msgflags & (1 << 1)) {
		const u32 flags = netbufReadU32(src);
		const u32 flags2 = netbufReadU32(src);
		const u32 flags3 = netbufReadU32(src);
		const u32 hidden = netbufReadHidden(src);
		const u8 hidden2 = netbufReadU8(src);
		const u16 extrascale = netbufReadU16(src);
		const s16 pad = netbufReadS16(src);
		f32 realrot[3][3];
		struct coord nextstep;
		struct prop *ownerprop = NULL;
		struct prop *targetprop = NULL;
		u32 projflags = 0;
		f32 unk08c = 0.f;
		s16 pickuptimer240 = 0;
		s16 droptype = 0;
		Mtxf projmtx;
		f32 unk010 = 0.f, unk014 = 0.f, unk018 = 0.f;
		s32 havepowered = 0;

		for (s32 i = 0; i < 3; ++i) {
			for (s32 j = 0; j < 3; ++j) {
				realrot[i][j] = netbufReadF32(src);
			}
		}

		if (hidden & OBJHFLAG_PROJECTILE) {
			netbufReadCoord(src, &nextstep);
			ownerprop = netbufReadPropPtr(src);
			targetprop = netbufReadPropPtr(src);
			projflags = netbufReadU32(src);
			unk08c = netbufReadF32(src);
			pickuptimer240 = netbufReadS16(src);
			droptype = netbufReadS16(src);
			netbufReadMtxf(src, &projmtx);
			if (projflags & PROJECTILEFLAG_POWERED) {
				unk010 = netbufReadF32(src);
				unk014 = netbufReadF32(src);
				unk018 = netbufReadF32(src);
				havepowered = 1;
			}
		}

		if (!prop->obj) {
			netdbgEvent("warn",
					"\"action\":\"spawn_obj_null\",\"syncid\":%u,\"type\":%u,\"objtype\":%u",
					(unsigned)syncid, (unsigned)type, (unsigned)objtype);
			netdbgFlush();
		} else {
			for (s32 i = 0; i < 3; ++i) {
				for (s32 j = 0; j < 3; ++j) {
					prop->obj->realrot[i][j] = realrot[i][j];
				}
			}
			if (hidden & OBJHFLAG_PROJECTILE) {
				func0f0685e4(prop);
				if (prop->obj->projectile) {
					prop->obj->projectile->nextsteppos = nextstep;
					prop->obj->projectile->ownerprop =
							(ownerprop && propIsInPool(ownerprop)) ? ownerprop : NULL;
					prop->obj->projectile->targetprop =
							(targetprop && propIsInPool(targetprop)) ? targetprop : NULL;
					prop->obj->projectile->flags = projflags;
					prop->obj->projectile->unk08c = unk08c;
					prop->obj->projectile->pickuptimer240 = pickuptimer240;
					prop->obj->projectile->droptype = droptype;
					prop->obj->projectile->flighttime240 = 0;
					prop->obj->projectile->mtx = projmtx;
					if (havepowered) {
						prop->obj->projectile->unk010 = unk010;
						prop->obj->projectile->unk014 = unk014;
						prop->obj->projectile->unk018 = unk018;
						prop->obj->projectile->powerlimit240 = TICKS(1200);
						prop->obj->projectile->smoketimer240 = TICKS(24);
					}
					if ((type == PROPTYPE_WEAPON && (projflags & PROJECTILEFLAG_00000002))
							|| objtype == OBJTYPE_AUTOGUN) {
						/* Local throw already played SFX; echo SPAWN would double it. */
						const s32 localthrow = (objtype == OBJTYPE_AUTOGUN
								&& g_NetMode == NETMODE_CLIENT
								&& g_Vars.currentplayer
								&& ownerprop == g_Vars.currentplayer->prop);
						if (!localthrow) {
							psCreate(NULL, prop, SFX_THROW, -1, -1, 0, 0, PSTYPE_NONE,
									NULL, -1, NULL, -1, -1, -1, -1);
						}
					}
				}
			}
			prop->obj->flags = flags;
			prop->obj->flags2 = flags2;
			prop->obj->flags3 = flags3;
			prop->obj->hidden = hidden;
			prop->obj->hidden2 = hidden2;
			prop->obj->extrascale = extrascale;
			prop->obj->pad = pad;
			if (prop->obj->model) {
				modelSetScale(prop->obj->model, prop->obj->model->scale * ((f32)extrascale / 256.f));
			}
		}
	}

	// just in case
	prop->pos = pos;

	g_NetApplyingPropSpawn = 0;
	return src->error;
}

u32 netmsgSvcPropDamageWrite(struct netbuf *dst, struct prop *prop, f32 damage, struct coord *pos, s32 weaponnum, s32 playernum)
{
	if (!prop || !prop->obj) {
		return dst->error;
	}
	netbufWriteU8(dst, SVC_PROP_DAMAGE);
	netdbgTx(1, SVC_PROP_DAMAGE);
	netbufWritePropPtr(dst, prop);
	netbufWriteCoord(dst, pos);
	netbufWriteF32(dst, prop->obj->damage);
	netbufWriteF32(dst, damage);
	netbufWriteS8(dst, weaponnum);
	netbufWriteS8(dst, playernum);
	netbufWriteU32(dst, prop->obj->hidden & ~(OBJHFLAG_PROJECTILE | OBJHFLAG_EMBEDDED));
	return dst->error;
}

u32 netmsgSvcPropDamageRead(struct netbuf *src, struct netclient *srccl)
{
	struct prop *prop = netbufReadPropPtr(src);
	struct coord pos; netbufReadCoord(src, &pos);
	const f32 damagepre = netbufReadF32(src);
	const f32 damage = netbufReadF32(src);
	const s8 weaponnum = netbufReadS8(src);
	const s8 playernum = netbufReadS8(src);
	const u32 hidden = netbufReadHidden(src);
	if (srccl->state < CLSTATE_GAME) {
		return src->error;
	}
	if (prop && prop->obj && prop->type != PROPTYPE_PLAYER && prop->type != PROPTYPE_CHR && !src->error) {
		prop->obj->damage = damagepre;
		prop->obj->hidden = hidden | (prop->obj->hidden & (OBJHFLAG_PROJECTILE | OBJHFLAG_EMBEDDED));
		objDamage(prop->obj, -damage, &pos, weaponnum, playernum);
	}
	return src->error;
}

u32 netmsgSvcPropPickupWrite(struct netbuf *dst, struct netclient *actcl, struct prop *prop, const s32 tickop, struct prop *holder)
{
	netbufWriteU8(dst, SVC_PROP_PICKUP);
	netdbgTx(1, SVC_PROP_PICKUP);
	netbufWriteU8(dst, actcl ? actcl->id : NET_NULL_CLIENT);
	netbufWriteS8(dst, tickop);
	netbufWritePropPtr(dst, prop);
	netbufWritePropPtr(dst, holder);
	return dst->error;
}

u32 netmsgSvcPropPickupRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 clid = netbufReadU8(src);
	const s8 tickop = netbufReadS8(src);
	struct prop *prop = netbufReadPropPtr(src);
	struct prop *holder = netbufReadPropPtr(src);

	if (src->error || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	/* Missing syncid (client never got SPAWN) — drain nothing further, no AV. */
	if (!prop || !propIsInPool(prop)) {
		netdbgEvent("warn", "\"action\":\"prop_pickup_unknown_prop\"");
		return src->error;
	}

	/* Already queued for deferred free — do not delist/free again mid-frame. */
	if (prop->obj && (prop->obj->hidden & OBJHFLAG_DELETING)) {
		return src->error;
	}

	if (clid != NET_NULL_CLIENT) {
		if (!netmsgClientIdOk(clid) || g_NetClients[clid].state < CLSTATE_GAME
				|| !g_NetClients[clid].player) {
			netdbgEvent("warn", "\"action\":\"prop_pickup_bad_clid\",\"clid\":%u", (unsigned)clid);
			return 1;
		}
		struct netclient *actcl = g_NetClients + clid;
		struct weaponobj *tokenw = prop->weapon;
		const s32 scenario_token = (tokenw
				&& (tokenw->weaponnum == WEAPON_BRIEFCASE2
						|| tokenw->weaponnum == WEAPON_DATAUPLINK));

		/*
		 * Clients: do not run propPickupByPlayer for scenario tokens — remapped
		 * playernums phantom-gave the uplink/case to the wrong peer. Give only
		 * when actcl is the local client; holder confirmation still arrives via
		 * SVC_SCENARIO.
		 */
		if (g_NetMode == NETMODE_CLIENT && scenario_token) {
			if (tickop == TICKOP_FREE || tickop == TICKOP_DISABLE) {
				netDeferPropDelete(prop);
			}
			if (actcl == g_NetLocalClient && tokenw) {
				const s32 prevplayernum = g_Vars.currentplayernum;
				const s32 weaponnum = tokenw->weaponnum;

				setCurrentPlayerNum(actcl->playernum);
				if (!invHasSingleWeaponExcAllGuns(weaponnum)) {
					invGiveSingleWeapon(weaponnum);
					currentPlayerQueuePickupWeaponHudmsg(weaponnum, false);
					weaponPlayPickupSound(weaponnum);
				}
				if (weaponnum == WEAPON_DATAUPLINK
						&& g_MpSetup.scenario == MPSCENARIO_HACKERCENTRAL
						&& g_Vars.currentplayer->prop) {
					g_ScenarioData.htm.uplink = g_Vars.currentplayer->prop;
				} else if (weaponnum == WEAPON_BRIEFCASE2
						&& g_MpSetup.scenario == MPSCENARIO_HOLDTHEBRIEFCASE
						&& g_Vars.currentplayer->prop) {
					g_ScenarioData.htb.token = g_Vars.currentplayer->prop;
					g_Vars.currentplayerstats->tokenheldtime = 0;
				}
				setCurrentPlayerNum(prevplayernum);
			}
		} else {
			const s32 prevplayernum = g_Vars.currentplayernum;
			setCurrentPlayerNum(actcl->playernum);

			propPickupByPlayer(prop, true);
			if (tickop == TICKOP_FREE || tickop == TICKOP_DISABLE) {
				netDeferPropDelete(prop);
			} else if (tickop != TICKOP_NONE) {
				propExecuteTickOperation(prop, tickop);
			}

			setCurrentPlayerNum(prevplayernum);
		}
	} else if (holder && holder->chr && holder->chr->aibot) {
		// Bot picked up a world prop (ammo/weapon/shield) or scenario token
		struct weaponobj *weapon = prop->weapon;
		if (weapon && weapon->weaponnum == WEAPON_BRIEFCASE2) {
			scenarioPickUpBriefcase(holder->chr, prop);
		} else if (weapon && weapon->weaponnum == WEAPON_DATAUPLINK) {
			scenarioPickUpUplink(holder->chr, prop);
		} else if (tickop == TICKOP_FREE || tickop == TICKOP_DISABLE) {
			netDeferPropDelete(prop);
		} else if (tickop != TICKOP_NONE) {
			propExecuteTickOperation(prop, tickop);
		}
	} else if (tickop == TICKOP_FREE || tickop == TICKOP_DISABLE) {
		/* Host floor cull — never propFree from net RX mid-packet. */
		netDeferPropDelete(prop);
	} else if (tickop != TICKOP_NONE) {
		propExecuteTickOperation(prop, tickop);
	}

	return src->error;
}

u32 netmsgSvcPropUseWrite(struct netbuf *dst, struct prop *prop, struct netclient *usercl, const s32 tickop)
{
	netbufWriteU8(dst, SVC_PROP_USE);
	netdbgTx(1, SVC_PROP_USE);
	netbufWritePropPtr(dst, prop);
	netbufWriteU8(dst, usercl->id);
	netbufWriteS8(dst, tickop);
	return dst->error;
}

u32 netmsgSvcPropUseRead(struct netbuf *src, struct netclient *srccl)
{
	struct prop *prop = netbufReadPropPtr(src);
	const u8 clid = netbufReadU8(src);
	const s8 tickop = netbufReadS8(src);

	if (!prop || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	if (!netmsgClientIdOk(clid) || g_NetClients[clid].state < CLSTATE_GAME
			|| !g_NetClients[clid].player) {
		netdbgEvent("warn", "\"action\":\"prop_use_bad_clid\",\"clid\":%u", (unsigned)clid);
		return 1;
	}

	struct netclient *actcl = &g_NetClients[clid];

	const s32 prevplayernum = g_Vars.currentplayernum;
	setCurrentPlayerNum(actcl->playernum);

	s32 ownop;
	switch (prop->type) {
		case PROPTYPE_OBJ:
		case PROPTYPE_WEAPON:
			if (!prop->obj || prop->obj->type != OBJTYPE_LIFT) {
				ownop = propobjInteract(prop);
			} else {
				ownop = TICKOP_NONE;
			}
			break;
		default:
			// NOTE: doors and lifts are handled with SVC_PROP_DOOR/_LIFT
			// TODO: eventually remove this message completely
			ownop = TICKOP_NONE;
			break;
	}

	if (tickop == TICKOP_FREE || tickop == TICKOP_DISABLE) {
		netDeferPropDelete(prop);
	} else if (tickop != TICKOP_NONE) {
		propExecuteTickOperation(prop, tickop);
	} else if (ownop == TICKOP_FREE || ownop == TICKOP_DISABLE) {
		netDeferPropDelete(prop);
	} else if (ownop != TICKOP_NONE) {
		propExecuteTickOperation(prop, ownop);
	}

	setCurrentPlayerNum(prevplayernum);

	return src->error;
}

u32 netmsgSvcPropDoorWrite(struct netbuf *dst, struct prop *prop, struct netclient *usercl)
{
	if (prop->type != PROPTYPE_DOOR || !prop->door) {
		return dst->error;
	}

	struct doorobj *door = prop->door;

	netbufWriteU8(dst, SVC_PROP_DOOR);
	netdbgTx(1, SVC_PROP_DOOR);
	netbufWritePropPtr(dst, prop);
	netbufWriteU8(dst, usercl ? usercl->id : NET_NULL_CLIENT);
	netbufWriteS8(dst, door->mode);
	netbufWriteU32(dst, door->base.flags);
	netbufWriteU32(dst, door->base.hidden);

	return dst->error;
}

u32 netmsgSvcPropDoorRead(struct netbuf *src, struct netclient *srccl)
{
	struct prop *prop = netbufReadPropPtr(src);
	const u8 clid = netbufReadU8(src);
	const s8 doormode = netbufReadS8(src);
	const u32 flags = netbufReadU32(src);
	const u32 hidden = netbufReadHidden(src);

	struct netclient *actcl = NULL;

	if (clid != NET_NULL_CLIENT) {
		if (!netmsgClientIdOk(clid)) {
			netdbgEvent("warn", "\"action\":\"prop_door_bad_clid\",\"clid\":%u", (unsigned)clid);
			return 1;
		}
		actcl = &g_NetClients[clid];
	}

	if (!prop || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	if (!prop->door || prop->type != PROPTYPE_DOOR) {
		sysLogPrintf(LOG_WARNING, "NET: SVC_PROP_DOOR: prop %u should be a door, but isn't", prop->syncid);
		return src->error;
	}

	const s32 prevplayernum = g_Vars.currentplayernum;
	if (actcl) {
		setCurrentPlayerNum(actcl->playernum);
	}

	doorSetMode(prop->door, doormode);
	prop->door->base.hidden = hidden;
	prop->door->base.flags = flags;

	if (actcl) {
		setCurrentPlayerNum(prevplayernum);
	}

	return src->error;
}

u32 netmsgSvcPropLiftWrite(struct netbuf *dst, struct prop *prop)
{
	if (prop->type != PROPTYPE_OBJ || !prop->obj || prop->obj->type != OBJTYPE_LIFT) {
		return dst->error;
	}

	struct liftobj *lift = (struct liftobj *)prop->obj;

	netbufWriteU8(dst, SVC_PROP_LIFT);
	netdbgTx(1, SVC_PROP_LIFT);
	netbufWritePropPtr(dst, prop);
	netbufWriteS8(dst, lift->levelcur);
	netbufWriteS8(dst, lift->levelaim);
	netbufWriteF32(dst, lift->accel);
	netbufWriteF32(dst, lift->speed);
	netbufWriteF32(dst, lift->dist);
	netbufWriteU32(dst, lift->base.flags);
	netbufWriteCoord(dst, &prop->pos);
	netbufWriteRooms(dst, prop->rooms, ARRAYCOUNT(prop->rooms));

	return dst->error;
}

u32 netmsgSvcPropLiftRead(struct netbuf *src, struct netclient *srccl)
{
	struct prop *prop = netbufReadPropPtr(src);
	const s8 levelcur = netbufReadS8(src);
	const s8 levelaim = netbufReadS8(src);
	const f32 accel = netbufReadF32(src);
	const f32 speed = netbufReadF32(src);
	const f32 dist = netbufReadF32(src);
	const u32 flags = netbufReadU32(src);
	struct coord pos; netbufReadCoord(src, &pos);
	RoomNum rooms[8] = { -1 };
	netbufReadRooms(src, rooms, ARRAYCOUNT(rooms));

	if (!prop || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	if (!prop->obj || prop->type != PROPTYPE_OBJ || prop->obj->type != OBJTYPE_LIFT) {
		sysLogPrintf(LOG_WARNING, "NET: SVC_PROP_LIFT: prop %u should be a lift, but isn't", prop->syncid);
		return src->error;
	}

	struct liftobj *lift = (struct liftobj *)prop->obj;
	lift->levelcur = levelcur;
	lift->levelaim = levelaim;
	lift->speed = speed;
	lift->dist = dist;
	lift->accel = accel;
	lift->base.flags = flags;

	prop->pos = pos;

	if (!propRoomsEqual(rooms, prop->rooms)) {
		if (prop->active) {
			propDeregisterRooms(prop);
		}
		roomsCopy(rooms, prop->rooms);
		if (prop->active) {
			propRegisterRooms(prop);
		}
	}

	return src->error;
}

static struct prop *netmsgFindPropBySyncId(u32 syncid)
{
	return netPropFindBySyncId(syncid);
}

u32 netmsgSvcWallhitWrite(struct netbuf *dst, struct coord *relpos, struct coord *arg1, struct coord *arg2,
		s16 texnum, RoomNum room, struct prop *objprop, struct prop *chrprop,
		s8 mtxindex, s8 arg10, struct chrdata *chr,
		f32 width, f32 height, u8 minalpha, u8 maxalpha,
		s32 rotdeg, u32 timermax, u32 timerspeed, bool xlu,
		const u8 colours[4][4])
{
	s8 playernum = -1;

	if (chr && chr->prop && chr->prop->type == PROPTYPE_PLAYER) {
		playernum = playermgrGetPlayerNumByProp(chr->prop);
	}

	netbufWriteU8(dst, SVC_WALLHIT);
	netbufWriteCoord(dst, relpos);
	netbufWriteCoord(dst, arg1);
	netbufWriteCoord(dst, arg2);
	netbufWriteS16(dst, texnum);
	netbufWriteS16(dst, room);
	netbufWritePropPtr(dst, objprop);
	netbufWritePropPtr(dst, chrprop);
	netbufWriteS8(dst, mtxindex);
	netbufWriteS8(dst, arg10);
	netbufWriteS8(dst, playernum);
	netbufWriteF32(dst, width);
	netbufWriteF32(dst, height);
	netbufWriteU8(dst, minalpha);
	netbufWriteU8(dst, maxalpha);
	netbufWriteS16(dst, (s16)rotdeg);
	netbufWriteU16(dst, (u16)timermax);
	netbufWriteU16(dst, (u16)timerspeed);
	netbufWriteU8(dst, xlu ? 1 : 0);
	netbufWriteData(dst, colours, 4 * 4);
	netdbgEvent("tx",
			"\"msgid\":%u,\"name\":\"SVC_WALLHIT\",\"room\":%d,\"objsync\":%u,\"chrsync\":%u",
			(unsigned)SVC_WALLHIT, (int)room,
			(unsigned)(objprop ? objprop->syncid : 0),
			(unsigned)(chrprop ? chrprop->syncid : 0));
	return dst->error;
}

u32 netmsgSvcWallhitRead(struct netbuf *src, struct netclient *srccl)
{
	struct coord relpos;
	struct coord arg1;
	struct coord arg2;
	netbufReadCoord(src, &relpos);
	netbufReadCoord(src, &arg1);
	netbufReadCoord(src, &arg2);
	const s16 texnum = netbufReadS16(src);
	const RoomNum room = netbufReadS16(src);
	const u32 objsyncid = netbufReadU32(src);

	netdbgEvent("fields",
			"\"msgid\":%u,\"name\":\"SVC_WALLHIT\",\"room\":%d,\"objsync\":%u",
			(unsigned)SVC_WALLHIT, (int)room, (unsigned)objsyncid);
	const u32 chrsyncid = netbufReadU32(src);
	const s8 mtxindex = netbufReadS8(src);
	const s8 arg10 = netbufReadS8(src);
	const s8 playernum = netbufReadS8(src);
	const f32 width = netbufReadF32(src);
	const f32 height = netbufReadF32(src);
	const u8 minalpha = netbufReadU8(src);
	const u8 maxalpha = netbufReadU8(src);
	const s16 rotdeg = netbufReadS16(src);
	const u16 timermax = netbufReadU16(src);
	const u16 timerspeed = netbufReadU16(src);
	const u8 xlu = netbufReadU8(src);
	netbufReadData(src, g_NetWallhitColours, sizeof(g_NetWallhitColours));

	if (src->error || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	/* BG wallhits index g_Rooms[room] / g_WallhitCountsPerRoom[room] unchecked. */
	if (room < 0 || room >= g_Vars.roomcount) {
		netdbgEvent("warn",
				"\"action\":\"wallhit_bad_room\",\"room\":%d,\"objsync\":%u",
				(int)room, (unsigned)objsyncid);
		netdbgFlush();
		return src->error;
	}

	struct prop *objprop = NULL;
	if (objsyncid) {
		objprop = netmsgFindPropBySyncId(objsyncid);
		if (!objprop) {
			// Prop not replicated yet — still place a room splat so shots aren't silent.
			objprop = NULL;
		} else if (objprop->type == PROPTYPE_OBJ || objprop->type == PROPTYPE_DOOR
				|| objprop->type == PROPTYPE_WEAPON) {
			if (!objprop->obj) {
				objprop = NULL;
			}
			// Do not gfxAllocate / objInitMatrices here — netStartFrame runs before
			// the camera worldtoscreen mtx is valid, and gfxAllocate is frame-scratch.
			// wallhitCreateWith20Args already falls back to obj->realrot when
			// model->matrices is NULL (see wallhit.c).
		}
		// PLAYER/CHR blood marks attach to the prop; wallhitCreate handles chr models.
	}

	struct prop *chrprop = chrsyncid ? netmsgFindPropBySyncId(chrsyncid) : NULL;

	struct chrdata *chr = NULL;
	if (playernum >= 0 && playernum < PLAYERCOUNT() && g_Vars.players[playernum] && g_Vars.players[playernum]->prop) {
		chr = g_Vars.players[playernum]->prop->chr;
	} else if (chrprop) {
		chr = chrprop->chr;
	}

	g_NetApplyingWallhit = 1;
	g_NetWallhitColoursValid = 1;
	wallhitCreateWith20Args(&relpos, &arg1, &arg2, 0, 0, texnum, room, objprop,
			chrprop, mtxindex, arg10, chr,
			width, height, minalpha, maxalpha,
			rotdeg, timermax, timerspeed, xlu);
	g_NetWallhitColoursValid = 0;
	g_NetApplyingWallhit = 0;
	netdbgEvent("client",
			"\"action\":\"wallhit_applied\",\"room\":%d,\"objsync\":%u,\"tex\":%d",
			(int)room, (unsigned)objsyncid, (int)texnum);
	netdbgFlush();

	// Host already played impact SFX when the shot landed; clients only got the splat.
	if (g_NetMode == NETMODE_CLIENT) {
		struct gset gset;
		RoomNum rooms[2];

		memset(&gset, 0, sizeof(gset));
		if (playernum >= 0 && playernum < PLAYERCOUNT()
				&& g_Vars.players[playernum] && g_Vars.players[playernum]->hands[HAND_RIGHT].inuse) {
			gset = g_Vars.players[playernum]->hands[HAND_RIGHT].gset;
		} else if (chr) {
			struct prop *wprop = chrGetHeldProp(chr, HAND_RIGHT);
			if (wprop && wprop->weapon) {
				gset = wprop->weapon->gset;
			}
		}

		rooms[0] = room;
		rooms[1] = -1;
		bgunPlayBgHitSound(&gset, &relpos, texnum, rooms);
	}

	return src->error;
}

u32 netmsgSvcPropGlassWrite(struct netbuf *dst, struct prop *prop, u8 kind)
{
	if (!prop) {
		return dst->error;
	}
	netbufWriteU8(dst, SVC_PROP_GLASS);
	netdbgTx(1, SVC_PROP_GLASS);
	netbufWriteU8(dst, kind);
	netbufWritePropPtr(dst, prop);
	return dst->error;
}

u32 netmsgSvcPropGlassRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 kind = netbufReadU8(src);
	struct prop *prop = netbufReadPropPtr(src);

	if (src->error || srccl->state < CLSTATE_GAME || !prop || !prop->obj) {
		return src->error;
	}

	if (kind == NETGLASS_DOORDESTROY) {
		if (prop->type != PROPTYPE_DOOR || !prop->door
				|| prop->obj->type != OBJTYPE_DOOR) {
			netdbgEvent("warn", "\"action\":\"glass_bad_door\",\"syncid\":%u",
					(unsigned)prop->syncid);
			return 1;
		}
	} else if (prop->obj->type != OBJTYPE_GLASS
			&& prop->obj->type != OBJTYPE_TINTEDGLASS) {
		netdbgEvent("warn", "\"action\":\"glass_bad_type\",\"syncid\":%u,\"objtype\":%u",
				(unsigned)prop->syncid, (unsigned)prop->obj->type);
		return 1;
	}

	g_NetApplyingGlass = 1;
	if (kind == NETGLASS_DOORDESTROY) {
		doorDestroyGlass((struct doorobj *)prop->obj);
	} else {
		glassDestroy(prop->obj);
	}
	g_NetApplyingGlass = 0;

	return src->error;
}

u32 netmsgSvcChrDamageWrite(struct netbuf *dst, struct chrdata *chr, f32 damage, struct coord *vector, struct gset *gset,
		struct prop *aprop, s32 hitpart, bool damageshield, struct prop *prop2, s32 side, s16 *arg11, bool explosion, struct coord *explosionpos)
{
	const u8 flags = damageshield | (explosion << 1) | ((gset != NULL) << 2) |
		((aprop != NULL) << 3) | ((prop2 != NULL) << 4) | ((arg11 != NULL) << 5) | ((explosionpos != NULL) << 6);

	netbufWriteU8(dst, SVC_CHR_DAMAGE);
	netbufWriteU8(dst, flags);
	netbufWritePropPtr(dst, chr->prop);
	netbufWriteF32(dst, damage);
	netbufWriteCoord(dst, vector);
	netbufWriteS16(dst, hitpart);
	netbufWriteS16(dst, side);
	if (gset) {
		netbufWriteGset(dst, gset);
	}
	if (aprop) {
		netbufWritePropPtr(dst, aprop);
	}
	if (prop2) {
		netbufWritePropPtr(dst, prop2);
	}
	if (arg11) {
		netbufWriteS16(dst, arg11[0]);
		netbufWriteS16(dst, arg11[1]);
		netbufWriteS16(dst, arg11[2]);
	}
	if (explosionpos) {
		netbufWriteCoord(dst, explosionpos);
	}

	netdbgEvent("tx",
			"\"msgid\":%u,\"name\":\"SVC_CHR_DAMAGE\",\"syncid\":%u,\"damage\":%.3f"
			",\"hitpart\":%d,\"weaponnum\":%d",
			(unsigned)SVC_CHR_DAMAGE, (unsigned)(chr->prop ? chr->prop->syncid : 0),
			damage, (int)hitpart, gset ? (int)gset->weaponnum : -1);

	return dst->error;
}

u32 netmsgSvcChrDamageRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 flags = netbufReadU8(src);
	struct prop *chrprop = netbufReadPropPtr(src);
	const f32 damage = netbufReadF32(src);
	struct coord vector; netbufReadCoord(src, &vector);
	const s16 hitpart = netbufReadS16(src);
	const s16 side = netbufReadS16(src);

	struct gset gsetvalue;
	struct gset *gset = NULL;
	if (flags & (1 << 2)) {
		netbufReadGset(src, &gsetvalue);
		gset = &gsetvalue;
	}

	struct prop *aprop = (flags & (1 << 3)) ? netbufReadPropPtr(src) : NULL;
	struct prop *prop2 = (flags & (1 << 4)) ? netbufReadPropPtr(src) : NULL;

	s16 arg11[3], *arg11ptr = NULL;
	if (flags & (1 << 5)) {
		arg11[0] = netbufReadS16(src);
		arg11[1] = netbufReadS16(src);
		arg11[2] = netbufReadS16(src);
		arg11ptr = arg11;
	}

	struct coord explosionpos, *explosionposptr = NULL;
	if (flags & (1 << 6)) {
		netbufReadCoord(src, &explosionpos);
		explosionposptr = &explosionpos;
	}

	if (src->error || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	if (!chrprop || !chrprop->chr) {
		return src->error;
	}

	const bool damageshield = (flags & (1 << 0)) != 0;
	const bool explosion = (flags & (1 << 1)) != 0;

	netdbgEvent("fields",
			"\"msgid\":%u,\"name\":\"SVC_CHR_DAMAGE\",\"syncid\":%u,\"damage\":%.3f"
			",\"hitpart\":%d,\"weaponnum\":%d",
			(unsigned)SVC_CHR_DAMAGE, (unsigned)(chrprop ? chrprop->syncid : 0),
			damage, (int)hitpart, gset ? (int)gset->weaponnum : -1);

	const s32 prevplayernum = g_Vars.currentplayernum;
	if (chrprop->type == PROPTYPE_PLAYER) {
		setCurrentPlayerNum(playermgrGetPlayerNumByProp(chrprop));
	}

	/* Clients must not re-run host kill side effects (botinvDropAll / scores). */
	g_NetApplyingChrDamage = 1;
	chrDamage(chrprop->chr, damage, &vector, gset, aprop, hitpart, damageshield, prop2, NULL, NULL, side, arg11ptr, explosion, explosionposptr);
	g_NetApplyingChrDamage = 0;

	if (chrprop->type == PROPTYPE_PLAYER) {
		setCurrentPlayerNum(prevplayernum);
	}

	return src->error;
}

u32 netmsgSvcChrDisarmWrite(struct netbuf *dst, struct chrdata *chr, struct prop *aprop, u8 weaponnum, f32 wpndamage, struct coord *wpnpos)
{
	netbufWriteU8(dst, SVC_CHR_DISARM);
	netdbgTx(1, SVC_CHR_DISARM);
	netbufWritePropPtr(dst, chr->prop);
	netbufWritePropPtr(dst, aprop);
	netbufWriteU8(dst, weaponnum);
	netbufWriteF32(dst, wpndamage);
	if (wpndamage > 0.f && wpnpos) {
		netbufWriteCoord(dst, wpnpos);
	}
	return dst->error;
}

u32 netmsgSvcChrDisarmRead(struct netbuf *src, struct netclient *srccl)
{
	struct prop *chrprop = netbufReadPropPtr(src);
	struct prop *aprop = netbufReadPropPtr(src);
	const u8 weaponnum = netbufReadU8(src);
	const f32 weapondmg = netbufReadF32(src);
	struct coord pos = { 0.f, 0.f, 0.f };

	/* Always drain optional coord before any early-out (bot path used to leave
	 * it unread → multiplexed packet desync after explosive shots). */
	if (weapondmg > 0.f) {
		netbufReadCoord(src, &pos);
	}

	if (src->error || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	if (!chrprop || !chrprop->chr) {
		return 1;
	}

	struct chrdata *chr = chrprop->chr;

	if (chrprop->type == PROPTYPE_CHR) {
		/* Bots: host already SPAWNs drops; nothing to apply. */
		return src->error;
	}

	if (weapondmg > 0.f) {
		// someone shot a grenade the chr is holding, explode that shit
		struct weaponobj *weapon = NULL;
		if (chr->weapons_held[0] && chr->weapons_held[0]->weapon) {
			weapon = chr->weapons_held[0]->weapon;
		} else if (chr->weapons_held[1] && chr->weapons_held[1]->weapon) {
			weapon = chr->weapons_held[1]->weapon;
		} else {
			sysLogPrintf(LOG_WARNING, "NET: trying to explode chr %u's gun, but there's no gun", chrprop->syncid);
			return src->error;
		}
		objSetDropped(chrprop, DROPTYPE_DEFAULT);
		chr->hidden |= CHRHFLAG_DROPPINGITEM;
		objDamage(&weapon->base, -weapondmg, &pos, weaponnum, g_Vars.currentplayernum);
		return src->error;
	}

	const s32 prevplayernum = g_Vars.currentplayernum;
	const s32 victimnum = playermgrGetPlayerNumByProp(chrprop);
	if (victimnum < 0 || victimnum >= PLAYERCOUNT() || !g_Vars.players[victimnum]) {
		return src->error;
	}
	setCurrentPlayerNum(victimnum);

	struct player *player = g_Vars.currentplayer;
	if (!player) {
		setCurrentPlayerNum(prevplayernum);
		return src->error;
	}

	if (weaponHasFlag(weaponnum, WEAPONFLAG_UNDROPPABLE) || weaponnum > WEAPON_RCP45 || weaponnum <= WEAPON_UNARMED) {
		setCurrentPlayerNum(prevplayernum);
		return src->error;
	}

	if (weaponnum == WEAPON_RCP120) {
		player->devicesactive &= ~DEVICE_CLOAKRCP120;
	}

	if (weaponnum == WEAPON_CLOAKINGDEVICE) {
		player->devicesactive &= ~DEVICE_CLOAKDEVICE;
	}

	weaponDeleteFromChr(chr, HAND_RIGHT);
	weaponDeleteFromChr(chr, HAND_LEFT);

	invRemoveItemByNum(weaponnum);

	player->hands[1].state = HANDSTATE_IDLE;
	player->hands[1].ejectstate = EJECTSTATE_INIT;
	player->hands[1].ejecttype = EJECTTYPE_GUN;
	player->hands[0].ejectstate = EJECTSTATE_INIT;
	player->hands[0].ejecttype = EJECTTYPE_GUN;
	player->hands[0].state = HANDSTATE_IDLE;

	if (player->visionmode == VISIONMODE_SLAYERROCKET) {
		struct weaponobj *rocket = g_Vars.currentplayer->slayerrocket;
		if (rocket && rocket->base.prop) {
			rocket->timer240 = 0;
		}
		player->visionmode = VISIONMODE_NORMAL;
	}

	bgunEquipWeapon2(HAND_RIGHT, WEAPON_UNARMED);
	bgunEquipWeapon2(HAND_LEFT, WEAPON_NONE);

	setCurrentPlayerNum(prevplayernum);

	return src->error;
}

u32 netmsgSvcChrBeamWrite(struct netbuf *dst, struct prop *chrprop, u8 handnum, s32 weaponnum, struct coord *from, struct coord *to)
{
	if (!chrprop || !from || !to) {
		return dst->error;
	}

	netbufWriteU8(dst, SVC_CHR_BEAM);
	netbufWritePropPtr(dst, chrprop);
	netbufWriteU8(dst, handnum);
	netbufWriteU8(dst, (u8)weaponnum);
	netbufWriteCoord(dst, from);
	netbufWriteCoord(dst, to);
	netdbgEvent("tx",
			"\"msgid\":%u,\"name\":\"SVC_CHR_BEAM\",\"syncid\":%u,\"weaponnum\":%d",
			(unsigned)SVC_CHR_BEAM, (unsigned)chrprop->syncid, weaponnum);
	return dst->error;
}

u32 netmsgSvcChrBeamRead(struct netbuf *src, struct netclient *srccl)
{
	const u32 syncid = netbufReadU32(src);
	const u8 handnum = netbufReadU8(src);
	const u8 weaponnum = netbufReadU8(src);
	struct coord from;
	struct coord to;

	netbufReadCoord(src, &from);
	netbufReadCoord(src, &to);

	netdbgEvent("fields",
			"\"msgid\":%u,\"name\":\"SVC_CHR_BEAM\",\"syncid\":%u,\"weaponnum\":%u",
			(unsigned)SVC_CHR_BEAM, (unsigned)syncid, (unsigned)weaponnum);

	if (src->error || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	struct prop *chrprop = syncid ? netmsgFindPropBySyncId(syncid) : NULL;
	struct chrdata *chr = (chrprop && propIsInPool(chrprop) && chrprop->chr)
			? chrprop->chr : NULL;

	if (!chr || !chr->prop || (handnum != HAND_LEFT && handnum != HAND_RIGHT)) {
		return src->error;
	}
	if (chrIsDead(chr) || (chr->chrflags & CHRCFLAG_HIDDEN)) {
		return src->error;
	}
	if (weaponnum == WEAPON_NONE || weaponnum > WEAPON_RCP45) {
		return src->error;
	}

	if (chr->fireslots[handnum] < 0) {
		chr->fireslots[handnum] = bgunAllocateFireslot();
	}

	if (chr->fireslots[handnum] >= 0) {
		beamCreate(&g_Fireslots[chr->fireslots[handnum]].beam, weaponnum, &from, &to);
	}

	return src->error;
}

static void netmsgWriteTokenRef(struct netbuf *dst, struct prop *token)
{
	u32 syncid = 0;
	s16 holder = -1;

	if (token) {
		// Always prefer prop syncid. Human playernums are remapped on clients
		// (local is always slot 0), so mpPlayerGetIndex is NOT stable across peers.
		// Player prop syncids are swapped in netSyncIdsAllocate to match.
		syncid = token->syncid;

		if ((token->type == PROPTYPE_PLAYER || token->type == PROPTYPE_CHR) && token->chr) {
			// Bots keep the same g_MpAllChrPtrs order on every peer; keep as fallback
			// when syncid is missing (should not happen after allocate).
			if (token->chr->aibot) {
				holder = (s16)mpPlayerGetIndex(token->chr);
			} else if (token->type == PROPTYPE_PLAYER) {
				// Encode net client id as negative-biased fallback: -(clid+2)
				// so -1 remains "nobody". Rarely needed if syncid is set.
				struct player *pl = g_Vars.players[playermgrGetPlayerNumByProp(token)];
				if (pl && pl->client) {
					holder = (s16)(-(s16)(pl->client->id + 2));
				}
			}
		}
	}

	netbufWriteU32(dst, syncid);
	netbufWriteS16(dst, holder);
}

static struct prop *netmsgReadTokenRef(struct netbuf *src)
{
	const u32 syncid = netbufReadU32(src);
	const s16 holder = netbufReadS16(src);

	if (src->error) {
		return NULL;
	}

	// Syncid is authoritative across host/client (player props remapped).
	if (syncid) {
		struct prop *prop = netmsgFindPropBySyncId(syncid);
		if (prop) {
			return prop;
		}
	}

	if (holder <= -2) {
		// Fallback: net client id encoded as -(clid+2)
		const u8 clid = (u8)(-(holder + 2));
		if (clid < NET_MAX_CLIENTS && g_NetClients[clid].state >= CLSTATE_GAME
				&& g_NetClients[clid].player) {
			return g_NetClients[clid].player->prop;
		}
	} else if (holder >= 0 && holder < g_MpNumChrs && g_MpAllChrPtrs[holder]) {
		// Bot (or legacy) index — bots share order on all peers
		return g_MpAllChrPtrs[holder]->prop;
	}

	return NULL;
}

u32 netmsgSvcPropRegenWrite(struct netbuf *dst, struct prop *prop)
{
	if (!prop || !prop->obj || !prop->syncid) {
		return dst->error;
	}

	netbufWriteU8(dst, SVC_PROP_REGEN);
	netdbgTx(1, SVC_PROP_REGEN);
	netbufWritePropPtr(dst, prop);
	netbufWriteCoord(dst, &prop->pos);
	netbufWriteRooms(dst, prop->rooms, ARRAYCOUNT(prop->rooms));
	return dst->error;
}

u32 netmsgSvcPropRegenRead(struct netbuf *src, struct netclient *srccl)
{
	struct prop *prop = netbufReadPropPtr(src);
	struct coord pos;
	RoomNum rooms[8] = { -1 };

	netbufReadCoord(src, &pos);
	netbufReadRooms(src, rooms, ARRAYCOUNT(rooms));

	if (src->error || srccl->state < CLSTATE_GAME || g_NetMode != NETMODE_CLIENT) {
		return src->error;
	}

	if (!prop || !prop->obj) {
		return src->error;
	}

	prop->pos = pos;
	prop->timetoregen = 0;
	prop->obj->hidden &= ~OBJHFLAG_GONE;
	prop->obj->hidden &= ~OBJHFLAG_DELETING;
	prop->obj->damage = 0;
	prop->obj->hidden2 &= ~OBJH2FLAG_DESTROYED;

	if (prop->obj->type == OBJTYPE_SHIELD) {
		struct shieldobj *shield = (struct shieldobj *)prop->obj;
		shield->amount = shield->initialamount;
	}

	if ((prop->obj->flags & OBJFLAG_INSIDEANOTHEROBJ) == 0) {
		propEnable(prop);
		setup0f0923d4(prop->obj);
		if (prop->active) {
			propDeregisterRooms(prop);
		}
		roomsCopy(rooms, prop->rooms);
		propRegisterRooms(prop);
		if (!prop->active) {
			propActivate(prop);
		}
	} else {
		roomsCopy(rooms, prop->rooms);
	}

	psCreate(NULL, prop, SFX_REGEN, -1,
			-1, 0, 0, PSTYPE_NONE, 0, -1, 0, -1, -1, -1, -1);

	return src->error;
}

u32 netmsgSvcScenarioWrite(struct netbuf *dst)
{
	const u8 scenario = (u8)g_MpSetup.scenario;

	netbufWriteU8(dst, SVC_SCENARIO);
	netbufWriteU8(dst, scenario);

	switch (scenario) {
	case MPSCENARIO_HOLDTHEBRIEFCASE: {
		struct scenariodata_htb *htb = &g_ScenarioData.htb;
		s32 holdtime = 0;
		s32 i;
		bool foundholder = false;
		netmsgWriteTokenRef(dst, htb->token);
		netbufWriteCoord(dst, &htb->pos);
		if (htb->token) {
			for (i = 0; i < PLAYERCOUNT(); i++) {
				if (g_Vars.players[i] && g_Vars.players[i]->prop == htb->token) {
					holdtime = g_Vars.playerstats[i].tokenheldtime;
					foundholder = true;
					break;
				}
			}
			if (!foundholder) {
				for (i = 0; i < g_MpNumChrs; i++) {
					struct chrdata *chr = g_MpAllChrPtrs[i];
					if (chr && chr->aibot && chr->prop == htb->token) {
						holdtime = chr->aibot->htbheldtimer60;
						break;
					}
				}
			}
		}
		netbufWriteS32(dst, holdtime);
		for (i = 0; i < MAX_MPCHRS; i++) {
			netbufWriteS32(dst, g_MpAllChrConfigPtrs[i] ? g_MpAllChrConfigPtrs[i]->numpoints : 0);
		}
		break;
	}
	case MPSCENARIO_HACKERCENTRAL: {
		struct scenariodata_htm *htm = &g_ScenarioData.htm;
		s32 i;
		netmsgWriteTokenRef(dst, htm->uplink);
		netbufWriteS16(dst, htm->dlplayernum);
		netbufWriteS16(dst, htm->playernuminrange);
		netbufWriteS32(dst, htm->dlterminalnum);
		netbufWriteU8(dst, (u8)htm->numterminals);
		for (i = 0; i < htm->numterminals && i < ARRAYCOUNT(htm->terminals); i++) {
			netbufWritePropPtr(dst, htm->terminals[i].prop);
		}
		for (i = 0; i < MAX_MPCHRS; i++) {
			netbufWriteS32(dst, htm->numpoints[i]);
			netbufWriteS32(dst, htm->dltime240[i]);
		}
		break;
	}
	case MPSCENARIO_CAPTURETHECASE: {
		struct scenariodata_ctc *ctc = &g_ScenarioData.ctc;
		s32 i;
		for (i = 0; i < ARRAYCOUNT(ctc->tokens); i++) {
			netmsgWriteTokenRef(dst, ctc->tokens[i]);
			netbufWriteS16(dst, ctc->teamindexes[i]);
			netbufWriteS16(dst, ctc->baserooms[i]);
		}
		for (i = 0; i < MAX_MPCHRS; i++) {
			netbufWriteS32(dst, g_MpAllChrConfigPtrs[i] ? g_MpAllChrConfigPtrs[i]->numpoints : 0);
		}
		break;
	}
	case MPSCENARIO_POPACAP: {
		struct scenariodata_pac *pac = &g_ScenarioData.pac;
		s32 i;
		netbufWriteS32(dst, pac->victimindex);
		netbufWriteU16(dst, pac->age240);
		for (i = 0; i < MAX_MPCHRS; i++) {
			netbufWriteS16(dst, pac->victims[i]);
			netbufWriteS16(dst, pac->killcounts[i]);
			netbufWriteS16(dst, pac->survivalcounts[i]);
		}
		break;
	}
	case MPSCENARIO_KINGOFTHEHILL: {
		struct scenariodata_koh *koh = &g_ScenarioData.koh;
		netbufWriteS16(dst, koh->occupiedteam);
		netbufWriteS16(dst, koh->elapsed240);
		netbufWriteS16(dst, koh->movehill);
		netbufWriteS16(dst, koh->hillindex);
		netbufWriteS16(dst, koh->hillcount);
		netbufWriteS16(dst, koh->hillrooms[0]);
		netbufWriteS16(dst, koh->hillrooms[1]);
		netbufWriteCoord(dst, &koh->hillpos);
		netbufWriteF32(dst, koh->colourfracr);
		netbufWriteF32(dst, koh->colourfracg);
		netbufWriteF32(dst, koh->colourfracb);
		for (s32 i = 0; i < MAX_MPCHRS; i++) {
			netbufWriteS32(dst, g_MpAllChrConfigPtrs[i] ? g_MpAllChrConfigPtrs[i]->numpoints : 0);
		}
		netdbgEvent("tx",
				"\"msgid\":%u,\"name\":\"SVC_SCENARIO\",\"scenario\":%u"
				",\"occupiedteam\":%d,\"elapsed240\":%d,\"movehill\":%d"
				",\"hillindex\":%d,\"hillrooms0\":%d",
				(unsigned)SVC_SCENARIO, (unsigned)scenario,
				(int)koh->occupiedteam, (int)koh->elapsed240, (int)koh->movehill,
				(int)koh->hillindex, (int)koh->hillrooms[0]);
		return dst->error;
	}
	default:
		break;
	}

	netdbgEvent("tx", "\"msgid\":%u,\"name\":\"SVC_SCENARIO\",\"scenario\":%u",
			(unsigned)SVC_SCENARIO, (unsigned)scenario);
	return dst->error;
}

u32 netmsgSvcScenarioRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 scenario = netbufReadU8(src);

	if (src->error) {
		return src->error;
	}

	/* Drain body on reject — leftover bytes become fake msgids in the packet. */
	if (srccl->state < CLSTATE_GAME || g_NetMode != NETMODE_CLIENT
			|| scenario != (u8)g_MpSetup.scenario) {
		netmsgSkipScenarioBody(src, scenario);
		return src->error;
	}

	switch (scenario) {
	case MPSCENARIO_HOLDTHEBRIEFCASE: {
		struct scenariodata_htb *htb = &g_ScenarioData.htb;
		s32 holdtime;
		s32 i;
		htb->token = netmsgReadTokenRef(src);
		netbufReadCoord(src, &htb->pos);
		holdtime = netbufReadS32(src);

		netmsgApplyRemappedNumpoints(src);
		for (i = 0; i < g_MpNumChrs; i++) {
			struct chrdata *chr = g_MpAllChrPtrs[i];
			if (chr && chr->aibot) {
				const bool holding = (htb->token == chr->prop);
				chr->aibot->hasbriefcase = holding;
				chr->aibot->htbheldtimer60 = holding ? holdtime : 0;
				// Keep bot inventory aligned with token so death drops the
				// correct sim's case (not a phantom from an earlier holder).
				if (holding) {
					botinvGiveSingleWeapon(chr, WEAPON_BRIEFCASE2);
				} else {
					botinvRemoveItem(chr, WEAPON_BRIEFCASE2);
				}
			}
		}
		for (i = 0; i < PLAYERCOUNT(); i++) {
			if (!g_Vars.players[i]) {
				continue;
			}
			if (htb->token && g_Vars.players[i]->prop == htb->token) {
				// Authority correction for remote holders; local holder predicts
				// in scenarioTickChr and only needs soft sync if drifted hard.
				// Always snap when host holdtime is 0 (death/leave reset) so the
				// HUD cannot stick at 0:30 after a false local-hold.
				if (i != g_Vars.currentplayernum
						|| holdtime == 0
						|| ABS(g_Vars.playerstats[i].tokenheldtime - holdtime) > TICKS(240)) {
					g_Vars.playerstats[i].tokenheldtime = holdtime;
				}
				/* Host says this local human holds it — grant BRIEFCASE2. */
				{
					const s32 prev = g_Vars.currentplayernum;
					setCurrentPlayerNum(i);
					if (!invHasSingleWeaponExcAllGuns(WEAPON_BRIEFCASE2)) {
						invGiveSingleWeapon(WEAPON_BRIEFCASE2);
					}
					setCurrentPlayerNum(prev);
				}
			} else {
				if (i == g_Vars.currentplayernum) {
					g_Vars.playerstats[i].tokenheldtime = 0;
				}
				// Always strip phantom briefcase — do not use invHasBriefcase()
				// here (it is token-gated on clients and returns false when dead).
				{
					const s32 prev = g_Vars.currentplayernum;
					setCurrentPlayerNum(i);
					invRemoveItemByNum(WEAPON_BRIEFCASE2);
					if (g_Vars.currentplayer->gunctrl.weaponnum == WEAPON_BRIEFCASE2) {
						bgunEquipWeapon(WEAPON_UNARMED);
					}
					setCurrentPlayerNum(prev);
				}
			}
		}
		break;
	}
	case MPSCENARIO_HACKERCENTRAL: {
		struct scenariodata_htm *htm = &g_ScenarioData.htm;
		s32 i;
		u8 numterminals;
		s32 hostpoints[MAX_MPCHRS];
		s32 hostdl[MAX_MPCHRS];
		s16 dlplayernum;
		s16 playernuminrange;

		htm->uplink = netmsgReadTokenRef(src);
		dlplayernum = netbufReadS16(src);
		playernuminrange = netbufReadS16(src);
		htm->dlterminalnum = netbufReadS32(src);
		numterminals = netbufReadU8(src);
		for (i = 0; i < numterminals && i < ARRAYCOUNT(htm->terminals); i++) {
			htm->terminals[i].prop = netbufReadPropPtr(src);
		}
		htm->numterminals = numterminals;
		for (i = 0; i < MAX_MPCHRS; i++) {
			s32 dltime;

			hostpoints[i] = netbufReadS32(src);
			dltime = netbufReadS32(src);
			/* Cap — runaway values stall htmRenderHud's progress-bar loop. */
			if (dltime < 0) {
				dltime = 0;
			} else if (dltime > TICKS(4800) + TICKS(240)) {
				dltime = TICKS(4800) + TICKS(240);
			}
			hostdl[i] = dltime;
		}
		/* Remap human slots like PAC — host playernum ≠ client local index. */
		for (i = 0; i < MAX_MPCHRS; i++) {
			s32 hosti = i;

			if (i < PLAYERCOUNT()) {
				hosti = netMpChrIndexForPlayernum(i);
				if (hosti < 0 || hosti >= MAX_MPCHRS) {
					hosti = i;
				}
			}
			htm->numpoints[i] = hostpoints[hosti];
			htm->dltime240[i] = hostdl[hosti];
		}
		if (dlplayernum >= 0 && dlplayernum < PLAYERCOUNT()) {
			dlplayernum = (s16)netPlayernumForMpChrIndex(dlplayernum);
		}
		if (playernuminrange >= 0 && playernuminrange < PLAYERCOUNT()) {
			playernuminrange = (s16)netPlayernumForMpChrIndex(playernuminrange);
		}
		htm->dlplayernum = dlplayernum;
		htm->playernuminrange = playernuminrange;
		if (htm->dlterminalnum < -1 || htm->dlterminalnum >= ARRAYCOUNT(htm->terminals)) {
			htm->dlterminalnum = -1;
			htm->dlplayernum = -1;
		}
		for (i = 0; i < ARRAYCOUNT(htm->terminals); i++) {
			if (htm->terminals[i].prop && !propIsInPool(htm->terminals[i].prop)) {
				htm->terminals[i].prop = NULL;
			}
		}
		// Mirror uplink ownership onto bots for radar / AI presentation
		for (i = 0; i < g_MpNumChrs; i++) {
			struct chrdata *chr = g_MpAllChrPtrs[i];
			if (chr && chr->aibot) {
				chr->aibot->hasuplink = (htm->uplink == chr->prop);
			}
		}
		/*
		 * Grant DATAUPLINK to the host-authoritative holder; strip everyone
		 * else. Do not reject token==self before give — that cleared real
		 * pickups (PROP_PICKUP skips scenario tokens on clients).
		 */
		for (i = 0; i < PLAYERCOUNT(); i++) {
			if (!g_Vars.players[i]) {
				continue;
			}
			{
				const s32 prev = g_Vars.currentplayernum;
				setCurrentPlayerNum(i);
				if (htm->uplink && g_Vars.players[i]->prop == htm->uplink) {
					if (!invHasSingleWeaponExcAllGuns(WEAPON_DATAUPLINK)) {
						invGiveSingleWeapon(WEAPON_DATAUPLINK);
					}
				} else if (invHasSingleWeaponExcAllGuns(WEAPON_DATAUPLINK)) {
					invRemoveItemByNum(WEAPON_DATAUPLINK);
					if (g_Vars.currentplayer->gunctrl.weaponnum == WEAPON_DATAUPLINK) {
						bgunEquipWeapon(WEAPON_UNARMED);
					}
				}
				setCurrentPlayerNum(prev);
			}
		}
		break;
	}
	case MPSCENARIO_CAPTURETHECASE: {
		struct scenariodata_ctc *ctc = &g_ScenarioData.ctc;
		s32 i;
		bool localholding = false;
		for (i = 0; i < ARRAYCOUNT(ctc->tokens); i++) {
			ctc->tokens[i] = netmsgReadTokenRef(src);
			ctc->teamindexes[i] = netbufReadS16(src);
			ctc->baserooms[i] = netbufReadS16(src);
		}
		netmsgApplyRemappedNumpoints(src);
		for (i = 0; i < g_MpNumChrs; i++) {
			struct chrdata *chr = g_MpAllChrPtrs[i];
			if (chr && chr->aibot) {
				s32 t;
				chr->aibot->hascase = false;
				for (t = 0; t < ARRAYCOUNT(ctc->tokens); t++) {
					if (ctc->tokens[t] == chr->prop) {
						chr->aibot->hascase = true;
						break;
					}
				}
			}
		}
		for (i = 0; i < ARRAYCOUNT(ctc->tokens); i++) {
			if (g_Vars.currentplayer && g_Vars.currentplayer->prop == ctc->tokens[i]) {
				localholding = true;
				break;
			}
		}
		if (!localholding && invHasBriefcase()) {
			invRemoveItemByNum(WEAPON_BRIEFCASE2);
			if (g_Vars.currentplayer->gunctrl.weaponnum == WEAPON_BRIEFCASE2) {
				bgunEquipWeapon(WEAPON_UNARMED);
			}
		}
		break;
	}
	case MPSCENARIO_POPACAP: {
		struct scenariodata_pac *pac = &g_ScenarioData.pac;
		s16 hostkills[MAX_MPCHRS];
		s16 hostsurv[MAX_MPCHRS];
		s32 i;
		pac->victimindex = netbufReadS32(src);
		pac->age240 = netbufReadU16(src);
		/*
		 * Keep host-stable mpchr indices in victims[]. Clients force local
		 * humans to playernum 0 — rewriting victims[] into local slots made
		 * "host is cap" look like "I am cap" on the client (and the timer
		 * flashed off after killing the real host). HUD/highlight compare
		 * via netMpChrIndexForPlayernum instead.
		 */
		for (i = 0; i < MAX_MPCHRS; i++) {
			pac->victims[i] = netbufReadS16(src);
			hostkills[i] = netbufReadS16(src);
			hostsurv[i] = netbufReadS16(src);
		}
		/* Scores are keyed by local g_MpAllChrConfigPtrs slot. */
		for (i = 0; i < MAX_MPCHRS; i++) {
			if (i < PLAYERCOUNT()) {
				const s32 hosti = netMpChrIndexForPlayernum(i);
				pac->killcounts[i] = (hosti >= 0 && hosti < MAX_MPCHRS) ? hostkills[hosti] : 0;
				pac->survivalcounts[i] = (hosti >= 0 && hosti < MAX_MPCHRS) ? hostsurv[hosti] : 0;
			} else {
				pac->killcounts[i] = hostkills[i];
				pac->survivalcounts[i] = hostsurv[i];
			}
		}
		break;
	}
	case MPSCENARIO_KINGOFTHEHILL: {
		struct scenariodata_koh *koh = &g_ScenarioData.koh;
		const RoomNum prevroom = koh->hillrooms[0];
		s32 i;
		koh->occupiedteam = netbufReadS16(src);
		koh->elapsed240 = netbufReadS16(src);
		koh->movehill = netbufReadS16(src);
		koh->hillindex = netbufReadS16(src);
		koh->hillcount = netbufReadS16(src);
		koh->hillrooms[0] = netbufReadS16(src);
		koh->hillrooms[1] = netbufReadS16(src);
		netbufReadCoord(src, &koh->hillpos);
		koh->colourfracr = netbufReadF32(src);
		koh->colourfracg = netbufReadF32(src);
		koh->colourfracb = netbufReadF32(src);
		netmsgApplyRemappedNumpoints(src);
		netdbgEvent("fields",
				"\"msgid\":%u,\"name\":\"SVC_SCENARIO\",\"scenario\":%u"
				",\"occupiedteam\":%d,\"elapsed240\":%d,\"movehill\":%d"
				",\"hillindex\":%d,\"hillrooms0\":%d,\"prevroom\":%d",
				(unsigned)SVC_SCENARIO, (unsigned)scenario,
				(int)koh->occupiedteam, (int)koh->elapsed240, (int)koh->movehill,
				(int)koh->hillindex, (int)koh->hillrooms[0], (int)prevroom);
		/*
		 * Vanilla only toggles LIGHTOP on hill relocate (clear old → highlight
		 * new). Re-applying HIGHLIGHT every SVC_SCENARIO (~4 Hz) was spamming
		 * room reshades and correlated with floating purple poly garbage while
		 * a team held the hill.
		 */
		{
			const RoomNum newroom = koh->hillrooms[0];
			const s32 roomchanged = (prevroom != newroom);
			const s32 prevok = (prevroom >= 0 && prevroom < g_Vars.roomcount);
			const s32 newok = (newroom >= 0 && newroom < g_Vars.roomcount);

			if (roomchanged && prevok) {
				netdbgEvent("client",
						"\"action\":\"koh_light_clear\",\"room\":%d", (int)prevroom);
				roomSetLightOp(prevroom, LIGHTOP_NONE, 0, 0, 0);
				netdbgFlush();
			}
			if (newok && !koh->movehill) {
				const s32 needhl = roomchanged
						|| g_Rooms[newroom].lightop != LIGHTOP_HIGHLIGHT;
				if (needhl) {
					netdbgEvent("client",
							"\"action\":\"koh_light_apply\",\"room\":%d,\"movehill\":%d",
							(int)newroom, (int)koh->movehill);
					roomSetLightOp(newroom, LIGHTOP_HIGHLIGHT, 0, 0, 0);
					netdbgFlush();
				}
			} else if (newok && koh->movehill
					&& g_Rooms[newroom].lightop == LIGHTOP_HIGHLIGHT
					&& koh->colourfracr >= 0.95f
					&& koh->colourfracg >= 0.95f
					&& koh->colourfracb >= 0.95f) {
				/* Host is mid-relocate fade; mirror the clear once whitened. */
				netdbgEvent("client",
						"\"action\":\"koh_light_clear\",\"room\":%d,\"reason\":\"movehill_fade\"",
						(int)newroom);
				roomSetLightOp(newroom, LIGHTOP_NONE, 0, 0, 0);
				netdbgFlush();
			}
		}
		break;
	}
	default:
		break;
	}

	return src->error;
}

u32 netmsgSvcScenarioEventWrite(struct netbuf *dst, u8 kind, s32 sfx,
		s32 playernum, u8 msgtype, u8 flags, struct prop *prop, const char *text)
{
	netbufWriteU8(dst, SVC_SCENARIO_EVENT);
	netdbgTx(1, SVC_SCENARIO_EVENT);
	netbufWriteU8(dst, kind);
	netbufWriteS16(dst, (s16)sfx);
	netbufWriteS8(dst, (s8)playernum);
	netbufWriteU8(dst, msgtype);
	netbufWriteU8(dst, flags);
	netbufWritePropPtr(dst, prop);
	if (kind == NETSCEN_HUD) {
		netbufWriteStr(dst, text ? text : "");
	}
	return dst->error;
}

u32 netmsgSvcScenarioEventRead(struct netbuf *src, struct netclient *srccl)
{
	const u8 kind = netbufReadU8(src);
	const s16 sfx = netbufReadS16(src);
	const s8 playernum = netbufReadS8(src);
	const u8 msgtype = netbufReadU8(src);
	const u8 flags = netbufReadU8(src);
	struct prop *prop = netbufReadPropPtr(src);
	char *text = NULL;

	if (kind == NETSCEN_HUD) {
		text = netbufReadStr(src);
	}

	if (src->error || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	switch (kind) {
	case NETSCEN_SFX:
		sndStart(var80095200, sfx, NULL, -1, -1, -1, -1, -1);
		break;
	case NETSCEN_SND:
		snd00010718(NULL, 0, AL_VOL_FULL, AL_PAN_CENTER, sfx, 1, 1, -1, 1);
		break;
	case NETSCEN_HUD:
		if (text && text[0]) {
			bool show = false;
			if (playernum < 0 || playernum == (s8)0xFF) {
				show = true;
			} else if (g_NetMode == NETMODE_CLIENT) {
				/* Host-stable index for the local human (always players[0]). */
				show = (playernum == (s8)netMpChrIndexForPlayernum(0));
			} else if (playernum == g_Vars.currentplayernum) {
				show = true;
			}
			if (show) {
				/*
				 * hudmsgCreateWithFlags drops all net msgs when
				 * currentplayernum != 0 (remote pawn). Force local slot.
				 */
				const s32 prevpn = g_Vars.currentplayernum;
				if (prevpn != 0) {
					setCurrentPlayerNum(0);
				}
				hudmsgCreateWithFlags(text, msgtype, flags);
				if (prevpn != g_Vars.currentplayernum) {
					setCurrentPlayerNum(prevpn);
				}
			}
		}
		break;
	case NETSCEN_PSCREATE:
		if (prop) {
			psCreate(NULL, prop, sfx, -1, -1,
					playernum ? PSFLAG_REPEATING : 0, PSFLAG2_MPPAUSABLE, PSTYPE_NONE,
					NULL, -1, NULL, -1, -1, -1, -1);
		}
		break;
	case NETSCEN_PSSTOP:
		if (prop) {
			psStopSound(prop, PSTYPE_GENERAL, 0xffff);
		}
		break;
	default:
		break;
	}

	return src->error;
}

u32 netmsgSvcExplosionWrite(struct netbuf *dst, struct coord *pos, RoomNum *rooms,
		s16 type, s32 playernum)
{
	if (!dst || !pos || !rooms) {
		return dst ? dst->error : 1;
	}

	netbufWriteU8(dst, SVC_EXPLOSION);
	netdbgTx(1, SVC_EXPLOSION);
	netbufWriteCoord(dst, pos);
	netbufWriteRooms(dst, rooms, 8);
	netbufWriteS16(dst, type);
	netbufWriteS8(dst, (s8)playernum);
	netdbgEvent("tx",
			"\"msgid\":%u,\"name\":\"SVC_EXPLOSION\",\"type\":%d,\"room0\":%d,\"owner\":%d",
			(unsigned)SVC_EXPLOSION, (int)type, (int)rooms[0], (int)playernum);
	return dst->error;
}

u32 netmsgSvcExplosionRead(struct netbuf *src, struct netclient *srccl)
{
	struct coord pos;
	RoomNum rooms[8] = { -1 };
	s32 ri;

	netbufReadCoord(src, &pos);
	netbufReadRooms(src, rooms, ARRAYCOUNT(rooms));
	const s16 type = netbufReadS16(src);
	const s8 playernum = netbufReadS8(src);

	if (src->error || srccl->state < CLSTATE_GAME) {
		return src->error;
	}

	if (type <= EXPLOSIONTYPE_NONE || type > EXPLOSIONTYPE_HUGE25) {
		netdbgEvent("warn",
				"\"action\":\"explosion_bad_type\",\"type\":%d",
				(int)type);
		return src->error;
	}

	for (ri = 0; ri < ARRAYCOUNT(rooms); ri++) {
		if (rooms[ri] < 0) {
			rooms[ri] = -1;
			break;
		}
		if (rooms[ri] >= g_Vars.roomcount) {
			netdbgEvent("warn",
					"\"action\":\"explosion_bad_room\",\"room\":%d,\"type\":%d",
					(int)rooms[ri], (int)type);
			rooms[0] = -1;
			break;
		}
	}
	rooms[ARRAYCOUNT(rooms) - 1] = -1;

	if (rooms[0] < 0) {
		return src->error;
	}

	netdbgEvent("fields",
			"\"msgid\":%u,\"name\":\"SVC_EXPLOSION\",\"type\":%d,\"room0\":%d,\"owner\":%d",
			(unsigned)SVC_EXPLOSION, (int)type, (int)rooms[0], (int)playernum);

	g_NetApplyingExplosion = 1;
	explosionCreateSimple(NULL, &pos, rooms, type, playernum);
	g_NetApplyingExplosion = 0;

	netdbgEvent("client",
			"\"action\":\"explosion_applied\",\"type\":%d,\"room0\":%d",
			(int)type, (int)rooms[0]);
	netdbgFlush();

	return src->error;
}
