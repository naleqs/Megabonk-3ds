#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "mathutil.h"
#include "ui.h"
#include "audio.h"

static C2D_TextBuf s_textBuf;
static C2D_Font s_font;
static vec3 s_camPos, s_camFwd;

static const Color NO_EMIT = { 0, 0, 0, 0 };

const u32 g_rarityColors[RARITY_COUNT] = {
	0xFFC8C8C8, 0xFF50D25A, 0xFFF0A03C, 0xFFE650B4, 0xFF28C8FF, // ABGR
};
const char* const g_rarityNames[RARITY_COUNT] = { "Common", "Uncommon", "Rare", "Epic", "Legendary" };

#define WHITE C2D_Color32(255, 255, 255, 255)
#define DIM   C2D_Color32(170, 170, 190, 255)
#define HINT  C2D_Color32(140, 140, 160, 255)
#define GOLD  C2D_Color32(255, 220, 90, 255)
#define PANEL C2D_Color32(30, 28, 42, 255)
#define PANEL_SEL C2D_Color32(55, 50, 75, 255)

void hud_init(void)
{
	s_textBuf = C2D_TextBufNew(8192);
	s_font = C2D_FontLoad("romfs:/font/alagard.bcfnt");
}

void hud_exit(void)
{
	C2D_TextBufDelete(s_textBuf);
	if (s_font) C2D_FontFree(s_font);
}

void ui_text(const char* str, float x, float y, float scale, u32 color, u32 flags, bool pixel)
{
	C2D_Text t;
	C2D_TextFontParse(&t, pixel ? s_font : NULL, s_textBuf, str);
	C2D_TextOptimize(&t);
	C2D_DrawText(&t, C2D_WithColor | flags, x, y, 0.5f, scale, scale, color);
}

void ui_text_shadow(const char* str, float x, float y, float scale, u32 color, u32 flags, bool pixel)
{
	C2D_Text t;
	C2D_TextFontParse(&t, pixel ? s_font : NULL, s_textBuf, str);
	C2D_TextOptimize(&t);
	C2D_DrawText(&t, C2D_WithColor | flags, x + 1.5f, y + 1.5f, 0.5f, scale, scale, C2D_Color32(0, 0, 0, 200));
	C2D_DrawText(&t, C2D_WithColor | flags, x, y, 0.5f, scale, scale, color);
}

// wrapped text (citro2d word-wraps within maxWidth)
static void ui_text_wrap(const char* str, float x, float y, float scale, u32 color, float maxWidth)
{
	C2D_Text t;
	C2D_TextParse(&t, s_textBuf, str);
	C2D_TextOptimize(&t);
	C2D_DrawText(&t, C2D_WithColor | C2D_WordWrap, x, y, 0.5f, scale, scale, color, maxWidth);
}

// Draw a romfs texture as a 2D image; returns false if missing.
bool ui_image(const char* name, float x, float y, float scale, bool centered)
{
	int uw, uh;
	C3D_Tex* tex = texture_get_sized(name, &uw, &uh);
	if (tex->width <= 8) return false;
	Tex3DS_SubTexture sub = {
		.width = uw, .height = uh,
		.left = 0, .top = 1.0f,
		.right = (float)uw / tex->width, .bottom = 1.0f - (float)uh / tex->height,
	};
	C2D_Image img = { tex, &sub };
	if (centered) { x -= uw * scale * 0.5f; y -= uh * scale * 0.5f; }
	C2D_DrawImageAt(img, x, y, 0.5f, NULL, scale, scale);
	return true;
}

void ui_bar(float x, float y, float w, float h, float frac, u32 fill, u32 back)
{
	frac = clampf(frac, 0, 1);
	C2D_DrawRectSolid(x - 1, y - 1, 0.5f, w + 2, h + 2, C2D_Color32(0, 0, 0, 220));
	C2D_DrawRectSolid(x, y, 0.5f, w, h, back);
	C2D_DrawRectSolid(x, y, 0.5f, w * frac, h, fill);
}

void ui_panel(float x, float y, float w, float h, u32 border, u32 fill)
{
	C2D_DrawRectSolid(x - 2, y - 2, 0.5f, w + 4, h + 4, border);
	C2D_DrawRectSolid(x, y, 0.5f, w, h, fill);
}

// 32x32 icon in a framed slot, with an optional count/level badge
static void ui_slot(const char* icon, float x, float y, float scale, u32 border, int badge)
{
	float s = 32 * scale;
	ui_panel(x, y, s, s, border, C2D_Color32(20, 18, 30, 255));
	if (icon) ui_image(icon, x, y, scale, false);
	if (badge > 1) {
		char b[12];
		snprintf(b, sizeof(b), "%d", badge);
		ui_text_shadow(b, x + s - 1, y + s - 11, 0.4f, WHITE, C2D_AlignRight, false);
	}
}

// ---------------------------------------------------------------- 3D scene

static void draw_shadow(vec3 pos, float r)
{
	C3D_Mtx m;
	Mtx_Identity(&m);
	Mtx_Translate(&m, pos.x, 0.05f, pos.z, true);
	Mtx_Scale(&m, r, 0.01f, r);
	render_mesh(&g_meshSphere, 0, &m, (Color){ 0, 0, 0, 0.35f }, NO_EMIT);
}

static void draw_enemies(void)
{
	float maxd2 = 95 * 95;
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Enemy* e = &g.enemies[i];
		if (!e->active) continue;
		float dx = e->pos.x - s_camPos.x, dz = e->pos.z - s_camPos.z;
		if (dx * dx + dz * dz > maxd2) continue;
		const EnemyDef* d = &g_enemyDefs[e->def];
		Mesh* m = g.enemyMeshes[e->def];
		if (dx * s_camFwd.x + dz * s_camFwd.z < -m->radius * e->scale - 2) continue;
		Color tint = { d->tint[0], d->tint[1], d->tint[2], 1 };
		if (e->elite) { tint.r *= 1.2f; tint.g *= 1.0f; tint.b *= 0.4f; }
		if (e->freezeTime > 0) { tint.r *= 0.5f; tint.g *= 0.8f; tint.b = 1.3f; }
		else if (e->poisonTime > 0) { tint.r *= 0.6f; tint.b *= 0.5f; }
		Color emit = e->hitFlash > 0 ? (Color){ 0.9f, 0.9f, 0.9f, 0 } : NO_EMIT;
		// bosses easily fill the screen: see through them when they block the player
		float r = d->radius * e->scale;
		if (r > 3.0f && seg_dist2_xz(s_camPos, g.pos, e->pos) < r * r) {
			tint.a = 0.35f;
			render_set_depth_write(false);
			render_mesh_anim_at(m, 0, e->animTime, e->pos, e->yaw, e->scale, tint, emit);
			render_set_depth_write(true);
			continue;
		}
		render_mesh_anim_at(m, 0, e->animTime, e->pos, e->yaw, e->scale, tint, emit);
	}
}

static void draw_player(void)
{
	Mesh* mesh = g.charMesh;
	float hspeed = sqrtf(g.vel.x * g.vel.x + g.vel.z * g.vel.z);
	int idle = mesh_find_anim(mesh, "idle"), run = mesh_find_anim(mesh, "run"), jump = mesh_find_anim(mesh, "jump");
	int anim = idle;
	float t = g.animTime;
	if (!g.grounded && jump >= 0) { anim = jump; t = g.jumpTime; }
	else if (hspeed > 1.5f && run >= 0) anim = run;

	// characters without a run clip still get a little bob
	float bob = (g.grounded && run < 0) ? fabsf(sinf(g.moveAnim)) * 0.3f : 0;
	vec3 p = v3_add(g.pos, v3(0, bob, 0));
	Color emit = g.hurtFlash > 0 ? (Color){ 0.8f, 0.1f, 0.1f, 0 } : NO_EMIT;
	if (g.aegisCharges > 0) emit = (Color){ emit.r + 0.12f, emit.g + 0.1f, emit.b + 0.02f, 0 };   // shielded glow
	C3D_Mtx m;
	Mtx_Identity(&m);
	Mtx_Translate(&m, p.x, p.y, p.z, true);
	Mtx_RotateY(&m, g.yaw, true);
	render_mesh_anim(mesh, anim, t, &m, COLOR(1, 1, 1), emit);
}

static void draw_pickups(void)
{
	for (int i = 0; i < MAX_PICKUPS; i++) {
		Pickup* p = &g.pickups[i];
		if (!p->active) continue;
		if (dist2_xz(p->pos, s_camPos) > 80 * 80) continue;
		vec3 pos = v3_add(p->pos, v3(0, sinf(p->bob) * 0.2f, 0));
		switch (p->kind) {
		case PICKUP_XP: {
			Color c = p->value >= 10 ? COLOR(0.7f, 0.3f, 1.0f) : p->value >= 3 ? COLOR(0.3f, 1.0f, 0.4f) : COLOR(0.3f, 0.75f, 1.0f);
			float s = p->value >= 10 ? 0.5f : 0.32f;
			render_mesh_at(&g_meshGem, 0, pos, p->bob, s, c, (Color){ c.r * 0.5f, c.g * 0.5f, c.b * 0.5f, 0 });
			break;
		}
		case PICKUP_GOLD: {
			C3D_Mtx m;
			Mtx_Identity(&m);
			Mtx_Translate(&m, pos.x, pos.y, pos.z, true);
			Mtx_RotateY(&m, p->bob, true);
			Mtx_RotateX(&m, 1.2f, true);  // stand the coin pile up so it reads from the camera
			Mtx_Scale(&m, 0.22f, 0.22f, 0.22f);
			render_mesh(g.meshCoins, 0, &m, COLOR(1, 1, 1), (Color){ 0.35f, 0.3f, 0.05f, 0 });
			break;
		}
		case PICKUP_HEART:
			render_mesh_at(&g_meshSphere, 0, pos, 0, 0.45f, COLOR(1, 0.15f, 0.2f), (Color){ 0.4f, 0, 0, 0 });
			break;
		case PICKUP_MAGNET:
			render_mesh_at(&g_meshSphere, 0, pos, 0, 0.5f, COLOR(0.3f, 0.4f, 1), (Color){ 0.2f, 0.2f, 0.5f, 0 });
			break;
		case PICKUP_CHEST:
			draw_shadow(p->pos, 1.2f);
			render_mesh_at(g.meshChest, 0, v3(p->pos.x, 0, p->pos.z), p->bob * 0.3f, 0.8f, COLOR(1, 1, 1), (Color){ 0.15f, 0.12f, 0, 0 });
			break;
		}
	}
}

static void draw_projectiles(void)
{
	for (int i = 0; i < MAX_PROJECTILES; i++) {
		Projectile* p = &g.projectiles[i];
		if (!p->active) continue;
		float yaw = yaw_of(p->vel);
		C3D_Mtx m;
		Mtx_Identity(&m);
		Mtx_Translate(&m, p->pos.x, p->pos.y, p->pos.z, true);
		switch (p->kind) {
		case PROJ_FIREBALL:
			render_mesh_at(&g_meshSphere, 0, p->pos, 0, 0.55f * p->size, COLOR(1, 0.45f, 0.1f), (Color){ 1, 0.5f, 0.1f, 0 });
			break;
		case PROJ_BULLET:
		case PROJ_PELLET:
			render_mesh_at(&g_meshSphere, 0, p->pos, 0, 0.22f, COLOR(1, 0.9f, 0.4f), (Color){ 1, 0.9f, 0.3f, 0 });
			break;
		case PROJ_WAVE:
			Mtx_RotateY(&m, yaw, true);
			Mtx_RotateZ(&m, (float)M_PI / 2, true);
			Mtx_Scale(&m, 1.1f * p->size, 1.1f * p->size, 1.1f * p->size);
			render_mesh(g.meshSword, 0, &m, (Color){ 1, 0.95f, 0.6f, 0.8f }, (Color){ 0.6f, 0.55f, 0.2f, 0 });
			break;
		case PROJ_SOUL:
			render_mesh_at(&g_meshSphere, 0, p->pos, 0, 0.35f, COLOR(0.7f, 0.9f, 1), (Color){ 0.6f, 0.8f, 1, 0 });
			break;
		case PROJ_ORB:
			render_mesh_at(&g_meshSphere, 0, p->pos, 0, 0.45f, COLOR(0.3f, 1, 0.9f), (Color){ 0.2f, 0.9f, 0.8f, 0 });
			break;
		case PROJ_ROCKET:
		case PROJ_MISSILE:
		case PROJ_SPIKE: {
			// cone primitive points along +Z: aim it along the velocity
			vec3 v = v3_norm(p->vel);
			Mtx_RotateY(&m, yaw, true);
			Mtx_RotateX(&m, -asinf(clampf(v.y, -1, 1)), true);
			float s = p->kind == PROJ_SPIKE ? 0.25f : 0.45f;
			Mtx_Scale(&m, s, s, s * 3);
			Color c = p->kind == PROJ_ROCKET ? COLOR(1, 0.5f, 0.2f) : p->kind == PROJ_MISSILE ? COLOR(1, 0.45f, 0.75f)
			        : COLOR(0.4f, 0.8f, 0.3f);
			render_mesh(&g_meshCone, 0, &m, c, (Color){ c.r * 0.5f, c.g * 0.5f, c.b * 0.5f, 0 });
			break;
		}
		case PROJ_BONE:
			Mtx_RotateY(&m, p->spin, true);
			Mtx_Scale(&m, 0.3f * p->size, 0.3f * p->size, 0.3f * p->size);
			render_mesh(g.meshBone, 0, &m, COLOR(1, 1, 1), NO_EMIT);
			break;
		case PROJ_BANANA:
			Mtx_RotateY(&m, p->spin * 1.5f, true);
			Mtx_Scale(&m, 0.9f * p->size, 0.9f * p->size, 0.9f * p->size);
			render_mesh(g.meshBanana, 0, &m, COLOR(1, 1, 1), (Color){ 0.15f, 0.15f, 0, 0 });
			break;
		case PROJ_ARROW:
			Mtx_RotateY(&m, yaw - (float)M_PI / 2, true);   // arrow mesh lies along X
			Mtx_Scale(&m, 0.45f, 0.45f, 0.45f);
			render_mesh(g.meshArrow, 0, &m, COLOR(1, 1, 1), NO_EMIT);
			break;
		case PROJ_DAGGER:
			Mtx_RotateY(&m, yaw - (float)M_PI / 2, true);
			Mtx_Scale(&m, 0.9f, 0.9f, 0.9f);
			render_mesh(g.meshDagger, 0, &m, COLOR(1, 1, 1), (Color){ 0.1f, 0.2f, 0.4f, 0 });
			break;
		case PROJ_FLASK:
			Mtx_RotateX(&m, p->spin, true);
			Mtx_Scale(&m, 1.3f, 1.3f, 1.3f);
			render_mesh(g.meshFlask, 0, &m, COLOR(1, 1, 1), (Color){ 0, 0.2f, 0, 0 });
			break;
		case PROJ_BLADE:
			Mtx_RotateY(&m, p->spin, true);
			Mtx_RotateZ(&m, (float)M_PI / 2, true);
			Mtx_Scale(&m, 0.8f * p->size, 0.8f * p->size, 0.8f * p->size);
			render_mesh(g.meshSword, 0, &m, COLOR(0.8f, 0.8f, 1), (Color){ 0.2f, 0.2f, 0.3f, 0 });
			break;
		case PROJ_SNIPE:
			render_mesh_at(&g_meshSphere, 0, p->pos, 0, 0.3f, COLOR(1, 1, 0.6f), (Color){ 1, 1, 0.5f, 0 });
			break;
		case PROJ_HOLE:
			render_mesh_at(&g_meshSphere, 0, p->pos, 0, 0.8f * p->size, COLOR(0.15f, 0, 0.25f), (Color){ 0.25f, 0, 0.4f, 0 });
			break;
		case PROJ_DICE:
			Mtx_RotateY(&m, p->spin, true);
			Mtx_RotateX(&m, p->spin * 0.7f, true);
			Mtx_Scale(&m, 1.4f, 1.4f, 1.4f);
			render_mesh(g.meshDice, 0, &m, COLOR(1, 1, 1), NO_EMIT);
			break;
		case PROJ_AXE:
			Mtx_RotateY(&m, yaw, true);
			Mtx_RotateX(&m, p->spin, true);
			Mtx_Scale(&m, 0.7f * p->size, 0.7f * p->size, 0.7f * p->size);
			render_mesh(g.meshAxe, 0, &m, COLOR(1, 1, 1), NO_EMIT);
			break;
		}
	}
}

static void draw_fx(void)
{
	render_set_depth_write(false);
	for (int i = 0; i < MAX_FX; i++) {
		Fx* f = &g.fx[i];
		if (!f->active) continue;
		float t = f->age / f->life;
		Color c = f->color;
		c.a = 1.0f - t;
		Color emit = { c.r * 0.6f, c.g * 0.6f, c.b * 0.6f, 0 };
		C3D_Mtx m;
		switch (f->kind) {
		case FX_EXPLOSION:
			render_mesh_at(&g_meshSphere, 0, f->pos, 0, f->size * (0.4f + 0.6f * t), c, emit);
			break;
		case FX_POISON:
			c.a = 0.4f * (1 - t);
			render_mesh_at(&g_meshSphere, 0, v3_add(f->pos, v3(0, 0.5f, 0)), 0, f->size, c, emit);
			break;
		case FX_DEATH:
			c.a = 0.6f * (1 - t);
			render_mesh_at(&g_meshSphere, 0, v3_add(f->pos, v3(0, 1, 0)), 0, f->size * (0.3f + 0.7f * t), c, emit);
			break;
		case FX_SPAWN:
		case FX_RING:
		case FX_LEVELUP:
			render_mesh_at(&g_meshRing, 0, v3_add(f->pos, v3(0, 0.15f, 0)), 0, f->size * (0.3f + t), c, emit);
			break;
		case FX_LIGHTNING:
			Mtx_Identity(&m);
			Mtx_Translate(&m, f->pos.x, 0, f->pos.z, true);
			Mtx_Scale(&m, 0.35f + 0.2f * (1 - t), 40, 0.35f + 0.2f * (1 - t));
			render_mesh(&g_meshCylinder, 0, &m, c, (Color){ 0.6f, 0.9f, 1, 0 });
			render_mesh_at(&g_meshRing, 0, v3_add(f->pos, v3(0, 0.15f, 0)), 0, f->size, c, emit);
			break;
		case FX_BEAM: {
			// a wobbly noodle: a row of blobs from pos along yaw
			vec3 d = dir_of_yaw(f->yaw);
			int n = (int)(f->size / 0.9f) + 1;
			if (n > 24) n = 24;
			for (int k = 0; k <= n; k++) {
				float u = (float)k / n;
				float w = sinf(u * 9.0f + g.realTime * 14.0f) * 0.35f * (1 - u) * u * 4;
				vec3 at = v3(f->pos.x + d.x * f->size * u + d.z * w, f->pos.y - u * 1.0f, f->pos.z + d.z * f->size * u - d.x * w);
				render_mesh_at(&g_meshSphere, 0, at, 0, 0.32f, c, emit);
			}
			break;
		}
		case FX_SLASH:
		case FX_SPIN: {
			float a = f->kind == FX_SPIN ? f->yaw + t * 6.2832f : f->yaw + lerpf(-1.2f, 1.2f, t);
			Mtx_Identity(&m);
			Mtx_Translate(&m, g.pos.x, g.pos.y + 1.8f, g.pos.z, true);
			Mtx_RotateY(&m, a, true);
			Mtx_RotateX(&m, (float)M_PI / 2, true);
			float s = f->size / 2.2f;
			Mtx_Scale(&m, s, s, s);
			render_mesh(g.meshSword, 0, &m, (Color){ f->color.r, f->color.g, f->color.b, 1 },
			            (Color){ f->color.r * 0.3f, f->color.g * 0.3f, f->color.b * 0.3f, 0 });
			c.a = 0.35f * (1 - t);
			render_mesh_at(&g_meshRing, 0, v3_add(g.pos, v3(0, 0.2f, 0)), 0, f->size, c, emit);
			break;
		}
		}
	}

	// weapon zones
	for (int i = 0; i < MAX_ZONES; i++) {
		Zone* z = &g.zones[i];
		if (!z->active) continue;
		float fade = clampf((z->life - z->age) * 2, 0, 1);
		C3D_Mtx m;
		switch (z->kind) {
		case ZONE_POISON:
			render_mesh_at(&g_meshRing, 0, v3_add(z->pos, v3(0, 0.15f, 0)), g.realTime, z->radius,
			               (Color){ 0.3f, 0.9f, 0.2f, 0.5f * fade }, (Color){ 0.1f, 0.4f, 0.05f, 0 });
			Mtx_Identity(&m);
			Mtx_Translate(&m, z->pos.x, 0.1f, z->pos.z, true);
			Mtx_Scale(&m, z->radius, 0.15f, z->radius);
			render_mesh(&g_meshSphere, 0, &m, (Color){ 0.3f, 0.8f, 0.2f, 0.3f * fade }, (Color){ 0.1f, 0.3f, 0.05f, 0 });
			break;
		case ZONE_TORNADO:
			Mtx_Identity(&m);
			Mtx_Translate(&m, z->pos.x, 0, z->pos.z, true);
			Mtx_RotateY(&m, g.realTime * 12, true);
			Mtx_Scale(&m, z->radius, 6.0f, z->radius);
			render_mesh(&g_meshCylinder, 0, &m, (Color){ 0.85f, 0.9f, 0.95f, 0.35f * fade }, (Color){ 0.3f, 0.3f, 0.3f, 0 });
			Mtx_Identity(&m);
			Mtx_Translate(&m, z->pos.x, 3, z->pos.z, true);
			Mtx_RotateY(&m, -g.realTime * 9, true);
			Mtx_Scale(&m, z->radius * 1.5f, 4.0f, z->radius * 1.5f);
			render_mesh(&g_meshCylinder, 0, &m, (Color){ 0.85f, 0.9f, 0.95f, 0.25f * fade }, (Color){ 0.3f, 0.3f, 0.3f, 0 });
			break;
		case ZONE_FIRE: {
			float flick = 0.85f + 0.15f * sinf(g.realTime * 20 + i);
			Mtx_Identity(&m);
			Mtx_Translate(&m, z->pos.x, 0.1f, z->pos.z, true);
			Mtx_Scale(&m, z->radius * flick, 0.5f * flick, z->radius * flick);
			render_mesh(&g_meshSphere, 0, &m, (Color){ 1, 0.45f, 0.08f, 0.55f * fade }, (Color){ 0.9f, 0.35f, 0.05f, 0 });
			break;
		}
		case ZONE_MINE: {
			bool blink = z->age < 0.5f || fmodf(z->age, 0.8f) < 0.15f;
			Mtx_Identity(&m);
			Mtx_Translate(&m, z->pos.x, 0.15f, z->pos.z, true);
			Mtx_Scale(&m, 0.7f, 0.3f, 0.7f);
			render_mesh(&g_meshSphere, 0, &m, (Color){ 0.25f, 0.25f, 0.28f, 1 }, NO_EMIT);
			render_mesh_at(&g_meshSphere, 0, v3_add(z->pos, v3(0, 0.45f, 0)), 0, 0.18f,
			               blink ? COLOR(1, 0.2f, 0.1f) : COLOR(0.4f, 0.05f, 0.05f), blink ? (Color){ 1, 0.1f, 0, 0 } : NO_EMIT);
			break;
		}
		case ZONE_BLACKHOLE:
			render_mesh_at(&g_meshSphere, 0, v3_add(z->pos, v3(0, 1.5f, 0)), 0, 1.2f, (Color){ 0.05f, 0, 0.1f, 0.95f }, NO_EMIT);
			render_mesh_at(&g_meshRing, 0, v3_add(z->pos, v3(0, 0.2f, 0)), -g.realTime * 3, z->radius * (1.2f - fmodf(g.realTime, 1.0f) * 0.6f),
			               (Color){ 0.5f, 0.1f, 0.9f, 0.5f * fade }, (Color){ 0.3f, 0.05f, 0.5f, 0 });
			break;
		}
	}

	// Chunkers rocks
	vec3 rocks[12];
	int nr = weapons_orbiters(rocks, 12);
	for (int k = 0; k < nr; k++)
		render_mesh_at(&g_meshSphere, 0, rocks[k], g.realTime * 3 + k, 0.8f, COLOR(0.55f, 0.5f, 0.45f), NO_EMIT);

	// aura weapon
	for (int i = 0; i < g.numWeapons; i++) {
		if (g.weapons[i].id != WEAPON_AURA) continue;
		float r = 6.0f * g.weapons[i].size * g.stats.sizeMult;
		float pulse = 0.25f + 0.1f * sinf(g.realTime * 6);
		render_mesh_at(&g_meshRing, 0, v3_add(g.pos, v3(0, 0.2f, 0)), g.realTime, r, (Color){ 1, 0.85f, 0.3f, pulse }, (Color){ 0.5f, 0.4f, 0.1f, 0 });
	}
	render_set_depth_write(true);
}

static void setup_camera(C3D_Mtx* view)
{
	vec3 target, eye;
	if (MENU_STATE(g.state)) {
		// orbit slowly around the character
		float a = g.realTime * 0.25f;
		target = v3(0, 2.2f, 0);
		eye = v3(sinf(a) * 11, 4.5f, cosf(a) * 11);
	} else {
		target = v3_add(g.pos, v3(0, 2.6f, 0));
		float cp = cosf(g.camPitch), sp = sinf(g.camPitch);
		eye = v3(g.pos.x - sinf(g.camYaw) * cp * g.camDist, g.pos.y + 2.0f + sp * g.camDist,
		         g.pos.z - cosf(g.camYaw) * cp * g.camDist);
	}
	s_camPos = eye;
	s_camFwd = v3_norm(v3(target.x - eye.x, 0, target.z - eye.z));
	Mtx_LookAt(view, FVec3_New(eye.x, eye.y, eye.z), FVec3_New(target.x, target.y, target.z), FVec3_New(0, 1, 0), true);
}

void draw_scene(C3D_RenderTarget* top)
{
	C3D_Mtx view;
	setup_camera(&view);
	const float* amb = STAGE->ambient;
	const float* lig = STAGE->light;
	Color ambient = { amb[0] * 0.6f, amb[1] * 0.6f, amb[2] * 0.6f, 1 };
	Color light = { lig[0] * 0.7f, lig[1] * 0.7f, lig[2] * 0.7f, 1 };
	render_begin_3d(top, &view, v3(0.45f, 0.8f, -0.35f), ambient, light);

	world_draw(s_camPos, s_camFwd);
	if (MENU_STATE(g.state)) {
		// loaded in the background by game_select_character(); the previous model stays until it's ready
		Mesh* m = game_displayed_character();
		draw_shadow(v3(0, 0, 0), 1.6f);
		if (m) render_mesh_anim_at(m, mesh_find_anim(m, "idle"), g.realTime, v3(0, 0, 0), g.realTime * 0.8f, 1.0f, COLOR(1, 1, 1), NO_EMIT);
		return;
	}
	interact_draw(s_camPos, s_camFwd);
	draw_shadow(g.pos, 1.3f);
	draw_player();
	draw_enemies();
	draw_pickups();
	draw_projectiles();
	draw_fx();
}

// ---------------------------------------------------------------- top HUD

static void fmt_time(char* buf, size_t n, float t)
{
	int s = (int)t;
	snprintf(buf, n, "%d:%02d", s / 60, s % 60);
}

void draw_hud_top(void)
{
	char buf[96];
	C2D_TextBufClear(s_textBuf);

	if (g.state == STATE_TITLE) {
		if (!ui_image("ui_logo", 200, 52, 1.0f, true))
			ui_text_shadow("MEGABONK", 200, 30, 2.2f, GOLD, C2D_AlignCenter, true);
		ui_text_shadow("3DS", 200, 92, 1.0f, GOLD, C2D_AlignCenter, true);
		return;
	}
	if (g.state == STATE_CHARSELECT) {
		const CharacterDef* ch = &g_characters[g.character];
		ui_text_shadow(ch->name, 200, 8, 1.5f, WHITE, C2D_AlignCenter, true);
		// passive ability
		C2D_DrawRectSolid(10, 168, 0.5f, 380, 66, C2D_Color32(0, 0, 0, 150));
		snprintf(buf, sizeof(buf), "Passive: %s", g_passiveNames[ch->passive]);
		ui_text_shadow(buf, 18, 172, 0.6f, C2D_Color32(140, 220, 255, 255), 0, true);
		ui_text_wrap(g_passiveDescs[ch->passive], 18, 192, 0.45f, WHITE, 364);
		return;
	}
	if (g.state == STATE_STAGESELECT) {
		const StageDef* s = &g_stages[g.stageCursor];
		ui_text_shadow(s->name, 200, 8, 1.6f, WHITE, C2D_AlignCenter, true);
		fmt_time(buf, sizeof(buf), s->stageTime);
		char line[128];
		snprintf(line, sizeof(line), "Survive %s, then face the boss", buf);
		ui_text_shadow(line, 200, 206, 0.6f, GOLD, C2D_AlignCenter, false);
		return;
	}

	// damage numbers
	for (int i = 0; i < MAX_DAMAGE_NUMS; i++) {
		DamageNum* d = &g.dmgNums[i];
		if (!d->active) continue;
		float sx, sy;
		if (!render_project(d->pos, &sx, &sy)) continue;
		if (d->value <= 0) snprintf(buf, sizeof(buf), "dodge");
		else snprintf(buf, sizeof(buf), "%d", (int)(d->value + 0.5f));
		u32 col = d->crit ? C2D_Color32(255, 220, 40, 255) : WHITE;
		ui_text_shadow(buf, sx, sy, d->crit ? 0.6f : 0.45f, col, C2D_AlignCenter, false);
	}

	// hurt vignette
	if (g.hurtFlash > 0 && g.state == STATE_PLAYING)
		C2D_DrawRectSolid(0, 0, 0.5f, 400, 240, C2D_Color32(255, 0, 0, (u8)(g.hurtFlash * 300)));

	// xp bar
	ui_bar(0, 0, 400, 7, g.xp / g.xpNext, C2D_Color32(80, 200, 255, 255), C2D_Color32(20, 30, 60, 220));
	snprintf(buf, sizeof(buf), "LV %d", g.level);
	ui_text_shadow(buf, 4, 9, 0.75f, WHITE, 0, true);

	// timer
	if (g.finalSwarm) {
		ui_text_shadow("FINAL SWARM", 200, 9, 0.9f, C2D_Color32(255, 80, 80, 255), C2D_AlignCenter, true);
	} else {
		fmt_time(buf, sizeof(buf), STAGE->stageTime - g.time);
		ui_text_shadow(buf, 200, 9, 1.0f, WHITE, C2D_AlignCenter, true);
	}

	snprintf(buf, sizeof(buf), "Kills %d", g.kills);
	ui_text_shadow(buf, 396, 9, 0.6f, WHITE, C2D_AlignRight, true);
	snprintf(buf, sizeof(buf), "%d", g.gold);
	ui_image("icon_icon_coin", 360, 26, 0.5f, false);
	ui_text_shadow(buf, 356, 26, 0.6f, GOLD, C2D_AlignRight, true);

	// hp
	ui_bar(6, 224, 150, 10, g.hp / g.stats.maxHp, C2D_Color32(220, 40, 50, 255), C2D_Color32(50, 10, 10, 220));
	if (g.stats.shieldMax > 0)
		ui_bar(6, 218, 150, 4, g.shield / g.stats.shieldMax, C2D_Color32(90, 170, 255, 255), C2D_Color32(15, 25, 50, 220));
	snprintf(buf, sizeof(buf), "%d / %d", (int)ceilf(g.hp), (int)g.stats.maxHp);
	ui_text_shadow(buf, 81, 223, 0.4f, WHITE, C2D_AlignCenter, false);

	// boss bar
	if (g.bossIndex >= 0 && g.enemies[g.bossIndex].active) {
		Enemy* b = &g.enemies[g.bossIndex];
		ui_bar(100, 36, 200, 8, b->hp / b->maxHp, C2D_Color32(170, 60, 230, 255), C2D_Color32(30, 10, 40, 220));
		// "EntStage1" -> "Ent", "Pharaoh1" -> "Pharaoh"
		char bname[32];
		int n = 0;
		for (const char* c = g_enemyDefs[b->def].name; *c && n < 31 && !(*c >= '0' && *c <= '9'); c++) bname[n++] = *c;
		bname[n] = 0;
		char* st = strstr(bname, "Stage");
		if (st) *st = 0;
		ui_text_shadow(bname, 200, 44, 0.6f, C2D_Color32(230, 200, 255, 255), C2D_AlignCenter, true);
	}

	if (g.toastTime > 0)
		ui_text_shadow(g.toast, 200, 62, 0.58f, C2D_Color32(255, 230, 120, (u8)(clampf(g.toastTime, 0, 1) * 255)), C2D_AlignCenter, true);

	// interaction prompt
	if (g.state == STATE_PLAYING) {
		char pb[64];
		if (interact_prompt(pb, sizeof(pb))) {
			float w = 200;
			C2D_DrawRectSolid(200 - w / 2, 196, 0.5f, w, 20, C2D_Color32(0, 0, 0, 160));
			ui_text_shadow(pb, 200, 197, 0.55f, WHITE, C2D_AlignCenter, false);
		}
	}

	if (g.state == STATE_LEVELUP) {
		bool shrine = g.choices[0].kind == CHOICE_STAT;
		C2D_DrawRectSolid(0, 0, 0.5f, 400, 240, C2D_Color32(0, 0, 0, 120));
		ui_text_shadow(shrine ? "SHRINE CHARGED!" : "LEVEL UP!", 200, 92, shrine ? 1.15f : 1.6f, C2D_Color32(255, 220, 60, 255), C2D_AlignCenter, true);
		ui_text_shadow("Choose an upgrade below", 200, 140, 0.6f, WHITE, C2D_AlignCenter, false);
	} else if (g.state == STATE_CHEST || g.state == STATE_SHOP || g.state == STATE_MICROWAVE) {
		C2D_DrawRectSolid(0, 0, 0.5f, 400, 240, C2D_Color32(0, 0, 0, 110));
		const char* title = g.state == STATE_CHEST ? "NEW ITEM!" : g.state == STATE_SHOP ? "SHADY GUY" : "MICROWAVE";
		ui_text_shadow(title, 200, 95, 1.6f, GOLD, C2D_AlignCenter, true);
	} else if (g.state == STATE_PAUSED) {
		C2D_DrawRectSolid(0, 0, 0.5f, 400, 240, C2D_Color32(0, 0, 0, 140));
		ui_text_shadow("PAUSED", 200, 95, 1.8f, WHITE, C2D_AlignCenter, true);
	} else if (g.state == STATE_DEAD) {
		C2D_DrawRectSolid(0, 0, 0.5f, 400, 240, C2D_Color32(60, 0, 0, 150));
		ui_text_shadow("YOU DIED", 200, 90, 2.0f, C2D_Color32(255, 70, 70, 255), C2D_AlignCenter, true);
	} else if (g.state == STATE_WIN) {
		C2D_DrawRectSolid(0, 0, 0.5f, 400, 240, C2D_Color32(0, 0, 0, 120));
		ui_text_shadow("STAGE CLEAR!", 200, 90, 1.8f, C2D_Color32(255, 220, 60, 255), C2D_AlignCenter, true);
	}
}

// ---------------------------------------------------------------- bottom screen

static const char* choice_icon(const LevelChoice* c)
{
	switch (c->kind) {
	case CHOICE_NEW_WEAPON:     return g_weaponIcons[c->index];
	case CHOICE_UPGRADE_WEAPON: return g_weaponIcons[g.weapons[c->index].id];
	default:                    return g_tomeIcons[c->index];
	}
}

void ui_describe_choice(const LevelChoice* c, char* title, size_t tn, char* desc, size_t dn)
{
	switch (c->kind) {
	case CHOICE_NEW_WEAPON:
		snprintf(title, tn, "NEW: %s", g_weaponDefs[c->index].name);
		snprintf(desc, dn, "%s", g_weaponDefs[c->index].desc);
		break;
	case CHOICE_UPGRADE_WEAPON: {
		const Weapon* w = &g.weapons[c->index];
		snprintf(title, tn, "%s  Lv%d", g_weaponDefs[w->id].name, w->level + 1);
		desc[0] = 0;
		for (int s = 0; s < c->nstats; s++) {
			char part[40];
			WeaponStat st = c->stat[s];
			weapon_format_stat(st, c->amount[s], part, sizeof(part));
			if (s) strncat(desc, ", ", dn - strlen(desc) - 1);
			strncat(desc, part, dn - strlen(desc) - 1);
		}
		break;
	}
	case CHOICE_TOME:
	case CHOICE_STAT: {
		if (c->kind == CHOICE_TOME)
			snprintf(title, tn, "%s%s", g_tomeNames[c->index], g.tomeLevels[c->index] ? "" : "  (new)");
		else
			snprintf(title, tn, "Shrine blessing");
		float a = tome_amount(c->index, c->rarity);
		snprintf(desc, dn, g_tomeDescs[c->index], tome_display(c->index, a));
		break;
	}
	}
}

static void radar_point(float cx, float cy, float r, vec3 p, float* ox, float* oy)
{
	const float range = 60.0f;
	float c = cosf(g.camYaw), s = sinf(g.camYaw);
	float dx = p.x - g.pos.x, dz = p.z - g.pos.z;
	float rx = dx * c - dz * s, ry = dx * s + dz * c;
	float d = sqrtf(rx * rx + ry * ry);
	if (d > range) { rx *= range / d; ry *= range / d; }
	*ox = cx + rx / range * r;
	*oy = cy - ry / range * r;
}

static void draw_radar(float cx, float cy, float r)
{
	C2D_DrawCircleSolid(cx, cy, 0.5f, r + 2, C2D_Color32(0, 0, 0, 255));
	C2D_DrawCircleSolid(cx, cy, 0.5f, r, C2D_Color32(25, 45, 35, 255));
	float x, y;
	for (int i = 0; i < g.numInteract; i++) {
		const Interactable* it = &g.interact[i];
		if (!it->active || it->used || it->kind == INT_POT) continue;
		radar_point(cx, cy, r, it->pos, &x, &y);
		u32 col;
		switch (it->kind) {
		case INT_CHEST: case INT_CHEST_FREE: col = C2D_Color32(255, 200, 60, 255); break;
		case INT_SHADY: case INT_MICROWAVE:  col = C2D_Color32(80, 230, 120, 255); break;
		case INT_BOSS_PORTAL:                col = C2D_Color32(220, 80, 255, 255); break;
		default:                             col = C2D_Color32(90, 160, 255, 255); break;
		}
		C2D_DrawRectSolid(x - 2, y - 2, 0.5f, 4, 4, col);
	}
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Enemy* e = &g.enemies[i];
		if (!e->active) continue;
		radar_point(cx, cy, r, e->pos, &x, &y);
		bool big = g_enemyDefs[e->def].flags & (ENEMY_BOSS | ENEMY_MINIBOSS);
		u32 col = big ? C2D_Color32(220, 80, 255, 255) : e->elite ? C2D_Color32(255, 210, 40, 255) : C2D_Color32(230, 60, 60, 255);
		C2D_DrawCircleSolid(x, y, 0.5f, big ? 3.5f : 1.4f, col);
	}
	for (int i = 0; i < MAX_PICKUPS; i++) {
		Pickup* p = &g.pickups[i];
		if (!p->active || p->kind != PICKUP_CHEST) continue;
		radar_point(cx, cy, r, p->pos, &x, &y);
		C2D_DrawRectSolid(x - 2.5f, y - 2.5f, 0.5f, 5, 5, C2D_Color32(255, 200, 60, 255));
	}
	C2D_DrawTriangle(cx, cy - 5, WHITE, cx - 4, cy + 4, WHITE, cx + 4, cy + 4, WHITE, 0.5f);
}

static void draw_item_grid(float y0, int highlight)
{
	for (int i = 0; i < g.numItemKinds && i < UI_ITEM_COLS * 3; i++) {
		int id = g.itemOrder[i];
		float x = UI_ITEM_X0 + (i % UI_ITEM_COLS) * UI_ITEM_STEP;
		float y = y0 + (i / UI_ITEM_COLS) * UI_ITEM_STEP;
		u32 border = i == highlight ? WHITE : g_rarityColors[g_itemDefs[id].rarity];
		ui_slot(g_itemDefs[id].icon, x, y, 0.875f, border, g.itemCounts[id]);
	}
}

static void draw_bottom_playing(void)
{
	char buf[96];
	snprintf(buf, sizeof(buf), "%s  Lv %d", g_characters[g.character].name, g.level);
	ui_text(buf, 8, 3, 0.65f, GOLD, 0, true);
	snprintf(buf, sizeof(buf), "%d fps", (int)(g_fps + 0.5f));
	ui_text(buf, 214, 4, 0.4f, HINT, C2D_AlignRight, false);

	// weapons + tomes as icon slots, like the game's HUD
	for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
		bool has = i < g.numWeapons;
		ui_slot(has ? g_weaponIcons[g.weapons[i].id] : NULL, 8 + i * 38, 24, 1.0f,
		        has ? C2D_Color32(200, 160, 80, 255) : C2D_Color32(60, 60, 75, 255), has ? g.weapons[i].level : 0);
	}
	for (int i = 0; i < MAX_TOME_SLOTS; i++) {
		bool has = i < g.numTomes;
		int t = has ? g.tomeSlots[i] : 0;
		ui_slot(has ? g_tomeIcons[t] : NULL, 8 + i * 38, 64, 1.0f,
		        has ? C2D_Color32(90, 160, 255, 255) : C2D_Color32(60, 60, 75, 255), has ? g.tomeLevels[t] : 0);
	}

	ui_text("Items", 8, 99, 0.45f, DIM, 0, false);
	if (!g.numItemKinds) ui_text("Open chests and visit Shady Guy for items", 50, 99, 0.42f, HINT, 0, false);
	draw_item_grid(UI_ITEM_Y0_HUD, -1);

	ui_text("X: interact   START: pause   L/R: camera", 160, 224, 0.42f, HINT, C2D_AlignCenter, false);
	draw_radar(268, 54, 46);
}

static void draw_choice_card(int i, const char* icon, const char* title, const char* desc, Rarity rarity, bool sel,
                             const char* right)
{
	float y = UI_CARD_Y0 + i * UI_CARD_STEP;
	u32 border = sel ? WHITE : g_rarityColors[rarity];
	ui_panel(UI_CARD_X, y, UI_CARD_W, UI_CARD_H, border, sel ? PANEL_SEL : PANEL);
	C2D_DrawRectSolid(UI_CARD_X, y, 0.5f, 5, UI_CARD_H, g_rarityColors[rarity]);
	ui_slot(icon, UI_CARD_X + 11, y + 10, 1.0f, g_rarityColors[rarity], 0);
	ui_text(title, UI_CARD_X + 50, y + 5, 0.55f, WHITE, 0, false);
	ui_text(right ? right : g_rarityNames[rarity], UI_CARD_X + UI_CARD_W - 6, y + 5, 0.42f,
	        right ? GOLD : g_rarityColors[rarity], C2D_AlignRight, false);
	ui_text_wrap(desc, UI_CARD_X + 50, y + 24, 0.45f, C2D_Color32(200, 200, 215, 255), UI_CARD_W - 56);
}

static void draw_levelup_cards(void)
{
	char title[64], desc[128];
	bool shrine = g.choices[0].kind == CHOICE_STAT;
	ui_text(shrine ? "Charge Shrine - Choose one" : "LEVEL UP!  Choose one", 160, 4, shrine ? 0.6f : 0.75f, GOLD, C2D_AlignCenter, true);
	for (int i = 0; i < LEVELUP_CHOICES; i++) {
		const LevelChoice* c = &g.choices[i];
		ui_describe_choice(c, title, sizeof(title), desc, sizeof(desc));
		draw_choice_card(i, choice_icon(c), title, desc, c->rarity, i == g.choiceCursor, NULL);
	}
	char buf[64];
	if (shrine) snprintf(buf, sizeof(buf), "A/touch: pick");
	else snprintf(buf, sizeof(buf), "A/touch: pick    Y: reroll (%d)", g.rerolls);
	ui_text(buf, 160, 222, 0.45f, HINT, C2D_AlignCenter, false);
}

static void draw_chest_reveal(void)
{
	const ItemDef* it = &g_itemDefs[g.revealItem];
	u32 rc = g_rarityColors[it->rarity];
	if (g.revealText) ui_text_wrap(g.revealText, 16, 8, 0.5f, DIM, 288);
	ui_panel(60, 52, 200, 128, rc, PANEL);
	ui_slot(it->icon, 136, 62, 1.5f, rc, 0);
	ui_text(it->name, 160, 116, 0.8f, WHITE, C2D_AlignCenter, true);
	ui_text(g_rarityNames[it->rarity], 160, 136, 0.45f, rc, C2D_AlignCenter, false);
	ui_text_wrap(it->desc, 68, 152, 0.42f, C2D_Color32(200, 200, 215, 255), 184);
	if (g.itemCounts[g.revealItem]) {
		char b[32];
		snprintf(b, sizeof(b), "Owned: %d", g.itemCounts[g.revealItem]);
		ui_text(b, 160, 186, 0.42f, HINT, C2D_AlignCenter, false);
	}
	ui_text("A: take      B: discard", 160, 222, 0.5f, HINT, C2D_AlignCenter, false);
}

static void draw_shop(void)
{
	const Interactable* sh = &g.interact[g.activeShop];
	if (g.revealText) ui_text_wrap(g.revealText, 12, 2, 0.42f, DIM, 296);
	for (int i = 0; i < 3; i++) {
		const ItemDef* it = &g_itemDefs[sh->shopItems[i]];
		char price[24];
		snprintf(price, sizeof(price), "%d gold", shop_price(sh, i));
		draw_choice_card(i, it->icon, it->name, it->desc, it->rarity, i == g.shopCursor, price);
	}
	char buf[64];
	snprintf(buf, sizeof(buf), "Gold: %d     A: buy    B: leave", g.gold);
	ui_text(buf, 160, 222, 0.45f, HINT, C2D_AlignCenter, false);
}

static void draw_microwave(void)
{
	ui_text("Choose an Item to clone.", 160, 4, 0.55f, WHITE, C2D_AlignCenter, false);
	ui_text("This uses a random Item of the same tier.", 160, 22, 0.42f, DIM, C2D_AlignCenter, false);
	draw_item_grid(UI_ITEM_Y0_MICRO, g.shopCursor);
	if (g.shopCursor < g.numItemKinds) {
		const ItemDef* it = &g_itemDefs[g.itemOrder[g.shopCursor]];
		ui_text(it->name, 160, 160, 0.6f, g_rarityColors[it->rarity], C2D_AlignCenter, false);
		ui_text_wrap(it->desc, 20, 180, 0.42f, DIM, 280);
	}
	ui_text("D-pad: select    A: microwave    B: leave", 160, 222, 0.45f, HINT, C2D_AlignCenter, false);
}

// Text scaled down to fit a width (tile name banners)
static void ui_text_fit(const char* str, float cx, float y, float maxScale, float maxWidth, u32 color)
{
	C2D_Text t;
	C2D_TextParse(&t, s_textBuf, str);
	C2D_TextOptimize(&t);
	float w, h;
	C2D_TextGetDimensions(&t, 1.0f, 1.0f, &w, &h);
	float s = w > 0 ? fminf(maxScale, maxWidth / w) : maxScale;
	C2D_DrawText(&t, C2D_WithColor | C2D_AlignCenter, cx + 1, y + 1, 0.5f, s, s, C2D_Color32(0, 0, 0, 220));
	C2D_DrawText(&t, C2D_WithColor | C2D_AlignCenter, cx, y, 0.5f, s, s, color);
}

// Smash 64 style roster: fiery portrait tiles with name banners, plus the
// character's starting weapon as a badge in the corner.
static void draw_char_select(void)
{
	// backdrop: dark blue fading to black, like the N64 select screen
	C2D_DrawRectangle(0, 0, 0.5f, 320, 240, C2D_Color32(18, 24, 60, 255), C2D_Color32(18, 24, 60, 255),
	                  C2D_Color32(4, 4, 12, 255), C2D_Color32(4, 4, 12, 255));
	for (int i = 0; i < CHARACTER_COUNT; i++) {
		float x = UI_CS_TILE_X(i), y = UI_CS_TILE_Y(i);
		bool sel = i == g.character;
		// fire gradient tile
		C2D_DrawRectSolid(x - 1, y - 1, 0.5f, UI_CS_W + 2, UI_CS_H + 2, C2D_Color32(40, 8, 4, 255));
		C2D_DrawRectangle(x, y, 0.5f, UI_CS_W, UI_CS_H,
		                  C2D_Color32(110, 14, 8, 255), C2D_Color32(110, 14, 8, 255),
		                  C2D_Color32(235, 110, 20, 255), C2D_Color32(220, 70, 15, 255));
		// a few flame tongues
		for (int k = 0; k < 3; k++) {
			float fx = x + 6 + k * 14 + sinf(g.realTime * 3 + i + k) * 2;
			C2D_DrawTriangle(fx, y + UI_CS_H, C2D_Color32(255, 190, 60, 160), fx + 10, y + UI_CS_H, C2D_Color32(255, 190, 60, 160),
			                 fx + 5, y + UI_CS_H - 18 - 6 * sinf(g.realTime * 5 + k * 2 + i), C2D_Color32(255, 120, 30, 0), 0.5f);
		}
		ui_image(g_characters[i].portrait, x + (UI_CS_W - 40) / 2.0f, y + 12, 40.0f / 64.0f, false);   // stored at 64px
		// name banner across the top, uppercase like Smash
		C2D_DrawRectSolid(x, y, 0.5f, UI_CS_W, 11, C2D_Color32(60, 10, 6, 210));
		char up[32];
		int n = 0;
		for (const char* c = g_characters[i].name; *c && n < 31; c++) up[n++] = (*c >= 'a' && *c <= 'z') ? *c - 32 : *c;
		up[n] = 0;
		ui_text_fit(up, x + UI_CS_W / 2.0f, y + 1, 0.36f, UI_CS_W - 3, C2D_Color32(235, 225, 200, 255));
		// starting weapon badge
		C2D_DrawRectSolid(x + UI_CS_W - 17, y + UI_CS_H - 17, 0.5f, 17, 17, C2D_Color32(20, 10, 10, 200));
		ui_image(g_weaponIcons[g_characters[i].startWeapon], x + UI_CS_W - 16.5f, y + UI_CS_H - 16.5f, 0.5f, false);
		if (sel) {
			// pulsing 1P selection frame
			u8 a = (u8)(200 + 55 * sinf(g.realTime * 8));
			u32 c = C2D_Color32(255, 230, 40, a);
			C2D_DrawRectSolid(x - 2, y - 2, 0.5f, UI_CS_W + 4, 2, c);
			C2D_DrawRectSolid(x - 2, y + UI_CS_H, 0.5f, UI_CS_W + 4, 2, c);
			C2D_DrawRectSolid(x - 2, y, 0.5f, 2, UI_CS_H, c);
			C2D_DrawRectSolid(x + UI_CS_W, y, 0.5f, 2, UI_CS_H, c);
			C2D_DrawCircleSolid(x + 7, y + UI_CS_H - 7, 0.5f, 7, C2D_Color32(220, 30, 30, 255));
			ui_text("1P", x + 7, y + UI_CS_H - 13, 0.33f, WHITE, C2D_AlignCenter, false);
		}
	}

	// info panel: name + starting weapon
	const CharacterDef* ch = &g_characters[g.character];
	const WeaponDef* wd = &g_weaponDefs[ch->startWeapon];
	float py = UI_CS_TILE_Y(CHARACTER_COUNT - 1) + UI_CS_H + 6;
	ui_panel(6, py, 308, 236 - py - 14, C2D_Color32(110, 30, 20, 255), C2D_Color32(24, 16, 26, 235));
	ui_text(ch->name, 12, py + 1, 0.75f, GOLD, 0, true);
	ui_slot(g_weaponIcons[ch->startWeapon], 278, py + 3, 1.0f, C2D_Color32(200, 160, 80, 255), 0);
	ui_text(wd->name, 272, py + 4, 0.48f, WHITE, C2D_AlignRight, false);
	ui_text_wrap(wd->desc, 12, py + 21, 0.4f, DIM, 228);
	ui_text("A: fight!    B: back", 160, 226, 0.42f, HINT, C2D_AlignCenter, false);
}

static void draw_stage_select(void)
{
	C2D_DrawRectangle(0, 0, 0.5f, 320, 240, C2D_Color32(18, 24, 60, 255), C2D_Color32(18, 24, 60, 255),
	                  C2D_Color32(4, 4, 12, 255), C2D_Color32(4, 4, 12, 255));
	ui_text("Choose a stage", 160, 8, 0.8f, GOLD, C2D_AlignCenter, true);
	char buf[48];
	for (int i = 0; i < STAGE_COUNT; i++) {
		const StageDef* s = &g_stages[i];
		float x = UI_STAGE_X(i), y = UI_STAGE_Y;
		bool sel = i == g.stageCursor;
		ui_panel(x, y, UI_STAGE_W, UI_STAGE_H, sel ? GOLD : C2D_Color32(110, 30, 20, 255), sel ? PANEL_SEL : PANEL);
		ui_image(s->icon, x + 2, y + 2, (UI_STAGE_W - 4) / 128.0f, false);
		ui_text(s->name, x + UI_STAGE_W / 2.0f, y + 78, 0.6f, WHITE, C2D_AlignCenter, true);
		if (s->boss >= 0) {
			int n = 0;
			for (const char* c = g_enemyDefs[s->boss].name; *c && n < 20 && !(*c >= '0' && *c <= '9'); c++) buf[n++] = *c;
			buf[n] = 0;
			char* st = strstr(buf, "Stage");
			if (st) *st = 0;
			char line[64];
			snprintf(line, sizeof(line), "Boss: %s", buf);
			ui_text(line, x + UI_STAGE_W / 2.0f, y + 96, 0.4f, DIM, C2D_AlignCenter, false);
		}
	}
	ui_text("Left/Right: choose    A: start    B: back", 160, 222, 0.42f, HINT, C2D_AlignCenter, false);
}

static void draw_menu_buttons(const char* const* labels, int n)
{
	for (int i = 0; i < n; i++) {
		float y = UI_BTN_Y0 + i * UI_BTN_STEP;
		bool sel = i == g.menuCursor;
		ui_panel(UI_BTN_X, y, UI_BTN_W, UI_BTN_H, sel ? GOLD : C2D_Color32(90, 90, 110, 255), sel ? PANEL_SEL : PANEL);
		ui_text(labels[i], 160, y + 8, 0.9f, WHITE, C2D_AlignCenter, true);
	}
}

void draw_bottom(void)
{
	switch (g.state) {
	case STATE_TITLE: {
		static const char* const labels[] = { "Play", "Exit" };
		draw_menu_buttons(labels, 2);
		ui_text("Unofficial fan port. Assets from your own copy.", 160, 200, 0.42f, HINT, C2D_AlignCenter, false);
		if (!audio_available())
			ui_text("No sound: dump DSP firmware (sdmc:/3ds/dspfirm.cdc)", 160, 218, 0.42f, C2D_Color32(220, 120, 120, 255), C2D_AlignCenter, false);
		break;
	}
	case STATE_CHARSELECT:
		draw_char_select();
		break;
	case STATE_STAGESELECT:
		draw_stage_select();
		break;
	case STATE_PLAYING:
		draw_bottom_playing();
		break;
	case STATE_LEVELUP:
		draw_levelup_cards();
		break;
	case STATE_CHEST:
		draw_chest_reveal();
		break;
	case STATE_SHOP:
		draw_shop();
		break;
	case STATE_MICROWAVE:
		draw_microwave();
		break;
	case STATE_PAUSED: {
		static const char* const labels[] = { "Resume", "Quit run" };
		draw_menu_buttons(labels, 2);
		break;
	}
	case STATE_DEAD:
	case STATE_WIN: {
		ui_text(g.state == STATE_WIN ? "Victory!" : "Game over", 160, 20, 1.2f, GOLD, C2D_AlignCenter, true);
		char tbuf[16];
		fmt_time(tbuf, sizeof(tbuf), g.time);
		char line[96];
		snprintf(line, sizeof(line), "Time survived: %s", tbuf);
		ui_text(line, 160, 80, 0.6f, WHITE, C2D_AlignCenter, false);
		snprintf(line, sizeof(line), "Level %d    Kills %d    Gold %d", g.level, g.kills, g.gold);
		ui_text(line, 160, 105, 0.6f, WHITE, C2D_AlignCenter, false);
		draw_item_grid(130, -1);
		ui_text("A: back to title", 160, 222, 0.6f, C2D_Color32(180, 180, 200, 255), C2D_AlignCenter, false);
		break;
	}
	}
}
