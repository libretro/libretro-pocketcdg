#include "libretro.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/time.h>

#include "pocketcdg-core/plateform.h"

#include "libmad/libmad.h"

#define FPS 50

typedef char BOOL;

char openCDGFilename[1024];
char openMP3Filename[1024];

void *mp3Mad;

char *mp3;
u32 mp3Length;
u32 mp3Position;

void updateFromEnvironnement();


s16 *soundBuffer;
u16 soundEnd;

u16 pixels[320 * 240 * 2];   // Verifier taille

char Core_Key_Sate[512];
char Core_old_Key_Sate[512];

retro_log_printf_t log_cb;
retro_video_refresh_t video_cb;

static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;

retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

u8 *disk = NULL;
u32 diskLength;

int width, height;


BOOL loadGame(void)
{
    CDGLoad(openCDGFilename);

    FILE *fic = fopen(openMP3Filename, "rb");
    if (fic == NULL) {
        return 0;
    }
    fseek(fic, 0, SEEK_END);
    mp3Length = ftell(fic);
    fseek(fic, 0, SEEK_SET);

    mp3 = (char *)malloc(mp3Length);
    if (mp3 == NULL) {
        return 0;
    }
    fread(mp3, 1, mp3Length, fic);
    fclose(fic);

    mp3Mad = mad_init();

    mp3Position = 0;

    soundEnd = 0;

    return true;
} /* loadGame */

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
    va_list va;

    (void)level;

    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

void retro_init(void)
{
    char *savedir = NULL;
    int i;

    environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &savedir);


    updateFromEnvironnement();

    soundBuffer = (s16 *)malloc(32768);

    width = 320;
    height = 240;

}

void retro_deinit(void)
{

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
    info->library_version = "git" GIT_VERSION;
#else
    info->library_version = "svn";
#endif

}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    info->timing.fps = FPS;
    info->timing.sample_rate = 44100.0;

    info->geometry.base_width   = 320;
    info->geometry.base_height  = 240;

    info->geometry.max_width    = 320;
    info->geometry.max_height   = 240;
    info->geometry.aspect_ratio = 1;
}

void retro_set_environment(retro_environment_t cb)
{
    struct retro_log_callback logging;

    environ_cb = cb;

    if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging)) {
        log_cb = logging.log;
    } else {
        log_cb = fallback_log;
    }

    log_cb = fallback_log;


    static const struct retro_variable vars[] = {
        { "pocketcdg_resize", "Resize; 320x240|Overscan"                                                         },
        { NULL,               NULL                                                                               },
    };

    cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void *)vars);


    static const struct retro_controller_description port[] = {
        { "RetroPad",      RETRO_DEVICE_JOYPAD                       },
        { "RetroKeyboard", RETRO_DEVICE_KEYBOARD                     },
    };

    static const struct retro_controller_info ports[] = {
        { port, 2 },
        { port, 2 },
        { NULL, 0 },
    };

    cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void *)ports);

} /* retro_set_environment */

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

    // loadGame();
}

// static void frame_time_cb(retro_usec_t usec)
// {
//     frame_time = usec / 1000000.0;

// }


void readIni(void)
{
    /*
     * CURSOR_UP, CURSOR_RIGHT, CURSOR_DOWN, F9, F6, F3, SMALL_ENTER, FDOT, CURSOR_LEFT, COPY, F7, F8, F5, F1, F2, F0, CLR, OPEN_SQUARE_BRACKET, RETURN, CLOSE_SQUARE_BRACKET, F4, SHIFT, FORWARD_SLASH, CONTROL, HAT, MINUS, AT, P, SEMICOLON, COLON, BACKSLASH, DOT, ZERO, 9, O, I, L, K, M, COMMA, 8, 7, U, Y, H, J, N, SPACE, 6, 5, R, T, G, F, B, V, 4, 3, E, W, S, D, C, X, 1, 2, ESC, Q, TAB, A, CAPS_LOCK, Z, JOY_UP, JOY_DOWN, JOY_LEFT, JOY_RIGHT, JOY_FIRE1, JOY_FIRE2, SPARE, DEL */

}

void updateFromEnvironnement()
{
    struct retro_variable pk1var = { "pocketcdg_resize" };

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &pk1var) && pk1var.value) {
        if (!strcmp(pk1var.value, "green")) {
            // ExecuteMenu(&gb, ID_GREEN_MONITOR, NULL);
        }
    }

}

void retro_key_down(int key)
{
    log_cb(RETRO_LOG_INFO, "key: %d\n", key);
}


char framebuf[128];

static int framecount = 0;

void retro_run(void)
{
    static bool updated = false;

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        updateFromEnvironnement();
    }

    input_poll_cb();


    getFrame(pixels, framecount * (1000 / FPS), FPS);
    framecount++;

    video_cb(pixels, width, height, width * 2);


// Play mp3 (to clean)

#define NEEDFRAME (44100 / 50) // 32768 max
#define NEEDBYTE  (NEEDFRAME * 4) // 32768 max

    int error = 0;

    while (soundEnd <= NEEDBYTE) {

        int length = 2048;
        int read;
        int done; // en bytes
        int resolution = 16;
        int halfsamplerate = 0;

        int retour = mad_decode(mp3Mad, mp3 + mp3Position, length, (char *)soundBuffer + soundEnd, 10000, &read, &done, resolution, halfsamplerate);

        soundEnd += done;

        if (done == 0) {
            fprintf(stderr, "map decode (Err:%d) %d (%d, %d) %d\n", retour, mp3Position, read, done, soundEnd);
            read++; // Skip in case of error... Maybe ID3 Tag ?
            error++;
            if (error > 265536) {
                break;
            }
        }

        mp3Position += read;
    }

    audio_batch_cb(soundBuffer, NEEDFRAME);

    soundEnd -= NEEDBYTE;

    memcpy( (char *)soundBuffer,  (char *)soundBuffer + NEEDBYTE, soundEnd);

} /* retro_run */




bool retro_load_game(const struct retro_game_info *info)
{
    struct retro_frame_time_callback frame_cb;
    struct retro_input_descriptor desc[] = {
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left"                  },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up"                    },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down"                  },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right"                 },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start"                 },
        { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Pause"                 },
        { 0 },
    };

    log_cb(RETRO_LOG_INFO, "begin of load games\n");

    environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

    // Init pixel format

    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
        if (log_cb)
            log_cb(RETRO_LOG_INFO, "XRGG565 is not supported.\n");
        return 0;
    }

    // Load .cdg

    strcpy(openCDGFilename, info->path);

    strcpy(openMP3Filename, openCDGFilename);
    if (strlen(openMP3Filename) > 4) {
        openMP3Filename[strlen(openMP3Filename) - 3] = 0;
        strcat(openMP3Filename, "mp3");
    }


    log_cb(RETRO_LOG_INFO, "open cdg file: %s\n", info->path);
    log_cb(RETRO_LOG_INFO, "open mp3 file: %s\n", openMP3Filename);

    return loadGame();
} /* retro_load_game */

void retro_unload_game(void)
{
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

