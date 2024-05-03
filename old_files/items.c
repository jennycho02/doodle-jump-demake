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

#define CHAR(x) ((x)-' ')

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
DEF_METASPRITE_2x2(playerStand, 0xd8, 0);

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

//static byte player_score;


typedef struct Platform {
  byte ypos;
  byte xpos;
  byte draw;
  byte item;
} Platform;

Platform platforms[ROWS];

int s;
int spot;
int curp;
char oam_id;
char item_id;
char score_id;
byte doodlex, doodley, dy;
int yvel, xvel;
int doodleplat;
char doodlep;
int prev_max;
word player_score;

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

void draw_bcd_word(byte col, byte row, word bcd) {
  byte j;
  static char buf[5];
  buf[4] = '0';
  for (j=3; j<0xF0; j--) {
    buf[j] = '0'+(bcd&0x2f);
    bcd >>= 4;
  }
  score_id = oam_spr(row, col, (char) player_score, 1, score_id);
  vrambuf_put(NTADR_A(col, row), buf, 5);
}

/*
void draw_scoreboard() {
  player_score = oam_spr(24+0, 24, '0'+(player_score >> 4), 2, player_score);
  player_score = oam_spr(24+8, 24, '0'+(player_score & 0xf), 2, player_score);
}

void draw_score() {
  vram_adr(NTADR_A(2,2));
  
  vram_write((char) player_score, 13);
}
*/

void add_score(word bcd) {
  player_score = bcd_add(player_score, bcd);
  //draw_scoreboard();
  draw_bcd_word(2, 5, player_score);
}

void draw_doodle() {
  if (doodlep == 1){
  	oam_id = oam_meta_spr(doodlex, doodley, oam_id, playerR);
  }else if (doodlep == 0){
      	oam_id = oam_meta_spr(doodlex, doodley, oam_id, playerL);
  }
}

int check_floors(){
  int i,top;
  int bottom = 0;
  for (i = 0; i < ROWS; i ++){/*
    if (doodley == platforms[i].ypos * 8 == doodley && doodlex >= (platforms[i].xpos*8) && doodlex <((platforms[i].xpos+3)*8+16)) { //need x cond too
      return 1;
    }*/
    top = (i*8-s);//%480;
    if (top >= 480){
      top = 480 - top;
      if (top >= 480){
        top = 480 - top;
      }
    }

    if (platforms[i].draw && top < 240 ){
      //top = (i*8+s)%240;
      //top = i*8;
      //if (doodley >= bottom && doodley <= top && doodlex >= platforms[i].xpos*8 && doodlex < (platforms[i].xpos+3)*8){
      if (yvel >= 0  && doodley <= top && doodley >= top - 16 && doodlex >= platforms[i].xpos*8 && doodlex < (platforms[i].xpos+2)*8){
        curp = top;
        return 1;
      }
      bottom = top;}
  }
  return 0;
}

int check_floors_2(){
  int i,top;
  if (s < 240){
    for (i = 1; i < 28 - (s/8); i ++){
      top = i * 8 - s;
      if (yvel >= 0  && doodley <= top && doodley >= top - 16 && doodlex >= platforms[i].xpos*8 && doodlex < (platforms[i].xpos+2)*8){
        curp = top;
        return 1;
      }
    }
    for (i = 59 - (s/8); i < 60; i ++){
      top = (59 - i) *8+s; 
      if (yvel >= 0  && doodley <= top && doodley >= top - 16 && doodlex >= platforms[i].xpos*8 && doodlex < (platforms[i].xpos+2)*8){
        curp = top;
        return 1;
      }
    }
  }else{
    for (i = 29-((s-240)/8); i < 30; i ++){
      top = (8*i)+s-240;

      if (yvel >= 0  && doodley <= top && doodley >= top - 16 && doodlex >= platforms[i].xpos*8 && doodlex < (platforms[i].xpos+2)*8){
        curp = top;
        return 1;
      }
    }
    for (i = 30; i < 60-((s-240)/8); i ++){
      top = (i - 30)*8 + (240-s);
      if (yvel >= 0  && doodley <= top && doodley >= top - 16 && doodlex >= platforms[i].xpos*8 && doodlex < (platforms[i].xpos+2)*8){
        curp = top;
        return 1;
      }
    }
  }
  return 0;
}

int check_floors_3(){
  int i, end, top,p, ind;
  i = 1 - s;
  if (i  < 0){
    i += 60;
  }
  end = i + 27;
  p = 0;
  while (i < end){
    if (i > 60){
      ind = i - 60;
    }else{
      ind = i;
    }
    top = (p) * 8;
    if (yvel >= 0  && doodley <= top && doodley >= top - 16 && doodlex >= platforms[ind].xpos*8 && doodlex < (platforms[ind].xpos+2)*8){
      return 1;
    }
    i++;
    p++;
  }
  return 0;
}


void move_player() {
  int oldp = curp;
  int i;
  byte joy = pad_poll(0);
  if (joy & PAD_LEFT){
    doodlex -= 4;
    doodlep = 0;
  }
  if (joy & PAD_RIGHT){
    doodlex += 4;
    doodlep = 1;
  }
  doodlex += xvel;
  doodley += yvel;

  yvel += 1;

  if(doodley < 10){
    for (i = s*8; i <  (s+1)*8 ; i++){
      scroll(0, 479 - ((i) % 480));  
    }
    s += 1;
    if (s >=60){
      s = 0;
    }
  }

  //if (yvel >= JUMP_VEL){ //check floors
  if (check_floors_3()){
    yvel = -1 *JUMP_VEL;
  }
  //yvel +=1;
  if (doodley > prev_max) {
    add_score((word) doodley - prev_max);
    prev_max = doodley;
  }
}

void gen_platform(int i){
  int td, rd; 

  Platform *p;
    td = rand8();
    p = &platforms[i];
    if (td <= 150){
      //x = rndint(1, 2);
      rd = rndint(1, 5);
      if (rd==5) {
        p->item = 1;
      }
      p->draw = 1;
      p->xpos = rndint(3, 27);
    }
    else{
      p->draw = 0;
      p->item = 0;
    }
}

void create_platforms() {
  byte i;
  byte y = 26;
  for (i=0; i<ROWS; i++) {
    gen_platform(i);
  }
  platforms[27].xpos = 14;
}

void print_table(){
  char buf[30];
  byte i;
  memset(buf, ' ', 30);
  for (i = 0; i < 30; i++){
    //itoa(platforms[i].xpos,buf,10);
    itoa(s,buf,10);
    vrambuf_put(NTADR_A(1,i),buf,4);
    vrambuf_flush();
    //VRAM_WRITE(NTADR_A(0,i),platforms[i].draw);

  }
}

void draw_item(int x, int y) {
  item_id = oam_spr(x+1*8, y-1*8, 0x18, 2, item_id);
  //0x18 star item
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
      if (p->item == 1) {
        draw_item(p->xpos, p->ypos);
      }
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

const char* RESCUE_TEXT = 
  "GAME OVER\n"
  "Score:";

void check_loss(){
  if (doodley > SCREEN_Y_BOTTOM) {
    oam_clear();
    //pal_clear();
    vrambuf_clear();
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
  //setup_sounds();		// init famitone library
  setup_graphics();
  create_platforms();
  draw_platforms();
  //print_table();
  spot = 0;
  doodlex = 120;
  doodley = SCREEN_Y_BOTTOM-10;
  doodlep = 0;
  dy = 0;
  yvel = -1 * JUMP_VEL;
  player_score = 0;
  prev_max = doodley;


  while (1) {
    //setup_graphics();		// setup PPU, clear screen
    //sfx_play(SND_START,0);	// play starting sound
    /*
    vrambuf_put(NTADR_A(2,2), "hello", 5);
    vrambuf_flush();*/
    oam_id = 0;
    score_id = 1;
    item_id = 2;
    draw_doodle();
     // print_table();

    
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
    //check_loss();
    //print_table();


    //oam_hide_rest(player_score);

    ppu_wait_frame();
  
    //scroll_demo();
    //oam_meta_spr_pal(2, 2, 1, 0x0d);

    //make_floors();		// make random level
    //music_play(0);		// start the music
    //play_scene();		// play the level
  }
}