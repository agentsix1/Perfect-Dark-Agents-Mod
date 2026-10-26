#ifndef _IN_NETDBG_H
#define _IN_NETDBG_H

#include <PR/ultratypes.h>

struct prop;

#ifdef __cplusplus
extern "C" {
#endif

/** Open per-boot NDJSON under ./logs/ (or home). Idempotent. */
void netdbgEnable(void);

/** Flush + footer + close. Safe if never enabled. */
void netdbgShutdown(void);

s32 netdbgIsEnabled(void);

/** fflush; also used from crash handler before dialog. */
void netdbgFlush(void);

/**
 * Append one NDJSON object.
 * cat = short category (rx/tx/client/crash/...).
 * fmt/args = extra JSON fields AFTER the common header, e.g.
 *   "\"msgid\":34,\"name\":\"SVC_PROP_SPAWN\",\"syncid\":12"
 * Pass NULL fmt for header-only.
 */
void netdbgEvent(const char *cat, const char *fmt, ...);

/** Log a received net message (before dispatch). is_svc: 1=SVC_* from server, 0=CLC_* from client. */
void netdbgRx(s32 is_svc, u8 msgid);

/** Log a transmitted net message (first byte of payload). */
void netdbgTx(s32 is_svc, u8 msgid);

/**
 * Scan the current onscreen prop list for draw anomalies (null model/matrices,
 * NaN transforms, DELETING ghosts, bad rooms, etc.). Throttled; flushes on hit.
 * Call after propsSort when netdbg is enabled. Implemented in game/prop.c.
 */
void netdbgScanOnscreenDraw(void);

/** One-shot log when objRender is about to draw a suspicious prop. */
void netdbgDrawPropAnomaly(struct prop *prop, const char *reason);

const char *netdbgSvcName(u8 msgid);
const char *netdbgClcName(u8 msgid);

#ifdef __cplusplus
}
#endif

#endif
