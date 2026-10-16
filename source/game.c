#include "game.h"
#include "mathutil.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>

Game g;

const char* const g_tomeNames[TOME_COUNT] = {
	"Damage Tome", "Cooldown Tome", "Agility Tome", "HP Tome", "Regen Tome", "Armor Tome",
	"Evasion Tome", "Attraction Tome", "Size Tome", "Quantity Tome", "Precision Tome", "XP Tome",
	"Luck Tome", "Projectile Speed Tome", "Shield Tome", "Thorns Tome", "Bloody Tome", "Knockback Tome",
	"Golden Tome", "Duration Tome", "Cursed Tome", "Chaos Tome",
};
const char* const g_tomeIcons[TOME_COUNT] = {
	"icon_damagetome", "icon_cooldowntome", "icon_speedtome", "icon_healthtome", "icon_regenerationtome",
	"icon_armortome", "icon_evasiontome", "icon_attractiontome", "icon_sizetome", "icon_quantitytome",
	"icon_precisiontome", "icon_xptome", "icon_lucktome", "icon_projectilespeedtome", "icon_shieldtome",
	"icon_itemspikyshieldf", "icon_bloodtome", "icon_knockbacktome", "icon_goldentome", "icon_durationtome",
	"icon_cursedtome", "icon_chaostome",
};
// printf formats taking the tome_amount() value
const char* const g_tomeDescs[TOME_COUNT] = {
	"+%.0f%% Damage", "+%.1f%% Attack speed", "+%.0f%% Movement speed", "+%.0f Max HP", "+%.0f HP regen/min",
	"+%.0f%% Armor", "+%.0f%% Evasion", "+%.0f%% Pickup range", "+%.0f%% Size", "+%.0f Projectile",
	"+%.0f%% Crit chance", "+%.0f%% XP gain", "+%.0f%% Luck", "+%.0f%% Projectile speed", "+%.0f Shield",
	"+%.0f Thorns", "+%.0f%% Lifesteal", "+%.0f%% Knockback", "+%.0f%% Gold gain", "+%.0f%% Duration",
	"+%.1f%% Difficulty", "Boost a random stat (x%.0f%%)",
};

static const float RARITY_MULT[RARITY_COUNT] = { 1.0f, 1.25f, 1.5f, 1.85f, 2.3f };

#define PLAYER_RADIUS 0.9f
#define GRAVITY 38.0f
#define JUMP_VEL 15.0f
#define ENEMY_CONTACT_SCALE 0.4f

// Spawn director tuning: alive-enemy target and credits/second, both growing per minute.
#define SPAWN_BASE_ALIVE     8.0f
#define SPAWN_ALIVE_PER_MIN  6.0f
#define SPAWN_BASE_RATE      1.0f
#define SPAWN_RATE_PER_MIN   0.6f

// ---------------------------------------------------------------- helpers

void game_toast(const char* msg)
{
	strncpy(g.toast, msg, sizeof(g.toast) - 1);
	g.toastTime = 2.5f;
}

Fx* fx_spawn(FxKind kind, vec3 pos, float size, float life, Color color)
{
	for (int i = 0; i < MAX_FX; i++) {
		Fx* f = &g.fx[i];
		if (f->active) continue;
		f->active = true;
		f->kind = kind;
		f->pos = pos;
		f->size = size;
		f->life = life;
		f->age = 0;
		f->yaw = randf() * 6.283f;
		f->color = color;
		return f;
	}
	return NULL;
}

static void spawn_damage_number(vec3 pos, float value, bool crit)
{
	int best = 0;
	float oldest = -1;
	for (int i = 0; i < MAX_DAMAGE_NUMS; i++) {
		if (!g.dmgNums[i].active) { best = i; oldest = 1e9f; break; }
		if (g.dmgNums[i].age > oldest) { oldest = g.dmgNums[i].age; best = i; }
	}
	DamageNum* d = &g.dmgNums[best];
	d->active = true;
	d->pos = v3_add(pos, v3(randrange(-0.6f, 0.6f), 0, randrange(-0.6f, 0.6f)));
	d->value = value;
	d->age = 0;
	d->crit = crit;
}

static float xp_for_level(int level)
{
	return floorf(8.0f * powf((float)level, 1.35f) + 4.0f);
}

float game_enemy_hp_mult(void)
{
	float m = g.time / 60.0f;
	float mult = (1.0f + m * 0.3f + m * m * 0.04f) * (1.0f + g.difficulty * 0.5f);
	if (g.finalSwarm) mult *= 1.0f + (g.time - STAGE->stageTime) / 30.0f;  // ramps until you die
	return mult;
}

float game_enemy_dmg_mult(void)
{
	float m = 1.0f + g.time / 60.0f * 0.12f;
	if (g.finalSwarm) m *= 1.0f + (g.time - STAGE->stageTime) / 60.0f;
	return m;
}

void game_heal(float hp)
{
	float cap = g.stats.maxHp * (1.0f + g.stats.overheal);
	float before = g.hp;
	if (g.hp < cap) g.hp = fminf(cap, g.hp + hp);
	items_on_heal(g.hp - before);
}

static void shield_update(float dt)
{
	if (g.stats.shieldMax <= 0) return;
	if (g.shieldDelay > 0) g.shieldDelay -= dt;
	else g.shield = fminf(g.stats.shieldMax, g.shield + g.stats.shieldMax * 0.25f * dt);
}

// ---------------------------------------------------------------- run setup

static void stats_reset(PlayerStats* s)
{
	memset(s, 0, sizeof(*s));
	s->maxHp = 100;
	s->regen = 0.2f;
	s->moveSpeed = 1.0f;
	s->damageMult = 1.0f;
	s->attackSpeed = 1.0f;
	s->sizeMult = 1.0f;
	s->durationMult = 1.0f;
	s->projSpeedMult = 1.0f;
	s->critChance = 0.05f;
	s->critMult = 2.0f;
	s->pickupRange = 9.0f;
	s->xpMult = 1.0f;
	s->knockback = 1.0f;
	s->goldMult = 1.0f;
}

// Character meshes are big (baked animation), so only one is resident. Selecting
// starts a background load; the previous model stays on screen until it's ready.
static int s_loadedChar = -1, s_wantChar = -1;

void game_select_character(int c)
{
	s_wantChar = c;
	mesh_request(g_characters[c].mesh);
}

void game_poll_character(void)
{
	if (s_wantChar < 0 || s_wantChar == s_loadedChar) return;
	if (!mesh_poll(g_characters[s_wantChar].mesh)) return;
	if (s_loadedChar >= 0) mesh_unload(g_characters[s_loadedChar].mesh);
	s_loadedChar = s_wantChar;
}

// Model to show on the title/select screens (may lag the selection briefly); NULL while none is loaded.
Mesh* game_displayed_character(void)
{
	return s_loadedChar >= 0 ? mesh_load(g_characters[s_loadedChar].mesh) : NULL;
}

static Mesh* load_character_now(int c)
{
	game_select_character(c);
	while (s_loadedChar != c) {
		game_poll_character();
		if (s_loadedChar != c) svcSleepThread(5 * 1000 * 1000);
	}
	return mesh_load(g_characters[c].mesh);
}

void game_preload(void)
{
	for (int i = 0; i < PROP_COUNT; i++) g.propMeshes[i] = mesh_load(g_propDefs[i].mesh);
	for (int i = 0; i < ENEMY_COUNT; i++) g.enemyMeshes[i] = mesh_load(g_enemyDefs[i].mesh);
	// characters are big (baked animation): only the selected one is kept loaded (see game_select_character)
	g.meshBanana = mesh_load("romfs:/mesh/obj_proj_banana.mbm");
	g.meshArrow = mesh_load("romfs:/mesh/obj_proj_arrow.mbm");
	g.meshDagger = mesh_load("romfs:/mesh/obj_proj_dagger.mbm");
	g.meshFlask = mesh_load("romfs:/mesh/obj_proj_flask.mbm");
	g.meshBone = mesh_load("romfs:/mesh/obj_bone.mbm");
	g.meshAxe = mesh_load("romfs:/mesh/obj_axe.mbm");
	g.meshDice = mesh_load("romfs:/mesh/obj_dice.mbm");
	g.meshSword = mesh_load("romfs:/mesh/obj_sword.mbm");
	g.meshChest = mesh_load("romfs:/mesh/obj_chest.mbm");
	g.meshCoins = mesh_load("romfs:/mesh/obj_coins.mbm");
}

void game_new_run(int character, int stage)
{
	memset(&g, 0, sizeof(g));
	game_preload(); // meshes are cached, this just re-fetches the pointers

	g.character = character;
	g.stage = stage;
	g.charMesh = load_character_now(character);
	render_set_fog_color(STAGE->fog[0], STAGE->fog[1], STAGE->fog[2]);
	g.state = STATE_PLAYING;
	stats_reset(&g.stats);
	g.hp = g.stats.maxHp;
	g.level = 1;
	g.xpNext = xp_for_level(1);
	g.camDist = 11.5f;
	g.camPitch = 0.34f;
	g.rerolls = 2;
	g.bossIndex = -1;
	g.swarmEnemy = -1;
	g.wKnock = 1;

	weapons_init_slot(&g.weapons[0], g_characters[character].startWeapon);
	g.numWeapons = 1;

	world_generate();
	interact_generate();
	g.pos = v3(0, 0, 0);
	world_collide(&g.pos, 3.0f);

	// enemy pool starts with the stage's start enemies; one new type joins per interval
	for (int i = 0; i < STAGE->startCount; i++)
		g.enemyPool[g.poolSize++] = STAGE->startEnemies[i];
	g.nextPoolTime = STAGE->newEnemyInterval;
	g.credits = 3.0f;
	g.lastAttacker = -1;
	g.passiveTimer[2] = 60.0f;   // Hoarder's first free chest
	passive_init();
#ifdef MB_DEBUG_START_TIME
	g.time = MB_DEBUG_START_TIME;
	g.stats.maxHp = g.hp = 100000;
	g.gold = 500;
#endif
	audio_play_music(STAGE->music);
}

// ---------------------------------------------------------------- enemies

static int enemy_alloc(void)
{
	for (int i = 0; i < MAX_ENEMIES; i++)
		if (!g.enemies[i].active) return i;
	return -1;
}

vec3 game_random_spawn_point(float minR, float maxR)
{
	for (int tries = 0; tries < 8; tries++) {
		float a = randf() * 6.2832f;
		float r = randrange(minR, maxR);
		vec3 p = v3(g.pos.x + sinf(a) * r, 0, g.pos.z + cosf(a) * r);
		if (fabsf(p.x) < MAP_HALF - 4 && fabsf(p.z) < MAP_HALF - 4) return p;
	}
	return v3(clampf(g.pos.x + randrange(-maxR, maxR), -MAP_HALF + 4, MAP_HALF - 4), 0,
	          clampf(g.pos.z + randrange(-maxR, maxR), -MAP_HALF + 4, MAP_HALF - 4));
}

int game_spawn_enemy(int def, vec3 pos, bool elite)
{
	int i = enemy_alloc();
	if (i < 0) return -1;
	const EnemyDef* d = &g_enemyDefs[def];
	Enemy* e = &g.enemies[i];
	memset(e, 0, sizeof(*e));
	e->active = true;
	e->def = def;
	e->elite = elite;
	e->pos = pos;
	if (d->flags & ENEMY_FLYING) e->pos.y = 2.0f;
	float hp = (float)d->hp;
	if (!(d->flags & ENEMY_BOSS)) hp *= game_enemy_hp_mult();
	if (elite) hp *= 6.0f;
	e->hp = e->maxHp = hp;
	e->scale = elite ? 1.35f : 1.0f;
	e->animTime = randf();
	e->attackCd = 0.5f;
	attacks_on_spawn(e);
	g.numEnemies++;
	fx_spawn(FX_SPAWN, e->pos, 1.5f, 0.4f, COLOR(0.6f, 0.2f, 0.9f));
	return i;
}

int game_find_enemy_def(const char* name)
{
	for (int i = 0; i < ENEMY_COUNT; i++)
		if (strcmp(g_enemyDefs[i].name, name) == 0) return i;
	return -1;
}

static bool in_pool(int def)
{
	for (int i = 0; i < g.poolSize; i++)
		if (g.enemyPool[i] == def) return true;
	return false;
}

// StageData.checkNewEnemyInterval: every interval one more enemy type joins, picked at random
// from this stage's enemies (EnemyData.maps) whose canSpawnAfterTime has passed.
static void pool_add_next(void)
{
	int cand[ENEMY_COUNT], n = 0;
	for (int i = 0; i < ENEMY_COUNT; i++) {
		const EnemyDef* d = &g_enemyDefs[i];
		if (d->flags & (ENEMY_NO_RANDOM_SPAWN | ENEMY_MINIBOSS | ENEMY_BOSS)) continue;
		if (!(d->maps & STAGE->mapBit)) continue;   // only this stage's enemies
		if (in_pool(i) || d->spawnAfter > g.time) continue;
		cand[n++] = i;
	}
	if (n && g.poolSize < ENEMY_COUNT) g.enemyPool[g.poolSize++] = cand[rand() % n];
}

// newer types are a little more common, so the mix shifts over the stage
static int pool_pick(void)
{
	float total = 0;
	for (int i = 0; i < g.poolSize; i++) total += 1.0f + i * 0.25f;
	float r = randf() * total;
	for (int i = 0; i < g.poolSize; i++) {
		r -= 1.0f + i * 0.25f;
		if (r <= 0) return g.enemyPool[i];
	}
	return g.enemyPool[g.poolSize - 1];
}

// Final Swarm: ghosts that get stronger the longer you stay (white, then black after
// 1 minute, purple after 6, red after 12).
static int final_swarm_enemy(void)
{
	static const char* const tiers[] = { "GhostRed", "GhostPurple", "GhostGreater", "Ghost" };
	static const float after[] = { 720, 360, 60, 0 };
	float t = g.time - STAGE->stageTime;
	for (int k = 0; k < 4; k++)
		if (t >= after[k]) {
			int def = game_find_enemy_def(tiers[k]);
			if (def >= 0) return def;
		}
	return pool_pick();
}

static void spawn_group(int def, int count, float minR, float maxR)
{
	vec3 c = game_random_spawn_point(minR, maxR);
	float minutes = g.time / 60.0f;
	for (int k = 0; k < count && g.numEnemies < MAX_ENEMIES - 4; k++) {
		bool elite = randf() < (0.01f + minutes * 0.0025f) * items_elite_mult();
		vec3 p = v3_add(c, v3(randrange(-3, 3), 0, randrange(-3, 3)));
		game_spawn_enemy(def, p, elite);
	}
}

static void director_update(float dt)
{
#ifdef MB_DEBUG_CALM
	return;   // dev builds: no enemies, for testing interactables
#endif
	float minutes = g.time / 60.0f;
	float diff = 1.0f + g.difficulty;

	while (g.time >= g.nextPoolTime) {
		g.nextPoolTime += STAGE->newEnemyInterval;
		pool_add_next();
	}

	// stage timeline events: type 1 = miniboss, type 2 = swarm
	for (int i = 0; i < STAGE->eventCount && i < 16; i++) {
		const StageEvent* ev = &STAGE->events[i];
		if (g.eventDone[i] || g.time < ev->minute * 60.0f) continue;
		g.eventDone[i] = true;
		if (ev->type == 1) {
			// some stages (Graveyard) leave the event empty and use the stage's miniboss list
			const int* list = ev->count ? ev->enemies : STAGE->minibosses;
			int n = ev->count ? ev->count : STAGE->minibossCount;
			if (!n) continue;
			int def = list[g.minibossToggle++ % n];
			game_spawn_enemy(def, game_random_spawn_point(28, 38), false);
			game_toast("A miniboss has appeared!");
			audio_play_sfx(SFX_BOSS);
		} else {
			if (!ev->count) continue;
			g.swarmEnemy = ev->enemies[rand() % ev->count];
			g.swarmTime = ev->duration;
			game_toast("A swarm is approaching!");
			audio_play_sfx(SFX_SWARM);
		}
	}

	if (!g.finalSwarm && g.time >= STAGE->stageTime && !g.bossDead) {
		g.finalSwarm = true;
		game_toast("FINAL SWARM! Find the boss portal!");
		audio_play_sfx(SFX_SWARM);
	}

	// alive target and spawn budget grow through the stage
	// (tuned down after hardware feedback: early minutes should feel sparse)
	int target = (int)((SPAWN_BASE_ALIVE + SPAWN_ALIVE_PER_MIN * minutes) * diff);
	float rate = (SPAWN_BASE_RATE + SPAWN_RATE_PER_MIN * minutes) * diff;
	if (g.finalSwarm) { target = MAX_ENEMIES; rate *= 3; }
	if (g.numEnemies < target / 3) rate *= 1.5f;   // mild catch-up after a big clear
	if (target > MAX_ENEMIES - 8) target = MAX_ENEMIES - 8;
	g.credits = fminf(g.credits + rate * dt, 15.0f);

	if (g.swarmEnemy >= 0) {
		g.swarmTime -= dt;
		if (g.swarmTime <= 0) g.swarmEnemy = -1;
	}

	g.spawnTimer -= dt;
	if (g.spawnTimer > 0 || g.poolSize == 0) return;
	g.spawnTimer = 0.5f;

	if (g.swarmEnemy >= 0 && g.numEnemies < target + 25)
		spawn_group(g.swarmEnemy, 3, 26, 36);   // swarms come in packs on top of the budget

	for (int guard = 0; guard < 2 && g.numEnemies < target; guard++) {
		int def = g.finalSwarm ? final_swarm_enemy() : pool_pick();
		float cost = g_enemyDefs[def].cost;
		int group = 1 + (randf() < 0.25f ? 1 + rand() % 2 : 0);
		if (g.credits < cost * group) break;
		g.credits -= cost * group;
		spawn_group(def, group, 30, 44);
	}
}

// uniform grid for enemy separation
#define GRID_N 32
#define GRID_CELL 5.0f
static s16 s_gridHead[GRID_N * GRID_N];
static s16 s_gridNext[MAX_ENEMIES];

static int grid_cell(vec3 p)
{
	int cx = (int)floorf((p.x - g.pos.x) / GRID_CELL) + GRID_N / 2;
	int cz = (int)floorf((p.z - g.pos.z) / GRID_CELL) + GRID_N / 2;
	if (cx < 0 || cz < 0 || cx >= GRID_N || cz >= GRID_N) return -1;
	return cz * GRID_N + cx;
}

static void enemy_kill(int idx);

static void enemies_update(float dt)
{
	memset(s_gridHead, 0xFF, sizeof(s_gridHead));
	for (int i = 0; i < MAX_ENEMIES; i++) {
		if (!g.enemies[i].active) continue;
		int c = grid_cell(g.enemies[i].pos);
		if (c < 0) continue;
		s_gridNext[i] = s_gridHead[c];
		s_gridHead[c] = i;
	}

	float dmgMult = game_enemy_dmg_mult();
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Enemy* e = &g.enemies[i];
		if (!e->active) continue;
		const EnemyDef* d = &g_enemyDefs[e->def];
		float radius = d->radius * e->scale;

		if (e->poisonTime > 0) {
			e->poisonTime -= dt;
			e->hp -= e->poisonDps * dt;
			if (e->hp <= 0) { enemy_kill(i); continue; }
			if (e->poisonTime <= 0) e->poisonDps = 0;
		}
		if (e->hitFlash > 0) e->hitFlash -= dt;
		if (e->attackCd > 0) e->attackCd -= dt;
		if (e->freezeTime > 0) { e->freezeTime -= dt; continue; }
		if (attacks_enemy_update(i, dt)) { e->animTime += dt * 0.3f; continue; }
		if (e->launched > 0) {   // Eagle Claw: tossed up, helpless until it lands
			e->launched -= dt;
			if (!(d->flags & ENEMY_FLYING)) e->pos.y = 3.0f * sinf(fmaxf(e->launched, 0) / 0.8f * (float)M_PI);
			continue;
		}

		vec3 to = v3_sub(g.pos, e->pos);
		to.y = 0;
		float dist = v3_len(to);
		vec3 dir = dist > 0.01f ? v3_scale(to, 1.0f / dist) : v3(0, 0, 0);

		// far away: relocate near the player (the game teleports stragglers too)
		if (dist > 75.0f && !(d->flags & ENEMY_BOSS)) {
			e->pos = game_random_spawn_point(38, 50);
			continue;
		}

		float speed = d->speed * (e->elite ? 1.1f : 1.0f);
		vec3 move = v3_scale(dir, speed);

		// separation from neighbours
		int c = grid_cell(e->pos);
		if (c >= 0) {
			int cx = c % GRID_N, cz = c / GRID_N;
			for (int oz = -1; oz <= 1; oz++)
				for (int ox = -1; ox <= 1; ox++) {
					int x = cx + ox, z = cz + oz;
					if (x < 0 || z < 0 || x >= GRID_N || z >= GRID_N) continue;
					for (int j = s_gridHead[z * GRID_N + x]; j >= 0; j = s_gridNext[j]) {
						if (j == i) continue;
						Enemy* o = &g.enemies[j];
						float rr = radius + g_enemyDefs[o->def].radius * o->scale;
						float dx = e->pos.x - o->pos.x, dz = e->pos.z - o->pos.z;
						float d2 = dx * dx + dz * dz;
						if (d2 < rr * rr && d2 > 1e-4f) {
							float dd = sqrtf(d2);
							float push = (rr - dd) / rr * 9.0f;
							move.x += dx / dd * push;
							move.z += dz / dd * push;
						}
					}
				}
		}

		e->pos = v3_add(e->pos, v3_scale(v3_add(move, e->vel), dt));
		float decay = expf(-dt * 8.0f);
		e->vel = v3_scale(e->vel, decay);
		if (d->flags & ENEMY_FLYING)
			e->pos.y = lerpf(e->pos.y, g.pos.y + 1.5f + sinf(e->animTime * 2) * 0.4f, dt * 2);
		else
			e->pos.y = 0;
		e->pos.x = clampf(e->pos.x, -MAP_HALF, MAP_HALF);
		e->pos.z = clampf(e->pos.z, -MAP_HALF, MAP_HALF);

		if (dist > 0.1f) {
			float target = yaw_of(dir);
			e->yaw += angle_diff(e->yaw, target) * clampf(dt * 8, 0, 1);
		}
		e->animTime += dt;

		// contact damage
		if (dist < radius + PLAYER_RADIUS && fabsf(e->pos.y - g.pos.y) < 3.5f && e->attackCd <= 0) {
			e->attackCd = 1.0f;
			g.lastAttacker = i;
			game_damage_player(d->damage * dmgMult * ENEMY_CONTACT_SCALE);
		}
	}
}

int game_nearest_enemy(vec3 from, float maxDist, int exclude)
{
	int best = -1;
	float bestD = maxDist * maxDist;
	for (int i = 0; i < MAX_ENEMIES; i++) {
		if (!g.enemies[i].active || i == exclude) continue;
		float d = dist2_xz(from, g.enemies[i].pos);
		if (d < bestD) { bestD = d; best = i; }
	}
	return best;
}

void game_spawn_pickup(PickupKind kind, vec3 pos, float value)
{
	int slot = -1;
	for (int i = 0; i < MAX_PICKUPS; i++)
		if (!g.pickups[i].active) { slot = i; break; }
	if (slot < 0) {
		if (kind == PICKUP_GOLD) { g.gold += (int)value; return; }
		if (kind != PICKUP_XP) return;
		// full: merge into the nearest xp gem
		float best = 1e9f;
		for (int i = 0; i < MAX_PICKUPS; i++) {
			if (g.pickups[i].kind != PICKUP_XP) continue;
			float d = dist2_xz(pos, g.pickups[i].pos);
			if (d < best) { best = d; slot = i; }
		}
		if (slot >= 0) g.pickups[slot].value += value;
		return;
	}
	Pickup* p = &g.pickups[slot];
	p->active = true;
	p->kind = kind;
	p->attracted = false;
	p->pos = v3(pos.x, 0.6f, pos.z);
	p->value = value;
	p->bob = randf() * 6.28f;
}

void game_attract_all_xp(void)
{
	for (int j = 0; j < MAX_PICKUPS; j++)
		if (g.pickups[j].active && (g.pickups[j].kind == PICKUP_XP || g.pickups[j].kind == PICKUP_GOLD))
			g.pickups[j].attracted = true;
}

static void enemy_kill(int idx)
{
	Enemy* e = &g.enemies[idx];
	const EnemyDef* d = &g_enemyDefs[e->def];
	e->active = false;
	g.numEnemies--;
	g.kills++;
	fx_spawn(FX_DEATH, e->pos, d->radius * e->scale * 1.4f, 0.35f, COLOR(1, 1, 1));
	audio_play_sfx(SFX_ENEMY_DIE);

	float xp = (float)d->xp;
	if (e->elite) xp *= 10;
	if (d->flags & ENEMY_MINIBOSS) xp = 40;
	game_spawn_pickup(PICKUP_XP, e->pos, xp);
	if (randf() < 0.35f)
		game_spawn_pickup(PICKUP_GOLD, v3_add(e->pos, v3(0, 0, 0.8f)), fmaxf(1, roundf(g.stats.goldMult)));

	if ((e->elite && !e->noChest) || (d->flags & ENEMY_MINIBOSS))
		game_spawn_pickup(PICKUP_CHEST, e->pos, 1);
	else if (randf() < 0.012f + g.stats.luck * 0.01f)
		game_spawn_pickup(PICKUP_HEART, v3_add(e->pos, v3(0.8f, 0, 0)), 20);
	else if (randf() < 0.0025f)
		game_spawn_pickup(PICKUP_MAGNET, e->pos, 0);

	items_on_kill(idx);
	passive_on_kill(idx);

	if (d->flags & ENEMY_BOSS) {
		g.bossDead = true;
		g.bossIndex = -1;
		g.state = STATE_WIN;
		audio_play_sfx(SFX_LEVELUP);
	}
}

void game_damage_enemy_ex(int idx, float dmg, vec3 knockDir, float knock, u32 flags)
{
	Enemy* e = &g.enemies[idx];
	if (!e->active) return;
	const EnemyDef* d = &g_enemyDefs[e->def];
	bool procs = !(flags & DMG_NO_PROC);
	if (procs) dmg = items_modify_damage(idx, dmg);
	dmg *= passive_damage_mult(idx);
	bool crit = !(flags & DMG_NO_CRIT) && randf() < g.stats.critChance + g.wCrit;
	if (crit) {
		dmg *= g.stats.critMult + g.wCritDmg;
		if (items_count(ITEM_GIANTFORK) && randf() < 0.10f * items_count(ITEM_GIANTFORK))   // megacrit
			dmg *= items_megacrit_mult() / 2.0f;
	}
	e->hp -= dmg;
	e->hitFlash = 0.12f;
	float kres = (d->flags & (ENEMY_BOSS | ENEMY_MINIBOSS)) ? 0.1f : 1.0f;
	e->vel = v3_add(e->vel, v3_scale(knockDir, knock * g.stats.knockback * g.wKnock * kres * 40.0f / (d->mass + 20.0f)));
	spawn_damage_number(v3_add(e->pos, v3(0, 3.0f * e->scale, 0)), dmg, crit);
	if (e->hp <= 0) enemy_kill(idx);
	else audio_play_sfx(SFX_HIT);
	passive_on_damage_dealt(idx, dmg);
	if (procs) items_on_hit(idx, dmg, crit);
	if (procs && g.stats.lifesteal > 0 && randf() < g.stats.lifesteal) game_heal(1);  // procs never re-proc (they pass DMG_NO_PROC)
}

void game_damage_enemy(int idx, float dmg, vec3 knockDir, float knock)
{
	game_damage_enemy_ex(idx, dmg, knockDir, knock, 0);
}

void game_area_damage(vec3 center, float radius, float dmg, float knock, u32 flags)
{
	float r2 = radius * radius;
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Enemy* e = &g.enemies[i];
		if (!e->active) continue;
		float er = g_enemyDefs[e->def].radius * e->scale;
		float d2 = dist2_xz(center, e->pos);
		if (d2 < (radius + er) * (radius + er) || d2 < r2) {
			vec3 kd = v3_norm(v3(e->pos.x - center.x, 0, e->pos.z - center.z));
			game_damage_enemy_ex(i, dmg, kd, knock, flags);
		}
	}
}

void game_damage_player(float dmg)
{
	if (g.iframes > 0 || g.state != STATE_PLAYING) return;
	if (randf() < fminf(g.stats.evasion, 0.6f)) {
		spawn_damage_number(v3_add(g.pos, v3(0, 4.5f, 0)), 0, false);
		passive_on_evade();
		items_on_evade();
		return;
	}
	if (weapons_try_block()) return;   // Aegis
	if (passive_on_player_hit(&dmg)) return;   // Flex
	dmg *= 1.0f / (1.0f + g.stats.armor);
	dmg = items_on_player_damaged(dmg);
	if (dmg <= 0) return;
	g.shieldDelay = 4.0f;
	if (g.shield > 0) {
		float absorbed = fminf(g.shield, dmg);
		g.shield -= absorbed;
		dmg -= absorbed;
		if (dmg <= 0) { g.iframes = 0.5f; audio_play_sfx(SFX_HIT); return; }
	}
	g.hp -= dmg;
	g.iframes = 0.5f;
	g.hurtFlash = 0.3f;
	audio_play_sfx(SFX_PLAYER_HURT);
	if (g.hp <= 0) {
		if (items_on_fatal()) return;
		g.hp = 0;
		g.state = STATE_DEAD;
		audio_stop_music();
	}
}

// ---------------------------------------------------------------- xp / level up

void game_add_xp(float xp)
{
	g.xp += xp * g.stats.xpMult;
	while (g.xp >= g.xpNext) {
		g.xp -= g.xpNext;
		g.level++;
		g.xpNext = xp_for_level(g.level);
		g.pendingLevels++;
		passive_on_level_up();
	}
}

static Rarity roll_rarity(float bonus)
{
	float r = randf() - g.stats.luck * 0.15f - bonus;
	if (r < 0.012f) return RARITY_LEGENDARY;
	if (r < 0.05f) return RARITY_EPIC;
	if (r < 0.15f) return RARITY_RARE;
	if (r < 0.38f) return RARITY_UNCOMMON;
	return RARITY_COMMON;
}

static bool choice_equal(const LevelChoice* a, const LevelChoice* b)
{
	return a->kind == b->kind && a->index == b->index;
}

void game_roll_choices(void)
{
	for (int c = 0; c < LEVELUP_CHOICES; c++) {
		LevelChoice ch;
		for (int tries = 0; tries < 40; tries++) {
			memset(&ch, 0, sizeof(ch));
			ch.rarity = roll_rarity(0);
			float r = randf();
			if (r < 0.2f && g.numWeapons < MAX_WEAPON_SLOTS) {
				ch.kind = CHOICE_NEW_WEAPON;
				ch.index = rand() % WEAPON_COUNT;
				bool owned = false;
				for (int w = 0; w < g.numWeapons; w++) owned |= g.weapons[w].id == ch.index;
				if (owned) continue;
			} else if (r < 0.62f) {
				ch.kind = CHOICE_UPGRADE_WEAPON;
				ch.index = rand() % g.numWeapons;
				if (g.weapons[ch.index].level >= items_weapon_level_cap()) continue;
				const WeaponDef* wd = &g_weaponDefs[g.weapons[ch.index].id];
				int stats[MAX_WEAPON_UPGRADES], n = 0;
				for (int s = 0; s < wd->nups; s++) stats[n++] = wd->ups[s].stat;
				if (!n) continue;
				int want = (ch.rarity >= RARITY_RARE ? 2 : 1) + (items_count(ITEM_ANVIL) ? 1 : 0);
				if (want > MAX_CHOICE_STATS) want = MAX_CHOICE_STATS;
				if (want > n) want = n;
				// pick `want` distinct stats
				for (int k = n - 1; k > 0; k--) { int j = rand() % (k + 1); int t = stats[k]; stats[k] = stats[j]; stats[j] = t; }
				ch.nstats = want;
				for (int s = 0; s < want; s++) {
					ch.stat[s] = stats[s];
					ch.amount[s] = weapon_stat_roll(g.weapons[ch.index].id, ch.stat[s], ch.rarity);
				}
			} else {
				ch.kind = CHOICE_TOME;
				ch.index = rand() % TOME_COUNT;
				if (!g.tomeLevels[ch.index] && g.numTomes >= MAX_TOME_SLOTS) continue;
			}
			bool dup = false;
			for (int o = 0; o < c; o++) dup |= choice_equal(&g.choices[o], &ch);
			if (!dup) break;
		}
		g.choices[c] = ch;
	}
	g.choiceCursor = 0;
}

// Charge shrine reward: three stat boosts of at least the given rarity.
void game_roll_stat_choices(Rarity minRarity)
{
	for (int c = 0; c < LEVELUP_CHOICES; c++) {
		LevelChoice ch;
		for (int tries = 0; tries < 40; tries++) {
			memset(&ch, 0, sizeof(ch));
			ch.kind = CHOICE_STAT;
			ch.index = rand() % TOME_COUNT;
			if (ch.index == TOME_QUANTITY && randf() < 0.7f) continue; // keep +projectiles rare
			if (ch.index == TOME_CURSED) continue;
			ch.rarity = roll_rarity(0.1f);
			if (ch.rarity < minRarity) ch.rarity = minRarity;
			bool dup = false;
			for (int o = 0; o < c; o++) dup |= choice_equal(&g.choices[o], &ch);
			if (!dup) break;
		}
		g.choices[c] = ch;
	}
	g.choiceCursor = 0;
	g.state = STATE_LEVELUP;
	g.choiceLock = 0.45f;
}

bool tome_is_percent(int t)
{
	return t != TOME_HEALTH && t != TOME_REGEN && t != TOME_QUANTITY && t != TOME_SHIELD && t != TOME_THORNS;
}

float tome_amount(int t, Rarity r)
{
	// per-level values of the game's tomes (megabonk.org database)
	static const float base[TOME_COUNT] = {
		[TOME_DAMAGE] = 0.08f, [TOME_COOLDOWN] = 0.075f, [TOME_AGILITY] = 0.15f, [TOME_HEALTH] = 25,
		[TOME_REGEN] = 40, [TOME_ARMOR] = 0.12f, [TOME_EVASION] = 0.10f, [TOME_ATTRACTION] = 0.75f,
		[TOME_SIZE] = 0.10f, [TOME_QUANTITY] = 1, [TOME_PRECISION] = 0.07f, [TOME_XP] = 0.07f,
		[TOME_LUCK] = 0.07f, [TOME_PROJSPEED] = 0.15f, [TOME_SHIELD] = 25, [TOME_THORNS] = 15,
		[TOME_BLOODY] = 0.10f, [TOME_KNOCKBACK] = 0.20f, [TOME_GOLDEN] = 0.12f, [TOME_DURATION] = 0.15f,
		[TOME_CURSED] = 0.035f, [TOME_CHAOS] = 1.0f,
	};
	return t == TOME_QUANTITY ? 1 : base[t] * RARITY_MULT[r];
}

float tome_display(int t, float a)
{
	return tome_is_percent(t) ? a * 100 : a;
}

static void apply_tome(int t, float a);
static void apply_tome(int t, float a)
{
	PlayerStats* s = &g.stats;
	switch (t) {
	case TOME_DAMAGE:     s->damageMult += a; break;
	case TOME_COOLDOWN:   s->attackSpeed += a; break;
	case TOME_AGILITY:    s->moveSpeed += a; break;
	case TOME_HEALTH:     s->maxHp += a; g.hp += a; break;
	case TOME_REGEN:      s->regen += a / 60.0f; break;   // HP per minute
	case TOME_ARMOR:      s->armor += a; break;
	case TOME_EVASION:    s->evasion += a; break;
	case TOME_ATTRACTION: s->pickupRange += 9.0f * a; break;   // % of the base range
	case TOME_SIZE:       s->sizeMult += a; break;
	case TOME_QUANTITY:   s->extraProjectiles += (int)a; break;
	case TOME_PRECISION:  s->critChance += a; break;
	case TOME_XP:         s->xpMult += a; break;
	case TOME_LUCK:       s->luck += a; break;
	case TOME_PROJSPEED:  s->projSpeedMult += a; break;
	case TOME_SHIELD:     s->shieldMax += a; g.shield += a; break;
	case TOME_THORNS:     s->thorns += a; break;
	case TOME_BLOODY:     s->lifesteal += a; break;
	case TOME_KNOCKBACK:  s->knockback += a; break;
	case TOME_GOLDEN:     s->goldMult += a; break;
	case TOME_DURATION:   s->durationMult += a; break;
	case TOME_CURSED:     g.difficulty += a; s->xpMult += a; s->goldMult += a; break;
	case TOME_CHAOS: {
		// a random other tome's boost, scaled by this roll
		int o;
		do o = rand() % TOME_COUNT; while (o == TOME_CHAOS || o == TOME_CURSED || o == TOME_QUANTITY);
		apply_tome(o, tome_amount(o, RARITY_COMMON) * a);
		break;
	}
	}
}

void game_apply_choice(const LevelChoice* c)
{
	switch (c->kind) {
	case CHOICE_NEW_WEAPON:
		if (g.numWeapons < MAX_WEAPON_SLOTS)
			weapons_init_slot(&g.weapons[g.numWeapons++], c->index);
		break;
	case CHOICE_UPGRADE_WEAPON: {
		Weapon* w = &g.weapons[c->index];
		w->level++;
		for (int s = 0; s < c->nstats; s++) weapon_apply_upgrade(w, c->stat[s], c->amount[s]);
		break;
	}
	case CHOICE_TOME:
		if (!g.tomeLevels[c->index]) g.tomeSlots[g.numTomes++] = c->index;
		g.tomeLevels[c->index]++;
		apply_tome(c->index, tome_amount(c->index, c->rarity));
		break;
	case CHOICE_STAT:
		apply_tome(c->index, tome_amount(c->index, c->rarity));
		break;
	}
}

// ---------------------------------------------------------------- player

static void player_update(float dt)
{
	circlePosition cp;
	hidCircleRead(&cp);
	u32 held = hidKeysHeld();
	u32 down = hidKeysDown();

	// camera: C-stick (New 3DS), L/R, or d-pad
	circlePosition cs;
	hidCstickRead(&cs);
	float camInput = cs.dx / 156.0f;
	if (held & (KEY_L | KEY_DLEFT)) camInput -= 1.0f;
	if (held & (KEY_R | KEY_DRIGHT)) camInput += 1.0f;
	g.camYaw += clampf(camInput, -1, 1) * 2.6f * dt;
	if (held & KEY_DUP) g.camPitch = clampf(g.camPitch + dt, 0.1f, 1.1f);
	if (held & KEY_DDOWN) g.camPitch = clampf(g.camPitch - dt, 0.1f, 1.1f);
	if (cs.dy > 40 || cs.dy < -40) g.camPitch = clampf(g.camPitch - cs.dy / 156.0f * dt, 0.1f, 1.1f);

	float ix = cp.dx / 156.0f, iz = cp.dy / 156.0f;
	float mag = sqrtf(ix * ix + iz * iz);
	if (mag < 0.15f) { ix = iz = 0; mag = 0; }
	if (mag > 1) { ix /= mag; iz /= mag; mag = 1; }

	vec3 fwd = dir_of_yaw(g.camYaw), right = v3(cosf(g.camYaw), 0, -sinf(g.camYaw));
	vec3 wish = v3_add(v3_scale(fwd, iz), v3_scale(right, ix));
	float speed = 8.5f * g.stats.moveSpeed * items_speed_mult();
	if (passive_current() == PASSIVE_SPEEDDEMON) speed *= 1.0f + 0.5f * g.speedDemon;
	float accel = g.grounded ? 14.0f : 5.0f;
	float t = clampf(accel * dt, 0, 1);
	g.vel.x = lerpf(g.vel.x, wish.x * speed, t);
	g.vel.z = lerpf(g.vel.z, wish.z * speed, t);

	if (mag > 0.1f) {
		g.yaw += angle_diff(g.yaw, yaw_of(wish)) * clampf(dt * 12, 0, 1);
		g.moveAnim += dt * mag * 10.0f;
	} else {
		g.moveAnim += dt * 2.0f;
	}

	if (g.grounded) g.jumpsLeft = items_max_jumps();
	// animation clocks: run cycle speed follows movement speed
	float hspeed = sqrtf(g.vel.x * g.vel.x + g.vel.z * g.vel.z);
	g.animTime += dt * (hspeed > 1.0f ? clampf(hspeed / 8.5f, 0.6f, 1.8f) : 1.0f);
	g.jumpTime += dt;

	if ((down & (KEY_A | KEY_B)) && g.jumpsLeft > 0) {
		g.vel.y = JUMP_VEL * items_jump_mult() * passive_jump_mult();
		g.grounded = false;
		g.jumpsLeft--;
		g.jumpTime = 0;
		audio_play_sfx(SFX_JUMP);
	}
	g.vel.y -= GRAVITY * dt;
	float fall = passive_fall_limit();   // Float / Hoarder: hold jump to glide
	if (fall > 0 && !g.grounded && (held & (KEY_A | KEY_B)) && g.vel.y < -fall) g.vel.y = -fall;
	g.pos = v3_add(g.pos, v3_scale(g.vel, dt));
	if (g.pos.y <= 0) {
		g.pos.y = 0;
		g.vel.y = 0;
		g.grounded = true;
	}
	world_collide(&g.pos, PLAYER_RADIUS);
	g.pos.x = clampf(g.pos.x, -MAP_HALF + 1, MAP_HALF - 1);
	g.pos.z = clampf(g.pos.z, -MAP_HALF + 1, MAP_HALF - 1);

	if (g.iframes > 0) g.iframes -= dt;
	if (g.hurtFlash > 0) g.hurtFlash -= dt;
	game_heal(g.stats.regen * dt);
}

static void pickups_update(float dt)
{
	float range2 = g.stats.pickupRange * g.stats.pickupRange;
	vec3 target = v3_add(g.pos, v3(0, 1.5f, 0));
	for (int i = 0; i < MAX_PICKUPS; i++) {
		Pickup* p = &g.pickups[i];
		if (!p->active) continue;
		p->bob += dt * 3;
		float d2 = dist2_xz(p->pos, g.pos);
		if (!p->attracted && (d2 < range2 || (p->kind == PICKUP_CHEST && d2 < 9))) p->attracted = true;
		if (!p->attracted) continue;
		vec3 to = v3_sub(target, p->pos);
		float d = v3_len(to);
		if (p->kind == PICKUP_XP && items_count(ITEM_SHATTEREDKNOWLEDGE)) {
			// Shattered Knowledge: flying shards slice through enemies
			p->hitCd -= dt;
			if (p->hitCd <= 0) {
				p->hitCd = 0.25f;
				for (int j = 0; j < MAX_ENEMIES; j++) {
					Enemy* e = &g.enemies[j];
					if (!e->active) continue;
					float rr = g_enemyDefs[e->def].radius * e->scale + 0.8f;
					if (dist2_xz(e->pos, p->pos) < rr * rr)
						game_damage_enemy_ex(j, 6 * g.stats.damageMult * items_count(ITEM_SHATTEREDKNOWLEDGE),
						                     v3(0, 0, 0), 0, DMG_NO_PROC);
				}
			}
		}
		if (d < 1.2f) {
			p->active = false;
			switch (p->kind) {
			case PICKUP_XP: game_add_xp(p->value); audio_play_sfx(SFX_XP); break;
			case PICKUP_GOLD: g.gold += (int)p->value; audio_play_sfx(SFX_GOLD); break;
			case PICKUP_HEART: game_heal(p->value); audio_play_sfx(SFX_HEAL); break;
			case PICKUP_MAGNET: game_attract_all_xp(); break;
			case PICKUP_CHEST:
				// elite / challenge chests are free item chests
				interact_open_item(items_roll(items_roll_tier(0.1f)), NULL);
				break;
			}
			continue;
		}
		float spd = p->kind == PICKUP_CHEST ? 8.0f : 22.0f + g.stats.moveSpeed * 8.5f;
		p->pos = v3_add(p->pos, v3_scale(to, fminf(spd * dt / d, 1.0f)));
	}
}

static void fx_update(float dt)
{
	for (int i = 0; i < MAX_FX; i++) {
		Fx* f = &g.fx[i];
		if (!f->active) continue;
		f->age += dt;
		if (f->age >= f->life) f->active = false;
	}
	for (int i = 0; i < MAX_DAMAGE_NUMS; i++) {
		DamageNum* d = &g.dmgNums[i];
		if (!d->active) continue;
		d->age += dt;
		d->pos.y += dt * 2.5f;
		if (d->age > 0.7f) d->active = false;
	}
}

void game_update(float dt)
{
	g.realTime += dt;
	if (g.toastTime > 0) g.toastTime -= dt;
	if (g.choiceLock > 0) g.choiceLock -= dt;
	if (g.state != STATE_PLAYING) return;

	g.time += dt;
#ifdef MB_DEBUG_SPAWN
	{
		static float respawn;
		int def = game_find_enemy_def(MB_DEBUG_SPAWN);
		bool alive = false;
		for (int i = 0; i < MAX_ENEMIES; i++) alive |= g.enemies[i].active && g.enemies[i].def == def;
		if (!alive && def >= 0 && (respawn -= dt) <= 0) {
			respawn = 2.0f;
			game_spawn_enemy(def, v3_add(g.pos, v3(0, 0, 18)), false);
		}
	}
#endif
	player_update(dt);
	shield_update(dt);
	interact_update(dt);
	director_update(dt);
	enemies_update(dt * items_enemy_time_scale());
	weapons_update(dt);
	projectiles_update(dt);
	zones_update(dt);
	attacks_update(dt);
	items_update(dt);
	passive_update(dt);
	pickups_update(dt);
	fx_update(dt);

	if (g.state == STATE_PLAYING && g.pendingLevels > 0) {
		g.pendingLevels--;
		game_roll_choices();
		g.state = STATE_LEVELUP;
		g.choiceLock = 0.45f;
		fx_spawn(FX_LEVELUP, g.pos, 3, 0.8f, COLOR(1, 0.9f, 0.3f));
		audio_play_sfx(SFX_LEVELUP);
	}
}
