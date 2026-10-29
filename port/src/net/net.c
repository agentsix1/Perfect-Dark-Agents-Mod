#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include "platform.h"
#include "net/netenet.h"
#include "net/net.h"
#include "net/netbuf.h"
#include "net/netmsg.h"
#include "net/netdbg.h"
#include "types.h"
#include "constants.h"
#include "data.h"
#include "bss.h"
#include "game/hudmsg.h"
#include "game/playermgr.h"
#include "game/player.h"
#include "game/bondgun.h"
#include "game/chraction.h"
#include "game/game_1531a0.h"
#include "game/game_0b0fd0.h"
#include "game/title.h"
#include "game/menu.h"
#include "game/pdmode.h"
#include "game/mplayer/mplayer.h"
#include "game/mplayer/scenarios.h"
#include "game/mplayer/ingame.h"
#include "game/challenge.h"
#include "game/botinv.h"
#include "game/botact.h"
#include "game/prop.h"
#include "game/chr.h"
#include "game/playermgr.h"
#include "game/propobj.h"
#include "game/inv.h"
#include "game/propsnd.h"
#include "game/modelmgr.h"
#include "game/camera.h"
#include "lib/model.h"
#include "lib/anim.h"
#include "lib/mtx.h"
#include "lib/main.h"
#include "lib/snd.h"
#include "lib/collision.h"
#include "lib/ailist.h"

extern struct menudialogdef g_NetWaitingPlayersDialog;
extern struct menudialogdef g_NetEndscreenWaitingDialog;
extern s32 var80087260;

extern s32 g_MpWeaponSetNum;
#include "lib/vi.h"
#include "config.h"
#include "system.h"
#include "console.h"
#include "fs.h"
#include "romdata.h"
#include "utils.h"

s32 g_NetMode = NETMODE_NONE;
s32 g_NetApplyingWallhit = 0;
s32 g_NetApplyingGlass = 0;
s32 g_NetApplyingMpSetup = 0;
s32 g_NetApplyingPropSpawn = 0;
s32 g_NetApplyingChrDamage = 0;
s32 g_NetApplyingExplosion = 0;
s32 g_NetAllowSetupChanges = 0;
s32 g_NetReturnToNetMenu = 0;
s32 g_NetWaitingForPlayers = 0;
s32 g_NetAwaitingRejoin = 0;
s32 g_NetForceRespawn = 0;
s32 g_NetWallhitColoursValid = 0;
u8 g_NetWallhitColours[4][4];

s32 g_NetHostLatch = false;
s32 g_NetJoinLatch = false;

u32 g_NetServerUpdateRate = 1;
u32 g_NetServerInRate = 128 * 1024;
u32 g_NetServerOutRate = 128 * 1024;
u32 g_NetServerPort = NET_DEFAULT_PORT;
s32 g_NetServerInfoQuery = true;

u32 g_NetClientUpdateRate = 1;
u32 g_NetClientInRate = 128 * 1024;
u32 g_NetClientOutRate = 128 * 1024;

u32 g_NetInterpTicks = 3;
char g_NetLastJoinAddr[NET_MAX_ADDR + 1] = "127.0.0.1:27100";

s32 g_NetLagCompEnabled = 1;
u32 g_NetLagCompMaxTicks = 24; // ~400ms at 60Hz; caps rewind abuse

u32 g_NetTick = 0;
u32 g_NetNextSyncId = 1;

/* Open-addressed syncid → prop map (tombstones keep probe chains valid). */
#define NET_SYNC_HASH_SIZE 4096
#define NET_SYNC_HASH_MASK (NET_SYNC_HASH_SIZE - 1)
#define NET_SYNC_EMPTY 0u
#define NET_SYNC_TOMB  0xffffffffu

static u32 s_NetSyncHashKeys[NET_SYNC_HASH_SIZE];
static struct prop *s_NetSyncHashVals[NET_SYNC_HASH_SIZE];
static s32 s_NetSyncHashLive;

static void netSyncHashClear(void)
{
	memset(s_NetSyncHashKeys, 0, sizeof(s_NetSyncHashKeys));
	memset(s_NetSyncHashVals, 0, sizeof(s_NetSyncHashVals));
	s_NetSyncHashLive = 0;
}

s32 g_NetSimPacketLoss = 0;
s32 g_NetDebugDraw = 0;

u64 g_NetRngSeeds[2];
u32 g_NetRngLatch = 0;

s32 g_NetMaxClients = NET_MAX_CLIENTS;
s32 g_NetNumClients = 0;
struct netclient g_NetClients[NET_MAX_CLIENTS + 1]; // last is an extra temporary client
struct netclient *g_NetLocalClient = &g_NetClients[NET_MAX_CLIENTS];

static u8 g_NetMsgBuf[NET_BUFSIZE];
struct netbuf g_NetMsg = { .data = g_NetMsgBuf, .size = sizeof(g_NetMsgBuf) };

static u8 g_NetMsgRelBuf[NET_BUFSIZE * 4]; // reliable buffer can be reliably fragmented
struct netbuf g_NetMsgRel = { .data = g_NetMsgRelBuf, .size = sizeof(g_NetMsgRelBuf) };

static s32 g_NetInit = false;
static ENetHost *g_NetHost;
static ENetAddress g_NetLocalAddr;
static ENetAddress g_NetRemoteAddr;

static u32 g_NetNextUpdate = 0;

static u32 g_NetReliableFrameLen = 0;
static u32 g_NetUnreliableFrameLen = 0;

static s32 netParseAddr(ENetAddress *out, const char *str)
{
	char tmp[256] = { 0 };

	if (!str || !str[0]) {
		return false;
	}

	strncpy(tmp, str, sizeof(tmp) - 1);

	char *host = tmp;
	char *port = NULL;
	char *colon = strrchr(tmp, ':');

	// if there is a : in the address string, there could be a port value
	// otherwise it's an ip or hostname with default port
	if (colon > tmp) {
		if (tmp[0] == '[' && colon[-1] == ']' && isdigit(colon[1])) {
			// ipv6 with port: [ADDR]:PORT
			colon[-1] = '\0'; // terminate ip
			host = tmp + 1; // skip [
			port = colon + 1; // skip :
		} else if (isdigit(colon[1]) && strchr(host, ':') == colon) {
			// ipv4 or hostname with port
			colon[0] = '\0'; // terminate ip
			port = colon + 1; // skip :
		}
	}

	if (!host[0]) {
		return false;
	}

	const s32 portval = port ? atoi(port) : NET_DEFAULT_PORT;
	if (portval < 0 || portval > 0xFFFF) {
		return false;
	}

	memset(out, 0, sizeof(*out));
	out->port = portval;

	if (isdigit(host[0]) || strchr(host, ':')) {
		// we stripped off the :PORT at this point, now check if this is an IP address
		if (enet_address_set_ip(out, host) == 0) {
			return true;
		}
	}

	// must be a domain name; do a lookup
	return (enet_address_set_hostname(out, host) == 0);
}

static const char *netFormatAddr(const ENetAddress *addr)
{
	static char str[256];
	char tmp[256];
	if (addr && enet_address_get_ip(addr, tmp, sizeof(tmp) - 1) == 0) {
		if (tmp[0]) {
			if (strchr(tmp, ':')) {
				// ipv6
				snprintf(str, sizeof(str) - 1, "[%s]:%u", tmp, addr->port);
			} else {
				// ipv4
				snprintf(str, sizeof(str) - 1, "%s:%u", tmp, addr->port);
			}
			return str;
		}
	}
	return NULL;
}

static inline const char *netFormatPeerAddr(const ENetPeer *peer)
{
	return netFormatAddr(&peer->address);
}

const char *netFormatClientAddr(const struct netclient *cl)
{
	return cl->peer ? netFormatPeerAddr(cl->peer) : "<local>";
}

static inline void netClientReset(struct netclient *cl)
{
	if (cl->state >= CLSTATE_GAME && cl->player) {
		cl->player->client = NULL;
		cl->player->isremote = false;
	}
	memset(cl, 0, sizeof(*cl));
	cl->out.data = cl->out_data;
	cl->out.size = sizeof(cl->out_data);
	cl->id = cl - g_NetClients;
	cl->settings.team = 0xff;
}

static inline void netClientResetAll(void)
{
	g_NetMaxClients = NET_MAX_CLIENTS;
	g_NetNumClients = 1; // always at least one client, which is us
	for (u32 i = 0; i < NET_MAX_CLIENTS + 1; ++i) {
		netClientReset(&g_NetClients[i]);
	}
}

static inline void netClientRecordMove(struct netclient *cl, const struct player *pl)
{
	// make space in the move stack
	memmove(cl->outmove + 1, cl->outmove, sizeof(cl->outmove) - sizeof(*cl->outmove));

	struct netplayermove *move = &cl->outmove[0];

	move->tick = g_NetTick;
	move->crouchofs = pl->crouchoffset;
	move->leanofs = pl->swaytarget / 75.f;
	move->movespeed[0] = pl->speedforwards;
	move->movespeed[1] = pl->speedsideways;
	move->angles[0] = pl->vv_theta;
	move->angles[1] = pl->vv_verta;
  move->pos = (pl->prop) ? pl->prop->pos : pl->cam_pos;

	move->crosspos[0] = pl->crosspos[0];
	move->crosspos[1] = pl->crosspos[1];

	if (!pl->isremote) {
		// normalize crosspos x
		move->crosspos[0] -= (f32)(SCREEN_WIDTH_LO / 2);
		move->crosspos[0] = (f32)(SCREEN_WIDTH_LO / 2) + move->crosspos[0] * pl->aspect / SCREEN_ASPECT;
	}

	move->ucmd = pl->ucmd;

	if (g_NetMode == NETMODE_SERVER && pl->isremote && cl->inmove[0].tick) {
		// carry some of the client inputs over to the outmove
		move->ucmd |= (cl->inmove[0].ucmd & (UCMD_FIRE | UCMD_RELOAD | UCMD_AIMMODE | UCMD_EYESSHUT | UCMD_SELECT | UCMD_SELECT_DUAL));
		move->crosspos[0] = cl->inmove[0].crosspos[0];
		move->crosspos[1] = cl->inmove[0].crosspos[1];
	}

	if (pl->crouchpos == CROUCHPOS_DUCK) {
		move->ucmd |= UCMD_DUCK;
	} else if (pl->crouchpos == CROUCHPOS_SQUAT) {
		move->ucmd |= UCMD_SQUAT;
	}

	if (pl->gunctrl.switchtoweaponnum >= 0 && !pl->gunctrl.throwing) {
		move->ucmd |= UCMD_SELECT;
		move->weaponnum = pl->gunctrl.switchtoweaponnum;
	} else {
		move->weaponnum = -1;
	}

	if (pl->gunctrl.dualwielding && !pl->gunctrl.throwing) {
		move->ucmd |= UCMD_SELECT_DUAL;
		if ((move->ucmd ^ cl->outmove[1].ucmd) & UCMD_SELECT_DUAL) {
			move->ucmd |= UCMD_SELECT;
		}
	}

	const s32 oldnum = g_Vars.currentplayernum;
	setCurrentPlayerNum(cl->playernum);

	if (bgunIsUsingSecondaryFunction()) {
		move->ucmd |= UCMD_SECONDARY;
	}

	if (pl->insightaimmode) {
		move->ucmd |= UCMD_AIMMODE;
		move->zoomfov = currentPlayerGetGunZoomFov();
	} else {
		move->zoomfov = 0.f;
	}

	setCurrentPlayerNum(oldnum);

	if (cl != g_NetLocalClient && !cl->forcetick && (move->ucmd & UCMD_FL_FORCEMASK)) {
		cl->forcetick = move->tick;
		sysLogPrintf(LOG_NOTE, "NET: forcing client %u to move at tick %u", cl->id, cl->forcetick);
	}
}

static inline s32 netClientNeedReliableMove(const struct netclient *cl)
{
	const struct netplayermove *move = &cl->outmove[0];
	const struct netplayermove *moveprev = &cl->outmove[1];
	return !moveprev->tick || (g_NetMode == NETMODE_SERVER && cl->forcetick) ||
		(moveprev->ucmd & UCMD_IMPORTANT_MASK) != (move->ucmd & UCMD_IMPORTANT_MASK) ||
		(move->ucmd & UCMD_ACTIVATE);
}

static inline s32 netClientNeedMove(const struct netclient *cl)
{
	if (g_NetTick < g_NetNextUpdate) {
		return false;
	}
	const struct netplayermove *move = &cl->outmove[0];
	const struct netplayermove *moveprev = &cl->outmove[1];
	if (move->tick && cl->outmoveack >= move->tick) {
		return false;
	}
	const u8 *cmpa = (const u8 *)move + sizeof(move->tick);
	const u8 *cmpb = (const u8 *)moveprev + sizeof(move->tick);
	return (memcmp(cmpa, cmpb, sizeof(*move) - sizeof(move->tick)) != 0);
}

static inline void netClientReadConfig(struct netclient *cl, const s32 playernum)
{
	cl->settings.options = g_PlayerConfigsArray[playernum].options;
	cl->settings.bodynum = g_PlayerConfigsArray[playernum].base.mpbodynum;
	cl->settings.headnum = g_PlayerConfigsArray[playernum].base.mpheadnum;
	cl->settings.team = g_PlayerConfigsArray[playernum].base.team;
	cl->settings.fovy = g_PlayerExtCfg[playernum].fovy;
	cl->settings.fovzoommult = g_PlayerExtCfg[playernum].fovzoommult;
	memcpy(cl->settings.name, g_PlayerConfigsArray[playernum].base.name, sizeof(cl->settings.name));
	// the \n will be readded in the playerconfig
	char *newline = strrchr(cl->settings.name, '\n');
	if (newline) {
		*newline = '\0';
	}
}

s32 netGetLocalConfigIndex(void)
{
	// Solo/CI uses MAX_PLAYERS as a sentinel "current player" slot. Writing
	// paintball (and other options) there made them look enabled in menus but
	// never apply in a Combat Sim match (which uses slots 0..MAX_PLAYERS-1).
	if (g_Vars.currentplayerstats && g_Vars.currentplayerstats->mpindex < MAX_PLAYERS) {
		return g_Vars.currentplayerstats->mpindex;
	}
	if (g_MpPlayerNum >= 0 && g_MpPlayerNum < MAX_PLAYERS) {
		return g_MpPlayerNum;
	}
	if (g_NetLocalClient && g_NetLocalClient->playernum < MAX_PLAYERS) {
		return g_NetLocalClient->playernum;
	}
	return 0;
}

// Only the fields players actually edit in Game Setup. Full chrslots player
// bits are rebuilt each frame on the host (see netLobbyRebuildHumanSlots);
// we track human occupancy + teams separately so Teams UI stays in sync.
static struct {
	u8 stagenum;
	u8 scenario;
	u8 scorelimit;
	u8 timelimit;
	u16 teamscorelimit;
	u32 options;
	u8 weapons[NUM_MPWEAPONSLOTS];
	u8 weaponset;
	u8 locktype;
	u8 challengeindex;
	u8 mphilltime;
	char name[MPSETUP_MAXNAME + 1];
	u16 simslots;
	u16 humanslots;
	u8 playerteams[MAX_PLAYERS];
	struct mpbotconfig bots[MAX_BOTS];
} g_NetMpSetupShadow;
static s32 g_NetMpSetupShadowValid = 0;

extern u8 g_MpSimulantDifficultiesPerNumPlayers[MAX_BOTS][MAX_LOCAL_PLAYERS];

void netMpSimulantsWrite(struct netbuf *dst)
{
	for (s32 i = 0; i < MAX_BOTS; ++i) {
		const struct mpbotconfig *bot = &g_BotConfigsArray[i];
		netbufWriteU8(dst, bot->base.mpheadnum);
		netbufWriteU8(dst, bot->base.mpbodynum);
		netbufWriteU8(dst, bot->base.team);
		netbufWriteU8(dst, bot->type);
		netbufWriteU8(dst, bot->difficulty);
		netbufWriteU32(dst, bot->base.displayoptions);
		netbufWriteData(dst, bot->base.name, MAX_PLAYERNAME);
	}
}

static s32 netMpSimulantsRead(struct netbuf *src, s32 apply)
{
	for (s32 i = 0; i < MAX_BOTS; ++i) {
		const u8 head = netbufReadU8(src);
		const u8 body = netbufReadU8(src);
		const u8 team = netbufReadU8(src);
		const u8 type = netbufReadU8(src);
		const u8 difficulty = netbufReadU8(src);
		const u32 displayoptions = netbufReadU32(src);
		u8 name[MAX_PLAYERNAME];
		netbufReadData(src, name, sizeof(name));

		if (src->error) {
			return -1;
		}

		if (!apply) {
			continue;
		}

		const u16 mask = (u16)(1 << (i + MAX_PLAYERS));
		if (g_MpSetup.chrslots & mask) {
			struct mpbotconfig *bot = &g_BotConfigsArray[i];
			bot->base.mpheadnum = head;
			bot->base.mpbodynum = body;
			bot->base.team = team;
			bot->type = type;
			bot->difficulty = difficulty;
			bot->base.displayoptions = displayoptions;
			memcpy(bot->base.name, name, sizeof(bot->base.name));
			for (s32 j = 0; j < MAX_LOCAL_PLAYERS; ++j) {
				g_MpSimulantDifficultiesPerNumPlayers[i][j] = difficulty;
			}
		} else {
			func0f1881d4(i);
		}
	}

	return 0;
}

void netMpSimulantsSkip(struct netbuf *src)
{
	(void)netMpSimulantsRead(src, 0);
}

void netMpSimulantsApply(struct netbuf *src)
{
	if (netMpSimulantsRead(src, 1) < 0) {
		sysLogPrintf(LOG_WARNING, "NET: malformed simulant block");
	}
}

u16 g_NetLobbyHumanSlots = 1;

/**
 * Host: mark one chrslot bit per connected lobby client (dense from 0) and
 * copy each client's name/team into PlayerConfigsArray so Teams/sims UI matches.
 */
void netLobbyRebuildHumanSlots(void)
{
	s32 slot = 0;

	g_MpSetup.chrslots &= 0xff00;

	for (s32 i = 0; i < g_NetMaxClients && slot < MAX_PLAYERS; ++i) {
		struct netclient *cl = &g_NetClients[i];
		if (cl->state < CLSTATE_LOBBY) {
			continue;
		}

		g_MpSetup.chrslots |= (u16)(1 << slot);

		struct mpplayerconfig *cfg = &g_PlayerConfigsArray[slot];
		if (cl->settings.name[0]) {
			snprintf(cfg->base.name, sizeof(cfg->base.name), "%s\n", cl->settings.name);
		}
		// Prefer config team (Teams menu edits this); seed settings from it.
		cl->settings.team = cfg->base.team;
		cl->playernum = (u8)slot;
		cl->config = cfg;
		slot++;
	}

	g_NetLobbyHumanSlots = g_MpSetup.chrslots & 0x00ff;
}

void netMpPlayersWrite(struct netbuf *dst)
{
	u8 count = 0;
	s32 slot = 0;

	for (s32 i = 0; i < g_NetMaxClients && slot < MAX_PLAYERS; ++i) {
		if (g_NetClients[i].state >= CLSTATE_LOBBY) {
			count++;
			slot++;
		}
	}

	netbufWriteU8(dst, count);
	slot = 0;
	for (s32 i = 0; i < g_NetMaxClients && slot < MAX_PLAYERS; ++i) {
		struct netclient *cl = &g_NetClients[i];
		if (cl->state < CLSTATE_LOBBY) {
			continue;
		}
		netbufWriteU8(dst, cl->id);
		netbufWriteU8(dst, g_PlayerConfigsArray[slot].base.team);
		netbufWriteStr(dst, cl->settings.name[0] ? cl->settings.name : g_PlayerConfigsArray[slot].base.name);
		slot++;
	}
}

static s32 netMpPlayersRead(struct netbuf *src, s32 apply)
{
	const u8 count = netbufReadU8(src);
	if (src->error || count > MAX_PLAYERS) {
		return -1;
	}

	if (apply) {
		g_MpSetup.chrslots = (g_MpSetup.chrslots & 0xff00);
	}

	for (u8 slot = 0; slot < count; ++slot) {
		const u8 id = netbufReadU8(src);
		const u8 team = netbufReadU8(src);
		char *name = netbufReadStr(src);

		if (src->error) {
			return -1;
		}

		if (!apply) {
			continue;
		}

		g_MpSetup.chrslots |= (u16)(1 << slot);
		g_PlayerConfigsArray[slot].base.team = team;
		if (name && name[0]) {
			snprintf(g_PlayerConfigsArray[slot].base.name, sizeof(g_PlayerConfigsArray[slot].base.name),
					"%s\n", name);
		}

		if (id < NET_MAX_CLIENTS) {
			struct netclient *cl = &g_NetClients[id];
			cl->settings.team = team;
			if (name && name[0]) {
				strncpy(cl->settings.name, name, sizeof(cl->settings.name) - 1);
				cl->settings.name[sizeof(cl->settings.name) - 1] = '\0';
			}
			cl->playernum = slot;
			cl->config = &g_PlayerConfigsArray[slot];
			// Clients learn about other lobby peers through this roster.
			if (g_NetMode == NETMODE_CLIENT && cl != g_NetLocalClient
					&& cl->state < CLSTATE_LOBBY) {
				cl->state = CLSTATE_LOBBY;
				cl->id = id;
			}
		}
	}

	if (apply) {
		g_NetLobbyHumanSlots = g_MpSetup.chrslots & 0x00ff;
	}

	return 0;
}

void netMpPlayersSkip(struct netbuf *src)
{
	(void)netMpPlayersRead(src, 0);
}

void netMpPlayersApply(struct netbuf *src)
{
	if (netMpPlayersRead(src, 1) < 0) {
		sysLogPrintf(LOG_WARNING, "NET: malformed lobby player roster");
	}
}

struct netsimsnap g_NetSimSnapshot[MAX_BOTS];
static u8 g_NetSimFlashLatch[MAX_BOTS][2]; // per-hand muzzle latch like remote players
static u8 g_NetSimGunSoundFlags[MAX_BOTS]; // last FLASH bits we played SFX for
static u8 g_NetSimEquipPending[MAX_BOTS]; // weapon swap queued off propsTickPlayer walk
/* Latch last applied equip so we do not delete/recreate syncid=0 guns every frame. */
static u8 g_NetSimEquipAppliedWeapon[MAX_BOTS];
static u8 g_NetSimEquipAppliedDual[MAX_BOTS];
static u8 g_NetSimEquipAppliedValid[MAX_BOTS];

static void netPlayPositionalGunShot(struct prop *prop, struct gset *gset)
{
	u16 soundnum;

	if (!prop || !gset) {
		return;
	}

	soundnum = gsetGetSingleShootSound(gset);
	if (soundnum) {
		psCreate(NULL, prop, soundnum, -1, -1,
				PSFLAG_0400, PSFLAG2_PRINTABLE | PSFLAG2_MPPAUSABLE,
				PSTYPE_CHRSHOOT, NULL, -1, NULL, -1, -1, -1, -1);
	}
}

static inline s32 netSimPropRoomsEqual(const RoomNum *ra, const RoomNum *rb)
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

s32 netSimSlotForChr(struct chrdata *chr)
{
	if (!chr || !chr->aibot || !chr->aibot->config) {
		return -1;
	}

	for (s32 slot = 0; slot < MAX_BOTS; ++slot) {
		if (chr->aibot->config == &g_BotConfigsArray[slot]) {
			return slot;
		}
	}

	return -1;
}

void netSimReceiveSnapshot(u8 slot, struct netsimsnap *snap)
{
	if (slot >= MAX_BOTS || !snap) {
		return;
	}

	g_NetSimSnapshot[slot] = *snap;
	g_NetSimSnapshot[slot].valid = 1;
	g_NetSimSnapshot[slot].dirty = 1;
}

static void netClientSimInvalidateSlot(s32 slot)
{
	if (slot < 0 || slot >= MAX_BOTS) {
		return;
	}

	g_NetSimSnapshot[slot].valid = 0;
	g_NetSimSnapshot[slot].dirty = 0;
	g_NetSimFlashLatch[slot][HAND_RIGHT] = 0;
	g_NetSimFlashLatch[slot][HAND_LEFT] = 0;
	g_NetSimGunSoundFlags[slot] = 0;
}

// Defined below — ForceRespawn needs it before the full body.
void netClientSimSyncGround(struct chrdata *chr, struct netsimsnap *snap);

/**
 * While a simulant is dead/dying, drop presentation so we do not reapply
 * walk/attack anims or muzzle flash over the death sequence.
 */
static bool netClientSimShouldHoldPresentation(struct chrdata *chr)
{
	if (!chr || !chr->aibot) {
		return false;
	}

	if (!chrIsDead(chr)) {
		return true;
	}

	netClientSimInvalidateSlot(netSimSlotForChr(chr));

	if (chr->model) {
		chrStopFiring(chr);
	}

	return false;
}

/** Fully opaque living body — death fade / spawn fade must not stick. */
static void netClientSimResetLivingAlpha(struct chrdata *chr)
{
	if (!chr || !chr->aibot) {
		return;
	}

	chr->fadealpha = 0xff;
	chr->aibot->fadeintimer60 = 0;
	chr->aibot->respawning = false;

	if ((chr->hidden & CHRHFLAG_CLOAKED) == 0) {
		chr->cloakfadefrac = 0;
		chr->cloakfadefinished = false;
	}
}

/** Hide + no collision until the first host pose — prevents ghost fire / floor pops. */
static void netClientSimHideUntilPose(struct chrdata *chr)
{
	if (!chr || !chr->prop) {
		return;
	}

	chr->chrflags |= CHRCFLAG_HIDDEN;
	if (chr->prop->flags & PROPFLAG_ENABLED) {
		propDisable(chr->prop);
	}
	if (chr->model) {
		chrStopFiring(chr);
	}
	// Do NOT arm fadeintimer here — HIDDEN is enough; a leftover timer
	// made living sims render translucent after pose arrived.
}

/**
 * Host is alive again (or first living snap arrived while we still showed dead).
 * Clear death state so presentation / geometry can resume from the snapshot.
 */
#define NET_DEFER_DELETE_MAX 256
static struct prop *g_NetDeferredDeletes[NET_DEFER_DELETE_MAX];
static s32 g_NetDeferredDeleteCount = 0;

static void netClientSimClearHeldWeapons(struct chrdata *chr);

void netClientSimForceRespawnFromSnap(struct chrdata *chr, struct netsimsnap *snap)
{
	if (!chr || !chr->aibot || !chr->prop || !snap) {
		return;
	}

	netdbgEvent("client",
			"\"action\":\"sim_force_respawn\",\"syncid\":%u,\"prev_act\":%d,\"slot_weapon\":%u",
			(unsigned)chr->prop->syncid, (int)chr->actiontype, (unsigned)snap->weaponnum);

	chrStopFiring(chr);
	/* Drop held guns before re-enable — leaving them for EquipWeapon caused
	 * mid-tick defer_delete churn that punched NULL holes in activeprops. */
	netClientSimClearHeldWeapons(chr);

	chr->actiontype = ACT_BONDMULTI;
	chr->sleep = 0;
	chr->damage = 0;
	chr->ailist = ailistFindById(GAILIST_AIBOT_MAIN);
	chr->aioffset = 0;
	chr->hidden &= ~(CHRHFLAG_CLOAKED | CHRHFLAG_NEEDANIM);
	chr->chrflags |= CHRCFLAG_HIDDEN;
	propDisable(chr->prop);

	chr->aibot->hasbriefcase = false;
	chr->aibot->hascase = false;
	chr->aibot->hasuplink = false;
	netClientSimResetLivingAlpha(chr);
	chr->cshield = snap->cshield;

	{
		s32 slot = netSimSlotForChr(chr);

		if (slot >= 0 && slot < MAX_BOTS) {
			g_NetSimEquipAppliedValid[slot] = 0;
			g_NetSimEquipPending[slot] = 1;
		}
	}

	netClientSimSyncGround(chr, snap);
}

static s32 netPropIsInParentChildList(struct prop *prop)
{
	struct prop *parent;
	struct prop *c;
	s32 csafety = 64;

	if (!prop || !prop->parent) {
		return 0;
	}
	parent = prop->parent;
	/* Same dangling-parent AV as propsRepairActiveList — validate pool first. */
	if (!propIsInPool(parent)) {
		prop->parent = NULL;
		return 0;
	}
	for (c = parent->child; c && --csafety >= 0; c = propGetChildNext(c)) {
		if (!propIsInPool(c)) {
			break;
		}
		if (c == prop) {
			return 1;
		}
	}
	return 0;
}

/**
 * Unlink from active/paused regardless of a stale prop->active flag.
 * propDelist alone misses head updates when active was cleared early.
 */
static void netPropForceDelist(struct prop *prop)
{
	if (!prop) {
		return;
	}

	if (prop == g_Vars.activeprops) {
		g_Vars.activeprops = (prop->next && propIsInPool(prop->next))
				? prop->next : g_Vars.pausedprops;
	}
	if (prop == g_Vars.activepropstail) {
		if (g_Vars.activeprops == g_Vars.pausedprops) {
			g_Vars.activepropstail = g_Vars.pausedprops;
		} else {
			g_Vars.activepropstail = (prop->prev && propIsInPool(prop->prev))
					? prop->prev : g_Vars.pausedprops;
		}
	}
	if (prop == g_Vars.pausedprops) {
		g_Vars.pausedprops = (prop->next && propIsInPool(prop->next))
				? prop->next : NULL;
		if (g_Vars.activeprops == prop) {
			g_Vars.activeprops = g_Vars.pausedprops;
		}
		if (g_Vars.activepropstail == prop) {
			g_Vars.activepropstail = g_Vars.pausedprops;
		}
	}

	if (prop->next && propIsInPool(prop->next)) {
		prop->next->prev = prop->prev;
	}
	if (prop->prev && propIsInPool(prop->prev)) {
		prop->prev->next = prop->next;
	}

	prop->next = NULL;
	prop->prev = NULL;
	prop->active = false;
}

s32 netPropIsQueuedForDelete(const struct prop *prop)
{
	s32 i;

	if (!prop) {
		return 0;
	}
	for (i = 0; i < g_NetDeferredDeleteCount; i++) {
		if (g_NetDeferredDeletes[i] == prop) {
			return 1;
		}
	}
	return 0;
}

void netDeferPropDelete(struct prop *prop)
{
	if (!prop) {
		return;
	}
	if (!propIsInPool(prop)) {
		netdbgEvent("warn", "\"action\":\"defer_delete_oob\"");
		return;
	}
	if (prop->freelinked) {
		/* Already propFree'd — never queue a free-list node. */
		netdbgEvent("warn", "\"action\":\"defer_delete_already_free\"");
		return;
	}
	if (netPropIsQueuedForDelete(prop)) {
		return;
	}

	/*
	 * PC: child siblings use childnext/childprev. Still detach before delist so
	 * parent→child walks never see a DELETING prop, then strip world links.
	 */
	if (prop->parent) {
		if (netPropIsInParentChildList(prop)) {
			propDetach(prop);
		} else {
			/* Stale parent; next/prev are world links — clear and delist below. */
			netdbgEvent("warn",
					"\"action\":\"defer_stale_parent\",\"syncid\":%u,\"type\":%u",
					(unsigned)prop->syncid, (unsigned)prop->type);
			prop->parent = NULL;
			prop->childnext = NULL;
			prop->childprev = NULL;
		}
	}

	if (prop->active || prop == g_Vars.activeprops || prop == g_Vars.activepropstail
			|| prop == g_Vars.pausedprops || prop->next || prop->prev) {
		propDeregisterRooms(prop);
		netPropForceDelist(prop);
		propDisable(prop);
	} else {
		propDeregisterRooms(prop);
		propDisable(prop);
	}

	if (prop->obj) {
		/*
		 * obj->model can be a dangling non-NULL after a prior free/reuse —
		 * writing attachedtomodel then AVs (host crash in propsTick →
		 * propExecuteTickOperation → netDeferPropDelete).
		 */
		struct model *model = prop->obj->model;

		if (model) {
			if (g_ModelSlots && g_MaxModels > 0
					&& model >= g_ModelSlots
					&& model < g_ModelSlots + g_MaxModels
					&& model->definition) {
				model->attachedtomodel = NULL;
				model->attachedtonode = NULL;
			} else {
				prop->obj->model = NULL;
			}
		}
		prop->obj->hidden |= OBJHFLAG_DELETING;
	}

	/*
	 * Never drop DELETING props — overflow used to orphan them, then rebuild
	 * resurrected zombies (null obj / bad propstateindex → client AV).
	 * Reap already-delisted entries first; last resort frees this prop now.
	 */
	if (g_NetDeferredDeleteCount >= NET_DEFER_DELETE_MAX) {
		netdbgEvent("warn",
				"\"action\":\"defer_delete_reap_full\",\"syncid\":%u,\"q\":%d",
				(unsigned)prop->syncid, g_NetDeferredDeleteCount);
		netReapDeferredDeletes();
	}
	if (g_NetDeferredDeleteCount < NET_DEFER_DELETE_MAX) {
		g_NetDeferredDeletes[g_NetDeferredDeleteCount++] = prop;
		netdbgEvent("client",
				"\"action\":\"defer_delete\",\"syncid\":%u,\"type\":%u,\"q\":%d",
				(unsigned)prop->syncid, (unsigned)prop->type, g_NetDeferredDeleteCount);
	} else if (prop->obj) {
		netdbgEvent("warn",
				"\"action\":\"defer_delete_force_free\",\"syncid\":%u",
				(unsigned)prop->syncid);
		propDeregisterRooms(prop);
		if (prop->active || prop == g_Vars.activeprops || prop->next || prop->prev) {
			netPropForceDelist(prop);
		}
		propDisable(prop);
		objFree(prop->obj, true, prop->obj->hidden2 & OBJH2FLAG_CANREGEN);
	}
}

void netReapDeferredDeletes(void)
{
	const s32 n = g_NetDeferredDeleteCount;

	if (n > 0) {
		netdbgEvent("client", "\"action\":\"reap_deferred\",\"count\":%d", n);
	}

	for (s32 i = 0; i < g_NetDeferredDeleteCount; i++) {
		struct prop *prop = g_NetDeferredDeletes[i];
		struct defaultobj *obj;
		g_NetDeferredDeletes[i] = NULL;

		if (!prop || !propIsInPool(prop) || prop->freelinked) {
			continue;
		}
		obj = prop->obj;
		if (!obj || obj->prop != prop) {
			continue;
		}
		if ((obj->hidden & OBJHFLAG_DELETING) == 0) {
			continue;
		}

		/* Drop wallhit heads before objFree — recycled/corrupt lists AVs. */
		prop->opawallhits = NULL;
		prop->xluwallhits = NULL;

		propDeregisterRooms(prop);
		if (prop->active || prop == g_Vars.activeprops || prop->next || prop->prev) {
			netPropForceDelist(prop);
		}
		propDisable(prop);
		objFree(obj, true, obj->hidden2 & OBJH2FLAG_CANREGEN);
	}
	g_NetDeferredDeleteCount = 0;
}

static void netClientSimClearHeldWeapons(struct chrdata *chr)
{
	for (s32 i = 0; i < 2; i++) {
		struct prop *wprop = chr->weapons_held[i];

		if (!wprop) {
			continue;
		}

		chr->weapons_held[i] = NULL;
		netDeferPropDelete(wprop);
	}
}

static void netClientSimEquipWeapon(struct chrdata *chr, struct netsimsnap *snap)
{
	struct aibot *aibot = chr->aibot;
	s32 weaponnum = snap->weaponnum;
	s32 gunfunc = snap->gunfunc;

	if (weaponnum == WEAPON_NONE) {
		weaponnum = WEAPON_UNARMED;
	}
	if (weaponnum > WEAPON_RCP45) {
		weaponnum = WEAPON_UNARMED;
	}

	aibot->changeguntimer60 = 0;
	aibot->gunfunc = gunfunc;
	aibot->weaponnum = weaponnum;

	if (weaponnum == WEAPON_UNARMED) {
		/* Held guns cleared+reaped in FlushEquips phase 1. */
		aibot->ismeleeweapon = true;
		return;
	}

	if (!botinvGetItem(chr, weaponnum)) {
		botinvGiveSingleWeapon(chr, weaponnum);
	}
	if (snap->dualweapons) {
		botinvGiveDualWeapon(chr, weaponnum);
	}

	struct invitem *item = botinvGetItem(chr, weaponnum);
	const s32 modelnum = playermgrGetModelOfWeapon(weaponnum);
	struct prop *rprop = chr->weapons_held[HAND_RIGHT];
	struct weaponobj *rheld = (rprop && rprop->weapon) ? rprop->weapon : NULL;
	const bool wantLeft = snap->dualweapons != 0;
	const bool haveLeft = chr->weapons_held[HAND_LEFT] != NULL;
	const bool haveRight = rheld && rheld->weaponnum == (u8)weaponnum;

	if (modelnum < 0 || !item) {
		return;
	}

	if (!haveRight || wantLeft != haveLeft) {
		/* Old guns already cleared+reaped in FlushEquips phase 1. */
		chrGiveWeapon(chr, modelnum, weaponnum, 0);
		botactReload(chr, HAND_RIGHT, false);
		if (wantLeft || item->type == INVITEMTYPE_DUAL) {
			chrGiveWeapon(chr, modelnum, weaponnum, OBJFLAG_WEAPON_LEFTHANDED);
			botactReload(chr, HAND_LEFT, false);
		}
	}

	struct weaponfunc *func = weaponGetFunctionById(weaponnum, gunfunc);
	aibot->ismeleeweapon = func && func->type == INVENTORYFUNCTYPE_MELEE;

	for (s32 i = 0; i < 2; i++) {
		if (chr->weapons_held[i] && chr->weapons_held[i]->weapon
				&& chr->weapons_held[i]->obj) {
			chr->weapons_held[i]->weapon->gunfunc = (u8)gunfunc;
			chr->weapons_held[i]->obj->flags &= ~OBJFLAG_WEAPON_AICANNOTUSE;
		}
	}
}

static f32 netSimLerpAngle(f32 from, f32 to, f32 t)
{
	f32 d;
	f32 out;
	s32 safety;

	if (!(from == from) || !(to == to) || !(t == t)) {
		return 0.0f;
	}
	d = to - from;
	safety = 16;
	while (d > M_PI && --safety >= 0) {
		d -= M_BADTAU;
	}
	safety = 16;
	while (d < -M_PI && --safety >= 0) {
		d += M_BADTAU;
	}
	out = from + d * t;
	safety = 16;
	while (out >= M_BADTAU && --safety >= 0) {
		out -= M_BADTAU;
	}
	safety = 16;
	while (out < 0 && --safety >= 0) {
		out += M_BADTAU;
	}
	return out;
}

static void netClientSimApplyAnim(struct chrdata *chr, struct netsimsnap *snap)
{
	struct model *model = chr->model;

	if (!model || !model->anim) {
		return;
	}

	// anim_id 0 means "keep current" — never leave the model without a pose.
	if (snap->anim_id == 0) {
		return;
	}
	/* Reject garbage anim indices (corrupt snap → AV in modelSetAnimation). */
	if (!animHasFrames(snap->anim_id)) {
		return;
	}

	const s16 cur = modelGetAnimNum(model);
	const bool flipped = modelIsFlipped(model);
	const bool changed = (cur != snap->anim_id) || (flipped != (snap->anim_flip != 0));

	if (changed) {
		// Merge like local TP anim switches — merge=0 was the glitchy pop.
		modelSetAnimation(model, snap->anim_id, snap->anim_flip != 0, snap->anim_frame, snap->anim_speed, 16);
		if (snap->anim_looping) {
			modelSetAnimLooping(model, snap->anim_loopframe, 16);
		}
	} else {
		// Let local ticks advance frames; only resync if we drift badly.
		if (snap->dirty) {
			const f32 curframe = modelGetCurAnimFrame(model);
			f32 drift = snap->anim_frame - curframe;
			if (drift < 0) {
				drift = -drift;
			}
			if (drift > 3.0f) {
				modelSetAnimFrame(model, snap->anim_frame);
			}
			if (modelGetAnimSpeed(model) != snap->anim_speed) {
				modelSetAnimSpeed(model, snap->anim_speed, 4);
			}
			if (snap->anim_looping && (!model->anim->looping || model->anim->loopframe != snap->anim_loopframe)) {
				modelSetAnimLooping(model, snap->anim_loopframe, 16);
			}
		}
	}
}

void netClientSimPrepare(struct chrdata *chr)
{
	if (!chr || !chr->aibot) {
		return;
	}

	if (!netClientSimShouldHoldPresentation(chr)) {
		return;
	}

	const s32 slot = netSimSlotForChr(chr);
	if (slot < 0 || !g_NetSimSnapshot[slot].valid) {
		netClientSimHideUntilPose(chr);
		return;
	}

	struct netsimsnap *snap = &g_NetSimSnapshot[slot];

	/*
	 * Do not ClearHeldWeapons / chrGiveWeapon here — that defer_deletes mid
	 * propsTickPlayer walk and punches activeprops holes. Latch for flush
	 * at propsTick start (before the list walk). Only when the snap weapon
	 * differs from what we last applied (or hands are empty) — otherwise we
	 * churn syncid=0 gun props every frame and corrupt freeprops.
	 */
	if (slot >= 0 && slot < MAX_BOTS) {
		u8 wantw = snap->weaponnum;
		u8 wantd = snap->dualweapons ? 1 : 0;
		u8 need = 0;

		if (wantw == WEAPON_NONE || wantw > WEAPON_RCP45) {
			wantw = WEAPON_UNARMED;
		}
		if (!g_NetSimEquipAppliedValid[slot]
				|| g_NetSimEquipAppliedWeapon[slot] != wantw
				|| g_NetSimEquipAppliedDual[slot] != wantd) {
			need = 1;
		} else if (wantw != WEAPON_UNARMED && !chr->weapons_held[HAND_RIGHT]) {
			need = 1;
		} else if (wantw == WEAPON_UNARMED
				&& (chr->weapons_held[HAND_RIGHT] || chr->weapons_held[HAND_LEFT])) {
			need = 1;
		}
		if (need) {
			g_NetSimEquipPending[slot] = 1;
		}
	}

	// No chrTick AI action handlers — presentation is applied after tick.
	chr->actiontype = ACT_BONDMULTI;
	chr->hidden &= ~CHRHFLAG_NEEDANIM;

	chr->aibot->lookangle = snap->look_yaw;
	chrSetLookAngle(chr, snap->look_yaw);
}

void netClientSimFlushEquips(void)
{
	s32 slot;
	u8 pending[MAX_BOTS];
	struct chrdata *chrs[MAX_BOTS];
	s32 any = 0;

	if (g_NetMode != NETMODE_CLIENT) {
		return;
	}

	/*
	 * Two-phase: defer-delete old held guns, reap them, THEN allocate replacements.
	 * Doing clear+chrGiveWeapon in one pass left DELETING props still in the pool
	 * while propAllocate ran → freeprops corruption / AV (reload-after-menu path).
	 */
	for (slot = 0; slot < MAX_BOTS; slot++) {
		pending[slot] = 0;
		chrs[slot] = NULL;

		if (!g_NetSimEquipPending[slot] || !g_NetSimSnapshot[slot].valid) {
			g_NetSimEquipPending[slot] = 0;
			continue;
		}
		g_NetSimEquipPending[slot] = 0;

		{
			s32 i;
			struct chrdata *chr = NULL;

			for (i = 0; i < g_MpNumChrs; i++) {
				struct chrdata *c = g_MpAllChrPtrs[i];

				if (c && c->aibot && netSimSlotForChr(c) == slot) {
					chr = c;
					break;
				}
			}
			if (!chr || chrIsDead(chr)) {
				continue;
			}
			chrs[slot] = chr;
			pending[slot] = 1;
			any = 1;

			/* Pre-clear when equip will replace held guns (same predicates as equip). */
			{
				struct netsimsnap *snap = &g_NetSimSnapshot[slot];
				s32 weaponnum = snap->weaponnum;
				struct prop *rprop;
				struct weaponobj *rheld;
				bool wantLeft;
				bool haveLeft;
				bool haveRight;

				if (weaponnum == WEAPON_NONE || weaponnum > WEAPON_RCP45) {
					weaponnum = WEAPON_UNARMED;
				}
				if (weaponnum == WEAPON_UNARMED) {
					if (chr->weapons_held[HAND_RIGHT] || chr->weapons_held[HAND_LEFT]) {
						netClientSimClearHeldWeapons(chr);
					}
					continue;
				}

				rprop = chr->weapons_held[HAND_RIGHT];
				rheld = (rprop && rprop->weapon) ? rprop->weapon : NULL;
				wantLeft = snap->dualweapons != 0;
				haveLeft = chr->weapons_held[HAND_LEFT] != NULL;
				haveRight = rheld && rheld->weaponnum == (u8)weaponnum;
				if (!haveRight || wantLeft != haveLeft) {
					netClientSimClearHeldWeapons(chr);
				}
			}
		}
	}

	if (any) {
		netReapDeferredDeletes();
	}

	for (slot = 0; slot < MAX_BOTS; slot++) {
		struct netsimsnap *snap;
		u8 wantw;
		u8 wantd;

		if (!pending[slot] || !chrs[slot] || chrIsDead(chrs[slot])) {
			continue;
		}
		snap = &g_NetSimSnapshot[slot];
		netClientSimEquipWeapon(chrs[slot], snap);

		wantw = snap->weaponnum;
		wantd = snap->dualweapons ? 1 : 0;
		if (wantw == WEAPON_NONE || wantw > WEAPON_RCP45) {
			wantw = WEAPON_UNARMED;
		}
		g_NetSimEquipAppliedWeapon[slot] = wantw;
		g_NetSimEquipAppliedDual[slot] = wantd;
		g_NetSimEquipAppliedValid[slot] = 1;
	}
}

void netClientSimSyncGround(struct chrdata *chr, struct netsimsnap *snap)
{
	f32 ground;
	struct coord probe;
	RoomNum rooms[8];

	if (!chr || !chr->prop || !snap) {
		return;
	}

	roomsCopy(snap->rooms, rooms);
	/* Reject garbage room lists before propRegisterRooms / cdFindGround. */
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
	if (!netSimPropRoomsEqual(rooms, chr->prop->rooms)) {
		if (chr->prop->active) {
			propDeregisterRooms(chr->prop);
		}
		roomsCopy(rooms, chr->prop->rooms);
		if (chr->prop->active) {
			propRegisterRooms(chr->prop);
		}
	}

	// Same probe offset as chrAllocate — find the real floor under host XZ.
	probe.x = snap->pos.x;
	probe.y = snap->pos.y + 100.0f;
	probe.z = snap->pos.z;
	if (rooms[0] < 0) {
		ground = snap->pos.y;
	} else {
		ground = cdFindGroundInfoAtCyl(&probe, chr->radius, chr->prop->rooms,
				&chr->floorcol, &chr->floortype, NULL, &chr->floorroom, NULL, NULL);
		if (ground < -1000000.0f) {
			ground = snap->pos.y;
		}
	}

	chr->fallspeed.x = 0.0f;
	chr->fallspeed.y = 0.0f;
	chr->fallspeed.z = 0.0f;
	chr->ground = ground;
	chr->manground = ground;
	chr->sumground = ground * (PAL ? 8.417509f : 9.999998f);

	// Trust host Y only when clearly airborne (jump/fall); otherwise plant.
	chr->prop->pos.x = snap->pos.x;
	chr->prop->pos.z = snap->pos.z;
	if (snap->pos.y > ground + 50.0f) {
		chr->prop->pos.y = snap->pos.y;
	} else {
		chr->prop->pos.y = ground;
	}

	if (chr->model && chr->model->definition
			&& (chr->model->definition->rootnode->type & 0xff) == MODELNODETYPE_CHRINFO) {
		union modelrwdata *rwdata = modelGetNodeRwData(chr->model, chr->model->definition->rootnode);
		rwdata->chrinfo.ground = ground;
		// Drop stale anim-relative Y so modelUpdateChrInfo can't re-float us.
		if (!(snap->pos.y > ground + 50.0f)) {
			rwdata->chrinfo.unk34.y = 0.0f;
			rwdata->chrinfo.unk24.y = 0.0f;
			rwdata->chrinfo.unk40.y = 0.0f;
			rwdata->chrinfo.unk4c.y = 0.0f;
		}
	}
}

void netClientSimApply(struct chrdata *chr)
{
	if (!chr || !chr->prop) {
		return;
	}

	if (!netClientSimShouldHoldPresentation(chr)) {
		return;
	}

	const s32 slot = netSimSlotForChr(chr);
	if (slot < 0 || !g_NetSimSnapshot[slot].valid) {
		netClientSimHideUntilPose(chr);
		return;
	}

	struct netsimsnap *snap = &g_NetSimSnapshot[slot];

	netClientSimSyncGround(chr, snap);

	if (chr->model) {
		struct coord rootpos;

		// Full XYZ — leaving Y stale parked the mesh underground while
		// muzzle FX (child props) still fired from the held gun.
		rootpos.x = chr->prop->pos.x;
		rootpos.y = chr->prop->pos.y;
		rootpos.z = chr->prop->pos.z;
		modelSetRootPosition(chr->model, &rootpos);

		// Soft-turn toward host yaw (hard snaps looked robotic).
		{
			const f32 cur_yaw = modelGetChrRotY(chr->model);
			const f32 t = snap->dirty ? 0.55f : 0.25f;
			modelSetChrRotY(chr->model, netSimLerpAngle(cur_yaw, snap->model_yaw, t));
		}
		netClientSimApplyAnim(chr, snap);

		// chrTick already ran this frame — rebuild matrices at the synced pose
		// so the body culls/renders/hits where the host says it is.
		if (chr->model->anim) {
			chr->model->anim->average = false;
		}
		modelUpdateInfo(chr->model);
	}

	if (!(chr->prop->flags & PROPFLAG_ENABLED)) {
		propEnable(chr->prop);
	}
	chr->chrflags &= ~CHRCFLAG_HIDDEN;

	if (chr->aibot) {
		chr->aibot->lookangle = snap->look_yaw;
		chrSetLookAngle(chr, snap->look_yaw);
		netClientSimResetLivingAlpha(chr);
	}

	// Shield must match host so CHR_DAMAGE can spawn shieldhit FX.
	chr->cshield = snap->cshield;

	// Upper-body aim — ease toward host (instant assign was snappy).
	{
		const f32 t = snap->dirty ? 0.5f : 0.2f;
		chr->hidden2 |= CHRH2FLAG_AUTOANIM;
		chr->aimendback = snap->aim_pitch;
		chr->aimendsideback = snap->aim_yaw;
		chr->aimendrshoulder = 0;
		chr->aimendlshoulder = 0;
		chr->aimupback += (snap->aim_pitch - chr->aimupback) * t;
		chr->aimsideback += (snap->aim_yaw - chr->aimsideback) * t;
		chr->aimendcount = 10;
	}

	// Muzzle flash: latch like remote players so one-tick host flashes stay visible.
	// Use FLASH bits only — FIRING alone includes melee/punch and must not light gunfire.
	{
		const u8 flashbits = snap->gunflags & (NETGUN_FLASH_L | NETGUN_FLASH_R);
		u8 want_r = (flashbits & NETGUN_FLASH_R) != 0;
		u8 want_l = (flashbits & NETGUN_FLASH_L) != 0;

		if (want_r) {
			g_NetSimFlashLatch[slot][HAND_RIGHT] = 5;
		} else if (g_NetSimFlashLatch[slot][HAND_RIGHT] > 0) {
			g_NetSimFlashLatch[slot][HAND_RIGHT]--;
		}
		if (want_l) {
			g_NetSimFlashLatch[slot][HAND_LEFT] = 5;
		} else if (g_NetSimFlashLatch[slot][HAND_LEFT] > 0) {
			g_NetSimFlashLatch[slot][HAND_LEFT]--;
		}

		chrSetFiring(chr, HAND_RIGHT, g_NetSimFlashLatch[slot][HAND_RIGHT] > 0);
		chrSetFiring(chr, HAND_LEFT, g_NetSimFlashLatch[slot][HAND_LEFT] > 0);

		// Clients never run host bot gun audio — edge-trigger TP shots from flash.
		for (s32 h = 0; h < 2; ++h) {
			const u8 bit = (h == HAND_LEFT) ? NETGUN_FLASH_L : NETGUN_FLASH_R;
			if ((flashbits & bit) && !(g_NetSimGunSoundFlags[slot] & bit)) {
				struct prop *wprop = chrGetHeldProp(chr, h);
				if (wprop && wprop->weapon) {
					netPlayPositionalGunShot(chr->prop, &wprop->weapon->gset);
				}
			}
		}
		g_NetSimGunSoundFlags[slot] = flashbits;
	}

	if (snap->dirty) {
		chr->prop->forcetick = true;
	}

	snap->dirty = 0;
}

void netClientSimApplyPose(struct chrdata *chr)
{
	if (!chr || !chr->prop || !chr->aibot) {
		return;
	}

	if (!netClientSimShouldHoldPresentation(chr)) {
		return;
	}

	const s32 slot = netSimSlotForChr(chr);
	if (slot < 0 || !g_NetSimSnapshot[slot].valid) {
		return;
	}

	struct netsimsnap *snap = &g_NetSimSnapshot[slot];

	netClientSimSyncGround(chr, snap);

	if (chr->model) {
		struct coord rootpos;
		rootpos.x = chr->prop->pos.x;
		rootpos.y = chr->prop->pos.y;
		rootpos.z = chr->prop->pos.z;
		modelSetRootPosition(chr->model, &rootpos);
		// Re-apply host anim — local fall/jump anims must not stick after tick.
		netClientSimApplyAnim(chr, snap);
		if (chr->model->anim) {
			chr->model->anim->average = false;
		}
		modelUpdateInfo(chr->model);
	}
}

u32 g_NetMpSetupSeq = 0;            // last applied / last broadcast authoritative seq
static u32 g_NetMpSetupSeqOut = 0;   // server's next seq to assign
static u32 g_NetMpSetupLastSendTick = 0;

static void netMpSetupCaptureShadow(void)
{
	g_NetMpSetupShadow.stagenum = g_MpSetup.stagenum;
	g_NetMpSetupShadow.scenario = g_MpSetup.scenario;
	g_NetMpSetupShadow.scorelimit = g_MpSetup.scorelimit;
	g_NetMpSetupShadow.timelimit = g_MpSetup.timelimit;
	g_NetMpSetupShadow.teamscorelimit = g_MpSetup.teamscorelimit;
	g_NetMpSetupShadow.options = g_MpSetup.options;
	memcpy(g_NetMpSetupShadow.weapons, g_MpSetup.weapons, sizeof(g_NetMpSetupShadow.weapons));
	g_NetMpSetupShadow.weaponset = (u8)g_MpWeaponSetNum;
	g_NetMpSetupShadow.locktype = (u8)g_BossFile.locktype;
	g_NetMpSetupShadow.challengeindex = (u8)g_MpChallengeIndex;
	g_NetMpSetupShadow.mphilltime = (u8)g_Vars.mphilltime;
	strncpy(g_NetMpSetupShadow.name, g_MpSetup.name, sizeof(g_NetMpSetupShadow.name) - 1);
	g_NetMpSetupShadow.name[sizeof(g_NetMpSetupShadow.name) - 1] = '\0';
	g_NetMpSetupShadow.simslots = g_MpSetup.chrslots & 0xff00;
	g_NetMpSetupShadow.humanslots = g_MpSetup.chrslots & 0x00ff;
	for (s32 i = 0; i < MAX_PLAYERS; ++i) {
		g_NetMpSetupShadow.playerteams[i] = g_PlayerConfigsArray[i].base.team;
	}
	memcpy(g_NetMpSetupShadow.bots, g_BotConfigsArray, sizeof(g_NetMpSetupShadow.bots));
	g_NetMpSetupShadowValid = 1;
}

static s32 netMpSetupIsDirty(void)
{
	if (!g_NetMpSetupShadowValid) {
		return false; // wait until we have a baseline (auth / first apply)
	}
	if (g_MpSetup.stagenum != g_NetMpSetupShadow.stagenum
			|| g_MpSetup.scenario != g_NetMpSetupShadow.scenario
			|| g_MpSetup.scorelimit != g_NetMpSetupShadow.scorelimit
			|| g_MpSetup.timelimit != g_NetMpSetupShadow.timelimit
			|| g_MpSetup.teamscorelimit != g_NetMpSetupShadow.teamscorelimit
			|| g_MpSetup.options != g_NetMpSetupShadow.options
			|| g_MpWeaponSetNum != g_NetMpSetupShadow.weaponset
			|| g_BossFile.locktype != g_NetMpSetupShadow.locktype
			|| g_MpChallengeIndex != g_NetMpSetupShadow.challengeindex
			|| (u8)g_Vars.mphilltime != g_NetMpSetupShadow.mphilltime
			|| memcmp(g_MpSetup.weapons, g_NetMpSetupShadow.weapons, sizeof(g_MpSetup.weapons)) != 0
			|| strncmp(g_MpSetup.name, g_NetMpSetupShadow.name, sizeof(g_MpSetup.name)) != 0
			|| (g_MpSetup.chrslots & 0xff00) != g_NetMpSetupShadow.simslots
			|| (g_MpSetup.chrslots & 0x00ff) != g_NetMpSetupShadow.humanslots
			|| memcmp(g_BotConfigsArray, g_NetMpSetupShadow.bots, sizeof(g_BotConfigsArray)) != 0) {
		return true;
	}
	for (s32 i = 0; i < MAX_PLAYERS; ++i) {
		if (g_PlayerConfigsArray[i].base.team != g_NetMpSetupShadow.playerteams[i]) {
			return true;
		}
	}
	return false;
}

static void netMpSetupApplyLock(u8 locktype, u8 challengeindex, s32 fromclient)
{
	const u8 prevlock = (u8)g_BossFile.locktype;

	// Only the host owns Challenges / Quick Start mode switches.
	if (fromclient) {
		return;
	}

	if (locktype == MPLOCKTYPE_CHALLENGE) {
		const s32 challengechanged = (prevlock != MPLOCKTYPE_CHALLENGE)
				|| (g_MpChallengeIndex != challengeindex);

		g_MpChallengeIndex = challengeindex;
		if (prevlock != MPLOCKTYPE_CHALLENGE) {
			mpSetLock(MPLOCKTYPE_CHALLENGE, 5);
		}
		/*
		 * MpSetup sync flattens bot difficulty to the live value. Restore the
		 * full 1P–4P ladder from the catalog so sanity scales when humans join.
		 */
		if (challengechanged || g_NetMode == NETMODE_CLIENT) {
			challengeSyncSimulantLadder();
			challengePerformSanityChecks();
		}
	} else if (prevlock == MPLOCKTYPE_CHALLENGE) {
		/* Unlock without re-broadcasting — caller already owns the MpSetup sync. */
		challengeUnsetCurrent();
		mpSetLock(locktype == MPLOCKTYPE_NONE ? MPLOCKTYPE_NONE : locktype, 0);
	} else if (locktype != prevlock) {
		mpSetLock(locktype, 0);
	}

	// Non-hosts never see the Challenges editor — keep them on Game Setup.
	if (g_NetMode == NETMODE_CLIENT && g_NetAllowSetupChanges && prevlock != g_BossFile.locktype) {
		extern struct menudialogdef g_MpAdvancedSetupMenuDialog;
		const s32 prevplayer = g_MpPlayerNum;
		g_MpPlayerNum = 0;
		if (g_Menus[0].curdialog) {
			func0f0f820c(&g_MpAdvancedSetupMenuDialog, MENUROOT_MPSETUP);
		}
		g_MpPlayerNum = prevplayer;
	}
}

u32 netMpSetupReadSeq(struct netbuf *src)
{
	return netbufReadU32(src);
}

void netMpSetupSend(void)
{
	if (!g_NetMode || !g_NetLocalClient || g_NetApplyingMpSetup) {
		return;
	}
	if (g_NetLocalClient->state != CLSTATE_LOBBY) {
		return;
	}

	g_NetMpSetupLastSendTick = g_NetTick;

	netbufStartWrite(&g_NetMsgRel);
	if (g_NetMode == NETMODE_SERVER) {
		// Ensure roster names/slots match live connections before broadcast.
		netLobbyRebuildHumanSlots();
		netMpSetupCaptureShadow();
		g_NetMpSetupSeqOut++;
		g_NetMpSetupSeq = g_NetMpSetupSeqOut;
		netmsgSvcMpSetupWrite(&g_NetMsgRel);
	} else if (g_NetAllowSetupChanges) {
		netMpSetupCaptureShadow();
		netmsgClcMpSetupWrite(&g_NetMsgRel);
	} else {
		return;
	}
	netSend(NULL, &g_NetMsgRel, true, NETCHAN_CONTROL);
}

void netMpSetupTick(void)
{
	if (!g_NetMode || !g_NetLocalClient) {
		return;
	}
	if (g_NetLocalClient->state != CLSTATE_LOBBY) {
		g_NetMpSetupShadowValid = 0;
		g_NetMpSetupSeq = 0;
		return;
	}
	if (g_StageNum != STAGE_CITRAINING && g_StageNum != STAGE_4MBMENU) {
		return;
	}
	if (g_NetApplyingMpSetup) {
		return;
	}
	if (g_NetMode == NETMODE_CLIENT && !g_NetAllowSetupChanges) {
		return;
	}
	// Establish a baseline once so the first lobby frame isn't treated as an edit.
	if (!g_NetMpSetupShadowValid) {
		netMpSetupCaptureShadow();
		return;
	}
	// Only push when the user actually changed a setup field — not every frame.
	if (!netMpSetupIsDirty()) {
		return;
	}
	// Light debounce so slider drags don't flood; still feels instant.
	if (g_NetMpSetupLastSendTick && g_NetTick < g_NetMpSetupLastSendTick + 2) {
		return;
	}
	netMpSetupSend();
}

void netMpSetupApply(struct netbuf *src, struct netclient *srccl)
{
	const u32 seq = netMpSetupReadSeq(src);
	const u8 stagenum = netbufReadU8(src);
	const u8 scenario = netbufReadU8(src);
	const u8 scorelimit = netbufReadU8(src);
	const u8 timelimit = netbufReadU8(src);
	const u16 teamscorelimit = netbufReadU16(src);
	const u16 chrslots = netbufReadU16(src);
	const u32 options = netbufReadU32(src);
	u8 weapons[NUM_MPWEAPONSLOTS];
	netbufReadData(src, weapons, sizeof(weapons));
	const u8 weaponset = netbufReadU8(src);
	const u8 locktype = netbufReadU8(src);
	const u8 challengeindex = netbufReadU8(src);
	const u8 mphilltime = netbufReadU8(src);
	char *name = netbufReadStr(src);

	if (src->error) {
		sysLogPrintf(LOG_WARNING, "NET: malformed MPSETUP");
		return;
	}

	// Drop stale/out-of-order updates so crossed host/client edits can't oscillate.
	// Seq 0 is used by CLC (client proposals); server always assigns a real seq on SVC.
	// Header/name already consumed — must still drain simulant/player blocks or the
	// multiplexed packet desyncs (next byte becomes a fake msgid).
	if (seq != 0 && seq <= g_NetMpSetupSeq) {
		netMpSimulantsSkip(src);
		netMpPlayersSkip(src);
		return;
	}

	g_NetApplyingMpSetup = 1;

	const s32 clientProposal = (g_NetMode == NETMODE_SERVER && seq == 0 && srccl != NULL);
	const u8 prevscenario = g_MpSetup.scenario;

	// Preserve live player occupancy bits; host rebuilds those from connections.
	const u16 savedplayers = g_MpSetup.chrslots & 0x00ff;
	g_MpSetup.stagenum = stagenum;
	g_MpSetup.scenario = scenario;
	g_MpSetup.scorelimit = scorelimit;
	g_MpSetup.timelimit = timelimit;
	g_MpSetup.teamscorelimit = teamscorelimit;
	g_MpSetup.options = options;
	memcpy(g_MpSetup.weapons, weapons, sizeof(g_MpSetup.weapons));
	if (g_NetMode == NETMODE_SERVER) {
		// Accept simulant slot bits from anyone with setup access; keep humans.
		g_MpSetup.chrslots = (chrslots & 0xff00) | savedplayers;
	} else {
		g_MpSetup.chrslots = chrslots;
	}
	if (name) {
		strncpy(g_MpSetup.name, name, sizeof(g_MpSetup.name) - 1);
		g_MpSetup.name[sizeof(g_MpSetup.name) - 1] = '\0';
	}
	g_MpWeaponSetNum = weaponset;
	g_Vars.mphilltime = mphilltime;

	/* Keep lobby scenario data (teams bit, cleared pads) aligned with host. */
	if (scenario != prevscenario) {
		scenarioInit();
		/* scenarioInit may OR TEAMSENABLED — preserve host options after. */
		g_MpSetup.options = options;
		if (scenario == MPSCENARIO_KINGOFTHEHILL
				|| scenario == MPSCENARIO_CAPTURETHECASE) {
			g_MpSetup.options |= MPOPTION_TEAMSENABLED;
		}
	}

	// seq 0 = client proposal: never let clients change Challenges / lock mode
	netMpSetupApplyLock(locktype, challengeindex, seq == 0 && g_NetMode == NETMODE_SERVER);

	// Clients with setup access may add/edit sims.
	netMpSimulantsApply(src);

	// Always apply lobby roster (teams/names). On the host after a client
	// proposal, rebuild occupancy from live connections but keep the teams.
	netMpPlayersApply(src);
	if (clientProposal) {
		u8 teams[MAX_PLAYERS];
		s32 i;
		for (i = 0; i < MAX_PLAYERS; i++) {
			teams[i] = g_PlayerConfigsArray[i].base.team;
		}
		netLobbyRebuildHumanSlots();
		for (i = 0; i < MAX_PLAYERS; i++) {
			if (g_MpSetup.chrslots & (1 << i)) {
				g_PlayerConfigsArray[i].base.team = teams[i];
				struct netclient *cl = netClientForPlayerNum(i);
				if (cl) {
					cl->settings.team = teams[i];
				}
			}
		}
	}

	netMpSetupCaptureShadow();
	if (seq != 0) {
		g_NetMpSetupSeq = seq;
		if (seq > g_NetMpSetupSeqOut) {
			g_NetMpSetupSeqOut = seq;
		}
	}

	g_NetApplyingMpSetup = 0;

	(void)srccl;
}

static u8 s_NetSimSeeded[MAX_BOTS];
static u8 s_NetSimLastWeapon[MAX_BOTS];
static u8 s_NetSimLastDual[MAX_BOTS];
static u32 s_LastScenarioSendFrame = 0;

static void netServerBroadcastSimMoves(void)
{
	if (g_NetMode != NETMODE_SERVER || !g_Vars.lvmpbotlevel) {
		return;
	}

	for (s32 slot = 0; slot < MAX_BOTS; ++slot) {
		if ((g_MpSetup.chrslots & (1 << (slot + MAX_PLAYERS))) == 0) {
			continue;
		}

		for (s32 i = 0; i < g_BotCount; ++i) {
			struct chrdata *chr = g_MpBotChrPtrs[i];
			u8 weaponnum;
			u8 dual;
			s32 needreliable;

			if (!chr || !chr->aibot || chr->aibot->config != &g_BotConfigsArray[slot]) {
				continue;
			}
			if (!chr->prop || chrIsDead(chr)) {
				// Allow a reliable reseed after respawn.
				if (chr && chrIsDead(chr)) {
					s_NetSimSeeded[slot] = 0;
				}
				break;
			}

			weaponnum = (u8)chr->aibot->weaponnum;
			dual = (chr->weapons_held[HAND_LEFT] != NULL) ? 1 : 0;
			/*
			 * Reliable keyframes: first living snap after spawn/respawn, and
			 * any weapon/dual change so clients never miss a gun swap on the
			 * unreliable pose stream.
			 */
			needreliable = !s_NetSimSeeded[slot]
					|| weaponnum != s_NetSimLastWeapon[slot]
					|| dual != s_NetSimLastDual[slot];

			if (needreliable && netbufWriteLeft(&g_NetMsgRel) >= 128) {
				netmsgSvcSimMoveWrite(&g_NetMsgRel, (u8)slot, chr);
				s_NetSimSeeded[slot] = 1;
				s_NetSimLastWeapon[slot] = weaponnum;
				s_NetSimLastDual[slot] = dual;
			} else {
				/* Unreliable pose, or rel buffer full (retry keyframe next tick). */
				netmsgSvcSimMoveWrite(&g_NetMsg, (u8)slot, chr);
			}
			break;
		}
	}
}

void netServerBroadcastScenario(void)
{
	if (g_NetMode != NETMODE_SERVER || !g_Vars.normmplayerisrunning) {
		return;
	}

	if (g_MpSetup.scenario == MPSCENARIO_COMBAT) {
		return;
	}

	// ~4 Hz at 60fps; scenarioTick already hosts-only so this is the presentation push
	if (g_Vars.lvframenum - s_LastScenarioSendFrame < 15
			&& s_LastScenarioSendFrame != 0) {
		return;
	}

	s_LastScenarioSendFrame = g_Vars.lvframenum;

	if (netbufWriteLeft(&g_NetMsgRel) < 256) {
		return;
	}

	netmsgSvcScenarioWrite(&g_NetMsgRel);
}

void netServerBroadcastScenarioImmediate(void)
{
	if (g_NetMode != NETMODE_SERVER || !g_Vars.normmplayerisrunning) {
		return;
	}

	if (g_MpSetup.scenario == MPSCENARIO_COMBAT) {
		return;
	}

	if (netbufWriteLeft(&g_NetMsgRel) < 256) {
		return;
	}

	s_LastScenarioSendFrame = g_Vars.lvframenum;
	netmsgSvcScenarioWrite(&g_NetMsgRel);
}

s32 netMpChrIndexForPlayernum(s32 playernum)
{
	s32 i;

	if (g_NetMode != NETMODE_CLIENT || playernum < 0 || playernum >= PLAYERCOUNT()) {
		return playernum;
	}

	struct player *pl = g_Vars.players[playernum];
	if (pl && pl->client) {
		// netPlayersAllocate swaps local ↔ host playernums. Recover the host's
		// ordering so g_MpAllChrPtrs / PAC victims match across peers.
		if (pl->client == g_NetLocalClient) {
			return (s32)g_NetClients[0].playernum;
		}
		if (pl->client->id == 0) {
			return 0;
		}
		return (s32)pl->client->playernum;
	}

	/* Fallback: scan netclients if player->client was cleared mid-frame. */
	if (!pl) {
		return -1;
	}
	for (i = 0; i < g_NetMaxClients; i++) {
		struct netclient *cl = &g_NetClients[i];
		if (cl->state < CLSTATE_GAME || cl->player != pl) {
			continue;
		}
		if (cl == g_NetLocalClient) {
			return (s32)g_NetClients[0].playernum;
		}
		if (cl->id == 0) {
			return 0;
		}
		return (s32)cl->playernum;
	}

	return -1;
}

s32 netPlayernumForMpChrIndex(s32 mpchr)
{
	s32 i;

	if (g_NetMode != NETMODE_CLIENT || mpchr < 0 || mpchr >= PLAYERCOUNT()) {
		return mpchr;
	}

	for (i = 0; i < PLAYERCOUNT(); i++) {
		if (netMpChrIndexForPlayernum(i) == mpchr) {
			return i;
		}
	}

	/* Do not return mpchr — that aliases host slot 0 to local slot 0. */
	return -1;
}

static inline void netFlushSendBuffers(void)
{
	if (g_NetMsgRel.wp) {
		if (g_NetMsgRel.error) {
			sysLogPrintf(LOG_WARNING, "NET: reliable out buffer overflow");
		}
		g_NetReliableFrameLen += g_NetMsgRel.wp;
		netSend(NULL, &g_NetMsgRel, true, NETCHAN_DEFAULT);
	}

	if (g_NetMsg.wp) {
		if (g_NetMsg.error) {
			sysLogPrintf(LOG_WARNING, "NET: unreliable out buffer overflow");
		}
		g_NetUnreliableFrameLen += g_NetMsg.wp;
		netSend(NULL, &g_NetMsg, false, NETCHAN_DEFAULT);
	}
}

static inline const char *netGetDisconnectReason(const u32 reason)
{
	static const char *msgs[] = {
		"Unknown",
		"Server is shutting down",
		"Protocol or version mismatch",
		"Kicked by console",
		"You are banned on this server",
		"Connection timed out",
		"Server is full",
		"The game is already in progress",
		"Your files differ from the server's"
	};
	if (reason < (u32)ARRAYCOUNT(msgs)) {
		return msgs[reason];
	}
	return msgs[0];
}

static void netServerQueryResponse(ENetAddress *address)
{
	static u8 data[256];
	static ENetBuffer ebuf;
	struct netbuf buf = { .data = data, .size = sizeof(data) };
	const u8 flags = (g_NetLocalClient && g_NetLocalClient->state > CLSTATE_LOBBY)
		| (0 << 1); // TODO: this will indicate coop/anti/etc
	const char *modDir = fsGetModDir();
	if (!modDir) {
		modDir = "";
	}

	netbufStartWrite(&buf);
	netbufWriteData(&buf, NET_QUERY_MAGIC, sizeof(NET_QUERY_MAGIC) - 1);
	netbufWriteU16(&buf, 0); // space for size
	netbufWriteU32(&buf, NET_PROTOCOL_VER);
	netbufWriteU8(&buf, flags);
	netbufWriteU8(&buf, g_NetNumClients);
	netbufWriteU8(&buf, g_NetMaxClients);
	netbufWriteU8(&buf, g_StageNum);
	netbufWriteU8(&buf, g_MpSetup.scenario);
	netbufWriteStr(&buf, g_NetLocalClient ? g_NetLocalClient->settings.name : "");
	netbufWriteStr(&buf, g_RomName);
	netbufWriteStr(&buf, modDir);
	netbufWriteU16(&buf, 0); // space for checksum

	ebuf.data = buf.data;
	ebuf.dataLength = buf.wp;

	// rewrite size
	buf.wp = sizeof(NET_QUERY_MAGIC) - 1;
	netbufWriteU16(&buf, ebuf.dataLength);

	// calculate and rewrite checksum
	buf.wp = ebuf.dataLength - sizeof(u16);
	u16 crc = 0xFFFF;
	u16 x;
	for (u32 i = 0; i < buf.wp; ++i) {
		x = crc >> 8 ^ buf.data[i];
		x ^= x >> 4;
		crc += (crc << 8) ^ (x << 12) ^ (x << 5) ^ x;
	}
	netbufWriteU16(&buf, crc);

	enet_socket_send(g_NetHost->socket, address, &ebuf, 1);
}

static s32 netServerConnectionlessPacket(ENetEvent *event, ENetAddress *address, u8 *rxdata, s32 rxlen)
{
	if (rxdata && rxlen >= 5) {
		if (!memcmp(rxdata, NET_QUERY_MAGIC, sizeof(NET_QUERY_MAGIC) - 1)) {
			// this is a query packet, respond with server status
			sysLogPrintf(LOG_NOTE | LOGFLAG_NOCON, "NET: query request from %s, responding", netFormatAddr(address));
			netServerQueryResponse(address);
			return 1;
		}
	}
	// probably a normal packet, pass through to enet
	return 0;
}

struct netclient *netClientForPlayerNum(s32 playernum)
{
	s32 slot = 0;
	for (s32 i = 0; i < g_NetMaxClients; ++i) {
		struct netclient *cl = &g_NetClients[i];
		if (cl->state >= CLSTATE_LOBBY) {
			if (slot == playernum) {
				return cl;
			}
			++slot;
		}
	}
	return NULL;
}

void netInit(void)
{
	if (enet_initialize() < 0) {
		sysLogPrintf(LOG_ERROR, "NET: could not init ENet, disabling networking");
		return;
	}

	if (sysArgCheck("--debug-log")) {
		netdbgEnable();
	}

	const s32 argmaxclients = sysArgGetInt("--maxclients", -1);
	if (argmaxclients > 0 && argmaxclients <= NET_MAX_CLIENTS) {
		g_NetMaxClients = argmaxclients;
	}

	const s32 argport = sysArgGetInt("--port", -1);
	if (argport > 0 && argport < 0x10000) {
		g_NetServerPort = argport;
	}

	const char *argjoin = sysArgGetString("--connect");
	if (argjoin) {
		strncpy(g_NetLastJoinAddr, argjoin, sizeof(g_NetLastJoinAddr) - 1);
		g_NetJoinLatch = true;
	}

	if (sysArgCheck("--host")) {
		g_NetHostLatch = true;
	}

	g_NetInit = true;
}

s32 netStartServer(u16 port, s32 maxclients)
{
	if (g_NetMode || !g_NetInit) {
		return -1;
	}

	memset(&g_NetLocalAddr, 0, sizeof(g_NetLocalAddr));
	g_NetLocalAddr.port = port;
	g_NetHost = enet_host_create(&g_NetLocalAddr, maxclients, NETCHAN_COUNT, g_NetServerInRate, g_NetServerOutRate, 0);
	if (!g_NetHost) {
		sysLogPrintf(LOG_ERROR, "NET: could not create ENet host");
		return -2;
	}

	if (g_NetServerInfoQuery) {
		enet_host_set_intercept_callback(g_NetHost, netServerConnectionlessPacket);
	}

	netClientResetAll();
	g_NetMaxClients = maxclients;

	// the server's local client is client 0
	g_NetLocalClient = &g_NetClients[0];
	g_NetLocalClient->state = CLSTATE_LOBBY; // local client doesn't need auth
	netClientReadConfig(g_NetLocalClient, 0);

	g_NetMode = NETMODE_SERVER;
	challengeDetermineUnlockedFeatures();

	g_NetTick = 0;
	g_NetNextUpdate = 0;
	g_NetNextSyncId = 1;
	netSyncHashClear();

	netdbgEnable();
	netdbgEvent("net", "\"action\":\"host\",\"port\":%u,\"maxclients\":%d",
			(unsigned)port, maxclients);
	netdbgFlush();

	sysLogPrintf(LOG_NOTE, "NET: using protocol version %d", NET_PROTOCOL_VER);
	sysLogPrintf(LOG_NOTE, "NET: created server on port %u", port);

	return 0;
}

void netServerStageStart(void)
{
	if (g_NetMode != NETMODE_SERVER) {
		return;
	}

	if (g_StageNum == STAGE_TITLE || g_StageNum == STAGE_CITRAINING) {
		g_NetLocalClient->state = CLSTATE_LOBBY;
		return;
	}

	g_NetWaitingForPlayers = 0;
	g_NetAwaitingRejoin = 0;
	g_NetForceRespawn = 0;
	netLagCompClearAll();
	for (s32 i = 0; i < NET_MAX_CLIENTS + 1; ++i) {
		g_NetClients[i].endscreenready = 0;
		g_NetClients[i].leftmatch = 0;
	}

	// re-read the player config in case it changed
	netClientReadConfig(g_NetLocalClient, 0);

	g_NetLocalClient->state = CLSTATE_GAME;

	netbufStartWrite(&g_NetMsgRel);
	netmsgSvcStageStartWrite(&g_NetMsgRel);
	netSend(NULL, &g_NetMsgRel, true, NETCHAN_DEFAULT);
}

void netServerStageEnd(void)
{
	if (g_NetMode != NETMODE_SERVER) {
		return;
	}

	g_NetLocalClient->state = CLSTATE_LOBBY;

	netbufStartWrite(&g_NetMsgRel);
	netmsgSvcStageEndWrite(&g_NetMsgRel);
	netSend(NULL, &g_NetMsgRel, true, NETCHAN_DEFAULT);
}

void netServerKick(struct netclient *cl, const u32 reason)
{
	if (g_NetMode != NETMODE_SERVER) {
		return;
	}

	if (!cl || !cl->state || !cl->peer) {
		return;
	}

	enet_peer_disconnect(cl->peer, reason);
}

s32 netStartClient(const char *addr)
{
	if (g_NetMode || !g_NetInit) {
		return -1;
	}

	if (!netParseAddr(&g_NetRemoteAddr, addr)) {
		sysLogPrintf(LOG_ERROR, "NET: `%s` is not a valid address", addr);
		return -2;
	}

	memset(&g_NetLocalAddr, 0, sizeof(g_NetLocalAddr));
	g_NetHost = enet_host_create(&g_NetLocalAddr, 1, NETCHAN_COUNT, g_NetClientInRate, g_NetClientOutRate, 0);
	if (!g_NetHost) {
		sysLogPrintf(LOG_ERROR, "NET: could not create ENet host");
		return -3;
	}

	enet_host_set_intercept_callback(g_NetHost, NULL);

	// save the address since it appears to be valid
	strncpy(g_NetLastJoinAddr, addr, NET_MAX_ADDR);

	// we'll use the whole array to store what we know of other clients
	netClientResetAll();

	// for now use last client struct
	g_NetLocalClient = &g_NetClients[NET_MAX_CLIENTS];

	sysLogPrintf(LOG_NOTE, "NET: using protocol version %d", NET_PROTOCOL_VER);
	sysLogPrintf(LOG_NOTE, "NET: connecting to %s...", addr);

	g_NetLocalClient->peer = enet_host_connect(g_NetHost, &g_NetRemoteAddr, NETCHAN_COUNT, NET_PROTOCOL_VER);
	if (!g_NetLocalClient->peer) {
		sysLogPrintf(LOG_WARNING, "NET: could not connect to %s", addr);
		enet_host_destroy(g_NetHost);
		g_NetHost = NULL;
		return -4;
	}

	g_NetLocalClient->state = CLSTATE_CONNECTING;
	netClientReadConfig(g_NetLocalClient, 0);

	g_NetMode = NETMODE_CLIENT;
	challengeDetermineUnlockedFeatures();

	g_NetTick = 0;
	g_NetNextUpdate = 0;
	g_NetNextSyncId = 1;
	netSyncHashClear();

	netdbgEnable();
	netdbgEvent("net", "\"action\":\"connect\",\"addr\":\"%s\"", addr ? addr : "");
	netdbgFlush();

	sysLogPrintf(LOG_NOTE, "NET: waiting for response from %s...", addr);

	return 0;
}

s32 netDisconnect(void)
{
	if (!g_NetMode) {
		return -1;
	}

	netdbgEvent("net", "\"action\":\"disconnect\"");
	netdbgFlush();

	// stop responding to connectionless packets
	enet_host_set_intercept_callback(g_NetHost, NULL);

	const bool wasingame = (g_NetLocalClient->state >= CLSTATE_GAME);
	const bool wasserver = (g_NetMode == NETMODE_SERVER);

	for (s32 i = 0; i < NET_MAX_CLIENTS + 1; ++i) {
		if (g_NetClients[i].peer) {
			enet_peer_disconnect_now(g_NetClients[i].peer, DISCONNECT_SHUTDOWN);
		}
		netClientReset(&g_NetClients[i]);
	}

	g_NetLocalClient = &g_NetClients[NET_MAX_CLIENTS];

	// flush pending packets
	enet_host_flush(g_NetHost);

	// service for a bit just to ensure disconnect gets to peer(s)
	enet_host_service(g_NetHost, NULL, 10);

	enet_host_destroy(g_NetHost);

	g_NetHost = NULL;
	g_NetMode = NETMODE_NONE;
	challengeDetermineUnlockedFeatures();
	g_NetAllowSetupChanges = 0;
	g_NetApplyingMpSetup = 0;
	g_NetMpSetupShadowValid = 0;
	g_NetMpSetupSeq = 0;
	g_NetMpSetupSeqOut = 0;

	const s32 waswaiting = g_NetWaitingForPlayers || g_NetAwaitingRejoin;
	g_NetWaitingForPlayers = 0;
	g_NetAwaitingRejoin = 0;
	g_NetForceRespawn = 0;

	for (s32 i = 0; i < MAX_BOTS; ++i) {
		g_NetSimSnapshot[i].valid = 0;
		g_NetSimFlashLatch[i][0] = 0;
		g_NetSimFlashLatch[i][1] = 0;
		g_NetSimGunSoundFlags[i] = 0;
		g_NetSimEquipPending[i] = 0;
		g_NetSimEquipAppliedValid[i] = 0;
		s_NetSimSeeded[i] = 0;
		s_NetSimLastWeapon[i] = 0xff;
		s_NetSimLastDual[i] = 0xff;
	}
	netSyncHashClear();
	s_LastScenarioSendFrame = 0;

	/* Clear challenge lock for everyone — clients kept red MPLOCKABLE dialogs. */
	challengeUnsetCurrent();
	challengeRemovePlayerLock();

	g_ExplosionShakeTotalTimer = 0;
	g_ExplosionShakeIntensityTimer = 0;
	viShake(0);

	sysLogPrintf(LOG_CHAT, "NET: disconnected");
	netdbgShutdown();

	if (wasingame || waswaiting) {
		// skip the "want to save" dialog for all players
		for (s32 i = 0; i < MAX_PLAYERS; ++i) {
			if (g_Vars.players[i]) {
				g_PlayerConfigsArray[i].options |= OPTION_ASKEDSAVEPLAYER;
			}
		}
		/* Do not call mainEndStage() — it can push another endscreen while we
		 * are tearing down to CI. Just leave the match. */
		mpSetPaused(MPPAUSEMODE_UNPAUSED);
		g_MpSetup.chrslots = (g_MpSetup.chrslots & 0xff00) | 1;
		g_Vars.mplayerisrunning = false;
		g_Vars.normmplayerisrunning = false;
		g_Vars.lvmpbotlevel = 0;
		titleSetNextStage(STAGE_CITRAINING);
		setNumPlayers(1);
		titleSetNextMode(TITLEMODE_SKIP);
		mainChangeToStage(STAGE_CITRAINING);
		/* Always land on Network Game after a match/wait disconnect. */
		g_NetReturnToNetMenu = 1;
	}

	return 0;
}

static void netServerEvConnect(ENetPeer *peer, const u32 data)
{
	const char *addrstr = netFormatPeerAddr(peer);

	sysLogPrintf(LOG_NOTE | LOGFLAG_NOCON, "NET: connection attempt from %s", addrstr);

	++g_NetNumClients;

	if (data != NET_PROTOCOL_VER) {
		sysLogPrintf(LOG_NOTE | LOGFLAG_NOCON, "NET: %s rejected: protocol mismatch", addrstr);
		enet_peer_disconnect(peer, DISCONNECT_VERSION);
		return;
	}

	if (g_NetLocalClient && g_NetLocalClient->state > CLSTATE_LOBBY) {
		sysLogPrintf(LOG_NOTE | LOGFLAG_NOCON, "NET: %s rejected: late joins not allowed", addrstr);
		enet_peer_disconnect(peer, DISCONNECT_LATE);
		return;
	}

	struct netclient *cl = NULL;

	// id 0 is the local client
	for (s32 i = 1; i < g_NetMaxClients; ++i) {
		if (!g_NetClients[i].state) {
			cl = &g_NetClients[i];
			break;
		}
	}

	if (!cl) {
		sysLogPrintf(LOG_NOTE | LOGFLAG_NOCON, "NET: %s rejected: server is full", addrstr);
		enet_peer_disconnect(peer, DISCONNECT_FULL);
		return;
	}

	netClientReset(cl);
	cl->state = CLSTATE_AUTH; // skip CLSTATE_CONNECTING, since we already know it connected
	cl->peer = peer;
	enet_peer_set_data(peer, cl);
}

static void netServerEvDisconnect(struct netclient *cl)
{
	sysLogPrintf(LOG_NOTE | LOGFLAG_NOCON, "NET: disconnect event from %s", netFormatClientAddr(cl));

	if (cl->peer) {
		enet_peer_reset(cl->peer);
	}

	if (cl->settings.name[0]) {
		sysLogPrintf(LOG_NOTE, "NET: client %u (%s) disconnected", cl->id, cl->settings.name);
		netChatPrintf(NULL, "%s disconnected", cl->settings.name);
	} else {
		sysLogPrintf(LOG_CHAT, "NET: client %u disconnected", cl->id);
	}

	netClientReset(cl);

	--g_NetNumClients;

	// Keep Teams/sims roster fresh for remaining peers still in the lobby.
	if (g_NetLocalClient && g_NetLocalClient->state == CLSTATE_LOBBY) {
		netLobbyRebuildHumanSlots();
		netMpSetupSend();
	}

	// A peer left while others are waiting on the endscreen — maybe everyone else is ready.
	if (g_NetWaitingForPlayers) {
		netServerCheckEndscreenReady();
	}
}

static void netServerEvReceive(struct netclient *cl)
{
	u32 rc = 0;
	u8 msgid = 0;

	while (!rc && netbufReadLeft(&cl->in) > 0) {
		msgid = netbufReadU8(&cl->in);
		netdbgRx(0, msgid);
		switch (msgid) {
			case CLC_NOP: rc = 0; break;
			case CLC_AUTH: rc = netmsgClcAuthRead(&cl->in, cl); break;
			case CLC_CHAT: rc = netmsgClcChatRead(&cl->in, cl); break;
			case CLC_MOVE: rc = netmsgClcMoveRead(&cl->in, cl); break;
			case CLC_SETTINGS: rc = netmsgClcSettingsRead(&cl->in, cl); break;
			case CLC_MPSETUP: rc = netmsgClcMpSetupRead(&cl->in, cl); break;
			case CLC_ENDSCREEN_READY: rc = netmsgClcEndscreenReadyRead(&cl->in, cl); break;
			case CLC_LEFT_MATCH: rc = netmsgClcLeftMatchRead(&cl->in, cl); break;
			case CLC_REJOIN_MATCH: rc = netmsgClcRejoinMatchRead(&cl->in, cl); break;
			default:
				rc = 1;
				break;
		}
	}

	if (rc) {
		netdbgEvent("warn", "\"action\":\"malformed_clc\",\"msgid\":%u,\"clid\":%u",
				(unsigned)msgid, (unsigned)cl->id);
		sysLogPrintf(LOG_WARNING , "NET: malformed or unknown message 0x%02x from client %u", msgid, cl->id);
	}
}

static void netClientEvConnect(const u32 data)
{
	sysLogPrintf(LOG_NOTE, "NET: connected to server, sending CLC_AUTH");

	g_NetLocalClient->state = CLSTATE_AUTH;

	// send auth request
	netbufStartWrite(&g_NetMsgRel);
	netmsgClcAuthWrite(&g_NetMsgRel);
	netmsgClcSettingsWrite(&g_NetMsgRel);
	netSend(g_NetLocalClient, &g_NetMsgRel, true, NETCHAN_CONTROL);
}

static void netClientEvDisconnect(const u32 reason)
{
	const s32 inlobby = (g_NetLocalClient && g_NetLocalClient->state >= CLSTATE_LOBBY
			&& g_NetLocalClient->state < CLSTATE_GAME);

	sysLogPrintf(LOG_CHAT, "NET: disconnected from server: %s (%u)", netGetDisconnectReason(reason), reason);
	netDisconnect();

	// Host left during setup — return clients to the Network Game menu.
	if (inlobby) {
		g_NetReturnToNetMenu = 1;
	}
}

static void netClientEvReceive(struct netclient *cl)
{
	u32 rc = 0;
	u8 msgid = 0;

	while (!rc && netbufReadLeft(&cl->in) > 0) {
		msgid = netbufReadU8(&cl->in);
		netdbgRx(1, msgid);
		/* Flush around the freeze-prone msgs so a hang leaves the msgid on disk. */
		if (msgid == SVC_WALLHIT || msgid == SVC_SIM_MOVE || msgid == SVC_CHR_DAMAGE
				|| msgid == SVC_PROP_SPAWN || msgid == SVC_CHR_BEAM) {
			netdbgFlush();
		}
		switch (msgid) {
			case SVC_NOP: rc = 0; break;
			case SVC_AUTH: rc = netmsgSvcAuthRead(&cl->in, cl); break;
			case SVC_CHAT: rc = netmsgSvcChatRead(&cl->in, cl); break;
			case SVC_STAGE_START: rc = netmsgSvcStageStartRead(&cl->in, cl); break;
			case SVC_STAGE_END: rc = netmsgSvcStageEndRead(&cl->in, cl); break;
			case SVC_MPSETUP: rc = netmsgSvcMpSetupRead(&cl->in, cl); break;
			case SVC_MATCH_PROCEED: rc = netmsgSvcMatchProceedRead(&cl->in, cl); break;
			case SVC_PLAYER_MOVE: rc = netmsgSvcPlayerMoveRead(&cl->in, cl); break;
			case SVC_PLAYER_GUNS: rc = netmsgSvcPlayerGunsRead(&cl->in, cl); break;
			case SVC_PLAYER_STATS: rc = netmsgSvcPlayerStatsRead(&cl->in, cl); break;
			case SVC_PROP_MOVE: rc = netmsgSvcPropMoveRead(&cl->in, cl); break;
			case SVC_PROP_SPAWN: rc = netmsgSvcPropSpawnRead(&cl->in, cl); break;
			case SVC_PROP_DAMAGE: rc = netmsgSvcPropDamageRead(&cl->in, cl); break;
			case SVC_PROP_PICKUP: rc = netmsgSvcPropPickupRead(&cl->in, cl); break;
			case SVC_PROP_USE: rc = netmsgSvcPropUseRead(&cl->in, cl); break;
			case SVC_PROP_DOOR: rc = netmsgSvcPropDoorRead(&cl->in, cl); break;
			case SVC_PROP_LIFT: rc = netmsgSvcPropLiftRead(&cl->in, cl); break;
			case SVC_WALLHIT: rc = netmsgSvcWallhitRead(&cl->in, cl); break;
			case SVC_PROP_GLASS: rc = netmsgSvcPropGlassRead(&cl->in, cl); break;
			case SVC_CHR_DAMAGE: rc = netmsgSvcChrDamageRead(&cl->in, cl); break;
			case SVC_CHR_DISARM: rc = netmsgSvcChrDisarmRead(&cl->in, cl); break;
			case SVC_SIM_MOVE: rc = netmsgSvcSimMoveRead(&cl->in, cl); break;
			case SVC_CHR_BEAM: rc = netmsgSvcChrBeamRead(&cl->in, cl); break;
			case SVC_SCENARIO: rc = netmsgSvcScenarioRead(&cl->in, cl); break;
			case SVC_PROP_REGEN: rc = netmsgSvcPropRegenRead(&cl->in, cl); break;
			case SVC_SCENARIO_EVENT: rc = netmsgSvcScenarioEventRead(&cl->in, cl); break;
			case SVC_EXPLOSION: rc = netmsgSvcExplosionRead(&cl->in, cl); break;
			default:
				rc = 1;
				break;
		}
	}

	if (rc) {
		netdbgEvent("warn", "\"action\":\"malformed_svc\",\"msgid\":%u", (unsigned)msgid);
		sysLogPrintf(LOG_WARNING, "NET: malformed or unknown message 0x%02x from server", msgid);
	}
}

void netClientSyncRng(void)
{
	if (g_NetMode == NETMODE_CLIENT && g_NetRngLatch) {
		g_NetRngLatch = 0;
		g_RngSeed = g_NetRngSeeds[0];
		g_Rng2Seed = g_NetRngSeeds[1];
	}
}

void netApplyRemoteGunVisuals(struct player *pl)
{
	if (!pl || !pl->isremote || !pl->client) {
		return;
	}

	struct netclient *cl = pl->client;

	// Client remotes: re-apply the host snapshot after local bgun re-sim may have
	// cleared flashon for this tick.
	if (g_NetMode == NETMODE_CLIENT && cl->netgunvalid) {
		const u8 flags = cl->netgunflags;
		const u8 flashbits = flags & (NETGUN_FLASH_L | NETGUN_FLASH_R);

		pl->hands[HAND_LEFT].flashon = (flags & NETGUN_FLASH_L) != 0;
		pl->hands[HAND_RIGHT].flashon = (flags & NETGUN_FLASH_R) != 0;
		pl->hands[HAND_LEFT].firing = (flags & NETGUN_FIRING_L) != 0;
		pl->hands[HAND_RIGHT].firing = (flags & NETGUN_FIRING_R) != 0;
		pl->insightaimmode = (flags & NETGUN_AIMMODE) != 0;
		pl->shootrotx = cl->netshootrotx;
		pl->shootroty = cl->netshootroty;

		// Remotes skip FP gun audio on clients — play positional shots on flash edge.
		for (s32 h = 0; h < 2; ++h) {
			const u8 bit = (h == HAND_LEFT) ? NETGUN_FLASH_L : NETGUN_FLASH_R;
			if ((flashbits & bit) && !(cl->netgunsoundflags & bit) && pl->hands[h].inuse) {
				netPlayPositionalGunShot(pl->prop, &pl->hands[h].gset);
			}
		}
		cl->netgunsoundflags = flashbits;
	} else if (pl->isremote) {
		// Host remotes never get FP shootrot; keep TP aim keyed to look pitch.
		pl->shootrotx = pl->vv_verta * (M_BADTAU / 360.0f);
		if (pl->shootrotx >= M_PI) {
			pl->shootrotx -= M_BADTAU;
		}
		pl->shootroty = 0.f;
	}

	// Latch muzzle flash across a few frames so short flashes remain visible on TP
	// held guns (and so clients don't miss a single-tick flash between packets).
	// Only latch on real flashon — melee/punch sets firing without NOMUZZLEFLASH
	// flash, and treating firing as flash made punches light gunfire parts.
	for (s32 h = 0; h < 2; ++h) {
		if (pl->hands[h].flashon) {
			cl->gunflashlatch[h] = 4;
		} else if (cl->gunflashlatch[h] > 0) {
			cl->gunflashlatch[h]--;
		}

		if (cl->gunflashlatch[h] > 0) {
			pl->hands[h].flashon = true;
		}
	}

	// Brief aim kick so remotes show a readable "recoil" in third person (FP recoil
	// never runs on remotes; shootrot alone is look-pitch without kick).
	if (cl->gunflashlatch[HAND_RIGHT] > 0 || cl->gunflashlatch[HAND_LEFT] > 0) {
		const u8 latch = cl->gunflashlatch[HAND_RIGHT] > cl->gunflashlatch[HAND_LEFT]
			? cl->gunflashlatch[HAND_RIGHT] : cl->gunflashlatch[HAND_LEFT];
		pl->shootrotx += 0.055f * ((f32)latch / 4.0f);
	}

	// Drive held-weapon gunfire parts directly — don't rely solely on
	// playerTickThirdPerson (which skips when currentplayernum == playernum).
	if (pl->prop && pl->prop->chr && pl->haschrbody) {
		struct chrdata *chr = pl->prop->chr;
		chrSetFiring(chr, HAND_RIGHT, pl->hands[HAND_RIGHT].flashon);
		chrSetFiring(chr, HAND_LEFT, pl->hands[HAND_LEFT].flashon);
	}
}

void netClientSettingsChanged(void)
{
	if (!g_NetMode || !g_NetLocalClient) {
		return;
	}

	netClientReadConfig(g_NetLocalClient, netGetLocalConfigIndex());

	// Host keeps settings locally so stage-start / remote wallhit paintball
	// evaluation matches the Control / Fun menu toggles.
	if (g_NetMode != NETMODE_CLIENT) {
		return;
	}

	netbufStartWrite(&g_NetMsgRel);
	netmsgClcSettingsWrite(&g_NetMsgRel);
	netSend(NULL, &g_NetMsgRel, true, NETCHAN_CONTROL);
}

/* -------------------------------------------------------------------------- */
/* Lag compensation: host rewinds targets to the shooter's perceived time.    */
/* -------------------------------------------------------------------------- */

#define NET_LAGCOMP_HISTORY 64

struct netlagpose {
	u32 tick;
	struct coord pos;
	u8 valid;
};

struct netlagring {
	struct netlagpose poses[NET_LAGCOMP_HISTORY];
	u32 write;
};

struct netlagrestore {
	u8 active;
	struct prop *prop;
	struct coord pos;
	f32 propz;
	struct coord dcam;
};

static struct netlagring g_NetLagPlayerHist[MAX_PLAYERS];
static struct netlagring g_NetLagSimHist[MAX_BOTS];
static struct netlagrestore g_NetLagPlayerRestore[MAX_PLAYERS];
static struct netlagrestore g_NetLagSimRestore[MAX_BOTS];
static s32 g_NetLagCompActive;

static void netLagCompClearAll(void)
{
	memset(g_NetLagPlayerHist, 0, sizeof(g_NetLagPlayerHist));
	memset(g_NetLagSimHist, 0, sizeof(g_NetLagSimHist));
	memset(g_NetLagPlayerRestore, 0, sizeof(g_NetLagPlayerRestore));
	memset(g_NetLagSimRestore, 0, sizeof(g_NetLagSimRestore));
	g_NetLagCompActive = 0;
}

static void netLagCompRingPush(struct netlagring *ring, const struct coord *pos)
{
	struct netlagpose *pose = &ring->poses[ring->write % NET_LAGCOMP_HISTORY];

	pose->tick = g_NetTick;
	pose->pos = *pos;
	pose->valid = 1;
	ring->write++;
}

static const struct netlagpose *netLagCompRingFind(const struct netlagring *ring, u32 targettick)
{
	const struct netlagpose *best = NULL;

	for (s32 i = 0; i < NET_LAGCOMP_HISTORY; ++i) {
		const struct netlagpose *pose = &ring->poses[i];

		if (!pose->valid || pose->tick > targettick) {
			continue;
		}
		if (!best || pose->tick > best->tick) {
			best = pose;
		}
	}

	return best;
}

static void netLagCompOffsetModel(struct model *model, const struct coord *dcam)
{
	s32 n;

	if (!model || !model->matrices || !model->definition) {
		return;
	}

	n = model->definition->nummatrices;
	for (s32 i = 0; i < n; ++i) {
		model->matrices[i].m[3][0] += dcam->x;
		model->matrices[i].m[3][1] += dcam->y;
		model->matrices[i].m[3][2] += dcam->z;
	}
}

static void netLagCompOffsetPropTree(struct prop *prop, const struct coord *dcam)
{
	struct prop *child;

	if (!prop) {
		return;
	}

	if (prop->type == PROPTYPE_CHR || prop->type == PROPTYPE_PLAYER) {
		if (prop->chr && prop->chr->model) {
			netLagCompOffsetModel(prop->chr->model, dcam);
		}
	} else if (prop->obj && prop->obj->model) {
		netLagCompOffsetModel(prop->obj->model, dcam);
	}

	child = prop->child;
	while (child) {
		struct prop *next = propGetChildNext(child);
		netLagCompOffsetPropTree(child, dcam);
		child = next;
	}
}

static void netLagCompApplyProp(struct prop *prop, const struct coord *histpos, struct netlagrestore *rest)
{
	struct coord delta;
	struct coord dcam;
	Mtxf *w2s;

	if (!prop || !prop->chr || !histpos || !rest) {
		return;
	}

	delta.x = histpos->x - prop->pos.x;
	delta.y = histpos->y - prop->pos.y;
	delta.z = histpos->z - prop->pos.z;

	if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f) {
		return;
	}

	rest->active = 1;
	rest->prop = prop;
	rest->pos = prop->pos;
	rest->propz = prop->z;

	prop->pos = *histpos;
	if (prop->chr->model) {
		modelSetRootPosition(prop->chr->model, &prop->pos);
	}

	/*
	 * Hitscan uses camera-space model matrices (built for the shooter's view).
	 * Translate those by the same world delta rotated into cam space.
	 */
	dcam.x = dcam.y = dcam.z = 0.0f;
	w2s = camGetWorldToScreenMtxf();
	if (w2s && (prop->flags & PROPFLAG_ONTHISSCREENTHISTICK)
			&& prop->chr->model && prop->chr->model->matrices) {
		mtx4RotateVec(w2s, &delta, &dcam);
		netLagCompOffsetPropTree(prop, &dcam);
		prop->z = -prop->chr->model->matrices[0].m[3][2];
	}

	rest->dcam = dcam;
}

static void netLagCompRestoreOne(struct netlagrestore *rest)
{
	struct coord neg;

	if (!rest || !rest->active || !rest->prop) {
		return;
	}

	rest->prop->pos = rest->pos;
	if (rest->prop->chr && rest->prop->chr->model) {
		modelSetRootPosition(rest->prop->chr->model, &rest->pos);
	}

	neg.x = -rest->dcam.x;
	neg.y = -rest->dcam.y;
	neg.z = -rest->dcam.z;
	if (neg.x != 0.0f || neg.y != 0.0f || neg.z != 0.0f) {
		netLagCompOffsetPropTree(rest->prop, &neg);
	}

	rest->prop->z = rest->propz;
	rest->active = 0;
	rest->prop = NULL;
}

void netLagCompRecord(void)
{
	if (g_NetMode != NETMODE_SERVER || !g_NetLagCompEnabled) {
		return;
	}
	if (!g_Vars.normmplayerisrunning || !g_Vars.players) {
		return;
	}

	for (s32 i = 0; i < PLAYERCOUNT(); ++i) {
		struct player *pl = g_Vars.players[i];

		if (!pl || !pl->prop || pl->isdead) {
			continue;
		}
		netLagCompRingPush(&g_NetLagPlayerHist[i], &pl->prop->pos);
	}

	for (s32 i = 0; i < g_BotCount && i < MAX_BOTS; ++i) {
		struct chrdata *chr = g_MpBotChrPtrs[i];

		if (!chr || !chr->prop || chrIsDead(chr)) {
			continue;
		}
		netLagCompRingPush(&g_NetLagSimHist[i], &chr->prop->pos);
	}
}

static u32 netLagCompRewindTicksForShooter(struct player *shooter)
{
	u32 rtt_ms = 0;
	u32 one_way;
	u32 ticks;
	struct netclient *cl;

	if (!shooter) {
		return 0;
	}

	/* Host's own shots already see live server state — no rewind. */
	if (!shooter->isremote) {
		return 0;
	}

	cl = shooter->client;
	if (!cl) {
		for (s32 i = 0; i < g_NetMaxClients; ++i) {
			if (g_NetClients[i].player == shooter) {
				cl = &g_NetClients[i];
				break;
			}
		}
	}

	if (cl && cl->peer) {
		rtt_ms = enet_peer_get_rtt(cl->peer);
	}

	/* One-way delay in 60Hz ticks + client interpolation buffer. */
	one_way = (rtt_ms * 60u) / 2000u;
	ticks = one_way + g_NetInterpTicks;
	if (ticks > g_NetLagCompMaxTicks) {
		ticks = g_NetLagCompMaxTicks;
	}
	return ticks;
}

s32 netLagCompBegin(struct player *shooter)
{
	u32 rewind;
	u32 targettick;
	s32 shooter_pnum;

	if (g_NetMode != NETMODE_SERVER || !g_NetLagCompEnabled || g_NetLagCompActive) {
		return 0;
	}
	if (!shooter || !shooter->prop || !g_Vars.normmplayerisrunning) {
		return 0;
	}

	rewind = netLagCompRewindTicksForShooter(shooter);
	if (rewind == 0) {
		return 0;
	}

	targettick = (g_NetTick > rewind) ? (g_NetTick - rewind) : 0;
	shooter_pnum = playermgrGetPlayerNumByProp(shooter->prop);

	memset(g_NetLagPlayerRestore, 0, sizeof(g_NetLagPlayerRestore));
	memset(g_NetLagSimRestore, 0, sizeof(g_NetLagSimRestore));

	for (s32 i = 0; i < PLAYERCOUNT(); ++i) {
		struct player *pl;
		const struct netlagpose *pose;

		if (i == shooter_pnum) {
			continue;
		}

		pl = g_Vars.players[i];
		if (!pl || !pl->prop || pl->isdead) {
			continue;
		}

		pose = netLagCompRingFind(&g_NetLagPlayerHist[i], targettick);
		if (!pose) {
			continue;
		}
		netLagCompApplyProp(pl->prop, &pose->pos, &g_NetLagPlayerRestore[i]);
	}

	for (s32 i = 0; i < g_BotCount && i < MAX_BOTS; ++i) {
		struct chrdata *chr = g_MpBotChrPtrs[i];
		const struct netlagpose *pose;

		if (!chr || !chr->prop || chrIsDead(chr)) {
			continue;
		}

		pose = netLagCompRingFind(&g_NetLagSimHist[i], targettick);
		if (!pose) {
			continue;
		}
		netLagCompApplyProp(chr->prop, &pose->pos, &g_NetLagSimRestore[i]);
	}

	g_NetLagCompActive = 1;
	return 1;
}

void netLagCompEnd(void)
{
	if (!g_NetLagCompActive) {
		return;
	}

	for (s32 i = 0; i < MAX_PLAYERS; ++i) {
		netLagCompRestoreOne(&g_NetLagPlayerRestore[i]);
	}
	for (s32 i = 0; i < MAX_BOTS; ++i) {
		netLagCompRestoreOne(&g_NetLagSimRestore[i]);
	}

	g_NetLagCompActive = 0;
}

/**
 * Every ~60 net ticks during MP, walk activeprops toward pausedprops.
 * Cycle / OOB / null-hole → rebuild before tick/render hang or vanish.
 */
static void netPropsIntegrityCheck(void)
{
	struct prop *pool = g_Vars.props;
	const s32 maxp = g_Vars.maxprops;
	struct prop *p;
	s32 safety;

	if (!pool || maxp <= 0 || !g_Vars.normmplayerisrunning) {
		return;
	}
	if ((g_NetTick % 60u) != 0u) {
		return;
	}

	p = g_Vars.activeprops;
	safety = maxp + 8;

	while (p && p != g_Vars.pausedprops && --safety >= 0) {
		if (p < pool || p >= pool + maxp) {
			propsRepairActiveList("periodic_oob");
			return;
		}
		p = p->next;
	}

	if (safety < 0 || (p != g_Vars.pausedprops)) {
		propsRepairActiveList("periodic");
	} else {
		propsScrubDanglingScenarioRefs();
	}
}

void netStartFrame(void)
{
	if (!g_NetMode) {
		return;
	}

	++g_NetTick;
	netPropsIntegrityCheck();

	const bool isClient = (g_NetMode == NETMODE_CLIENT);
	s32 polled = false;
	ENetEvent ev = { .type = ENET_EVENT_TYPE_NONE };
	while (!polled) {
		if (enet_host_check_events(g_NetHost, &ev) <= 0) {
			if (enet_host_service(g_NetHost, &ev, 1) <= 0) {
				break;
			}
			polled = true;
		}

		switch (ev.type) {
			case ENET_EVENT_TYPE_CONNECT:
				if (isClient) {
					netClientEvConnect(ev.data);
				} else if (ev.peer) {
					netServerEvConnect(ev.peer, ev.data);
				}
				break;
			case ENET_EVENT_TYPE_DISCONNECT:
			case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
				if (isClient) {
					netClientEvDisconnect(ev.type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT ? DISCONNECT_TIMEOUT : ev.data);
				} else if (ev.peer) {
					struct netclient *cl = enet_peer_get_data(ev.peer);
					if (cl) {
						netServerEvDisconnect(cl);
					} else {
						sysLogPrintf(LOG_WARNING | LOGFLAG_NOCON, "NET: disconnect from %s without attached client", netFormatPeerAddr(ev.peer));
						--g_NetNumClients;
					}
				}
				break;
			case ENET_EVENT_TYPE_RECEIVE:
				if (ev.peer) {
					struct netclient *cl = (g_NetMode == NETMODE_CLIENT) ? g_NetLocalClient : enet_peer_get_data(ev.peer);
					if (cl && cl->state) {
						if (ev.packet->data && ev.packet->dataLength) {
							netbufStartReadData(&cl->in, ev.packet->data, ev.packet->dataLength);
							if (isClient) {
								netClientEvReceive(cl);
							} else {
								netServerEvReceive(cl);
							}
							netbufReset(&cl->in);
						}
					} else if (!isClient) {
						sysLogPrintf(LOG_WARNING | LOGFLAG_NOCON, "NET: receive from %s without attached client", netFormatPeerAddr(ev.peer));
					}
				}
				enet_packet_dispose(ev.packet);
				break;
			default:
				break;
		}
	}

	netbufStartWrite(&g_NetMsg);
	netbufStartWrite(&g_NetMsgRel);
}

void netEndFrame(void)
{
	if (!g_NetMode) {
		return;
	}

	g_NetReliableFrameLen = 0;
	g_NetUnreliableFrameLen = 0;

	// Snapshot poses for lag-compensated hitscan on the next shots.
	netLagCompRecord();

	// sync Combat Sim setup while everyone is still in the lobby
	netMpSetupTick();

	// send whatever messages have accumulated so far
	netFlushSendBuffers();

	if (g_NetLocalClient->state == CLSTATE_GAME && g_NetLocalClient->player && g_NetLocalClient->player->prop) {
		if (g_NetMode == NETMODE_CLIENT) {
			// Still send moves after Rejoin (force respawn); skip while sitting on wait UI.
			if (g_NetTick > 100 && !g_NetAwaitingRejoin) {
				netClientRecordMove(g_NetLocalClient, g_NetLocalClient->player);
				const bool needrel = netClientNeedReliableMove(g_NetLocalClient);
				if (needrel || netClientNeedMove(g_NetLocalClient)) {
					netmsgClcMoveWrite(needrel ? &g_NetMsgRel : &g_NetMsg);
				}
			}
			if (g_NetNextUpdate <= g_NetTick) {
				g_NetNextUpdate = g_NetTick + g_NetClientUpdateRate;
			}
		} else {
			const bool updateTick = (g_NetNextUpdate <= g_NetTick);
			for (s32 i = 0; i < g_NetMaxClients; ++i) {
				struct netclient *cl = &g_NetClients[i];
				if (cl->state >= CLSTATE_GAME && cl->player && !cl->leftmatch) {
					netClientRecordMove(cl, cl->player);
					const bool needrel = netClientNeedReliableMove(cl);
					if (needrel || netClientNeedMove(cl)) {
						netmsgSvcPlayerMoveWrite(needrel ? &g_NetMsgRel : &g_NetMsg, cl);
					}

					// Gun-play (muzzle flash + TP aim) — send on change, and every
					// update tick while firing/flashing/aiming so short flashes aren't missed.
					const u8 gunflags = (cl->player->hands[HAND_LEFT].flashon ? NETGUN_FLASH_L : 0)
						| (cl->player->hands[HAND_RIGHT].flashon ? NETGUN_FLASH_R : 0)
						| (cl->player->hands[HAND_LEFT].firing ? NETGUN_FIRING_L : 0)
						| (cl->player->hands[HAND_RIGHT].firing ? NETGUN_FIRING_R : 0)
						| (cl->player->insightaimmode ? NETGUN_AIMMODE : 0);
					const bool gunActive = (gunflags & (NETGUN_FLASH_L | NETGUN_FLASH_R | NETGUN_FIRING_L | NETGUN_FIRING_R | NETGUN_AIMMODE)) != 0;
					const bool gunChanged = !cl->gunflagsvalid || cl->lastgunflags != gunflags;
					if (gunChanged || (updateTick && gunActive)) {
						netmsgSvcPlayerGunsWrite(&g_NetMsg, cl);
						cl->lastgunflags = gunflags;
						cl->gunflagsvalid = 1;
					}
				}
			}
			if (updateTick) {
				g_NetNextUpdate = g_NetTick + g_NetServerUpdateRate;
			}

			// Flush player moves first so sim snapshots are not truncated when
			// the shared unreliable buffer is near full.
			netFlushSendBuffers();
			netbufStartWrite(&g_NetMsg);
			netServerBroadcastSimMoves();
		}
	}

	// send position updates
	netFlushSendBuffers();

	enet_host_flush(g_NetHost);
}

u32 netSend(struct netclient *dstcl, struct netbuf *buf, const s32 reliable, const s32 chan)
{
	if (g_NetMode == NETMODE_CLIENT) {
		dstcl = g_NetLocalClient;
	}

	if (buf == NULL) {
		if (dstcl) {
			buf = &dstcl->out;
		} else {
			buf = reliable ? &g_NetMsgRel : &g_NetMsg;
		}
	}

	if (reliable || !g_NetSimPacketLoss || (rand() % g_NetSimPacketLoss) == 0) {
		const u32 flags = (reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
		ENetPacket *p = enet_packet_create(buf->data, buf->wp, flags);
		if (!p) {
			sysLogPrintf(LOG_ERROR, "NET: could not alloc %u bytes for packet", buf->wp);
			return 0;
		}

		if (dstcl == NULL) {
			enet_host_broadcast(g_NetHost, chan, p);
		} else {
			enet_peer_send(dstcl->peer, chan, p);
		}
	}

	const u32 ret = buf->wp;

	netbufStartWrite(buf);

	return ret;
}

void netPlayersAllocate(void)
{
	s32 playernum = 0;

	if (g_NetMode == NETMODE_CLIENT) {
		// we always put the local player at index 0, even client-side
		// which means that clientside we have to put the server's player into our slot
		const s32 svplayernum = g_NetLocalClient->playernum;
		g_NetLocalClient->playernum = 0;
		g_NetClients[0].playernum = svplayernum;
	}

	for (s32 i = 0; i < g_NetMaxClients; ++i) {
		struct netclient *cl = &g_NetClients[i];
		if (cl->state < CLSTATE_LOBBY) {
			continue;
		}

		if (g_NetMode == NETMODE_SERVER) {
			// on the server allocate players sequentially
			cl->playernum = playernum++;
		}

		{
			struct mpplayerconfig *cfg = &g_PlayerConfigsArray[cl->playernum];
			if (cl != g_NetLocalClient) {
				// disable controls for the remote pawns and set their settings
				// TODO: backup the player configs or something
				cfg->controlmode = CONTROLMODE_NA;
				cfg->base.mpbodynum = cl->settings.bodynum;
				cfg->base.mpheadnum = cl->settings.headnum;
				snprintf(cfg->base.name, sizeof(cfg->base.name), "%s\n", cl->settings.name);
				// Use each client's own options (including paintball) so paint vs
				// normal bullet holes match who actually fired.
				cfg->options = cl->settings.options;
				// don't enable toggle aim, invert pitch or lookahead for remote players
				cfg->options &= ~(OPTION_AIMCONTROL | OPTION_LOOKAHEAD);
				cfg->options |= OPTION_FORWARDPITCH | OPTION_ASKEDSAVEPLAYER;
			} else {
				// Keep local control options, but force paintball from the
				// settings snapshot so Fun/Control toggles always apply.
				if (cl->settings.options & OPTION_PAINTBALL) {
					cfg->options |= OPTION_PAINTBALL;
				} else {
					cfg->options &= ~OPTION_PAINTBALL;
				}
			}
		}

		cl->config = &g_PlayerConfigsArray[cl->playernum];
		cl->config->client = cl;
		cl->config->handicap = 0x80;
		cl->player = g_Vars.players[cl->playernum];
		if (cl->player) {
			cl->player->client = cl;
			cl->player->isremote = (cl != g_NetLocalClient);
		}
	}
}

static u32 netSyncHashBucket(u32 syncid)
{
	return (syncid * 0x9e3779b1u) & NET_SYNC_HASH_MASK;
}

static void netSyncHashRemoveKey(u32 syncid)
{
	u32 i;
	u32 b;

	if (!syncid || syncid == NET_SYNC_TOMB) {
		return;
	}

	b = netSyncHashBucket(syncid);
	for (i = 0; i < NET_SYNC_HASH_SIZE; i++) {
		u32 idx = (b + i) & NET_SYNC_HASH_MASK;
		u32 key = s_NetSyncHashKeys[idx];

		if (key == NET_SYNC_EMPTY) {
			return;
		}
		if (key == syncid) {
			s_NetSyncHashKeys[idx] = NET_SYNC_TOMB;
			s_NetSyncHashVals[idx] = NULL;
			if (s_NetSyncHashLive > 0) {
				s_NetSyncHashLive--;
			}
			return;
		}
	}
}

static void netSyncHashInsertRaw(struct prop *prop)
{
	u32 i;
	u32 b;
	s32 tomb = -1;

	if (!prop || !prop->syncid || prop->syncid == NET_SYNC_TOMB) {
		return;
	}

	b = netSyncHashBucket(prop->syncid);
	for (i = 0; i < NET_SYNC_HASH_SIZE; i++) {
		u32 idx = (b + i) & NET_SYNC_HASH_MASK;
		u32 key = s_NetSyncHashKeys[idx];

		if (key == NET_SYNC_EMPTY || key == prop->syncid) {
			if (key == NET_SYNC_EMPTY && tomb >= 0) {
				idx = (u32)tomb;
			}
			if (key != prop->syncid) {
				s_NetSyncHashLive++;
			}
			s_NetSyncHashKeys[idx] = prop->syncid;
			s_NetSyncHashVals[idx] = prop;
			return;
		}
		if (key == NET_SYNC_TOMB && tomb < 0) {
			tomb = (s32)idx;
		}
	}

	sysLogPrintf(LOG_WARNING, "NET: syncid hash full (live=%d)", s_NetSyncHashLive);
}

static void netSyncHashInsert(struct prop *prop)
{
	if (!prop || !prop->syncid || prop->syncid == NET_SYNC_TOMB) {
		return;
	}

	/* Drop any prior entry for this syncid (rebind / rebuild). */
	netSyncHashRemoveKey(prop->syncid);
	netSyncHashInsertRaw(prop);
}

struct prop *netPropFindBySyncId(u32 syncid)
{
	u32 i;
	u32 b;

	if (!syncid || syncid == NET_SYNC_TOMB) {
		return NULL;
	}

	b = netSyncHashBucket(syncid);
	for (i = 0; i < NET_SYNC_HASH_SIZE; i++) {
		u32 idx = (b + i) & NET_SYNC_HASH_MASK;
		u32 key = s_NetSyncHashKeys[idx];

		if (key == NET_SYNC_EMPTY) {
			break;
		}
		if (key == syncid) {
			struct prop *p = s_NetSyncHashVals[idx];
			if (p && p->syncid == syncid) {
				return p;
			}
			break;
		}
	}

	/* Linear fallback if hash missed (table pressure / partial rebuild). */
	if (g_Vars.props) {
		for (i = 0; i < (u32)g_Vars.maxprops; i++) {
			if (g_Vars.props[i].syncid == syncid) {
				netSyncHashInsert(&g_Vars.props[i]);
				return &g_Vars.props[i];
			}
		}
	}
	return NULL;
}

void netPropClearSyncId(struct prop *prop)
{
	if (!prop || !prop->syncid) {
		return;
	}
	netSyncHashRemoveKey(prop->syncid);
	prop->syncid = 0;
}

void netPropSetSyncId(struct prop *prop, u32 syncid)
{
	if (!prop) {
		return;
	}
	if (prop->syncid == syncid) {
		if (syncid) {
			netSyncHashInsert(prop);
		}
		return;
	}
	if (prop->syncid) {
		netSyncHashRemoveKey(prop->syncid);
	}
	prop->syncid = syncid;
	if (syncid) {
		netSyncHashInsert(prop);
	}
}

void netPropEnsureSyncId(struct prop *prop)
{
	u32 id;

	if (!prop || prop->syncid) {
		return;
	}

	id = g_NetNextSyncId++;
	if (g_NetNextSyncId == 0) {
		g_NetNextSyncId = 1;
	}
	netPropSetSyncId(prop, id);
}

void netSyncIdsAllocate(void)
{
	// allocate sync ids sequentially for all active or paused props
	g_NetNextSyncId = 1;
	netSyncHashClear();

	// don't allocate anything else if we're in lobby
	if (g_StageNum == STAGE_TITLE || g_StageNum == STAGE_CITRAINING) {
		return;
	}

	// iterate active props first
	struct prop *prop = g_Vars.activeprops;
	{
		s32 walksafety = g_Vars.maxprops + 8;

		while (prop && prop != g_Vars.pausedprops) {
			u32 id = (u32)(prop - g_Vars.props) + 1;
			netPropSetSyncId(prop, id);
			if (id >= g_NetNextSyncId) {
				// Next free id must be strictly above every stage prop, or the first
				// mid-match SPAWN (bot death drops, etc.) collides and clients AV in
				// netmsgSvcPropSpawnRead writing weapon fields into a chr/map prop.
				g_NetNextSyncId = id + 1;
			}
			prop = propListNext(prop, &walksafety, "netSyncIds_active");
		}
	}

	// then the paused props
	prop = g_Vars.pausedprops;
	{
		s32 walksafety = g_Vars.maxprops + 8;

		while (prop) {
			u32 id = (u32)(prop - g_Vars.props) + 1;
			netPropSetSyncId(prop, id);
			if (id >= g_NetNextSyncId) {
				g_NetNextSyncId = id + 1;
			}
			prop = propListNext(prop, &walksafety, "netSyncIds_paused");
		}
	}

	// HACK: when we're a client, we'll need to swap our player and server player's props
	// because of what we do in netPlayersAllocate
	if (g_NetMode == NETMODE_CLIENT) {
		if (!g_NetLocalClient->player || !g_NetLocalClient->player->prop) {
			sysLogPrintf(LOG_ERROR, "NET: no props allocated for players?");
			netDisconnect();
			return;
		}
		{
			struct prop *hostprop = g_NetClients[0].player->prop;
			struct prop *localprop = g_NetLocalClient->player->prop;
			u32 hostid = hostprop->syncid;
			u32 localid = localprop->syncid;

			netPropSetSyncId(hostprop, localid);
			netPropSetSyncId(localprop, hostid);
		}
	}

	sysLogPrintf(LOG_NOTE, "NET: next free syncid: %u", g_NetNextSyncId);
}

void netChatPrintf(struct netclient *dst, const char *fmt, ...)
{
	char tmp[512];
	u8 bufdata[600];
	struct netbuf buf = { NULL };

	if (!g_NetMode || !g_NetLocalClient || g_NetLocalClient->state < CLSTATE_LOBBY) {
		return;
	}

	va_list args;
	va_start(args, fmt);
	vsnprintf(tmp, sizeof(tmp) - 1, fmt, args);
	va_end(args);

	buf.data = bufdata;
	buf.size = sizeof(bufdata);

	if (g_NetMode == NETMODE_SERVER) {
		sysLogPrintf(LOG_CHAT, "%s", tmp);
		netmsgSvcChatWrite(&buf, tmp);
	} else {
		netmsgClcChatWrite(&buf, tmp);
	}

	netSend(dst, &buf, true, NETCHAN_CONTROL);
}

void netChat(struct netclient *dst, const char *text)
{
	if (g_NetMode && g_NetLocalClient) {
		netChatPrintf(dst, "%s: %s", g_NetLocalClient->settings.name, text);
	}
}

Gfx *netDebugRender(Gfx *gdl)
{
	char tmp[384];

	if (!g_NetMode || !g_NetDebugDraw) {
		return gdl;
	}

	if (!g_CharsHandelGothicXs || !g_FontHandelGothicXs) {
		return gdl;
	}

	gdl = text0f153628(gdl);
	gSPSetExtraGeometryModeEXT(gdl++, G_ASPECT_LEFT_EXT);

	s32 x = 2;
	s32 y = viGetHeight() - 1 - 6*8;
	snprintf(tmp, sizeof(tmp), "Nettick: %u\nPing: %u\nSent: %u\nRecv: %u\nReliable frame: %u\nUnreliable frame: %u\n",
		g_NetTick, g_NetLocalClient->peer ? enet_peer_get_rtt(g_NetLocalClient->peer) : 0,
		enet_host_get_bytes_sent(g_NetHost), enet_host_get_bytes_received(g_NetHost),
		g_NetReliableFrameLen, g_NetUnreliableFrameLen);
	gdl = textRenderProjected(gdl, &x, &y, tmp, g_CharsHandelGothicXs, g_FontHandelGothicXs, 0x00ff00ff, viGetWidth(), viGetHeight(), 0, 0);

	gSPClearExtraGeometryModeEXT(gdl++, G_ASPECT_CENTER_EXT);
	gdl = text0f153780(gdl);

	return gdl;
}

s32 netShouldWaitForPlayers(void)
{
	if (!g_NetMode || !g_NetLocalClient) {
		return false;
	}
	// Solo host (or alone after others left) can return to Combat Sim immediately.
	return g_NetNumClients > 1;
}

void netEndscreenProceedToLobby(void)
{
	g_NetWaitingForPlayers = 0;

	for (s32 i = 0; i < NET_MAX_CLIENTS + 1; ++i) {
		g_NetClients[i].endscreenready = 0;
		if (g_NetClients[i].state >= CLSTATE_GAME) {
			g_NetClients[i].state = CLSTATE_LOBBY;
			g_NetClients[i].leftmatch = 0;
		}
	}

	if (g_NetLocalClient) {
		g_NetLocalClient->state = CLSTATE_LOBBY;
		g_NetLocalClient->leftmatch = 0;
	}

	// Keep simulant bits across the CI return; menutick restores humans.
	g_NetLobbyHumanSlots = g_MpSetup.chrslots & 0x00ff;
	if (g_NetMode == NETMODE_SERVER) {
		netLobbyRebuildHumanSlots();
		netMpSetupSend();
	}

	g_MpPlayerNum = 0;
	menuStop();

	mpSetPaused(MPPAUSEMODE_UNPAUSED);
	g_Vars.mplayerisrunning = false;
	g_Vars.normmplayerisrunning = false;
	g_Vars.lvmpbotlevel = 0;

	var80087260 = 3;
	titleSetNextStage(STAGE_CITRAINING);
	setNumPlayers(1);
	titleSetNextMode(TITLEMODE_SKIP);
	mainChangeToStage(STAGE_CITRAINING);
}

void netServerCheckAllHumansLeftMatch(void)
{
	if (g_NetMode != NETMODE_SERVER || g_MainIsEndscreen) {
		return;
	}
	if (!g_Vars.normmplayerisrunning || g_Vars.stagenum >= STAGE_TITLE) {
		return;
	}

	s32 ingame = 0;
	for (s32 i = 0; i < g_NetMaxClients; ++i) {
		struct netclient *cl = &g_NetClients[i];
		if (cl->state < CLSTATE_GAME || !cl->player) {
			continue;
		}
		ingame++;
		if (!cl->leftmatch) {
			return;
		}
	}

	if (ingame > 0) {
		sysLogPrintf(LOG_NOTE, "NET: all humans left match — ending stage");
		// Drop mid-match wait so mpEndMatch can push the stats endscreen.
		g_NetAwaitingRejoin = 0;
		mainEndStage();
	}
}

void netServerCheckEndscreenReady(void)
{
	if (g_NetMode != NETMODE_SERVER || !g_NetWaitingForPlayers) {
		return;
	}

	for (s32 i = 0; i < g_NetMaxClients; ++i) {
		struct netclient *cl = &g_NetClients[i];
		if (cl->state >= CLSTATE_LOBBY && !cl->endscreenready) {
			return;
		}
	}

	netbufStartWrite(&g_NetMsgRel);
	netmsgSvcMatchProceedWrite(&g_NetMsgRel);
	netSend(NULL, &g_NetMsgRel, true, NETCHAN_CONTROL);

	netEndscreenProceedToLobby();
}

void netEndscreenBeginWait(void)
{
	if (!g_NetMode || !g_NetLocalClient || g_NetWaitingForPlayers) {
		return;
	}

	g_NetWaitingForPlayers = 1;
	g_NetLocalClient->endscreenready = 1;

	if (g_NetMode == NETMODE_SERVER) {
		netServerCheckEndscreenReady();
		if (!g_NetWaitingForPlayers) {
			return; // already proceeded (only local player)
		}
	} else {
		netbufStartWrite(&g_NetMsgRel);
		netmsgClcEndscreenReadyWrite(&g_NetMsgRel, 1);
		netSend(g_NetLocalClient, &g_NetMsgRel, true, NETCHAN_CONTROL);
	}

	g_MpPlayerNum = 0;
	g_Menus[0].playernum = 0;
	menuPushRootDialog(&g_NetEndscreenWaitingDialog, MENUROOT_MPENDSCREEN);
	netWaitingUnlockMouseForMenu();
}

void netEndscreenCancelWait(void)
{
	if (!g_NetMode || !g_NetLocalClient || !g_NetWaitingForPlayers) {
		return;
	}

	g_NetWaitingForPlayers = 0;
	g_NetLocalClient->endscreenready = 0;

	if (g_NetMode == NETMODE_CLIENT) {
		netbufStartWrite(&g_NetMsgRel);
		netmsgClcEndscreenReadyWrite(&g_NetMsgRel, 0);
		netSend(g_NetLocalClient, &g_NetMsgRel, true, NETCHAN_CONTROL);
	}

	g_MpPlayerNum = 0;
	mpPushEndscreenDialog(0, 0);
}

void netEndscreenLeaveGame(void)
{
	// Host disconnect tears down every peer; clients just leave the session.
	g_NetWaitingForPlayers = 1; // ensure disconnect boots to CI + Network Game
	netDisconnect();
}

void netClientBeginAwaitRejoin(void)
{
	if (!g_NetMode || !g_NetLocalClient) {
		return;
	}
	if (g_NetLocalClient->state < CLSTATE_GAME || g_NetAwaitingRejoin) {
		return;
	}

	g_NetAwaitingRejoin = 1;
	g_NetForceRespawn = 0;
	g_NetLocalClient->leftmatch = 1;

	if (g_Vars.currentplayer) {
		g_Vars.currentplayer->aborted = true;
		// Clear carry + die immediately on this peer. Waiting for host
		// PLAYER_STATS left clients half-alive until rejoin and glitched
		// scenario items (briefcase/uplink/CTC).
		scenarioNetReleaseCarriedItems();
		if (!g_Vars.currentplayer->isdead) {
			playerDieByShooter(g_Vars.currentplayernum, true);
		}
	}

	if (g_NetMode == NETMODE_CLIENT) {
		netbufStartWrite(&g_NetMsgRel);
		netmsgClcLeftMatchWrite(&g_NetMsgRel);
		netSend(g_NetLocalClient, &g_NetMsgRel, true, NETCHAN_CONTROL);
	} else {
		netApplyLeftMatch(g_NetLocalClient);
		sysLogPrintf(LOG_NOTE, "NET: host left match (suicide, awaiting rejoin)");
	}

	// Last human End Game ends the match immediately (stats via mpEndMatch).
	// Do not overlay mid-match "Waiting for Players" on top of that.
	if (g_MainIsEndscreen || g_MpSetup.paused == MPPAUSEMODE_GAMEOVER) {
		g_NetAwaitingRejoin = 0;
		return;
	}

	g_MpPlayerNum = 0;
	g_Menus[0].playernum = 0;
	menuPushRootDialog(&g_NetWaitingPlayersDialog, MENUROOT_MPPAUSE);
	netWaitingUnlockMouseForMenu();
}

void netClientRejoinMatch(void)
{
	if (!g_NetMode || !g_NetLocalClient || !g_NetAwaitingRejoin) {
		return;
	}

	g_NetAwaitingRejoin = 0;
	g_NetLocalClient->leftmatch = 0;
	g_NetForceRespawn = 1;

	if (g_Vars.currentplayer) {
		g_Vars.currentplayer->aborted = false;
		// Ensure we don't respawn still marked as briefcase/uplink holder.
		scenarioNetReleaseCarriedItems();
		scenarioNetClearLocalHolderState();
	}

	if (g_NetMode == NETMODE_CLIENT) {
		netbufStartWrite(&g_NetMsgRel);
		netmsgClcRejoinMatchWrite(&g_NetMsgRel);
		netSend(g_NetLocalClient, &g_NetMsgRel, true, NETCHAN_CONTROL);
	} else {
		scenarioNetResyncGroundTokens();
	}

	// menuStop() does not pop dialogs — close the green wait UI and return to gameplay.
	{
		const u32 prevplayernum = g_MpPlayerNum;
		g_MpPlayerNum = 0;
		func0f0f8120();
		if (g_Menus[0].depth > 0 || g_Menus[0].curdialog) {
			menuClose();
		}
		g_MpPlayerNum = prevplayernum;
	}

	if (g_Vars.currentplayer) {
		g_Vars.currentplayer->joybutinhibit = 0xffffffff;
		playerUnpause();
	}

	mpSetPaused(MPPAUSEMODE_UNPAUSED);
}

void netClientLeaveMatch(void)
{
	if (!g_NetMode) {
		return;
	}

	// Leave from mid-match wait UI — host disconnect ends lobby for everyone.
	g_NetAwaitingRejoin = 1; // ensure disconnect boots to CI + Network Game
	g_NetReturnToNetMenu = 1;
	netDisconnect();
}

PD_CONSTRUCTOR static void netConfigInit(void)
{
	configRegisterUInt("Net.LerpTicks", &g_NetInterpTicks, 0, 600);
	configRegisterInt("Net.LagComp.Enabled", &g_NetLagCompEnabled, 0, 1);
	configRegisterUInt("Net.LagComp.MaxTicks", &g_NetLagCompMaxTicks, 0, NET_LAGCOMP_HISTORY - 1);

	configRegisterString("Net.Client.LastJoinAddr", g_NetLastJoinAddr, NET_MAX_ADDR);
	configRegisterUInt("Net.Client.InRate", &g_NetClientInRate, 0, 10 * 1024 * 1024);
	configRegisterUInt("Net.Client.OutRate", &g_NetClientOutRate, 0, 10 * 1024 * 1024);
	configRegisterUInt("Net.Client.UpdateFrames", &g_NetClientUpdateRate, 0, 60);

	configRegisterUInt("Net.Server.Port", &g_NetServerPort, 0, 0xFFFF);
	configRegisterUInt("Net.Server.InRate", &g_NetServerInRate, 0, 10 * 1024 * 1024);
	configRegisterUInt("Net.Server.OutRate", &g_NetServerOutRate, 0, 10 * 1024 * 1024);
	configRegisterUInt("Net.Server.UpdateFrames", &g_NetServerUpdateRate, 0, 60);
	configRegisterInt("Net.Server.AllowInfoQuery", &g_NetServerInfoQuery, 0, 1);
}
