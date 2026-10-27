#ifndef _IN_PORT_FILES_H
#define _IN_PORT_FILES_H

/*
 * Dab expands the file table past NUM_FILES for mods/aliases.
 * XBLA mesh bookkeeping indexes by file id with this ceiling.
 */
#ifndef PLATFORM_N64
#ifndef NUM_FILE_SLOTS
#define NUM_FILE_SLOTS 4096
#endif
#else
#ifndef NUM_FILE_SLOTS
#define NUM_FILE_SLOTS NUM_FILES
#endif
#endif

#endif
