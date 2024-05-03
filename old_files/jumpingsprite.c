#include <stdlib.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>

// include NESLIB header
#include "neslib.h"

// include CC65 NES Header (PPU)
#include <nes.h>

// link the pattern table into CHR ROM
//#link "chr_generic.s"

// BCD arithmetic support
#include "bcd.h"
//#link "bcd.c"

// VRAM update buffer
#include "vrambuf.h"
//#link "vrambuf.c"

#define COLS 30
#define ROWS 60

#define SCREEN_Y_BOTTOM 208	// bottom of screen in pixels
#define JUMP_VEL 10

//static int scroll_pixel_yy = 0;
//static byte player_screen_y = 0;
char oam_id;
byte doodlex, doodley, dy, yvel;

#define DEF_METASPRITE_2x2(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        0,      8,      (code)+1,   pal, \
        8,      0,      (code)+2,   pal, \
        8,      8,      (code)+3,   pal, \
        128};


DEF_METASPRITE_2x2(playerR, 0xe8, 0);
DEF_METASPRITE_2x2(playerL, 0xe8, 0);

typedef struct Platform {
  byte ypos;
  byte xpos;
  byte height;
} Platform;

Platform platforms[1];

/*{pal:"nes",layout:"nes"}*/
const char PALETTE[32] = { 
  0x03,			// screen color

  0x11,0x30,0x27,0x0,	// background palette 0
  0x1c,0x20,0x2c,0x0,	// background palette 1
  0x00,0x10,0x20,0x0,	// background palette 2
  0x06,0x16,0x26,0x0,   // background palette 3

  0x16,0x35,0x24,0x0,	// sprite palette 0
  0x00,0x37,0x25,0x0,	// sprite palette 1
  0x0d,0x2d,0x3a,0x0,	// sprite palette 2
  0x0d,0x27,0x2a	// sprite palette 3
};

word getntaddr(byte x, byte y) {
  word addr;
  if (y < 30) {
    addr = NTADR_A(x,y);	// nametable A
  } else {
    addr = NTADR_C(x,y-30);	// nametable C
  }
  return addr;
}

// convert nametable address to attribute address
word nt2attraddr(word a) {
  return (a & 0x2c00) | 0x3c0 |
    ((a >> 4) & 0x38) | ((a >> 2) & 0x07);
}

byte rndint(byte a, byte b) {
  return (rand() % (b-a)) + a;
}

void create_platforms() {
  byte i, x;
  byte y = 2;
  Platform* p;
  
  for (i=0; i<20; i++) {
    p = &platforms[i];
    p->ypos = y;
    x = rndint(1, 4);
    p->height = x;
    y += x;
    
    /*
    vram_adr(NTADR_A(2,2));
    itoa(p->ypos, buffr, 10);
    vram_write(buffr, 2);
    */
    p->xpos = rndint(0, 27);
  }
}

void draw_floor(byte row_height) {
  byte floor, dy, rowy;
  char buff[COLS];
  //char buffr[30];
  /*
  vram_adr(NTADR_A(2, a->ypos));		// set address
  itoa(dy, buffr, 10);
  vram_write(buffr, 2);
  */
  word addr;
  char attrs[8];
  Platform* a;
  
  for (floor=0; floor < 20; floor++) {
    a = &platforms[floor];
    dy = row_height - a->ypos;
    
    memset(buff, 0, sizeof(buff));
    if (dy < a->height) {
      
      if (a->ypos == row_height) {
        
        buff[a->xpos] = 0x8c;
        buff[a->xpos+1] = 0x8c;
      }
      buff[0] = 0x85;
      buff[COLS-1] = 0x8a;
      //vram_write(buff, 4);
    }
    rowy = (ROWS-1) - (row_height % ROWS);
    addr = getntaddr(1, rowy);
    if ((addr & 0x60) == 0) {
      byte b;
      if (dy==1)
        b = 0x05;	// top of attribute block
      else if (dy==3)
        b = 0x50;	// bottom of attribute block
      else
        b = 0x00;	// does not intersect attr. block
      // write entire row of attribute blocks
      memset(attrs, b, 8);
      vrambuf_put(nt2attraddr(addr), attrs, 8);
    }
    vrambuf_put(addr, buff,COLS);
  }
}
/*
void draw_one() {
  byte floor, dy, rowy;
  char buff[1];
  word addr;
  char attrs[8];
  Platform* a;
  
  
}*/

void draw_platforms() {
  byte i;
  Platform* x;
  
  //draw_one();
  
  for (i=0; i<ROWS; i++) {
    x = &platforms[i];
    draw_floor(i);
    vrambuf_flush();
  }
  
}

void draw_doodle() {
  oam_id = oam_meta_spr(doodlex, doodley, oam_id, playerR);
}

void move_player() {
  byte joy = pad_poll(0);
  if (joy & PAD_LEFT) doodlex -= 2;
  if (joy & PAD_RIGHT) doodlex += 2;
}

// setup PPU and tables
void setup_graphics() {
  ppu_off();
  oam_clear();
  pal_all(PALETTE);
  vram_adr(0x2000);
  vram_fill(0x20, 0x1000);
  vrambuf_clear();
  set_vram_update(updbuf);
  ppu_on_all();
}

void main(void)
{
  setup_graphics();
  // enable rendering
  ppu_on_all();
  oam_off = 0;
  doodlex = 124;
  doodley = SCREEN_Y_BOTTOM-10;
  dy = 0;
  yvel = JUMP_VEL;
  
  //create_platforms();
  //draw_platforms();
  
  // infinite loop
  while(1) {
    //create_platforms();
    //draw_platforms();
    
    oam_id = 0;
    draw_doodle();
    
    //jumping mechanic
    //if (doodley < doodley-10){
      //doodley += yvel/3;
    //}
    yvel -= 1;
    
    if (doodley > SCREEN_Y_BOTTOM) {
      yvel = 0;
      doodley = SCREEN_Y_BOTTOM -10;
    }
    //jumping mechanic
    
    move_player();

    oam_hide_rest(oam_id);
    
    ppu_wait_frame();
  }
}