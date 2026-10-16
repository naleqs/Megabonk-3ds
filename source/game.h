#pragma once
#include "game_types.h"
#include "render.h"
#include "gen/assets_gen.h"
#include "items.h"

#define MAP_HALF          140.0f
#define MAX_INTERACT      80
#define MAX_OWNED_ITEMS   64   // distinct item kinds shown in the HUD
#define MAX_CHOICE_STATS  3
#define MAX_ENEMIES       120
#define MAX_PROJECTILES   128
#define MAX_PICKUPS       240
#define MAX_FX            96
#define MAX_PROPS         640
#define MAX_WEAPON_SLOTS  4
#define MAX_TOME_SLOTS    4
#define MAX_DAMAGE_NUMS   24
#define LEVELUP_CHOICES   3

// ---------------------------------------------------------------- stats / tomes

typedef enum {
	TOME_DAMAGE, TOME_COOLDOWN, TOME_AGILITY, TOME_HEALTH, TOME_REGEN, TOME_ARMOR,
	TOME_EVASION, TOME_ATTRACTION, TOME_SIZE, TOME_QUANTITY, TOME_PRECISION, TOME_XP,
	TOME_LUCK, TOME_PROJSPEED, TOME_SHIELD, TOME_THORNS, TOME_BLOODY, TOME_KNOCKBACK, TOME_GOLDEN,
	TOME_DURATION, TOME_CURSED, TOME_CHAOS, TOME_COUNT
} TomeId;

typedef struct {
	float maxHp, regen, armor, evasion;
	float moveSpeed, damageMult, attackSpeed, sizeMult, durationMult, projSpeedMult;
	int extraProjectiles;
	float critChance, critMult, pickupRange, xpMult, luck, knockback;
	float goldMult;
	float thorns;
	float shieldMax;    // absorbs damage first, refills after a few seconds without being hit
	float overheal;     // healing can exceed Max HP by this fraction (Chonkplate)
	float lifesteal;    // chance to heal 1 HP per hit
} PlayerStats;

// ---------------------------------------------------------------- weapons

// Weapon upgrade stats (UpgradeData). DAMAGE is flat, CRIT/CRITDMG add percentage points,
// PROJECTILES/BOUNCES are counts, the rest are +x% multipliers.
typedef enum {
	STAT_DAMAGE, STAT_SIZE, STAT_DURATION, STAT_SPEED, STAT_PROJECTILES, STAT_BOUNCES,
	STAT_CRIT, STAT_CRITDMG, STAT_KNOCKBACK, STAT_COUNT
} WeaponStat;

#define MAX_WEAPON_UPGRADES 6
typedef struct { u8 stat; float amount; } WeaponUpgrade;

typedef struct {
	const char* name;
	const char* desc;
	// from the game's WeaponData / UpgradeData
	float damage, knockback, crit;
	int projectiles, bounces;
	float cooldown;
	int nups;
	WeaponUpgrade ups[MAX_WEAPON_UPGRADES];
	// engine units (hand-tuned for this port)
	float speed, duration;
} WeaponDef;

typedef struct {
	WeaponId id;
	int level;
	float damage, cooldown, size, speed, duration;
	float crit, critDmg, knockback;   // per-weapon bonuses on top of the player's stats
	int projectiles, bounces;
	float timer;
	float burstTimer;
	int burstLeft;
	float orbit;                      // Chunkers: orbit angle / Flamewalker: trail timer
	float tick;
} Weapon;

extern const WeaponDef g_weaponDefs[WEAPON_COUNT];
extern const char* const g_tomeNames[TOME_COUNT];
extern const char* const g_tomeDescs[TOME_COUNT];
extern const char* const g_tomeIcons[TOME_COUNT];
extern const char* const g_weaponIcons[WEAPON_COUNT];

// ---------------------------------------------------------------- entities

typedef struct {
	bool active;
	u8 def;
	bool elite;
	vec3 pos, vel;
	float hp, maxHp;
	float animTime;
	float hitFlash;
	float attackCd;
	float yaw;
	float scale;
	// status effects
	float poisonTime, poisonDps;
	float freezeTime;
	bool noChest;        // challenge elites: the shrine pays out instead
	bool marked;         // Bullseye passive: explodes on death
	u8 bloodmarks;       // Bloodmark stacks (Unstable Transfusion / Bloody Cleaver)
	float bloodTimer;
	float launched;      // Eagle Claw: knocked into the air (stunned, counts as airborne)
	u16 cursedTick;      // Cursed Doll: last curse tick that hit this enemy
} Enemy;

typedef enum {
	PROJ_FIREBALL, PROJ_BONE, PROJ_BULLET, PROJ_AXE, PROJ_DICE,
	PROJ_SOUL, PROJ_ROCKET, PROJ_ORB, PROJ_SPIKE,   // item projectiles (home in / no procs)
	PROJ_FLASK, PROJ_BLADE, PROJ_SNIPE, PROJ_BANANA, PROJ_ARROW, PROJ_HOLE, PROJ_DAGGER,
	PROJ_PELLET, PROJ_WAVE, PROJ_MISSILE,
} ProjKind;

// Lingering weapon areas
typedef enum { ZONE_POISON, ZONE_TORNADO, ZONE_BLACKHOLE, ZONE_FIRE, ZONE_MINE } ZoneKind;
#define MAX_ZONES 40
typedef struct {
	bool active;
	u8 kind;
	u8 weapon;
	vec3 pos, vel;
	float radius, life, age, tick, damage;
} Zone;

// game_damage_enemy flags
enum { DMG_NO_PROC = 1 << 0, DMG_NO_CRIT = 1 << 1 };

typedef struct {
	bool active;
	u8 kind;
	u8 weapon;
	vec3 pos, vel;
	float damage, size, life, age;
	int bounces;
	int pierce;
	int id;
	int lastHit;
	float spin;
	s16 hits[8];   // piercing shots: enemies already hit
	u8 nhits;
} Projectile;

typedef enum { PICKUP_XP, PICKUP_HEART, PICKUP_MAGNET, PICKUP_CHEST, PICKUP_GOLD } PickupKind;

typedef struct {
	bool active;
	u8 kind;
	bool attracted;
	vec3 pos;
	float value;
	float bob;
	float hitCd;         // Shattered Knowledge: slice cooldown
} Pickup;

// FX_SLASH sweeps a blade across an arc (size = range, color tints the blade);
// FX_SPIN sweeps it a full circle (scythe).
typedef enum { FX_EXPLOSION, FX_RING, FX_SLASH, FX_LIGHTNING, FX_DEATH, FX_SPAWN, FX_LEVELUP, FX_POISON, FX_SPIN, FX_BEAM } FxKind;
// FX_BEAM: a line from pos along yaw, size = length

typedef struct {
	bool active;
	u8 kind;
	vec3 pos;
	float yaw, size, age, life;
	Color color;
} Fx;

typedef struct {
	bool active;
	vec3 pos;
	float value, age;
	bool crit;
} DamageNum;

typedef struct {
	u8 def;
	vec3 pos;
	float yaw, scale;
	float colRadius; // 0 = no collision
} Prop;

// ---------------------------------------------------------------- level up

// CHOICE_STAT = a one-off stat boost (charge shrines), index is a TomeId
typedef enum { CHOICE_NEW_WEAPON, CHOICE_UPGRADE_WEAPON, CHOICE_TOME, CHOICE_STAT } ChoiceKind;
typedef enum { RARITY_COMMON, RARITY_UNCOMMON, RARITY_RARE, RARITY_EPIC, RARITY_LEGENDARY, RARITY_COUNT } Rarity;

typedef struct {
	ChoiceKind kind;
	int index;         // weapon slot, weapon id or tome id
	Rarity rarity;
	WeaponStat stat[MAX_CHOICE_STATS];
	float amount[MAX_CHOICE_STATS];
	int nstats;
} LevelChoice;

// ---------------------------------------------------------------- interactables

typedef enum {
	INT_CHEST, INT_CHEST_FREE, INT_POT, INT_SHRINE_CHARGE, INT_SHRINE_GREED, INT_SHRINE_CURSED,
	INT_SHRINE_MAGNET, INT_SHRINE_MOAI, INT_SHRINE_CHALLENGE, INT_SHADY, INT_MICROWAVE,
	INT_BOSS_PORTAL, INT_KIND_COUNT
} InteractKind;

typedef struct {
	bool active;
	u8 kind;
	bool golden;         // golden charge shrine: better rewards
	bool used;           // spent shrines stay in the world, dimmed
	vec3 pos;
	float yaw;
	float progress;      // charge shrine 0..1
	float timer;         // challenge shrine wave
	u8 shopRarity;
	s8 shopItems[3];
	u16 shopPrices[3];
} Interactable;

// ---------------------------------------------------------------- game

typedef enum {
	STATE_TITLE, STATE_CHARSELECT, STATE_PLAYING, STATE_LEVELUP, STATE_PAUSED, STATE_DEAD, STATE_WIN,
	STATE_CHEST,      // item reveal (chest / moai)
	STATE_SHOP,       // Shady Guy
	STATE_MICROWAVE,  // pick an item to clone
	STATE_STAGESELECT,
} GameState;

#define STAGE (&g_stages[g.stage])
#define MENU_STATE(s) ((s) == STATE_TITLE || (s) == STATE_CHARSELECT || (s) == STATE_STAGESELECT)

typedef struct {
	GameState state;
	int character;
	int stage;           // index into g_stages
	int stageCursor;     // stage select screen
	float time;          // run time in seconds
	float realTime;

	// player
	vec3 pos, vel;
	float yaw;           // facing
	bool grounded;
	float hp;
	float shield;
	float shieldDelay;   // seconds until the shield starts refilling
	float iframes;
	float hurtFlash;
	int level;
	float xp, xpNext;
	int kills, gold;
	PlayerStats stats;
	Weapon weapons[MAX_WEAPON_SLOTS];
	int numWeapons;
	int tomeLevels[TOME_COUNT];
	int tomeSlots[MAX_TOME_SLOTS];
	int numTomes;
	float moveAnim;
	float animTime, jumpTime;   // character animation clocks
	int jumpsLeft;
	int aegisCharges;
	float wCrit, wCritDmg, wKnock;   // damage context of the weapon currently hitting (weapons.c)

	// passive ability state (passives.c)
	float passiveTimer[4];
	float speedDemon;     // Calcium: built-up speed bonus 0..1
	float flexBuff;       // Megachad: permanent damage bonus from flexing
	float zapCharge;      // Tony: distance moved toward the next zap
	int lastAttacker;     // enemy that last hit (or tried to hit) the player

	// items
	u8 itemCounts[ITEM_COUNT];
	u8 itemOrder[MAX_OWNED_ITEMS];   // acquisition order for the HUD
	int numItemKinds;
	int killStacks;                  // Demonic Blood / Soul
	float itemTimers[16];
	float stillTime, movedDist, bobDist;
	int cardChests;                  // chests opened (Credit Cards)
	int joeStacks;                   // Joe's Dagger executes
	bool phantomNext;                // Phantom Shroud: next hit deals 2x
	float holyHeal;                  // Holy Book: healing banked toward the next pulse
	u16 dollTick;

	// camera
	float camYaw, camPitch, camDist;

	// world
	Prop props[MAX_PROPS];
	int numProps;
	Mesh* propMeshes[PROP_COUNT];
	Mesh* enemyMeshes[ENEMY_COUNT];
	Mesh* charMesh;
	Mesh* meshBone, *meshAxe, *meshDice, *meshSword, *meshChest, *meshCoins;
	Mesh* meshBanana, *meshArrow, *meshDagger, *meshFlask;
	Mesh* interactMeshes[INT_KIND_COUNT];
	Interactable interact[MAX_INTERACT];
	int numInteract;
	int nearInteract;     // index of the interactable in range, or -1
	int chestsOpened;
	int activeShop;       // interactable index while in STATE_SHOP / STATE_MICROWAVE
	int revealItem;       // STATE_CHEST
	const char* revealText;
	int shopCursor;

	Enemy enemies[MAX_ENEMIES];
	int numEnemies;
	Projectile projectiles[MAX_PROJECTILES];
	int projIdCounter;
	Zone zones[MAX_ZONES];
	Pickup pickups[MAX_PICKUPS];
	Fx fx[MAX_FX];
	DamageNum dmgNums[MAX_DAMAGE_NUMS];

	// director (follows StageData.stageTimeline)
	float credits;
	float spawnTimer;
	u8 enemyPool[ENEMY_COUNT];
	int poolSize;
	float nextPoolTime;
	bool eventDone[16];
	int swarmEnemy;       // -1 when no swarm is active
	float swarmTime;
	int minibossToggle;
	float difficulty;     // greed / curse shrines raise this
	bool finalSwarm;
	bool bossSpawned, bossDead;
	int bossIndex;
	int challengeIndex;   // active challenge shrine, or -1

	// level up
	int pendingLevels;
	LevelChoice choices[LEVELUP_CHOICES];
	int choiceCursor;
	float choiceLock;     // ignore input briefly so a held button doesn't auto-pick
	int rerolls;

	// misc
	float auraTimer;
	float swordSwing;     // > 0 while sword fx is visible
	float swordYaw;
	int menuCursor;
	char toast[64];
	float toastTime;
} Game;

extern Game g;
extern float g_fps;   // measured in main.c, shown on the bottom screen

// game.c
void game_preload(void);
void game_select_character(int character);
Mesh* game_displayed_character(void);
void game_new_run(int character, int stage);
void game_poll_character(void);   // finishes background character loads (call outside a frame)

// passives.c
extern const char* const g_passiveNames[PASSIVE_COUNT];
extern const char* const g_passiveDescs[PASSIVE_COUNT];
PassiveId passive_current(void);
void  passive_init(void);                   // start-of-run bonuses
float passive_jump_mult(void);
void  passive_on_level_up(void);
void  passive_update(float dt);
float passive_damage_mult(int enemyIdx);
void  passive_on_damage_dealt(int enemyIdx, float dmg);
bool  passive_on_player_hit(float* dmg);    // true = hit negated
void  passive_on_evade(void);
void  passive_on_kill(int enemyIdx);
float passive_fall_limit(void);             // max fall speed while jump is held (0 = none)
void game_update(float dt);
void game_damage_enemy(int idx, float dmg, vec3 knockDir, float knock);
void game_damage_enemy_ex(int idx, float dmg, vec3 knockDir, float knock, u32 flags);
void game_area_damage(vec3 center, float radius, float dmg, float knock, u32 flags);
void game_damage_player(float dmg);
void game_heal(float hp);
void game_spawn_pickup(PickupKind kind, vec3 pos, float value);
void game_attract_all_xp(void);
int  game_spawn_enemy(int def, vec3 pos, bool elite);
int  game_find_enemy_def(const char* name);
vec3 game_random_spawn_point(float minR, float maxR);
void game_fire_item_projectile(ProjKind kind, vec3 from, vec3 dir, float speed, float dmg, float life);
void game_roll_stat_choices(Rarity minRarity);
float game_enemy_hp_mult(void);
void game_add_xp(float xp);
void game_apply_choice(const LevelChoice* c);
void game_roll_choices(void);
float tome_amount(int tome, Rarity r);
bool tome_is_percent(int tome);
float tome_display(int tome, float amount);   // value for g_tomeDescs
int  game_nearest_enemy(vec3 from, float maxDist, int exclude);
Fx* fx_spawn(FxKind kind, vec3 pos, float size, float life, Color color);
void game_toast(const char* msg);

// weapons.c
void weapons_init_slot(Weapon* w, WeaponId id);
void weapons_update(float dt);
void projectiles_update(float dt);
void weapon_apply_upgrade(Weapon* w, WeaponStat stat, float amount);
void zones_update(float dt);
bool weapons_try_block(void);   // Aegis: true if an incoming hit was blocked
const char* weapon_stat_name(WeaponStat s);
float weapon_stat_roll(WeaponId id, WeaponStat s, Rarity r);
void weapon_format_stat(WeaponStat s, float amount, char* buf, size_t n);
int  weapons_orbiters(vec3* out, int max);   // Chunkers rock positions (for drawing)

// interact.c
void interact_generate(void);
void interact_update(float dt);
void interact_use(void);           // X pressed
void interact_draw(vec3 camPos, vec3 camFwd);
const char* interact_prompt(char* buf, size_t n);   // NULL if nothing in range
void interact_open_item(int item, const char* text); // -> STATE_CHEST
void shop_buy(int slot);
int  shop_price(const Interactable* it, int slot);
void microwave_use(int ownedSlot);

// world.c
void world_generate(void);
void world_clear_area(vec3 pos, float radius);
void world_collide(vec3* pos, float radius);
void world_draw(vec3 camPos, vec3 camFwd);

// draw.c
void draw_scene(C3D_RenderTarget* top);
void draw_hud_top(void);
void draw_bottom(void);
void hud_init(void);
void hud_exit(void);
