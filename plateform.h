#ifndef PLATEFORM_T
#define PLATEFORM_T

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;


void CDGLoad(char *filename);

void getFrame(u16 *frame, int pos_mp3, int fps);		// pos_mp3 in ms


#endif
