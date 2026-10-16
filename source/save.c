#include "game.h"
#include "save.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Two files next to the app on the SD card:
//   profile.sav  lifetime records (kept forever, survives updates)
//   run.sav      a suspended run (the whole Game state), deleted once it's resumed or the run ends.
//                Only valid for the exact build that wrote it (it contains pointers to static data).
#define SAVE_DIR     "sdmc:/3ds/megabonk3ds"
#define PROFILE_PATH SAVE_DIR "/profile.sav"
#define RUN_PATH     SAVE_DIR "/run.sav"

#define PROFILE_MAGIC 0x3150424Du   // "MBP1"
#define RUN_MAGIC     0x3152424Du   // "MBR1"

SaveProfile g_profile;
static bool s_runSaveExists;

static const char* build_id(void) { return __DATE__ " " __TIME__; }

static void ensure_dir(void)
{
	mkdir("sdmc:/3ds", 0777);
	mkdir(SAVE_DIR, 0777);
}

static void profile_defaults(void)
{
	memset(&g_profile, 0, sizeof(g_profile));
	g_profile.magic = PROFILE_MAGIC;
	g_profile.size = sizeof(SaveProfile);
}

void save_init(void)
{
	profile_defaults();
	FILE* f = fopen(PROFILE_PATH, "rb");
	if (f) {
		SaveProfile p;
		size_t n = fread(&p, 1, sizeof(p), f);
		fclose(f);
		// older/smaller profiles load what they have; new fields start at zero
		if (n >= 8 && p.magic == PROFILE_MAGIC && p.size <= sizeof(SaveProfile) && n >= p.size) {
			memcpy(&g_profile, &p, p.size);
			g_profile.size = sizeof(SaveProfile);
		}
	}
	FILE* r = fopen(RUN_PATH, "rb");
	s_runSaveExists = false;
	if (r) {
		RunSaveHeader h;
		if (fread(&h, 1, sizeof(h), r) == sizeof(h) && h.magic == RUN_MAGIC && h.size == sizeof(Game) &&
		    strncmp(h.build, build_id(), sizeof(h.build)) == 0)
			s_runSaveExists = true;
		fclose(r);
		if (!s_runSaveExists) remove(RUN_PATH);   // from another build: unusable
	}
}

void profile_save(void)
{
	ensure_dir();
	FILE* f = fopen(PROFILE_PATH, "wb");
	if (!f) return;
	fwrite(&g_profile, 1, sizeof(g_profile), f);
	fclose(f);
}

bool run_save_exists(void) { return s_runSaveExists; }

bool run_save(void)
{
	ensure_dir();
	FILE* f = fopen(RUN_PATH, "wb");
	if (!f) return false;
	RunSaveHeader h = { RUN_MAGIC, sizeof(Game), { 0 } };
	strncpy(h.build, build_id(), sizeof(h.build) - 1);
	bool ok = fwrite(&h, 1, sizeof(h), f) == sizeof(h) && fwrite(&g, 1, sizeof(g), f) == sizeof(g);
	fclose(f);
	if (!ok) { remove(RUN_PATH); return false; }
	s_runSaveExists = true;
	return true;
}

void run_save_delete(void)
{
	if (s_runSaveExists) remove(RUN_PATH);
	s_runSaveExists = false;
}

bool run_load(void)
{
	FILE* f = fopen(RUN_PATH, "rb");
	if (!f) return false;
	static Game tmp;   // read into a copy so a bad file can't leave g half-written
	RunSaveHeader h;
	bool ok = fread(&h, 1, sizeof(h), f) == sizeof(h) && h.magic == RUN_MAGIC && h.size == sizeof(Game) &&
	          strncmp(h.build, build_id(), sizeof(h.build)) == 0 && fread(&tmp, 1, sizeof(tmp), f) == sizeof(tmp);
	fclose(f);
	run_save_delete();   // a suspended run can be resumed once
	if (!ok) return false;
	g = tmp;
	game_restore_after_load();
	return true;
}

// ---------------------------------------------------------------- records

void profile_run_start(void)
{
	g_profile.runs++;
	if (g.character < MAX_PROFILE_CHARS) g_profile.charRuns[g.character]++;
	profile_save();
}

void profile_stage_cleared(void)
{
	int first = game_first_stage(g.stage);
	if (first < MAX_PROFILE_STAGES && STAGE->number > g_profile.bestStage[first]) g_profile.bestStage[first] = STAGE->number;
	g_profile.bossesKilled++;
	profile_save();
}

void profile_run_end(bool won)
{
	if (g.runRecorded) return;
	g.runRecorded = true;
	g_profile.totalKills += g.kills;
	g_profile.totalTime += (u32)g.runTime;
	if ((u32)g.kills > g_profile.bestKills) g_profile.bestKills = g.kills;
	if ((u32)g.level > g_profile.bestLevel) g_profile.bestLevel = g.level;
	if (won) {
		g_profile.wins++;
		if (g.character < MAX_PROFILE_CHARS) g_profile.charWins[g.character]++;
		int first = game_first_stage(g.stage);
		if (first < MAX_PROFILE_STAGES &&
		    (g_profile.bestWinTime[first] == 0 || (u32)g.runTime < g_profile.bestWinTime[first]))
			g_profile.bestWinTime[first] = (u32)g.runTime;
	}
	profile_save();
}
