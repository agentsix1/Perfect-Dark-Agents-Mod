#ifndef _IN_NETMSG_H
#define _IN_NETMSG_H

#include <PR/ultratypes.h>
#include "net/net.h"
#include "net/netbuf.h"

#define SVC_BAD           0x00 // trash
#define SVC_NOP           0x01 // does nothing
#define SVC_AUTH          0x02 // auth response, sent in response to CLC_AUTH
#define SVC_CHAT          0x03 // chat message
#define SVC_STAGE_START   0x10 // start level
#define SVC_STAGE_END     0x11 // end level
#define SVC_MPSETUP       0x12 // combat sim setup sync (lobby)
#define SVC_MATCH_PROCEED 0x13 // all players left endscreen — return to lobby
#define SVC_PLAYER_MOVE   0x20 // player movement and inputs
#define SVC_PLAYER_GUNS   0x21 // player gun state
#define SVC_PLAYER_STATS  0x22 // player stats (health etc)
#define SVC_PROP_MOVE     0x30 // prop movement
#define SVC_PROP_SPAWN    0x31 // new prop spawned
#define SVC_PROP_DAMAGE   0x32 // prop was damaged
#define SVC_PROP_PICKUP   0x33 // prop was picked up
#define SVC_PROP_USE      0x34 // door/lift/etc was used
#define SVC_PROP_DOOR     0x35 // door state changed
#define SVC_PROP_LIFT     0x36 // lift state changed
#define SVC_WALLHIT       0x37 // bullet hole / splat / wallhit
#define SVC_PROP_GLASS    0x38 // glass or door glass destroyed
#define SVC_CHR_DAMAGE    0x42 // chr was damaged
#define SVC_CHR_DISARM    0x43 // chr's weapons were dropped
#define SVC_SIM_MOVE      0x44 // host-authoritative simulant pose
#define SVC_CHR_BEAM      0x45 // bullet tracer / beam (simulant shots)
#define SVC_SCENARIO      0x46 // host-authoritative scenario state (HTB/HTM/CTC/PAC/KOH)
#define SVC_PROP_REGEN    0x47 // host-authoritative weapon/ammo/shield respawn
#define SVC_SCENARIO_EVENT 0x48 // scenario SFX / HUD / positional audio for clients
#define SVC_EXPLOSION     0x49 // host-authoritative explosion FX (mines/grenades/laptop/rockets)

#define NETGLASS_DESTROY     0 // glassDestroy
#define NETGLASS_DOORDESTROY 1 // doorDestroyGlass

#define CLC_BAD      0x00 // trash
#define CLC_NOP      0x01 // does nothing
#define CLC_AUTH     0x02 // auth request, sent immediately after connecting
#define CLC_CHAT     0x03 // chat message
#define CLC_MOVE     0x04 // player input
#define CLC_SETTINGS 0x05 // player settings changed
#define CLC_MPSETUP  0x06 // client proposes combat sim setup change
#define CLC_ENDSCREEN_READY 0x07 // client dismissed endscreen (u8 ready: 1=wait, 0=rejoin)
#define CLC_LEFT_MATCH  0x08 // client left mid-match; still connected, awaiting rejoin
#define CLC_REJOIN_MATCH 0x09 // client wants back into the ongoing match

u32 netmsgClcAuthWrite(struct netbuf *dst);
u32 netmsgClcAuthRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgClcChatWrite(struct netbuf *dst, const char *str);
u32 netmsgClcChatRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgClcMoveWrite(struct netbuf *dst);
u32 netmsgClcMoveRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgClcSettingsWrite(struct netbuf *dst);
u32 netmsgClcSettingsRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgClcMpSetupWrite(struct netbuf *dst);
u32 netmsgClcMpSetupRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgClcEndscreenReadyWrite(struct netbuf *dst, u8 ready);
u32 netmsgClcEndscreenReadyRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgClcLeftMatchWrite(struct netbuf *dst);
u32 netmsgClcLeftMatchRead(struct netbuf *src, struct netclient *srccl);
void netApplyLeftMatch(struct netclient *cl);
u32 netmsgClcRejoinMatchWrite(struct netbuf *dst);
u32 netmsgClcRejoinMatchRead(struct netbuf *src, struct netclient *srccl);

u32 netmsgSvcAuthWrite(struct netbuf *dst, struct netclient *authcl);
u32 netmsgSvcAuthRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcChatWrite(struct netbuf *dst, const char *str);
u32 netmsgSvcChatRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcStageStartWrite(struct netbuf *dst);
u32 netmsgSvcStageStartRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcStageEndWrite(struct netbuf *dst);
u32 netmsgSvcStageEndRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcMpSetupWrite(struct netbuf *dst);
u32 netmsgSvcMpSetupRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcMatchProceedWrite(struct netbuf *dst);
u32 netmsgSvcMatchProceedRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPlayerMoveWrite(struct netbuf *dst, struct netclient *movecl);
u32 netmsgSvcPlayerMoveRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPlayerGunsWrite(struct netbuf *dst, struct netclient *actcl);
u32 netmsgSvcPlayerGunsRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPlayerStatsWrite(struct netbuf *dst, struct netclient *actcl);
u32 netmsgSvcPlayerStatsRead(struct netbuf *src, struct netclient *srccl);

#define NETGUN_FLASH_L   (1 << 0)
#define NETGUN_FLASH_R   (1 << 1)
#define NETGUN_FIRING_L  (1 << 2)
#define NETGUN_FIRING_R  (1 << 3)
#define NETGUN_AIMMODE   (1 << 4)
u32 netmsgSvcPropSpawnWrite(struct netbuf *dst, struct prop *prop);
u32 netmsgSvcPropSpawnRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPropMoveWrite(struct netbuf *dst, struct prop *prop, struct coord *initrot);
u32 netmsgSvcPropMoveRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPropDamageWrite(struct netbuf *dst, struct prop *prop, f32 damage, struct coord *pos, s32 weaponnum, s32 playernum);
u32 netmsgSvcPropDamageRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPropPickupWrite(struct netbuf *dst, struct netclient *actcl, struct prop *prop, const s32 tickop, struct prop *holder);
u32 netmsgSvcPropPickupRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPropUseWrite(struct netbuf *dst, struct prop *prop, struct netclient *usercl, const s32 tickop);
u32 netmsgSvcPropUseRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPropDoorWrite(struct netbuf *dst, struct prop *prop, struct netclient *usercl);
u32 netmsgSvcPropDoorRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPropLiftWrite(struct netbuf *dst, struct prop *prop);
u32 netmsgSvcPropLiftRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcWallhitWrite(struct netbuf *dst, struct coord *relpos, struct coord *arg1, struct coord *arg2,
		s16 texnum, RoomNum room, struct prop *objprop, struct prop *chrprop,
		s8 mtxindex, s8 arg10, struct chrdata *chr,
		f32 width, f32 height, u8 minalpha, u8 maxalpha,
		s32 rotdeg, u32 timermax, u32 timerspeed, bool xlu,
		const u8 colours[4][4]);
u32 netmsgSvcWallhitRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPropGlassWrite(struct netbuf *dst, struct prop *prop, u8 kind);
u32 netmsgSvcPropGlassRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcChrDamageWrite(struct netbuf *dst, struct chrdata *chr, f32 damage, struct coord *vector, struct gset *gset, struct prop *aprop, s32 hitpart, bool damageshield, struct prop *prop2, s32 side, s16 *arg11, bool explosion, struct coord *explosionpos);
u32 netmsgSvcChrDamageRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcChrDisarmWrite(struct netbuf *dst, struct chrdata *chr, struct prop *attacker, u8 weaponnum, f32 wpndamage, struct coord *wpnpos);
u32 netmsgSvcChrDisarmRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcSimMoveWrite(struct netbuf *dst, u8 slot, struct chrdata *chr);
u32 netmsgSvcSimMoveRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcChrBeamWrite(struct netbuf *dst, struct prop *chrprop, u8 handnum, s32 weaponnum, struct coord *from, struct coord *to);
u32 netmsgSvcChrBeamRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcScenarioWrite(struct netbuf *dst);
u32 netmsgSvcScenarioRead(struct netbuf *src, struct netclient *srccl);
u32 netmsgSvcPropRegenWrite(struct netbuf *dst, struct prop *prop);
u32 netmsgSvcPropRegenRead(struct netbuf *src, struct netclient *srccl);

#define NETSCEN_SFX      0
#define NETSCEN_HUD      1
#define NETSCEN_PSCREATE 2
#define NETSCEN_PSSTOP   3
#define NETSCEN_SND      4

u32 netmsgSvcScenarioEventWrite(struct netbuf *dst, u8 kind, s32 sfx,
		s32 playernum, u8 msgtype, u8 flags, struct prop *prop, const char *text);
u32 netmsgSvcScenarioEventRead(struct netbuf *src, struct netclient *srccl);

u32 netmsgSvcExplosionWrite(struct netbuf *dst, struct coord *pos, RoomNum *rooms,
		s16 type, s32 playernum);
u32 netmsgSvcExplosionRead(struct netbuf *src, struct netclient *srccl);

#endif
