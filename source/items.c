#include "game.h"
#include "mathutil.h"
#include "audio.h"
#include <string.h>

// Names, icons and tiers come from the game's ItemData. Numbers follow the megabonk.org
// item database where it lists them; the rest are hand-tuned (item code is IL2CPP).
#define C RARITY_COMMON
#define R RARITY_RARE
#define E RARITY_EPIC
#define L RARITY_LEGENDARY
const ItemDef g_itemDefs[ITEM_COUNT] = {
	[ITEM_KEY]             = { "Key", "icon_itemkey", C, "10% chance to open chests for free" },
	[ITEM_BORGAR]          = { "Borgar", "icon_itemborgor", C, "2% chance to spawn Borgar on kill (heals)" },
	[ITEM_BOSSBUSTER]      = { "Boss Buster", "icon_itembossbuster", C, "+15% damage to Elites and Bosses" },
	[ITEM_GOLDENGLOVE]     = { "Golden Glove", "icon_itemgoldenglove", C, "Earn 15% more gold from kills" },
	[ITEM_GYMSAUCE]        = { "Gym Sauce", "icon_itemgymsauce", C, "+10% Damage" },
	[ITEM_MEDKIT]          = { "Medkit", "icon_itemmedkit", C, "+45 HP Regen per minute" },
	[ITEM_MOLDYCHEESE]     = { "Moldy Cheese", "icon_itemmoldycheese", C, "40% chance to poison enemies on hit. Moldy Cheese my beloved <3" },
	[ITEM_OATS]            = { "Oats", "icon_itemoats", C, "+25 Max HP" },
	[ITEM_SLIPPERYRING]    = { "Slippery Ring", "icon_itemslipperyring", C, "+15% Evasion" },
	[ITEM_TACTICALGLASSES] = { "Tactical Glasses", "icon_itemtacticalgoggles", C, "+20% damage to enemies above 90% HP" },
	[ITEM_TURBOSOCKS]      = { "Turbo Socks", "icon_itemturbosocks", C, "+15% Movement speed" },
	[ITEM_ICECRYSTAL]      = { "Ice Crystal", "icon_itemicecrystal", C, "7.5% chance to freeze enemies upon hitting them" },
	[ITEM_TIMEBRACELET]    = { "Time Bracelet", "icon_itemtimebracelet", C, "+8% XP gain. Definitely not just a normal watch." },
	[ITEM_BATTERY]         = { "Battery", "icon_itembattery", C, "+8% Attack speed" },
	[ITEM_WRENCH]          = { "Wrench", "icon_itemwrench", C, "Charge Shrines charge 4% faster" },
	[ITEM_CACTUS]          = { "Cactus", "icon_itemcactus", C, "Shoots out spikes upon taking damage" },
	[ITEM_CURSEDDOLL]      = { "Cursed Doll", "icon_itemcurseddoll", C, "Curse up to 5 enemies, dealing 30% of their Max HP every second" },
	[ITEM_CLOVER]          = { "Clover", "icon_itemclover", C, "+10% Luck" },
	[ITEM_FORBIDDENJUICE]  = { "Forbidden Juice", "icon_itemshadowjuice", C, "+10% Crit Chance" },
	[ITEM_GHOST]           = { "Ghost", "icon_itemghost", C, "Using an interactable summons ghosts" },
	[ITEM_SKULEG]          = { "Skuleg", "icon_itemskuleg", C, "+7% Difficulty (tougher enemies, more XP)" },
	[ITEM_OLDMASK]         = { "Old Mask", "icon_itemoldmask", C, "+15% Elite spawns" },

	[ITEM_BACKPACK]        = { "Backpack", "icon_itembackpack", R, "+1 Projectile for all weapons" },
	[ITEM_BEER]            = { "Beer", "icon_itembeer", R, "+20% Damage, -5% Max HP" },
	[ITEM_BRASSKNUCKLES]   = { "Brass Knuckles", "icon_itembrassknuckle", R, "Deal 20% more damage to nearby enemies" },
	[ITEM_CAMPFIRE]        = { "Campfire", "icon_itemcampfire", R, "Standing still heals you" },
	[ITEM_DEMONBLADE]      = { "Demonic Blade", "icon_itemdemonblade", R, "Crits have a 25% chance to heal you" },
	[ITEM_DEMONICBLOOD]    = { "Demonic Blood", "icon_itemdemonblood", R, "+0.5 Max HP per kill (max 200 stacks)" },
	[ITEM_ECHOSHARD]       = { "Echo Shard", "icon_itemechoshard", R, "12% chance to spawn an extra XP shard when killing an enemy" },
	[ITEM_ELECTRICPLUG]    = { "Electric Plug", "icon_itemelectricplug", R, "Upon taking damage, send out an electric shock to nearby enemies" },
	[ITEM_FEATHERS]        = { "Feathers", "icon_itemfeathers", R, "Jump higher and further. Also gain +1 Extra Jump" },
	[ITEM_GOLDENSHIELD]    = { "Golden Shield", "icon_itemgoldenshield", R, "Get extra gold when taking damage" },
	[ITEM_GOLDENSNEAKERS]  = { "Golden Sneakers", "icon_itemgoldensneakers", R, "Earn money by moving around" },
	[ITEM_LEECHINGCRYSTAL] = { "Leeching Crystal", "icon_itemleechingcrystal", R, "+50% Max HP, -50% HP Regen" },
	[ITEM_THUNDERMITTS]    = { "Thunder Mitts", "icon_itemgloveslightning", R, "Hitting an enemy generates a lightning strike. 2s cooldown" },
	[ITEM_MOLDYGLOVES]     = { "Moldy Gloves", "icon_itemglovespoison", R, "Create a moldy poison cloud upon hitting an enemy" },
	[ITEM_CREDITCARDRED]   = { "Credit Card (Red)", "icon_itemcreditcardred", R, "+2.5% Damage for every chest you open" },
	[ITEM_PHANTOMSHROUD]   = { "Phantom Shroud", "icon_itemphantomshroud", R, "After evading, your next attack deals 2x damage and you get faster" },
	[ITEM_UNSTABLETRANSFUSION] = { "Unstable Transfusion", "icon_unstable_transfusion", R, "35% chance to apply a stack of Bloodmark when hitting an enemy" },
	[ITEM_IDLEJUICE]       = { "Idle Juice", "icon_idlejuice", R, "The longer you stand still, the more damage you do. Max +100% damage" },
	[ITEM_COWARDSCLOAK]    = { "Coward's Cloak", "icon_itemspeedycloak", R, "Boost Movement Speed upon taking damage. Also gain +5% Movement Speed" },

	[ITEM_BEEFYRING]       = { "Beefy Ring", "icon_itembeefyring", E, "+20% Damage per 100 Max HP" },
	[ITEM_MIRROR]          = { "Mirror", "icon_itemmirror", E, "Reflect damage back and become briefly invulnerable" },
	[ITEM_SCARF]           = { "Scarf", "icon_itemscarf", E, "Deal 50% more damage while airborne" },
	[ITEM_SLUTTYCANNON]    = { "Slutty Cannon", "icon_itemsluttycannon", E, "20% chance to spawn missiles upon killing an enemy" },
	[ITEM_SPIKYSHIELD]     = { "Spiky Shield", "icon_itemspikyshieldf", E, "+2 Thorns for every 1% Armor. Also gain +10% Armor" },
	[ITEM_BOBSLANTERN]     = { "Bob's Light", "icon_itembobslantern", E, "A sphere of light explodes every 45s, killing all nearby enemies. Stacks: -3s, +radius" },
	[ITEM_DEMONICSOUL]     = { "Demonic Soul", "icon_itemdemonsoul", E, "+0.1% Damage per kill (max +100%)" },
	[ITEM_SLURPGLOVES]     = { "Slurp Gloves", "icon_itemglovesbloody", E, "Hits cast blood magic, damaging nearby enemies and healing you. 3s cooldown" },
	[ITEM_CURSEDGRABBIES]  = { "Cursed Grabbies", "icon_itemglovescursed", E, "+10% Difficulty, -20% Max HP. 5% chance to cast cursed magic on hit" },
	[ITEM_ROLLERBLADES]    = { "Turbo Skates", "icon_itemrollerblades", E, "The faster you move, the more Attack Speed you gain. Max +40% per stack" },
	[ITEM_BOBDEAD]         = { "Bob (Dead)", "icon_itembobdead", E, "Every 30m moved, summon Bob (he's dead)" },
	[ITEM_GASMASK]         = { "Gas Mask", "icon_gasmask", E, "Take less damage for every enemy infected with poison" },
	[ITEM_EAGLECLAW]       = { "Eagle Claw", "icon_itemeagleclaw", E, "+66% damage to airborne enemies. 8% chance to knock enemies into the air" },
	[ITEM_QUINSMASK]       = { "Quin's Mask", "icon_itemquinsmask", E, "Thorns damage has a 30% chance to explode and damage nearby enemies" },
	[ITEM_CREDITCARDGREEN] = { "Credit Card (Green)", "icon_itemcreditcardgreen", E, "+1% Crit Chance for every chest you open. Also +5% Luck" },
	[ITEM_KEVIN]           = { "Kevin", "icon_kevin", E, "Kevin hates you. 25% chance he punches you when you hit an enemy. He cannot kill you" },
	[ITEM_TOXICBARREL]     = { "Toxic Barrel", "icon_toxicbarrel", E, "Release toxic waste upon taking damage, poisoning all nearby enemies" },
	[ITEM_GAMERGOGGLES]    = { "Gamer Goggles", "icon_gamerlens", E, "Increase Damage the lower your HP is. chat im locking in" },
	[ITEM_SHATTEREDKNOWLEDGE] = { "Shattered Knowledge", "icon_itemshatteredwisdom", E, "XP shards slice through enemies, dealing damage as they pass" },
	[ITEM_GRANDMASTONIC]   = { "Grandma's Secret Tonic", "icon_grandmassecrettonic", E, "Crits have a 50% chance to explode for 50% damage" },

	[ITEM_ANVIL]           = { "Anvil", "icon_itemanvil", L, "Weapon Upgrades can now have +1 stats" },
	[ITEM_BONKER]          = { "Big Bonk", "icon_itembonker", L, "2% chance to BONK an enemy for 20x damage" },
	[ITEM_DRAGONFIRE]      = { "Dragonfire", "icon_itemdragonfire", L, "15% chance to summon dragonfire when hitting enemies. Fire can now burn" },
	[ITEM_ICECUBE]         = { "Ice Cube", "icon_itemicecube", L, "20% chance to inflict ice damage when hitting enemies. Ice can now freeze" },
	[ITEM_LIGHTNINGORB]    = { "Lightning Orb", "icon_itemlightningorb", L, "25% chance to summon lightning when hitting enemies. Lightning can now stun" },
	[ITEM_SOULHARVESTER]   = { "Soul Harvester", "icon_itemsoulharvester", L, "Killing an enemy summons 2 homing souls" },
	[ITEM_SPICYMEATBALL]   = { "Spicy Meatball", "icon_itemspicymeatball", L, "Attacks have a 25% chance to explode, dealing 65% damage to nearby enemies" },
	[ITEM_SUCKYMAGNET]     = { "Sucky Magnet", "icon_itemsuckyhoof", L, "Periodically drag in all XP around you" },
	[ITEM_POWERGLOVES]     = { "Power Gloves", "icon_itemglovespower", L, "8% chance on hit to create a giant blast, knocking enemies away" },
	[ITEM_GOLDENRING]      = { "Golden Ring", "icon_itemgoldenring", L, "It doesn't seem to do anything..." },
	[ITEM_GIANTFORK]       = { "Giant Fork", "icon_itemfork", L, "+10% Crit. Crits can become Megacrits (4x damage)" },
	[ITEM_ENERGYCORE]      = { "Energy Core", "icon_itemenergycore", L, "Shoot energy orbs at enemies" },
	[ITEM_HOLYBOOK]        = { "Holy Book", "icon_itemholybook", L, "+100 Max HP, +50 HP Regen/min. Healing radiates damage to nearby enemies" },
	[ITEM_ZAWARUDO]        = { "Za Warudo", "icon_itemzawarudo", L, "Upon taking damage that would kill you, freeze time. Breaks after one use" },
	[ITEM_SPEEDBOI]        = { "Speed Boi", "icon_speed_boi", L, "When hit below 50% HP, slow down time and deal 2x damage" },
	[ITEM_JOESDAGGER]      = { "Joe's Dagger", "icon_joesdagger", L, "1% chance to Execute an enemy. Each Execute gives +1% Damage" },
	[ITEM_CHONKPLATE]      = { "Chonkplate", "icon_chonkplate", L, "+20% Lifesteal. Healing can go up to 75% above Max HP" },
	[ITEM_POT]             = { "Pot (stainless steel)", "icon_itempot", L, "More room to cook. +10 max levels for all weapons (less per stack)" },
	[ITEM_SNEK]            = { "Snek", "icon_itemsnek", L, "Apply 4 stacks of poison on hit, with a 5% chance to burst all the poison at once" },
	[ITEM_BLOODYCLEAVER]   = { "Bloody Cleaver", "icon_bloody_cleaver", L, "50% chance to apply Bloodmark on hit" },
	[ITEM_OVERPOWEREDLAMP] = { "Overpowered Lamp", "icon_itemoverpoweredlamp", L, "+1 chance to proc on hit effects" },
};
#undef C
#undef R
#undef E
#undef L

enum {
	T_THUNDER, T_MIRROR, T_LANTERN, T_MAGNET, T_CORE, T_DOLL, T_SLURP, T_KEVIN,
	T_SPEEDBOI, T_SLOW, T_COWARD, T_PHANTOM, T_HOLYDMG, T_COUNT
};

int items_count(ItemId id) { return g.itemCounts[id]; }

static void scale_max_hp(float f)
{
	g.stats.maxHp *= f;
	g.hp *= f;
}

static void apply_passive(ItemId id, float sign)
{
	PlayerStats* s = &g.stats;
	switch (id) {
	case ITEM_GOLDENGLOVE:    s->goldMult += 0.15f * sign; break;
	case ITEM_GYMSAUCE:       s->damageMult += 0.10f * sign; break;
	case ITEM_MEDKIT:         s->regen += 0.75f * sign; break;
	case ITEM_OATS:           s->maxHp += 25 * sign; if (sign > 0) g.hp += 25; break;
	case ITEM_SLIPPERYRING:   s->evasion += 0.15f * sign; break;
	case ITEM_TURBOSOCKS:     s->moveSpeed += 0.15f * sign; break;
	case ITEM_TIMEBRACELET:   s->xpMult += 0.08f * sign; break;
	case ITEM_BATTERY:        s->attackSpeed += 0.08f * sign; break;
	case ITEM_CLOVER:         s->luck += 0.10f * sign; break;
	case ITEM_FORBIDDENJUICE: s->critChance += 0.10f * sign; break;
	case ITEM_SKULEG:         g.difficulty += 0.07f * sign; s->xpMult += 0.07f * sign; break;
	case ITEM_BACKPACK:       s->extraProjectiles += (int)sign; break;
	case ITEM_BEER:           s->damageMult += 0.20f * sign; scale_max_hp(sign > 0 ? 0.95f : 1 / 0.95f); break;
	case ITEM_LEECHINGCRYSTAL:
		scale_max_hp(sign > 0 ? 1.5f : 1 / 1.5f);
		s->regen *= sign > 0 ? 0.5f : 2.0f;
		break;
	case ITEM_COWARDSCLOAK:   s->moveSpeed += 0.05f * sign; break;
	case ITEM_SPIKYSHIELD:    s->armor += 0.1f * sign; break;
	case ITEM_CURSEDGRABBIES: g.difficulty += 0.10f * sign; scale_max_hp(sign > 0 ? 0.8f : 1 / 0.8f); break;
	case ITEM_CREDITCARDGREEN: s->luck += 0.05f * sign; break;
	case ITEM_GIANTFORK:      s->critChance += 0.10f * sign; break;
	case ITEM_HOLYBOOK:       s->maxHp += 100 * sign; s->regen += 50 / 60.0f * sign; if (sign > 0) g.hp += 100; break;
	case ITEM_CHONKPLATE:     s->lifesteal += 0.20f * sign; break;
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

// Chance roll; Overpowered Lamp gives each stack an extra independent roll.
static bool proc(float chance)
{
	if (randf() < chance) return true;
	for (int i = 0; i < n_(ITEM_OVERPOWEREDLAMP); i++)
		if (randf() < chance) return true;
	return false;
}

static bool is_tough(int idx)
{
	const Enemy* e = &g.enemies[idx];
	return e->elite || (g_enemyDefs[e->def].flags & (ENEMY_BOSS | ENEMY_MINIBOSS));
}

static int count_poisoned(void)
{
	int n = 0;
	for (int i = 0; i < MAX_ENEMIES; i++)
		if (g.enemies[i].active && g.enemies[i].poisonTime > 0) n++;
	return n;
}

float items_key_chance(void) { return 1.0f - powf(0.9f, n_(ITEM_KEY)); }
float items_elite_mult(void) { return 1.0f + 0.15f * n_(ITEM_OLDMASK); }
float items_megacrit_mult(void) { return 4.0f + 0.15f * (n_(ITEM_GIANTFORK) - 1); }

int items_weapon_level_cap(void)
{
	int cap = 40;
	for (int i = 0; i < n_(ITEM_POT); i++) cap += i < 8 ? 10 - i : 2;   // 10, 9, 8 ... down to 2
	return cap;
}

float items_speed_mult(void)
{
	float m = 1.0f;
	if (g.itemTimers[T_COWARD] > 0) m += 0.3f;
	if (g.itemTimers[T_PHANTOM] > 0) m += 0.2f;
	return m;
}

float items_attack_speed_mult(void)
{
	float m = 1.0f;
	if (g.itemTimers[T_PHANTOM] > 0) m += 0.2f;
	if (n_(ITEM_ROLLERBLADES)) {
		float spd = sqrtf(g.vel.x * g.vel.x + g.vel.z * g.vel.z);
		m += fminf(spd / 12.0f, 1.0f) * 0.40f * n_(ITEM_ROLLERBLADES);
	}
	return m;
}

float items_modify_damage(int idx, float dmg)
{
	const Enemy* e = &g.enemies[idx];
	float mult = 1.0f;
	if (n_(ITEM_BOSSBUSTER) && is_tough(idx)) mult += 0.15f * n_(ITEM_BOSSBUSTER);
	if (n_(ITEM_TACTICALGLASSES) && e->hp > e->maxHp * 0.9f) mult += 0.20f * n_(ITEM_TACTICALGLASSES);
	if (n_(ITEM_BRASSKNUCKLES) && dist2_xz(e->pos, g.pos) < 8 * 8) mult += 0.20f * n_(ITEM_BRASSKNUCKLES);
	if (n_(ITEM_DEMONICSOUL)) mult += fminf(0.001f * g.killStacks, 1.0f);
	if (n_(ITEM_BEEFYRING)) mult += 0.002f * g.stats.maxHp * n_(ITEM_BEEFYRING);
	if (n_(ITEM_SCARF) && !g.grounded) mult += 0.50f * n_(ITEM_SCARF);
	if (n_(ITEM_EAGLECLAW) && ((g_enemyDefs[e->def].flags & ENEMY_FLYING) || e->launched > 0)) mult += 0.66f * n_(ITEM_EAGLECLAW);
	if (n_(ITEM_IDLEJUICE)) mult += fminf(g.stillTime / 4.0f, 1.0f) * 1.0f * n_(ITEM_IDLEJUICE);
	if (n_(ITEM_GAMERGOGGLES)) mult += 0.5f * (1.0f - g.hp / g.stats.maxHp) * n_(ITEM_GAMERGOGGLES);
	if (n_(ITEM_CREDITCARDRED)) mult += 0.025f * g.cardChests * n_(ITEM_CREDITCARDRED);
	if (n_(ITEM_JOESDAGGER)) mult += 0.01f * g.joeStacks;
	if (g.itemTimers[T_SPEEDBOI] > 0) mult += 1.0f;
	if (g.phantomNext) { mult *= 2.0f; g.phantomNext = false; }
	if (n_(ITEM_BONKER) && proc(0.02f * n_(ITEM_BONKER))) {
		mult *= 20.0f;
		fx_spawn(FX_RING, e->pos, 3.0f, 0.3f, COLOR(1, 0.85f, 0.3f));
	}
	return dmg * mult;
}

static void strike(vec3 at, float radius, float dmg, Color c, FxKind fx)
{
	game_area_damage(at, radius, dmg, 0.6f, DMG_NO_PROC);
	fx_spawn(fx, at, radius, 0.3f, c);
}

static void poison(Enemy* e, float dps, float time)
{
	e->poisonTime = fmaxf(e->poisonTime, time);
	e->poisonDps = fmaxf(e->poisonDps, dps);
}

static void bloodmark(Enemy* e)
{
	if (e->bloodmarks < 50) e->bloodmarks++;
	if (e->bloodTimer <= 0) e->bloodTimer = 3.0f;
}

void items_on_hit(int idx, float dmg, bool crit)
{
	Enemy* e = &g.enemies[idx];
	vec3 at = e->pos;
	float pdmg = 5 * g.stats.damageMult;
	if (n_(ITEM_JOESDAGGER) && e->active && !(g_enemyDefs[e->def].flags & (ENEMY_BOSS | ENEMY_MINIBOSS)) &&
	    proc(0.01f * n_(ITEM_JOESDAGGER))) {
		g.joeStacks++;
		fx_spawn(FX_SLASH, g.pos, 3.0f, 0.2f, COLOR(0.9f, 0.2f, 0.2f));
		game_damage_enemy_ex(idx, e->hp + 1, v3(0, 0, 0), 0, DMG_NO_PROC | DMG_NO_CRIT);
		return;
	}
	if (!e->active) at = e->pos;   // killed by the hit: procs still go off where it died
	if (n_(ITEM_MOLDYCHEESE) && proc(0.40f)) poison(e, pdmg * n_(ITEM_MOLDYCHEESE), 3.0f);
	if (n_(ITEM_SNEK)) {
		e->poisonDps += pdmg * 0.4f * 4 * n_(ITEM_SNEK);   // 4 stacks
		e->poisonTime = fmaxf(e->poisonTime, 3.0f);
		if (e->active && proc(0.05f)) {
			float burst = e->poisonDps * e->poisonTime;
			e->poisonTime = 0;
			e->poisonDps = 0;
			fx_spawn(FX_POISON, at, 2.5f, 0.4f, COLOR(0.5f, 1, 0.2f));
			game_damage_enemy_ex(idx, burst, v3(0, 0, 0), 0, DMG_NO_PROC | DMG_NO_CRIT);
		}
	}
	if (n_(ITEM_ICECRYSTAL) && proc(0.075f * n_(ITEM_ICECRYSTAL))) e->freezeTime = 1.5f;
	if (n_(ITEM_ICECUBE) && proc(0.20f * n_(ITEM_ICECUBE))) {
		e->freezeTime = 2.0f;
		strike(at, 2.0f, 20 * g.stats.damageMult, COLOR(0.6f, 0.9f, 1), FX_RING);
	}
	if (n_(ITEM_UNSTABLETRANSFUSION) && e->active && proc(0.35f * n_(ITEM_UNSTABLETRANSFUSION))) bloodmark(e);
	if (n_(ITEM_BLOODYCLEAVER) && e->active && proc(0.50f * n_(ITEM_BLOODYCLEAVER))) bloodmark(e);
	if (crit && n_(ITEM_DEMONBLADE) && proc(0.25f * n_(ITEM_DEMONBLADE))) game_heal(3);
	if (crit && n_(ITEM_GRANDMASTONIC) && proc(0.50f))
		strike(at, 4.0f, dmg * 0.5f * n_(ITEM_GRANDMASTONIC), COLOR(0.9f, 0.6f, 1), FX_EXPLOSION);
	if (n_(ITEM_THUNDERMITTS) && g.itemTimers[T_THUNDER] <= 0) {
		g.itemTimers[T_THUNDER] = 2.0f / n_(ITEM_THUNDERMITTS);
		strike(at, 2.5f, 40 * g.stats.damageMult, COLOR(0.5f, 0.85f, 1), FX_LIGHTNING);
	}
	if (n_(ITEM_LIGHTNINGORB) && proc(0.25f * n_(ITEM_LIGHTNINGORB))) {
		strike(at, 3.0f, 30 * g.stats.damageMult, COLOR(0.6f, 0.8f, 1), FX_LIGHTNING);
		if (e->active) e->freezeTime = fmaxf(e->freezeTime, 0.5f);   // stun
	}
	if (n_(ITEM_MOLDYGLOVES) && proc(0.10f * n_(ITEM_MOLDYGLOVES))) {
		for (int i = 0; i < MAX_ENEMIES; i++) {
			Enemy* o = &g.enemies[i];
			if (o->active && dist2_xz(o->pos, at) < 16) poison(o, 8 * g.stats.damageMult, 3);
		}
		fx_spawn(FX_POISON, at, 4.0f, 0.6f, COLOR(0.4f, 0.9f, 0.3f));
	}
	if (n_(ITEM_SLURPGLOVES) && g.itemTimers[T_SLURP] <= 0) {
		g.itemTimers[T_SLURP] = 3.0f / n_(ITEM_SLURPGLOVES);
		strike(at, 5.0f, 25 * g.stats.damageMult, COLOR(0.9f, 0.1f, 0.2f), FX_EXPLOSION);
		game_heal(3);
	}
	if (n_(ITEM_CURSEDGRABBIES) && proc(0.05f))
		strike(at, 4.0f, 30 * g.stats.damageMult * n_(ITEM_CURSEDGRABBIES), COLOR(0.6f, 0.1f, 0.8f), FX_EXPLOSION);
	if (n_(ITEM_EAGLECLAW) && e->active && !is_tough(idx) && proc(0.08f)) e->launched = 0.8f;   // knocked into the air
	if (n_(ITEM_DRAGONFIRE) && proc(0.15f * n_(ITEM_DRAGONFIRE))) {
		strike(at, 4.0f, dmg * 0.6f, COLOR(1, 0.4f, 0.05f), FX_EXPLOSION);
		if (e->active) poison(e, dmg * 0.2f, 2.0f);   // burn
	}
	if (n_(ITEM_SPICYMEATBALL) && proc(0.25f * n_(ITEM_SPICYMEATBALL)))
		strike(at, 5.0f, dmg * 0.65f, COLOR(1, 0.3f, 0.1f), FX_EXPLOSION);
	if (n_(ITEM_POWERGLOVES) && proc(0.08f * n_(ITEM_POWERGLOVES))) {
		game_area_damage(at, 6.0f, 100 * g.stats.damageMult, 4.0f, DMG_NO_PROC);
		fx_spawn(FX_EXPLOSION, at, 6.0f, 0.35f, COLOR(1, 0.9f, 0.6f));
	}
	if (n_(ITEM_KEVIN) && g.itemTimers[T_KEVIN] <= 0 && randf() < 0.25f) {
		// Kevin punches you. He can't kill you, but everything that triggers on damage does.
		g.itemTimers[T_KEVIN] = 1.0f;
		float d = items_on_player_damaged(5.0f);
		g.hp = fmaxf(1.0f, g.hp - d);
		g.hurtFlash = 0.2f;
	}
}

void items_on_kill(int idx)
{
	Enemy* e = &g.enemies[idx];
	if (n_(ITEM_DEMONICBLOOD) || n_(ITEM_DEMONICSOUL)) {
		g.killStacks++;
		if (n_(ITEM_DEMONICBLOOD) && g.killStacks <= 200 * n_(ITEM_DEMONICBLOOD)) {
			g.stats.maxHp += 0.5f;
			g.hp += 0.5f;
		}
	}
	if (n_(ITEM_BORGAR) && proc(0.02f * n_(ITEM_BORGAR))) game_spawn_pickup(PICKUP_HEART, e->pos, 25);
	if (n_(ITEM_ECHOSHARD) && proc(0.12f * n_(ITEM_ECHOSHARD))) game_spawn_pickup(PICKUP_XP, e->pos, 1);
	vec3 from = v3_add(e->pos, v3(0, 1.5f, 0));
	if (n_(ITEM_SOULHARVESTER))
		for (int i = 0; i < 2 * n_(ITEM_SOULHARVESTER); i++)
			game_fire_item_projectile(PROJ_SOUL, from, v3(randrange(-1, 1), 1, randrange(-1, 1)), 18,
			                          (5 + 1.75f * g.level) * g.stats.damageMult, 4);
	if (n_(ITEM_SLUTTYCANNON) && proc(0.20f * n_(ITEM_SLUTTYCANNON)))
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
	if (n_(ITEM_GASMASK)) dmg /= 1.0f + fminf(0.02f * count_poisoned() * n_(ITEM_GASMASK), 1.0f);
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
	if (n_(ITEM_TOXICBARREL)) {
		for (int i = 0; i < MAX_ENEMIES; i++) {
			Enemy* o = &g.enemies[i];
			if (o->active && dist2_xz(o->pos, g.pos) < 9 * 9) poison(o, 10 * g.stats.damageMult * n_(ITEM_TOXICBARREL), 4);
		}
		fx_spawn(FX_POISON, g.pos, 9.0f, 0.6f, COLOR(0.5f, 1, 0.2f));
	}
	float thorns = g.stats.thorns + (n_(ITEM_SPIKYSHIELD) ? g.stats.armor * 200 * n_(ITEM_SPIKYSHIELD) : 0);
	if (thorns > 0) {
		game_area_damage(g.pos, 3.0f, thorns * g.stats.damageMult, 0.5f, DMG_NO_PROC);
		if (n_(ITEM_QUINSMASK) && proc(0.30f * n_(ITEM_QUINSMASK)))
			strike(g.pos, 6.0f, thorns * g.stats.damageMult, COLOR(1, 0.5f, 0.3f), FX_EXPLOSION);
	}
	if (n_(ITEM_GOLDENSHIELD)) g.gold += 2 * n_(ITEM_GOLDENSHIELD);
	if (n_(ITEM_COWARDSCLOAK)) g.itemTimers[T_COWARD] = 2.0f;
	if (n_(ITEM_SPEEDBOI) && g.itemTimers[T_SPEEDBOI] <= -20.0f / n_(ITEM_SPEEDBOI) && g.hp - dmg < g.stats.maxHp * 0.5f) {
		g.itemTimers[T_SPEEDBOI] = 4.0f;   // 2x damage; the cooldown runs below zero
		g.itemTimers[T_SLOW] = 4.0f;
		game_toast("Speed Boi!");
	}
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

void items_on_evade(void)
{
	if (!n_(ITEM_PHANTOMSHROUD)) return;
	g.phantomNext = true;
	g.itemTimers[T_PHANTOM] = 1.5f * n_(ITEM_PHANTOMSHROUD);
}

void items_on_heal(float amount)
{
	if (!n_(ITEM_HOLYBOOK) || amount <= 0) return;
	g.holyHeal += amount;
	if (g.holyHeal >= 10.0f && g.itemTimers[T_HOLYDMG] <= 0) {
		g.holyHeal = 0;
		g.itemTimers[T_HOLYDMG] = 0.5f;
		game_area_damage(g.pos, 7.0f, 20 * g.stats.damageMult * n_(ITEM_HOLYBOOK), 0.5f, DMG_NO_PROC);
		fx_spawn(FX_RING, g.pos, 7.0f, 0.35f, COLOR(1, 1, 0.6f));
	}
}

void items_on_interact(void)
{
	for (int i = 0; i < 3 * n_(ITEM_GHOST); i++)
		game_fire_item_projectile(PROJ_SOUL, v3_add(g.pos, v3(0, 2, 0)), v3(randrange(-1, 1), 1, randrange(-1, 1)), 16,
		                          30 * g.stats.damageMult, 6);
}

void items_on_chest_open(void)
{
	g.cardChests++;
	if (n_(ITEM_CREDITCARDGREEN)) g.stats.critChance += 0.01f * n_(ITEM_CREDITCARDGREEN);
}

int items_max_jumps(void) { return 1 + n_(ITEM_FEATHERS); }
float items_jump_mult(void) { return 1.0f + 0.15f * n_(ITEM_FEATHERS); }

void items_update(float dt)
{
	for (int i = 0; i < T_COUNT; i++)
		if (i == T_SPEEDBOI ? g.itemTimers[i] > -60 : g.itemTimers[i] > 0) g.itemTimers[i] -= dt;

	float spd = sqrtf(g.vel.x * g.vel.x + g.vel.z * g.vel.z);
	g.stillTime = spd < 0.5f ? g.stillTime + dt : 0;
	if (n_(ITEM_CAMPFIRE) && g.stillTime > 1.0f) game_heal(2.0f * n_(ITEM_CAMPFIRE) * dt);
	g.movedDist += spd * dt;
	if (n_(ITEM_GOLDENSNEAKERS))
		while (g.movedDist > 10) { g.movedDist -= 10; g.gold += n_(ITEM_GOLDENSNEAKERS); }
	if (n_(ITEM_BOBDEAD)) {
		g.bobDist += spd * dt;
		if (g.bobDist > 30) {
			g.bobDist = 0;
			for (int i = 0; i < n_(ITEM_BOBDEAD); i++)
				game_fire_item_projectile(PROJ_SOUL, v3_add(g.pos, v3(0, 2, 0)), v3(randrange(-1, 1), 0.5f, randrange(-1, 1)),
				                          14, 40 * g.stats.damageMult, 6);
		}
	}
	if (n_(ITEM_BOBSLANTERN) && g.itemTimers[T_LANTERN] <= 0) {
		int n = n_(ITEM_BOBSLANTERN);
		g.itemTimers[T_LANTERN] = fmaxf(5.0f, 45.0f - 3.0f * (n - 1));
		float r = 9.0f + 2.0f * (n - 1);
		for (int i = 0; i < MAX_ENEMIES; i++) {
			Enemy* e = &g.enemies[i];
			if (!e->active || dist2_xz(e->pos, g.pos) > r * r) continue;
			bool boss = g_enemyDefs[e->def].flags & (ENEMY_BOSS | ENEMY_MINIBOSS);
			game_damage_enemy_ex(i, boss ? e->maxHp * 0.04f : e->hp + 1, v3(0, 0, 0), 0, DMG_NO_PROC | DMG_NO_CRIT);
		}
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
		// curse the 5 closest enemies
		for (int k = 0; k < 5; k++) {
			int t = -1;
			float best = 25 * 25;
			for (int i = 0; i < MAX_ENEMIES; i++) {
				Enemy* e = &g.enemies[i];
				if (!e->active || e->cursedTick == g.dollTick + 1) continue;
				float d2 = dist2_xz(e->pos, g.pos);
				if (d2 < best) { best = d2; t = i; }
			}
			if (t < 0) break;
			Enemy* e = &g.enemies[t];
			e->cursedTick = g.dollTick + 1;
			float pct = (is_tough(t) ? 0.01f : 0.30f) * n_(ITEM_CURSEDDOLL);
			game_damage_enemy_ex(t, fmaxf(e->maxHp * pct, 1), v3(0, 0, 0), 0, DMG_NO_PROC | DMG_NO_CRIT);
		}
		g.dollTick++;
	}
	// Bloodmark: stacked damage that resolves when its timer runs out
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Enemy* e = &g.enemies[i];
		if (!e->active || !e->bloodmarks) continue;
		e->bloodTimer -= dt;
		if (e->bloodTimer > 0) continue;
		float dmg = e->bloodmarks * (4 + 0.5f * g.level) * g.stats.damageMult;
		e->bloodmarks = 0;
		fx_spawn(FX_EXPLOSION, v3_add(e->pos, v3(0, 1, 0)), 1.5f, 0.25f, COLOR(0.8f, 0.05f, 0.1f));
		game_damage_enemy_ex(i, dmg, v3(0, 0, 0), 0, DMG_NO_PROC);
	}
}

// Time slow from Speed Boi (enemies move slower).
float items_enemy_time_scale(void) { return g.itemTimers[T_SLOW] > 0 ? 0.4f : 1.0f; }
