#pragma once
#include "game_types.h"

// Items found in chests / bought from Shady Guy. Tiers follow the game's
// Gray / Blue / Pink / Yellow tables (common / rare / epic / legendary).
typedef enum {
	// common
	ITEM_KEY, ITEM_BORGAR, ITEM_BOSSBUSTER, ITEM_GOLDENGLOVE, ITEM_GYMSAUCE, ITEM_MEDKIT,
	ITEM_MOLDYCHEESE, ITEM_OATS, ITEM_SLIPPERYRING, ITEM_TACTICALGLASSES, ITEM_TURBOSOCKS,
	ITEM_ICECRYSTAL, ITEM_TIMEBRACELET, ITEM_BATTERY, ITEM_WRENCH, ITEM_CACTUS, ITEM_CURSEDDOLL,
	// rare
	ITEM_BACKPACK, ITEM_BEER, ITEM_BRASSKNUCKLES, ITEM_CAMPFIRE, ITEM_DEMONBLADE, ITEM_DEMONICBLOOD,
	ITEM_ECHOSHARD, ITEM_ELECTRICPLUG, ITEM_FEATHERS, ITEM_GOLDENSHIELD, ITEM_GOLDENSNEAKERS,
	ITEM_LEECHINGCRYSTAL, ITEM_THUNDERMITTS, ITEM_MOLDYGLOVES,
	// epic
	ITEM_BEEFYRING, ITEM_MIRROR, ITEM_SCARF, ITEM_SLUTTYCANNON, ITEM_SPIKYSHIELD, ITEM_ZAWARUDO,
	ITEM_BOBSLANTERN, ITEM_DEMONICSOUL, ITEM_SLURPGLOVES, ITEM_CURSEDGRABBIES, ITEM_ROLLERBLADES,
	// legendary
	ITEM_ANVIL, ITEM_BONKER, ITEM_DRAGONFIRE, ITEM_ICECUBE, ITEM_LIGHTNINGORB, ITEM_SOULHARVESTER,
	ITEM_SPICYMEATBALL, ITEM_SUCKYMAGNET, ITEM_POWERGLOVES, ITEM_GOLDENRING, ITEM_GIANTFORK,
	ITEM_ENERGYCORE, ITEM_HOLYBOOK,
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
int   items_max_jumps(void);
float items_jump_mult(void);
