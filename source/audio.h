#pragma once
#include <stdbool.h>

typedef enum {
	SFX_HIT, SFX_ENEMY_DIE, SFX_PLAYER_HURT, SFX_XP, SFX_LEVELUP, SFX_JUMP, SFX_FIREBALL,
	SFX_EXPLOSION, SFX_THROW, SFX_SWORD, SFX_GUN, SFX_LIGHTNING, SFX_HEAL, SFX_CHEST,
	SFX_MENU, SFX_SELECT, SFX_POT, SFX_SHRINE, SFX_SWARM, SFX_BOSS, SFX_PURCHASE, SFX_GOLD, SFX_COUNT
} SfxId;

typedef enum {
	MUSIC_MENU, MUSIC_FOREST, MUSIC_FOREST_BOSS, MUSIC_DESERT, MUSIC_DESERT_BOSS,
	MUSIC_GRAVEYARD, MUSIC_GRAVEYARD_BOSS, MUSIC_COUNT
} MusicId;

// Audio silently does nothing if ndsp can't start (sdmc:/3ds/dspfirm.cdc missing).
void audio_init(void);
void audio_exit(void);
void audio_update(void);   // call once per frame (refills the music stream)
void audio_play_sfx(SfxId id);
void audio_play_music(MusicId id);
void audio_stop_music(void);
bool audio_available(void);
