#pragma once
#include "game_types.h"

void  terrain_generate(u32 seed);           // for the current stage (same seed = same map)
void  terrain_flat(void);                   // flat ground (menus)
float terrain_height(float x, float z);
float terrain_slope(float x, float z);      // rise over run
bool  terrain_wall(vec3 from, vec3 to);     // moving from -> to would walk into a cliff
void  terrain_draw(void);
