
/*
A platform game with a randomly generated stage.
Uses the Famitone2 library for sound and music.
It scrolls vertically (horizontal mirroring) and nametable
updates are performed offscreen one row at a time with the
draw_floor_line() function.
The enemies use the same movement logic as the player, just
with random inputs.
*/
#include <stdio.h>
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
#define JUMP_VEL 10

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
char doodlep;
bool f = false;
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
  if (doodlep == 1){
  	oam_id = oam_meta_spr(doodlex, doodley, oam_id, playerR);
  }else if (doodlep == 0){
      	oam_id = oam_meta_spr(doodlex, doodley, oam_id, playerL);

  }
}



void gen_platform(int i){
  int td;
  int spot; 

  Platform *p;
  Platform *prev;

  
    td = rand8();
    p = &platforms[i];
    if (td <= 180){
      //x = rndint(1, 2);
      p->draw = 1;
      p->xpos = rndint(3, 26);
    }
    else{
      p->draw = 0;
    }

  spot = i -1;
  while (spot > 0 && spot > i - 5){
    prev = &platforms[spot];
    if(p->xpos >= prev->xpos -3 && p->xpos <= prev->xpos + 3){
      if (p->xpos >4){
        p->xpos -=4;
      }
      else if(p->xpos < 23){
        p->xpos += 4;
      }
    }
    spot --;
    if (i <=0){
      i += 59;
    }
  }
    if (i == 27){
    p->draw = 1;
    p->xpos = 14;
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
    //itoa(platforms[i].xpos,buf,10);
    itoa(s,buf,10);

    
    vrambuf_put(NTADR_A(1,i),buf,4);
    vrambuf_flush();
    //VRAM_WRITE(NTADR_A(0,i),platforms[i].draw);

  }
}


void draw_platform(int i){
  char buf[COLS];
  byte j;
  Platform *p = &platforms[i];
  memset(buf, ' ',COLS);
  for (j = 0; j < COLS; j++){
    if (p->draw == 1 && p->xpos == j){
      buf[j] = 0x83;
      buf[j+1] = 0x84;
      buf[j+2] = 0x84;
      buf[j+3] = 0x85;
      j += 3;
    }
  }
  vrambuf_put(getntaddr(0,i),buf,COLS);
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
    if (platforms[ind].draw && yvel >= 0  &&doodley >= 60&& doodley <= top && doodley >= top - 16 && doodlex >= platforms[ind].xpos*8-8 && doodlex < (platforms[ind].xpos+3)*8){
        return 1;
        }
    i ++;
    p++;
  }
  return 0;
}

void detect_fall(){
  int i;
  char buf[COLS];
  if (f){


    for (i = 0; i < 30; i ++){
      memset(buf, ' ',COLS);
        if (i == 13){
        sprintf(&buf[9], "Game Over :(");
        }if (i == 15){
        sprintf(&buf[10], "Score: "); //enter %d and var name
        }if (i == 17){
        sprintf(&buf[2], "Press down arrow to restart"); //enter %d and var name
        }
    vrambuf_put(getntaddr(0,i),buf,COLS);
    vrambuf_flush();
        s = 0;
    scroll(0, 479 - ((s) % 480));  
      

    }
  
  }
  
  

}

void update_offscreen(){
   int p = 30 -s;
   if (p <0){
     p += 60;
   }
  gen_platform(p);
  draw_platform(p);

}


void move_player() {
  int oldp = curp;
  int i;
  byte joy = pad_poll(0);
  if (joy & PAD_LEFT){
    doodlex -= 3;
    doodlep = 0;
  }
  if (joy & PAD_RIGHT){
    doodlex += 3;
    doodlep = 1;
  }
  doodlex += xvel;
  if (doodley += yvel > 50){
    
  	doodley += yvel;
    
  }else{
    yvel = 10;
    doodley += yvel;
          for (i = s*8; i <  (s+1)*8 ; i++){
        scroll(0, 479 - ((i) % 480));  
        
      }
      s += 1;
      update_offscreen();
      if (s >=60){
        s = 0;
      }

  }

  yvel += 1;
  
      if(doodley < 100 && yvel > 0 ){
      for (i = s*8; i <  (s+1)*8 ; i++){
        scroll(0, 479 - ((i) % 480));  
        doodley += 2;
        
      }
      s += 1;
      update_offscreen();
      if (s >=60){
        s = 0;
      }
      //  doodley += 8;
    }

  //if (yvel >= JUMP_VEL){ //check floors
  if (check_floors_3()){
    yvel = -1 *JUMP_VEL;

   
    
  }
  /*while (doodley ==0){

     for (i = s*8; i <  (s+1)*8 ; i++){
        scroll(0, 479 - ((i) % 480));  
      }
      s += 1;
      if (s >=60){
        s = 0;
      }
    yvel +=1;
    doodley +=5;
    
  }*/
  if (doodley >= 212){
    f = true;
  }
  
  //yvel +=1;
  

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






void draw_platforms(){
  int i;
  int p=0;
  for (i = 0; i < 60; i++){
    draw_platform(i);
    p++;
  }
}

void clear_platforms(){
  int i;
  Platform *p;
  for (i=0; i < ROWS; i++){
     p = &platforms[i];
     p->draw = 0;
     p->xpos = 0;
  }
  s = 0;
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
void detect_reset(){
  byte joy = pad_poll(0);
  if (joy & PAD_DOWN){
    f = false;
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

void gameloop(){
    int s = 0;
    bool reset = false;

  //setup_sounds();		// init famitone library
  create_platforms();

  draw_platforms();
  //print_table();
  spot = 0;
  doodlex = 120;
  doodley = SCREEN_Y_BOTTOM-10;
  doodlep = 0;
  dy = 0;
  yvel = -1 * JUMP_VEL;

  draw_platforms();
  while(!f){
    oam_id = 0;
    draw_doodle();

    move_player();

    oam_hide_rest(oam_id);

    ppu_wait_frame();
  }

  detect_fall();
  while(f){
    detect_reset();
  }
  clear_platforms();

}


// main program
void main() {

    setup_graphics();
    create_platforms();
  while (1) {

    gameloop();
  }
}
