#include "libretro.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <retro_endianness.h>
#include <streams/file_stream.h>
#include <libretro.h>

#include "platform.h"

#include "libmad/libmad.h"

#include "font8x16.h"

#define GP_RGB24(r, g, b) (((((r >> 3)) & 0x1f) << 11) | ((((g >> 3)) & 0x1f) << 6) | ((((b >> 3)) & 0x1f) << 1))

#define FPS 50

static void *mp3Mad     = NULL;

static char *mp3        = NULL;
static u32 mp3Length    = 0;
static u32 mp3Position  = 0;

static s16 *soundBuffer = NULL;
static u16 soundEnd     = 0;

static u8 kpause        = 0;

u16 pixels[320 * 240 * 2];
u16 pixels2[320 * 240 * 2];

static void fallback_log(enum retro_log_level level, const char *fmt, ...);

retro_log_printf_t log_cb = fallback_log;
retro_video_refresh_t video_cb;

static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;

retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

int width, height;

#pragma pack(1)

typedef struct
{
   u8 identifier[3];
   u16 version;
   u8 flags;
   u8 lengthSyncSafe[4];
} id3Header;

#pragma pack()

//  KfDirect555 = xRRRRRGG.GGGBBBBB
//  Red         = xRRRRRxx.xxxxxxxx
//  Green       = xxxxxxGG.GGGxxxxx
//  Blue        = xxxxxxxx.xxxBBBBB

// Blend image with background, based on opacity
// Code optimization from http://www.gamedev.net/reference/articles/article817.asp
// result = destPixel + ((srcPixel - destPixel) * ALPHA) / 256
static u16 AlphaBlend(u16 pixel, u16 backpixel, u16 opacity)
{
   u32 dwAlphaRBtemp = (backpixel & 0xf81f);
   u32 dwAlphaGtemp  = (backpixel & 0x07e0);
   u32 dw6bitOpacity = (opacity >> 2);

   return (
         ((dwAlphaRBtemp + ((((pixel & 0xf81f) - dwAlphaRBtemp) * dw6bitOpacity) >> 6)) & 0xf81f) |
         ((dwAlphaGtemp + ((((pixel & 0x07e0) - dwAlphaGtemp) * dw6bitOpacity) >> 6)) & 0x07e0)
         );
}

static bool retro_load_game_internal(const char *mp3_filename, const char *cdg_filename)
{
   FILE *fic = NULL;
   CDGLoad(cdg_filename);
   fic = fopen(mp3_filename, "rb");
   if (!fic)
      return false;
   fseek(fic, 0, SEEK_END);
   mp3Length = ftell(fic);
   fseek(fic, 0, SEEK_SET);
   if (!(mp3 = (char *)malloc(mp3Length)))
      return false;
   fread(mp3, 1, mp3Length, fic);
   fclose(fic);

   mp3Position = 0;

   if (mp3Length > 10) /* Skip ID3 Tag */
   {
      id3Header header;
      memcpy(&header, mp3, 10);

      if ((header.identifier[0] == 0x49) && (header.identifier[1] == 0x44) && (header.identifier[2] == 0x33))
      {

         mp3Position = (header.lengthSyncSafe[0] & 0x7f);
         mp3Position = (mp3Position << 7) | (header.lengthSyncSafe[1] & 0x7f);
         mp3Position = (mp3Position << 7) | (header.lengthSyncSafe[2] & 0x7f);
         mp3Position = (mp3Position << 7) | (header.lengthSyncSafe[3] & 0x7f);

         log_cb(RETRO_LOG_INFO, "id3 length: %d\n", mp3Position);

         mp3Position = mp3Position + 10;
      }
   }

   mp3Mad   = mad_init();
   soundEnd = 0;

   return true;
}

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}

static void update_variables(void) { }

void retro_init(void)
{
   update_variables();

   soundBuffer = (s16 *)malloc(32768);

   width       = 320;
   height      = 240;
}

void retro_deinit(void)
{
   if (soundBuffer)
      free(soundBuffer);
   soundBuffer = NULL;
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
    (void)port;
    (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "pocketcdg";
   info->need_fullpath    = false;
   info->valid_extensions = "cdg";
#ifdef GIT_VERSION
   info->library_version  = "git" GIT_VERSION;
#else
   info->library_version  = "svn";
#endif
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    info->timing.fps            = FPS;
    info->timing.sample_rate    = 44100.0;

    info->geometry.base_width   = 320;
    info->geometry.base_height  = 240;

    info->geometry.max_width    = 320;
    info->geometry.max_height   = 240;
    info->geometry.aspect_ratio = 1;
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_vfs_interface_info vfs_iface_info;
   struct retro_log_callback logging;
   static const struct retro_controller_description port[] = {
      {"RetroPad",      RETRO_DEVICE_JOYPAD     },
      {"RetroKeyboard", RETRO_DEVICE_KEYBOARD   },
   };

   static const struct retro_controller_info ports[] = {
      {port, 2},
      {port, 2},
      {NULL, 0},
   };

   environ_cb = cb;

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;

   cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void *)ports);

   vfs_iface_info.required_interface_version = 1;
   vfs_iface_info.iface                      = NULL;
   if (cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
	   filestream_vfs_init(&vfs_iface_info);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
    audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
    input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
    input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
    video_cb = cb;
}

void retro_reset(void)
{
}

struct KeyMap
{
   unsigned port;
   unsigned index;

   int scanCode;
} keymap[] = {
    {0, RETRO_DEVICE_ID_JOYPAD_A,      0     },
    {0, RETRO_DEVICE_ID_JOYPAD_B,      0     },
    {0, RETRO_DEVICE_ID_JOYPAD_UP,     0     },
    {0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  0     },
    {0, RETRO_DEVICE_ID_JOYPAD_LEFT,   0     },
    {0, RETRO_DEVICE_ID_JOYPAD_DOWN,   0     },
    {0, RETRO_DEVICE_ID_JOYPAD_X,      0     },
    {0, RETRO_DEVICE_ID_JOYPAD_Y,      0     },                                                                                                                                                        // 7
    {0, RETRO_DEVICE_ID_JOYPAD_L,      0     },
    {0, RETRO_DEVICE_ID_JOYPAD_R,      0     },
    {0, RETRO_DEVICE_ID_JOYPAD_SELECT, 0     },
    {0, RETRO_DEVICE_ID_JOYPAD_START,  0     },                                                                                                                                                        // 11

    {1, RETRO_DEVICE_ID_JOYPAD_A,      0     },
    {1, RETRO_DEVICE_ID_JOYPAD_B,      0     },
    {1, RETRO_DEVICE_ID_JOYPAD_UP,     0     },
    {1, RETRO_DEVICE_ID_JOYPAD_RIGHT,  0     },
    {1, RETRO_DEVICE_ID_JOYPAD_LEFT,   0     },
    {1, RETRO_DEVICE_ID_JOYPAD_DOWN,   0     },
    {1, RETRO_DEVICE_ID_JOYPAD_X,      0     },
    {1, RETRO_DEVICE_ID_JOYPAD_Y,      0     },
    {1, RETRO_DEVICE_ID_JOYPAD_L,      0     },
    {1, RETRO_DEVICE_ID_JOYPAD_R,      0     },
    {1, RETRO_DEVICE_ID_JOYPAD_SELECT, 0     },
    {1, RETRO_DEVICE_ID_JOYPAD_START,  0     }
};

#define NEEDFRAME (44100 / 50) // 32768 max
#define NEEDBYTE  (NEEDFRAME * 4) // 32768 max

void retro_run(void)
{
   int i;
   static int framecount      = 0;
   static char keyPressed[24] = {0};
   static bool updated        = false;

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE,
            &updated) && updated)
      update_variables();

   input_poll_cb();

   for (i = 0; i < 24; i++)
   {
      if (input_state_cb(keymap[i].port, RETRO_DEVICE_JOYPAD, 0,
               keymap[i].index))
      {
         if (keyPressed[i] == 0)
         {
            keyPressed[i] = 1;

            if (keymap[i].index == RETRO_DEVICE_ID_JOYPAD_R)
               environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);

            if (keymap[i].index == RETRO_DEVICE_ID_JOYPAD_SELECT)
               kpause = !kpause;

         }
      }
      else
      {
         if (keyPressed[i] == 1)
            keyPressed[i] = 0;
      }

   }

   if (!kpause)
   {
      getFrame(pixels, framecount * (1000 / FPS), FPS);
      framecount++;
   }

   if (framecount < 150)
   {
      int i, j;
      char str[512];
      u16 col;
#ifdef GIT_VERSION
      char *version = "git" GIT_VERSION;
#else
      char *version = "svn";
#endif

      memcpy(pixels2, pixels, width * height * 2);

      col = GP_RGB24(100, 100, 100);

      sprintf(str, "Pocket CDG by Kyuran (%s)", version);

      for (i = 0; i < strlen(str); i++)
      {

         for (j = 0; j < 16; j++)
         {
            int n;
            u8 car = (u8)font8x16[str[i] * 16 + j];

            for (n = 0; n < 8; n++)
            {
               if ((car & 128) == 128)
               {
                  if (framecount > 100)
                  {
                     float ratio = ((float)(framecount-100)/50) * 255;
                     u16 origcol = pixels2[n + i * 8 + (j + height - 16) 
                        * 320];
                     u16 destcol = AlphaBlend(origcol, col, (u8)ratio);

                     pixels2[n + i * 8 + (j + height - 16) * 320] = destcol;

                  }
                  else
                     pixels2[n + i * 8 + (j + height - 16) * 320] = col;
               }
               car = car << 1;
            }
         }
      }
      video_cb(pixels2, width, height, width * 2);

   }
   else
      video_cb(pixels, width, height, width * 2);

   // Play mp3 (to clean)
   if (!kpause)
   {

      int error = 0;

      while (soundEnd <= NEEDBYTE)
      {
         int length = 2048;
         int read;
         int retour;
         int done; // en bytes
         int resolution = 16;
         int halfsamplerate = 0;

         if (mp3Position + length > mp3Length)
         {
            length = mp3Length - mp3Position;
            if (length <= 128) // 2048 / 16
            {
               log_cb(RETRO_LOG_INFO,"Song ended, exiting libretro\n");
               environ_cb(RETRO_ENVIRONMENT_SHUTDOWN, NULL);
               break;
            }
         }

         retour = mad_decode(mp3Mad, mp3 + mp3Position, length,
               (char *)soundBuffer + soundEnd, 10000, &read,
               &done, resolution, halfsamplerate);

         soundEnd += done;

         if (done == 0)
         {
            log_cb(RETRO_LOG_ERROR,
                  "mad decode (Err:%d) %d (%d, %d) %d\n",
                  retour, mp3Position, read, done, soundEnd);
            read++; /* Skip in case of error. */
            error++;
            if (error > 65536)
               break;
         }

         mp3Position += read;
      }

      if (RETRO_IS_BIG_ENDIAN)
      {
         int i;
         for (i = 0; i < NEEDFRAME * 2; i++)
            soundBuffer[i] = SWAP16(soundBuffer[i]);
      }

      audio_batch_cb(soundBuffer, NEEDFRAME);

      soundEnd -= NEEDBYTE;

      memcpy( (char *)soundBuffer,
              (char *)soundBuffer + NEEDBYTE, soundEnd);
   }
}

bool retro_load_game(const struct retro_game_info *info)
{
   char openCDGFilename[1024];
   char openMP3Filename[1024];
   struct retro_input_descriptor desc[] = {
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left"  },
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up"    },
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down"  },
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Pause" },
      {0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Shutdown" },
      {0},
   };
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;

   log_cb(RETRO_LOG_INFO, "begin of load games\n");

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   // Init pixel format
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      log_cb(RETRO_LOG_INFO, "XRGG565 is not supported.\n");
      return 0;
   }

   // Load .cdg
   strcpy(openCDGFilename, info->path);
   strcpy(openMP3Filename, openCDGFilename);

   if (strlen(openMP3Filename) > 4)
   {
      openMP3Filename[strlen(openMP3Filename) - 3] = 0;
      strcat(openMP3Filename, "mp3");
   }

   return retro_load_game_internal(openMP3Filename, openCDGFilename);
}

void retro_unload_game(void)
{
    CDGUnload();
    if (mp3)
       free(mp3);
    if (mp3Mad)
       mad_uninit(mp3Mad);
    mp3 = NULL;
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_PAL;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
    (void)type;
    (void)info;
    (void)num;
    return false;
}

size_t retro_serialize_size(void)
{
    return 0;
}

bool retro_serialize(void *data_, size_t size)
{
    return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
    return false;
}

void * retro_get_memory_data(unsigned id)
{
    return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
    return 0;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
    (void)index;
    (void)enabled;
    (void)code;
}

