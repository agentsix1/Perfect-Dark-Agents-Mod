/**
 * Slim texture-pack loader (PD Plus / Dab-compatible numbered packs).
 *
 * Phase 1: N64 hex-numbered PNG/JPEG under texture-packs/<name>/ (and pd-n64/).
 * Phase 2 stubs: pd-xbla/ and xbla/ are indexed but not served yet.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <PR/ultratypes.h>

#include "platform.h"
#include "system.h"
#include "fs.h"
#include "config.h"
#include "video.h"
#include "pngread.h"
#include "jpegread.h"
#include "archive.h"
#include "loadsplash.h"
#include "texpack.h"

#include <sys/stat.h>

#ifdef PLATFORM_WIN32
#include <windows.h>
#define strcasecmp _stricmp
#define strdup _strdup
#else
#include <dirent.h>
#include <strings.h>
#endif

#define TEXPACK_MAX_TEXNUM   4096
#define TEXPACK_MAX_PACKS    64
#define TEXPACK_NAME_LEN     128
#define TEXPACK_REG_SIZE     8192
#define TEXPACK_KEEP_SIZE    512
#define TEXPACK_XBLA_SLOTS   8192

struct texpackreg {
	const void *data;
	s32 texturenum;
};

struct texpackkeep {
	s32 texturenum;
	u8 *rgba;
	s32 width;
	s32 height;
	u8 inuse;
};

static struct texpackreg s_reg[TEXPACK_REG_SIZE];
static s32 s_regCount;

static char *s_paths[TEXPACK_MAX_TEXNUM];
static u8 s_flip[TEXPACK_MAX_TEXNUM];
static s32 s_numReplacements;

static char *s_xblaPaths[TEXPACK_XBLA_SLOTS];
static u8 s_xblaFlip[TEXPACK_XBLA_SLOTS];
static s32 s_numXbla;

struct texpacklisted {
	char name[TEXPACK_NAME_LEN];  /* menu / pd.ini key (no .zip) */
	char entry[TEXPACK_NAME_LEN]; /* filename under texture-packs/ */
	u8 isZip;
};

static struct texpacklisted s_packs[TEXPACK_MAX_PACKS];
static s32 s_numPacks;
static s32 s_selectedPack = -1; /* -1 = Default (ROM) */
static char s_selectedPackName[TEXPACK_NAME_LEN];
static s32 s_indexSerial = 0;

static char s_packsDir[FS_MAXPATH];
static char s_cacheRoot[FS_MAXPATH];
static s32 s_haveIndex;
static s32 s_packsListed;

static struct texpackkeep s_keep[TEXPACK_KEEP_SIZE];

static char s_cfgPackName[TEXPACK_NAME_LEN];
static s32 s_configApplied;

static void texpackEnsureConfigApplied(void);

/* -------------------------------------------------------------------------- */

static u32 texpackHashPtr(const void *p)
{
	uintptr_t x = (uintptr_t)p;
	x = (x >> 4) ^ (x >> 9);
	return (u32)(x % TEXPACK_REG_SIZE);
}

void texpackRegisterTexture(const void *data, s32 texturenum)
{
	u32 i;
	u32 start;

	if (!data || texturenum < 0) {
		return;
	}

	start = texpackHashPtr(data);
	for (i = 0; i < TEXPACK_REG_SIZE; i++) {
		u32 idx = (start + i) % TEXPACK_REG_SIZE;
		if (s_reg[idx].data == data || s_reg[idx].data == NULL) {
			if (s_reg[idx].data == NULL) {
				s_regCount++;
			}
			s_reg[idx].data = data;
			s_reg[idx].texturenum = texturenum;
			return;
		}
	}
}

void texpackForgetTexture(const void *data)
{
	u32 i;
	u32 start;

	if (!data) {
		return;
	}

	start = texpackHashPtr(data);
	for (i = 0; i < TEXPACK_REG_SIZE; i++) {
		u32 idx = (start + i) % TEXPACK_REG_SIZE;
		if (s_reg[idx].data == NULL) {
			return;
		}
		if (s_reg[idx].data == data) {
			s_reg[idx].data = NULL;
			s_reg[idx].texturenum = -1;
			s_regCount--;
			return;
		}
	}
}

void texpackForgetAll(void)
{
	memset(s_reg, 0, sizeof(s_reg));
	s_regCount = 0;
}

s32 texpackGetTextureNum(const void *data)
{
	u32 i;
	u32 start;

	if (!data) {
		return -1;
	}

	start = texpackHashPtr(data);
	for (i = 0; i < TEXPACK_REG_SIZE; i++) {
		u32 idx = (start + i) % TEXPACK_REG_SIZE;
		if (s_reg[idx].data == NULL) {
			return -1;
		}
		if (s_reg[idx].data == data) {
			return s_reg[idx].texturenum;
		}
	}
	return -1;
}

/* -------------------------------------------------------------------------- */

static void texpackClearKeep(void)
{
	s32 i;

	for (i = 0; i < TEXPACK_KEEP_SIZE; i++) {
		if (s_keep[i].rgba) {
			free(s_keep[i].rgba);
			s_keep[i].rgba = NULL;
		}
		s_keep[i].inuse = 0;
		s_keep[i].texturenum = -1;
	}
}

static void texpackClearIndex(void)
{
	s32 i;

	for (i = 0; i < TEXPACK_MAX_TEXNUM; i++) {
		if (s_paths[i]) {
			free(s_paths[i]);
			s_paths[i] = NULL;
		}
		s_flip[i] = 0;
	}
	s_numReplacements = 0;

	for (i = 0; i < TEXPACK_XBLA_SLOTS; i++) {
		if (s_xblaPaths[i]) {
			free(s_xblaPaths[i]);
			s_xblaPaths[i] = NULL;
		}
		s_xblaFlip[i] = 0;
	}
	s_numXbla = 0;
	s_haveIndex = 0;
	texpackClearKeep();
}

static s32 texpackParseHexId(const char *name, s32 *outId, const char **outRest)
{
	s32 i;
	s32 id = 0;

	for (i = 0; i < 4; i++) {
		char c = name[i];
		s32 v;

		if (c >= '0' && c <= '9') {
			v = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			v = c - 'a' + 10;
		} else if (c >= 'A' && c <= 'F') {
			v = c - 'A' + 10;
		} else {
			return 0;
		}
		id = (id << 4) | v;
	}

	if (name[4] != '.' && name[4] != '_' && name[4] != '\0') {
		return 0;
	}

	*outId = id;
	*outRest = name + 4;
	return 1;
}

static s32 texpackHasImageExt(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot) {
		return 0;
	}
	dot++;
	return !strcasecmp(dot, "png") || !strcasecmp(dot, "jpg") || !strcasecmp(dot, "jpeg");
}

static void texpackIndexFile(const char *fullpath, s32 id, s32 flip, s32 xbla)
{
	char *copy;

	if (xbla) {
		if (id < 0 || id >= TEXPACK_XBLA_SLOTS) {
			return;
		}
		if (s_xblaPaths[id]) {
			return; /* first wins */
		}
		copy = strdup(fullpath);
		if (!copy) {
			return;
		}
		s_xblaPaths[id] = copy;
		s_xblaFlip[id] = flip ? 1 : 0;
		s_numXbla++;
		return;
	}

	if (id < 0 || id >= TEXPACK_MAX_TEXNUM) {
		return;
	}
	if (s_paths[id]) {
		return;
	}
	copy = strdup(fullpath);
	if (!copy) {
		return;
	}
	s_paths[id] = copy;
	s_flip[id] = flip ? 1 : 0;
	s_numReplacements++;
}

static s32 texpackPathHasBottomUpMarker(const char *dir)
{
	char marker[FS_MAXPATH];
	snprintf(marker, sizeof(marker), "%s/bottomup.txt", dir);
	if (fsFileSize(marker) >= 0) {
		return 1;
	}
	/* path segment "ext_tex" means emulator bottom-up order */
	if (strstr(dir, "ext_tex") || strstr(dir, "EXT_TEX")) {
		return 1;
	}
	return 0;
}

static void texpackScanDir(const char *dir, s32 flip, s32 depth, s32 xblaHint);

#ifdef PLATFORM_WIN32
static void texpackScanDir(const char *dir, s32 flip, s32 depth, s32 xblaHint)
{
	char pattern[FS_MAXPATH];
	WIN32_FIND_DATAA fd;
	HANDLE h;
	s32 localFlip = flip;

	if (depth > 8) {
		return;
	}

	if (texpackPathHasBottomUpMarker(dir)) {
		localFlip = 0;
	}

	snprintf(pattern, sizeof(pattern), "%s/*", dir);
	h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE) {
		return;
	}

	do {
		char child[FS_MAXPATH];
		const char *name = fd.cFileName;
		s32 id;
		const char *rest;

		if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
			continue;
		}

		snprintf(child, sizeof(child), "%s/%s", dir, name);

		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			s32 subXbla = xblaHint;
			if (!strcasecmp(name, "pd-xbla") || !strcasecmp(name, "xbla")) {
				subXbla = 1;
			} else if (!strcasecmp(name, "pd-n64") || !strcasecmp(name, "textures")) {
				subXbla = 0;
			}
			texpackScanDir(child, localFlip, depth + 1, subXbla);
			continue;
		}

		if (!texpackHasImageExt(name)) {
			continue;
		}
		if (!texpackParseHexId(name, &id, &rest)) {
			continue;
		}
		/* optional _suffix before extension is fine */
		texpackIndexFile(child, id, localFlip, xblaHint);
	} while (FindNextFileA(h, &fd));

	FindClose(h);
}
#else
static void texpackScanDir(const char *dir, s32 flip, s32 depth, s32 xblaHint)
{
	DIR *d;
	struct dirent *ent;
	s32 localFlip = flip;

	if (depth > 8) {
		return;
	}

	if (texpackPathHasBottomUpMarker(dir)) {
		localFlip = 0;
	}

	d = opendir(dir);
	if (!d) {
		return;
	}

	while ((ent = readdir(d)) != NULL) {
		char child[FS_MAXPATH];
		struct stat st;
		const char *name = ent->d_name;
		s32 id;
		const char *rest;

		if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
			continue;
		}

		snprintf(child, sizeof(child), "%s/%s", dir, name);
		if (stat(child, &st) != 0) {
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			s32 subXbla = xblaHint;
			if (!strcasecmp(name, "pd-xbla") || !strcasecmp(name, "xbla")) {
				subXbla = 1;
			} else if (!strcasecmp(name, "pd-n64") || !strcasecmp(name, "textures")) {
				subXbla = 0;
			}
			texpackScanDir(child, localFlip, depth + 1, subXbla);
			continue;
		}

		if (!texpackHasImageExt(name)) {
			continue;
		}
		if (!texpackParseHexId(name, &id, &rest)) {
			continue;
		}
		texpackIndexFile(child, id, localFlip, xblaHint);
	}

	closedir(d);
}
#endif

static void texpackStripZipExt(char *name)
{
	size_t n = strlen(name);
	if (n > 4 && !strcasecmp(name + n - 4, ".zip")) {
		name[n - 4] = '\0';
	} else if (n > 4 && !strcasecmp(name + n - 4, ".pk3")) {
		name[n - 4] = '\0';
	}
}

static s32 texpackAddListed(const char *entry, s32 isZip)
{
	char name[TEXPACK_NAME_LEN];
	s32 i;

	if (s_numPacks >= TEXPACK_MAX_PACKS || !entry || !entry[0] || entry[0] == '.') {
		return 0;
	}

	strncpy(name, entry, TEXPACK_NAME_LEN - 1);
	name[TEXPACK_NAME_LEN - 1] = '\0';
	if (isZip) {
		texpackStripZipExt(name);
	}
	if (!name[0]) {
		return 0;
	}

	/* Prefer a folder over a zip of the same display name */
	for (i = 0; i < s_numPacks; i++) {
		if (!strcmp(s_packs[i].name, name)) {
			if (s_packs[i].isZip && !isZip) {
				strncpy(s_packs[i].entry, entry, TEXPACK_NAME_LEN - 1);
				s_packs[i].entry[TEXPACK_NAME_LEN - 1] = '\0';
				s_packs[i].isZip = 0;
			}
			return 0;
		}
	}

	strncpy(s_packs[s_numPacks].name, name, TEXPACK_NAME_LEN - 1);
	s_packs[s_numPacks].name[TEXPACK_NAME_LEN - 1] = '\0';
	strncpy(s_packs[s_numPacks].entry, entry, TEXPACK_NAME_LEN - 1);
	s_packs[s_numPacks].entry[TEXPACK_NAME_LEN - 1] = '\0';
	s_packs[s_numPacks].isZip = isZip ? 1 : 0;
	s_numPacks++;
	return 1;
}

static void texpackEnsurePacksDir(void)
{
	const char *candidates[] = { "$E/" TEXPACK_PACKS_DIR, "$S/" TEXPACK_PACKS_DIR };
	s32 i;

	if (s_packsDir[0]) {
		return;
	}

	for (i = 0; i < 2; i++) {
		const char *full = fsFullPath(candidates[i]);
		if (fsFileSize(full) >= 0) {
			strncpy(s_packsDir, full, FS_MAXPATH - 1);
			s_packsDir[FS_MAXPATH - 1] = '\0';
			break;
		}
	}

	if (!s_packsDir[0]) {
		strncpy(s_packsDir, fsFullPath("$E/" TEXPACK_PACKS_DIR), FS_MAXPATH - 1);
		s_packsDir[FS_MAXPATH - 1] = '\0';
	}

	fsCreateDir(s_packsDir);

	strncpy(s_cacheRoot, fsFullPath("$S/cache/" TEXPACK_PACKS_DIR), FS_MAXPATH - 1);
	s_cacheRoot[FS_MAXPATH - 1] = '\0';
	fsCreateDir(fsFullPath("$S/cache"));
	fsCreateDir(s_cacheRoot);
}

void texpackInit(void)
{
	texpackEnsurePacksDir();
	sysLogPrintf(LOG_NOTE, "texpack dir: %s", s_packsDir);
}

void texpackRefreshPacks(void)
{
	s32 i;

	/* Do not call EnsureConfigApplied here — it calls RefreshPacks. */
	texpackEnsurePacksDir();
	s_numPacks = 0;
	s_packsListed = 1;

	if (fsFileSize(s_packsDir) < 0) {
		return;
	}

#ifdef PLATFORM_WIN32
	{
		char pattern[FS_MAXPATH];
		WIN32_FIND_DATAA fd;
		HANDLE h;

		snprintf(pattern, sizeof(pattern), "%s/*", s_packsDir);
		h = FindFirstFileA(pattern, &fd);
		if (h == INVALID_HANDLE_VALUE) {
			return;
		}
		do {
			if (fd.cFileName[0] == '.') {
				continue;
			}
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				texpackAddListed(fd.cFileName, 0);
			} else if (archiveIsZip(fd.cFileName)) {
				texpackAddListed(fd.cFileName, 1);
			}
		} while (FindNextFileA(h, &fd) && s_numPacks < TEXPACK_MAX_PACKS);
		FindClose(h);
	}
#else
	{
		DIR *d = opendir(s_packsDir);
		struct dirent *ent;
		if (!d) {
			return;
		}
		while ((ent = readdir(d)) != NULL && s_numPacks < TEXPACK_MAX_PACKS) {
			char child[FS_MAXPATH];
			struct stat st;
			if (ent->d_name[0] == '.') {
				continue;
			}
			snprintf(child, sizeof(child), "%s/%s", s_packsDir, ent->d_name);
			if (stat(child, &st) != 0) {
				continue;
			}
			if (S_ISDIR(st.st_mode)) {
				texpackAddListed(ent->d_name, 0);
			} else if (S_ISREG(st.st_mode) && archiveIsZip(ent->d_name)) {
				texpackAddListed(ent->d_name, 1);
			}
		}
		closedir(d);
	}
#endif

	/* Re-resolve selection after refresh */
	if (s_selectedPackName[0]) {
		s_selectedPack = -1;
		for (i = 0; i < s_numPacks; i++) {
			if (!strcmp(s_packs[i].name, s_selectedPackName)) {
				s_selectedPack = i;
				break;
			}
		}
		if (s_selectedPack < 0) {
			sysLogPrintf(LOG_WARNING, "texpack: pack '%s' missing; using Default", s_selectedPackName);
			s_selectedPackName[0] = '\0';
			s_cfgPackName[0] = '\0';
		}
	} else {
		s_selectedPack = -1;
	}
}

s32 texpackGetNumPacks(void)
{
	texpackEnsureConfigApplied();
	if (!s_packsListed) {
		texpackRefreshPacks();
	}
	return s_numPacks;
}

const char *texpackGetPackName(s32 index)
{
	if (!s_packsListed) {
		texpackRefreshPacks();
	}
	if (index < 0 || index >= s_numPacks) {
		return "";
	}
	return s_packs[index].name;
}

s32 texpackGetSelectedPack(void)
{
	texpackEnsureConfigApplied();
	return s_selectedPack;
}

const char *texpackGetSelectedPackName(void)
{
	return s_selectedPackName;
}

const char *texpackGetPacksDirPath(void)
{
	texpackEnsurePacksDir();
	return s_packsDir;
}

/**
 * Unpack a zip into cache once (or again if the archive changed).
 * Returns the directory to scan, or NULL on failure.
 */
static const char *texpackPreparePackDir(const struct texpacklisted *pack, char *outPath, u32 outLen)
{
	char zipPath[FS_MAXPATH];
	char stampPath[FS_MAXPATH];
	char stampBuf[128];
	FILE *sf;
	struct stat st;
	s32 needExtract = 1;
	s32 n;

	if (!pack->isZip) {
		snprintf(outPath, outLen, "%s/%s", s_packsDir, pack->entry);
		return outPath;
	}

	snprintf(zipPath, sizeof(zipPath), "%s/%s", s_packsDir, pack->entry);
	if (stat(zipPath, &st) != 0) {
		sysLogPrintf(LOG_WARNING, "texpack: zip missing: %s", zipPath);
		return NULL;
	}

	snprintf(outPath, outLen, "%s/%s", s_cacheRoot, pack->name);
	fsCreateDir(outPath);
	snprintf(stampPath, sizeof(stampPath), "%s/.texpack-src", outPath);

	sf = fopen(stampPath, "rb");
	if (sf) {
		memset(stampBuf, 0, sizeof(stampBuf));
		n = (s32)fread(stampBuf, 1, sizeof(stampBuf) - 1, sf);
		fclose(sf);
		if (n > 0) {
			char expect[128];
			snprintf(expect, sizeof(expect), "%lld %lld",
				(long long)st.st_size, (long long)st.st_mtime);
			if (!strcmp(stampBuf, expect)) {
				needExtract = 0;
			}
		}
	}

	if (needExtract) {
		sysLogPrintf(LOG_NOTE, "texpack: unpacking '%s' (first select or zip changed)...", pack->name);
		loadSplashBegin("LOADING", "TEXTURE PACK");
		loadSplashSetProgress(0, 1);
		if (archiveExtractZip(zipPath, outPath) < 0) {
			loadSplashEnd();
			sysLogPrintf(LOG_ERROR, "texpack: failed to unpack %s", zipPath);
			return NULL;
		}
		loadSplashSetProgress(1, 1);
		loadSplashEnd();

		sf = fopen(stampPath, "wb");
		if (sf) {
			fprintf(sf, "%lld %lld", (long long)st.st_size, (long long)st.st_mtime);
			fclose(sf);
		}
	}

	return outPath;
}

static void texpackBuildIndex(void)
{
	char packPath[FS_MAXPATH];
	const char *scanDir;
	const struct texpacklisted *pack;

	texpackClearIndex();

	if (s_selectedPack < 0 || !s_selectedPackName[0]) {
		s_haveIndex = 1;
		return;
	}

	texpackEnsurePacksDir();
	if (s_selectedPack < 0 || s_selectedPack >= s_numPacks) {
		s_haveIndex = 1;
		return;
	}

	pack = &s_packs[s_selectedPack];
	scanDir = texpackPreparePackDir(pack, packPath, sizeof(packPath));
	if (!scanDir || fsFileSize(scanDir) < 0) {
		sysLogPrintf(LOG_WARNING, "texpack: cannot open pack path for '%s'", s_selectedPackName);
		s_haveIndex = 1;
		return;
	}

	/* Default flip=1: top-down authored dumps need vertical flip for N64 order */
	texpackScanDir(scanDir, 1, 0, 0);
	s_haveIndex = 1;

	sysLogPrintf(LOG_NOTE, "texpack: '%s'%s - %d N64 + %d XBLA (stub) replacements",
		s_selectedPackName, pack->isZip ? " (zip)" : "", s_numReplacements, s_numXbla);
}

static void texpackSetSelectedPackEx(s32 index, s32 clearCache)
{
	if (!s_packsListed) {
		texpackRefreshPacks();
	}

	if (index < 0) {
		s_selectedPack = -1;
		s_selectedPackName[0] = '\0';
		s_cfgPackName[0] = '\0';
	} else if (index < s_numPacks) {
		s_selectedPack = index;
		strncpy(s_selectedPackName, s_packs[index].name, TEXPACK_NAME_LEN - 1);
		s_selectedPackName[TEXPACK_NAME_LEN - 1] = '\0';
		strncpy(s_cfgPackName, s_selectedPackName, TEXPACK_NAME_LEN - 1);
	} else {
		return;
	}

	texpackClearIndex();
	texpackBuildIndex();
	s_indexSerial++;
	if (clearCache) {
		videoResetTextureCache();
	}
}

void texpackSetSelectedPack(s32 index)
{
	texpackSetSelectedPackEx(index, 1);
}

s32 texpackGetIndexSerial(void)
{
	return s_indexSerial;
}


s32 texpackSelectPackByName(const char *name)
{
	s32 i;
	char stripped[TEXPACK_NAME_LEN];

	texpackRefreshPacks();
	if (!name || !name[0] || !strcasecmp(name, "Default") || !strcasecmp(name, "None")) {
		texpackSetSelectedPackEx(-1, 1);
		return 1;
	}

	strncpy(stripped, name, TEXPACK_NAME_LEN - 1);
	stripped[TEXPACK_NAME_LEN - 1] = '\0';
	texpackStripZipExt(stripped);

	for (i = 0; i < s_numPacks; i++) {
		if (!strcmp(s_packs[i].name, name) || !strcmp(s_packs[i].name, stripped)
				|| !strcmp(s_packs[i].entry, name)) {
			texpackSetSelectedPackEx(i, 1);
			return 1;
		}
	}
	return 0;
}

void texpackReload(void)
{
	texpackRefreshPacks();
	texpackClearIndex();
	texpackBuildIndex();
	videoResetTextureCache();
}

s32 texpackGetNumReplacements(void)
{
	if (!s_haveIndex && s_selectedPack >= 0) {
		texpackBuildIndex();
	}
	return s_numReplacements;
}

s32 texpackHaveReplacements(void)
{
	texpackEnsureConfigApplied();
	if (s_selectedPack < 0) {
		return 0;
	}
	if (!s_haveIndex) {
		texpackBuildIndex();
	}
	return s_numReplacements > 0;
}

s32 texpackHaveReplacementFor(s32 texturenum)
{
	if (!texpackHaveReplacements()) {
		return 0;
	}
	if (texturenum < 0 || texturenum >= TEXPACK_MAX_TEXNUM) {
		return 0;
	}
	return s_paths[texturenum] != NULL;
}

static void texpackFlipVertical(u8 *rgba, s32 width, s32 height)
{
	s32 rowBytes = width * 4;
	u8 *tmp;
	s32 y;

	if (height <= 1) {
		return;
	}
	tmp = (u8 *)malloc((size_t)rowBytes);
	if (!tmp) {
		return;
	}
	for (y = 0; y < height / 2; y++) {
		u8 *a = rgba + (size_t)y * rowBytes;
		u8 *b = rgba + (size_t)(height - 1 - y) * rowBytes;
		memcpy(tmp, a, (size_t)rowBytes);
		memcpy(a, b, (size_t)rowBytes);
		memcpy(b, tmp, (size_t)rowBytes);
	}
	free(tmp);
}

static u8 *texpackLoadImagePath(const char *path, s32 flip, s32 *outWidth, s32 *outHeight)
{
	u8 *rgba = NULL;
	const char *dot = strrchr(path, '.');
	s32 w = 0, h = 0;

	if (!dot) {
		return NULL;
	}

	if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) {
		rgba = jpegRead(path, &w, &h);
	} else {
		rgba = pngRead(path, &w, &h);
		if (!rgba) {
			rgba = pngReadFallback(path, &w, &h);
		}
	}

	if (!rgba) {
		return NULL;
	}

	if (flip) {
		texpackFlipVertical(rgba, w, h);
	}

	*outWidth = w;
	*outHeight = h;
	return rgba;
}

static u8 *texpackKeepGet(s32 texturenum, s32 *outWidth, s32 *outHeight)
{
	s32 i;

	for (i = 0; i < TEXPACK_KEEP_SIZE; i++) {
		if (s_keep[i].inuse && s_keep[i].texturenum == texturenum && s_keep[i].rgba) {
			*outWidth = s_keep[i].width;
			*outHeight = s_keep[i].height;
			return s_keep[i].rgba;
		}
	}
	return NULL;
}

static void texpackKeepPut(s32 texturenum, u8 *rgba, s32 width, s32 height)
{
	s32 i;
	s32 slot = -1;

	for (i = 0; i < TEXPACK_KEEP_SIZE; i++) {
		if (s_keep[i].inuse && s_keep[i].texturenum == texturenum) {
			if (s_keep[i].rgba && s_keep[i].rgba != rgba) {
				free(s_keep[i].rgba);
			}
			s_keep[i].rgba = rgba;
			s_keep[i].width = width;
			s_keep[i].height = height;
			return;
		}
		if (!s_keep[i].inuse && slot < 0) {
			slot = i;
		}
	}

	if (slot < 0) {
		/* Evict slot 0 */
		slot = 0;
		if (s_keep[0].rgba && s_keep[0].rgba != rgba) {
			free(s_keep[0].rgba);
		}
	}

	s_keep[slot].inuse = 1;
	s_keep[slot].texturenum = texturenum;
	s_keep[slot].rgba = rgba;
	s_keep[slot].width = width;
	s_keep[slot].height = height;
}

u8 *texpackDecodeReplacementNow(s32 texturenum, s32 *outWidth, s32 *outHeight)
{
	u8 *kept;
	u8 *rgba;
	s32 w, h;

	if (!texpackHaveReplacementFor(texturenum)) {
		return NULL;
	}

	kept = texpackKeepGet(texturenum, outWidth, outHeight);
	if (kept) {
		return kept;
	}

	rgba = texpackLoadImagePath(s_paths[texturenum], s_flip[texturenum], &w, &h);
	if (!rgba) {
		return NULL;
	}

	texpackKeepPut(texturenum, rgba, w, h);
	*outWidth = w;
	*outHeight = h;
	return rgba;
}

u8 *texpackLoadReplacement(const void *data, s32 *outWidth, s32 *outHeight)
{
	s32 texturenum;

	if (!texpackHaveReplacements()) {
		return NULL;
	}

	texturenum = texpackGetTextureNum(data);
	if (texturenum < 0) {
		return NULL;
	}

	return texpackDecodeReplacementNow(texturenum, outWidth, outHeight);
}

void texpackFreeReplacement(u8 *rgba)
{
	s32 i;

	if (!rgba) {
		return;
	}
	/* Owned by keep-store — do not free */
	for (i = 0; i < TEXPACK_KEEP_SIZE; i++) {
		if (s_keep[i].rgba == rgba) {
			return;
		}
	}
	free(rgba);
}

/* -------------------------------------------------------------------------- */
/* XBLA record replacements (pd-xbla / xbla folders in the selected pack)     */

s32 texpackHaveXblaReplacement(s32 record)
{
	if (!texpackHaveReplacements()) {
		return 0;
	}
	if (record < 0 || record >= TEXPACK_XBLA_SLOTS) {
		return 0;
	}
	return s_xblaPaths[record] != NULL;
}

s32 texpackXblaReplacementIsSelectedPack(s32 record)
{
	return texpackHaveXblaReplacement(record);
}

u8 *texpackDecodeXblaReplacementNow(s32 record, s32 *outWidth, s32 *outHeight)
{
	u8 *rgba;
	s32 w = 0, h = 0;

	if (!texpackHaveXblaReplacement(record)) {
		return NULL;
	}
	rgba = texpackLoadImagePath(s_xblaPaths[record], s_xblaFlip[record], &w, &h);
	if (!rgba) {
		return NULL;
	}
	*outWidth = w;
	*outHeight = h;
	return rgba;
}

u8 *texpackLoadXblaReplacement(s32 record, s32 *outWidth, s32 *outHeight)
{
	/* Sync decode for now (same as numbered packs). */
	return texpackDecodeXblaReplacementNow(record, outWidth, outHeight);
}

s32 texpackGetNumXblaReplacements(void)
{
	if (!s_haveIndex && s_selectedPack >= 0) {
		texpackBuildIndex();
	}
	return s_numXbla;
}

s32 texpackXblaRecordFromId(s32 id)
{
	/* Job ids not used in sync path */
	(void)id;
	return -1;
}

s32 texpackLoadEnabled(void)
{
	return s_selectedPack >= 0;
}

void texpackSetLoadEnabled(s32 enabled)
{
	if (!enabled) {
		texpackSetSelectedPack(-1);
	}
}

/* -------------------------------------------------------------------------- */

static void texpackApplyConfig(void)
{
	s32 i;

	texpackRefreshPacks();
	if (!s_cfgPackName[0]) {
		texpackSetSelectedPackEx(-1, 0);
		return;
	}

	for (i = 0; i < s_numPacks; i++) {
		if (!strcmp(s_packs[i].name, s_cfgPackName)
				|| !strcmp(s_packs[i].entry, s_cfgPackName)) {
			texpackSetSelectedPackEx(i, 0);
			return;
		}
	}

	sysLogPrintf(LOG_WARNING, "texpack: saved pack '%s' missing; using Default", s_cfgPackName);
	texpackSetSelectedPackEx(-1, 0);
}

PD_CONSTRUCTOR static void texpackConfigInit(void)
{
	s_cfgPackName[0] = '\0';
	configRegisterString("Mod.TexturePack", s_cfgPackName, TEXPACK_NAME_LEN - 1);
}

static void texpackEnsureConfigApplied(void)
{
	if (s_configApplied) {
		return;
	}
	s_configApplied = 1;
	texpackApplyConfig();
}
