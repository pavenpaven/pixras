#pragma once
#include "pixras.h"

void init();
void graphics(pix_state state);
void process(pix_state* state);
bool command(pix_state* state, char* cmd);
void uninitialize();
