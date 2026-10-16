#include "game.h"
#include "mathutil.h"
#include "audio.h"
#include <stdio.h>

// Names are the game's (PassiveAbilities table). Descriptions follow the game's
// text where it has one; the numbers are hand-tuned since the passive code is IL2CPP.
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
	[PASSIVE_RNG] = "Gain 2% Luck per level.",
	[PASSIVE_SPEEDDEMON] = "Speed builds up over time, but is halved when you take damage. Also gain damage based on speed.",
	[PASSIVE_REINFORCED] = "Gain 3% Armor per level.",
	[PASSIVE_CRIT] = "Gain 1% Crit Chance per level.",
	[PASSIVE_FLEX] = "Flex to stop damage every 15s (-0.4s per level). Gain 30% Damage when flexing.",
	[PASSIVE_WARRIOR] = "Gain 2% Damage per level.",
	[PASSIVE_LOCKIN] = "Deal more damage on lower HP. Gain 2% Armor per level.",
	[PASSIVE_FLOAT] = "Hold Jump to float in the air. Also gain 2% Airborne Damage per level.",
	[PASSIVE_BULLSEYE] = "Random enemies are marked and take double damage. Also gain 1% Crit Chance per level.",
	[PASSIVE_FLOWSTATE] = "Gain 2% Attack Speed per level.",
	[PASSIVE_MONKEGRIP] = "Gain 4 Max HP per level.",
	[PASSIVE_ENDURING] = "Gain 2% Armor per level. Also increase Damage per currently frozen enemy.",
	[PASSIVE_STONKS] = "Gain 3% Gold per level. Also increase Damage slightly by total Gold in inventory.",
	[PASSIVE_ZAP] = "Charge an electric attack by moving around. Gain 1% Movement Speed per level.",
	[PASSIVE_PLAGUE] = "Poisoned enemies explode on death, poisoning nearby enemies. Also gain 2% Damage per level.",
	[PASSIVE_QUANTUM] = "Gain 3% XP per level.",
	[PASSIVE_SHADOWSTEP] = "Execute an enemy upon evading them. Also gain 1% Evasion per level.",
	[PASSIVE_VAMPIRE] = "Heal 0.4% of damage dealt per level.",
	[PASSIVE_GAMBA] = "Roll the dice and upgrade a random stat when you level up!",
	[PASSIVE_CURSE] = "Gain 3% Damage and 3% Curse (more enemies) per level.",
	[PASSIVE_HOARDER] = "Free chest every 90s, decreasing to 30s over levels. He can also float (a little).",
};

enum { PT_FLEX, PT_BULLSEYE, PT_HOARDER, PT_VAMPIRE };

PassiveId passive_current(void) { return g_characters[g.character].passive; }

void passive_on_level_up(void)
{
	PlayerStats* s = &g.stats;
	switch (passive_current()) {
	case PASSIVE_RNG:        s->luck += 0.02f; break;
	case PASSIVE_REINFORCED: s->armor += 0.03f; break;
	case PASSIVE_CRIT:       s->critChance += 0.01f; break;
	case PASSIVE_WARRIOR:    s->damageMult += 0.02f; break;
	case PASSIVE_LOCKIN:     s->armor += 0.02f; break;
	case PASSIVE_BULLSEYE:   s->critChance += 0.01f; break;
	case PASSIVE_FLOWSTATE:  s->attackSpeed += 0.02f; break;
	case PASSIVE_MONKEGRIP:  s->maxHp += 4; g.hp += 4; break;
	case PASSIVE_ENDURING:   s->armor += 0.02f; break;
	case PASSIVE_STONKS:     s->goldMult += 0.03f; break;
	case PASSIVE_ZAP:        s->moveSpeed += 0.01f; break;
	case PASSIVE_PLAGUE:     s->damageMult += 0.02f; break;
	case PASSIVE_QUANTUM:    s->xpMult += 0.03f; break;
	case PASSIVE_SHADOWSTEP: s->evasion += 0.01f; break;
	case PASSIVE_CURSE:      s->damageMult += 0.03f; g.difficulty += 0.03f; break;
	case PASSIVE_GAMBA: {
		// a free random stat roll with a random rarity
		int t = rand() % TOME_COUNT;
		if (t == TOME_QUANTITY) t = TOME_DAMAGE;
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
	case PASSIVE_SPEEDDEMON: m += 0.5f * g.speedDemon; break;
	case PASSIVE_LOCKIN:     m += 0.6f * (1.0f - g.hp / g.stats.maxHp); break;
	case PASSIVE_FLOAT:      if (!g.grounded) m += 0.02f * g.level; break;
	case PASSIVE_BULLSEYE:   if (g.enemies[idx].marked) m += 1.0f; break;
	case PASSIVE_ENDURING:   m += fminf(0.02f * count_frozen(), 0.4f); break;
	case PASSIVE_STONKS:     m += fminf(g.gold * 0.001f, 0.5f); break;
	case PASSIVE_FLEX:       if (g.flexBuff > 0) m += 0.3f; break;
	default: break;
	}
	return m;
}

void passive_on_damage_dealt(int idx, float dmg)
{
	(void)idx;
	if (passive_current() == PASSIVE_VAMPIRE && g.passiveTimer[PT_VAMPIRE] < 8.0f) {
		float h = dmg * 0.004f * g.level;
		g.passiveTimer[PT_VAMPIRE] += h;    // budget: ~8 HP/s
		game_heal(h);
	}
}

bool passive_on_player_hit(float* dmg)
{
	switch (passive_current()) {
	case PASSIVE_FLEX:
		if (g.passiveTimer[PT_FLEX] <= 0) {
			g.passiveTimer[PT_FLEX] = fmaxf(5.0f, 15.0f - 0.4f * g.level);
			g.flexBuff = 3.0f;
			g.iframes = 0.6f;
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
		float dps = fmaxf(e->poisonDps, 5.0f * g.stats.damageMult);
		for (int i = 0; i < MAX_ENEMIES; i++) {
			Enemy* o = &g.enemies[i];
			if (o->active && dist2_xz(o->pos, e->pos) < 5 * 5) {
				o->poisonTime = 3.0f;
				o->poisonDps = fmaxf(o->poisonDps, dps);
			}
		}
		fx_spawn(FX_POISON, e->pos, 5.0f, 0.5f, COLOR(0.4f, 0.9f, 0.3f));
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
	g.passiveTimer[PT_VAMPIRE] = fmaxf(0, g.passiveTimer[PT_VAMPIRE] - 8.0f * dt);
	if (g.flexBuff > 0) g.flexBuff -= dt;

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
		if (g.passiveTimer[PT_HOARDER] <= 0) {   // starts at 90s (game_new_run)
			game_spawn_pickup(PICKUP_CHEST, v3_add(g.pos, dir_of_yaw(g.yaw + 1.0f)), 1);
			g.passiveTimer[PT_HOARDER] = fmaxf(30.0f, 90.0f - 3.0f * (g.level - 1));
		}
		break;
	default:
		break;
	}
}
