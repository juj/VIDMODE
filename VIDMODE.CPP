#include <stdio.h>
#include <dos.h>
#include <conio.h>
#include <string.h>
#include <ctype.h>

#define KEY_ESC    0x1B
#define KEY_LEFT  0x14B
#define KEY_RIGHT 0x14D
#define KEY_UP    0x148
#define KEY_DOWN  0x150

int get_key()
{
  int key = getch();
  if (!key) key = 0x100 | getch();
  return key;
}

void write_text(int x, int y, const char *text, unsigned char attr)
{
  int textLen = strlen(text);
  int textSeg = FP_SEG(text);
  int textOfs = FP_OFF(text);
  unsigned int pos = ((unsigned int)y) << 8 | x;
  asm {
    push bp
    push es
    push dx
    push cx
    push bx
    push ax
    mov ah, 13h
    mov al, 1
    mov bh, 0
    mov bl, attr
    mov cx, textLen
    mov dx, pos
    mov es, textSeg
    mov bp, textOfs
    int 10h
    pop ax
    pop bx
    pop cx
    pop dx
    pop es
    pop bp
  }
}

void put_pixel(int x, int y, unsigned char color)
{
  union REGS regs;
  regs.h.ah = 0x0C; // INT 10h, 0x0C - Write Graphics Pixel at Coordinate
  regs.h.al = color;
  regs.h.bh = 0;
  regs.x.cx = x;
  regs.x.dx = y;
  int86(0x10, &regs, &regs);
}

int get_current_video_mode()
{
  union REGS regs;
  regs.h.ah = 0x0F; // INT 10h, 0x0F - Get Video State
  int86(0x10, &regs, &regs);
  return regs.h.al;
}

int set_video_mode(int mode)
{
  union REGS regs;
  regs.h.ah = 0;
  regs.h.al = mode;
  int86(0x10, &regs, &regs);
  return regs.x.ax;
}

#define TEXT 0
#define GFX  1
const char * const GfxStrings[] = { "TEXT", "GFX" };

#define MONO_2  0
#define COL_4   1
#define COL_16  2
#define MONO_16 3
#define COL_256 4
const char * const BppStrings[] = { "B/W", "4 colors", "16 colors", "16 shades gray", "256 colors" };

#define MDA  0
#define CGA  1
#define EGA  2
#define MCGA 3
#define VGA  4
#define PCJR 5
const char * const StdStrings[] = { "MDA", "CGA", "EGA", "MCGA", "VGA", "PCJR" };

struct Mode
{
  int mode; // int 10h video mode number
  int gfx; // 1: gfx mode, 0: text mode
  int width;
  int height;
  int bpp; // gfx mode: bits per pixel: text mode: bits per character
  int std;
  int failed;
};

Mode modes[] =
{
  {    0, TEXT,  40,  25,  MONO_2, CGA  },
  {    1, TEXT,  40,  25,  COL_16, CGA  },
  {    2, TEXT,  80,  25, MONO_16, CGA  },
  {    3, TEXT,  80,  25,  COL_16, CGA  },
  {    4,  GFX, 320, 200,   COL_4, CGA  },
  {    5,  GFX, 320, 200,   COL_4, CGA  },
  {    6,  GFX, 640, 200,  MONO_2, CGA  },
  {    7, TEXT,  80,  25,  MONO_2, MDA  },
  {    8,  GFX, 160, 200,  COL_16, PCJR },
  {    9,  GFX, 320, 200,  COL_16, PCJR },
  {  0xA,  GFX, 640, 200,   COL_4, PCJR },
  {  0xD,  GFX, 320, 200,  COL_16, EGA  },
  {  0xE,  GFX, 640, 200,  COL_16, EGA  },
  {  0xF,  GFX, 640, 350,  MONO_2, EGA  },
  { 0x10,  GFX, 640, 480,  COL_16, EGA  },
  { 0x11,  GFX, 640, 480,  MONO_2, MCGA },
  { 0x12,  GFX, 640, 480,  COL_16, VGA  },
  { 0x13,  GFX, 320, 200, COL_256, MCGA }
};
#define NUMELEMS(x) (sizeof(x) / sizeof((x)[0]))

int white_color(Mode *mode)
{
  if (mode->mode == 0) return 15;
  switch(mode->bpp)
  {
  case MONO_2: return mode->std == MDA ? 9 : 1;
  case COL_4:  return 3;
  default: return 15;
  }
}

int text_height(Mode *mode)
{
  if (!mode->gfx) return mode->height;
  return 25;
}

void write_lorem_ipsum()
{
  for(int y = 0; y < 16; ++y)
    write_text(0, y+1, "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.", y);
}

// Draws a rectangle (0,0) x (w-1,h-1) with all colors
void draw_rainbow_rectangle(int w, int h)
{
  for(int x = 0; x < w; ++x)
  {
    put_pixel(x, 0, x);
    put_pixel(x, h-1, x);
  }
  for(int y = 0; y < h; ++y)
  {
    put_pixel(0, y, y);
    put_pixel(w-1, y, y);
  }
}

void fill_rectangle(int w, int h, unsigned char color)
{
  for(int y = 0; y < h; ++y)
    for(int x = 0; x < w; ++x)
      put_pixel(x, y, color);
}

void fill_text_rect(int w, int h, unsigned char color)
{
  for(int y = 0; y < h; ++y)
    for(int x = 0; x < w; ++x)
      write_text(x, y, " ", (color << 4) | 7);
}

void fill_screen(Mode *mode, unsigned char color)
{
  if (mode->gfx) fill_rectangle(mode->width, mode->height, color);
  else fill_text_rect(mode->width, mode->height, color);
}

unsigned char read_misc_output_register()
{
  // bit 7: VSync polarity
  // bit 6: HSync polarity
  // bit 5: Odd/Even Page Select: "Selects the upper/lower 64K page of memory when the system is in an even/odd mode (modes 0,1,2,3,7)"
  // bits 3-2: Clock select: 0=25MHz (320/640 pixel modes), 1=28MHz (360/720 pixel modes)
  //                         2=undefined (external clock 1) 3=undefined (external clock 2)
  // bit 1: RAM Enable: "Controls system access to the display buffer."
  //                         0=disable address decode for the display buffer from the system.
  // bit 0: Input/Output Address Select:
  //          "0=CRT controller addresses=0x3Bx and Input Status Register 1=0x3BA for compatibility with the monochrome adapter.
  //           1=CRT controller addresses=0x3Dx and Input Status Register 1=0x3DA for compatibility with the color/graphics adapter.
  return inp(0x3CC);
}

unsigned char read_CRTC_subreg(int subRegIndex)
{
  disable();
  unsigned int crtc_address = (read_misc_output_register() & 1) ? 0x3D4 : 0x3B4;
  outp(crtc_address, subRegIndex);
  unsigned char val = inp(crtc_address+1);
  enable();
  return val;
}

unsigned char read_SEQ_subreg(int subRegIndex)
{
  disable();
  outp(0x3C4, subRegIndex);
  unsigned char val = inp(0x3C5);
  enable();
  return val;
}

void dump_CRTC()
{
  unsigned char r[24];
  for(int i = 0; i < 24; ++i)
    r[i] = read_CRTC_subreg(i);
  gotoxy(1,2);
  printf("00h: Horiz. Total: %u\n", r[0]);
  printf("01h: End Horiz. Display: %u\n", r[1]);
  printf("02h: Start Horiz. Blank: %u\n", r[2]);
  printf("03h: End Horiz. Blank: %u\n", r[3]);
  printf("04h: Start Horiz. Retrace: %u\n", r[4]);
  printf("05h: End Horiz. Retrace: %u\n", r[5]);
  printf("06h: Vertical Total: %u\n", r[6]);
  printf("07h: Overflow: %u\n", r[7]);
  printf("08h: Preset Row Scan: %u\n", r[8]);
  printf("09h: Maximum Scan Line: %u\n", r[9]);
//  printf("0Ah: Cursor Start: %u\n", r[10]);
//  printf("0Bh: Cursor End: %u\n", r[11]);
//  printf("0Ch: Start Address High: %u\n", r[12]);
//  printf("0Dh: Start Address Low: %u\n", r[13]);
  printf("0C-0Dh: VGA Start Offset: 0x%X\n", ((unsigned int)r[12]) << 8 | r[13]);
//  printf("0Eh: Cursor Location High: %u\n", r[14]);
//  printf("0Fh: Cursor Location Low: %u\n", r[15]);
  printf("0E-0Fh: Cursor Address: 0x%X\n", ((unsigned int)r[14]) << 8 | r[15]);
  printf("10h: Vertical Retrace Start: %u\n", r[16]);
  printf("11h: Vertical Retrace End: %u\n", r[17]);
  printf("12h: Vertical Display End: %u\n", r[18]);
  printf("13h: Offset: %u\n", r[19]);
  printf("14h: Underline Location: %u\n", r[20]);
  printf("15h: Start Vertical Blank: %u\n", r[21]);
  printf("16h: End Vertical Blank: %u\n", r[22]);
  printf("17h: CRTC Mode Control: %u\n", r[23]);
  printf("18h: Line Compare: %u\n", r[23]);
}

int calc_signal_end(int start_clock, int clock_max, int end_clock_mod, int bits)
{
  for(int x = start_clock+1; x != start_clock; x = (x+1)%clock_max)
    if ((x & ((1 << bits) - 1)) == end_clock_mod)
      return x;
  return start_clock;
}

void dump_timings(Mode *mode)
{
  gotoxy(1,2);
  unsigned char r[24];
  for(int i = 0; i < 24; ++i)
    r[i] = read_CRTC_subreg(i);

  unsigned char misc = read_misc_output_register();
  unsigned char hsyncpol = (misc >> 6) & 1;
  unsigned char vsyncpol = (misc >> 7) & 1;

  unsigned char clocking = read_SEQ_subreg(0x1);
  unsigned int dots = (clocking & 1) ? 8 : 9;
  unsigned int clkdiv = (clocking & 8) ? 2 : 1;

  unsigned char clock = (misc >> 2) & 0x03;
  float clks[4] = { 25.175f, 28.322f, 0.f, 0.f };
  const char *clocks[4] = { "25.175 MHz", "28.322 MHz", "(external 0x10)", "(external 0x11)" };
  const char *clocksDiv[4] = { "12.5875 MHz", "14.161 MHz", "? MHz", "? MHz" };
  if (clkdiv == 2)
    printf("Clock: %s/2=%s, px/ch: %u\n",
      clocks[clock], clocksDiv[clock], dots);
  else
    printf("Clock: %s (clkdiv=%d), dots/char: %u\n",
      clocks[clock], clkdiv, dots);

  int scandoubling = r[9] >> 7;
  if (mode->gfx)
    printf("Scanline mult.: %u, scan doubling: %d (%d lines)\n",
	    (r[9] & 31) + 1, scandoubling,
	    scandoubling ? mode->height*2 : mode->height);
  else
  {
    int textheight = (r[9] & 31) + 1;
    printf("Text height: %u lines, scan doubling: %d (%dx%d=%d lines)\n",
	    textheight, scandoubling,
	    mode->height, textheight,
	    (scandoubling ? 2 : 1) * mode->height*textheight);
  }

  unsigned int htotal = (unsigned int)r[0] + 5;

  float hclock = clks[clock] * 1000.0f / clkdiv / (htotal * dots);

  printf("\nHsync pol: %c, Hclock: %f KHz\n",
    hsyncpol ? '-' : '+', hclock);

  unsigned int hend = (unsigned int)r[1] + 1;
  unsigned int henableskew = (r[3] >> 5) & 3;
  printf("Horiz. total: %u, end: %u\n", htotal*dots, hend*dots);
  unsigned int hblankstart = r[2];
  unsigned int hblankend = (r[3] & 31) | (r[5] >> 2);
  hblankend = calc_signal_end(hblankstart, htotal, hblankend, 6);
  printf("Hblank start: %u (skew: %u), end: %u\n", hblankstart*dots, henableskew*dots, hblankend*dots);

  unsigned int hretracestart = r[4];
  unsigned int hretraceskew = (r[5] >> 5) & 3;
  unsigned int hretraceend = r[5] & 31;
  hretraceend = calc_signal_end(hretracestart, htotal, hretraceend, 5);
  printf("Hretrace start: %u (skew: %u), end: %u\n", hretracestart*dots, hretraceskew*dots, hretraceend*dots);

  unsigned int vtotal = r[6] | ((((unsigned int)r[7]) & 1) << 8) | ((((unsigned int)r[7]) & 32) << 4);

  printf("\nVsync pol: %c, Vclock: %f hz\n",
    vsyncpol ? '-' : '+', hclock * 1000.0f / vtotal);

  unsigned int vdisplayend = r[18] | ((((unsigned int)r[7]) & 2) << 7) | ((((unsigned int)r[7]) & 64) << 3);
  printf("Vertical total: %u, end: %u\n", vtotal, vdisplayend);

  unsigned int vretracestart = r[16] | ((((unsigned int)r[7]) & 4) << 6) | ((((unsigned int)r[7]) & 128) << 2);
  unsigned int vretraceend = r[17] & 15;
  vretraceend = calc_signal_end(vretracestart, vtotal, vretraceend, 4);
  printf("Vretrace start: %u, end: %u\n", vretracestart, vretraceend);

  unsigned int vblankstart = r[21] | ((((unsigned int)r[7]) & 8) << 5) | ((((unsigned int)r[9]) & 32) << 4);
  unsigned int vblankend = r[22] & 127;
  vblankend = calc_signal_end(vblankstart, vtotal, vblankend, 7);

  printf("Vblank start: %u, end: %u\n", vblankstart, vblankend);
}

void draw_mode_screen(Mode *mode)
{
  fill_screen(mode, 0);
  char str[256];
  sprintf(str, "Mode %2Xh: %s %3dx%3d %4s %s\n",
    mode->mode, StdStrings[mode->std], mode->width, mode->height,
    GfxStrings[mode->gfx], BppStrings[mode->bpp]);

  write_text(0, 0, str, white_color(mode));

  sprintf(str, "%c%c: Cycle video mode, %c%c: Cycle test",
    0x1B, 0x1A, 0x18, 0x19);
  write_text(0, text_height(mode)-1, str, white_color(mode));

//  dump_CRTC();
  dump_timings(mode);
//  write_lorem_ipsum();

  if (mode->gfx)
    draw_rainbow_rectangle(mode->width, mode->height);
}

int test_video_mode(Mode *mode)
{
  set_video_mode(mode->mode);
  int vidmode = get_current_video_mode();
  if (vidmode != mode->mode)
  {
    mode->failed = 1;
    return 0;
  }

  int screen = 0;
  for(;;)
  {
    if (screen == 0) draw_mode_screen(mode);
    else fill_screen(mode, (screen + 4) % 16);
    int key = get_key();
    if (key == KEY_LEFT) return -1;
    else if (key == KEY_RIGHT) return 1;
    else if (key == KEY_UP || key == KEY_DOWN)
    {
      int d = (key == KEY_UP) ? 1 : -1;
      int mod = mode->bpp == COL_4 ? 5 : (mode->gfx ? 17 : 9);
      screen = (screen + d + mod) % mod;
    }
    else break;
  }
  return 0;
}

const char *chs = "0123456789ADEFGHIJKLMNOPQRSTUVWXYZ";
int main()
{
  int start_mode = get_current_video_mode();

  for(;;)
  {
    clrscr();
    printf("Choose video mode to test:\n");
    for(int i = 0; i < NUMELEMS(modes); ++i)
    {
      printf("  %c. Mode %2Xh: %s\t%3dx%3d\t%4s %s%s\n",
	chs[i], modes[i].mode, StdStrings[modes[i].std],
	modes[i].width, modes[i].height,
	GfxStrings[modes[i].gfx],
	BppStrings[modes[i].bpp],
	modes[i].failed ? "\tFAILED" : "");
    }
    printf("\nPress ESC to quit.\n");
    int tested = 0;
    while(!tested)
    {
      int key = get_key();
      if (key == KEY_ESC)
	return 0;
      key = toupper(key);
      for(int j = 0; j < strlen(chs) && j < NUMELEMS(modes); ++j)
      {
	if (key == chs[j])
	{
	  for(;;)
	  {
	    int nextMode = test_video_mode(&modes[j]);
	    if (nextMode == 0)
	      break;
	    for(;;)
	    {
	      j = (j + nextMode + NUMELEMS(modes)) % NUMELEMS(modes);
	      if (!modes[j].failed)
		break;
	    }
	  }
	  set_video_mode(start_mode);
	  tested = 1;
	}
      }
    }
  }
}
