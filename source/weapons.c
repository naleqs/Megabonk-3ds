#include "game.h"
#include "terrain.h"
#include "mathutil.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>
#include "unlocks.h"

const char* const g_weaponIcons[WEAPON_COUNT] = {
	[WEAPON_FIRESTAFF] = "icon_firestaff", [WEAPON_AURA] = "icon_aura", [WEAPON_BONE] = "icon_bone",
	[WEAPON_SWORD] = "icon_sword", [WEAPON_REVOLVER] = "icon_revolver", [WEAPON_LIGHTNING] = "icon_lightningstaff",
	[WEAPON_AXE] = "icon_axe", [WEAPON_DICE] = "icon_dice",
	[WEAPON_POISONFLASK] = "icon_poison_flask", [WEAPON_AEGIS] = "icon_aegis", [WEAPON_DEXECUTIONER] = "icon_dexecutioner",
	[WEAPON_TORNADO] = "icon_tornado", [WEAPON_SNIPER] = "icon_sniper", [WEAPON_CORRUPTEDSWORD] = "icon_corruptedsword",
	[WEAPON_BANANARANG] = "icon_bananarang", [WEAPON_KATANA] = "icon_katana", [WEAPON_FROSTWALKER] = "icon_frostwalker",
	[WEAPON_SCYTHE] = "icon_scythe", [WEAPON_BOW] = "icon_bow", [WEAPON_BLACKHOLE] = "icon_blackhole",
	[WEAPON_WIRELESSDAGGER] = "icon_bluetooth_knife", [WEAPON_BLOODMAGIC] = "icon_bloodmagic",
	[WEAPON_SHOTGUN] = "icon_shotgun", [WEAPON_MINES] = "icon_mine", [WEAPON_SLUTTYROCKET] = "icon_sluttyrocket",
	[WEAPON_SPACENOODLE] = "icon_space_noodle", [WEAPON_HEROSWORD] = "icon_herosword", [WEAPON_CHUNKERS] = "icon_chunkers",
	[WEAPON_FLAMEWALKER] = "icon_firewalker", [WEAPON_DRAGONBREATH] = "icon_dragonsbreath",
};

// Names and descriptions are the game's. Damage, knockback, crit, projectile count, bounces,
// cooldown (burstTime + endCooldown) and the level-up stat lines come from the game's
// WeaponData / UpgradeData assets. Speed and duration are in this port's own units.
//   name, desc, damage, knockback, crit, projectiles, bounces, cooldown, nups, ups, speed, duration
const WeaponDef g_weaponDefs[WEAPON_COUNT] = {
	[WEAPON_FIRESTAFF] = { "Firestaff", "Shoots fireballs that explode on impact",
		10, 1.0f, 0.0f, 1, 0, 1.5f, 4,
		{ { STAT_SPEED, 0.2f }, { STAT_DAMAGE, 2.5f }, { STAT_PROJECTILES, 1.0f }, { STAT_SIZE, 0.16f } }, 26, 2.0f },
	[WEAPON_AURA] = { "Aura", "Damage enemies in an area around you",
		6, 0.5f, 0.0f, 0, 0, 0.3f, 2,
		{ { STAT_SIZE, 0.14f }, { STAT_DAMAGE, 1.4f } }, 0, 0.0f },
	[WEAPON_BONE] = { "Bone", "Throws bones that bounce off enemies",
		11, 1.5f, 0.1f, 1, 1, 1.4f, 6,
		{ { STAT_DAMAGE, 2.5f }, { STAT_PROJECTILES, 1.0f }, { STAT_SPEED, 0.25f }, { STAT_BOUNCES, 1.0f }, { STAT_CRIT, 0.12f }, { STAT_CRITDMG, 0.25f } }, 24, 3.0f },
	[WEAPON_SWORD] = { "Sword", "Slashes through multiple enemies",
		11, 2.0f, 0.0f, 1, 0, 1.25f, 4,
		{ { STAT_SIZE, 0.2f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_KNOCKBACK, 0.25f } }, 0, 0.0f },
	[WEAPON_REVOLVER] = { "Revolver", "Fires multiple bullets at enemies",
		5, 0.4f, 0.05f, 6, 0, 2.5f, 6,
		{ { STAT_DAMAGE, 2.5f }, { STAT_PROJECTILES, 1.0f }, { STAT_CRIT, 0.1f }, { STAT_CRITDMG, 0.2f }, { STAT_BOUNCES, 1.0f }, { STAT_SPEED, 0.2f } }, 55, 1.2f },
	[WEAPON_LIGHTNING] = { "Lightning Staff", "Summons lightning to smite nearby enemies",
		7, 1.0f, 0.0f, 1, 0, 1.4f, 4,
		{ { STAT_SIZE, 0.2f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_BOUNCES, 1.0f } }, 0, 0.0f },
	[WEAPON_AXE] = { "Axe", "Throws spinning axes that deal area-damage",
		6, 1.0f, 0.0f, 2, 0, 1.4f, 4,
		{ { STAT_SIZE, 0.1f }, { STAT_DAMAGE, 1.5f }, { STAT_PROJECTILES, 1.0f }, { STAT_DURATION, 0.1f } }, 22, 1.6f },
	[WEAPON_DICE] = { "Dice", "Throw dice that deal 1-6 damage. Rolling a 6 increases the weapon's Crit Chance permanently.",
		0, 2.5f, 0.1f, 1, 0, 1.5f, 6,
		{ { STAT_SPEED, 0.25f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_SIZE, 0.15f }, { STAT_CRIT, 0.1f }, { STAT_CRITDMG, 0.2f } }, 26, 3.0f },
	[WEAPON_POISONFLASK] = { "Poison Flask", "Throws toxic flasks at enemies, dealing area-damage and poisoning them.",
		1, 1.0f, 0.0f, 1, 0, 1.8f, 5,
		{ { STAT_DAMAGE, 0.5f }, { STAT_PROJECTILES, 1.0f }, { STAT_SIZE, 0.15f }, { STAT_DURATION, 0.2f }, { STAT_SPEED, 0.125f } }, 16, 3.0f },
	[WEAPON_AEGIS] = { "Aegis", "Blocks incoming attacks and sends out shockwaves",
		10, 3.0f, 0.0f, 2, 1, 1.5f, 4,
		{ { STAT_SIZE, 0.15f }, { STAT_DAMAGE, 2.0f }, { STAT_KNOCKBACK, 0.25f }, { STAT_PROJECTILES, 1.0f } }, 0, 0.0f },
	[WEAPON_DEXECUTIONER] = { "Dexecutioner", "A piercing blade. Small chance to instantly execute an enemy.",
		9, 1.0f, 0.0f, 1, 0, 1.6f, 4,
		{ { STAT_SIZE, 0.2f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_CRIT, 0.05f } }, 30, 1.2f },
	[WEAPON_TORNADO] = { "Tornado", "Sends out tornadoes, knocking enemies back",
		8, 3.0f, 0.0f, 1, 99, 1.8f, 5,
		{ { STAT_SPEED, 0.2f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_KNOCKBACK, 0.2f }, { STAT_SIZE, 0.14f } }, 9, 3.0f },
	[WEAPON_SNIPER] = { "Sniper Rifle", "Fires piercing bullets at the toughest enemy",
		22, 4.0f, 0.05f, 1, 0, 2.0f, 3,
		{ { STAT_DAMAGE, 4.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_SIZE, 0.13f } }, 110, 1.0f },
	[WEAPON_CORRUPTEDSWORD] = { "Corrupted Sword", "Slashes in both directions. Deals increased damage at low HP.",
		12, 2.0f, 0.0f, 1, 0, 1.6f, 4,
		{ { STAT_SIZE, 0.15f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_SPEED, 0.133f } }, 0, 0.0f },
	[WEAPON_BANANARANG] = { "Bananarang", "Throws bananas that return to the owner",
		9, 1.0f, 0.0f, 1, 99, 1.7f, 4,
		{ { STAT_SIZE, 0.14f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_SPEED, 0.1f } }, 22, 1.4f },
	[WEAPON_KATANA] = { "Katana", "A sharp blade that targets the closest enemy.",
		11, 1.0f, 0.0f, 1, 0, 0.9f, 5,
		{ { STAT_SIZE, 0.2f }, { STAT_DAMAGE, 2.2f }, { STAT_PROJECTILES, 1.0f }, { STAT_CRIT, 0.08f }, { STAT_CRITDMG, 0.2f } }, 0, 0.0f },
	[WEAPON_FROSTWALKER] = { "Frostwalker", "Freezes and damages enemies in an area around you",
		9, 0.5f, 0.0f, 0, 0, 1.25f, 3,
		{ { STAT_SIZE, 0.1f }, { STAT_DAMAGE, 2.0f }, { STAT_DURATION, 0.075f } }, 0, 1.0f },
	[WEAPON_SCYTHE] = { "Scythe", "Slash in a circle around you. Killing Elite Enemies charges a stronger attack.",
		11, 1.0f, 0.0f, 1, 0, 2.35f, 4,
		{ { STAT_SIZE, 0.12f }, { STAT_DAMAGE, 1.5f }, { STAT_PROJECTILES, 1.0f }, { STAT_KNOCKBACK, 0.15f } }, 0, 0.0f },
	[WEAPON_BOW] = { "Bow", "Shoots arrows that pierce enemies",
		9, 0.5f, 0.0f, 1, 99, 0.92f, 6,
		{ { STAT_SPEED, 0.2f }, { STAT_DAMAGE, 1.75f }, { STAT_PROJECTILES, 1.0f }, { STAT_SIZE, 0.15f }, { STAT_CRIT, 0.08f }, { STAT_CRITDMG, 0.18f } }, 45, 1.2f },
	[WEAPON_BLACKHOLE] = { "Black Hole", "Shoots a black hole that pulls enemies in.",
		4.5, 6.0f, 0.0f, 1, 0, 2.5f, 4,
		{ { STAT_DAMAGE, 1.25f }, { STAT_PROJECTILES, 1.0f }, { STAT_SIZE, 0.13f }, { STAT_DURATION, 0.017f } }, 10, 3.0f },
	[WEAPON_WIRELESSDAGGER] = { "Wireless Dagger", "Throw smart daggers at your enemies. Wireless tech goes crazy these days.",
		9, 0.4f, 0.0f, 1, 1, 1.6f, 4,
		{ { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_BOUNCES, 1.0f }, { STAT_SPEED, 0.15f } }, 34, 3.0f },
	[WEAPON_BLOODMAGIC] = { "Blood Magic", "Summons blood magic. 5% chance to increase Max HP upon killing enemies.",
		9, 1.0f, 0.0f, 1, 99, 1.52f, 3,
		{ { STAT_SIZE, 0.15f }, { STAT_DAMAGE, 1.5f }, { STAT_PROJECTILES, 1.0f } }, 0, 0.0f },
	[WEAPON_SHOTGUN] = { "Shotgun", "Fires a spread shot, stronger at close range.",
		9, 3.0f, 0.05f, 1, 0, 2.0f, 5,
		{ { STAT_DAMAGE, 3.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_SIZE, 0.15f }, { STAT_CRIT, 0.1f }, { STAT_CRITDMG, 0.2f } }, 60, 0.3f },
	[WEAPON_MINES] = { "Mines", "Deploys mines around you that explode when enemies get close.",
		10, 4.0f, 0.0f, 1, 0, 2.0f, 4,
		{ { STAT_SIZE, 0.15f }, { STAT_DAMAGE, 3.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_DURATION, 0.1f } }, 0, 12.0f },
	[WEAPON_SLUTTYROCKET] = { "Slutty Rocket", "Fires homing missiles",
		11, 2.0f, 0.05f, 1, 0, 1.3f, 4,
		{ { STAT_SPEED, 0.2f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_CRIT, 0.08f } }, 22, 4.0f },
	[WEAPON_SPACENOODLE] = { "Space Noodle", "Attaches to a nearby enemy. Deals damage to enemies in-between.",
		10, 0.5f, 0.0f, 0, 0, 1.0f, 3,
		{ { STAT_SIZE, 0.2f }, { STAT_DAMAGE, 2.0f }, { STAT_DURATION, 0.05f } }, 0, 0.5f },
	[WEAPON_HEROSWORD] = { "Hero Sword", "Slashes through multiple enemies and fires a slashing projectile.",
		11, 2.0f, 0.0f, 1, 0, 1.5f, 4,
		{ { STAT_SIZE, 0.15f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_SPEED, 0.133f } }, 30, 0.8f },
	[WEAPON_CHUNKERS] = { "Chunkers", "Summons chunky rocks that orbit around you.",
		12, 2.5f, 0.0f, 3, 0, 1.5f, 5,
		{ { STAT_SIZE, 0.2f }, { STAT_DAMAGE, 3.0f }, { STAT_KNOCKBACK, 0.25f }, { STAT_PROJECTILES, 1.0f }, { STAT_SPEED, 0.233f } }, 3, 0.0f },
	[WEAPON_FLAMEWALKER] = { "Flamewalker", "Leaves a trail of fire behind you",
		5, 0.0f, 0.0f, 1, 0, 1.0f, 4,
		{ { STAT_SIZE, 0.15f }, { STAT_DAMAGE, 2.0f }, { STAT_PROJECTILES, 1.0f }, { STAT_DURATION, 0.12f } }, 0, 3.0f },
	[WEAPON_DRAGONBREATH] = { "Dragon's Breath", "Fires a fierce dragon's breath in a cone towards nearby enemies.",
		16, 0.5f, 0.0f, 0, 0, 2.0f, 3,
		{ { STAT_SIZE, 0.15f }, { STAT_DAMAGE, 4.0f }, { STAT_DURATION, 0.133f } }, 0, 1.5f },
};

static const char* const s_statNames[STAT_COUNT] = {
	"Damage", "Size", "Duration", "Proj. Speed", "Projectiles", "Bounces", "Crit", "Crit Dmg", "Knockback",
};

const char* weapon_stat_name(WeaponStat s) { return s_statNames[s]; }

// The upgrade's base amount from UpgradeData, scaled by rarity.
float weapon_stat_roll(WeaponId id, WeaponStat s, Rarity r)
{
	static const float mult[RARITY_COUNT] = { 1.0f, 1.2f, 1.4f, 1.7f, 2.0f };
	const WeaponDef* d = &g_weaponDefs[id];
	float base = 0;
	for (int i = 0; i < d->nups; i++)
		if (d->ups[i].stat == s) base = d->ups[i].amount;
	if (s == STAT_PROJECTILES || s == STAT_BOUNCES) return r >= RARITY_EPIC ? 2 : 1;
	return base * mult[r];
}

void weapon_format_stat(WeaponStat s, float amount, char* buf, size_t n)
{
	if (s == STAT_PROJECTILES || s == STAT_BOUNCES)
		snprintf(buf, n, "+%d %s", (int)amount, s_statNames[s]);
	else if (s == STAT_DAMAGE)
		snprintf(buf, n, "+%.1f %s", amount, s_statNames[s]);
	else
		snprintf(buf, n, "+%d%% %s", (int)(amount * 100 + 0.5f), s_statNames[s]);
}

void weapon_apply_upgrade(Weapon* w, WeaponStat stat, float amount)
{
	switch (stat) {
	case STAT_DAMAGE:      w->damage += amount; break;
	case STAT_SIZE:        w->size *= 1.0f + amount; break;
	case STAT_DURATION:    w->duration *= 1.0f + amount; break;
	case STAT_SPEED:       w->speed *= 1.0f + amount; break;
	case STAT_PROJECTILES: w->projectiles += (int)amount; break;
	case STAT_BOUNCES:     w->bounces += (int)amount; break;
	case STAT_CRIT:        w->crit += amount; break;
	case STAT_CRITDMG:     w->critDmg += amount; break;
	case STAT_KNOCKBACK:   w->knockback *= 1.0f + amount; break;
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
	w->size = 1;
	w->speed = d->speed;
	w->duration = d->duration;
	w->bounces = d->bounces;
	w->crit = d->crit;
	w->knockback = 1;
	w->timer = 0.3f;
	if (id == WEAPON_AEGIS) g.aegisCharges = w->projectiles;
}

// Damage context: per-weapon crit / knockback for everything a weapon hits.
static void weapon_ctx(const Weapon* w)
{
	g.wCrit = w ? w->crit : 0;
	g.wCritDmg = w ? w->critDmg : 0;
	g.wKnock = w ? w->knockback : 1;
	g.dmgSource = w ? w->id : DMG_SRC_NONE;
}

static Weapon* weapon_slot(int id)
{
	for (int i = 0; i < g.numWeapons; i++)
		if (g.weapons[i].id == id) return &g.weapons[i];
	return NULL;
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

static u8 s_firing = 0xFF;   // weapon currently firing / whose projectile is being updated

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
			z->weapon = s_firing;
			z->pos.y = terrain_height(pos.x, pos.z);
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
	unlocks_count(CNT_PROJECTILES_FIRED, 1);
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
			p->vel = v3(to.x / flight, 0.5f * 30.0f * flight + (g.enemies[t].pos.y - origin.y) / flight, to.z / flight);
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
				if (!e->active && randf() < 0.05f) { g.stats.maxHp += 1; g.hp += 1; unlocks_count(CNT_BLOOD_MAGIC_PROCS, 1); }
			} else {
				fx_spawn(FX_LIGHTNING, at, 2.5f * size, 0.25f, COLOR(0.5f, 0.85f, 1.0f));
			}
			any = true;
		}
		if (any) audio_play_sfx(blood ? SFX_EXPLOSION : SFX_LIGHTNING);
		return any;
	}
	case WEAPON_SHOTGUN: {
		int t = game_nearest_enemy(g.pos, 22, -1);
		if (t < 0) return false;
		vec3 dir = v3_norm(v3_sub(enemy_center(t), origin));
		int pellets = 3 + 2 * count;
		for (int i = 0; i < pellets; i++)
			fire_projectile(w, PROJ_PELLET, origin, rotate_y(dir, (i - (pellets - 1) * 0.5f) * 0.1f + randrange(-0.03f, 0.03f)));
		audio_play_sfx(SFX_GUN);
		return true;
	}
	case WEAPON_MINES:
		for (int i = 0; i < count; i++) {
			float a = randf() * 6.2832f, r = randrange(2.5f, 7.0f);
			zone_spawn(ZONE_MINE, v3(g.pos.x + sinf(a) * r, 0, g.pos.z + cosf(a) * r), 4.0f * size, dur, dmg);
		}
		return true;
	case WEAPON_SLUTTYROCKET: {
		int t = game_nearest_enemy(g.pos, 40, -1);
		if (t < 0) return false;
		vec3 dir = flat_dir_to(t);
		for (int i = 0; i < count; i++)
			fire_projectile(w, PROJ_MISSILE, origin, v3_norm(v3_add(rotate_y(dir, (i - (count - 1) * 0.5f) * 0.7f), v3(0, 0.8f, 0))));
		audio_play_sfx(SFX_THROW);
		return true;
	}
	case WEAPON_SPACENOODLE: {
		bool any = false;
		for (int s = 0; s < 1 + g.stats.extraProjectiles; s++) {
			int t = s == 0 ? game_nearest_enemy(g.pos, 18, -1) : random_enemy_near(18);
			if (t < 0) break;
			// damage everything between the player and the target
			vec3 a = g.pos, b = g.enemies[t].pos;
			vec3 ab = v3(b.x - a.x, 0, b.z - a.z);
			float len = v3_len(ab);
			if (len < 0.01f) continue;
			vec3 dir = v3_scale(ab, 1.0f / len);
			for (int i = 0; i < MAX_ENEMIES; i++) {
				Enemy* e = &g.enemies[i];
				if (!e->active) continue;
				float along = (e->pos.x - a.x) * dir.x + (e->pos.z - a.z) * dir.z;
				if (along < 0 || along > len + 1) continue;
				float side = fabsf((e->pos.x - a.x) * dir.z - (e->pos.z - a.z) * dir.x);
				if (side > 1.0f * size + g_enemyDefs[e->def].radius * e->scale) continue;
				game_damage_enemy(i, dmg, v3(dir.z, 0, -dir.x), 0.3f);
			}
			Fx* f = fx_spawn(FX_BEAM, v3_add(a, v3(0, 1.8f, 0)), len, 0.35f * dur / 0.5f, COLOR(1.0f, 0.4f, 0.8f));
			if (f) f->yaw = yaw_of(dir);
			any = true;
		}
		return any;
	}
	case WEAPON_HEROSWORD: {
		float range = 7.0f * size;
		int t = game_nearest_enemy(g.pos, 30, -1);
		vec3 facing = t >= 0 ? flat_dir_to(t) : dir_of_yaw(g.yaw);
		float yaw = yaw_of(facing);
		for (int s = 0; s < count; s++) {
			float a = yaw + (s - (count - 1) * 0.5f) * 0.35f;
			slash(a, range, 1.25f, dmg, 1.0f, COLOR(1, 0.95f, 0.6f), false);
			Projectile* p = fire_projectile(w, PROJ_WAVE, origin, dir_of_yaw(a));
			if (p) p->bounces = 99;
		}
		audio_play_sfx(SFX_SWORD);
		return true;
	}
	case WEAPON_CHUNKERS:
		return true;   // the rocks orbit continuously (weapons_update)
	case WEAPON_FLAMEWALKER:
		if (sqrtf(g.vel.x * g.vel.x + g.vel.z * g.vel.z) < 1.0f) return false;   // a trail, not a puddle
		for (int i = 0; i < count; i++) {
			vec3 at = v3(g.pos.x + randrange(-1.5f, 1.5f) * i, 0, g.pos.z + randrange(-1.5f, 1.5f) * i);
			zone_spawn(ZONE_FIRE, at, 2.5f * size, dur, dmg);
		}
		return true;
	case WEAPON_DRAGONBREATH: {
		int t = game_nearest_enemy(g.pos, 12.0f * size, -1);
		if (t < 0) return false;
		w->orbit = yaw_of(flat_dir_to(t));
		w->burstLeft = (int)(5 * dur / 1.5f + 0.5f);   // breathe over the attack duration
		w->burstTimer = 0;
		return true;
	}
	default:
		return false;
	}
}

static void dragon_breath_tick(Weapon* w)
{
	int t = game_nearest_enemy(g.pos, 12.0f * w->size * g.stats.sizeMult, -1);
	if (t >= 0) w->orbit = yaw_of(flat_dir_to(t));
	float range = 10.0f * w->size * g.stats.sizeMult;
	float dmg = w->damage * g.stats.damageMult * 0.4f;
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Enemy* e = &g.enemies[i];
		if (!e->active) continue;
		vec3 to = v3(e->pos.x - g.pos.x, 0, e->pos.z - g.pos.z);
		float d = v3_len(to);
		if (d > range + g_enemyDefs[e->def].radius * e->scale) continue;
		if (d > 1.5f && fabsf(angle_diff(w->orbit, yaw_of(to))) > 0.45f) continue;
		game_damage_enemy(i, dmg, v3_norm(to), 0.2f);
	}
	vec3 dir = dir_of_yaw(w->orbit);
	for (int k = 1; k <= 3; k++)
		fx_spawn(FX_EXPLOSION, v3_add(g.pos, v3(dir.x * range * k / 3.5f, 1.6f, dir.z * range * k / 3.5f)),
		         0.6f + 0.5f * k * w->size, 0.3f, COLOR(1, 0.45f + 0.1f * k, 0.1f));
}

static float s_orbitTick;

int weapons_orbiters(vec3* out, int max)
{
	Weapon* w = weapon_slot(WEAPON_CHUNKERS);
	if (!w) return 0;
	int n = w->projectiles + g.stats.extraProjectiles;
	if (n > max) n = max;
	float r = 4.5f * w->size * g.stats.sizeMult;
	for (int k = 0; k < n; k++) {
		float a = w->orbit + k * 6.2832f / n;
		out[k] = v3(g.pos.x + sinf(a) * r, g.pos.y + 1.5f, g.pos.z + cosf(a) * r);
	}
	return n;
}

static void chunkers_update(Weapon* w, float dt)
{
	w->orbit += dt * w->speed * g.stats.projSpeedMult;
	s_orbitTick -= dt;
	if (s_orbitTick > 0) return;
	s_orbitTick = 0.2f;
	vec3 rocks[12];
	int n = weapons_orbiters(rocks, 12);
	float rr = 1.3f * w->size * g.stats.sizeMult;
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Enemy* e = &g.enemies[i];
		if (!e->active || e->hitFlash > 0) continue;   // hitFlash doubles as a short per-enemy hit cooldown
		float er = g_enemyDefs[e->def].radius * e->scale + rr;
		for (int k = 0; k < n; k++)
			if (dist2_xz(e->pos, rocks[k]) < er * er) {
				game_damage_enemy(i, w->damage * g.stats.damageMult, v3_norm(v3(e->pos.x - g.pos.x, 0, e->pos.z - g.pos.z)), 1.2f);
				break;
			}
	}
}

bool weapons_try_block(void)
{
	if (g.aegisCharges <= 0) return false;
	for (int i = 0; i < g.numWeapons; i++) {
		Weapon* w = &g.weapons[i];
		if (w->id != WEAPON_AEGIS) continue;
		g.aegisCharges--;
		unlocks_count(CNT_AEGIS_BLOCKS, 1);
		float r = 6.0f * w->size * g.stats.sizeMult;
		weapon_ctx(w);
		game_area_damage(g.pos, r, w->damage * g.stats.damageMult, 2.5f, 0);
		weapon_ctx(NULL);
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
		weapon_ctx(w);
		s_firing = w->id;
		if (w->id == WEAPON_CHUNKERS) chunkers_update(w, dt);
		if (w->id == WEAPON_DRAGONBREATH && w->burstLeft > 0) {
			w->burstTimer -= dt;
			if (w->burstTimer <= 0) {
				w->burstTimer = 0.3f;
				w->burstLeft--;
				dragon_breath_tick(w);
				if (w->burstLeft == 4) audio_play_sfx(SFX_FIREBALL);
			}
		}
		// revolver burst
		if (w->id == WEAPON_REVOLVER && w->burstLeft > 0) {
			w->burstTimer -= dt;
			if (w->burstTimer <= 0) {
				w->burstTimer = 0.12f;
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
		w->timer -= dt * g.stats.attackSpeed * items_attack_speed_mult();
		if (w->timer <= 0) {
			if (fire_weapon(w)) w->timer = w->cooldown;
			else w->timer = 0.1f;
		}
	}
	weapon_ctx(NULL);
	s_firing = 0xFF;
}

// ---------------------------------------------------------------- zones

void zones_update(float dt)
{
	for (int i = 0; i < MAX_ZONES; i++) {
		Zone* z = &g.zones[i];
		if (!z->active) continue;
		z->age += dt;
		if (z->age >= z->life) { z->active = false; continue; }
		weapon_ctx(weapon_slot(z->weapon));
		s_firing = z->weapon;
		if (z->kind == ZONE_MINE) {
			if (z->age < 0.5f) continue;   // arming
			int t = game_nearest_enemy(z->pos, 2.0f + z->radius * 0.25f, -1);
			if (t < 0) continue;
			game_area_damage(z->pos, z->radius, z->damage, 2.0f, 0);
			fx_spawn(FX_EXPLOSION, v3_add(z->pos, v3(0, 0.5f, 0)), z->radius, 0.35f, COLOR(1, 0.6f, 0.2f));
			audio_play_sfx(SFX_EXPLOSION);
			z->active = false;
			continue;
		}
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
		z->tick = z->kind == ZONE_POISON || z->kind == ZONE_FIRE ? 0.5f : 0.3f;
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
			case ZONE_FIRE:
				game_damage_enemy(j, z->damage, kd, 0.2f);
				break;
			}
		}
	}
	weapon_ctx(NULL);
	s_firing = 0xFF;
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
		weapon_ctx(p->weapon == 0xFF ? NULL : weapon_slot(p->weapon));
		s_firing = p->weapon;
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
		if (p->kind == PROJ_SOUL || p->kind == PROJ_ROCKET || p->kind == PROJ_ORB || p->kind == PROJ_DAGGER ||
		    p->kind == PROJ_MISSILE) {
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
		float floorY = terrain_height(p->pos.x, p->pos.z) + 0.3f;
		if (p->kind == PROJ_FLASK && p->pos.y <= floorY) { flask_burst(p); continue; }
		if (p->pos.y < floorY) p->pos.y = floorY;
		if (p->kind == PROJ_FLASK || p->kind == PROJ_HOLE) continue;   // only burst on landing / timeout

		float radius = p->kind == PROJ_BULLET || p->kind == PROJ_SPIKE || p->kind == PROJ_SNIPE || p->kind == PROJ_PELLET ? 0.5f
		             : p->kind == PROJ_AXE || p->kind == PROJ_BANANA || p->kind == PROJ_BLADE || p->kind == PROJ_WAVE ? 1.6f * p->size
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
		case PROJ_PELLET:
			// stronger at close range
			game_damage_enemy(hit, p->damage * (1.5f - p->age / p->life), kd, 1.0f);
			p->active = false;
			break;
		case PROJ_MISSILE:
			game_area_damage(p->pos, 3.0f * p->size, p->damage, 0.8f, 0);
			fx_spawn(FX_EXPLOSION, p->pos, 3.0f * p->size, 0.3f, COLOR(1.0f, 0.5f, 0.7f));
			audio_play_sfx(SFX_EXPLOSION);
			p->active = false;
			break;
		case PROJ_SNIPE:
		case PROJ_BLADE:
		case PROJ_WAVE:
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
		case PROJ_DICE: {
			// 1-6 (+ flat damage upgrades); a six permanently adds 1% crit chance to the dice
			int roll = 1 + rand() % 6;
			game_damage_enemy(hit, roll * g.stats.damageMult + p->damage, kd, 0.6f);
			Weapon* w = weapon_slot(WEAPON_DICE);
			if (roll == 6 && w) w->crit += 0.01f;
			bounce_or_die(p, hit);
			break;
		}
		default:
			break;
		}
	}
	weapon_ctx(NULL);
	s_firing = 0xFF;
}
