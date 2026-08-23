/*
 * Pixras - A minimal pixel art editor configurable in c.
 * Copywrite (c) 2026 Mattias Burman <mmburman@hotmail.com>
 * Licensed under GPL3 Lincense. See LICENSE for details.
 */

#pragma once

#include "raylib.h"

#define HISTORYALLOC 1000000 // default 1 mB

#define MAXFILENAMESIZE 255

typedef struct {
  int x;
  int y;
} ivec2; // maybe remove this it's maybe not necessary y'know

typedef struct {
  ivec2 camera_pos;
  float camera_scale;
  int image_width;
  int image_height;
  int current_gen;
  int max_gen;
  int min_gen;
  char* filename;
  Image  preview;
  Color* history_buf;
} pix_state;

typedef struct { // needs to work with zii
  char filename[MAXFILENAMESIZE + 1];
  bool load;
  int width;
  int height;
} cmd_opts;

// Usefull in config
Image     current_image(pix_state state);
void      copy_image(pix_state* state);
void      pixras_draw_image(pix_state state, Image im);
void      pixras_draw_edit_image(pix_state state);
void      pixras_draw_preview(pix_state state);
void      draw_image_background(pix_state state, Color col1, Color col2);
void      draw_color_selector(Image im);
void      fill_region(Image* im, ivec2 pos, Color col, float threshold); /*1 - threshold is accepted scalar product of normalized 4 dim color vectors*/
bool      get_color_selected(Color* res, Image im, ivec2 pos);
Image     gen_palate_image(const unsigned int* hex_values, int num_values, int width, int height);
ivec2     get_image_pos(pix_state state, ivec2 pos);
bool      is_inside_image(pix_state state, ivec2 im_pos);
ivec2     get_screen_pos(pix_state state, ivec2 im_pos);
void      center_image(pix_state* state);
void      export_image(pix_state state);
void      pixras_undo(pix_state* state);
void      pixras_redo(pix_state* state);
void      image_draw_line(Image* im, ivec2 orig, ivec2 dest, Color col);
void      image_draw_rect(Image* im, ivec2 orig, ivec2 dest, Color col);

// experimental
unsigned int sdbm_ivec2(ivec2 vec);

/*
void copy_image(pix_state* state, int layer, int frame);
*/

// Mostly in main
cmd_opts  parse_args(int argc, char** argv);
void      init_image(pix_state* state, cmd_opts opts);
void      free_image(pix_state state);
pix_state init_state();


