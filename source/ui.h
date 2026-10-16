#pragma once
#include "game.h"

// Bottom-screen layout (shared by drawing and touch hit-testing)
#define UI_CARD_X    10
#define UI_CARD_W    300
#define UI_CARD_H    52
#define UI_CARD_Y0   32
#define UI_CARD_STEP 62

#define UI_BTN_X     40
#define UI_BTN_W     240
#define UI_BTN_H     36
#define UI_BTN_Y0    60
#define UI_BTN_STEP  52

// item icon grid (HUD and microwave)
#define UI_ITEM_X0   8
#define UI_ITEM_STEP 34
#define UI_ITEM_COLS 9
#define UI_ITEM_Y0_HUD   114
#define UI_ITEM_Y0_MICRO 44

// stage select cards
#define UI_STAGE_W  100
#define UI_STAGE_H  110
#define UI_STAGE_Y  44
#define UI_STAGE_X(i) (6 + (i) * (UI_STAGE_W + 4))

// character select grid (Smash 64 style)
#define UI_CS_COLS  7
#define UI_CS_X0    4
#define UI_CS_Y0    4
#define UI_CS_W     43
#define UI_CS_H     54
#define UI_CS_GAP   2
#define UI_CS_TILE_X(i) (UI_CS_X0 + ((i) % UI_CS_COLS) * (UI_CS_W + UI_CS_GAP))
#define UI_CS_TILE_Y(i) (UI_CS_Y0 + ((i) / UI_CS_COLS) * (UI_CS_H + UI_CS_GAP))

extern const u32 g_rarityColors[RARITY_COUNT];
extern const char* const g_rarityNames[RARITY_COUNT];

bool ui_image(const char* name, float x, float y, float scale, bool centered);
void ui_text(const char* str, float x, float y, float scale, u32 color, u32 flags, bool pixel);
void ui_text_shadow(const char* str, float x, float y, float scale, u32 color, u32 flags, bool pixel);
void ui_describe_choice(const LevelChoice* c, char* title, size_t tn, char* desc, size_t dn);
