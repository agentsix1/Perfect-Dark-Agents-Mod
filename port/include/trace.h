#ifndef _IN_TRACE_H
#define _IN_TRACE_H
#include <stdio.h>
#include <PR/ultratypes.h>

static inline void traceNoteEvent(const char *fmt, ...) { (void)fmt; }
static inline void traceNop(void) {}

#endif
