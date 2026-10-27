/**
 * JPEG decoder (and PNG fallback) via stb_image.
 */

#include <stdlib.h>
#include <PR/ultratypes.h>
#include "system.h"
#include "jpegread.h"

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_WRITE
#define STBI_ASSERT(x) ((void)0)
#define STB_IMAGE_IMPLEMENTATION
#include "external/stb_image.h"

static u8 *stbRead(const char *path, s32 *outWidth, s32 *outHeight, const char *what)
{
	s32 width = 0;
	s32 height = 0;
	s32 channels = 0;
	u8 *rgba;

	rgba = stbi_load(path, &width, &height, &channels, 4);
	if (!rgba) {
		sysLogPrintf(LOG_ERROR, "%s: %s: %s", what, path, stbi_failure_reason());
		return NULL;
	}

	if (width <= 0 || height <= 0) {
		stbi_image_free(rgba);
		return NULL;
	}

	*outWidth = width;
	*outHeight = height;
	return rgba;
}

u8 *jpegRead(const char *path, s32 *outWidth, s32 *outHeight)
{
	return stbRead(path, outWidth, outHeight, "jpegRead");
}

u8 *pngReadFallback(const char *path, s32 *outWidth, s32 *outHeight)
{
	return stbRead(path, outWidth, outHeight, "pngReadFallback");
}
