
/*
A platform game with a randomly generated stage.
Uses the Famitone2 library for sound and music.
It scrolls vertically (horizontal mirroring) and nametable
updates are performed offscreen one row at a time with the
draw_floor_line() function.
The enemies use the same movement logic as the player, just
with random inputs.
*/

#include <stdlib.h>
#include <string.h>

// include NESLIB header
#include "neslib.h"

// include CC65 NES Header (PPU)
#include <nes.h>

// BCD arithmetic support
#include "bcd.h"
//#link "bcd.c"

// VRAM update buffer
#include "vrambuf.h"
//#link "vrambuf.c"

// link the pattern table into CHR ROM
//#link "chr_generic.s"

// famitone2 library
//#link "famitone2.s"
/*{pal:"nes",layout:"nes"}*/


#define COLS 30		// floor width in tiles
#define ROWS 60		// total scrollable height in tiles
#define JUMP_VEL 9

#define MAX_FLOORS 20		// total # of floors in a stage
#define GAPSIZE 4		// gap size in tiles
#define BOTTOM_FLOOR_Y 2	// offset for bottommost floor

#define MAX_ACTORS 8		// max # of moving actors
#define SCREEN_Y_BOTTOM 208	// bottom of screen in pixels
#define ACTOR_MIN_X 16		// leftmost position of actor
#define ACTOR_MAX_X 228		// rightmost position of actor
#define ACTOR_SCROLL_UP_Y 110	// min Y position to scroll up
#define ACTOR_SCROLL_DOWN_Y 140	// max Y position to scroll down
#define JUMP_VELOCITY 32	// Y velocity when jumping

// constants for various tiles
#define CH_BORDER 0x40
#define CH_FLOOR 0xf4
#define CH_LADDER 0xd4
#define CH_ITEM 0xc4
#define CH_BLANK 0x20
#define CH_BASEMENT 0x97

#define DEF_METASPRITE_2x2(name,code,pal)\
const unsigned char name[]={\
        0,      0,      (code)+0,   pal, \
        0,      8,      (code)+1,   pal, \
        8,      0,      (code)+2,   pal, \
        8,      8,      (code)+3,   pal, \
        128};

#define DEF_METASPRITE_2x2_FLIP(name,code,pal)\
const unsigned char name[]={\
        8,      0,      (code)+0,   (pal)|OAM_FLIP_H, \
        8,      8,      (code)+1,   (pal)|OAM_FLIP_H, \
        0,      0,      (code)+2,   (pal)|OAM_FLIP_H, \
        0,      8,      (code)+3,   (pal)|OAM_FLIP_H, \
        128};

DEF_METASPRITE_2x2(playerR, 0xe8, 0);
DEF_METASPRITE_2x2_FLIP(playerL, 0xe8, 0);

const unsigned char* const playerFaceSeq[2] = {
  playerR, playerL,
};

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


typedef struct Platform {
  byte ypos;
  byte xpos;
  byte draw;
} Platform;

Platform platforms[ROWS];

int s;
int spot;
int curp;
char oam_id;
byte doodlex, doodley, dy;
int yvel, xvel;
int doodleplat;
byte doodlep;

// random byte between (a ... b-1)
// use rand() because rand8() has a cycle of 255
byte rndint(byte a, byte b) {
  return (rand() % (b-a)) + a;
}

// return nametable address for tile (x,y)
// assuming vertical scrolling (horiz. mirroring)
word getntaddr(byte x, byte y) {
  
  word addr;
  if (y < 30) {
    addr = NTADR_A(x,y);	// nametable A
  } else {
    addr = NTADR_C(x,y-30);	// nametable C
  }
  return addr;
}
// get Y pixel position for a given floor
int get_floor_yy(byte floor) {
  return platforms[floor].ypos * 8;
}

void draw_doodle() {
  oam_id = oam_meta_spr(doodlex, doodley, oam_id, playerFaceSeq[doodlep]);
}

int check_floors(){
  int i,top;
  int bottom = 0;
  for (i = 0; i < ROWS; i ++){/*
    if (doodley == platforms[i].ypos * 8 == doodley && doodlex >= (platforms[i].xpos*8) && doodlex <((platforms[i].xpos+3)*8+16)) { //need x cond too
      return 1;
    }*/
    top = (i*8+s);//%240;
    if (top >= 480) {
      top = 480 - top;
    }
    //top = i*8;
    //if (doodley >= bottom && doodley <= top && doodlex >= platforms[i].xpos*8 && doodlex < (platforms[i].xpos+3)*8){
    if (yvel >= 0  && doodley <= top && doodley >= top - 12 && doodlex >= platforms[i].xpos*8 && doodlex <= (platforms[i].xpos+2)*8){
      curp = top;
      return 1;
    }
    bottom = top;
  }
  return 0;
}


void move_player() {
  int oldp = curp;
  int i;
  byte joy = pad_poll(0);
  if (joy & PAD_LEFT){
    doodlex -= 4;
    doodlep = 1;
  }
  if (joy & PAD_RIGHT){
    doodlex += 4;
    doodlep = 0;
  }
  doodlex += xvel;
  doodley += yvel;

  yvel += 1;
  //if (yvel >= JUMP_VEL){ //check floors
  if (check_floors()){
    yvel = -1 *JUMP_VEL;
    if(doodley < 120){
      for (i = 0; i <  120-doodley ; i++){
        s += 1;
        scroll(0, 479 - ((s) % 480));  
      }
    }
  }
  //yvel +=1;
}

void gen_platform(int i){
  int td; 

  Platform *p;
    td = rand8();
    p = &platforms[i];
    if (td <= 150){
      //x = rndint(1, 2);
      p->draw = 1;
      p->xpos = rndint(3, 27);
    }
    else{
      p->draw = 0;
    }
}


void create_platforms() {
  byte i;
  byte y = 26;
  for (i=0; i<ROWS; i++) {
    gen_platform(i);
  }
}



void print_table(){
  char buf[30];
  byte i;
  memset(buf, ' ', 30);
  for (i = 0; i < 30; i++){
    itoa(platforms[i].xpos,buf,10);
    
    vrambuf_put(NTADR_A(1,i),buf,4);
    vrambuf_flush();
    //VRAM_WRITE(NTADR_A(0,i),platforms[i].draw);

  }
}


void draw_platform(int i){
  char buf[COLS];
  byte j;
  Platform *p = &platforms[i];
  for (j = 0; j < COLS; j++){
    if (j < p->xpos || j > p->xpos + 2){
      buf[j] = CH_BLANK;
    }
    else if (p->draw == 1){
      buf[j] = 0x83;
      buf[j+1] = 0x84;
      buf[j+2] = 0x85;
      j += 2;
    }
  }
  vrambuf_put(getntaddr(0,i),buf,COLS);
}

void draw(byte i,byte t){
  char buf[COLS];
  byte j;
  Platform *p = &platforms[i];
  for (j = 0; j < COLS; j++){
    if (j < p->xpos || j > p->xpos + 2){
      buf[j] = CH_BLANK;
    }
    else if (p->draw == 1){
      buf[j] = 0x83;
      buf[j+1] = 0x84;
      buf[j+2] = 0x85;
      j += 2;
    }
  }
  vrambuf_put(NTADR_A(1,t),buf,COLS);
}

void scroll_screen(){
  s --;
  s = s % 28;
}

void update_offscreen(int amt, int start){
  int i;
  for (i = 0; i < amt; i ++){
    gen_platform(start + i % ROWS);
  }
}

void draw_platforms(){
  int i;
  int p=0;
  for (i = 0; i < 60; i++){
    draw_platform(i);
    p++;
  }
}

void scroll_demo() {
  int x = 0;   // x scroll position
  int y = 0;   // y scroll position
  int dy = 1;  // y scroll direction
  // infinite loop
  while (1) {
    // wait for next frame
    ppu_wait_frame();
    // update y variable
    y += dy;
    // change direction when hitting either edge of scroll area
    /*if (y >= 479) dy = -1;
    if (y == 0) dy = 1;*/
    //if (y > 479) y = 0;
    // set scroll register
    scroll(0, 479 - ((y + 224) % 480));  }
}

// function to scroll window up and down until end
void scroll_demo2() {
  //int i;
  //scroll(0, amt);
  //for(i = 0; i < amt; i++){
  while(1){
    // wait for next frame
    ppu_wait_frame();
    // update y variable
    scroll(0,-1);
  }
}


// set up PPU
void setup_graphics() {
  ppu_off();
  oam_clear();
  pal_all(PALETTE);
  vram_adr(0x2000);
  vram_fill(CH_BLANK, 0x1000);
  vrambuf_clear();
  set_vram_update(updbuf);
  ppu_on_all();
}


// main program
void main() {
  int s = 0;
  byte joy = pad_poll(0);
  //setup_sounds();		// init famitone library
  setup_graphics();
  create_platforms();
  draw_platforms();
  //print_table();
  spot = 0;
  doodlex = 160;
  doodley = SCREEN_Y_BOTTOM-10;
  doodlep = 0;
  dy = 0;
  yvel = -1 * JUMP_VEL;

  while (1) {
    //setup_graphics();		// setup PPU, clear screen
    //sfx_play(SND_START,0);	// play starting sound
    /*
    vrambuf_put(NTADR_A(2,2), "hello", 5);
    vrambuf_flush();*/
    oam_id = 0;
    draw_doodle();
    
    //jumping mechanic
    //if (doodley < doodley-10){
      //doodley += yvel/3;
    //}
    //yvel -= 1;
    /*
    if (doodley > SCREEN_Y_BOTTOM) {
      yvel = 0;
      doodley = SCREEN_Y_BOTTOM -10;
    }*/
    //jumping mechanic
    //scroll(0,-1);

    move_player();
    


    oam_hide_rest(oam_id);

    ppu_wait_frame();
  
    //scroll_demo();
    //oam_meta_spr_pal(2, 2, 1, 0x0d);

    //make_floors();		// make random level
    //music_play(0);		// start the music
    //play_scene();		// play the level
    //print_table();
  }
}