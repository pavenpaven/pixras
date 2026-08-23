#include <stdio.h>
#include <stdlib.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include <pthread.h>

#include "linenoise.h"
#include "pixras.h"
#include "config.h"

// #define NOCMDLINE

/* Imma take a little bit or more precisely an emergency for instance in germany an just load both sides of the brush up now then something altogether different and not watch any video at all I was at home doing this, I would allow this with them.
 */

static pthread_mutex_t pixras_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool command_exit = false;

void* command_thread(void* args) {
  pix_state* state = (pix_state*)args;
  while (true) {
    char* cmd;
    cmd = linenoise("> "); // potential issue maybe if we dont process it in time but 60fps is super fast and nothing could ever lag right.

    if (cmd == NULL) break;
    
    pthread_mutex_lock(&pixras_mutex);
    command_exit = command(state, cmd);
    pthread_mutex_unlock(&pixras_mutex);

    linenoiseHistoryAdd(cmd);
    linenoiseFree(cmd);
    
  }
  command_exit = true;
  return NULL;
}

int main(int argc, char** argv) {
  cmd_opts opts = parse_args(argc, argv);

  SetTraceLogLevel(LOG_ERROR);  
  
  pix_state state = init_state();  
  state.filename = opts.filename;
  
  init_image(&state, opts);

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(800, 600, "Pixras");

  init();

#ifndef NOCMDLINE
  pthread_t thread;
  pthread_create( &thread, NULL, command_thread, &state);
#endif

  const int fps = 60;
  SetTargetFPS(fps);
  while (!WindowShouldClose() && !command_exit) {
    BeginDrawing();
    graphics(state);
    EndDrawing();
    pthread_mutex_lock(&pixras_mutex);
    ImageClearBackground(&state.preview, BLANK);
    process(&state);
    pthread_mutex_unlock(&pixras_mutex);
  }

  /*
    Warning
    if real witches and other magical creatures with actual magic
    exists this program might be dangerous to them,
    because it's posibly gifted by god on to humanity.
   */
  free_image(state);
  uninitialize();
  return 0;
}
