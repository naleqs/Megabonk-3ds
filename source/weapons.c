#include "game.h"
#include "mathutil.h"
#include "audio.h"
#include <string.h>

#define U(s) (1u << (s))
#define UP_BASIC (U(STAT_DAMAGE) | U(STAT_COOLDOWN))
#define UP_PROJ  (UP_BASIC | U(STAT_PROJECTILES) | U(STAT_SPEED))

const char* const g_weaponIcons[WEAPON_COUNT] = {
	[WEAPON_FIRESTAFF] = "icon_firestaff", [WEAPON_AURA] = "icon_aura", [WEAPON_BONE] = "icon_bone",
	[WEAPON_SWORD] = "icon_sword", [WEAPON_REVOLVER] = "icon_revolver", [WEAPON_LIGHTNING] = "icon_lightningstaff",
	[WEAPON_AXE] = "icon_axe", [WEAPON_DICE] = "icon_dice",
	[WEAPON_POISONFLASK] = "icon_poison_flask", [WEAPON_AEGIS] = "icon_aegis", [WEAPON_DEXECUTIONER] = "icon_dexecutioner",
	[WEAPON_TORNADO] = "icon_tornado", [WEAPON_SNIPER] = "icon_sniper", [WEAPON_CORRUPTEDSWORD] = "icon_corruptedsword",
	[WEAPON_BANANARANG] = "icon_bananarang", [WEAPON_KATANA] = "icon_katana", [WEAPON_FROSTWALKER] = "icon_frostwalker",
	[WEAPON_SCYTHE] = "icon_scythe", [WEAPON_BOW] = "icon_bow", [WEAPON_BLACKHOLE] = "icon_blackhole",
	[WEAPON_WIRELESSDAGGER] = "icon_bluetooth_knife", [WEAPON_BLOODMAGIC] = "icon_bloodmagic",
};

// Names/descriptions are the game's; numbers are hand-tuned (IL2CPP WeaponData
// doesn't decode reliably).
//   name, desc, damage, cooldown, projectiles, size, speed, duration, bounces, upgrade mask
const WeaponDef g_weaponDefs[WEAPON_COUNT] = {
	[WEAPON_FIRESTAFF] = { "Firestaff", "Shoots exploding fireballs", 14, 1.3f, 1, 1, 26, 2.0f, 0, UP_PROJ | U(STAT_SIZE) },
	[WEAPON_AURA] = { "Aura", "Damages nearby enemies", 6, 0.6f, 0, 1, 0, 0, 0, UP_BASIC | U(STAT_SIZE) },
	[WEAPON_BONE] = { "Bone", "Bouncing bone", 12, 1.1f, 1, 1, 24, 3.0f, 2, UP_PROJ | U(STAT_BOUNCES) | U(STAT_SIZE) },
	[WEAPON_SWORD] = { "Sword", "Slashes in front of you", 18, 1.2f, 1, 1, 0, 0, 0, UP_BASIC | U(STAT_PROJECTILES) | U(STAT_SIZE) },
	[WEAPON_REVOLVER] = { "Revolver", "Fires a quick burst", 9, 1.0f, 2, 1, 55, 1.2f, 0, UP_PROJ },
	[WEAPON_LIGHTNING] = { "Lightning Staff", "Strikes random enemies", 22, 1.8f, 1, 1, 0, 0, 0, UP_BASIC | U(STAT_PROJECTILES) | U(STAT_SIZE) },
	[WEAPON_AXE] = { "Axe", "Returns like a boomerang", 16, 1.8f, 1, 1, 22, 1.6f, 0, UP_PROJ | U(STAT_SIZE) | U(STAT_DURATION) },
	[WEAPON_DICE] = { "Dice", "Random damage, bounces", 7, 1.4f, 1, 1, 26, 3.0f, 3, UP_BASIC | U(STAT_PROJECTILES) | U(STAT_BOUNCES) },
	[WEAPON_POISONFLASK] = { "Poison Flask", "Throws toxic flasks at enemies, dealing area-damage and poisoning them.",
		8, 1.5f, 1, 1, 16, 3.0f, 0, UP_BASIC | U(STAT_PROJECTILES) | U(STAT_SIZE) | U(STAT_DURATION) },
	[WEAPON_AEGIS] = { "Aegis", "Blocks incoming attacks and sends out shockwaves", 25, 4.0f, 1, 1, 0, 0, 0,
		UP_BASIC | U(STAT_PROJECTILES) | U(STAT_SIZE) },
	[WEAPON_DEXECUTIONER] = { "Dexecutioner", "A piercing blade. Small chance to instantly execute an enemy.",
		20, 1.5f, 1, 1, 30, 1.2f, 0, UP_PROJ | U(STAT_SIZE) },
	[WEAPON_TORNADO] = { "Tornado", "Sends out tornadoes, knocking enemies back", 6, 2.6f, 1, 1, 9, 3.0f, 0,
		UP_BASIC | U(STAT_PROJECTILES) | U(STAT_SIZE) | U(STAT_DURATION) },
	[WEAPON_SNIPER] = { "Sniper Rifle", "Fires piercing bullets at the toughest enemy", 45, 2.0f, 1, 1, 110, 1.0f, 0, UP_PROJ },
	[WEAPON_CORRUPTEDSWORD] = { "Corrupted Sword", "Slashes in both directions. Deals increased damage at low HP.",
		16, 1.2f, 1, 1, 0, 0, 0, UP_BASIC | U(STAT_PROJECTILES) | U(STAT_SIZE) },
	[WEAPON_BANANARANG] = { "Bananarang", "Throws bananas that return to the owner", 12, 1.4f, 1, 1, 22, 1.4f, 0,
		UP_PROJ | U(STAT_SIZE) | U(STAT_DURATION) },
	[WEAPON_KATANA] = { "Katana", "A sharp blade that targets the closest enemy.", 13, 0.7f, 1, 1, 0, 0, 0,
		UP_BASIC | U(STAT_PROJECTILES) | U(STAT_SIZE) },
	[WEAPON_FROSTWALKER] = { "Frostwalker", "Freezes and damages enemies in an area around you", 8, 1.6f, 0, 1, 0, 1.0f, 0,
		UP_BASIC | U(STAT_SIZE) | U(STAT_DURATION) },
	[WEAPON_SCYTHE] = { "Scythe", "Slash in a circle around you.", 18, 1.6f, 1, 1, 0, 0, 0, UP_BASIC | U(STAT_SIZE) },
	[WEAPON_BOW] = { "Bow", "Shoots arrows that pierce enemies", 11, 0.9f, 1, 1, 45, 1.2f, 2, UP_PROJ | U(STAT_BOUNCES) },
	[WEAPON_BLACKHOLE] = { "Black Hole", "Shoots a black hole that pulls enemies in.", 5, 4.0f, 1, 1, 10, 3.0f, 0,
		UP_BASIC | U(STAT_SIZE) | U(STAT_DURATION) },
	[WEAPON_WIRELESSDAGGER] = { "Wireless Dagger", "Throw smart daggers at your enemies. Wireless tech goes crazy these days.",
		9, 1.0f, 2, 1, 34, 3.0f, 4, UP_PROJ | U(STAT_BOUNCES) },
	[WEAPON_BLOODMAGIC] = { "Blood Magic", "Summons blood magic. 5% chance to increase Max HP upon killing enemies.",
		14, 1.3f, 3, 1, 0, 0, 0, UP_BASIC | U(STAT_PROJECTILES) | U(STAT_SIZE) },
};

static const char* const s_statNames[STAT_COUNT] = {
	"Damage", "Cooldown", "Projectiles", "Size", "Proj. Speed", "Duration", "Bounces",
};

const char* weapon_stat_name(WeaponStat s) { return s_statNames[s]; }

float weapon_stat_roll(WeaponStat s, Rarity r)
{
	static const float mult[RARITY_COUNT] = { 1.0f, 1.25f, 1.5f, 1.85f, 2.3f };
	float m = mult[r];
	switch (s) {
	case STAT_DAMAGE:      return 0.20f * m;   // +x% damage
	case STAT_COOLDOWN:    return 0.08f * m;   // -x% cooldown
	case STAT_PROJECTILES: return r >= RARITY_EPIC ? 2 : 1;
	case STAT_SIZE:        return 0.12f * m;
	case STAT_SPEED:       return 0.12f * m;
	case STAT_DURATION:    return 0.12f * m;
	case STAT_BOUNCES:     return r >= RARITY_RARE ? 2 : 1;
	default:               return 0;
	}
}

void weapon_apply_upgrade(Weapon* w, WeaponStat stat, float amount)
{
	switch (stat) {
	case STAT_DAMAGE:      w->damage *= 1.0f + amount; break;
	case STAT_COOLDOWN:    w->cooldown *= 1.0f - amount; if (w->cooldown < 0.12f) w->cooldown = 0.12f; break;
	case STAT_PROJECTILES: w->projectiles += (int)amount; break;
	case STAT_SIZE:        w->size *= 1.0f + amount; break;
	case STAT_SPEED:       w->speed *= 1.0f + amount; break;
	case STAT_DURATION:    w->duration *= 1.0f + amount; break;
	case STAT_BOUNCES:     w->bounces += (int)amount; break;
	default: break;
	}
}

void weapons_init_slot(Weapon* w, WeaponId id)
{
	const WeaponDef* d = &g_weaponDefs[id];
	memset(w, 0, sizeof(*w));
	w->id = id;
	w->level = 1;
	w->damage = d->damage;
	w->cooldown = d->cooldown;
	w->projectiles = d->projectiles;
	w->size = d->size;
	w->speed = d->speed;
	w->duration = d->duration;
	w->bounces = d->bounces;
	w->timer = 0.3f;
	if (id == WEAPON_AEGIS) g.aegisCharges = 1;
}

// ---------------------------------------------------------------- helpers

static Projectile* proj_alloc(void)
{
	for (int i = 0; i < MAX_PROJECTILES; i++)
		if (!g.projectiles[i].active) {
			Projectile* p = &g.projectiles[i];
			memset(p, 0, sizeof(*p));
			p->active = true;
			p->id = ++g.projIdCounter;
			p->lastHit = -1;
			return p;
		}
	return NULL;
}

static Zone* zone_spawn(ZoneKind kind, vec3 pos, float radius, float life, float dmg)
{
	for (int i = 0; i < MAX_ZONES; i++)
		if (!g.zones[i].active) {
			Zone* z = &g.zones[i];
			memset(z, 0, sizeof(*z));
			z->active = true;
			z->kind = kind;
			z->pos = pos;
			z->radius = radius;
			z->life = life;
			z->damage = dmg;
			return z;
		}
	return NULL;
}

static vec3 enemy_center(int idx)
{
	Enemy* e = &g.enemies[idx];
	return v3_add(e->pos, v3(0, 1.4f * e->scale, 0));
}

static Projectile* fire_projectile(Weapon* w, ProjKind kind, vec3 from, vec3 dir)
{
	Projectile* p = proj_alloc();
	if (!p) return NULL;
	p->kind = kind;
	p->weapon = w->id;
	p->pos = from;
	p->vel = v3_scale(dir, w->speed * g.stats.projSpeedMult);
	p->damage = w->damage * g.stats.damageMult;
	p->size = w->size * g.stats.sizeMult;
	p->life = w->duration * g.stats.durationMult;
	p->bounces = w->bounces;
	return p;
}

static vec3 rotate_y(vec3 d, float a)
{
	float c = cosf(a), s = sinf(a);
	return v3(d.x * c + d.z * s, d.y, -d.x * s + d.z * c);
}

static void area_damage(vec3 center, float radius, float dmg, float knock)
{
	game_area_damage(center, radius, dmg, knock, 0);
}

static vec3 flat_dir_to(int enemy)
{
	return v3_norm(v3(g.enemies[enemy].pos.x - g.pos.x, 0, g.enemies[enemy].pos.z - g.pos.z));
}

// Item-spawned projectiles (souls, missiles, energy orbs, cactus spikes): never proc items.
void game_fire_item_projectile(ProjKind kind, vec3 from, vec3 dir, float speed, float dmg, float life)
{
	Projectile* p = proj_alloc();
	if (!p) return;
	p->kind = kind;
	p->weapon = 0xFF;
	p->pos = from;
	p->vel = v3_scale(v3_norm(dir), speed);
	p->damage = dmg;
	p->size = 1;
	p->life = life;
}

// Melee arc: damages enemies within range and angle of yaw; spawns the slash effect.
static void slash(float yaw, float range, float halfAngle, float dmg, float knock, Color c, bool spin)
{
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Enemy* e = &g.enemies[i];
		if (!e->active) continue;
		vec3 to = v3(e->pos.x - g.pos.x, 0, e->pos.z - g.pos.z);
		float d = v3_len(to);
		float er = g_enemyDefs[e->def].radius * e->scale;
		if (d > range + er) continue;
		if (!spin && d > 1.5f && fabsf(angle_diff(yaw, yaw_of(to))) > halfAngle) continue;
		game_damage_enemy(i, dmg, v3_norm(to), knock);
	}
	Fx* f = fx_spawn(spin ? FX_SPIN : FX_SLASH, g.pos, range, spin ? 0.35f : 0.22f, c);
	if (f) f->yaw = yaw;
}

static int random_enemy_near(float range)
{
	int cand[32], n = 0;
	for (int i = 0; i < MAX_ENEMIES && n < 32; i++)
		if (g.enemies[i].active && dist2_xz(g.enemies[i].pos, g.pos) < range * range) cand[n++] = i;
	return n ? cand[rand() % n] : -1;
}

static bool fire_weapon(Weapon* w)
{
	int count = w->projectiles + g.stats.extraProjectiles;
	vec3 origin = v3_add(g.pos, v3(0, 2.0f, 0));
	float dmg = w->damage * g.stats.damageMult;
	float size = w->size * g.stats.sizeMult;
	float dur = w->duration * g.stats.durationMult;

	switch (w->id) {
	case WEAPON_FIRESTAFF:
	case WEAPON_BONE:
	case WEAPON_DICE:
	case WEAPON_BOW:
	case WEAPON_WIRELESSDAGGER:
	case WEAPON_DEXECUTIONER: {
		int t = game_nearest_enemy(g.pos, w->id == WEAPON_BOW ? 45 : 34, -1);
		if (t < 0) return false;
		vec3 dir = v3_norm(v3_sub(enemy_center(t), origin));
		ProjKind kind = w->id == WEAPON_FIRESTAFF ? PROJ_FIREBALL : w->id == WEAPON_BONE ? PROJ_BONE
		              : w->id == WEAPON_DICE ? PROJ_DICE : w->id == WEAPON_BOW ? PROJ_ARROW
		              : w->id == WEAPON_WIRELESSDAGGER ? PROJ_DAGGER : PROJ_BLADE;
		for (int i = 0; i < count; i++) {
			float spread = (i - (count - 1) * 0.5f) * 0.22f;
			fire_projectile(w, kind, origin, rotate_y(dir, spread));
		}
		audio_play_sfx(w->id == WEAPON_FIRESTAFF ? SFX_FIREBALL : SFX_THROW);
		return true;
	}
	case WEAPON_REVOLVER: {
		int t = game_nearest_enemy(g.pos, 40, -1);
		if (t < 0) return false;
		w->burstLeft = count;
		w->burstTimer = 0;
		return true;
	}
	case WEAPON_SNIPER: {
		// the toughest enemy in range
		int best = -1;
		for (int i = 0; i < MAX_ENEMIES; i++)
			if (g.enemies[i].active && dist2_xz(g.enemies[i].pos, g.pos) < 60 * 60 &&
			    (best < 0 || g.enemies[i].hp > g.enemies[best].hp)) best = i;
		if (best < 0) return false;
		vec3 dir = v3_norm(v3_sub(enemy_center(best), origin));
		for (int i = 0; i < count; i++)
			fire_projectile(w, PROJ_SNIPE, origin, rotate_y(dir, (i - (count - 1) * 0.5f) * 0.08f));
		audio_play_sfx(SFX_GUN);
		return true;
	}
	case WEAPON_AXE:
	case WEAPON_BANANARANG: {
		vec3 dir = dir_of_yaw(g.yaw);
		int t = game_nearest_enemy(g.pos, 30, -1);
		if (t >= 0) dir = flat_dir_to(t);
		for (int i = 0; i < count; i++)
			fire_projectile(w, w->id == WEAPON_AXE ? PROJ_AXE : PROJ_BANANA, origin,
			                rotate_y(dir, (i - (count - 1) * 0.5f) * 0.5f));
		audio_play_sfx(SFX_THROW);
		return true;
	}
	case WEAPON_POISONFLASK: {
		bool any = false;
		for (int i = 0; i < count; i++) {
			int t = random_enemy_near(25);
			if (t < 0) break;
			// lob: aim at the target with an upward arc (gravity applied in flight)
			vec3 to = v3_sub(g.enemies[t].pos, g.pos);
			float flight = 0.7f;
			Projectile* p = fire_projectile(w, PROJ_FLASK, origin, v3(0, 0, 0));
			if (!p) break;
			p->vel = v3(to.x / flight, 0.5f * 30.0f * flight + (0 - 2.0f) / flight, to.z / flight);
			p->life = flight + 0.5f;
			any = true;
		}
		if (any) audio_play_sfx(SFX_THROW);
		return any;
	}
	case WEAPON_TORNADO: {
		int t = game_nearest_enemy(g.pos, 30, -1);
		float aim = t >= 0 ? yaw_of(flat_dir_to(t)) : randf() * 6.2832f;
		for (int i = 0; i < count; i++) {
			float a = aim + (i - (count - 1) * 0.5f) * 0.6f + randrange(-0.2f, 0.2f);
			Zone* z = zone_spawn(ZONE_TORNADO, v3_add(g.pos, v3(sinf(a) * 2, 0, cosf(a) * 2)), 2.5f * size, dur, dmg);
			if (z) z->vel = v3(sinf(a) * w->speed, 0, cosf(a) * w->speed);
		}
		return true;
	}
	case WEAPON_BLACKHOLE: {
		int t = game_nearest_enemy(g.pos, 30, -1);
		vec3 dir = t >= 0 ? flat_dir_to(t) : dir_of_yaw(g.yaw);
		Projectile* p = fire_projectile(w, PROJ_HOLE, origin, dir);
		if (p) p->life = 1.0f;   // travels, then collapses into a black hole zone
		return true;
	}
	case WEAPON_AURA:
		area_damage(g.pos, 6.0f * size, dmg, 0.3f);
		return true;
	case WEAPON_FROSTWALKER: {
		float r = 6.0f * size;
		for (int i = 0; i < MAX_ENEMIES; i++) {
			Enemy* e = &g.enemies[i];
			if (!e->active || dist2_xz(e->pos, g.pos) > r * r) continue;
			e->freezeTime = fmaxf(e->freezeTime, dur);
			game_damage_enemy(i, dmg, v3(0, 0, 0), 0);
		}
		fx_spawn(FX_RING, g.pos, r, 0.4f, COLOR(0.5f, 0.85f, 1.0f));
		return true;
	}
	case WEAPON_AEGIS:
		// recharge a block charge; the shockwave fires when a hit is blocked
		if (g.aegisCharges < count) g.aegisCharges++;
		return true;
	case WEAPON_SWORD:
	case WEAPON_KATANA:
	case WEAPON_CORRUPTEDSWORD:
	case WEAPON_SCYTHE: {
		bool katana = w->id == WEAPON_KATANA;
		float range = (katana ? 6.0f : 7.0f) * size;
		vec3 facing = dir_of_yaw(g.yaw);
		int t = game_nearest_enemy(g.pos, range + 2, -1);
		if (t >= 0) facing = flat_dir_to(t);
		else if (katana) return false;   // katana only swings at a target
		float yaw = yaw_of(facing);
		if (w->id == WEAPON_SCYTHE) {
			slash(yaw, range, 0, dmg, 1.2f, COLOR(0.8f, 0.8f, 0.9f), true);
		} else if (w->id == WEAPON_CORRUPTEDSWORD) {
			float lowHp = 1.0f + (1.0f - g.hp / g.stats.maxHp);   // up to 2x at low HP
			for (int s = 0; s < count; s++) {
				slash(yaw + s * 0.6f, range, 1.25f, dmg * lowHp, 1.0f, COLOR(0.7f, 0.3f, 1.0f), false);
				slash(yaw + (float)M_PI + s * 0.6f, range, 1.25f, dmg * lowHp, 1.0f, COLOR(0.7f, 0.3f, 1.0f), false);
			}
		} else {
			for (int s = 0; s < count; s++)
				slash(yaw + (katana ? s * 0.5f : s * (float)M_PI * 2.0f / (count > 1 ? count : 1)), range,
				      katana ? 0.8f : 1.25f, dmg, katana ? 0.5f : 1.0f, COLOR(1, 1, 1), false);
		}
		audio_play_sfx(SFX_SWORD);
		return true;
	}
	case WEAPON_LIGHTNING:
	case WEAPON_BLOODMAGIC: {
		bool blood = w->id == WEAPON_BLOODMAGIC;
		bool any = false;
		for (int s = 0; s < count; s++) {
			int t = random_enemy_near(blood ? 16 : 28);
			if (t < 0) break;
			vec3 at = g.enemies[t].pos;
			Enemy* e = &g.enemies[t];
			area_damage(at, 2.5f * size, dmg, 0.5f);
			if (blood) {
				fx_spawn(FX_EXPLOSION, v3_add(at, v3(0, 1, 0)), 2.5f * size, 0.3f, COLOR(0.8f, 0.05f, 0.1f));
				if (!e->active && randf() < 0.05f) { g.stats.maxHp += 1; g.hp += 1; }
			} else {
				fx_spawn(FX_LIGHTNING, at, 2.5f * size, 0.25f, COLOR(0.5f, 0.85f, 1.0f));
			}
			any = true;
		}
		if (any) audio_play_sfx(blood ? SFX_EXPLOSION : SFX_LIGHTNING);
		return any;
	}
	default:
		return false;
	}
}

bool weapons_try_block(void)
{
	if (g.aegisCharges <= 0) return false;
	for (int i = 0; i < g.numWeapons; i++) {
		Weapon* w = &g.weapons[i];
		if (w->id != WEAPON_AEGIS) continue;
		g.aegisCharges--;
		float r = 6.0f * w->size * g.stats.sizeMult;
		game_area_damage(g.pos, r, w->damage * g.stats.damageMult, 2.5f, 0);
		fx_spawn(FX_RING, g.pos, r, 0.4f, COLOR(1.0f, 0.9f, 0.5f));
		g.iframes = 0.5f;
		audio_play_sfx(SFX_EXPLOSION);
		return true;
	}
	return false;
}

void weapons_update(float dt)
{
	for (int i = 0; i < g.numWeapons; i++) {
		Weapon* w = &g.weapons[i];
		// revolver burst
		if (w->burstLeft > 0) {
			w->burstTimer -= dt;
			if (w->burstTimer <= 0) {
				w->burstTimer = 0.09f;
				w->burstLeft--;
				int t = game_nearest_enemy(g.pos, 45, -1);
				if (t >= 0) {
					vec3 origin = v3_add(g.pos, v3(0, 2.0f, 0));
					vec3 dir = v3_norm(v3_sub(enemy_center(t), origin));
					fire_projectile(w, PROJ_BULLET, origin, rotate_y(dir, randrange(-0.05f, 0.05f)));
					audio_play_sfx(SFX_GUN);
				}
			}
		}
		w->timer -= dt * g.stats.attackSpeed;
		if (w->timer <= 0) {
			if (fire_weapon(w)) w->timer = w->cooldown;
			else w->timer = 0.1f;
		}
	}
}

// ---------------------------------------------------------------- zones

void zones_update(float dt)
{
	for (int i = 0; i < MAX_ZONES; i++) {
		Zone* z = &g.zones[i];
		if (!z->active) continue;
		z->age += dt;
		if (z->age >= z->life) { z->active = false; continue; }
		z->pos = v3_add(z->pos, v3_scale(z->vel, dt));
		if (z->kind == ZONE_TORNADO) {
			// wander a little
			z->vel = rotate_y(z->vel, randrange(-2, 2) * dt);
		}
		if (z->kind == ZONE_BLACKHOLE) {
			// pull enemies toward the centre
			for (int j = 0; j < MAX_ENEMIES; j++) {
				Enemy* e = &g.enemies[j];
				if (!e->active || (g_enemyDefs[e->def].flags & ENEMY_BOSS)) continue;
				float d2 = dist2_xz(e->pos, z->pos);
				if (d2 > z->radius * z->radius * 4 || d2 < 0.25f) continue;
				vec3 to = v3_norm(v3(z->pos.x - e->pos.x, 0, z->pos.z - e->pos.z));
				e->pos = v3_add(e->pos, v3_scale(to, 9.0f * dt));
			}
		}
		z->tick -= dt;
		if (z->tick > 0) continue;
		z->tick = z->kind == ZONE_POISON ? 0.5f : 0.3f;
		for (int j = 0; j < MAX_ENEMIES; j++) {
			Enemy* e = &g.enemies[j];
			if (!e->active) continue;
			float er = g_enemyDefs[e->def].radius * e->scale;
			float rr = z->radius + er;
			if (dist2_xz(e->pos, z->pos) > rr * rr) continue;
			vec3 kd = v3_norm(v3(e->pos.x - z->pos.x, 0, e->pos.z - z->pos.z));
			switch (z->kind) {
			case ZONE_POISON:
				e->poisonTime = 2.0f;
				e->poisonDps = fmaxf(e->poisonDps, z->damage * 0.8f);
				game_damage_enemy(j, z->damage * 0.5f, kd, 0);
				break;
			case ZONE_TORNADO:
				game_damage_enemy(j, z->damage, kd, 2.5f);
				break;
			case ZONE_BLACKHOLE:
				game_damage_enemy(j, z->damage, v3(0, 0, 0), 0);
				break;
			}
		}
	}
}

// ---------------------------------------------------------------- projectiles

static bool already_hit(Projectile* p, int idx)
{
	for (int i = 0; i < p->nhits; i++)
		if (p->hits[i] == idx) return true;
	return false;
}

static int proj_hit_enemy(Projectile* p, float radius)
{
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Enemy* e = &g.enemies[i];
		if (!e->active || i == p->lastHit || already_hit(p, i)) continue;
		float er = g_enemyDefs[e->def].radius * e->scale;
		float rr = er + radius;
		if (dist2_xz(p->pos, e->pos) > rr * rr) continue;
		float top = e->pos.y + 4.0f * e->scale;
		if (p->pos.y < e->pos.y - 1.0f || p->pos.y > top) continue;
		return i;
	}
	return -1;
}

static void bounce_or_die(Projectile* p, int hit)
{
	p->lastHit = hit;
	if (p->bounces-- > 0) {
		int next = game_nearest_enemy(p->pos, 20, hit);
		if (next >= 0) {
			float spd = v3_len(p->vel);
			p->vel = v3_scale(v3_norm(v3_sub(enemy_center(next), p->pos)), spd);
			p->age = 0;
			return;
		}
	}
	p->active = false;
}

static void flask_burst(Projectile* p)
{
	zone_spawn(ZONE_POISON, v3(p->pos.x, 0, p->pos.z), 4.0f * p->size, 3.0f * g.stats.durationMult, p->damage);
	fx_spawn(FX_POISON, v3(p->pos.x, 0, p->pos.z), 4.0f * p->size, 0.6f, COLOR(0.4f, 0.9f, 0.3f));
	p->active = false;
}

void projectiles_update(float dt)
{
	for (int i = 0; i < MAX_PROJECTILES; i++) {
		Projectile* p = &g.projectiles[i];
		if (!p->active) continue;
		p->age += dt;
		p->spin += dt * 14.0f;
		if (p->age > p->life) {
			if (p->kind == PROJ_FIREBALL) {
				area_damage(p->pos, 3.5f * p->size, p->damage, 0.8f);
				fx_spawn(FX_EXPLOSION, p->pos, 3.5f * p->size, 0.35f, COLOR(1.0f, 0.5f, 0.1f));
			} else if (p->kind == PROJ_FLASK) {
				flask_burst(p);
			} else if (p->kind == PROJ_HOLE) {
				zone_spawn(ZONE_BLACKHOLE, v3(p->pos.x, 0, p->pos.z), 3.5f * p->size, 3.0f * g.stats.durationMult, p->damage);
			}
			p->active = false;
			continue;
		}

		if (p->kind == PROJ_AXE || p->kind == PROJ_BANANA) {
			// boomerang: fly out, then home back to the player
			float half = p->life * 0.5f;
			if (p->age > half) {
				vec3 to = v3_sub(v3_add(g.pos, v3(0, 2, 0)), p->pos);
				float d = v3_len(to);
				if (d < 1.5f) { p->active = false; continue; }
				float spd = v3_len(p->vel);
				p->vel = v3_scale(v3_norm(v3_add(v3_scale(v3_norm(p->vel), 0.7f), v3_scale(v3_norm(to), 1.3f))), spd);
				p->life = p->age + 2.0f;
				p->nhits = 0;   // can hit again on the way back
			}
		}
		if (p->kind == PROJ_SOUL || p->kind == PROJ_ROCKET || p->kind == PROJ_ORB || p->kind == PROJ_DAGGER) {
			// homing: steer toward the nearest enemy after a short launch
			int t = p->age > 0.2f ? game_nearest_enemy(p->pos, 40, p->lastHit) : -1;
			if (t >= 0) {
				float spd = v3_len(p->vel);
				vec3 want = v3_norm(v3_sub(enemy_center(t), p->pos));
				p->vel = v3_scale(v3_norm(v3_add(v3_norm(p->vel), v3_scale(want, dt * 8))), spd);
			}
		}
		if (p->kind == PROJ_FLASK) p->vel.y -= 30.0f * dt;
		p->pos = v3_add(p->pos, v3_scale(p->vel, dt));
		if (p->kind == PROJ_FLASK && p->pos.y <= 0.3f) { flask_burst(p); continue; }
		if (p->pos.y < 0.3f) p->pos.y = 0.3f;
		if (p->kind == PROJ_FLASK || p->kind == PROJ_HOLE) continue;   // only burst on landing / timeout

		float radius = p->kind == PROJ_BULLET || p->kind == PROJ_SPIKE || p->kind == PROJ_SNIPE ? 0.5f
		             : p->kind == PROJ_AXE || p->kind == PROJ_BANANA || p->kind == PROJ_BLADE ? 1.6f * p->size
		             : 0.8f * p->size;
		int hit = proj_hit_enemy(p, radius);
		if (hit < 0) continue;
		vec3 kd = v3_norm(v3(p->vel.x, 0, p->vel.z));

		switch (p->kind) {
		case PROJ_SOUL:
		case PROJ_ORB:
		case PROJ_SPIKE:
			game_damage_enemy_ex(hit, p->damage, kd, 0.4f, DMG_NO_PROC);
			p->active = false;
			break;
		case PROJ_ROCKET:
			game_area_damage(p->pos, 3.5f, p->damage, 0.8f, DMG_NO_PROC);
			fx_spawn(FX_EXPLOSION, p->pos, 3.5f, 0.3f, COLOR(1.0f, 0.6f, 0.2f));
			p->active = false;
			break;
		case PROJ_FIREBALL:
			area_damage(p->pos, 3.5f * p->size, p->damage, 0.8f);
			fx_spawn(FX_EXPLOSION, p->pos, 3.5f * p->size, 0.35f, COLOR(1.0f, 0.5f, 0.1f));
			audio_play_sfx(SFX_EXPLOSION);
			p->active = false;
			break;
		case PROJ_BULLET:
			game_damage_enemy(hit, p->damage, kd, 0.4f);
			p->active = false;
			break;
		case PROJ_SNIPE:
		case PROJ_BLADE:
		case PROJ_ARROW: {
			// piercing: remember who was hit
			Enemy* e = &g.enemies[hit];
			bool exec = p->kind == PROJ_BLADE && randf() < 0.03f &&
			            !(g_enemyDefs[e->def].flags & (ENEMY_BOSS | ENEMY_MINIBOSS));
			game_damage_enemy(hit, exec ? e->hp + 1 : p->damage, kd, p->kind == PROJ_SNIPE ? 1.0f : 0.5f);
			if (p->nhits < 8) p->hits[p->nhits++] = hit;
			if (p->kind == PROJ_ARROW && p->bounces-- <= 0) p->active = false;   // bounces = pierce count
			break;
		}
		case PROJ_AXE:
		case PROJ_BANANA:
			game_damage_enemy(hit, p->damage, kd, 0.7f);
			if (p->nhits < 8) p->hits[p->nhits++] = hit;
			break;
		case PROJ_BONE:
		case PROJ_DAGGER:
			game_damage_enemy(hit, p->damage, kd, 0.6f);
			bounce_or_die(p, hit);
			break;
		case PROJ_DICE:
			game_damage_enemy(hit, p->damage * (1 + rand() % 6), kd, 0.6f);
			bounce_or_die(p, hit);
			break;
		default:
			break;
		}
	}
}
