#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

#include "platform.h"
#include "system.h"
#include "net/net.h"
#include "net/netmsg.h"
#include "net/netdbg.h"

#ifdef PLATFORM_WIN32
#include <direct.h>
#include <process.h>
#define NETDBG_MKDIR(path) _mkdir(path)
#define NETDBG_GETPID() _getpid()
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define NETDBG_MKDIR(path) mkdir(path, 0755)
#define NETDBG_GETPID() getpid()
#endif

#define NETDBG_ROTATE_BYTES (64u * 1024u * 1024u)
#define NETDBG_FLUSH_EVERY 32

static FILE *s_fp;
static char s_path[2048];
static char s_basepath[2048]; /* without -partN */
static s32 s_enabled;
static s32 s_force;
static u32 s_lines_since_flush;
static u32 s_part;
static u64 s_bytes_written;

static const char *netdbgModeStr(void)
{
	if (g_NetMode == NETMODE_CLIENT) {
		return "client";
	}
	if (g_NetMode == NETMODE_SERVER) {
		return "server";
	}
	return "none";
}

const char *netdbgSvcName(u8 msgid)
{
	switch (msgid) {
	case SVC_NOP: return "SVC_NOP";
	case SVC_AUTH: return "SVC_AUTH";
	case SVC_CHAT: return "SVC_CHAT";
	case SVC_STAGE_START: return "SVC_STAGE_START";
	case SVC_STAGE_END: return "SVC_STAGE_END";
	case SVC_MPSETUP: return "SVC_MPSETUP";
	case SVC_MATCH_PROCEED: return "SVC_MATCH_PROCEED";
	case SVC_PLAYER_MOVE: return "SVC_PLAYER_MOVE";
	case SVC_PLAYER_GUNS: return "SVC_PLAYER_GUNS";
	case SVC_PLAYER_STATS: return "SVC_PLAYER_STATS";
	case SVC_PROP_MOVE: return "SVC_PROP_MOVE";
	case SVC_PROP_SPAWN: return "SVC_PROP_SPAWN";
	case SVC_PROP_DAMAGE: return "SVC_PROP_DAMAGE";
	case SVC_PROP_PICKUP: return "SVC_PROP_PICKUP";
	case SVC_PROP_USE: return "SVC_PROP_USE";
	case SVC_PROP_DOOR: return "SVC_PROP_DOOR";
	case SVC_PROP_LIFT: return "SVC_PROP_LIFT";
	case SVC_WALLHIT: return "SVC_WALLHIT";
	case SVC_PROP_GLASS: return "SVC_PROP_GLASS";
	case SVC_CHR_DAMAGE: return "SVC_CHR_DAMAGE";
	case SVC_CHR_DISARM: return "SVC_CHR_DISARM";
	case SVC_SIM_MOVE: return "SVC_SIM_MOVE";
	case SVC_CHR_BEAM: return "SVC_CHR_BEAM";
	case SVC_SCENARIO: return "SVC_SCENARIO";
	case SVC_PROP_REGEN: return "SVC_PROP_REGEN";
	case SVC_SCENARIO_EVENT: return "SVC_SCENARIO_EVENT";
	case SVC_EXPLOSION: return "SVC_EXPLOSION";
	default: return "SVC_UNKNOWN";
	}
}

const char *netdbgClcName(u8 msgid)
{
	switch (msgid) {
	case CLC_NOP: return "CLC_NOP";
	case CLC_AUTH: return "CLC_AUTH";
	case CLC_CHAT: return "CLC_CHAT";
	case CLC_MOVE: return "CLC_MOVE";
	case CLC_SETTINGS: return "CLC_SETTINGS";
	case CLC_MPSETUP: return "CLC_MPSETUP";
	case CLC_ENDSCREEN_READY: return "CLC_ENDSCREEN_READY";
	case CLC_LEFT_MATCH: return "CLC_LEFT_MATCH";
	case CLC_REJOIN_MATCH: return "CLC_REJOIN_MATCH";
	default: return "CLC_UNKNOWN";
	}
}

static void netdbgEscape(char *dst, size_t dstlen, const char *src)
{
	size_t o = 0;

	if (!dst || dstlen == 0) {
		return;
	}
	if (!src) {
		dst[0] = '\0';
		return;
	}
	while (*src && o + 2 < dstlen) {
		const unsigned char c = (unsigned char)*src++;
		if (c == '"' || c == '\\') {
			if (o + 3 >= dstlen) {
				break;
			}
			dst[o++] = '\\';
			dst[o++] = (char)c;
		} else if (c < 0x20) {
			/* skip control chars */
		} else {
			dst[o++] = (char)c;
		}
	}
	dst[o] = '\0';
}

static void netdbgBuildPath(char *out, size_t outlen, u32 part)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);
	char stamp[64];
	const int pid = (int)NETDBG_GETPID();

	if (tm) {
		strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tm);
	} else {
		snprintf(stamp, sizeof(stamp), "unknown");
	}

	if (part <= 1) {
		snprintf(out, outlen, "%s/pd-net-%s-P%d.ndjson", s_basepath, stamp, pid);
	} else {
		snprintf(out, outlen, "%s/pd-net-%s-P%d-part%u.ndjson", s_basepath, stamp, pid, part);
	}
}

static s32 netdbgTryOpenIn(const char *dir)
{
	char mkdirbuf[2048];

	snprintf(mkdirbuf, sizeof(mkdirbuf), "%s", dir);
	NETDBG_MKDIR(mkdirbuf); /* ignore EEXIST */

	snprintf(s_basepath, sizeof(s_basepath), "%s", dir);
	s_part = 1;
	netdbgBuildPath(s_path, sizeof(s_path), s_part);

	s_fp = fopen(s_path, "wb");
	return s_fp != NULL;
}

static void netdbgWriteHeader(void)
{
	char timestr[64];
	char cwd[1024];
	char esc[1100];
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);

	if (tm) {
		strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", tm);
	} else {
		snprintf(timestr, sizeof(timestr), "?");
	}

	cwd[0] = '\0';
#ifdef PLATFORM_WIN32
	_getcwd(cwd, sizeof(cwd) - 1);
#else
	if (!getcwd(cwd, sizeof(cwd) - 1)) {
		cwd[0] = '\0';
	}
#endif
	netdbgEscape(esc, sizeof(esc), cwd);

	fprintf(s_fp,
			"{\"t\":0,\"tick\":0,\"mode\":\"%s\",\"cat\":\"session\""
			",\"boot\":\"%s\",\"pid\":%d,\"cwd\":\"%s\""
#ifdef VERSION_HASH
			",\"version\":\"" VERSION_BRANCH " " VERSION_HASH "\""
#endif
			"}\n",
			netdbgModeStr(), timestr, (int)NETDBG_GETPID(), esc);
	fflush(s_fp);
	s_bytes_written = (u64)ftell(s_fp);
	s_lines_since_flush = 0;
}

static void netdbgRotateIfNeeded(void)
{
	char newpath[2048];

	if (!s_fp || s_bytes_written < NETDBG_ROTATE_BYTES) {
		return;
	}

	fflush(s_fp);
	fclose(s_fp);
	s_fp = NULL;
	s_part++;
	netdbgBuildPath(newpath, sizeof(newpath), s_part);
	snprintf(s_path, sizeof(s_path), "%s", newpath);
	s_fp = fopen(s_path, "wb");
	if (!s_fp) {
		sysLogPrintf(LOG_WARNING, "NETDBG: rotate open failed: %s", s_path);
		s_enabled = 0;
		return;
	}
	s_bytes_written = 0;
	netdbgWriteHeader();
	sysLogPrintf(LOG_NOTE, "NETDBG: rotated to %s", s_path);
}

void netdbgEnable(void)
{
	char home[1900];

	if (s_enabled && s_fp) {
		return;
	}

	s_force = sysArgCheck("--debug-log") ? 1 : s_force;

	if (!netdbgTryOpenIn("./logs")) {
		sysGetHomePath(home, sizeof(home) - 1);
		strncat(home, "/logs", sizeof(home) - 1);
		if (!netdbgTryOpenIn(home)) {
			sysLogPrintf(LOG_WARNING, "NETDBG: could not open log file");
			return;
		}
	}

	s_enabled = 1;
	netdbgWriteHeader();
	sysLogPrintf(LOG_NOTE, "NETDBG: writing %s", s_path);
}

s32 netdbgIsEnabled(void)
{
	return s_enabled && s_fp != NULL;
}

void netdbgFlush(void)
{
	if (s_fp) {
		fflush(s_fp);
		s_lines_since_flush = 0;
	}
}

void netdbgShutdown(void)
{
	if (!s_fp) {
		s_enabled = 0;
		return;
	}

	fprintf(s_fp,
			"{\"t\":%llu,\"tick\":%u,\"mode\":\"%s\",\"cat\":\"shutdown\"}\n",
			(unsigned long long)(sysGetMicroseconds() / 1000ULL),
			(unsigned)g_NetTick,
			netdbgModeStr());
	fflush(s_fp);
	fclose(s_fp);
	s_fp = NULL;
	s_enabled = 0;
}

void netdbgEvent(const char *cat, const char *fmt, ...)
{
	char extra[1536];
	int n;
	long pos;

	if (!s_enabled || !s_fp) {
		return;
	}

	extra[0] = '\0';
	if (fmt && fmt[0]) {
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(extra, sizeof(extra), fmt, ap);
		va_end(ap);
	}

	if (extra[0]) {
		n = fprintf(s_fp,
				"{\"t\":%llu,\"tick\":%u,\"mode\":\"%s\",\"cat\":\"%s\",%s}\n",
				(unsigned long long)(sysGetMicroseconds() / 1000ULL),
				(unsigned)g_NetTick,
				netdbgModeStr(),
				cat ? cat : "?",
				extra);
	} else {
		n = fprintf(s_fp,
				"{\"t\":%llu,\"tick\":%u,\"mode\":\"%s\",\"cat\":\"%s\"}\n",
				(unsigned long long)(sysGetMicroseconds() / 1000ULL),
				(unsigned)g_NetTick,
				netdbgModeStr(),
				cat ? cat : "?");
	}

	if (n > 0) {
		s_bytes_written += (u64)n;
	}

	s_lines_since_flush++;
	/* warn/crash/draw always flush — draw anomalies are why we're hunting freezes. */
	if (s_lines_since_flush >= NETDBG_FLUSH_EVERY
			|| (cat && (strcmp(cat, "crash") == 0
					|| strcmp(cat, "warn") == 0
					|| strcmp(cat, "error") == 0
					|| strcmp(cat, "draw") == 0))) {
		fflush(s_fp);
		s_lines_since_flush = 0;
	}

	pos = ftell(s_fp);
	if (pos > 0) {
		s_bytes_written = (u64)pos;
	}
	netdbgRotateIfNeeded();
}

void netdbgRx(s32 is_svc, u8 msgid)
{
	const char *name = is_svc ? netdbgSvcName(msgid) : netdbgClcName(msgid);

	netdbgEvent("rx", "\"msgid\":%u,\"name\":\"%s\",\"kind\":\"%s\"",
			(unsigned)msgid, name, is_svc ? "svc" : "clc");
}

void netdbgTx(s32 is_svc, u8 msgid)
{
	const char *name = is_svc ? netdbgSvcName(msgid) : netdbgClcName(msgid);

	netdbgEvent("tx", "\"msgid\":%u,\"name\":\"%s\",\"kind\":\"%s\"",
			(unsigned)msgid, name, is_svc ? "svc" : "clc");
}
