#include "game.h"
#include "mathutil.h"
#include "audio.h"
#include <stdio.h>

// Names are the game's (PassiveAbilities table). Descriptions follow the game's text;
// per-level numbers are from the megabonk.org character guides (the passive code is IL2CPP).
const char* const g_passiveNames[PASSIVE_COUNT] = {
	[PASSIVE_NONE] = "", [PASSIVE_RNG] = "RNG Blessing", [PASSIVE_SPEEDDEMON] = "Speed Demon",
	[PASSIVE_REINFORCED] = "Reinforced", [PASSIVE_CRIT] = "Crit Happens", [PASSIVE_FLEX] = "Flex",
	[PASSIVE_WARRIOR] = "Warrior", [PASSIVE_LOCKIN] = "Lock in", [PASSIVE_FLOAT] = "Float",
	[PASSIVE_BULLSEYE] = "Bullseye", [PASSIVE_FLOWSTATE] = "Flowstate", [PASSIVE_MONKEGRIP] = "Monke Grip",
	[PASSIVE_ENDURING] = "Enduring", [PASSIVE_STONKS] = "Stonks", [PASSIVE_ZAP] = "Zap",
	[PASSIVE_PLAGUE] = "Plague", [PASSIVE_QUANTUM] = "Quantum XP Theory", [PASSIVE_SHADOWSTEP] = "Shadowstep",
	[PASSIVE_VAMPIRE] = "Vampire", [PASSIVE_GAMBA] = "Gamba", [PASSIVE_CURSE] = "Curse",
	[PASSIVE_HOARDER] = "Hoarder",
};

const char* const g_passiveDescs[PASSIVE_COUNT] = {
	[PASSIVE_NONE] = "",
	[PASSIVE_RNG] = "Gain 1.5% Luck per level.",
	[PASSIVE_SPEEDDEMON] = "Speed builds up over time, but is halved when you take damage. Also gain damage based on speed.",
	[PASSIVE_REINFORCED] = "Gain 1% Armor per level.",
	[PASSIVE_CRIT] = "Gain 1% Crit Chance per level.",
	[PASSIVE_FLEX] = "Flex to stop damage and stun nearby enemies. Cooldown -0.4s per level. Gain 2.5% Damage when flexing.",
	[PASSIVE_WARRIOR] = "Gain 1.5% Damage per level.",
	[PASSIVE_LOCKIN] = "Deal more damage on lower HP. Gain 2 Thorns per level.",
	[PASSIVE_FLOAT] = "Hold Jump to float in the air. Also gain 1% Airborne Damage per level.",
	[PASSIVE_BULLSEYE] = "Random enemies are marked and explode on death. Also gain 1% Crit Damage per level.",
	[PASSIVE_FLOWSTATE] = "Gain 1% Attack Speed per level.",
	[PASSIVE_MONKEGRIP] = "Jump higher. Also gain 2 Max HP per level.",
	[PASSIVE_ENDURING] = "Gain 0.75% Size per level. Also gain 1.5% Damage per currently frozen enemy.",
	[PASSIVE_STONKS] = "Gain 1% Gold per level. Also increase Damage slightly by total Gold in inventory.",
	[PASSIVE_ZAP] = "Charge an electric attack by moving around. Gain 2% Pickup Range per level.",
	[PASSIVE_PLAGUE] = "Poisoned enemies explode on death, poisoning nearby enemies. Also gain 1% Poison Damage per level.",
	[PASSIVE_QUANTUM] = "Gain 5% XP, plus 1.5% XP per level.",
	[PASSIVE_SHADOWSTEP] = "Execute an enemy upon evading them. Also gain 0.5% Evasion per level.",
	[PASSIVE_VAMPIRE] = "5% Lifesteal, plus 1% Lifesteal per level.",
	[PASSIVE_GAMBA] = "Roll the dice and upgrade a random stat when you level up!",
	[PASSIVE_CURSE] = "Gain 1% Curse per level (tougher enemies, more XP and gold).",
	[PASSIVE_HOARDER] = "Free chest every 60s, increasing to 120s over levels. He can also float (a little).",
};

enum { PT_FLEX, PT_BULLSEYE, PT_HOARDER, PT_VAMPIRE };

PassiveId passive_current(void) { return g_characters[g.character].passive; }

void passive_init(void)
{
	switch (passive_current()) {
	case PASSIVE_VAMPIRE: g.stats.lifesteal += 0.05f; break;
	case PASSIVE_QUANTUM: g.stats.xpMult += 0.05f; break;
	default: break;
	}
}

float passive_jump_mult(void) { return passive_current() == PASSIVE_MONKEGRIP ? 1.3f : 1.0f; }

void passive_on_level_up(void)
{
	PlayerStats* s = &g.stats;
	switch (passive_current()) {
	case PASSIVE_RNG:        s->luck += 0.015f; break;
	case PASSIVE_SPEEDDEMON: s->damageMult += 0.0075f; break;
	case PASSIVE_REINFORCED: s->armor += 0.01f; break;
	case PASSIVE_CRIT:       s->critChance += 0.01f; break;
	case PASSIVE_WARRIOR:    s->damageMult += 0.015f; break;
	case PASSIVE_LOCKIN:     s->thorns += 2; break;
	case PASSIVE_BULLSEYE:   s->critMult += 0.01f; break;
	case PASSIVE_FLOWSTATE:  s->attackSpeed += 0.01f; break;
	case PASSIVE_MONKEGRIP:  s->maxHp += 2; g.hp += 2; break;
	case PASSIVE_ENDURING:   s->sizeMult += 0.0075f; break;
	case PASSIVE_STONKS:     s->goldMult += 0.01f; break;
	case PASSIVE_ZAP:        s->pickupRange += 9.0f * 0.02f; break;
	case PASSIVE_QUANTUM:    s->xpMult += 0.015f; break;
	case PASSIVE_SHADOWSTEP: s->evasion += 0.005f; break;
	case PASSIVE_VAMPIRE:    s->lifesteal += 0.01f; break;
	case PASSIVE_CURSE:      g.difficulty += 0.01f; s->xpMult += 0.01f; s->goldMult += 0.01f; break;
	case PASSIVE_GAMBA: {
		// a free random stat roll with a random rarity
		int t = rand() % TOME_COUNT;
		if (t == TOME_QUANTITY || t == TOME_CURSED) t = TOME_DAMAGE;
		LevelChoice c = { .kind = CHOICE_STAT, .index = t, .rarity = rand() % RARITY_COUNT };
		game_apply_choice(&c);
		char buf[64];
		snprintf(buf, sizeof(buf), "Gamba! %s", g_tomeNames[t]);
		game_toast(buf);
		break;
	}
	default: break;
	}
}

static int count_frozen(void)
{
	int n = 0;
	for (int i = 0; i < MAX_ENEMIES; i++)
		if (g.enemies[i].active && g.enemies[i].freezeTime > 0) n++;
	return n;
}

float passive_damage_mult(int idx)
{
	float m = 1.0f;
	switch (passive_current()) {
	case PASSIVE_SPEEDDEMON: m += 0.5f * (g.stats.moveSpeed - 1.0f + 0.5f * g.speedDemon); break;   // 0.5% per 1% speed
	case PASSIVE_LOCKIN:     m += 0.6f * (1.0f - g.hp / g.stats.maxHp); break;
	case PASSIVE_FLOAT:      if (!g.grounded) m += 0.01f * g.level; break;
	case PASSIVE_ENDURING:   m += 0.015f * count_frozen(); break;
	case PASSIVE_STONKS:     m += fminf(g.gold * 0.001f, 0.5f); break;
	case PASSIVE_FLEX:       m += g.flexBuff; break;   // permanent stacks
	default: break;
	}
	return m;
}

void passive_on_damage_dealt(int idx, float dmg)
{
	(void)idx;
	(void)dmg;
}

bool passive_on_player_hit(float* dmg)
{
	switch (passive_current()) {
	case PASSIVE_FLEX:
		if (g.passiveTimer[PT_FLEX] <= 0) {
			g.passiveTimer[PT_FLEX] = fmaxf(5.0f, 15.0f - 0.4f * g.level);
			g.flexBuff += 0.025f;   // permanent +2.5% damage per flex
			g.iframes = 0.6f;
			for (int i = 0; i < MAX_ENEMIES; i++)   // stun everything close
				if (g.enemies[i].active && dist2_xz(g.enemies[i].pos, g.pos) < 6 * 6)
					g.enemies[i].freezeTime = fmaxf(g.enemies[i].freezeTime, 1.0f);
			fx_spawn(FX_RING, g.pos, 4.0f, 0.4f, COLOR(1, 0.85f, 0.4f));
			game_toast("FLEX!");
			return true;
		}
		break;
	case PASSIVE_SPEEDDEMON:
		g.speedDemon *= 0.5f;
		break;
	default:
		break;
	}
	(void)dmg;
	return false;
}

void passive_on_evade(void)
{
	if (passive_current() != PASSIVE_SHADOWSTEP) return;
	int a = g.lastAttacker;
	if (a < 0 || !g.enemies[a].active) return;
	if (g_enemyDefs[g.enemies[a].def].flags & (ENEMY_BOSS | ENEMY_MINIBOSS)) return;
	fx_spawn(FX_SLASH, g.pos, 3.0f, 0.2f, COLOR(0.4f, 0.2f, 0.6f));
	game_damage_enemy_ex(a, g.enemies[a].hp + 1, v3(0, 0, 0), 0, DMG_NO_PROC | DMG_NO_CRIT);
}

void passive_on_kill(int idx)
{
	Enemy* e = &g.enemies[idx];
	if (passive_current() == PASSIVE_PLAGUE && e->poisonTime > 0) {
		float dps = fmaxf(e->poisonDps, 5.0f * g.stats.damageMult) * (1.0f + 0.01f * g.level);
		float r = fminf(4.0f + 0.2f * g.level, 12.0f);   // explosion radius grows 4 -> 12
		for (int i = 0; i < MAX_ENEMIES; i++) {
			Enemy* o = &g.enemies[i];
			if (o->active && dist2_xz(o->pos, e->pos) < r * r) {
				o->poisonTime = 3.0f;
				o->poisonDps = fmaxf(o->poisonDps, dps);
			}
		}
		fx_spawn(FX_POISON, e->pos, r, 0.5f, COLOR(0.4f, 0.9f, 0.3f));
	}
	if (passive_current() == PASSIVE_BULLSEYE && e->marked) {
		float dmg = (10 + 2 * g.level) * g.stats.damageMult;
		e->marked = false;
		game_area_damage(e->pos, 4.0f, dmg, 1.0f, DMG_NO_PROC);
		fx_spawn(FX_EXPLOSION, v3_add(e->pos, v3(0, 1, 0)), 4.0f, 0.3f, COLOR(1, 0.3f, 0.2f));
	}
}

float passive_fall_limit(void)
{
	switch (passive_current()) {
	case PASSIVE_FLOAT:   return 2.5f;   // glide
	case PASSIVE_HOARDER: return 8.0f;   // "can also float (a little)"
	default:              return 0;
	}
}

void passive_update(float dt)
{
	for (int i = 0; i < 4; i++)
		if (i != PT_VAMPIRE && g.passiveTimer[i] > 0) g.passiveTimer[i] -= dt;

	switch (passive_current()) {
	case PASSIVE_SPEEDDEMON:
		// speed builds while you keep moving
		g.speedDemon = fminf(1.0f, g.speedDemon + dt / 20.0f);
		break;
	case PASSIVE_BULLSEYE:
		if (g.passiveTimer[PT_BULLSEYE] <= 0) {
			g.passiveTimer[PT_BULLSEYE] = 3.0f;
			int cand[MAX_ENEMIES], n = 0;
			for (int i = 0; i < MAX_ENEMIES; i++)
				if (g.enemies[i].active && !g.enemies[i].marked && dist2_xz(g.enemies[i].pos, g.pos) < 35 * 35) cand[n++] = i;
			if (n) g.enemies[cand[rand() % n]].marked = true;
		}
		break;
	case PASSIVE_ZAP: {
		float spd = sqrtf(g.vel.x * g.vel.x + g.vel.z * g.vel.z);
		g.zapCharge += spd * dt;
		if (g.zapCharge >= 40.0f) {
			g.zapCharge = 0;
			for (int k = 0; k < 3; k++) {
				int t = -1, tries = 0;
				while (tries++ < 8) {
					int i = rand() % MAX_ENEMIES;
					if (g.enemies[i].active && dist2_xz(g.enemies[i].pos, g.pos) < 25 * 25) { t = i; break; }
				}
				if (t < 0) continue;
				game_area_damage(g.enemies[t].pos, 2.5f, (30 + 3 * g.level) * g.stats.damageMult, 0.5f, DMG_NO_PROC);
				fx_spawn(FX_LIGHTNING, g.enemies[t].pos, 2.5f, 0.25f, COLOR(0.6f, 0.9f, 1));
			}
			audio_play_sfx(SFX_LIGHTNING);
		}
		break;
	}
	case PASSIVE_HOARDER:
		if (g.passiveTimer[PT_HOARDER] <= 0) {   // starts at 60s (game_new_run)
			game_spawn_pickup(PICKUP_CHEST, v3_add(g.pos, dir_of_yaw(g.yaw + 1.0f)), 1);
			g.passiveTimer[PT_HOARDER] = fminf(120.0f, 60.0f + 3.0f * (g.level - 1));
		}
		break;
	default:
		break;
	}
}
