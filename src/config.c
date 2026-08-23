/* 
 * Pixras default config file
 * Copyright (c) 2026 Mattias Burman <mmburman@hotmail.com>
 * Licensed under the MIT License. See LICENSE-MIT for details.
 */

#include <stdio.h>
#include <math.h>
#include "config.h"
#include "stb_ds.h"

#define KEY_UNDO       KEY_Q
#define KEY_REDO       KEY_W

#define KEY_EXPORT     KEY_S
#define KEY_LINE       KEY_D
#define KEY_RECT       KEY_F
#define KEY_FILL       KEY_A

#define KEY_COLORSELECT KEY_LEFT_CONTROL

// #define CLICKTOSELECTCOLOR

// current default config. right now i wont garantee that i dont push to this file.
// I would want it to be shorter so might 


static Color current_col = BLACK;
static Color col1        = BLACK;
static Color col2        = BLACK;
static Color col3        = BLACK;
static Color col4        = BLACK;
static ivec2 line_orig   = (ivec2){-1,-1};
static ivec2 rect_orig   = (ivec2){-1,-1};
static Image palate_im;
static ivec2 previous_mouse_pos;

typedef struct {
  ivec2 key;
  bool  value;
} pos_entry;

void init() {
  const unsigned int hex_values[] =
    // modified version of https://lospec.com/palette-list/the-y-gigante-reverted pallate by yedamame
    {0x09090eff, 0x1a1c26ff, 0x3c4851ff, 0x617077ff,
     0x99a5a7ff, 0xcbd2d9ff, 0xffffffff, 0x211b21ff,
     0x6b5853ff, 0xab684cff, 0xcea65fff, 0xe7d494ff,
     0xf9f3c0ff, 0x24131dff, 0x402830ff, 0x5e423cff,
     0x806352ff, 0xa19477ff, 0xbdbb93ff, 0x4c0717ff,
     0x810b0bff, 0xa82b12ff, 0xd45c1dff, 0xe38524ff,
     0xebab4cff, 0xf1c256ff, 0xf6dd7aff, 0x03121fff,
     0x0f343fff, 0x1a5556ff, 0x2c7d63ff, 0x4ba245ff,
     0x94cc47ff, 0xeaf257ff, 0x021017ff, 0x0b3b44ff,
     0x17756eff, 0x30a387ff, 0x50cd90ff, 0x6ae291ff,
     0xc9e8a1ff, 0x17092eff, 0x151556ff, 0x113f82ff,
     0x3466b0ff, 0x71b5dbff, 0x9ee4efff, 0xd1fbf0ff,
     0x261646ff, 0x552d72ff, 0x884b93ff, 0xac6ca2ff,
     0xc58faaff, 0xdfb2c6ff, 0xedd1d6ff, 0x140333ff,
     0x461565ff, 0x7b2584ff, 0xa94b84ff, 0xd07482ff,
     0xde9e8cff, 0x7b0d69ff, 0xa41057ff, 0xc3435cff,
     0xe17676ff, 0xf3bfadff, 0x3c133bff, 0x6b2e5aff,
     0xaa557cff, 0xca867aff, 0xf2cdaaff, 0xfaf8dbff,
     0x8a4028ff, 0xb3794dff, 0xdab580ff, 0xf3e7a8ff};
  
  palate_im = gen_palate_image(hex_values, 8*9, 8, 9);
}


void graphics(pix_state state) {
  ClearBackground(BLACK);
  draw_image_background(state, LIGHTGRAY, DARKGRAY);
  pixras_draw_edit_image(state);
  pixras_draw_preview(state);
  if (IsKeyDown(KEY_COLORSELECT)) {
    draw_color_selector(palate_im);
  }
}

/*
  Pixelart lines
  - Symetric
  - same thickness
  - all pixels should intersect the line
  - all 
 */




void process(pix_state* state) {
  center_image(state);

  Vector2 v         = GetMousePosition();
  ivec2   mouse_pos = (ivec2){floor(v.x), floor(v.y)};
  ivec2   im_pos    = get_image_pos(*state, mouse_pos);
  
  if (IsKeyDown(KEY_COLORSELECT)) {
    if (IsKeyPressed(KEY_ONE))   get_color_selected(&col1, palate_im, mouse_pos);
    if (IsKeyPressed(KEY_TWO))   get_color_selected(&col2, palate_im, mouse_pos);
    if (IsKeyPressed(KEY_THREE)) get_color_selected(&col3, palate_im, mouse_pos);
    if (IsKeyPressed(KEY_FOUR))  get_color_selected(&col4, palate_im, mouse_pos);
    return;
  }

  if (IsKeyReleased(KEY_COLORSELECT)) {
    get_color_selected(&current_col, palate_im, mouse_pos);
  }  

  if (IsKeyPressed(KEY_ONE))   current_col = col1;
  if (IsKeyPressed(KEY_TWO))   current_col = col2;
  if (IsKeyPressed(KEY_THREE)) current_col = col3;
  if (IsKeyPressed(KEY_FOUR))  current_col = col4;
  
  if (IsKeyPressed(KEY_UNDO)) pixras_undo(state);
  if (IsKeyPressed(KEY_REDO)) pixras_redo(state);
  
  if (IsKeyPressed(KEY_EXPORT)) export_image(*state);

  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
      IsKeyPressed(KEY_LINE) ||
      IsKeyPressed(KEY_RECT) ||
      IsKeyPressed(KEY_FILL)) {
    copy_image(state);
  }

  Image im = current_image(*state);
  
  if (is_inside_image(*state, im_pos)) {
    if (IsKeyPressed(KEY_FILL)) {
      fill_region(&im, im_pos, current_col, 0);
    }
    
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
      current_col = GetImageColor(im, im_pos.x, im_pos.y);
    
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)){
      ImageDrawPixel(&im, im_pos.x, im_pos.y, current_col);
      ivec2 pre_im_pos = get_image_pos(*state, previous_mouse_pos);
      if (is_inside_image(*state, pre_im_pos)) {
	ImageDrawLine(&im, pre_im_pos.x, pre_im_pos.y, im_pos.x, im_pos.y, current_col);
      }
    }
    
    if (IsKeyPressed(KEY_LINE)) {
      line_orig = im_pos;
    }

    if (IsKeyDown(KEY_LINE)) {
      if (line_orig.x != -1) {
	image_draw_line(&state->preview, line_orig, im_pos, current_col);
      }
    }

    if (IsKeyReleased(KEY_LINE)) {
      if (line_orig.x != -1) {
	image_draw_line(&im, line_orig, im_pos, current_col);
	
	line_orig = (ivec2){-1,-1};
      }
    }

    
    if (IsKeyPressed(KEY_RECT)) {
      rect_orig = im_pos;
    }

    if (IsKeyDown(KEY_RECT)) {
      if (rect_orig.x != -1) {
	image_draw_rect(&state->preview, rect_orig, im_pos, current_col);
      }
    }

    if (IsKeyReleased(KEY_RECT)) {
      if (rect_orig.x != -1) {
	image_draw_rect(&im, rect_orig, im_pos, current_col);
	
	rect_orig = (ivec2){-1,-1};
      }
    }    
  }
  
  previous_mouse_pos = mouse_pos;
}

bool command(pix_state* state, char* cmd) {
  if (strcmp(cmd, "quit") == 0 ||
      strcmp(cmd, "qui")  == 0)
    return true;

  if (sscanf(cmd, "filename %s", state->filename) == 1)
    return false;

  int color_slot;
  int hexcolor;
  if (sscanf(cmd, "color %d %xd", &color_slot, &hexcolor) == 2) {
    Color color;
    if (strlen(cmd) > 8 + 6) {
      color = GetColor(hexcolor);
    } else {
      color = GetColor(hexcolor * 0x100 + 0xff);
    }
    switch (color_slot) {
    case 1: col1 = color; break;
    case 2: col2 = color; break;
    case 3: col3 = color; break;
    case 4: col4 = color; break;
    default : printf("slot does not exist\n");
    }
    return false;
  }
  
  if (cmd[0] == '!') {
    system(cmd + 1);
    return false;
  }
  
  printf("Failed to parse or run command: %s\n", cmd);
  return false;
}

void uninitialize() {
  UnloadImage(palate_im);
}

