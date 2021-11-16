#include <signal.h>
#include <streams/file_stream.h>

#include "platform.h"

#define COL_MASK       15
#define SC_MASK        0x3F
#define SC_CDG_COMMAND 0x09

#define GP_RGB24(r, g, b)      (((((r >> 3)) & 0x1f) << 11) | ((((g >> 3)) & 0x1f) << 6) | ((((b >> 3)) & 0x1f) << 1))
#define CDG_Pixel(x, y)        CDG_screenBuffer[(240 * (x)) + (240 - (y))]
#define CDG_pal_Pixel(x, y, c) CDG_pal_screenBuffer[(x) * xPitch0 + (y) * yPitch0] = palette[c]

typedef struct {
    unsigned char command;
    unsigned char instruction;
    unsigned char parityQ[2];
    unsigned char data[16];
    unsigned char parityP[4];
} SUBCODE;

/* Forward declarations */
RFILE* rfopen(const char *path, const char *mode);
int rfclose(RFILE* stream);
int64_t rfread(void* buffer,
   size_t elem_size, size_t elem_count, RFILE* stream);

/* Variables */
int save, load;
int xPitch0, yPitch0;
u8 *CDG_screenBuffer;
u16 *CDG_pal_screenBuffer;
static RFILE *fp;
u16 palette[16];
int pos_cdg;
int firsttime;
int play;
int stop;
int action;
int pauseCDG;
int pause_pos;
int cdg_refresh;

#define RGB15(R, G, B) ((((R) & 0xF8) << 8) | (((G) & 0xFC) << 3) | (((B) & 0xF8) >> 3))

// ---

void CDG_Handler(SUBCODE *subCode);
void CDG_MemPreset(unsigned char *data);
void CDG_BorderPreset(unsigned char *data);
void CDG_TileBlock(unsigned char *data);
void GpSetPaletteEntry(u8 i, u8 r, u8 g, u8 b);
void CDG_LoadCLUT(unsigned char *data, short first);
void CDG_TileBlockXOR(unsigned char *data);
void CDG_SetTransparentColor(unsigned char *data);
void CDG_Reset(void);

// ---


void GpSetPaletteEntry(u8 i, u8 r, u8 g, u8 b)
{
   palette[i] = RGB15(r, g, b);
   cdg_refresh = 1;
}

void CDGUnload(void)
{
   if (fp)
     rfclose(fp);
   fp = NULL;
}

void CDGLoad(const char *filename)
{
   int n;
   firsttime = 1;

   for (n = 0; n < 16; n++)
      GpSetPaletteEntry(n, 0, 0, 0);

   CDG_screenBuffer = (unsigned char *)malloc(76800);
   memset(CDG_screenBuffer, 0, 76800);

   cdg_refresh = 0;

   pos_cdg = 0;

   pauseCDG = 0;
   save = 0;
   load = 0;
   action = 0;

   fp = rfopen(filename, "rb");
}

void getFrame(u16 *frame, int pos_mp3, int fps)
{
   SUBCODE subCode;
   unsigned long m_size;
   int n;
   int cont = 1;

   if (pauseCDG == 1)
      cont = 0;

   if (!fp)
      cont = 0;

   if (cont)
   {
      int until;
      if (firsttime == 1)
      {

         memset(frame, 0, 320 * 240 * 2);

         // [backbuffer FillRect:NULL dwColor:RGB(0, 0, 0) dwFlags:0 GDFillRectFx:NULL];
         firsttime = 0;
      }



      xPitch0 = 1;
      yPitch0 = 320;
      CDG_pal_screenBuffer = frame;

#if 0
      if (pos_mp3 == -1) {
         // [self Shutdown];
      }
#endif

      /*
       * int old_pos_cdg;         // pos_cdg  en 1000/300 ms
       * old_pos_cdg = pos_cdg;
       */

      until = 300 / fps;

      if ((pos_mp3 * 3 - pos_cdg * 10) > 300) // Plus d'une seconde de decalage
         until = (pos_mp3 * 3 - pos_cdg * 10) / 10;
      if ((pos_mp3 * 3 - pos_cdg * 10) < -300) // Plus d'une seconde de decalage
         until = 0;

      for (n = 0; n < until; n++)
      {
         pos_cdg++;

         m_size = rfread(&subCode, 1, sizeof(SUBCODE), fp);
         if (m_size != 0)
         {
            if ((subCode.command & SC_MASK) == SC_CDG_COMMAND)
               CDG_Handler(&subCode);
         }
      }
   }
} /* getFrame */

void CDG_Handler(SUBCODE *subCode)
{
   static int p = 0;

   switch (subCode->instruction & SC_MASK) {
      case 1: // Memory Preset
         CDG_MemPreset(subCode->data);
         break;

      case 2: // Border Preset
         CDG_BorderPreset(subCode->data);
         break;

      case 6: // Tile Block (Normal)
         CDG_TileBlock(subCode->data);
         break;


      case 30: // Load Color Table (entries 0-7)
         CDG_LoadCLUT(subCode->data, 0);
         break;

      case 31: // Load Color Table (entries 8-15)
         CDG_LoadCLUT(subCode->data, 8);
         break;

      case 38: // Tile Block (XOR)
         CDG_TileBlockXOR(subCode->data);
         break;

      case 28: // Define Transparent Color
         CDG_SetTransparentColor(subCode->data);
         break;

         // Presumably, this is not very useful unless we do
         // video overlays in a future version.


      case 20: // Scroll Preset

         // I have no idea how this is supposed to work.. I
         // need to find a CD+G subcode dump that utilizes
         // this feature.
      case 24: // Scroll Copy

         // Ditto on the above Scroll Preset comment
      default:
         subCode->instruction = 0;
         break;
   } /* switch */

   p++;
   if (p == 50) {
      p = 0;
      if (cdg_refresh == 1) {
         CDG_Reset();
         cdg_refresh = 0;
      }
   }
}

void CDG_MemPreset(unsigned char *data)
{
   int color = data[0] & COL_MASK;
   short repeat = data[1] & SC_MASK;

   if (repeat == 0)
   {
      int oldpos, pos;
      int x, y, j;
      u16 c = palette[color];

      for (j = 240 * 10; j < 240 * 310; j += 240)
      {
         for (y = 12; y < 228; y++)
            CDG_screenBuffer[j + y] = color;
      }

      pos = 10 * xPitch0 + 12 * yPitch0;
      for (x = 10; x < 310; x++)
      {
         oldpos = pos;
         for (y = 12; y < 228; y++)
         {
            CDG_pal_screenBuffer[pos] = c;
            pos += yPitch0;
         }
         pos = oldpos + xPitch0;
      }
   }
} /* CDG_MemPreset */

void CDG_BorderPreset(unsigned char *data)
{
   int i, j;
   u16 c;
   int oldpos, pos;
   int x, y;

   unsigned char color = data[0] & COL_MASK;

   for (i = 0; i < 2400; i++)
      CDG_screenBuffer[i] = color;

   for (j = 2400; j < 74400; j += 240)
   {
      for (y = 228; y < 240; y++)
         CDG_screenBuffer[j + y] = color;

      for (y = 0; y < 12; y++)
         CDG_screenBuffer[j + y] = color;
   }

   for (i = 74400; i < 76800; i++)
      CDG_screenBuffer[i] = color;

   c = palette[color];

   // Haut

   pos = 0 * xPitch0 + 0 * yPitch0;
   for (x = 0; x < 320; x++)
   {
      oldpos = pos;
      for (y = 0; y < 12; y++)
      {
         CDG_pal_screenBuffer[pos] = c;
         pos += yPitch0;
      }
      pos = oldpos + xPitch0;
   }

   // Bas

   pos = 0 * xPitch0 + 228 * yPitch0;
   for (x = 0; x < 320; x++) {
      oldpos = pos;
      for (y = 228; y < 240; y++) {
         CDG_pal_screenBuffer[pos] = c;
         pos += yPitch0;
      }
      pos = oldpos + xPitch0;
   }

   // Gauche

   pos = 0 * xPitch0 + 12 * yPitch0;
   for (x = 0; x < 10; x++) {
      oldpos = pos;
      for (y = 12; y < 228; y++) {
         CDG_pal_screenBuffer[pos] = c;
         pos += yPitch0;
      }
      pos = oldpos + xPitch0;
   }

   // Droit

   pos = 310 * xPitch0 + 12 * yPitch0;
   for (x = 310; x < 320; x++) {
      oldpos = pos;
      for (y = 12; y < 228; y++) {
         CDG_pal_screenBuffer[pos] = c;
         pos += yPitch0;
      }
      pos = oldpos + xPitch0;
   }
}

void CDG_TileBlock(unsigned char *data)
{
   unsigned char color0 = data[0] & COL_MASK;
   unsigned char color1 = data[1] & COL_MASK;
   unsigned char c;

   short row = data[2] & 0x1F;
   short col = data[3] & 0x3F;

   int x_pos = (col * 6) + 10;
   int y_pos = (row * 12) + 12;

   int y;

   for (y = 0; y < 12; y++) {
      c = data[y + 4] & 0x20 ? color1 : color0;
      CDG_Pixel(x_pos, y_pos + y) = c;
      CDG_pal_Pixel(x_pos, y_pos + y, c);
      c = data[y + 4] & 0x10 ? color1 : color0;
      CDG_Pixel(x_pos + 1, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 1, y_pos + y, c);
      c = data[y + 4] & 0x08 ? color1 : color0;
      CDG_Pixel(x_pos + 2, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 2, y_pos + y, c);
      c = data[y + 4] & 0x04 ? color1 : color0;
      CDG_Pixel(x_pos + 3, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 3, y_pos + y, c);
      c = data[y + 4] & 0x02 ? color1 : color0;
      CDG_Pixel(x_pos + 4, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 4, y_pos + y, c);
      c = data[y + 4] & 0x01 ? color1 : color0;
      CDG_Pixel(x_pos + 5, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 5, y_pos + y, c);
   }
}

void CDG_LoadCLUT(unsigned char *data, short first)
{
   int i;
   int j = 0;

   for (i = 0; i < 8; i++)
   {
      unsigned char r = ((data[j]  & SC_MASK) >> 2) * 17;
      unsigned char g = (((data[j] & 0x03) << 2) | ((data[j + 1] & SC_MASK) >> 4)) * 17;
      unsigned char b = (data[j + 1]  & 0x0F) * 17;
      j += 2;

      GpSetPaletteEntry(i + first, r, g, b);
   }
}

void CDG_TileBlockXOR(unsigned char *data)
{
   unsigned char c;
   unsigned char color0 = data[0] & COL_MASK;
   unsigned char color1 = data[1] & COL_MASK;
   short row = data[2] & 0x1F;
   short col = data[3] & 0x3F;

   int x_pos = (col * 6) + 10;
   int y_pos = (row * 12) + 12;

   int y;

   for (y = 0; y < 12; y++)
   {
      c =  CDG_Pixel(x_pos, y_pos + y) ^ (data[y + 4] & 0x20 ? color1 : color0);
      CDG_Pixel(x_pos, y_pos + y) = c;
      CDG_pal_Pixel(x_pos, y_pos + y, c);
      c =  CDG_Pixel(x_pos + 1, y_pos + y) ^ (data[y + 4] & 0x10 ? color1 : color0);
      CDG_Pixel(x_pos + 1, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 1, y_pos + y, c);
      c =  CDG_Pixel(x_pos + 2, y_pos + y) ^ (data[y + 4] & 0x08 ? color1 : color0);
      CDG_Pixel(x_pos + 2, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 2, y_pos + y, c);
      c =  CDG_Pixel(x_pos + 3, y_pos + y) ^ (data[y + 4] & 0x04 ? color1 : color0);
      CDG_Pixel(x_pos + 3, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 3, y_pos + y, c);
      c =  CDG_Pixel(x_pos + 4, y_pos + y) ^ (data[y + 4] & 0x02 ? color1 : color0);
      CDG_Pixel(x_pos + 4, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 4, y_pos + y, c);
      c =  CDG_Pixel(x_pos + 5, y_pos + y) ^ (data[y + 4] & 0x01 ? color1 : color0);
      CDG_Pixel(x_pos + 5, y_pos + y) = c;
      CDG_pal_Pixel(x_pos + 5, y_pos + y, c);
   }
}

void CDG_SetTransparentColor(unsigned char *data)
{
}

void CDG_Reset(void)
{
   int oldpos;
   int x, y, j;
   int pos = 10 * xPitch0 + 12 * yPitch0;
   for (x = 10, j = 240 * 10; x < 310; x++, j += 240)
   {
      oldpos = pos;
      for (y = 12; y < 228; y++)
      {
         CDG_pal_screenBuffer[pos] = palette[CDG_screenBuffer[j + (240 - y)]];
         pos += yPitch0;
      }
      pos = oldpos + xPitch0;
   }
}
