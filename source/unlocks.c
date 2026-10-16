#include "game.h"
#include "save.h"
#include "unlocks.h"
#include "audio.h"
#include "mathutil.h"
#include <string.h>
#include <stdio.h>

// Unlock requirements from the game's MyAchievement assets (targetValue / statName) with the
// game's English requirement text. A few need things this port doesn't have (quests, the crypt,
// hidden objects, tumbleweeds, sandstorms...): those use a stand-in marked "(port)", or start
// unlocked (C_FREE) when there's nothing sensible to stand in.
enum {
	C_FREE,            // unlocked from the start in the port
	C_COUNT,           // lifetime counter a >= target
	C_CHAR_KILLS,      // kills as character a
	C_WEAPON_KILLS,    // kills with weapon a
	C_TOME_LEVEL,      // tome a at level >= target in a run
	C_TOMES_LEVEL,     // tomes a and b both at level >= target in a run
	C_WEAPON_LEVEL,    // weapon a at level >= target
	C_STAGE_CLEAR,     // beat the boss of stage b of map a (stage-select index)
	C_STAGE_CLEAR_AS,  // ... as character target
	C_LEVEL_AS,        // player level >= target as character a
	C_MAX_HP,          // max HP >= target
	C_DIFFICULTY,      // difficulty >= target %
	C_FINAL_SWARM,     // survive the final swarm for target seconds
	C_NO_HIT,          // no damage for target seconds
	C_ITEM_COUNT,      // hold target copies of item a
	C_MICROWAVE_STREAK,// microwave item a target times in a row
	C_BOSS_FAST,       // beat a stage boss within target seconds of it appearing
	C_MINIBOSS_FAST,   // beat a miniboss within target seconds of it appearing
	C_BOSS_LOW_HP,     // beat a stage boss below target % HP
	C_BOSS_NO_ITEMS,   // beat a stage boss without taking items or using shrines that stage
	C_FIRST_BOSS_CLEAN,// beat the first stage boss without taking damage that run
	C_BOSS_AS_WITH,    // beat a stage boss as character a with weapon b landing the kill
	C_STAGE_CURSES,    // clear a stage with >= target boss curses
	C_ALL_CHARGES,     // clear a stage with every charge shrine charged
	C_TOME_FAST,       // tome a at level >= target within the first 10 minutes of a run
	C_STAGE_FAST,      // beat a stage boss before target seconds of stage time
	C_POTS_RUN,        // break target pots in one run
	C_DROP,            // target per-mille chance to drop on any kill
	C_DROP_TUMBLE,     // target % chance to drop from a Tumblebone on map a stage b
};

#define CH(i) UNL_CHARACTER, i
#define WP(i) UNL_WEAPON, i
#define TM(i) UNL_TOME, i
#define IT(i) UNL_ITEM, i
enum { FOX, CALCIUM, OOFIE, CLANK, MEGACHAD, OGRE, ATHENA, BIRDO, BUSH, BANDIT, MONKE, NOELLE, ROBINETTE,
       TONY, AMOG, SPACEMAN, NINJA, VLAD, DICEHEAD, CHADWELL, ROBERTO };
enum { FOREST = 0, DESERT = 1 };

// Order matters: the profile stores unlocked flags by index. Only append.
const UnlockDef g_unlockDefs[] = {
	// characters (Fox and Sir Oofie start unlocked)
	{ CH(CALCIUM),   C_COUNT, CNT_SKELETON_KILLS, 0, 1000, "Kill 1000 skeletons" },
	{ CH(MEGACHAD),  C_TOME_LEVEL, TOME_DAMAGE, 0, 7, "Get the Damage Tome to level 7" },
	{ CH(AMOG),      C_COUNT, CNT_MOLDY_CHEESE_PROCS, 0, 50000, "Poison 50000 enemies with Moldy Cheese" },
	{ CH(ATHENA),    C_TOME_LEVEL, TOME_THORNS, 0, 9, "Get the Thorns Tome to level 9" },
	{ CH(BANDIT),    C_FREE, 0, 0, 0, "Find and defeat Bandit in the Desert" },
	{ CH(BIRDO),     C_COUNT, CNT_TORNADO_DESERT_KILLS, 0, 100, "(port) Kill 100 enemies with the Tornado on Desert" },
	{ CH(BUSH),      C_FREE, 0, 0, 0, "Find and defeat Bush in the Forest" },
	{ CH(CHADWELL),  C_STAGE_CLEAR, FOREST, 3, 0, "(port) Clear Forest stage 3" },
	{ CH(CLANK),     C_STAGE_CLEAR, FOREST, 1, 0, "(port) Clear Forest stage 1" },
	{ CH(DICEHEAD),  C_FREE, 0, 0, 0, "Complete 100 Quests" },
	{ CH(MONKE),     C_FREE, 0, 0, 0, "Find and release Monke in the Forest" },
	{ CH(NINJA),     C_STAGE_CLEAR, DESERT, 1, 0, "(port) Clear Desert stage 1" },
	{ CH(NOELLE),    C_MICROWAVE_STREAK, ITEM_ICECRYSTAL, 0, 3, "Use a Microwave to duplicate Ice Crystal 3 times in a row" },
	{ CH(OGRE),      C_COUNT, CNT_GOBLIN_KILLS, 0, 15000, "Kill 15000 Goblins" },
	{ CH(ROBERTO),   C_FREE, 0, 0, 0, "Kill Spooky Steve" },
	{ CH(ROBINETTE), C_STAGE_CLEAR, FOREST, 2, 0, "(port) Clear Forest stage 2" },
	{ CH(SPACEMAN),  C_COUNT, CNT_CHALLENGE_SHRINES, 0, 6, "(port) Complete 6 Challenge Shrines" },
	{ CH(TONY),      C_COUNT, CNT_CHALLENGE_SHRINES, 0, 2, "(port) Complete 2 Challenge Shrines" },
	{ CH(VLAD),      C_STAGE_CLEAR, DESERT, 2, 0, "(port) Clear Desert stage 2" },

	// weapons (Firestaff, Bone, Sword, Lightning Staff, Bow, Chunkers, Flamewalker start unlocked)
	{ WP(WEAPON_AEGIS),          C_COUNT, CNT_OOFIE_ARMOR_BLOCKED, 0, 500, "Block 500 damage with Armor as Sir Oofie" },
	{ WP(WEAPON_AURA),           C_NO_HIT, 0, 0, 120, "Don't take any damage for 2 minutes" },
	{ WP(WEAPON_AXE),            C_WEAPON_KILLS, WEAPON_SWORD, 0, 2000, "Kill 2000 enemies using the Sword" },
	{ WP(WEAPON_BANANARANG),     C_FREE, 0, 0, 0, "Find the hidden Banana" },
	{ WP(WEAPON_BLACKHOLE),      C_TOME_LEVEL, TOME_KNOCKBACK, 0, 10, "Get the Knockback Tome to level 10" },
	{ WP(WEAPON_BLOODMAGIC),     C_TOME_LEVEL, TOME_BLOODY, 0, 12, "Get the Bloody Tome to level 12" },
	{ WP(WEAPON_CORRUPTEDSWORD), C_TOME_FAST, TOME_CURSED, 0, 20, "Get the Cursed Tome to level 20 in under 10 minutes" },
	{ WP(WEAPON_DEXECUTIONER),   C_WEAPON_KILLS, WEAPON_SWORD, 0, 12500, "Kill 12500 enemies using the Sword" },
	{ WP(WEAPON_DICE),           C_TOME_LEVEL, TOME_LUCK, 0, 12, "Get the Luck Tome to level 12" },
	{ WP(WEAPON_DRAGONBREATH),   C_COUNT, CNT_FOX_DESERT_WISPS, 0, 1000, "Kill 1000 Wisps as Fox on Desert" },
	{ WP(WEAPON_FROSTWALKER),    C_COUNT, CNT_ICECUBE_FREEZES, 0, 1000, "Freeze 1000 enemies with the Ice Cube" },
	{ WP(WEAPON_HEROSWORD),      C_BOSS_NO_ITEMS, 0, 0, 0, "Defeat a Stage Boss without picking up any items or using shrines" },
	{ WP(WEAPON_KATANA),         C_DROP_TUMBLE, DESERT, 1, 5, "(port) 5% chance to drop from a Tumblebone on Desert stage 1" },
	{ WP(WEAPON_MINES),          C_WEAPON_KILLS, WEAPON_SLUTTYROCKET, 0, 7500, "Kill 7500 enemies with the Slutty Rocket" },
	{ WP(WEAPON_POISONFLASK),    C_COUNT, CNT_SCORPION_MINIBOSS_KILLS, 0, 3, "Kill the Scorpionussy Miniboss on Desert 3 times" },
	{ WP(WEAPON_REVOLVER),       C_COUNT, CNT_KILLS, 0, 7500, "Kill 7500 enemies" },
	{ WP(WEAPON_SCYTHE),         C_FREE, 0, 0, 0, "Kill Big Bob on Graveyard" },
	{ WP(WEAPON_SHOTGUN),        C_DROP_TUMBLE, DESERT, 2, 5, "(port) 5% chance to drop from a Tumblebone on Desert stage 2" },
	{ WP(WEAPON_SLUTTYROCKET),   C_CHAR_KILLS, CLANK, 0, 15000, "Kill 15000 enemies as CL4NK" },
	{ WP(WEAPON_SNIPER),         C_TOME_LEVEL, TOME_PRECISION, 0, 10, "Get the Precision Tome to level 10" },
	{ WP(WEAPON_SPACENOODLE),    C_STAGE_CLEAR_AS, DESERT, 2, TONY, "(port) Clear Desert stage 2 as Tony McZoom" },
	{ WP(WEAPON_TORNADO),        C_COUNT, CNT_DESERT_SHRINE_CHARGES, 0, 5, "(port) Charge 5 Charge Shrines on Desert" },
	{ WP(WEAPON_WIRELESSDAGGER), C_WEAPON_LEVEL, WEAPON_LIGHTNING, 0, 15, "Get the Lightning Staff to level 15" },

	// tomes
	{ TM(TOME_ARMOR),      C_CHAR_KILLS, OOFIE, 0, 5000, "Kill 5000 enemies as Sir Oofie" },
	{ TM(TOME_ATTRACTION), C_COUNT, CNT_MAGNET_SHRINES, 0, 8, "Use the Shrine of Succ 8 times" },
	{ TM(TOME_BLOODY),     C_COUNT, CNT_KILLS, 0, 12500, "Kill 12500 enemies" },
	{ TM(TOME_CHAOS),      C_ALL_CHARGES, 3, 0, 0, "(port) Clear a stage 3 with every Charge Shrine charged" },
	{ TM(TOME_CURSED),     C_STAGE_FAST, 0, 0, 300, "Beat the Stage Boss in under 5 minutes" },
	{ TM(TOME_DURATION),   C_WEAPON_LEVEL, WEAPON_AXE, 0, 10, "Upgrade the Axe to level 10" },
	{ TM(TOME_LUCK),       C_DROP, 0, 0, 10, "1% chance to drop when killing an enemy" },
	{ TM(TOME_QUANTITY),   C_COUNT, CNT_PROJECTILES_FIRED, 0, 5000, "Fire 5000 projectiles" },
	{ TM(TOME_THORNS),     C_COUNT, CNT_AEGIS_BLOCKS, 0, 250, "Block 250 attacks with the Aegis" },
	{ TM(TOME_XP),         C_POTS_RUN, 0, 0, 20, "Break 20 pots in a single run" },

	// items
	{ IT(ITEM_BATTERY),         C_TOME_LEVEL, TOME_COOLDOWN, 0, 5, "Upgrade the Cooldown Tome to level 5" },
	{ IT(ITEM_BOSSBUSTER),      C_BOSS_FAST, 0, 0, 30, "Defeat a Stage Boss in under 30 seconds" },
	{ IT(ITEM_CACTUS),          C_COUNT, CNT_CACTUS_THORN_KILLS, 0, 100, "Kill 100 cactus enemies with Thorns damage" },
	{ IT(ITEM_CURSEDDOLL),      C_COUNT, CNT_DEATHS, 0, 10, "Die 10 times" },
	{ IT(ITEM_FORBIDDENJUICE),  C_COUNT, CNT_CRITS, 0, 100, "Crit 100 times" },
	{ IT(ITEM_GHOST),           C_FINAL_SWARM, 0, 0, 60, "Survive the Final Swarm for 60 seconds" },
	{ IT(ITEM_ICECRYSTAL),      C_WEAPON_KILLS, WEAPON_FROSTWALKER, 0, 50, "Get 50 kills with the Frostwalker" },
	{ IT(ITEM_KEY),             C_COUNT, CNT_CHESTS_BOUGHT, 0, 25, "Purchase 25 chests" },
	{ IT(ITEM_OLDMASK),         C_FREE, 0, 0, 0, "Find on Graveyard" },
	{ IT(ITEM_SKULEG),          C_DIFFICULTY, 0, 0, 40, "Obtain 40% Difficulty" },
	{ IT(ITEM_TACTICALGLASSES), C_MINIBOSS_FAST, 0, 0, 25, "Defeat a Miniboss in 25 seconds" },
	{ IT(ITEM_TURBOSOCKS),      C_TOME_LEVEL, TOME_AGILITY, 0, 5, "Upgrade the Agility Tome to level 5" },
	{ IT(ITEM_BRASSKNUCKLES),   C_WEAPON_KILLS, WEAPON_SWORD, 0, 5000, "Kill 5000 enemies with the Sword" },
	{ IT(ITEM_DEMONBLADE),      C_COUNT, CNT_LIFESTEAL_HEALING, 0, 350, "Heal for 350 HP using Lifesteal" },
	{ IT(ITEM_DEMONICBLOOD),    C_COUNT, CNT_BLOOD_MAGIC_PROCS, 0, 400, "Increase Max HP with Blood Magic 400 times" },
	{ IT(ITEM_ECHOSHARD),       C_TOME_LEVEL, TOME_XP, 0, 8, "Upgrade the XP Tome to level 8" },
	{ IT(ITEM_GOLDENSNEAKERS),  C_COUNT, CNT_POTS_BROKEN, 0, 150, "Break 150 pots" },
	{ IT(ITEM_IDLEJUICE),       C_COUNT, CNT_STILL_KILLS, 0, 500, "Kill 500 enemies while standing still" },
	{ IT(ITEM_KEVIN),           C_ITEM_COUNT, ITEM_LEECHINGCRYSTAL, 0, 3, "Obtain 3 Leeching Crystals in a run" },
	{ IT(ITEM_LEECHINGCRYSTAL), C_TOME_LEVEL, TOME_REGEN, 0, 8, "Upgrade the Regen Tome to level 8" },
	{ IT(ITEM_MOLDYGLOVES),     C_MICROWAVE_STREAK, ITEM_MOLDYCHEESE, 0, 3, "Use a Microwave to duplicate Moldy Cheese 3 times in a row" },
	{ IT(ITEM_BOBDEAD),         C_FINAL_SWARM, 0, 0, 120, "Survive the Final Swarm for 2 minutes" },
	{ IT(ITEM_BOBSLANTERN),     C_FREE, 0, 0, 0, "Kill Big Bob on Graveyard with the damage from a lamp" },
	{ IT(ITEM_CURSEDGRABBIES),  C_TOME_LEVEL, TOME_CURSED, 0, 10, "Get the Cursed Tome to level 10" },
	{ IT(ITEM_DEMONICSOUL),     C_LEVEL_AS, CALCIUM, 0, 30, "Reach level 30 as Calcium" },
	{ IT(ITEM_EAGLECLAW),       C_CHAR_KILLS, BIRDO, 0, 10000, "Kill 10000 enemies as Birdo" },
	{ IT(ITEM_GAMERGOGGLES),    C_BOSS_LOW_HP, 0, 0, 10, "Defeat a Stage Boss with less than 10% HP left" },
	{ IT(ITEM_GASMASK),         C_CHAR_KILLS, AMOG, 0, 5000, "Kill 5000 enemies using Amog" },
	{ IT(ITEM_GRANDMASTONIC),   C_WEAPON_KILLS, WEAPON_SNIPER, 0, 3000, "Kill 3000 enemies with the Sniper Rifle" },
	{ IT(ITEM_QUINSMASK),       C_BOSS_AS_WITH, ATHENA, WEAPON_AEGIS, 0, "Defeat a Stage Boss as Athena, landing the killing blow with the Aegis" },
	{ IT(ITEM_SHATTEREDKNOWLEDGE), C_TOME_LEVEL, TOME_ATTRACTION, 0, 8, "Upgrade the Attraction Tome to level 8" },
	{ IT(ITEM_SLUTTYCANNON),    C_WEAPON_KILLS, WEAPON_SLUTTYROCKET, 0, 5000, "Kill 5000 enemies with the Slutty Rocket" },
	{ IT(ITEM_TOXICBARREL),     C_COUNT, CNT_POISON_KILLS, 0, 2000, "Kill 2000 enemies using Poison damage" },
	{ IT(ITEM_ROLLERBLADES),    C_TOMES_LEVEL, TOME_COOLDOWN, TOME_AGILITY, 5, "Upgrade both the Cooldown and Agility Tomes to level 5 in a run" },
	{ IT(ITEM_ANVIL),           C_COUNT, CNT_CHALLENGE_SHRINES, 0, 3, "(port) Complete 3 Challenge Shrines" },
	{ IT(ITEM_BLOODYCLEAVER),   C_LEVEL_AS, VLAD, 0, 50, "Reach level 50 as Vlad" },
	{ IT(ITEM_CHONKPLATE),      C_MAX_HP, 0, 0, 500, "Get Max HP to 500" },
	{ IT(ITEM_DRAGONFIRE),      C_COUNT, CNT_FIRE_KILLS, 0, 4000, "Kill 4000 enemies using fire damage" },
	{ IT(ITEM_ENERGYCORE),      C_COUNT, CNT_STAGE3_BOSS_KILLS, 0, 2, "(port) Defeat a stage 3 boss 2 times" },
	{ IT(ITEM_HOLYBOOK),        C_FIRST_BOSS_CLEAN, 0, 0, 0, "Kill a first Stage Boss without taking any damage that run" },
	{ IT(ITEM_JOESDAGGER),      C_WEAPON_KILLS, WEAPON_DEXECUTIONER, 0, 10000, "Kill 10000 enemies using the Dexecutioner" },
	{ IT(ITEM_LIGHTNINGORB),    C_COUNT, CNT_LIGHTNING_KILLS, 0, 3000, "Kill 3000 enemies using lightning damage" },
	{ IT(ITEM_POT),             C_FREE, 0, 0, 0, "Open every chest and pot in the first crypt on Graveyard" },
	{ IT(ITEM_SNEK),            C_FREE, 0, 0, 0, "Find in the crypt on Graveyard" },
	{ IT(ITEM_SOULHARVESTER),   C_FINAL_SWARM, 0, 0, 360, "Survive the Final Swarm for 6 minutes" },
	{ IT(ITEM_SPEEDBOI),        C_STAGE_CURSES, 0, 0, 2, "Complete a stage with at least 2 Boss Curses activated" },
	{ IT(ITEM_SUCKYMAGNET),     C_ALL_CHARGES, 0, 0, 0, "Complete every single Charge Shrine on the map" },
};
const int g_unlockCount = sizeof(g_unlockDefs) / sizeof(g_unlockDefs[0]);

#define UNLOCKED_BIT(u) (g_profile.unlockBits[(u) >> 3] & (1 << ((u) & 7)))

static int find(int kind, int index)
{
	for (int u = 0; u < g_unlockCount; u++)
		if (g_unlockDefs[u].kind == kind && g_unlockDefs[u].index == index) return u;
	return -1;
}

int unlock_find(int kind, int index) { return find(kind, index); }

bool unlock_done(int u)
{
#ifdef MB_DEBUG_START_TIME
	return true;   // dev builds: everything available
#endif
	return u < 0 || g_unlockDefs[u].cond == C_FREE || UNLOCKED_BIT(u) || g_profile.unlockAll;
}

bool unlocked_character(int c) { return unlock_done(find(UNL_CHARACTER, c)); }
bool unlocked_weapon(int w)    { return unlock_done(find(UNL_WEAPON, w)); }
bool unlocked_tome(int t)      { return unlock_done(find(UNL_TOME, t)); }
bool unlocked_item(int i)      { return unlock_done(find(UNL_ITEM, i)); }

// ---------------------------------------------------------------- unlocking

static int s_popup[8], s_popupHead, s_popupTail;
static float s_popupTime;
static bool s_dirty;
static float s_checkTimer;

static const char* unlock_name(int u)
{
	const UnlockDef* d = &g_unlockDefs[u];
	switch (d->kind) {
	case UNL_CHARACTER: return g_characters[d->index].name;
	case UNL_WEAPON:    return g_weaponDefs[d->index].name;
	case UNL_TOME:      return g_tomeNames[d->index];
	default:            return g_itemDefs[d->index].name;
	}
}

static void unlock(int u)
{
	if (u < 0 || UNLOCKED_BIT(u) || g_unlockDefs[u].cond == C_FREE) return;
	g_profile.unlockBits[u >> 3] |= 1 << (u & 7);
	s_dirty = true;
	if (((s_popupTail + 1) & 7) != s_popupHead) {
		s_popup[s_popupTail] = u;
		s_popupTail = (s_popupTail + 1) & 7;
	}
}

static void unlock_cond(int cond, bool (*ok)(const UnlockDef*))
{
	for (int u = 0; u < g_unlockCount; u++)
		if (g_unlockDefs[u].cond == cond && !UNLOCKED_BIT(u) && ok(&g_unlockDefs[u])) unlock(u);
}

void unlock_everything(void)
{
	g_profile.unlockAll = 1;
	profile_save();
}

void unlocks_init(void) { s_popupHead = s_popupTail = 0; }

void unlock_progress(int u, u32* have, u32* need)
{
	const UnlockDef* d = &g_unlockDefs[u];
	*have = 0;
	*need = 0;
	switch (d->cond) {
	case C_COUNT:        *have = g_profile.counters[d->a]; *need = d->target; break;
	case C_CHAR_KILLS:   *have = g_profile.charKills[d->a]; *need = d->target; break;
	case C_WEAPON_KILLS: *have = g_profile.weaponKills[d->a]; *need = d->target; break;
	default: break;
	}
	if (*have > *need) *have = *need;
}

// ---------------------------------------------------------------- counters

static bool ok_counter(const UnlockDef* d)  { return g_profile.counters[d->a] >= d->target; }
static bool ok_charkills(const UnlockDef* d) { return g_profile.charKills[d->a] >= d->target; }
static bool ok_wkills(const UnlockDef* d)   { return g_profile.weaponKills[d->a] >= d->target; }

void unlocks_count(Counter c, u32 amount)
{
	if (MENU_STATE(g.state)) return;
	g_profile.counters[c] += amount;
	s_dirty = true;   // checked (and saved) by unlocks_check
}

static int weapon_level(int id)
{
	for (int i = 0; i < g.numWeapons; i++)
		if (g.weapons[i].id == id) return g.weapons[i].level;
	return 0;
}

static bool ok_tome(const UnlockDef* d)     { return g.tomeLevels[d->a] >= (int)d->target; }
static bool ok_tomes(const UnlockDef* d)    { return g.tomeLevels[d->a] >= (int)d->target && g.tomeLevels[d->b] >= (int)d->target; }
static bool ok_tomefast(const UnlockDef* d) { return g.runTime < 600 && g.tomeLevels[d->a] >= (int)d->target; }
static bool ok_wlevel(const UnlockDef* d)   { return weapon_level(d->a) >= (int)d->target; }
static bool ok_levelas(const UnlockDef* d)  { return g.character == d->a && g.level >= (int)d->target; }
static bool ok_maxhp(const UnlockDef* d)    { return g.stats.maxHp >= d->target; }
static bool ok_diff(const UnlockDef* d)     { return g.difficulty * 100 >= d->target; }
static bool ok_swarm(const UnlockDef* d)    { return g.finalSwarm && g.time - STAGE->stageTime >= d->target; }
static bool ok_nohit(const UnlockDef* d)    { return g.noHitTime >= d->target; }
static bool ok_items(const UnlockDef* d)    { return items_count(d->a) >= (int)d->target; }
static bool ok_pots(const UnlockDef* d)     { return g.potsThisRun >= (int)d->target; }

void unlocks_check(void)
{
	unlock_cond(C_COUNT, ok_counter);
	unlock_cond(C_CHAR_KILLS, ok_charkills);
	unlock_cond(C_WEAPON_KILLS, ok_wkills);
	if (!MENU_STATE(g.state)) {
		unlock_cond(C_TOME_LEVEL, ok_tome);
		unlock_cond(C_TOMES_LEVEL, ok_tomes);
		unlock_cond(C_TOME_FAST, ok_tomefast);
		unlock_cond(C_WEAPON_LEVEL, ok_wlevel);
		unlock_cond(C_LEVEL_AS, ok_levelas);
		unlock_cond(C_MAX_HP, ok_maxhp);
		unlock_cond(C_DIFFICULTY, ok_diff);
		unlock_cond(C_FINAL_SWARM, ok_swarm);
		unlock_cond(C_NO_HIT, ok_nohit);
		unlock_cond(C_ITEM_COUNT, ok_items);
		unlock_cond(C_POTS_RUN, ok_pots);
	}
}

static float s_saveTimer;
static void save_if_dirty(float dt)
{
	if (!s_dirty || (s_saveTimer -= dt) > 0) return;
	s_saveTimer = 30.0f;
	s_dirty = false;
	profile_save();
}

void unlocks_update(float dt)
{
	if (g.state == STATE_PLAYING) {
		g.noHitTime += dt;
		if ((s_checkTimer -= dt) <= 0) { s_checkTimer = 1.0f; unlocks_check(); }
	}
	save_if_dirty(dt);
	if (s_popupTime > 0) s_popupTime -= dt;
}

const char* unlocks_popup(void)
{
	static char buf[64];
	if (s_popupTime > 0) return buf;
	if (s_popupHead == s_popupTail) return NULL;
	int u = s_popup[s_popupHead];
	s_popupHead = (s_popupHead + 1) & 7;
	snprintf(buf, sizeof(buf), "Unlocked: %s!", unlock_name(u));
	s_popupTime = 3.0f;
	audio_play_sfx(SFX_LEVELUP);
	return buf;
}

// ---------------------------------------------------------------- gameplay hooks

static bool weapon_is(u8 src, int a, int b, int c)
{
	return src == a || src == b || src == c;
}

static bool ok_drop(const UnlockDef* d) { return randf() * 1000 < d->target; }
static bool ok_tumble(const UnlockDef* d)
{
	return game_first_stage(g.stage) == d->a && STAGE->number == d->b && randf() * 100 < d->target;
}

void unlocks_on_kill(int idx, u8 src)
{
	const Enemy* e = &g.enemies[idx];
	const char* name = g_enemyDefs[e->def].name;
	g_profile.counters[CNT_KILLS]++;
	if (g.character < UNLOCK_CHARS) g_profile.charKills[g.character]++;
	if (src < WEAPON_COUNT) g_profile.weaponKills[src]++;
	if (strstr(name, "Skeleton")) g_profile.counters[CNT_SKELETON_KILLS]++;
	if (strstr(name, "Goblin")) g_profile.counters[CNT_GOBLIN_KILLS]++;
	if (strstr(name, "Cactus") && src == DMG_SRC_THORNS) g_profile.counters[CNT_CACTUS_THORN_KILLS]++;
	int map = game_first_stage(g.stage);
	if (map == DESERT && src == WEAPON_TORNADO) g_profile.counters[CNT_TORNADO_DESERT_KILLS]++;
	if (map == DESERT && g.character == FOX && strstr(name, "Wisp")) g_profile.counters[CNT_FOX_DESERT_WISPS]++;
	if (src == DMG_SRC_POISON || src == WEAPON_POISONFLASK) g_profile.counters[CNT_POISON_KILLS]++;
	if (weapon_is(src, WEAPON_FIRESTAFF, WEAPON_FLAMEWALKER, WEAPON_DRAGONBREATH)) g_profile.counters[CNT_FIRE_KILLS]++;
	if (src == WEAPON_LIGHTNING || src == WEAPON_WIRELESSDAGGER) g_profile.counters[CNT_LIGHTNING_KILLS]++;
	if (g.stillTime > 0.5f) g_profile.counters[CNT_STILL_KILLS]++;
	if (strstr(name, "Miniboss_Scorpion")) g_profile.counters[CNT_SCORPION_MINIBOSS_KILLS]++;
	s_dirty = true;
	unlock_cond(C_DROP, ok_drop);
	if (strstr(name, "Tumblebone")) unlock_cond(C_DROP_TUMBLE, ok_tumble);
}

static float s_bossHpFrac, s_bossFight;
static u8 s_bossSrc;
static bool ok_bossfast(const UnlockDef* d)  { return s_bossFight <= d->target; }
static bool ok_bosslow(const UnlockDef* d)   { return s_bossHpFrac * 100 < d->target; }
static bool ok_bossclean(const UnlockDef* d) { (void)d; return !g.stageTookItems; }
static bool ok_first(const UnlockDef* d)     { (void)d; return STAGE->number == 1 && !g.runDamaged; }
static bool ok_bossas(const UnlockDef* d)    { return g.character == d->a && s_bossSrc == d->b; }
static bool ok_stagefast(const UnlockDef* d) { return g.time <= d->target; }

void unlocks_on_boss_kill(int idx, bool boss)
{
	const Enemy* e = &g.enemies[idx];
	s_bossFight = g.time - e->spawnTime;
	s_bossHpFrac = g.hp / g.stats.maxHp;
	s_bossSrc = g.dmgSource;
	if (!boss) {
		unlock_cond(C_MINIBOSS_FAST, ok_bossfast);
		return;
	}
	unlock_cond(C_BOSS_FAST, ok_bossfast);
	unlock_cond(C_BOSS_LOW_HP, ok_bosslow);
	unlock_cond(C_BOSS_NO_ITEMS, ok_bossclean);
	unlock_cond(C_FIRST_BOSS_CLEAN, ok_first);
	unlock_cond(C_BOSS_AS_WITH, ok_bossas);
	unlock_cond(C_STAGE_FAST, ok_stagefast);
	if (STAGE->number == 3) g_profile.counters[CNT_STAGE3_BOSS_KILLS]++;
	s_dirty = true;
}

static bool ok_clear(const UnlockDef* d)   { return game_first_stage(g.stage) == d->a && STAGE->number == d->b; }
static bool ok_clearas(const UnlockDef* d) { return ok_clear(d) && g.character == (int)d->target; }
static bool ok_curses(const UnlockDef* d)  { return g.cursesThisStage >= (int)d->target; }
static bool ok_charges(const UnlockDef* d)
{
	if (d->a && STAGE->number != d->a) return false;
	int total = 0, done = 0;
	for (int i = 0; i < g.numInteract; i++)
		if (g.interact[i].kind == INT_SHRINE_CHARGE) { total++; done += g.interact[i].used; }
	return total > 0 && done == total;
}

void unlocks_on_stage_clear(void)
{
	unlock_cond(C_STAGE_CLEAR, ok_clear);
	unlock_cond(C_STAGE_CLEAR_AS, ok_clearas);
	unlock_cond(C_STAGE_CURSES, ok_curses);
	unlock_cond(C_ALL_CHARGES, ok_charges);
	unlocks_check();
}

static bool ok_streak(const UnlockDef* d) { return g.microwaveItem == d->a && g.microwaveStreak >= (int)d->target; }

void unlocks_on_microwave(int item)
{
	if (g.microwaveItem == item) g.microwaveStreak++;
	else { g.microwaveItem = item; g.microwaveStreak = 1; }
	unlock_cond(C_MICROWAVE_STREAK, ok_streak);
}

void unlocks_on_damage_taken(void)
{
	g.noHitTime = 0;
	g.runDamaged = true;
}
