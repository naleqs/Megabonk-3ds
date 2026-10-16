#pragma once
#include <3ds.h>
#include <stdbool.h>

// Lifetime counters (kept in the profile). Append only: the profile stores them by index.
typedef enum {
	CNT_KILLS, CNT_SKELETON_KILLS, CNT_GOBLIN_KILLS, CNT_MOLDY_CHEESE_PROCS, CNT_TORNADO_DESERT_KILLS,
	CNT_CRITS, CNT_CHESTS_BOUGHT, CNT_CACTUS_THORN_KILLS, CNT_LIFESTEAL_HEALING, CNT_BLOOD_MAGIC_PROCS,
	CNT_POTS_BROKEN, CNT_STILL_KILLS, CNT_POISON_KILLS, CNT_FIRE_KILLS, CNT_LIGHTNING_KILLS,
	CNT_PROJECTILES_FIRED, CNT_AEGIS_BLOCKS, CNT_OOFIE_ARMOR_BLOCKED, CNT_FOX_DESERT_WISPS, CNT_ICECUBE_FREEZES,
	CNT_SCORPION_MINIBOSS_KILLS, CNT_DESERT_SHRINE_CHARGES, CNT_MAGNET_SHRINES, CNT_CHALLENGE_SHRINES,
	CNT_DEATHS, CNT_STAGE3_BOSS_KILLS,
	CNT_COUNT
} Counter;

#define MAX_UNLOCKS 160
#define UNLOCK_CHARS 32
#define UNLOCK_WEAPONS 32

typedef enum { UNL_CHARACTER, UNL_WEAPON, UNL_TOME, UNL_ITEM } UnlockKind;

typedef struct {
	u8 kind;            // UnlockKind
	u8 index;           // character / WeaponId / TomeId / ItemId
	u8 cond;            // see unlocks.c
	u8 a, b;            // condition parameters
	u32 target;
	const char* text;   // the game's requirement text (or the port's substitute, marked "(port)")
} UnlockDef;

extern const UnlockDef g_unlockDefs[];
extern const int g_unlockCount;

void unlocks_init(void);                 // after the profile is loaded
bool unlocked_character(int c);
bool unlocked_weapon(int w);
bool unlocked_tome(int t);
bool unlocked_item(int i);
bool unlock_done(int u);
int  unlock_find(int kind, int index);   // -1 if it has no requirement
void unlock_progress(int u, u32* have, u32* need);   // for the unlocks screen (need 0 = no counter)
void unlock_everything(void);            // cheat (held SELECT on the unlocks screen)

void unlocks_count(Counter c, u32 amount);
void unlocks_check(void);                // run-state conditions (tome levels, level, ...); call ~once a second
void unlocks_update(float dt);           // timers + popups
const char* unlocks_popup(void);         // name of something just unlocked, or NULL

// gameplay hooks
void unlocks_on_kill(int enemyIdx, u8 source);   // source: weapon id, or DMG_SRC_*
void unlocks_on_boss_kill(int enemyIdx, bool boss);
void unlocks_on_stage_clear(void);
void unlocks_on_microwave(int item);
void unlocks_on_damage_taken(void);

// damage sources that aren't weapons
#define DMG_SRC_NONE   0xFF
#define DMG_SRC_POISON 0xFE
#define DMG_SRC_THORNS 0xFD
