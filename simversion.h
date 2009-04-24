#ifndef simversion_h
#define simversion_h

#define MAKEOBJ_VERSION "50"

#define VERSION_NUMBER "102.1 Experimental "
#define WIDE_VERSION_NUMBER L"102.1 Experimental "

#define VERSION_DATE __DATE__

#define SAVEGAME_PREFIX  "Simutrans "
#define XML_SAVEGAME_PREFIX  "<?xml version=\"1.0\"?>"

#define SAVEGAME_VER_NR  "0.102.1"
#define SAVEGAME_VERSION (SAVEGAME_PREFIX SAVEGAME_VER_NR)

#define EXPERIMENTAL_VERSION L"2.7"
#define EXPERIMENTAL_VER_NR ".2"
#define EXPERIMENTAL_SAVEGAME_VERSION (SAVEGAME_PREFIX SAVEGAME_VER_NR EXPERIMENTAL_VER_NR)
#define COMBINED_VER_NR (SAVEGAME_VER_NR EXPERIMENTAL_VER_NR)

#define RES_VERSION_NUMBER  0, 102, 0, 0

#endif
