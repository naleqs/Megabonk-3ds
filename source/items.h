#pragma once
#include "game_types.h"

// Items found in chests / bought from Shady Guy. Tiers are the game's own ItemData rarities.
typedef enum {
	// common
	ITEM_KEY, ITEM_BORGAR, ITEM_BOSSBUSTER, ITEM_GOLDENGLOVE, ITEM_GYMSAUCE, ITEM_MEDKIT,
	ITEM_MOLDYCHEESE, ITEM_OATS, ITEM_SLIPPERYRING, ITEM_TACTICALGLASSES, ITEM_TURBOSOCKS,
	ITEM_ICECRYSTAL, ITEM_TIMEBRACELET, ITEM_BATTERY, ITEM_WRENCH, ITEM_CACTUS, ITEM_CURSEDDOLL,
	ITEM_CLOVER, ITEM_FORBIDDENJUICE, ITEM_GHOST, ITEM_SKULEG, ITEM_OLDMASK,
	// rare
	ITEM_BACKPACK, ITEM_BEER, ITEM_BRASSKNUCKLES, ITEM_CAMPFIRE, ITEM_DEMONBLADE, ITEM_DEMONICBLOOD,
	ITEM_ECHOSHARD, ITEM_ELECTRICPLUG, ITEM_FEATHERS, ITEM_GOLDENSHIELD, ITEM_GOLDENSNEAKERS,
	ITEM_LEECHINGCRYSTAL, ITEM_THUNDERMITTS, ITEM_MOLDYGLOVES, ITEM_CREDITCARDRED, ITEM_PHANTOMSHROUD,
	ITEM_UNSTABLETRANSFUSION, ITEM_IDLEJUICE, ITEM_COWARDSCLOAK,
	// epic
	ITEM_BEEFYRING, ITEM_MIRROR, ITEM_SCARF, ITEM_SLUTTYCANNON, ITEM_SPIKYSHIELD,
	ITEM_BOBSLANTERN, ITEM_DEMONICSOUL, ITEM_SLURPGLOVES, ITEM_CURSEDGRABBIES, ITEM_ROLLERBLADES,
	ITEM_BOBDEAD, ITEM_GASMASK, ITEM_EAGLECLAW, ITEM_QUINSMASK, ITEM_CREDITCARDGREEN, ITEM_KEVIN,
	ITEM_TOXICBARREL, ITEM_GAMERGOGGLES, ITEM_SHATTEREDKNOWLEDGE, ITEM_GRANDMASTONIC,
	// legendary
	ITEM_ANVIL, ITEM_BONKER, ITEM_DRAGONFIRE, ITEM_ICECUBE, ITEM_LIGHTNINGORB, ITEM_SOULHARVESTER,
	ITEM_SPICYMEATBALL, ITEM_SUCKYMAGNET, ITEM_POWERGLOVES, ITEM_GOLDENRING, ITEM_GIANTFORK,
	ITEM_ENERGYCORE, ITEM_HOLYBOOK, ITEM_ZAWARUDO, ITEM_SPEEDBOI, ITEM_JOESDAGGER, ITEM_CHONKPLATE,
	ITEM_POT, ITEM_SNEK, ITEM_BLOODYCLEAVER, ITEM_OVERPOWEREDLAMP,
	ITEM_COUNT
} ItemId;

typedef struct {
	const char* name;
	const char* icon;   // romfs texture name
	u8 rarity;          // Rarity
	const char* desc;
} ItemDef;

extern const ItemDef g_itemDefs[ITEM_COUNT];

int  items_count(ItemId id);                 // stacks owned
void items_give(ItemId id);                  // add one stack (applies passive stats)
bool items_take(ItemId id);                  // remove one stack (microwave)
int  items_roll(u8 rarity);                  // random item of a tier
u8   items_roll_tier(float luckBonus);       // chest tier roll

// Hooks called from the game loop
void  items_update(float dt);
float items_modify_damage(int enemyIdx, float dmg);     // conditional damage bonuses
void  items_on_hit(int enemyIdx, float dmg, bool crit); // procs (only for direct weapon hits)
void  items_on_kill(int enemyIdx);
float items_on_player_damaged(float dmg);               // may reduce/negate; returns damage to apply
bool  items_on_fatal(void);                             // true if death was prevented
void  items_on_evade(void);
void  items_on_heal(float amount);
void  items_on_interact(void);                          // pots, chests, shrines (Ghost)
void  items_on_chest_open(void);                        // Credit Cards
int   items_max_jumps(void);
float items_jump_mult(void);
float items_speed_mult(void);                           // temporary movement buffs
float items_attack_speed_mult(void);
float items_megacrit_mult(void);                        // Giant Fork
float items_key_chance(void);
float items_elite_mult(void);                           // Old Mask
int   items_weapon_level_cap(void);                     // Pot (stainless steel)
float items_enemy_time_scale(void);                     // Speed Boi slow-mo
