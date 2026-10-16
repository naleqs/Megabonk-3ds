#include "game.h"
#include "mathutil.h"
#include "audio.h"
#include "gen/strings_gen.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	const char* mesh;
	const char* name;
	float scale;
	float range;      // interaction distance
	int count;        // how many per map
	float collide;    // player collision radius (0 = none)
} InteractDef;

static const InteractDef s_defs[INT_KIND_COUNT] = {
	[INT_CHEST]            = { "romfs:/mesh/obj_chest.mbm", "Chest", 1.0f, 4.5f, 16, 1.2f },
	[INT_CHEST_FREE]       = { "romfs:/mesh/obj_chest_free.mbm", "Free Chest", 1.0f, 4.5f, 3, 1.2f },
	[INT_POT]              = { "romfs:/mesh/obj_pot.mbm", "Pot", 0.35f, 3.5f, 24, 0 },
	[INT_SHRINE_CHARGE]    = { "romfs:/mesh/obj_shrine_charge.mbm", "Charge Shrine", 0.8f, 7.0f, 6, 1.2f },
	[INT_SHRINE_GREED]     = { "romfs:/mesh/obj_shrine_greed.mbm", "Greed Shrine", 1.6f, 5.0f, 1, 1.2f },
	[INT_SHRINE_CURSED]    = { "romfs:/mesh/obj_shrine_cursed.mbm", "Boss Curse", 0.7f, 5.0f, 2, 2.0f },
	[INT_SHRINE_MAGNET]    = { "romfs:/mesh/obj_shrine_magnet.mbm", "Shrine of Succ", 0.7f, 5.0f, 2, 2.0f },
	[INT_SHRINE_MOAI]      = { "romfs:/mesh/obj_shrine_moai.mbm", "Moai", 0.7f, 5.0f, 1, 1.6f },
	[INT_SHRINE_CHALLENGE] = { "romfs:/mesh/obj_shrine_challenge.mbm", "Challenge Shrine", 0.7f, 5.0f, 1, 2.0f },
	[INT_SHADY]            = { "romfs:/mesh/obj_shadyguy.mbm", "Shady Guy", 1.0f, 5.0f, 2, 1.0f },
	[INT_MICROWAVE]        = { "romfs:/mesh/obj_microwave.mbm", "Microwave", 0.8f, 5.0f, 1, 1.5f },
	[INT_BOSS_PORTAL]      = { "romfs:/mesh/obj_boss_portal.mbm", "Boss Portal", 0.55f, 8.0f, 1, 0 },
};

#define CHARGE_TIME 5.0f

static int chest_price(void)
{
	// each chest opened makes the next one pricier, like the game
	return (int)(12.0f * powf(1.2f, (float)g.chestsOpened) * (1.0f + g.time / 900.0f) + 0.5f);
}

void interact_generate(void)
{
	for (int k = 0; k < INT_KIND_COUNT; k++)
		g.interactMeshes[k] = mesh_load(s_defs[k].mesh);
	g.numInteract = 0;
	g.nearInteract = -1;
	g.challengeIndex = -1;
	for (int k = 0; k < INT_KIND_COUNT; k++) {
		for (int c = 0; c < s_defs[k].count && g.numInteract < MAX_INTERACT; c++) {
			vec3 p;
			int tries = 0;
			bool ok;
			do { // keep away from spawn and from each other
				p = v3(randrange(-MAP_HALF + 10, MAP_HALF - 10), 0, randrange(-MAP_HALF + 10, MAP_HALF - 10));
				ok = p.x * p.x + p.z * p.z > 18 * 18;
				for (int i = 0; ok && i < g.numInteract; i++)
					ok = dist2_xz(p, g.interact[i].pos) > 9 * 9;
			} while (!ok && ++tries < 30);
#ifdef MB_DEBUG_START_TIME
			// dev builds: one of each kind in a ring around spawn, for quick testing
			if (c == 0) {
				float a = k * 6.2832f / INT_KIND_COUNT;
				p = v3(sinf(a) * 16, 0, cosf(a) * 16);
			}
#endif
			Interactable* it = &g.interact[g.numInteract++];
			memset(it, 0, sizeof(*it));
			it->active = true;
			it->kind = k;
			it->pos = p;
			it->yaw = randf() * 6.283f;
			it->golden = k == INT_SHRINE_CHARGE && c == 0;
			if (k == INT_SHADY) {
				float r = randf();
				it->shopRarity = r < 0.1f ? RARITY_LEGENDARY : r < 0.4f ? RARITY_EPIC : RARITY_RARE;
				for (int s = 0; s < 3; s++) {
					int item;
					bool dup;
					do {
						item = items_roll(it->shopRarity);
						dup = false;
						for (int o = 0; o < s; o++) dup |= it->shopItems[o] == item;
					} while (dup);
					it->shopItems[s] = item;
				}
			}
			world_clear_area(p, k == INT_BOSS_PORTAL ? 9 : 5);
		}
	}
}

static void attract_xp_fx(vec3 at)
{
	game_attract_all_xp();
	fx_spawn(FX_RING, at, 10, 0.6f, COLOR(0.4f, 0.7f, 1));
}

void interact_open_item(int item, const char* text)
{
	g.revealItem = item;
	g.revealText = text;
	g.state = STATE_CHEST;
	g.choiceLock = 0.4f;
	audio_play_sfx(SFX_CHEST);
}

static void use_one(int i)
{
	Interactable* it = &g.interact[i];
	switch (it->kind) {
	case INT_CHEST:
	case INT_CHEST_FREE: {
		bool free = it->kind == INT_CHEST_FREE || randf() < items_key_chance();
		int price = chest_price();
		if (!free && g.gold < price) { game_toast("Not enough gold"); return; }
		if (!free) g.gold -= price;
		g.chestsOpened++;
		items_on_chest_open();
		it->active = false;
		interact_open_item(items_roll(items_roll_tier(it->kind == INT_CHEST_FREE ? 0.05f : 0)), NULL);
		break;
	}
	case INT_POT: {
		it->active = false;
		int coins = 1 + rand() % 4;
		for (int c = 0; c < coins; c++)
			game_spawn_pickup(PICKUP_GOLD, v3_add(it->pos, v3(randrange(-1, 1), 0, randrange(-1, 1))), 1);
		if (randf() < 0.3f) game_spawn_pickup(PICKUP_XP, it->pos, 3);
		if (randf() < 0.1f) game_spawn_pickup(PICKUP_HEART, it->pos, 20);
		fx_spawn(FX_DEATH, it->pos, 1.5f, 0.3f, COLOR(0.8f, 0.6f, 0.4f));
		audio_play_sfx(SFX_POT);
		break;
	}
	case INT_SHRINE_GREED:
		it->used = true;
		g.stats.goldMult += 0.25f;
		g.difficulty += 0.2f;
		game_toast("Greed: +25% gold, but more enemies...");
		fx_spawn(FX_RING, it->pos, 8, 0.6f, COLOR(1, 0.85f, 0.2f));
		audio_play_sfx(SFX_SHRINE);
		break;
	case INT_SHRINE_CURSED: {
		it->used = true;
		g.difficulty += 0.1f;
		if (STAGE->minibossCount) {
			int d = STAGE->minibosses[rand() % STAGE->minibossCount];
			game_spawn_enemy(d, v3_add(it->pos, v3(randrange(-6, 6), 0, randrange(6, 10))), true);
		}
		game_toast("Boss Curse! An elite boss appears");
		fx_spawn(FX_RING, it->pos, 8, 0.6f, COLOR(0.9f, 0.1f, 0.2f));
		audio_play_sfx(SFX_SHRINE);
		break;
	}
	case INT_SHRINE_MAGNET:
		it->used = true;
		attract_xp_fx(it->pos);
		game_toast("The Shrine of Succ pulls in all XP");
		audio_play_sfx(SFX_SHRINE);
		break;
	case INT_SHRINE_MOAI: {
		it->used = true;
		float r = randf() - g.stats.luck * 0.1f;
		int judgement = r < 0.10f ? 3 : r < 0.35f ? 2 : r < 0.75f ? 1 : 0;
		const char* line = g_moaiLines[judgement][rand() % 3];
		audio_play_sfx(SFX_SHRINE);
		if (judgement == 0) { g.gold += 5; game_toast(line); break; }
		static const u8 tier[4] = { 0, RARITY_COMMON, RARITY_RARE, RARITY_LEGENDARY };
		interact_open_item(items_roll(tier[judgement]), line);
		break;
	}
	case INT_SHRINE_CHALLENGE:
		if (g.challengeIndex >= 0) return;
		it->used = true;
		it->timer = 30.0f;
		g.challengeIndex = i;
		game_toast("Challenge! Survive the elites for 30s");
		audio_play_sfx(SFX_SWARM);
		break;
	case INT_SHADY:
		g.activeShop = i;
		g.shopCursor = 0;
		g.state = STATE_SHOP;
		g.choiceLock = 0.3f;
		g.revealText = g_shadyLines[rand() % SHADY_LINE_COUNT];
		break;
	case INT_MICROWAVE:
		if (!g.numItemKinds) { game_toast("You have nothing to microwave"); return; }
		g.activeShop = i;
		g.shopCursor = 0;
		g.state = STATE_MICROWAVE;
		g.choiceLock = 0.3f;
		break;
	case INT_BOSS_PORTAL: {
		if (g.bossSpawned) return;
		it->used = true;
		g.bossSpawned = true;
		int d = STAGE->boss;
		if (d >= 0) g.bossIndex = game_spawn_enemy(d, v3_add(it->pos, v3(0, 0, 10)), false);
		game_toast("The boss awakens! Defeat it!");
		audio_play_sfx(SFX_BOSS);
		audio_play_music(STAGE->bossMusic);
		break;
	}
	default:
		break;
	}
}

void interact_use(void)
{
	if (g.nearInteract < 0) return;
	Interactable* it = &g.interact[g.nearInteract];
	bool wasActive = it->active, wasUsed = it->used;
	use_one(g.nearInteract);
	if ((wasActive && !it->active) || (!wasUsed && it->used)) items_on_interact();   // Ghost
}

int shop_price(const Interactable* it, int slot)
{
	static const int base[RARITY_COUNT] = { 15, 20, 30, 55, 95 };
	(void)slot;
	return (int)(base[it->shopRarity] * (1.0f + g.time / 600.0f) + 0.5f);
}

void shop_buy(int slot)
{
	Interactable* it = &g.interact[g.activeShop];
	int price = shop_price(it, slot);
	if (g.gold < price) { game_toast("\"You got money? Didn't think so.\""); return; }
	g.gold -= price;
	items_give(it->shopItems[slot]);
	it->active = false;   // he leaves after one sale
	g.state = STATE_PLAYING;
	game_toast("\"Thanks idiot\"");
	audio_play_sfx(SFX_PURCHASE);
}

void microwave_use(int slot)
{
	if (slot < 0 || slot >= g.numItemKinds) return;
	int chosen = g.itemOrder[slot];
	u8 tier = g_itemDefs[chosen].rarity;
	// sacrifice a random item of the same tier (may be another copy of the chosen one)
	int cand[MAX_OWNED_ITEMS * 4], n = 0;
	for (int i = 0; i < g.numItemKinds; i++) {
		int id = g.itemOrder[i];
		if (g_itemDefs[id].rarity != tier) continue;
		int copies = g.itemCounts[id] - (id == chosen ? 1 : 0);
		for (int c = 0; c < copies && n < (int)(sizeof(cand) / sizeof(cand[0])); c++) cand[n++] = id;
	}
	if (!n) { game_toast("Needs another item of the same tier"); return; }
	items_take(cand[rand() % n]);
	items_give(chosen);
	g.interact[g.activeShop].used = true;
	g.state = STATE_PLAYING;
	char buf[64];
	snprintf(buf, sizeof(buf), "Microwaved: +1 %s", g_itemDefs[chosen].name);
	game_toast(buf);
	audio_play_sfx(SFX_PURCHASE);
}

// Solid interactables take part in world collision (walls below the top, floor on top).
bool interact_solid(int i, float* radius, float* top)
{
	const Interactable* it = &g.interact[i];
	const InteractDef* d = &s_defs[it->kind];
	if (!it->active || d->collide <= 0) return false;
	const Mesh* m = g.interactMeshes[it->kind];
	*radius = d->collide;
	*top = m ? m->maxy * d->scale : 2.0f;
	return true;
}

void interact_update(float dt)
{
	g.nearInteract = -1;
	float best = 1e9f;
	for (int i = 0; i < g.numInteract; i++) {
		Interactable* it = &g.interact[i];
		if (!it->active) continue;
		const InteractDef* d = &s_defs[it->kind];
		float d2 = dist2_xz(it->pos, g.pos);

		if (it->kind == INT_SHRINE_CHARGE) {
			if (it->used) continue;
			bool inside = d2 < d->range * d->range;
			float rate = (1.0f + 0.04f * items_count(ITEM_WRENCH)) / CHARGE_TIME;
			float prev = it->progress;
			it->progress = clampf(it->progress + (inside ? rate : -0.5f * rate) * dt, 0, 1);
			if (inside && prev == 0 && it->progress > 0) audio_play_sfx(SFX_SHRINE);
			if (it->progress >= 1) {
				it->used = true;
				items_on_interact();
				game_roll_stat_choices(it->golden ? RARITY_EPIC : RARITY_UNCOMMON);
				audio_play_sfx(SFX_LEVELUP);
			}
			continue; // charge shrines aren't "used" with X
		}
		if (it->used) continue;
		if (d2 < d->range * d->range && d2 < best) { best = d2; g.nearInteract = i; }
	}

	// active challenge: elites keep coming for 30s, then a free chest
	if (g.challengeIndex >= 0) {
		Interactable* it = &g.interact[g.challengeIndex];
		float before = it->timer;
		it->timer -= dt;
		if ((int)(before / 4.0f) != (int)(it->timer / 4.0f) && g.poolSize > 0) {
			int e = game_spawn_enemy(g.enemyPool[rand() % g.poolSize], game_random_spawn_point(18, 26), true);
			if (e >= 0) g.enemies[e].noChest = true;
		}
		if (it->timer <= 0) {
			game_spawn_pickup(PICKUP_CHEST, it->pos, 1);
			game_toast("Challenge complete! A chest appeared");
			g.challengeIndex = -1;
		}
	}
}

const char* interact_prompt(char* buf, size_t n)
{
	if (g.nearInteract < 0) return NULL;
	const Interactable* it = &g.interact[g.nearInteract];
	switch (it->kind) {
	case INT_CHEST:        snprintf(buf, n, "X: Open Chest (%d gold)", chest_price()); break;
	case INT_CHEST_FREE:   snprintf(buf, n, "X: Open Free Chest"); break;
	case INT_POT:          snprintf(buf, n, "X: Break pot"); break;
	case INT_SHRINE_MOAI:  snprintf(buf, n, "X: Consult the Moai"); break;
	case INT_SHRINE_CHALLENGE: snprintf(buf, n, "X: Summon the Challenge!"); break;
	case INT_SHADY:        snprintf(buf, n, "X: Talk to Shady Guy"); break;
	case INT_MICROWAVE:    snprintf(buf, n, "X: Use Microwave"); break;
	case INT_BOSS_PORTAL:  snprintf(buf, n, "X: Spawn Boss"); break;
	default:               snprintf(buf, n, "X: %s", s_defs[it->kind].name); break;
	}
	return buf;
}

void interact_draw(vec3 camPos, vec3 camFwd)
{
	Color none = { 0, 0, 0, 0 };
	for (int i = 0; i < g.numInteract; i++) {
		Interactable* it = &g.interact[i];
		if (!it->active) continue;
		float dx = it->pos.x - camPos.x, dz = it->pos.z - camPos.z;
		if (dx * dx + dz * dz > 100 * 100) continue;
		const InteractDef* d = &s_defs[it->kind];
		Mesh* m = g.interactMeshes[it->kind];
		if (dx * camFwd.x + dz * camFwd.z < -m->radius * d->scale - 2) continue;
		Color tint = it->used ? COLOR(0.45f, 0.45f, 0.5f) : it->golden ? COLOR(1.3f, 1.1f, 0.5f) : COLOR(1, 1, 1);
		Color emit = (i == g.nearInteract) ? (Color){ 0.25f, 0.25f, 0.25f, 0 } : none;
		if (m->nanims > 0)   // Shady Guy's briefcase idle; offset per instance so they don't sync up
			render_mesh_anim_at(m, 0, g.realTime + i * 1.7f, it->pos, it->yaw, d->scale, tint, emit);
		else
			render_mesh_at(m, 0, it->pos, it->yaw, d->scale, tint, emit);
	}
	// translucent pass: charge zones
	render_set_depth_write(false);
	for (int i = 0; i < g.numInteract; i++) {
		Interactable* it = &g.interact[i];
		if (!it->active || it->kind != INT_SHRINE_CHARGE || it->used) continue;
		if (dist2_xz(it->pos, camPos) > 100 * 100) continue;
		float r = s_defs[INT_SHRINE_CHARGE].range;
		Color zc = it->golden ? (Color){ 1, 0.8f, 0.2f, 0.35f } : (Color){ 0.3f, 0.6f, 1, 0.35f };
		render_mesh_at(&g_meshRing, 0, v3_add(it->pos, v3(0, 0.15f, 0)), 0, r, zc, (Color){ zc.r * 0.5f, zc.g * 0.5f, zc.b * 0.5f, 0 });
		if (it->progress > 0)
			render_mesh_at(&g_meshRing, 0, v3_add(it->pos, v3(0, 0.2f, 0)), g.realTime, r * it->progress,
			               (Color){ 1, 1, 1, 0.6f }, (Color){ 0.6f, 0.6f, 0.6f, 0 });
	}
	render_set_depth_write(true);
}
