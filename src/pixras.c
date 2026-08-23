/*
 * Pixras - A minimal pixel art editor configurable in c.
 * Copywrite (c) 2026 Mattias Burman <mmburman@hotmail.com>
 * Licensed under GPL3 Lincense. See LICENSE for details.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include "raylib.h"

#include "stb_ds.h"
#include "pixras.h"



int block_size(pix_state state) {
  return state.image_width*state.image_height*sizeof(Color);
}

void assert_image_size(pix_state state) {
  if (block_size(state) >= HISTORYALLOC) {
    perror("Image size is larger than pre allocated buffer with size HISTORYALLOC. If you want to load images of this size increase HISTORYALLOC");
    exit(1);
  }
}

void init_empty_image(pix_state* state, cmd_opts opts){
  assert_image_size(*state);
  state->image_width  = opts.width;
  state->image_height = opts.height;
}

void init_loaded_image(pix_state* state, char* filename) {
  Image im = LoadImage(filename);
  state->image_width  = im.width;
  state->image_height = im.height;
  // lets just hope they dont do anything stupid and load bad format
  assert_image_size(*state);
  memcpy(state->history_buf, im.data, block_size(*state));
  UnloadImage(im);
}

Color* current_block(pix_state state) {
  const int b_size    = block_size(state);
  const int used_size = floor(HISTORYALLOC / b_size / sizeof(Color)) * b_size;
  return state.history_buf + (((state.current_gen * state.image_width * state.image_height)
			       % (used_size)));
}

Image current_image(pix_state state) {
  Color* blockid = current_block(state);
  return (Image){blockid, state.image_width, state.image_height, 1,
		 PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}


void copy_image(pix_state* state) {
  Color* old_blockid = current_block(*state);
  state->current_gen += 1;
  state->max_gen = state->current_gen;
  Color* blockid = current_block(*state);
  
  memcpy(blockid, old_blockid, block_size(*state));
}

void parse_size(cmd_opts* opts, char* str) {
  int width;
  int height;

  if (sscanf(str, "%dx%d", &width, &height) <= 1) {
    errno = EIO;
    perror("Failed to parse size argument. Expected argument to be on the form nxm");
    exit(1);
  };

  opts->width  = width;
  opts->height = height;
}

cmd_opts parse_args(int argc, char** argv) {
  cmd_opts opts;  
  memset(&opts, 0, sizeof(opts));
  
  bool create          = false;
  bool previous_size   = false;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] != '-') {
      if (previous_size) {
	parse_size(&opts, argv[i]);
      } else {
	if (opts.filename[0] == 0) {
	  strncpy((char*)&opts, argv[i], MAXFILENAMESIZE);
	} else {
	  errno = EIO;
	  perror("Expected extacly 1 filename");
	  exit(1);
	}
      }
    }
    
    previous_size = false;
    if (strncmp("-c", argv[i], 2) == 0) create        = true;
    if (strncmp("-s", argv[i], 2) == 0) previous_size = true;
  }
  
  if (opts.filename[0] == 0) {
    errno = EIO;
    perror("Expected a filename to be given");
    exit(1);
  }

  if ((opts.width > 0 || opts.height > 0) ^ create) {
    errno = EIO;
    perror("-c should be present if and only if -s is present and given correct size");
    exit(1);
  }

  if (opts.width == 0 || opts.height == 0) {
    opts.load = true;
  }
  
  return opts;
}

void init_image(pix_state* state, cmd_opts opts) {
  if (opts.load) {
    init_loaded_image(state, opts.filename);
  } else {
    init_empty_image(state, opts);
  }
  state->preview = GenImageColor(state->image_width, state->image_height, BLANK);
}

void free_image(pix_state state) {
  free(state.history_buf);
  UnloadImage(state.preview);
}

void internal_draw_image(Image im, float scale, int cam_x, int cam_y) {
  for (int x = 0; x < im.width; x++) {
    for (int y = 0; y < im.height; y++) {
      DrawRectangle(floor(x * scale) + cam_x,
		    floor(y * scale) + cam_y,
		    ceil(scale),
		    ceil(scale),
		    GetImageColor(im, x, y));
    }
  }
}

void pixras_draw_image(pix_state state, Image im) {
  internal_draw_image(im, state.camera_scale, state.camera_pos.x, state.camera_pos.y);
}

void pixras_draw_edit_image(pix_state state) {
  pixras_draw_image(state, current_image(state));
}

void pixras_draw_preview(pix_state state) {
  pixras_draw_image(state, state.preview);
}

ivec2 get_image_pos(pix_state state, ivec2 pos) { // check bounds
  const int x = floor((pos.x - state.camera_pos.x) / state.camera_scale);
  const int y = floor((pos.y - state.camera_pos.y) / state.camera_scale);
  
  return (ivec2){x, y};
}

bool is_inside_image(pix_state state, ivec2 im_pos){
  return (im_pos.x >= 0) && (im_pos.y >= 0) \
      && (im_pos.x < state.image_width)     \
      && (im_pos.y < state.image_height);
}

ivec2 get_screen_pos(pix_state state, ivec2 im_pos) {
  const int x = floor((im_pos.x + state.camera_pos.x) * state.camera_scale);
  const int y = floor((im_pos.y + state.camera_pos.y) * state.camera_scale);

  return (ivec2){x, y};
}

void internal_center_image(float* scale, int* x, int* y, int width, int height) {
  float w = GetScreenWidth()  / width;
  float h = GetScreenHeight() / height;
  if (w < h) {
    *scale = w;
    *x     = 0;
    *y     = floor((GetScreenHeight() - (height * floor(w))) / 2);
  } else {
    *scale = h;
    *x     = floor((GetScreenWidth() - (width * floor(h))) / 2);
    *y     = 0;
  }
}

void center_image(pix_state* state) {
  internal_center_image(&state->camera_scale,
			&state->camera_pos.x, &state->camera_pos.y,
			state->image_width, state->image_height);
}

pix_state init_state() {
  Color* history_buf = (Color*)malloc(HISTORYALLOC);
  memset(history_buf, 0, HISTORYALLOC); // probobly not necessary on every existing operating system.
  if (history_buf == NULL) {
    perror("Couldn't allocate history buffer probobly ran out of memory. Maybe HISTORYALLOC is set too high.");
    exit(1);
  }

  Image preview;
  memset(&preview, 0, sizeof(preview));
  
  return (pix_state){(ivec2){0,0}
                     ,1
		     ,0
		     ,0
		     ,0
		     ,0
		     ,0
		     ,NULL
		     ,preview
		     ,history_buf};
}

void export_image(pix_state state) {
  Image im = current_image(state);
  ExportImage(im, state.filename);
}


void draw_image_background(pix_state state, Color col1, Color col2) {
  for (int x = 0; x < state.image_width; x++) {
    for (int y = 0; y < state.image_height; y++) {
      Color col = col1;
      if ((x + y) % 2) col = col2;
      
      DrawRectangle(floor(x * state.camera_scale) + state.camera_pos.x,
		    floor(y * state.camera_scale) + state.camera_pos.y,
		    ceil(state.camera_scale),
		    ceil(state.camera_scale),
		    col);
    }
  }
}

void pixras_undo(pix_state* state) {
  if (state->current_gen > 0) {
    state->current_gen--;
  }
}

void pixras_redo(pix_state* state) {
  if (state->current_gen < state->max_gen) {
    state->current_gen++;
  }
}

void image_draw_line(Image* im, ivec2 orig, ivec2 dest, Color col) {
  ImageDrawLine(im, orig.x, orig.y, dest.x, dest.y, col);
  ImageDrawPixel(im, dest.x, dest.y, col);
}

void image_draw_rect(Image* im, ivec2 orig, ivec2 dest, Color col) {
  ivec2 pos;
  int width;
  int height;

  pos.x = dest.x;
  width = orig.x - dest.x; 
  if (orig.x < dest.x){
    pos.x = orig.x;
    width = dest.x - orig.x;
  }

  pos.y  = dest.y;
  height = orig.y - dest.y;
  if (orig.y < dest.y) {
    pos.y  = orig.y;
    height = dest.y - orig.y;
  }

  ImageDrawRectangle(im, pos.x, pos.y, width + 1, height + 1, col);
}

Image gen_palate_image(const unsigned int* hex_values, int num_values, int width, int height) {
  Image im = GenImageColor(width, height, BLANK);
  for (int i = 0; i < num_values; i++) {
    int x = i % width;
    int y = floor(i / width);
    
    ImageDrawPixel(&im, x, y, GetColor(hex_values[i]));
  }
  return im;
}

void draw_color_selector(Image im) {
  float scale;
  int   x;
  int   y;
  internal_center_image(&scale, &x, &y, im.width, im.height);
  
  internal_draw_image(im, scale, x, y);
}


bool get_color_selected(Color* res, Image im, ivec2 pos) {
  float scale;
  int   x;
  int   y;
  internal_center_image(&scale, &x, &y, im.width, im.height);

  const int im_x = floor((pos.x - x) / scale);
  const int im_y = floor((pos.y - y) / scale);

  if (im_x < 0 || im_x >= im.width)  return false;
  if (im_y < 0 || im_y >= im.height) return false;

  *res = GetImageColor(im, im_x, im_y);
  return true;
}


typedef struct {
  ivec2 key;
  bool  value;
} pos_entry;

// We use stb hashmaps for the fill algorithm and I think we just need to supress
// a warning from the wierd ass macro that it has. This might be sus i do not
// know perf stuff. Theres like a 50% chance this algo is absolute garbage.
// AI told me this is shit and i should not do this.
#pragma GCC diagnostic ignored "-Wunused-value"
bool in_threshold(Color col, Color colb, float threshold) {
  return (ColorToInt(col) == ColorToInt(colb));
}

bool fill_allowed(Image im, ivec2 pos, Color col, float threshold) {
  if (pos.x < 0 || pos.y < 0 || pos.x >= im.width || pos.y >= im.height) return false;

  return in_threshold(GetImageColor(im, pos.x, pos.y), col, threshold);
}

void fill_update(Image im, pos_entry** active_positions, pos_entry** will_be_active,
		 ivec2 pos, Color col, float threshold) {
  if (fill_allowed(im, (ivec2){pos.x + 1, pos.y}, col, threshold) ||
      fill_allowed(im, (ivec2){pos.x - 1, pos.y}, col, threshold) ||
      fill_allowed(im, (ivec2){pos.x, pos.y + 1}, col, threshold) ||
      fill_allowed(im, (ivec2){pos.x, pos.y - 1}, col, threshold)) {
    return;
  }

  hmdel(*active_positions, pos);
  hmdel(*will_be_active,  pos);
}

void cast_rays(pos_entry** active_positions, pos_entry** will_be_active,
	       Color overwrite_col, ivec2 dir,
	       Image* im, ivec2 pos, Color col, float threshold) {
  pos.x += dir.x;
  pos.y += dir.y;
  while (fill_allowed(*im, pos, overwrite_col, threshold)) {
    ImageDrawPixel(im, pos.x, pos.y, col);
    hmput(*will_be_active, pos, true);

    fill_update(*im, active_positions, will_be_active, (ivec2){pos.x, pos.y},
		overwrite_col, threshold);
    fill_update(*im, active_positions, will_be_active, (ivec2){pos.x + 1, pos.y},
		overwrite_col, threshold);
    fill_update(*im, active_positions, will_be_active, (ivec2){pos.x - 1, pos.y},
		overwrite_col, threshold);
    fill_update(*im, active_positions, will_be_active, (ivec2){pos.x, pos.y + 1},
		overwrite_col, threshold);
    fill_update(*im, active_positions, will_be_active, (ivec2){pos.x, pos.y - 1},
		overwrite_col, threshold);
    
    pos.x += dir.x;
    pos.y += dir.y;
  }
}

void fill_region(Image* im, ivec2 pos, Color col, float threshold) {
  Color overwrite_col = GetImageColor(*im, pos.x, pos.y);
  if (in_threshold(overwrite_col, col, threshold)) return; // might fix potential bug
  
  pos_entry* active_positions = NULL;
  pos_entry* will_be_active   = NULL;
  hmput(active_positions, pos, true);
  int i = 0;
  while (hmlen(active_positions) > 0) {
    ivec2 apos = active_positions->key; // lowkey need to be put down for this

    ivec2 dir = (ivec2){0, 0};
    if (i % 2) {
      dir.x = 1;
    } else {
      dir.y = 1;
    }
    cast_rays(&active_positions, &will_be_active, overwrite_col, dir, im, apos, col, threshold);
    dir.x = -dir.x;
    dir.y = -dir.y;
    cast_rays(&active_positions, &will_be_active, overwrite_col, dir, im, apos, col, threshold);
    
    hmdel(active_positions, apos);
    
    if (hmlen(active_positions) == 0) {
      pos_entry* tmp = active_positions;
      active_positions = will_be_active;
      will_be_active = tmp;
      i++;
    }
  }
  hmfree(active_positions);
  hmfree(will_be_active);
}
#pragma GCC diagnostic pop
