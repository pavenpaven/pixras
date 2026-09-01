/*
 * Pixras - A minimal pixel art editor configurable in c.
 * Copyright (c) 2026 Mattias Burman <mmburman@hotmail.com>
 * Licensed under GPL3 License. See LICENSE for details.
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

#if (RAYLIB_VERSION_MAJOR < 6 || (RAYLIB_VERSION_MAJOR == 6 && RAYLIB_VERSION_MINOR < 1))
void ImageDrawImage(Image *dst, Image src, int posX, int posY, Color tint) {
  Rectangle srcRec = {0, 0, (float)src.width, (float)src.height};
  Rectangle dstRec = {(float)posX, (float)posY, (float)src.width, (float)src.height};
  ImageDraw(dst, src, srcRec, dstRec, tint);
}

/* void ImageDrawImageRec(Image *dst, Image src, */
/* 		       Rectangle srcRec, */
/* 		       Vector2 position, */
/* 		       Color tint) { */
/*   Rectangle dstRec = {position.x, position.y, src.width, src.height}; */
/*   ImageDraw(dst, src, srcRec, dstRec, tint); */
/* } */
#endif

size_t block_size(pix_state state) {
  return (state.image_width*state.image_height)*sizeof(Color) + \
         sizeof(history_header);
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
  state->image_parts[0] = malloc(sizeof(Color)*opts.width*opts.height);
  state->num_layers = 1;
  state->num_frames = 1;
  memset(state->image_parts[0], 0, sizeof(Color)*opts.width*opts.height);
}

void init_loaded_image(pix_state* state, char* filename, int expected_width, int expected_height) {
  // if no width or height is expected they should be 0
  Image im = LoadImage(filename);
  
  state->num_layers = 1;
  state->num_frames = 1;
  
  if (expected_width > 0 && expected_height > 0) {
    if (im.width % expected_width || im.height % expected_height) {
      printf("The image width or height is not divisible by the expected image width or height\n");
      exit(1);
    }
    state->num_layers = (int)(im.height / expected_height);
    state->num_frames = (int)(im.width /  expected_width);
  } else {
    expected_width = im.width;
    expected_height = im.height;
  }

  state->image_width  = expected_width;
  state->image_height = expected_height;
  
  // lets just hope they dont do anything stupid and load bad format
  assert_image_size(*state); // FIXME

  for (int frame = 0; frame < state->num_frames; frame++) {
    for (int layer = 0; layer < state->num_layers; layer++) {
      int index = frame + MAX_NUM_LAYERS * layer;
      state->image_parts[index] = malloc(sizeof(Color)*expected_width*expected_height);
      Image im_part = pixras_get_image(*state, (ivec2){frame, layer});
      for (int x = 0; x < expected_width; x++) {
	for (int y = 0; y < expected_height; y++) {
	  Color col = GetImageColor(im,
				    x + frame * expected_width,
				    y + layer * expected_height);
	  ImageDrawPixel(&im_part, x, y, col);
	}
      }
      if (frame == 0 && layer == 0) {
	state->current_gen = -1; // oops
	pixras_add_to_history(state, (ivec2){frame, layer});
      } else {
	pixras_add_to_history_compound(state, (ivec2){frame, layer});
      }
    }
  }
  UnloadImage(im);
}

void init_image(pix_state* state, cmd_opts opts) {
  if (opts.load) {
    init_loaded_image(state, opts.filename, opts.width, opts.height);
  } else {
    init_empty_image(state, opts);
  }
  state->preview = GenImageColor(state->image_width, state->image_height, BLANK);
}

void free_image(pix_state state) {
  free(state.history_buf);
  UnloadImage(state.preview);
}

Color* current_block(pix_state state) {
  const int b_size    = block_size(state);
  const int used_size = floor(HISTORYALLOC / b_size / sizeof(Color)) * b_size;
  return (Color*)(((void*)state.history_buf) + ((state.current_gen * b_size) % used_size));
}

Image pixras_get_image(pix_state state, ivec2 pos) {
  int index = pos.x + MAX_NUM_LAYERS * pos.y;
  Color* buf = state.image_parts[index];
  return (Image){buf, state.image_width, state.image_height, 1,
		 PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
}

void pixras_add_to_history(pix_state* state, ivec2 pos) {
  Image im = pixras_get_image(*state, pos);
  state->current_gen++; // should add layer here because min_gen or something
  state->max_gen=state->current_gen;

  history_header header = {1, pos};
  
  Color* blockid    = current_block(*state);
  Color* image_data = (Color*)(((void*)blockid) + sizeof(header));
  
  *(history_header*)blockid = header; 
  memcpy(image_data, im.data, block_size(*state) - sizeof(header)); 
}

void pixras_add_to_history_compound(pix_state* state, ivec2 pos) {
  Image im = pixras_get_image(*state, pos);
  state->current_gen += 1; // should add layer here because min_gen or something

  history_header header = {0, pos};
  
  Color* blockid    = current_block(*state);
  Color* image_data = (Color*)(((void*)blockid) + sizeof(header));
  
  *(history_header*)&blockid = header;
  memcpy(image_data, im.data, block_size(*state) - sizeof(header)); 
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

  if ((opts.width == 0 || opts.height == 0) && create) {
    errno = EIO;
    perror("-c presens => -s is present and given correct size");
    exit(1);
  }

  opts.load = !create;
  
  return opts;
}



void internal_draw_image(Image im, float scale, int cam_x, int cam_y) {
  // maybe doesnt need to be internal only
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

void pixras_draw_part(pix_state state, ivec2 pos) {
  pixras_draw_image(state, pixras_get_image(state, pos));
}

void pixras_draw_preview(pix_state state) {
  pixras_draw_image(state, state.preview);
}

ivec2 pixras_tilemap_size(pix_state state) {
  return (ivec2){state.num_frames, state.num_layers};
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
		     ,0
		     ,0
		     ,NULL
		     ,preview
		     ,{0} // hopefully works
		     ,history_buf};
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

bool apply_history(pix_state* state) { // returns true if compound
  Color* block = current_block(*state);
  history_header header = *(history_header*)block;
  Color* image_data = (Color*)(((void*)block) + sizeof(header));
  memcpy(state->image_parts[header.changed_image.x + MAX_NUM_LAYERS * header.changed_image.y],
	 image_data,
	 sizeof(Color)*state->image_width*state->image_height);
  return !header.is_beginning;
}

void pixras_undo(pix_state* state) {
  while (state->current_gen > state->min_gen) {
    state->current_gen--;
    if (!apply_history(state)) break;
  }
}

void pixras_redo(pix_state* state) {
  if (state->current_gen < state->max_gen) {
    state->current_gen++;
    apply_history(state);    
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


void export_tilemap(pix_state state, char* filename) {
  ivec2 tilemap_size = pixras_tilemap_size(state);
  Image im = GenImageColor(state.image_width  * tilemap_size.x,
			   state.image_height * tilemap_size.y,
			   BLANK);
  
  for (int x = 0; x < tilemap_size.x; x++) {
    for (int y = 0; y < tilemap_size.y; y++) {
      ImageDrawImage(&im, pixras_get_image(state, (ivec2){x, y}),
		     state.image_width * x,
		     state.image_height * y,
		     WHITE);
    }
  }
  if (filename == NULL) {
    ExportImage(im, state.filename);
  } else {
    ExportImage(im, filename);
  }
  
  UnloadImage(im);
}


void export_frames(pix_state state, char* filename) {
  ivec2 tilemap_size = pixras_tilemap_size(state);
  Image im = GenImageColor(state.image_width  * tilemap_size.x,
			   state.image_height,
			   BLANK);
  
  for (int x = 0; x < tilemap_size.x; x++) {
    for (int y = 0; y < tilemap_size.y; y++) {
      ImageDrawImage(&im, pixras_get_image(state, (ivec2){x, y}),
		     state.image_width * x,
		     0,
		     WHITE);
    }
  }
  if (filename == NULL) { // is this cursed?
    ExportImage(im, state.filename);
  } else {
    ExportImage(im, filename);
  }
  
  UnloadImage(im);
}


void export_frame(pix_state state, char* filename, int x) {
  ivec2 tilemap_size = pixras_tilemap_size(state);
  Image im = GenImageColor(state.image_width,
			   state.image_height,
			   BLANK);

  for (int y = 0; y < tilemap_size.y; y++) {
    ImageDrawImage(&im, pixras_get_image(state, (ivec2){x, y}),
		   0,
		   0,
		   WHITE);
  }
  if (filename == NULL) { // is this cursed?
    ExportImage(im, state.filename);
  } else {
    ExportImage(im, filename);
  }
  
  UnloadImage(im);
}


void export_part(pix_state state, char* filename, ivec2 pos) {
  Image im = pixras_get_image(state, pos);
  
  if (filename == NULL) { // is this cursed?
    ExportImage(im, state.filename);
  } else {
    ExportImage(im, filename);
  }
}

void swap_cols(pix_state* state, int col1, int col2) {
  for (int y = 0; y < pixras_tilemap_size(*state).y; y++) {
    int index1 = col1 + MAX_NUM_LAYERS * y;
    int index2 = col2 + MAX_NUM_LAYERS * y;
    
    Color* tmp = state->image_parts[index2];
    state->image_parts[index2] = state->image_parts[index1];
    state->image_parts[index1] = tmp;

    if (y == 0) {
      pixras_add_to_history(state, (ivec2){col1, y});
    } else {
      pixras_add_to_history_compound(state, (ivec2){col1, y});
    }
    pixras_add_to_history_compound(state, (ivec2){col2, y});
  } 
}

void swap_rows(pix_state* state, int row1, int row2) {
  for (int x = 0; x < pixras_tilemap_size(*state).x; x++) {
    int index1 = x + MAX_NUM_LAYERS * row1;
    int index2 = x + MAX_NUM_LAYERS * row2;
    
    Color* tmp = state->image_parts[index2];
    state->image_parts[index2] = state->image_parts[index1];
    state->image_parts[index1] = tmp;

    if (x == 0) {
      pixras_add_to_history(state, (ivec2){x, row1});
    } else {
      pixras_add_to_history_compound(state, (ivec2){x, row1});
    }
    pixras_add_to_history_compound(state, (ivec2){x, row2});
  } 
}

void add_blank_row(pix_state* state, int row) {
  ivec2 tilemap_size = pixras_tilemap_size(*state);
  state->num_layers++;
  pixras_add_to_history(state, (ivec2){0,0}); // this is lowkey bad
  for (int y = tilemap_size.y - 1; y >= row; y--) {
    for (int x = 0; x < tilemap_size.x; x++) {
      int index_src = x + MAX_NUM_LAYERS * y;
      int index_dst = x + MAX_NUM_LAYERS * (y + 1);

      state->image_parts[index_dst] = state->image_parts[index_src];
      pixras_add_to_history_compound(state, (ivec2){x, y+1});
    }
  }
  for (int x = 0; x < tilemap_size.x; x++) {
    int index = x + MAX_NUM_LAYERS * row;
    size_t image_size  = sizeof(Color) * state->image_width * state->image_height;
    state->image_parts[index] = (Color*)malloc(image_size);
    memset(state->image_parts[index], 0, image_size); // many mallocs put should be fine
    pixras_add_to_history_compound(state, (ivec2){x, row});
  }
}

void add_blank_col(pix_state* state, int col) {
  ivec2 tilemap_size = pixras_tilemap_size(*state);
  state->num_frames++;
  pixras_add_to_history(state, (ivec2){0,0}); // this is lowkey bad
  for (int x = tilemap_size.x - 1; x >= col; x--) {
    for (int y = 0; y < tilemap_size.y; y++) {
      int index_src = x +       MAX_NUM_LAYERS * y;
      int index_dst = (x + 1) + MAX_NUM_LAYERS * y;

      state->image_parts[index_dst] = state->image_parts[index_src];
      pixras_add_to_history_compound(state, (ivec2){x+1, y});
    }
  }
  for (int y = 0; y < tilemap_size.y; y++) {
    int index = col + MAX_NUM_LAYERS * y;
    size_t image_size  = sizeof(Color) * state->image_width * state->image_height;
    state->image_parts[index] = (Color*)malloc(image_size);
    memset(state->image_parts[index], 0, image_size); // many mallocs put should be fine
    pixras_add_to_history_compound(state, (ivec2){col, y});
  }
}

void add_copied_col(pix_state* state, int col) {
  add_blank_col(state, col);
  if (col > 0) {
    for (int y = 0; y < state->num_layers; y++) {
      int index_dst = col + MAX_NUM_LAYERS * y;
      int index_src = col - 1 + MAX_NUM_LAYERS * y;
      size_t image_size  = sizeof(Color) * state->image_width * state->image_height;
      
      memcpy(state->image_parts[index_dst],
	     state->image_parts[index_src],
	     image_size);
      
      pixras_add_to_history_compound(state, (ivec2){col, y});
    }
  }
}

void remove_row(pix_state* state, int row) {
  ivec2 tilemap_size = pixras_tilemap_size(*state);
  pixras_add_to_history(state, (ivec2){0,0});
  for (int x = 0; x < tilemap_size.x; x++) {
    int index = x + MAX_NUM_LAYERS * row;
    free(state->image_parts[index]);
    
    for (int y = row + 1; y < tilemap_size.y; y++) {
      int index_src = x + MAX_NUM_LAYERS * y;
      int index_dst = x + MAX_NUM_LAYERS * (y - 1);

      state->image_parts[index_dst] = state->image_parts[index_src];
      pixras_add_to_history_compound(state, (ivec2){x, y - 1});
    }
  }
  state->num_layers--;
}

void remove_col(pix_state* state, int col) {
  ivec2 tilemap_size = pixras_tilemap_size(*state);
  pixras_add_to_history(state, (ivec2){0,0});
  for (int y = 0; y < tilemap_size.y; y++) {
    int index = col + MAX_NUM_LAYERS * y;
    free(state->image_parts[index]);
    
    for (int x = col + 1; x < tilemap_size.x; x++) {
      int index_src = x + MAX_NUM_LAYERS * y;
      int index_dst = (x - 1) + MAX_NUM_LAYERS * y;

      state->image_parts[index_dst] = state->image_parts[index_src];
      pixras_add_to_history_compound(state, (ivec2){x - 1, y});
    }
  }
  state->num_frames--;
}
