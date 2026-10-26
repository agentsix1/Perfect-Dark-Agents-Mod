#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <PR/ultratypes.h>
#include "platform.h"
#include "data.h"
#include "bss.h"
#include "types.h"
#include "constants.h"
#include "game/mainmenu.h"
#include "game/menu.h"
#include "game/challenge.h"
#include "game/gamefile.h"
#include "game/filelist.h"
#include "lib/vi.h"
#include "video.h"
#include "input.h"
#include "config.h"
#include "mpsetups.h"
#include "net/net.h"

extern s32 g_MainChangeToStageNum;

extern MenuItemHandlerResult menuhandlerMainMenuCombatSimulator(s32 operation, struct menuitem *item, union handlerdata *data);
extern MenuItemHandlerResult menuhandlerMpAdvancedSetup(s32 operation, struct menuitem *item, union handlerdata *data);
extern struct menuitem g_MpPlayerSetup234MenuItems[];
extern struct menudialogdef g_NetJoinPlayerSetupMenuDialog;

static s32 g_NetMenuMaxPlayers = NET_MAX_CLIENTS;
static s32 g_NetMenuPort = NET_DEFAULT_PORT;
static s32 g_NetMenuAllowSetupChanges = 0;
static char g_NetJoinAddr[NET_MAX_ADDR + 1];
static s32 g_NetJoinAddrPtr = 0;

/* host */

static MenuItemHandlerResult menuhandlerHostMaxPlayers(s32 operation, struct menuitem *item, union handlerdata *data)
{
	switch (operation) {
	case MENUOP_GETSLIDER:
		data->slider.value = g_NetMenuMaxPlayers;
		break;
	case MENUOP_SET:
		if (data->slider.value) {
			g_NetMenuMaxPlayers = data->slider.value;
		}
		break;
	}

	return 0;
}

static MenuItemHandlerResult menuhandlerHostAllowSetupChanges(s32 operation, struct menuitem *item, union handlerdata *data)
{
	switch (operation) {
	case MENUOP_GET:
		return g_NetMenuAllowSetupChanges;
	case MENUOP_SET:
		g_NetMenuAllowSetupChanges = data->checkbox.value ? 1 : 0;
		break;
	}

	return 0;
}

static MenuItemHandlerResult menuhandlerHostPort(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (operation == MENUOP_SET) {

	}

	return 0;
}

static char *menuhandlerHostPortValue(struct menuitem *item)
{
	static char tmp[16];
	snprintf(tmp, sizeof(tmp), "%u\n", g_NetMenuPort);
	return tmp;
}

MenuItemHandlerResult menuhandlerHostStart(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (operation == MENUOP_SET) {
		g_NetAllowSetupChanges = g_NetMenuAllowSetupChanges;
		if (netStartServer(g_NetMenuPort, g_NetMenuMaxPlayers) == 0) {
			// load the setup file when entering the Combat Simulator
			mpsetupCopyAllFromPak();
			mpsetupLoadCurrentFile();
			menuhandlerMainMenuCombatSimulator(MENUOP_SET, NULL, NULL);
			menuhandlerMpAdvancedSetup(MENUOP_SET, NULL, NULL);
		}
	}

	return 0;
}

struct menuitem g_NetHostMenuItems[] = {
	{
		MENUITEMTYPE_SLIDER,
		0,
		MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"Max Players",
		NET_MAX_CLIENTS,
		menuhandlerHostMaxPlayers,
	},
	{
		MENUITEMTYPE_SELECTABLE,
		0,
		MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"Port\n",
		(uintptr_t)&menuhandlerHostPortValue,
		menuhandlerHostPort,
	},
	{
		MENUITEMTYPE_CHECKBOX,
		0,
		MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"Allow Setup Changes\n",
		0,
		menuhandlerHostAllowSetupChanges,
	},
	{
		MENUITEMTYPE_SELECTABLE,
		0,
		0,
		L_MPMENU_036, // "Start Game"
		0,
		menuhandlerHostStart,
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
		L_OPTIONS_213, // "Back"
		0,
		NULL,
	},
	{ MENUITEMTYPE_END },
};

struct menudialogdef g_NetHostMenuDialog = {
	MENUDIALOGTYPE_DEFAULT,
	(uintptr_t)"Host Network Game",
	g_NetHostMenuItems,
	NULL,
	MENUDIALOGFLAG_LITERAL_TEXT | MENUDIALOGFLAG_STARTSELECTS,
	NULL,
};

/* join */

static const char *menutextJoinAddress(struct menuitem *item)
{
	static char tmp[256 + 1];
	if (item && item->flags & MENUITEMFLAG_SELECTABLE_CENTRE) {
		// in centered dialog
		if (g_NetMode == NETMODE_NONE) {
			snprintf(tmp, sizeof(tmp), "%s_\n", g_NetJoinAddr);
		} else if (g_NetLocalClient->state == CLSTATE_CONNECTING) {
			snprintf(tmp, sizeof(tmp), "Connecting to %s...\n", g_NetJoinAddr);
		} else if (g_NetLocalClient->state == CLSTATE_AUTH) {
			snprintf(tmp, sizeof(tmp), "Authenticating with %s...\n", g_NetJoinAddr);
		} else if (g_NetLocalClient->state == CLSTATE_LOBBY) {
			snprintf(tmp, sizeof(tmp), "Opening setup...\n");
		}
	} else {
		// label
		snprintf(tmp, sizeof(tmp), "%s\n", g_NetJoinAddr);
	}
	return tmp;
}

static MenuItemHandlerResult menuhandlerJoining(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (inputKeyPressed(VK_ESCAPE)) {
		netDisconnect();
		menuPopDialog();
		return 0;
	}

	/*
	 * Always open Combat Sim setup once in the lobby so clients see live host
	 * settings (teams, scenario, options) via SVC_MPSETUP — including host-only
	 * edit mode (menus are lockable / view-only for those clients).
	 */
	if (g_NetLocalClient && g_NetLocalClient->state == CLSTATE_LOBBY) {
		menuPopDialog();
		menuhandlerMainMenuCombatSimulator(MENUOP_SET, NULL, NULL);
		menuhandlerMpAdvancedSetup(MENUOP_SET, NULL, NULL);
	}

	return 0;
}

struct menuitem g_NetJoiningMenuItems[] = {
	{
		MENUITEMTYPE_LABEL,
		0,
		MENUITEMFLAG_SELECTABLE_CENTRE,
		(uintptr_t)&menutextJoinAddress,
		0,
		NULL,
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
		MENUITEMFLAG_SELECTABLE_CENTRE | MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"ESC to abort\n",
		0,
		menuhandlerJoining,
	},
	{ MENUITEMTYPE_END },
};

struct menudialogdef g_NetJoiningDialog = {
	MENUDIALOGTYPE_SUCCESS,
	(uintptr_t)"Joining Game...",
	g_NetJoiningMenuItems,
	NULL,
	MENUDIALOGFLAG_LITERAL_TEXT | MENUDIALOGFLAG_IGNOREBACK | MENUDIALOGFLAG_STARTSELECTS,
	NULL,
};

static MenuItemHandlerResult menuhandlerEnterJoinAddress(s32 operation, struct menuitem *item, union handlerdata *data);

struct menuitem g_NetJoinAddressMenuItems[] = {
	{
		MENUITEMTYPE_LABEL,
		0,
		MENUITEMFLAG_SELECTABLE_CENTRE,
		(uintptr_t)&menutextJoinAddress,
		0,
		NULL,
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
		MENUITEMFLAG_SELECTABLE_CENTRE | MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"ESC to return\n",
		0,
		menuhandlerEnterJoinAddress,
	},
	{ MENUITEMTYPE_END },
};

struct menudialogdef g_NetJoinAddressDialog = {
	MENUDIALOGTYPE_SUCCESS,
	(uintptr_t)"Enter Address",
	g_NetJoinAddressMenuItems,
	NULL,
	MENUDIALOGFLAG_LITERAL_TEXT | MENUDIALOGFLAG_IGNOREBACK | MENUDIALOGFLAG_STARTSELECTS,
	NULL,
};

static MenuItemHandlerResult menuhandlerEnterJoinAddress(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (!menuIsDialogOpen(&g_NetJoinAddressDialog)) {
		return 0;
	}

	if (inputTextHandler(g_NetJoinAddr, NET_MAX_ADDR, &g_NetJoinAddrPtr, false) < 0) {
		// escape has been pressed, stop editing
		inputStopTextInput();
		menuPopDialog();
	}

	return 0;
}

static MenuItemHandlerResult menuhandlerJoinAddress(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (operation == MENUOP_SET) {
		inputClearLastKey();
		inputClearLastTextChar();
		inputStartTextInput();
		g_NetJoinAddrPtr = strlen(g_NetJoinAddr);
		menuPushDialog(&g_NetJoinAddressDialog);
	}

	return 0;
}

MenuItemHandlerResult menuhandlerJoinStart(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (operation == MENUOP_SET) {
		if (netStartClient(g_NetJoinAddr) == 0) {
			menuPushDialog(&g_NetJoiningDialog);
		}
	}

	return 0;
}

static MenuItemHandlerResult menuhandlerJoinPlayerSetup(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (operation == MENUOP_SET) {
		// load MP player file
		filelistCreate(0, FILETYPE_MPPLAYER);
		filelistsTick();
		menuPushDialog(&g_NetJoinPlayerSetupMenuDialog);
	}

	return 0;
}

struct menuitem g_NetJoinMenuItems[] = {
	{
		MENUITEMTYPE_SELECTABLE,
		0,
		MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"Address:   \n",
		(uintptr_t)&menutextJoinAddress,
		menuhandlerJoinAddress,
	},
	{
		MENUITEMTYPE_SELECTABLE,
		0,
		0,
		L_MPMENU_028, // "Player Setup"
		0,
		menuhandlerJoinPlayerSetup,
	},
	{
		MENUITEMTYPE_SELECTABLE,
		0,
		0,
		L_MPMENU_036, // "Start Game"
		0,
		menuhandlerJoinStart,
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
		L_OPTIONS_213, // "Back"
		0,
		NULL,
	},
	{ MENUITEMTYPE_END },
};

struct menudialogdef g_NetJoinMenuDialog = {
	MENUDIALOGTYPE_DEFAULT,
	(uintptr_t)"Join Network Game",
	g_NetJoinMenuItems,
	NULL,
	MENUDIALOGFLAG_LITERAL_TEXT | MENUDIALOGFLAG_STARTSELECTS,
	NULL,
};

/* main */

MenuItemHandlerResult menuhandlerHostGame(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (operation == MENUOP_SET) {
		g_NetMenuPort = g_NetServerPort;
		g_NetMenuMaxPlayers = g_NetMaxClients;
		g_NetMenuAllowSetupChanges = g_NetAllowSetupChanges;
		menuPushDialog(&g_NetHostMenuDialog);
	}

	return 0;
}

MenuItemHandlerResult menuhandlerJoinGame(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (operation == MENUOP_SET) {
		if (g_NetJoinAddr[0] == '\0') {
			strncpy(g_NetJoinAddr, g_NetLastJoinAddr, NET_MAX_ADDR);
			g_NetJoinAddrPtr = strlen(g_NetJoinAddr);
		}
		menuPushDialog(&g_NetJoinMenuDialog);
	}

	return 0;
}

struct menuitem g_NetMenuItems[] = {
	{
		MENUITEMTYPE_SELECTABLE,
		0,
		MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"Host Game\n",
		0,
		menuhandlerHostGame,
	},
	{
		MENUITEMTYPE_SELECTABLE,
		0,
		MENUITEMFLAG_LITERAL_TEXT,
		(uintptr_t)"Join Game\n",
		0,
		menuhandlerJoinGame,
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
		L_OPTIONS_213, // "Back"
		0,
		NULL,
	},
	{ MENUITEMTYPE_END },
};

struct menudialogdef g_NetMenuDialog = {
	MENUDIALOGTYPE_DEFAULT,
	(uintptr_t)"Network Game",
	g_NetMenuItems,
	NULL,
	MENUDIALOGFLAG_MPLOCKABLE | MENUDIALOGFLAG_LITERAL_TEXT | MENUDIALOGFLAG_STARTSELECTS,
	NULL,
};

void netPollReturnToNetMenu(void)
{
	if (!g_NetReturnToNetMenu) {
		return;
	}

	/* Wait until CI has finished loading — pushing during the old MP stage
	 * (or before menuReset) leaves an unpaused bobbing view under blur menus. */
	if (g_MainChangeToStageNum >= 0) {
		return;
	}
	if (g_Vars.stagenum != STAGE_CITRAINING && g_Vars.stagenum != STAGE_4MBMENU) {
		return;
	}
	if (g_Vars.lvframenum < 4) {
		return;
	}

	g_NetReturnToNetMenu = 0;
	g_MpPlayerNum = 0;
	g_Menus[0].playernum = 0;
	g_Vars.mpsetupmenu = 0;
	g_Vars.usingadvsetup = false;

	/* Kill any leftover match shake so the viewport cannot keep flipping. */
	g_ExplosionShakeTotalTimer = 0;
	g_ExplosionShakeIntensityTimer = 0;
	viShake(0);

	/* Host-left clients can still hold a challenge lock (red MPLOCKABLE). */
	challengeUnsetCurrent();
	challengeRemovePlayerLock();

	/* Same path as using the CI computer — pauses LV so head-bob cannot
	 * make every menu look like it is vibrating up and down. */
	func0f0f85e0(&g_CiMenuViaPcMenuDialog, MENUROOT_MAINMENU);
	menuPushDialog(&g_NetMenuDialog);
}

/* post-match / mid-match wait UI */

void netWaitingUnlockMouseForMenu(void)
{
	// Force unlock even when Mouse Lock Mode is ON — menu clicks need a cursor.
	inputLockMouse(0);
	inputMouseShowCursor(1);
}

static MenuItemHandlerResult menuhandlerNetWaitLeave(s32 operation, struct menuitem *item, union handlerdata *data)
{
	if (operation == MENUOP_SET) {
		if (g_NetAwaitingRejoin) {
			netClientLeaveMatch();
		} else {
			netEndscreenLeaveGame();
		}
	}
	return 0;
}

static MenuItemHandlerResult menuhandlerNetWaitRejoin(s32 operation, struct menuitem *item, union handlerdata *data)
{
	// End Game sits you out until the match finishes — no mid-match rejoin.
	if (operation == MENUOP_CHECKHIDDEN) {
		return true;
	}

	(void)item;
	(void)data;
	return 0;
}

static MenuDialogHandlerResult menudialogNetWaitingPlayers(s32 operation, struct menudialogdef *dialogdef, union handlerdata *data)
{
	if (operation == MENUOP_OPEN || operation == MENUOP_TICK) {
		netWaitingUnlockMouseForMenu();
	}
	return 0;
}

/* Mid-match End Game sit-out (MENUROOT_MPPAUSE). */
struct menuitem g_NetWaitingPlayersMenuItems[] = {
	{
		MENUITEMTYPE_LABEL,
		0,
		MENUITEMFLAG_LESSLEFTPADDING | MENUITEMFLAG_LITERAL_TEXT | MENUITEMFLAG_SELECTABLE_CENTRE,
		(uintptr_t)"Sitting out until the match ends...\n",
		0,
		NULL,
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
		MENUITEMFLAG_LITERAL_TEXT | MENUITEMFLAG_SELECTABLE_CENTRE,
		(uintptr_t)"Rejoin Game\n",
		0,
		menuhandlerNetWaitRejoin,
	},
	{
		MENUITEMTYPE_SELECTABLE,
		0,
		MENUITEMFLAG_LITERAL_TEXT | MENUITEMFLAG_SELECTABLE_CENTRE,
		(uintptr_t)"Leave Game\n",
		0,
		menuhandlerNetWaitLeave,
	},
	{ MENUITEMTYPE_END },
};

struct menudialogdef g_NetWaitingPlayersDialog = {
	MENUDIALOGTYPE_SUCCESS,
	(uintptr_t)"Waiting for Players",
	g_NetWaitingPlayersMenuItems,
	menudialogNetWaitingPlayers,
	MENUDIALOGFLAG_LITERAL_TEXT | MENUDIALOGFLAG_IGNOREBACK | MENUDIALOGFLAG_STARTSELECTS,
	NULL,
};

/* After game-over stats — peers still on their endscreen. */
struct menuitem g_NetEndscreenWaitingMenuItems[] = {
	{
		MENUITEMTYPE_LABEL,
		0,
		MENUITEMFLAG_LESSLEFTPADDING | MENUITEMFLAG_LITERAL_TEXT | MENUITEMFLAG_SELECTABLE_CENTRE,
		(uintptr_t)"Players checking stats...\n",
		0,
		NULL,
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
		MENUITEMFLAG_LITERAL_TEXT | MENUITEMFLAG_SELECTABLE_CENTRE,
		(uintptr_t)"Leave Game\n",
		0,
		menuhandlerNetWaitLeave,
	},
	{ MENUITEMTYPE_END },
};

struct menudialogdef g_NetEndscreenWaitingDialog = {
	MENUDIALOGTYPE_SUCCESS,
	(uintptr_t)"Waiting...",
	g_NetEndscreenWaitingMenuItems,
	menudialogNetWaitingPlayers,
	MENUDIALOGFLAG_LITERAL_TEXT | MENUDIALOGFLAG_IGNOREBACK | MENUDIALOGFLAG_STARTSELECTS,
	NULL,
};

struct menudialogdef g_NetJoinPlayerSetupMenuDialog = {
	MENUDIALOGTYPE_DEFAULT,
	L_MPMENU_028, // "Player Setup"
	g_MpPlayerSetup234MenuItems,
	NULL,
	MENUDIALOGFLAG_DROPOUTONCLOSE | MENUDIALOGFLAG_STARTSELECTS,
	NULL,
};
