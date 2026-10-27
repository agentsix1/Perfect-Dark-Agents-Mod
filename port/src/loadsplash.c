/**
 * Minimal PD-themed loading overlay during archive unpack (texture packs / XBLA).
 */
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <PR/ultratypes.h>
#include "loadsplash.h"
#include "system.h"
#include "video.h"

#ifndef PLATFORM_N64

static s32 s_active;
static char s_title[64];
static char s_detail[128];
static s32 s_done;
static s32 s_total;

static void loadSplashPaint(void)
{
	SDL_Window *win = (SDL_Window *)videoGetWindowHandle();
	SDL_Surface *surf;
	SDL_Rect barBg;
	SDL_Rect barFg;
	f32 frac;
	u32 bg;
	u32 dim;
	u32 bright;

	if (!win) {
		return;
	}

	surf = SDL_GetWindowSurface(win);
	if (!surf) {
		/* OpenGL windows often have no surface — still pump events. */
		return;
	}

	bg = SDL_MapRGB(surf->format, 8, 12, 18);
	dim = SDL_MapRGB(surf->format, 40, 48, 58);
	bright = SDL_MapRGB(surf->format, 180, 200, 220);
	SDL_FillRect(surf, NULL, bg);

	barBg.w = surf->w * 2 / 3;
	barBg.h = 10;
	barBg.x = (surf->w - barBg.w) / 2;
	barBg.y = surf->h * 2 / 3;
	SDL_FillRect(surf, &barBg, dim);

	frac = (s_total > 0) ? ((f32)s_done / (f32)s_total) : 0.15f;
	if (frac < 0.0f) {
		frac = 0.0f;
	}
	if (frac > 1.0f) {
		frac = 1.0f;
	}
	barFg = barBg;
	barFg.w = (s32)(barBg.w * frac);
	SDL_FillRect(surf, &barFg, bright);
	SDL_UpdateWindowSurface(win);

	(void)s_title;
	(void)s_detail;
}

static void loadSplashPump(void)
{
	SDL_Event ev;

	while (SDL_PollEvent(&ev)) {
		if (ev.type == SDL_QUIT) {
			/* Ignore during unpack — main loop will see later. */
		}
	}
	loadSplashPaint();
}

void loadSplashBegin(const char *title, const char *detail)
{
	snprintf(s_title, sizeof(s_title), "%s", title ? title : "LOADING");
	snprintf(s_detail, sizeof(s_detail), "%s", detail ? detail : "");
	s_done = 0;
	s_total = 0;
	s_active = 1;
	sysLogPrintf(LOG_NOTE, "loadsplash: %s — %s", s_title, s_detail);
	loadSplashPump();
}

void loadSplashSetProgress(s32 done, s32 total)
{
	s_done = done;
	s_total = total;
	if (s_active) {
		loadSplashPump();
	}
}

void loadSplashEnd(void)
{
	if (!s_active) {
		return;
	}
	s_active = 0;
	s_done = s_total > 0 ? s_total : 1;
	loadSplashPaint();
}

#else

void loadSplashBegin(const char *title, const char *detail) { (void)title; (void)detail; }
void loadSplashSetProgress(s32 done, s32 total) { (void)done; (void)total; }
void loadSplashEnd(void) {}

#endif
