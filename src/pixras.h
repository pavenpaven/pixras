/*
 * Pixras - A minimal pixel art editor configurable in c.
 * Copyright (c) 2026 Mattias Burman <mmburman@hotmail.com>
 * Licensed under GPL3 License. See LICENSE for details.
 */

#pragma once

#include "raylib.h"

#define HISTORYALLOC 1000000 // default 1 mB

#define MAXFILENAMESIZE 255
#define MAX_NUM_LAYERS  64
#define MAX_NUM_FRAMES  256

typedef struct {
  int x;
  int y;
} ivec2; // maybe remove this it's maybe not necessary y'know

typedef struct {
  int   is_beginning; // int for explicit padding
  ivec2 changed_image;
} history_header;

typedef struct {
  ivec2 camera_pos;
  float camera_scale;
  int image_width;
  int image_height;
  int current_gen;
  int max_gen;
  int min_gen;
  int num_layers;
  int num_frames;
  char* filename;
  Image  preview;
  Color* image_parts[MAX_NUM_LAYERS * MAX_NUM_FRAMES]; // there might be a cursed as argument to reverse these 
  Color* history_buf;
} pix_state;

typedef struct { // needs to work with zii
  char filename[MAXFILENAMESIZE + 1];
  bool load;
  int width;
  int height;
} cmd_opts;


Image     pixras_get_image(pix_state state, ivec2 pos);
ivec2     pixras_tilemap_size(pix_state state);

void      pixras_draw_image(pix_state state, Image im);
void      pixras_draw_part(pix_state state, ivec2 pos);
void      pixras_draw_preview(pix_state state);
void      draw_image_background(pix_state state, Color col1, Color col2);
void      draw_color_selector(Image im);

ivec2     get_image_pos(pix_state state, ivec2 pos);
bool      is_inside_image(pix_state state, ivec2 im_pos);
ivec2     get_screen_pos(pix_state state, ivec2 im_pos);

Image     gen_palate_image(const unsigned int* hex_values, int num_values, int width, int height);
bool      get_color_selected(Color* res, Image im, ivec2 pos);
void      fill_region(Image* im, ivec2 pos, Color col, float threshold); /* currently threshold is unused for future compatibility should be set to 0*/
void      center_image(pix_state* state);
void      pixras_undo(pix_state* state);
void      pixras_redo(pix_state* state);
void      image_draw_line(Image* im, ivec2 orig, ivec2 dest, Color col);
void      image_draw_rect(Image* im, ivec2 orig, ivec2 dest, Color col);

void      pixras_add_to_history(pix_state* state, ivec2 pos);
void      pixras_add_to_history_compound(pix_state* state, ivec2 pos);

// Exporting
void      export_tilemap(pix_state state, char* filename);
void      export_frames(pix_state state, char* filename);
void      export_frame(pix_state state, char* filename, int x);
void      export_part(pix_state state, char* filename, ivec2 pos);

// Layer and Frame manip
void      swap_rows(pix_state* state, int row1, int row2);
void      swap_cols(pix_state* state, int col1, int col2);
void      add_blank_row(pix_state* state, int row);
void      add_blank_col(pix_state* state, int col);
void      add_copied_col(pix_state* state, int col);
void      remove_row(pix_state* state, int row);
void      remove_col(pix_state* state, int col);

/*
void copy_image(pix_state* state, int layer, int frame);
*/

// Mostly in main
cmd_opts  parse_args(int argc, char** argv);
void      init_image(pix_state* state, cmd_opts opts);
void      free_image(pix_state state);
pix_state init_state();


