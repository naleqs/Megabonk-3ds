#include <3ds.h>
#include <citro2d.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "ui.h"
#include "audio.h"
#include "mathutil.h"
#include "save.h"

static C3D_RenderTarget *s_top, *s_bottom;
float g_fps;
static const u32 CLEAR_BOTTOM = 0xFF1E1418; // ABGR
static void draw_loading(void);

static bool touched_rect(touchPosition t, float x, float y, float w, float h)
{
	return t.px >= x && t.px < x + w && t.py >= y && t.py < y + h;
}

static u32 fog_clear_color(void)
{
	const float* fog = STAGE->fog;
	return C2D_Color32((u8)(fog[0] * 255), (u8)(fog[1] * 255), (u8)(fog[2] * 255), 255);
}

// The menus show the last chosen stage's world behind the character.
static void preview_stage(int stage)
{
	g.stage = stage;
	render_set_fog_color(STAGE->fog[0], STAGE->fog[1], STAGE->fog[2]);
	world_generate();
}

static void go_title(void)
{
	int ch = g.character, st = game_first_stage(g.stage);
	memset(&g, 0, sizeof(g));
	game_preload();
	g.character = ch;
	g.stageCursor = st;
	game_select_character(ch);
	g.state = STATE_TITLE;
	preview_stage(st);
	audio_play_music(MUSIC_MENU);
}

static void menu_move(int* cursor, int n, u32 down)
{
	if (down & (KEY_DUP | KEY_CPAD_UP)) { *cursor = (*cursor + n - 1) % n; audio_play_sfx(SFX_MENU); }
	if (down & (KEY_DDOWN | KEY_CPAD_DOWN)) { *cursor = (*cursor + 1) % n; audio_play_sfx(SFX_MENU); }
}

// returns false to quit the app
static bool handle_input(void)
{
	u32 down = hidKeysDown();
	touchPosition t;
	hidTouchRead(&t);
	bool tap = down & KEY_TOUCH;

	switch (g.state) {
	case STATE_TITLE: {
		// [Continue run,] Play, Records, Exit
		bool resume = run_save_exists();
		int n = resume ? 4 : 3;
		menu_move(&g.menuCursor, n, down);
		int pick = -1;
		if (down & KEY_A) pick = g.menuCursor;
		for (int i = 0; i < n && tap; i++)
			if (touched_rect(t, UI_BTN_X, UI_BTN_Y0 + i * UI_BTN_STEP, UI_BTN_W, UI_BTN_H)) pick = i;
		if (pick >= 0 && !resume) pick++;   // same meaning for both layouts
		if (pick == 0) {
			audio_play_sfx(SFX_SELECT);
			draw_loading();
			if (!run_load()) { go_title(); game_toast("Couldn't load the saved run"); }
		}
		if (pick == 1) { g.state = STATE_CHARSELECT; audio_play_sfx(SFX_SELECT); }
		if (pick == 2) { g.state = STATE_RECORDS; audio_play_sfx(SFX_SELECT); }
		if (pick == 3 || (down & KEY_START)) return false;
		break;
	}
	case STATE_RECORDS:
		if (down & (KEY_B | KEY_A) || tap) { g.state = STATE_TITLE; audio_play_sfx(SFX_MENU); }
		break;
	case STATE_CHARSELECT: {
		int c = g.character;
		if (down & (KEY_DRIGHT | KEY_CPAD_RIGHT)) c = (c + 1) % CHARACTER_COUNT;
		if (down & (KEY_DLEFT | KEY_CPAD_LEFT)) c = (c + CHARACTER_COUNT - 1) % CHARACTER_COUNT;
		if (down & (KEY_DDOWN | KEY_CPAD_DOWN)) c = c + UI_CS_COLS < CHARACTER_COUNT ? c + UI_CS_COLS : c % UI_CS_COLS;
		if (down & (KEY_DUP | KEY_CPAD_UP)) {
			c -= UI_CS_COLS;
			if (c < 0) { c += ((CHARACTER_COUNT - 1) / UI_CS_COLS + 1) * UI_CS_COLS; if (c >= CHARACTER_COUNT) c -= UI_CS_COLS; }
		}
		bool start = down & KEY_A;
		for (int i = 0; i < CHARACTER_COUNT && tap; i++)
			if (touched_rect(t, UI_CS_TILE_X(i), UI_CS_TILE_Y(i), UI_CS_W, UI_CS_H)) {
				if (g.character == i) start = true;
				c = i;
			}
		if (c != g.character) {
			g.character = c;
			game_select_character(c);   // load the model now, outside the frame being drawn
			audio_play_sfx(SFX_MENU);
		}
		if (down & KEY_B) g.state = STATE_TITLE;
		if (start) { audio_play_sfx(SFX_SELECT); g.state = STATE_STAGESELECT; g.choiceLock = 0.25f; }
		break;
	}
	case STATE_STAGESELECT: {
		if (g.choiceLock > 0) break;
		int s = g.stageCursor;
		if (down & (KEY_DRIGHT | KEY_CPAD_RIGHT)) s = (s + 1) % STAGE_SELECT_COUNT;
		if (down & (KEY_DLEFT | KEY_CPAD_LEFT)) s = (s + STAGE_SELECT_COUNT - 1) % STAGE_SELECT_COUNT;
		bool start = down & KEY_A;
		for (int i = 0; i < STAGE_SELECT_COUNT && tap; i++)
			if (touched_rect(t, UI_STAGE_X(i), UI_STAGE_Y, UI_STAGE_W, UI_STAGE_H)) {
				if (s == i) start = true;
				s = i;
			}
		if (s != g.stageCursor) {
			g.stageCursor = s;
			preview_stage(s);
			audio_play_sfx(SFX_MENU);
		}
		if (down & KEY_B) g.state = STATE_CHARSELECT;
		if (start) {
			audio_play_sfx(SFX_SELECT);
			run_save_delete();   // starting over replaces a suspended run
			game_new_run(g.character, g.stageCursor);
			profile_run_start();
		}
		break;
	}
	case STATE_PLAYING:
		if (down & KEY_START) { g.state = STATE_PAUSED; g.menuCursor = 0; }
		if (down & KEY_X) interact_use();
#ifdef MB_DEBUG_START_TIME
		// dev builds: ZL kills everything (tests boss/win and kill hooks)
		if (down & KEY_ZL)
			for (int i = 0; i < MAX_ENEMIES; i++)
				if (g.enemies[i].active) game_damage_enemy_ex(i, 1e9f, v3(0, 0, 0), 0, DMG_NO_PROC | DMG_NO_CRIT);
		// dev builds: Y swaps the last weapon slot to the next weapon and hands out 3 random items
		if (down & KEY_Y) {
			static int nextWeapon = WEAPON_SHOTGUN;
			int slot = g.numWeapons < MAX_WEAPON_SLOTS ? g.numWeapons++ : MAX_WEAPON_SLOTS - 1;
			weapons_init_slot(&g.weapons[slot], nextWeapon);
			game_toast(g_weaponDefs[nextWeapon].name);
			nextWeapon = (nextWeapon + 1) % WEAPON_COUNT;
			for (int k = 0; k < 3; k++) items_give(rand() % ITEM_COUNT);
		}
		// dev builds: SELECT hops to the next interactable (for testing)
		if (down & KEY_SELECT) {
			static int nextKind;
			for (int tries = 0; tries < INT_KIND_COUNT; tries++) {
				int kind = nextKind++ % INT_KIND_COUNT;
				int i = 0;
				while (i < g.numInteract && (g.interact[i].kind != kind || !g.interact[i].active || g.interact[i].used)) i++;
				if (i == g.numInteract) continue;
				g.pos = v3_add(g.interact[i].pos, v3(0, 0, -3.5f));
				g.camYaw = 0;
				char msg[48];
				snprintf(msg, sizeof(msg), "[debug] kind %d", kind);
				game_toast(msg);
				break;
			}
		}
#endif
		break;
	case STATE_CHEST:
		if (g.choiceLock > 0) break;
		if (down & KEY_A || tap) { items_give(g.revealItem); g.state = STATE_PLAYING; audio_play_sfx(SFX_SELECT); }
		else if (down & KEY_B) { g.state = STATE_PLAYING; audio_play_sfx(SFX_MENU); }
		break;
	case STATE_SHOP: {
		if (g.choiceLock > 0) break;
		menu_move(&g.shopCursor, 3, down);
		int pick = -1;
		if (down & KEY_A) pick = g.shopCursor;
		for (int i = 0; i < 3 && tap; i++)
			if (touched_rect(t, UI_CARD_X, UI_CARD_Y0 + i * UI_CARD_STEP, UI_CARD_W, UI_CARD_H)) pick = i;
		if (pick >= 0) shop_buy(pick);
		else if (down & KEY_B) g.state = STATE_PLAYING;
		break;
	}
	case STATE_MICROWAVE: {
		if (g.choiceLock > 0) break;
		int n = g.numItemKinds < UI_ITEM_COLS * 3 ? g.numItemKinds : UI_ITEM_COLS * 3;
		int c = g.shopCursor;
		if (down & KEY_DRIGHT) c++;
		if (down & KEY_DLEFT) c--;
		if (down & KEY_DDOWN) c += UI_ITEM_COLS;
		if (down & KEY_DUP) c -= UI_ITEM_COLS;
		if (c != g.shopCursor && c >= 0 && c < n) { g.shopCursor = c; audio_play_sfx(SFX_MENU); }
		int pick = -1;
		if (down & KEY_A) pick = g.shopCursor;
		for (int i = 0; i < n && tap; i++) {
			float x = UI_ITEM_X0 + (i % UI_ITEM_COLS) * UI_ITEM_STEP, y = UI_ITEM_Y0_MICRO + (i / UI_ITEM_COLS) * UI_ITEM_STEP;
			if (touched_rect(t, x, y, 28, 28)) { if (g.shopCursor == i) pick = i; g.shopCursor = i; }
		}
		if (pick >= 0) microwave_use(pick);
		else if (down & KEY_B) g.state = STATE_PLAYING;
		break;
	}
	case STATE_LEVELUP: {
		if (g.choiceLock > 0) break;
		menu_move(&g.choiceCursor, LEVELUP_CHOICES, down);
		int pick = -1;
		if (down & KEY_A) pick = g.choiceCursor;
		for (int i = 0; i < LEVELUP_CHOICES && tap; i++)
			if (touched_rect(t, UI_CARD_X, UI_CARD_Y0 + i * UI_CARD_STEP, UI_CARD_W, UI_CARD_H)) pick = i;
		if ((down & KEY_Y) && g.rerolls > 0) { g.rerolls--; game_roll_choices(); audio_play_sfx(SFX_MENU); }
		if (pick >= 0) {
			game_apply_choice(&g.choices[pick]);
			g.state = STATE_PLAYING;
			audio_play_sfx(SFX_SELECT);
		}
		break;
	}
	case STATE_PAUSED: {
		// Resume, Save & quit, Quit run
		menu_move(&g.menuCursor, 3, down);
		int pick = -1;
		if (down & KEY_A) pick = g.menuCursor;
		if (down & (KEY_START | KEY_B)) pick = 0;
		for (int i = 0; i < 3 && tap; i++)
			if (touched_rect(t, UI_BTN_X, UI_BTN_Y0 + i * UI_BTN_STEP, UI_BTN_W, UI_BTN_H)) pick = i;
		if (pick == 0) g.state = STATE_PLAYING;
		if (pick == 1) {
			if (run_save()) { go_title(); game_toast("Run saved"); }
			else game_toast("Couldn't write to the SD card");
		}
		if (pick == 2) { profile_run_end(false); go_title(); }
		break;
	}
	case STATE_WIN:
		if (g.choiceLock > 0) break;
		if (STAGE->next >= 0) {
			// stage cleared: A / top button continues the run, B / bottom button ends it
			int pick = -1;
			if (down & KEY_A) pick = 0;
			if (down & KEY_B) pick = 1;
			for (int i = 0; i < 2 && tap; i++)
				if (touched_rect(t, UI_BTN_X, UI_BTN_Y0 + 60 + i * UI_BTN_STEP, UI_BTN_W, UI_BTN_H)) pick = i;
			if (pick == 0) { audio_play_sfx(SFX_SELECT); game_next_stage(); }
			if (pick == 1) { profile_run_end(false); go_title(); }
			break;
		}
		if (down & (KEY_A | KEY_START) || tap) go_title();
		break;
	case STATE_DEAD:
		if (down & (KEY_A | KEY_START) || tap) go_title();
		break;
	}
	return true;
}

static void draw_loading(void)
{
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C2D_TargetClear(s_top, 0xFF000000);
	C2D_TargetClear(s_bottom, CLEAR_BOTTOM);
	C2D_SceneBegin(s_top);
	ui_text("Loading...", 200, 110, 1.0f, C2D_Color32(255, 255, 255, 255), C2D_AlignCenter, true);
	C3D_FrameEnd(0);
}

int main(int argc, char* argv[])
{
	romfsInit();
	gfxInitDefault();
	osSetSpeedupEnable(true); // 804MHz on New 3DS
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 4);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 2);
	C2D_Prepare();
	s_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	s_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	render_init();
	hud_init();
	draw_loading();
	save_init();
	audio_init();
	srand(osGetTime());
	go_title();

	u64 last = osGetTime();
	while (aptMainLoop()) {
		hidScanInput();
		game_poll_character();   // finish background model loads (must be outside the frame)
		u64 now = osGetTime();
		float dt = (now - last) / 1000.0f;
		last = now;
		if (dt > 0.0001f) g_fps = g_fps * 0.95f + (1.0f / dt) * 0.05f;
		if (dt > 0.05f) dt = 0.05f;

		if (!handle_input()) break;
		game_update(dt);
		audio_update();

		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(s_top, fog_clear_color());
		C2D_TargetClear(s_bottom, CLEAR_BOTTOM);
		draw_scene(s_top);

		// 2D overlay: citro2d resets most state; turn off our fog and depth test
		C2D_Prepare();
		C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
		C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
		C2D_SceneBegin(s_top);
		draw_hud_top();
		C2D_SceneBegin(s_bottom);
		draw_bottom();
		C3D_FrameEnd(0);
	}

	audio_exit();
	hud_exit();
	render_exit();
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
	return 0;
}
