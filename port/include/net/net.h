#ifndef _IN_NET_H
#define _IN_NET_H

#include "types.h"
#include "constants.h"
#include "net/netbuf.h"

#define NET_PROTOCOL_VER 34

#define NET_QUERY_MAGIC "PDQM\x01"

#define NET_MAX_CLIENTS MAX_PLAYERS
#define NET_MAX_NAME MAX_PLAYERNAME
#define NET_MAX_ADDR 256

#define NET_BUFSIZE 1440

#define NET_DEFAULT_PORT 27100

#define NET_NULL_CLIENT 0xFF
#define NET_NULL_PROP 0

#define NETCHAN_DEFAULT 0
#define NETCHAN_CONTROL 1
#define NETCHAN_COUNT 2

#define DISCONNECT_UNKNOWN  0
#define DISCONNECT_SHUTDOWN 1
#define DISCONNECT_VERSION 2
#define DISCONNECT_KICKED 3
#define DISCONNECT_BANNED 4
#define DISCONNECT_TIMEOUT 5
#define DISCONNECT_FULL 6
#define DISCONNECT_LATE 7
#define DISCONNECT_FILES 8

#define CLSTATE_DISCONNECTED 0
#define CLSTATE_CONNECTING 1
#define CLSTATE_AUTH 2
#define CLSTATE_LOBBY 3
#define CLSTATE_GAME 4

#define UCMD_FIRE (1 << 0)
#define UCMD_ACTIVATE (1 << 1)
#define UCMD_RELOAD (1 << 2)
#define UCMD_AIMMODE (1 << 3)
#define UCMD_DUCK (1 << 4)
#define UCMD_SQUAT (1 << 5)
#define UCMD_ZOOMIN (1 << 6)
#define UCMD_SELECT (1 << 7)
#define UCMD_SELECT_DUAL (1 << 8)
#define UCMD_EYESSHUT (1 << 9)
#define UCMD_SECONDARY (1 << 10)
#define UCMD_RESPAWN (1 << 27)
#define UCMD_CHAT (1 << 28)
#define UCMD_IMPORTANT_MASK (UCMD_FIRE | UCMD_ACTIVATE | UCMD_RELOAD | UCMD_AIMMODE | UCMD_SELECT | UCMD_SELECT_DUAL | UCMD_SECONDARY)
#define UCMD_FL_FORCEPOS (1 << 29)
#define UCMD_FL_FORCEANGLE (1 << 30)
#define UCMD_FL_FORCEGROUND (1 << 31)
#define UCMD_FL_FORCEMASK (UCMD_FL_FORCEPOS | UCMD_FL_FORCEANGLE | UCMD_FL_FORCEGROUND)

struct netplayermove {
	u32 tick; // g_NetTIck value when this struct was written; if 0, this struct is invalid
	u32 ucmd; // player commands (UCMD_)
	f32 leanofs; // analog lean value (-1 .. 1; equal to player->swaytarget / 75.f)
	f32 crouchofs; // analog crouch value (-90 for SQUAT, 0 for STAND; player->crouchofs)
	f32 zoomfov; // manual zoom fov for the current gun; synced only if UCMD_AIMING is set
	f32 movespeed[2]; // move inputs, [0] is forward, [1] is sideways; used mostly for animation
	f32 angles[2]; // view angles, [0] is theta, [1] is verta
	f32 crosspos[2]; // crosshair position in aiming mode; normalized to default aspect ratio
	s8 weaponnum; // switch to this weapon if UCMD_SELECT is set
	struct coord pos; // player position at g_NetTick == tick
};

struct netclient {
	struct _ENetPeer *peer;
	u32 id; // remote client number, server is always 0, even on clients
	u32 state; // CLSTATE_
	u32 flags; // CLFLAG_

	struct {
		char name[NET_MAX_NAME];
		u16 options;
		u8 headnum;
		u8 bodynum;
		u8 team;
		f32 fovy;
		f32 fovzoommult;
	} settings;

	struct mpplayerconfig *config;
	struct player *player;
	u8 playernum;

	struct netplayermove outmove[2]; // last 2 outgoing player inputs, newest one first
	struct netplayermove inmove[2]; // last 2 incoming player inputs, newest one first
	u32 inmovetick; // last inmove tick which was applied to the player
	u32 outmoveack; // last acked outmove tick
	u32 forcetick; // tick on which the client's position was forced, or 0 if not forcing
	u32 lerpticks; // how many ticks we've been lerping the position

	u8 endscreenready; // dismissed endscreen; waiting for others before lobby
	u8 leftmatch; // mid-match leave; awaiting rejoin (still connected)

	u8 lastgunflags; // last SVC_PLAYER_GUNS flags we broadcast (change detection)
	u8 gunflagsvalid; // whether lastgunflags has been seeded

	// Last guns snapshot received from the server (clients apply to remotes)
	u8 netgunflags;
	u8 netgunvalid;
	u8 netgunsoundflags; // last FLASH bits we already played SFX for (edge detect)
	f32 netshootrotx;
	f32 netshootroty;
	// One-frame muzzle flashes are easy to miss on remotes; keep TP flash lit briefly.
	u8 gunflashlatch[2]; // HAND_RIGHT, HAND_LEFT — remaining visible frames

	struct netbuf out; // outbound messages are written here, except broadcasts
	struct netbuf in; // incoming packets are fed here

	u8 out_data[NET_BUFSIZE]; // buffer for out
};

extern s32 g_NetMode;

extern s32 g_NetJoinLatch;
extern s32 g_NetHostLatch;

// net frame, ticks at 60 fps, starts at 0 when the server is started
extern u32 g_NetTick;
extern u32 g_NetNextSyncId;

extern u64 g_NetRngSeeds[2];
extern u32 g_NetRngLatch;

extern u32 g_NetInterpTicks;
extern u32 g_NetServerPort;
extern char g_NetLastJoinAddr[NET_MAX_ADDR + 1];

extern s32 g_NetDebugDraw;

extern s32 g_NetMaxClients;
extern s32 g_NetNumClients;
extern struct netclient g_NetClients[NET_MAX_CLIENTS + 1]; // last is an extra temporary client
extern struct netclient *g_NetLocalClient;

extern struct netbuf g_NetMsg;
extern struct netbuf g_NetMsgRel;

// set while applying a net message so local handlers don't re-broadcast / early-out
extern s32 g_NetApplyingWallhit;
extern s32 g_NetApplyingGlass;
extern s32 g_NetApplyingMpSetup;
extern s32 g_NetApplyingPropSpawn;
/** Client applying SVC_CHR_DAMAGE — skip host-only kill side effects (drops/stats). */
extern s32 g_NetApplyingChrDamage;
/** Client applying SVC_EXPLOSION — allow explosionCreate without re-broadcast. */
extern s32 g_NetApplyingExplosion;
// When set, wallhitCreate uses these vertex colours instead of rolling new RNG ones.
extern s32 g_NetWallhitColoursValid;
extern u8 g_NetWallhitColours[4][4];

// When set, all players in the lobby may edit Combat Sim setup; changes are synced.
extern s32 g_NetAllowSetupChanges;

// Set on client when the host drops the lobby; menutick opens Network Game.
extern s32 g_NetReturnToNetMenu;

// Local player dismissed the match endscreen and is waiting for peers.
extern s32 g_NetWaitingForPlayers;

// Local player left mid-match but stays connected (green Waiting for Players UI).
extern s32 g_NetAwaitingRejoin;
// OR UCMD_RESPAWN into the next outbound move after Rejoin.
extern s32 g_NetForceRespawn;

const char *netFormatClientAddr(const struct netclient *cl);

void netInit(void);
s32 netDisconnect(void);
void netStartFrame(void);
void netEndFrame(void);

s32 netStartServer(u16 port, s32 maxclients);
s32 netStartClient(const char *addr);

u32 netSend(struct netclient *dstcl, struct netbuf *buf, const s32 reliable, const s32 chan);

void netChat(struct netclient *dst, const char *text);
void netChatPrintf(struct netclient *dst, const char *fmt, ...);

void netServerStageStart(void);
void netServerStageEnd(void);
void netServerKick(struct netclient *cl, const u32 reason);

struct netclient *netClientForPlayerNum(s32 playernum);

void netClientSyncRng(void);
void netClientSettingsChanged(void);
s32 netGetLocalConfigIndex(void);
void netApplyRemoteGunVisuals(struct player *pl);

void netMpSetupTick(void);
void netMpSetupSend(void);
void netMpSetupApply(struct netbuf *src, struct netclient *srccl);
u32 netMpSetupReadSeq(struct netbuf *src);

// Host-authoritative simulant configs (lobby MPSETUP + match start).
void netMpSimulantsWrite(struct netbuf *dst);
void netMpSimulantsSkip(struct netbuf *src);
void netMpSimulantsApply(struct netbuf *src);

// Lobby human roster (names/teams/slots) embedded in MPSETUP.
void netMpPlayersWrite(struct netbuf *dst);
void netMpPlayersSkip(struct netbuf *src);
void netMpPlayersApply(struct netbuf *src);
// Rebuild host lobby human chrslots + push names into PlayerConfigsArray.
void netLobbyRebuildHumanSlots(void);
// Last synced human occupancy bits (clients must not wipe these in menutick).
extern u16 g_NetLobbyHumanSlots;

// Client-side: after host leaves setup, open the Network Game menu.
void netPollReturnToNetMenu(void);

// Post-match: wait until every connected player has left the endscreen.
s32 netShouldWaitForPlayers(void);
void netEndscreenBeginWait(void);
void netEndscreenCancelWait(void); // "Rejoin Game" — back to endscreen
void netEndscreenLeaveGame(void);  // disconnect (host drops everyone)
void netEndscreenProceedToLobby(void);
void netServerCheckEndscreenReady(void);

// Unlock mouse for net waiting dialogs (works even when Mouse Lock Mode is ON).
void netWaitingUnlockMouseForMenu(void);

// Mid-match End Game: sit out until the match ends (Leave disconnects; rejoin disabled).
void netClientBeginAwaitRejoin(void);
void netClientRejoinMatch(void);
void netClientLeaveMatch(void);

void netPlayersAllocate(void);
void netSyncIdsAllocate(void);

/** Assign a fresh syncid if prop still has 0 (mid-match drops, etc.). */
void netPropEnsureSyncId(struct prop *prop);
/** Set/clear syncid and keep the O(1) lookup hash in sync. */
void netPropSetSyncId(struct prop *prop, u32 syncid);
void netPropClearSyncId(struct prop *prop);
/** O(1) average syncid → prop (replaces linear pool scans). */
struct prop *netPropFindBySyncId(u32 syncid);
void netServerBroadcastScenario(void);
void netServerBroadcastScenarioImmediate(void);

/**
 * Map local playernum ↔ host-stable MP chr index.
 * Clients force local to playernum 0; these keep g_MpAllChrPtrs / PAC victims
 * aligned with the host's ordering (server playernum for humans).
 */
s32 netMpChrIndexForPlayernum(s32 playernum);
s32 netPlayernumForMpChrIndex(s32 mpchr);

// When every connected human has End Game'd (leftmatch), end the match.
void netServerCheckAllHumansLeftMatch(void);

/**
 * Host-authoritative simulant presentation.
 * Fields are named for what the client must show, not AI internals.
 */
struct netsimsnap {
	u8 valid;
	u8 dirty; // new packet received; cleared after apply
	struct coord pos;
	RoomNum rooms[8];
	f32 model_yaw; // on-screen body facing (modelGetChrRotY after host movement)
	f32 look_yaw;  // aibot look intent (radians); kept in sync for local readers
	s16 anim_id;
	u8 anim_flip;
	u8 anim_looping;
	f32 anim_frame;
	f32 anim_speed;
	f32 anim_loopframe;
	f32 aim_pitch; // chr->aimendback
	f32 aim_yaw;   // chr->aimendsideback
	u8 weaponnum;
	u8 gunfunc;
	u8 gunflags;
	u8 dualweapons;
	u8 crouchpos; // CROUCHPOS_* (informational / future use)
	f32 cshield;  // for shield-hit FX / correct damage absorb on client
};

extern struct netsimsnap g_NetSimSnapshot[MAX_BOTS];

s32 netSimSlotForChr(struct chrdata *chr);
void netSimReceiveSnapshot(u8 slot, struct netsimsnap *snap);
void netClientSimPrepare(struct chrdata *chr);
/** Apply latched client sim weapon swaps (call before propsTick list walk). */
void netClientSimFlushEquips(void);
void netClientSimApply(struct chrdata *chr);
/** Re-assert host XYZ/rooms after chrTick overwrote local ground physics. */
void netClientSimApplyPose(struct chrdata *chr);
/** Plant living client sim on floor under host XZ; keeps manground/chrinfo coherent. */
void netClientSimSyncGround(struct chrdata *chr, struct netsimsnap *snap);
/** Living host snap while client still shows dead — clear death and hide until Apply. */
void netClientSimForceRespawnFromSnap(struct chrdata *chr, struct netsimsnap *snap);
/** Detach + mark DELETING; reaped at start of propsTick (never mid-list). */
void netDeferPropDelete(struct prop *prop);
void netReapDeferredDeletes(void);
/** True if prop is waiting on the deferred-delete queue (not yet propFree'd). */
s32 netPropIsQueuedForDelete(const struct prop *prop);

Gfx *netDebugRender(Gfx *gdl);

#endif // _IN_NET_H
