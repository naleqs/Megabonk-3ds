#include "game.h"
#include "mathutil.h"
#include "audio.h"
#include <string.h>

// Names and icons come from the game; numbers are hand-tuned versions of the
// in-game descriptions (the item scripts themselves are IL2CPP code).
#define C RARITY_COMMON
#define R RARITY_RARE
#define E RARITY_EPIC
#define L RARITY_LEGENDARY
const ItemDef g_itemDefs[ITEM_COUNT] = {
	[ITEM_KEY]             = { "Key", "icon_itemkey", C, "10% chance to open chests for free" },
	[ITEM_BORGAR]          = { "Borgar", "icon_itemborgor", C, "3% chance to spawn Borgar on kill (heals)" },
	[ITEM_BOSSBUSTER]      = { "Boss Buster", "icon_itembossbuster", C, "+15% damage to Elites and Bosses" },
	[ITEM_GOLDENGLOVE]     = { "Golden Glove", "icon_itemgoldenglove", C, "Earn 15% more gold from kills" },
	[ITEM_GYMSAUCE]        = { "Gym Sauce", "icon_itemgymsauce", C, "+10% Damage" },
	[ITEM_MEDKIT]          = { "Medkit", "icon_itemmedkit", C, "+0.5 HP/s regen" },
	[ITEM_MOLDYCHEESE]     = { "Moldy Cheese", "icon_itemmoldycheese", C, "10% chance to poison enemies on hit" },
	[ITEM_OATS]            = { "Oats", "icon_itemoats", C, "+25 Max HP" },
	[ITEM_SLIPPERYRING]    = { "Slippery Ring", "icon_itemslipperyring", C, "+5% Evasion" },
	[ITEM_TACTICALGLASSES] = { "Tactical Glasses", "icon_itemtacticalgoggles", C, "+40% damage to enemies above 90% HP" },
	[ITEM_TURBOSOCKS]      = { "Turbo Socks", "icon_itemturbosocks", C, "+8% Movement speed" },
	[ITEM_ICECRYSTAL]      = { "Ice Crystal", "icon_itemicecrystal", C, "7.5% chance to freeze enemies on hit" },
	[ITEM_TIMEBRACELET]    = { "Time Bracelet", "icon_itemtimebracelet", C, "+8% XP gain. Definitely not just a watch." },
	[ITEM_BATTERY]         = { "Battery", "icon_itembattery", C, "+8% Attack speed" },
	[ITEM_WRENCH]          = { "Wrench", "icon_itemwrench", C, "Charge Shrines charge 25% faster" },
	[ITEM_CACTUS]          = { "Cactus", "icon_itemcactus", C, "Shoots out spikes when you take damage" },
	[ITEM_CURSEDDOLL]      = { "Cursed Doll", "icon_itemcurseddoll", C, "Curse an enemy every second for 3% of its Max HP" },

	[ITEM_BACKPACK]        = { "Backpack", "icon_itembackpack", R, "+1 Projectile for all weapons" },
	[ITEM_BEER]            = { "Beer", "icon_itembeer", R, "+20% Damage, -10 Max HP" },
	[ITEM_BRASSKNUCKLES]   = { "Brass Knuckles", "icon_itembrassknuckle", R, "Deal 25% more damage to nearby enemies" },
	[ITEM_CAMPFIRE]        = { "Campfire", "icon_itemcampfire", R, "Standing still heals you" },
	[ITEM_DEMONBLADE]      = { "Demonic Blade", "icon_itemdemonblade", R, "Crits have a 10% chance to heal you" },
	[ITEM_DEMONICBLOOD]    = { "Demonic Blood", "icon_itemdemonblood", R, "+0.5% Damage per kill (max 40 stacks)" },
	[ITEM_ECHOSHARD]       = { "Echo Shard", "icon_itemechoshard", R, "10% chance to spawn an extra XP shard on kill" },
	[ITEM_ELECTRICPLUG]    = { "Electric Plug", "icon_itemelectricplug", R, "Taking damage shocks nearby enemies" },
	[ITEM_FEATHERS]        = { "Feathers", "icon_itemfeathers", R, "Jump higher. Also gain +1 Extra Jump" },
	[ITEM_GOLDENSHIELD]    = { "Golden Shield", "icon_itemgoldenshield", R, "Get extra gold when taking damage" },
	[ITEM_GOLDENSNEAKERS]  = { "Golden Sneakers", "icon_itemgoldensneakers", R, "Earn money by moving around" },
	[ITEM_LEECHINGCRYSTAL] = { "Leeching Crystal", "icon_itemleechingcrystal", R, "Heal for 2% of damage dealt" },
	[ITEM_THUNDERMITTS]    = { "Thunder Mitts", "icon_itemgloveslightning", R, "Hits generate a lightning strike. 2s cooldown" },
	[ITEM_MOLDYGLOVES]     = { "Moldy Gloves", "icon_itemglovespoison", R, "Hits can create a moldy poison cloud" },

	[ITEM_BEEFYRING]       = { "Beefy Ring", "icon_itembeefyring", E, "+1% Damage per 10 Max HP" },
	[ITEM_MIRROR]          = { "Mirror", "icon_itemmirror", E, "Reflect damage back and become briefly invulnerable" },
	[ITEM_SCARF]           = { "Scarf", "icon_itemscarf", E, "Deal 30% more damage while airborne" },
	[ITEM_SLUTTYCANNON]    = { "Slutty Cannon", "icon_itemsluttycannon", E, "10% chance to spawn missiles on kill" },
	[ITEM_SPIKYSHIELD]     = { "Spiky Shield", "icon_itemspikyshieldf", E, "+30 Thorns, +10% Armor" },
	[ITEM_ZAWARUDO]        = { "Za Warudo", "icon_itemzawarudo", E, "Survive a killing blow and freeze time. Breaks after use" },
	[ITEM_BOBSLANTERN]     = { "Bob's Light", "icon_itembobslantern", E, "A sphere of light explodes every 10s, killing nearby enemies" },
	[ITEM_DEMONICSOUL]     = { "Demonic Soul", "icon_itemdemonsoul", E, "+0.5 Max HP per kill (max 100)" },
	[ITEM_SLURPGLOVES]     = { "Slurp Gloves", "icon_itemglovesbloody", E, "Hits cast blood magic that heals you. 3s cooldown" },
	[ITEM_CURSEDGRABBIES]  = { "Cursed Grabbies", "icon_itemglovescursed", E, "+10% Damage, +5% Crit. Hits can cast cursed magic" },
	[ITEM_ROLLERBLADES]    = { "Turbo Skates", "icon_itemrollerblades", E, "The faster you move, the more damage (max 30%)" },

	[ITEM_ANVIL]           = { "Anvil", "icon_itemanvil", L, "Weapon Upgrades can now have +1 stats" },
	[ITEM_BONKER]          = { "Big Bonk", "icon_itembonker", L, "5% chance to BONK an enemy for 10x damage" },
	[ITEM_DRAGONFIRE]      = { "Dragonfire", "icon_itemdragonfire", L, "10% chance to summon dragonfire on hit" },
	[ITEM_ICECUBE]         = { "Ice Cube", "icon_itemicecube", L, "15% chance on hit to deal ice damage and freeze" },
	[ITEM_LIGHTNINGORB]    = { "Lightning Orb", "icon_itemlightningorb", L, "10% chance to summon lightning on hit" },
	[ITEM_SOULHARVESTER]   = { "Soul Harvester", "icon_itemsoulharvester", L, "Killing an enemy summons a homing soul" },
	[ITEM_SPICYMEATBALL]   = { "Spicy Meatball", "icon_itemspicymeatball", L, "15% chance for attacks to explode" },
	[ITEM_SUCKYMAGNET]     = { "Sucky Magnet", "icon_itemsuckyhoof", L, "Periodically drag in all XP" },
	[ITEM_POWERGLOVES]     = { "Power Gloves", "icon_itemglovespower", L, "5% chance on hit to create a giant blast" },
	[ITEM_GOLDENRING]      = { "Golden Ring", "icon_itemgoldenring", L, "It doesn't seem to do anything..." },
	[ITEM_GIANTFORK]       = { "Giant Fork", "icon_itemfork", L, "+10% Crit. Crits can become Megacrits" },
	[ITEM_ENERGYCORE]      = { "Energy Core", "icon_itemenergycore", L, "Shoot energy orbs at enemies" },
	[ITEM_HOLYBOOK]        = { "Holy Book", "icon_itemholybook", L, "+50 Max HP, +1 HP/s regen" },
};
#undef C
#undef R
#undef E
#undef L

enum { T_THUNDER, T_MIRROR, T_LANTERN, T_MAGNET, T_CORE, T_DOLL, T_SLURP, T_LEECH };

int items_count(ItemId id) { return g.itemCounts[id]; }

static void apply_passive(ItemId id, float sign)
{
	PlayerStats* s = &g.stats;
	switch (id) {
	case ITEM_GOLDENGLOVE:    s->goldMult += 0.15f * sign; break;
	case ITEM_GYMSAUCE:       s->damageMult += 0.10f * sign; break;
	case ITEM_MEDKIT:         s->regen += 0.5f * sign; break;
	case ITEM_OATS:           s->maxHp += 25 * sign; if (sign > 0) g.hp += 25; break;
	case ITEM_SLIPPERYRING:   s->evasion += 0.05f * sign; break;
	case ITEM_TURBOSOCKS:     s->moveSpeed += 0.08f * sign; break;
	case ITEM_TIMEBRACELET:   s->xpMult += 0.08f * sign; break;
	case ITEM_BATTERY:        s->attackSpeed += 0.08f * sign; break;
	case ITEM_BACKPACK:       s->extraProjectiles += (int)sign; break;
	case ITEM_BEER:           s->damageMult += 0.20f * sign; s->maxHp -= 10 * sign; break;
	case ITEM_SPIKYSHIELD:    s->thorns += 30 * sign; s->armor += 0.1f * sign; break;
	case ITEM_CURSEDGRABBIES: s->damageMult += 0.10f * sign; s->critChance += 0.05f * sign; break;
	case ITEM_GIANTFORK:      s->critChance += 0.10f * sign; break;
	case ITEM_HOLYBOOK:       s->maxHp += 50 * sign; s->regen += 1.0f * sign; if (sign > 0) g.hp += 50; break;
	default: break;
	}
	if (g.hp > s->maxHp) g.hp = s->maxHp;
}

void items_give(ItemId id)
{
	if (g.itemCounts[id] == 0 && g.numItemKinds < MAX_OWNED_ITEMS)
		g.itemOrder[g.numItemKinds++] = id;
	if (g.itemCounts[id] < 255) g.itemCounts[id]++;
	apply_passive(id, 1);
}

bool items_take(ItemId id)
{
	if (!g.itemCounts[id]) return false;
	g.itemCounts[id]--;
	apply_passive(id, -1);
	if (!g.itemCounts[id]) {
		for (int i = 0; i < g.numItemKinds; i++)
			if (g.itemOrder[i] == id) {
				memmove(&g.itemOrder[i], &g.itemOrder[i + 1], g.numItemKinds - i - 1);
				g.numItemKinds--;
				break;
			}
	}
	return true;
}

int items_roll(u8 rarity)
{
	int cand[ITEM_COUNT], n = 0;
	for (int i = 0; i < ITEM_COUNT; i++)
		if (g_itemDefs[i].rarity == rarity) cand[n++] = i;
	return n ? cand[rand() % n] : 0;
}

u8 items_roll_tier(float luckBonus)
{
	float r = randf() - g.stats.luck * 0.1f - luckBonus;
	if (r < 0.03f) return RARITY_LEGENDARY;
	if (r < 0.12f) return RARITY_EPIC;
	if (r < 0.40f) return RARITY_RARE;
	return RARITY_COMMON;
}

// ---------------------------------------------------------------- hooks

static int n_(ItemId id) { return g.itemCounts[id]; }

static bool is_tough(int idx)
{
	const Enemy* e = &g.enemies[idx];
	return e->elite || (g_enemyDefs[e->def].flags & (ENEMY_BOSS | ENEMY_MINIBOSS));
}

float items_modify_damage(int idx, float dmg)
{
	const Enemy* e = &g.enemies[idx];
	float mult = 1.0f;
	if (n_(ITEM_BOSSBUSTER) && is_tough(idx)) mult += 0.15f * n_(ITEM_BOSSBUSTER);
	if (n_(ITEM_TACTICALGLASSES) && e->hp > e->maxHp * 0.9f) mult += 0.40f * n_(ITEM_TACTICALGLASSES);
	if (n_(ITEM_BRASSKNUCKLES) && dist2_xz(e->pos, g.pos) < 8 * 8) mult += 0.25f * n_(ITEM_BRASSKNUCKLES);
	if (n_(ITEM_DEMONICBLOOD)) mult += 0.005f * fminf(g.killStacks, 40.0f * n_(ITEM_DEMONICBLOOD));
	if (n_(ITEM_BEEFYRING)) mult += 0.001f * g.stats.maxHp * n_(ITEM_BEEFYRING);
	if (n_(ITEM_SCARF) && !g.grounded) mult += 0.30f * n_(ITEM_SCARF);
	if (n_(ITEM_ROLLERBLADES)) {
		float spd = sqrtf(g.vel.x * g.vel.x + g.vel.z * g.vel.z);
		mult += fminf(spd / 30.0f, 1.0f) * 0.30f * n_(ITEM_ROLLERBLADES);
	}
	if (n_(ITEM_BONKER) && randf() < 0.05f * n_(ITEM_BONKER)) {
		mult *= 10.0f;
		fx_spawn(FX_RING, e->pos, 3.0f, 0.3f, COLOR(1, 0.85f, 0.3f));
	}
	return dmg * mult;
}

static void strike(vec3 at, float radius, float dmg, Color c, FxKind fx)
{
	game_area_damage(at, radius, dmg, 0.6f, DMG_NO_PROC);
	fx_spawn(fx, at, radius, 0.3f, c);
}

void items_on_hit(int idx, float dmg, bool crit)
{
	Enemy* e = &g.enemies[idx];
	vec3 at = e->pos;
	if (n_(ITEM_MOLDYCHEESE) && randf() < 0.10f * n_(ITEM_MOLDYCHEESE)) {
		e->poisonTime = 3.0f;
		e->poisonDps = fmaxf(e->poisonDps, dmg * 0.35f);
	}
	if (n_(ITEM_ICECRYSTAL) && randf() < 0.075f * n_(ITEM_ICECRYSTAL)) e->freezeTime = 1.5f;
	if (n_(ITEM_ICECUBE) && randf() < 0.15f * n_(ITEM_ICECUBE)) {
		e->freezeTime = 2.0f;
		strike(at, 2.0f, 20 * g.stats.damageMult, COLOR(0.6f, 0.9f, 1), FX_RING);
	}
	if (n_(ITEM_LEECHINGCRYSTAL) && g.itemTimers[T_LEECH] < 6.0f) {
		float h = dmg * 0.02f * n_(ITEM_LEECHINGCRYSTAL);
		g.itemTimers[T_LEECH] += h;
		game_heal(h);
	}
	if (crit && n_(ITEM_DEMONBLADE) && randf() < 0.10f * n_(ITEM_DEMONBLADE)) game_heal(5);
	if (n_(ITEM_THUNDERMITTS) && g.itemTimers[T_THUNDER] <= 0) {
		g.itemTimers[T_THUNDER] = 2.0f / n_(ITEM_THUNDERMITTS);
		strike(at, 2.5f, 40 * g.stats.damageMult, COLOR(0.5f, 0.85f, 1), FX_LIGHTNING);
	}
	if (n_(ITEM_LIGHTNINGORB) && randf() < 0.10f * n_(ITEM_LIGHTNINGORB))
		strike(at, 3.0f, 50 * g.stats.damageMult, COLOR(0.6f, 0.8f, 1), FX_LIGHTNING);
	if (n_(ITEM_MOLDYGLOVES) && randf() < 0.10f * n_(ITEM_MOLDYGLOVES)) {
		for (int i = 0; i < MAX_ENEMIES; i++) {
			Enemy* o = &g.enemies[i];
			if (o->active && dist2_xz(o->pos, at) < 16) { o->poisonTime = 3; o->poisonDps = fmaxf(o->poisonDps, 8 * g.stats.damageMult); }
		}
		fx_spawn(FX_POISON, at, 4.0f, 0.6f, COLOR(0.4f, 0.9f, 0.3f));
	}
	if (n_(ITEM_SLURPGLOVES) && g.itemTimers[T_SLURP] <= 0) {
		g.itemTimers[T_SLURP] = 3.0f / n_(ITEM_SLURPGLOVES);
		strike(at, 5.0f, 25 * g.stats.damageMult, COLOR(0.9f, 0.1f, 0.2f), FX_EXPLOSION);
		game_heal(3);
	}
	if (n_(ITEM_CURSEDGRABBIES) && randf() < 0.10f)
		strike(at, 4.0f, 30 * g.stats.damageMult * n_(ITEM_CURSEDGRABBIES), COLOR(0.6f, 0.1f, 0.8f), FX_EXPLOSION);
	if (n_(ITEM_DRAGONFIRE) && randf() < 0.10f * n_(ITEM_DRAGONFIRE))
		strike(at, 4.0f, dmg * 0.6f, COLOR(1, 0.4f, 0.05f), FX_EXPLOSION);
	if (n_(ITEM_SPICYMEATBALL) && randf() < 0.15f * n_(ITEM_SPICYMEATBALL))
		strike(at, 5.0f, dmg * 0.5f, COLOR(1, 0.3f, 0.1f), FX_EXPLOSION);
	if (n_(ITEM_POWERGLOVES) && randf() < 0.05f * n_(ITEM_POWERGLOVES)) {
		game_area_damage(at, 6.0f, 100 * g.stats.damageMult, 4.0f, DMG_NO_PROC);
		fx_spawn(FX_EXPLOSION, at, 6.0f, 0.35f, COLOR(1, 0.9f, 0.6f));
	}
}

void items_on_kill(int idx)
{
	Enemy* e = &g.enemies[idx];
	if (n_(ITEM_DEMONICBLOOD) || n_(ITEM_DEMONICSOUL)) {
		g.killStacks++;
		if (n_(ITEM_DEMONICSOUL) && g.killStacks <= 200 * n_(ITEM_DEMONICSOUL)) {
			g.stats.maxHp += 0.5f;
			g.hp += 0.5f;
		}
	}
	if (n_(ITEM_BORGAR) && randf() < 0.03f * n_(ITEM_BORGAR)) game_spawn_pickup(PICKUP_HEART, e->pos, 25);
	if (n_(ITEM_ECHOSHARD) && randf() < 0.10f * n_(ITEM_ECHOSHARD)) game_spawn_pickup(PICKUP_XP, e->pos, 1);
	vec3 from = v3_add(e->pos, v3(0, 1.5f, 0));
	if (n_(ITEM_SOULHARVESTER))
		for (int i = 0; i < n_(ITEM_SOULHARVESTER); i++)
			game_fire_item_projectile(PROJ_SOUL, from, v3(randrange(-1, 1), 1, randrange(-1, 1)), 18, 25 * g.stats.damageMult, 4);
	if (n_(ITEM_SLUTTYCANNON) && randf() < 0.10f * n_(ITEM_SLUTTYCANNON))
		for (int i = 0; i < 2; i++)
			game_fire_item_projectile(PROJ_ROCKET, from, v3(randrange(-1, 1), 1.2f, randrange(-1, 1)), 22, 40 * g.stats.damageMult, 4);
}

float items_on_player_damaged(float dmg)
{
	if (n_(ITEM_MIRROR) && g.itemTimers[T_MIRROR] <= 0) {
		g.itemTimers[T_MIRROR] = 6.0f / n_(ITEM_MIRROR);
		game_area_damage(g.pos, 6.0f, dmg * 5, 1.5f, DMG_NO_PROC);
		fx_spawn(FX_RING, g.pos, 6.0f, 0.35f, COLOR(0.8f, 0.9f, 1));
		g.iframes = 1.0f;
		return 0;
	}
	if (n_(ITEM_ELECTRICPLUG)) {
		for (int k = 0; k < 3; k++) {
			int t = game_nearest_enemy(g.pos, 12, -1);
			if (t < 0) break;
			strike(g.enemies[t].pos, 2.0f, 30 * n_(ITEM_ELECTRICPLUG) * g.stats.damageMult, COLOR(0.5f, 0.85f, 1), FX_LIGHTNING);
		}
	}
	if (n_(ITEM_CACTUS))
		for (int k = 0; k < 8; k++) {
			float a = k * 0.785f;
			game_fire_item_projectile(PROJ_SPIKE, v3_add(g.pos, v3(0, 1.5f, 0)), v3(sinf(a), 0, cosf(a)), 30,
			                          10 * n_(ITEM_CACTUS) * g.stats.damageMult, 0.8f);
		}
	if (g.stats.thorns > 0) game_area_damage(g.pos, 3.0f, g.stats.thorns * g.stats.damageMult, 0.5f, DMG_NO_PROC);
	if (n_(ITEM_GOLDENSHIELD)) g.gold += 2 * n_(ITEM_GOLDENSHIELD);
	return dmg;
}

bool items_on_fatal(void)
{
	if (!n_(ITEM_ZAWARUDO)) return false;
	items_take(ITEM_ZAWARUDO);
	g.hp = g.stats.maxHp * 0.5f;
	g.iframes = 2.0f;
	for (int i = 0; i < MAX_ENEMIES; i++)
		if (g.enemies[i].active) g.enemies[i].freezeTime = 3.0f;
	game_toast("ZA WARUDO!");
	return true;
}

int items_max_jumps(void) { return 1 + n_(ITEM_FEATHERS); }
float items_jump_mult(void) { return 1.0f + 0.15f * n_(ITEM_FEATHERS); }

void items_update(float dt)
{
	for (int i = 0; i < 8; i++)
		if (i != T_LEECH && g.itemTimers[i] > 0) g.itemTimers[i] -= dt;
	g.itemTimers[T_LEECH] = fmaxf(0, g.itemTimers[T_LEECH] - dt * 6.0f); // ~6 HP/s leech cap

	float spd = sqrtf(g.vel.x * g.vel.x + g.vel.z * g.vel.z);
	g.stillTime = spd < 0.5f ? g.stillTime + dt : 0;
	if (n_(ITEM_CAMPFIRE) && g.stillTime > 1.0f) game_heal(2.0f * n_(ITEM_CAMPFIRE) * dt);
	if (n_(ITEM_GOLDENSNEAKERS)) {
		g.movedDist += spd * dt;
		while (g.movedDist > 25) { g.movedDist -= 25; g.gold += n_(ITEM_GOLDENSNEAKERS); }
	}
	if (n_(ITEM_BOBSLANTERN) && g.itemTimers[T_LANTERN] <= 0) {
		int n = n_(ITEM_BOBSLANTERN);
		g.itemTimers[T_LANTERN] = fmaxf(3.0f, 10.0f - 1.5f * (n - 1));
		float r = 7.0f + 1.5f * n;
		game_area_damage(g.pos, r, 500 * g.stats.damageMult, 2.0f, DMG_NO_PROC);
		fx_spawn(FX_EXPLOSION, g.pos, r, 0.4f, COLOR(1, 1, 0.7f));
		audio_play_sfx(SFX_EXPLOSION);
	}
	if (n_(ITEM_SUCKYMAGNET) && g.itemTimers[T_MAGNET] <= 0) {
		g.itemTimers[T_MAGNET] = 15.0f / n_(ITEM_SUCKYMAGNET);
		game_attract_all_xp();
	}
	if (n_(ITEM_ENERGYCORE) && g.itemTimers[T_CORE] <= 0) {
		int t = game_nearest_enemy(g.pos, 30, -1);
		if (t >= 0) {
			g.itemTimers[T_CORE] = 1.5f / n_(ITEM_ENERGYCORE);
			vec3 from = v3_add(g.pos, v3(0, 3, 0));
			game_fire_item_projectile(PROJ_ORB, from, v3_norm(v3_sub(g.enemies[t].pos, from)), 20, 30 * g.stats.damageMult, 3);
		}
	}
	if (n_(ITEM_CURSEDDOLL) && g.itemTimers[T_DOLL] <= 0) {
		g.itemTimers[T_DOLL] = 1.0f;
		int t = game_nearest_enemy(g.pos, 25, -1);
		if (t >= 0) {
			Enemy* e = &g.enemies[t];
			float pct = 0.03f * n_(ITEM_CURSEDDOLL);
			if (is_tough(t)) pct *= 0.1f;
			game_damage_enemy_ex(t, fmaxf(e->maxHp * pct, 1), v3(0, 0, 0), 0, DMG_NO_PROC | DMG_NO_CRIT);
		}
	}
}
