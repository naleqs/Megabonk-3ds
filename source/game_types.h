#pragma once
#include <3ds.h>
#include <stdbool.h>
#include "audio.h"

typedef struct { float x, y, z; } vec3;

typedef enum {
	WEAPON_FIRESTAFF,
	WEAPON_AURA,
	WEAPON_BONE,
	WEAPON_SWORD,
	WEAPON_REVOLVER,
	WEAPON_LIGHTNING,
	WEAPON_AXE,
	WEAPON_DICE,
	// starting weapons of the rest of the roster
	WEAPON_POISONFLASK,
	WEAPON_AEGIS,
	WEAPON_DEXECUTIONER,
	WEAPON_TORNADO,
	WEAPON_SNIPER,
	WEAPON_CORRUPTEDSWORD,
	WEAPON_BANANARANG,
	WEAPON_KATANA,
	WEAPON_FROSTWALKER,
	WEAPON_SCYTHE,
	WEAPON_BOW,
	WEAPON_BLACKHOLE,
	WEAPON_WIRELESSDAGGER,
	WEAPON_BLOODMAGIC,
	// level-up only
	WEAPON_SHOTGUN,
	WEAPON_MINES,
	WEAPON_SLUTTYROCKET,
	WEAPON_SPACENOODLE,
	WEAPON_HEROSWORD,
	WEAPON_CHUNKERS,
	WEAPON_FLAMEWALKER,
	WEAPON_DRAGONBREATH,
	WEAPON_COUNT
} WeaponId;

// Character passive abilities (CharacterData.passive), see passives.c
typedef enum {
	PASSIVE_NONE,
	PASSIVE_RNG, PASSIVE_SPEEDDEMON, PASSIVE_REINFORCED, PASSIVE_CRIT, PASSIVE_FLEX, PASSIVE_WARRIOR,
	PASSIVE_LOCKIN, PASSIVE_FLOAT, PASSIVE_BULLSEYE, PASSIVE_FLOWSTATE, PASSIVE_MONKEGRIP, PASSIVE_ENDURING,
	PASSIVE_STONKS, PASSIVE_ZAP, PASSIVE_PLAGUE, PASSIVE_QUANTUM, PASSIVE_SHADOWSTEP, PASSIVE_VAMPIRE,
	PASSIVE_GAMBA, PASSIVE_CURSE, PASSIVE_HOARDER,
	PASSIVE_COUNT
} PassiveId;

typedef struct {
	const char* name;
	const char* mesh;
	WeaponId startWeapon;
	float height;
	const char* portrait;  // texture name
	PassiveId passive;
} CharacterDef;

enum {
	ENEMY_FLYING          = 1 << 0,
	ENEMY_MINIBOSS        = 1 << 1,
	ENEMY_BOSS            = 1 << 2,
	ENEMY_NO_RANDOM_SPAWN = 1 << 3,
};

typedef struct {
	const char* name;
	const char* mesh;
	int hp;
	int damage;
	float speed;
	int xp;
	float radius;
	float mass;
	float cost;
	float spawnAfter;
	u32 flags;
	float tint[3];
	u8 maps;              // EnemyData.maps: which stages spawn it randomly (StageDef.mapBit)
} EnemyDef;

typedef struct {
	const char* mesh;
	int amount;
	float scaleMin, scaleMax;
	float radius;
} PropDef;

// Stage timeline event: type 1 = miniboss, type 2 = swarm
typedef struct { int type; float minute, duration; int enemies[4]; int count; } StageEvent;

typedef struct {
	const char* name;
	const char* icon;          // texture
	u8 mapBit;                 // matches EnemyDef.maps
	float stageTime, newEnemyInterval;
	float fog[3], ambient[3], light[3];
	const char* groundTex;
	const char* cliffTex;
	int startEnemies[4];
	int startCount;
	const StageEvent* events;
	int eventCount;
	int minibosses[4];         // fallback when a miniboss event lists none
	int minibossCount;
	int boss;                  // EnemyDef index, -1 if none
	int propFirst, propCount;  // range in g_propDefs
	MusicId music, bossMusic;
} StageDef;
