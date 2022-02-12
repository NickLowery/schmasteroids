#ifndef SCHM_PLATFORM_INCLUDES_H

// Platform debug and intrinsic stuff
#include <immintrin.h>

#if SCHM_SDL

#include <emmintrin.h>
#include <string.h>
#if DEBUG_BUILD
// TODO: Deal with sprintf in a better way (that actually works on linux) 
// for debug build only
#define sprintf_s(...)
#else
#define sprintf_s(...)
#endif

#else //Windows

#include <windows.h>
#include <intrin.h>

#endif

#define SCHM_PLATFORM_INCLUDES_H 1
#endif
