#pragma once
#include <3ds.h>
#include <stdbool.h>

#define MAX_PROFILE_CHARS  32
#define MAX_PROFILE_STAGES 8

// Lifetime records (sdmc:/3ds/megabonk3ds/profile.sav). Append new fields at the end only.
typedef struct {
	u32 magic, size;
	u32 runs, wins, totalKills, bestKills, bestLevel, totalTime, bossesKilled;
	u16 charRuns[MAX_PROFILE_CHARS], charWins[MAX_PROFILE_CHARS];
	u8  bestStage[MAX_PROFILE_STAGES];     // furthest stage reached, per starting stage
	u32 bestWinTime[MAX_PROFILE_STAGES];   // fastest full run, seconds (0 = never won)
	// unlocks (unlocks.c)
	u32 counters[48];
	u32 charKills[32], weaponKills[32];
	u8  unlockBits[160 / 8];
	u8  unlockAll;                         // the unlock-everything cheat was used
} SaveProfile;

typedef struct {
	u32 magic, size;
	char build[32];
} RunSaveHeader;

extern SaveProfile g_profile;

void save_init(void);          // load the profile, check for a suspended run
void profile_save(void);
void profile_run_start(void);
void profile_stage_cleared(void);
void profile_run_end(bool won);

bool run_save_exists(void);
bool run_save(void);           // suspend the current run
bool run_load(void);           // resume it (the save is removed); false if it couldn't be read
void run_save_delete(void);
