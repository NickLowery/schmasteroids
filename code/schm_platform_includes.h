#ifndef SCHM_PLATFORM_INCLUDES_H

// Platform debug and intrinsic stuff
#include <immintrin.h>

#if SCHM_SDL

#include <emmintrin.h>
#include <string.h>
// TODO: Do this in a nicer way and only define for DEBUG_BUILD
// #define snprintf

#else

#include <windows.h>
#include <intrin.h>

#endif

#define SCHM_PLATFORM_INCLUDES_H 1
#endif
