#include "game.h"
#include "mathutil.h"
#include "audio.h"
#include <string.h>

// Enemy special attacks. Timings, radii, cooldowns, trigger ranges and damage multipliers
// come from EnemyData.specialAttacks (g_enemyAttacks); the shapes of the patterns are
// recreated from the attack names since the attack prefabs are IL2CPP code.

#define ATTACK_DAMAGE_SCALE 0.6f   // special attacks hit harder than contact damage (0.4)
#define LASER_TICK          0.2f
#define LASER_LENGTH        55.0f

static const Color NO_EMIT = { 0, 0, 0, 0 };

static float attack_damage(const Enemy* e, const EnemyAttack* a)
{
	float dmg = g_enemyDefs[e->def].damage * a->damageMult * game_enemy_dmg_mult() * ATTACK_DAMAGE_SCALE;
	return e->elite ? dmg * 1.5f : dmg;
}

static Hazard* hazard_add(vec3 pos, float radius, float delay, float charge, float dmg, int owner)
{
	for (int i = 0; i < MAX_HAZARDS; i++) {
		Hazard* h = &g.hazards[i];
		if (h->active) continue;
		memset(h, 0, sizeof(*h));
		h->active = true;
		h->pos = v3(clampf(pos.x, -MAP_HALF, MAP_HALF), 0, clampf(pos.z, -MAP_HALF, MAP_HALF));
		h->radius = radius;
		h->delay = delay;
		h->charge = charge;
		h->dmg = dmg;
		h->owner = owner;
		return h;
	}
	return NULL;
}

static void laser_add(int owner, float yaw, float spin, bool track, float charge, float dur, float width, float dmg)
{
	for (int i = 0; i < MAX_LASERS; i++) {
		Laser* l = &g.lasers[i];
		if (l->active) continue;
		memset(l, 0, sizeof(*l));
		l->active = true;
		l->owner = owner;
		l->yaw = yaw;
		l->spin = spin;
		l->track = track;
		l->age = -charge;      // negative = warning line only
		l->dur = dur;
		l->width = width;
		l->dmg = dmg;
		return;
	}
}

static void eproj_fire(vec3 from, vec3 dir, float speed, float dmg, int owner)
{
	for (int i = 0; i < MAX_ENEMY_PROJ; i++) {
		EnemyProj* p = &g.enemyProj[i];
		if (p->active) continue;
		p->active = true;
		p->pos = from;
		p->vel = v3_scale(v3_norm(dir), speed);
		p->dmg = dmg;
		p->life = 4.0f;
		p->owner = owner;
		return;
	}
}

static vec3 player_predicted(float t)
{
	return v3(g.pos.x + g.vel.x * t, 0, g.pos.z + g.vel.z * t);
}

static void hazard_line(const Enemy* e, float yaw, float r, float charge, float dmg, int owner)
{
	vec3 d = dir_of_yaw(yaw);
	float dist = sqrtf(dist2_xz(e->pos, g.pos));
	float len = fminf(dist + 12.0f, 50.0f);
	float step = r * 1.1f;
	int n = (int)(len / step);
	if (n > 14) n = 14;
	for (int k = 1; k <= n; k++)
		hazard_add(v3(e->pos.x + d.x * step * k, 0, e->pos.z + d.z * step * k), r * 0.6f, k * 0.07f, charge, dmg, owner);
}

// Spawn the telegraphs / lasers of an attack when its charge starts.
static void attack_begin(int idx, const EnemyAttack* a)
{
	Enemy* e = &g.enemies[idx];
	float dmg = attack_damage(e, a);
	float r = a->radius, c = a->charge;
	float toYaw = yaw_of(v3(g.pos.x - e->pos.x, 0, g.pos.z - e->pos.z));
	switch (a->kind) {
	case ATK_SLAM: {
		Hazard* h = hazard_add(e->pos, r, 0, c, dmg, idx);
		if (h) h->slam = true;
		break;
	}
	case ATK_SPLASH:
		hazard_add(g.pos, r, 0, c, dmg, idx);
		break;
	case ATK_PREDICT:
		hazard_add(player_predicted(c), r, 0, c, dmg, idx);
		break;
	case ATK_MULTI: {
		vec3 p = player_predicted(c);
		hazard_add(p, r * 0.6f, 0, c, dmg, idx);
		for (int k = 0; k < 4; k++) {
			float ang = randf() * 6.2832f, dd = randrange(r * 0.6f, r * 1.6f);
			hazard_add(v3(p.x + sinf(ang) * dd, 0, p.z + cosf(ang) * dd), r * 0.6f, 0.1f * (k + 1), c, dmg, idx);
		}
		break;
	}
	case ATK_CIRCLE: {
		// a ring of pillars closes you in, then the middle erupts: leave through a gap
		float ring = r * 1.6f;
		for (int k = 0; k < 8; k++) {
			float ang = k * 0.785f;
			hazard_add(v3(g.pos.x + sinf(ang) * ring, 0, g.pos.z + cosf(ang) * ring), r * 0.5f, 0, c, dmg, idx);
		}
		hazard_add(g.pos, r * 0.9f, 0.8f, c, dmg, idx);
		break;
	}
	case ATK_LINE:
		hazard_line(e, toYaw, r, c, dmg, idx);
		break;
	case ATK_LINES3:
		for (int k = -1; k <= 1; k++) hazard_line(e, toYaw + k * 0.4f, r, c, dmg, idx);
		break;
	case ATK_CHASE:
		for (int k = 0; k < 8; k++) {
			Hazard* h = hazard_add(g.pos, r * 0.6f, k * 0.45f, c, dmg, idx);
			if (h) h->follow = true;   // snaps to the player when its warning appears
		}
		break;
	case ATK_HITSCAN: {
		Hazard* h = hazard_add(e->pos, fmaxf(r, 0.6f) * 1.8f, 0, c, dmg, idx);
		if (h) {
			vec3 d = dir_of_yaw(toYaw);
			h->line = true;
			h->end = v3(e->pos.x + d.x * 45, 0, e->pos.z + d.z * 45);
		}
		break;
	}
	case ATK_LASER:
		laser_add(idx, toYaw, 0, true, c, a->duration, 1.0f, dmg);
		break;
	case ATK_DISCO: {
		float spin = (rand() & 1) ? 0.45f : -0.45f;
		for (int k = 0; k < 4; k++) laser_add(idx, toYaw + 0.4f + k * 1.5708f, spin, false, c, a->duration, 0.9f, dmg);
		break;
	}
	default:
		break;
	}
}

// Things that happen when the charge finishes.
static void attack_release(int idx, const EnemyAttack* a)
{
	Enemy* e = &g.enemies[idx];
	if (a->kind == ATK_PROJECTILE) {
		vec3 from = v3_add(e->pos, v3(0, 2.0f * e->scale, 0));
		vec3 to = v3_add(player_predicted(0.3f), v3(0, 1.5f, 0));
		eproj_fire(from, v3_sub(to, from), 20.0f, attack_damage(e, a), idx);
	}
}

void attacks_on_spawn(Enemy* e)
{
	const EnemyDef* d = &g_enemyDefs[e->def];
	e->atkState = 0;
	e->atkGap = 2.0f;
	for (int k = 0; k < d->attackCount; k++) {
		const EnemyAttack* a = &g_enemyAttacks[d->attackFirst + k];
		e->atkCd[k] = a->initial + randrange(1.0f, 4.0f);
	}
}

// Returns true while the enemy is busy with a special attack (it stands still).
bool attacks_enemy_update(int idx, float dt)
{
	Enemy* e = &g.enemies[idx];
	const EnemyDef* d = &g_enemyDefs[e->def];
	if (!d->attackCount) return false;
	for (int k = 0; k < d->attackCount; k++)
		if (e->atkCd[k] > 0) e->atkCd[k] -= dt;
	const EnemyAttack* cur = &g_enemyAttacks[d->attackFirst + e->atkCur];

	if (e->atkState == 1) {   // charging: face the player
		e->yaw += angle_diff(e->yaw, yaw_of(v3(g.pos.x - e->pos.x, 0, g.pos.z - e->pos.z))) * clampf(dt * 6, 0, 1);
		e->atkTimer -= dt;
		if (e->atkTimer <= 0) {
			attack_release(idx, cur);
			e->atkState = 2;
			e->atkTimer = fminf(cur->endLag, 2.0f);
		}
		return true;
	}
	if (e->atkState == 2) {   // end lag
		e->atkTimer -= dt;
		if (e->atkTimer <= 0) { e->atkState = 0; e->atkGap = 1.0f; }
		return e->atkState != 0;
	}
	if (e->atkGap > 0) { e->atkGap -= dt; return false; }

	float dist = sqrtf(dist2_xz(e->pos, g.pos));
	int cand[5], n = 0;
	for (int k = 0; k < d->attackCount; k++) {
		const EnemyAttack* a = &g_enemyAttacks[d->attackFirst + k];
		if (e->atkCd[k] <= 0 && dist <= a->trigger) cand[n++] = k;
	}
	if (!n) return false;
	int k = cand[rand() % n];
	const EnemyAttack* a = &g_enemyAttacks[d->attackFirst + k];
	e->atkCur = k;
	e->atkCd[k] = a->cooldown + (a->cooldownMax > a->cooldown ? randf() * (a->cooldownMax - a->cooldown) : 0);
	e->atkState = 1;
	e->atkTimer = a->charge;
	attack_begin(idx, a);
	return true;
}

static bool player_hittable(void) { return g.pos.y - g.groundY < 2.5f; }   // jumping clears ground attacks

static float seg_dist2(vec3 a, vec3 b, vec3 p)
{
	float dx = b.x - a.x, dz = b.z - a.z;
	float len2 = dx * dx + dz * dz;
	float t = len2 > 0 ? clampf(((p.x - a.x) * dx + (p.z - a.z) * dz) / len2, 0, 1) : 0;
	float ex = a.x + dx * t - p.x, ez = a.z + dz * t - p.z;
	return ex * ex + ez * ez;
}

static void hit_player(float dmg, int owner)
{
	if (owner >= 0 && g.enemies[owner].active) g.lastAttacker = owner;
	game_damage_player(dmg);
}

void attacks_update(float dt)
{
	for (int i = 0; i < MAX_HAZARDS; i++) {
		Hazard* h = &g.hazards[i];
		if (!h->active) continue;
		if (h->delay > 0) {
			h->delay -= dt;
			if (h->delay <= 0 && h->follow) h->pos = v3(g.pos.x, 0, g.pos.z);
			continue;
		}
		bool wasCharging = h->age < h->charge;
		h->age += dt;
		if (wasCharging && h->age >= h->charge) {
			// erupt
			bool hit = h->line ? seg_dist2(h->pos, h->end, g.pos) < (h->radius * 0.5f + 0.9f) * (h->radius * 0.5f + 0.9f)
			                   : dist2_xz(h->pos, g.pos) < (h->radius + 0.6f) * (h->radius + 0.6f);
			if (hit && (player_hittable() || h->slam)) hit_player(h->dmg, h->owner);
			if (h->slam) {
				fx_spawn(FX_RING, h->pos, h->radius, 0.35f, COLOR(1, 0.8f, 0.5f));
				fx_spawn(FX_EXPLOSION, v3_add(h->pos, v3(0, 0.5f, 0)), h->radius * 0.5f, 0.3f, COLOR(0.8f, 0.7f, 0.5f));
			}
			audio_play_sfx(SFX_EXPLOSION);
		}
		if (h->age > h->charge + 0.5f) h->active = false;
	}

	for (int i = 0; i < MAX_ENEMY_PROJ; i++) {
		EnemyProj* p = &g.enemyProj[i];
		if (!p->active) continue;
		p->life -= dt;
		p->pos = v3_add(p->pos, v3_scale(p->vel, dt));
		if (p->life <= 0 || p->pos.y < 0) { p->active = false; continue; }
		vec3 c = v3_add(g.pos, v3(0, 1.5f, 0));
		vec3 to = v3_sub(c, p->pos);
		if (v3_len(to) < 1.3f) {
			hit_player(p->dmg, p->owner);
			p->active = false;
		}
	}

	for (int i = 0; i < MAX_LASERS; i++) {
		Laser* l = &g.lasers[i];
		if (!l->active) continue;
		Enemy* o = &g.enemies[l->owner];
		if (!o->active) { l->active = false; continue; }
		l->age += dt;
		if (l->age > l->dur) { l->active = false; continue; }
		if (l->track) {
			float want = yaw_of(v3(g.pos.x - o->pos.x, 0, g.pos.z - o->pos.z));
			float turn = (l->age < 0 ? 3.0f : 0.55f) * dt;   // aims quickly while warning, then lags behind
			l->yaw += clampf(angle_diff(l->yaw, want), -turn, turn);
		} else if (l->age > 0) {
			l->yaw += l->spin * dt;
		}
		if (l->age < 0) continue;
		l->tick -= dt;
		if (l->tick > 0) continue;
		l->tick = LASER_TICK;
		vec3 d = dir_of_yaw(l->yaw);
		vec3 a = o->pos, b = v3(o->pos.x + d.x * LASER_LENGTH, 0, o->pos.z + d.z * LASER_LENGTH);
		float w = l->width + 0.9f;
		if (player_hittable() && seg_dist2(a, b, g.pos) < w * w) hit_player(l->dmg, l->owner);
	}
}

// ---------------------------------------------------------------- drawing

static Color stage_spike_color(void)
{
	switch (g.stage) {
	case 1:  return COLOR(0.85f, 0.72f, 0.48f);   // sandstone
	case 2:  return COLOR(0.88f, 0.86f, 0.78f);   // bone
	default: return COLOR(0.45f, 0.32f, 0.2f);    // roots
	}
}

static void draw_flat_line(vec3 a, vec3 b, float width, float y, Color c, Color emit)
{
	float len = sqrtf(dist2_xz(a, b));
	C3D_Mtx m;
	Mtx_Identity(&m);
	Mtx_Translate(&m, a.x, y, a.z, true);
	Mtx_RotateY(&m, yaw_of(v3(b.x - a.x, 0, b.z - a.z)), true);
	Mtx_RotateX(&m, (float)M_PI / 2, true);   // cylinder axis (+Y) -> forward (+Z)
	Mtx_Scale(&m, width, len, width * 0.15f);
	render_mesh(&g_meshCylinder, 0, &m, c, emit);
}

void attacks_draw(void)
{
	Color spike = stage_spike_color();
	for (int i = 0; i < MAX_HAZARDS; i++) {
		Hazard* h = &g.hazards[i];
		if (!h->active || h->delay > 0) continue;
		if (h->age < h->charge) {
			// warning: red outline plus a fill that grows until it erupts
			float t = h->age / h->charge;
			Color warn = { 1, 0.15f, 0.1f, 0.35f + 0.35f * t };
			Color glow = { 0.5f, 0.05f, 0.02f, 0 };
			if (h->line) {
				draw_flat_line(h->pos, h->end, h->radius * 0.5f * (0.3f + 0.7f * t), 0.12f, warn, glow);
				continue;
			}
			render_mesh_at(&g_meshRing, 0, v3_add(h->pos, v3(0, 0.12f, 0)), 0, h->radius, warn, glow);
			C3D_Mtx m;
			Mtx_Identity(&m);
			Mtx_Translate(&m, h->pos.x, 0.06f, h->pos.z, true);
			Mtx_Scale(&m, h->radius * t, 0.05f, h->radius * t);
			render_mesh(&g_meshSphere, 0, &m, (Color){ 1, 0.2f, 0.1f, 0.3f }, glow);
			continue;
		}
		if (h->slam) continue;   // slams are drawn as fx
		// eruption: spikes shoot up and sink back
		float t = (h->age - h->charge) / 0.5f;
		float hgt = sinf(clampf(t, 0, 1) * (float)M_PI) * fminf(h->radius, 4.0f) * 1.2f;
		if (h->line) {
			vec3 d = v3_sub(h->end, h->pos);
			for (int k = 0; k < 12; k++) {
				vec3 p = v3(h->pos.x + d.x * (k + 0.5f) / 12, 0, h->pos.z + d.z * (k + 0.5f) / 12);
				C3D_Mtx m;
				Mtx_Identity(&m);
				Mtx_Translate(&m, p.x, 0, p.z, true);
				Mtx_RotateX(&m, -(float)M_PI / 2, true);
				Mtx_Scale(&m, 0.4f, 0.4f, hgt);
				render_mesh(&g_meshCone, 0, &m, spike, NO_EMIT);
			}
			continue;
		}
		int n = h->radius > 3 ? 7 : 4;
		for (int k = 0; k <= n; k++) {
			float ang = k * 6.2832f / n + i, dd = k == n ? 0 : h->radius * 0.6f;
			float s = (k == n ? 0.5f : 0.35f) * fmaxf(h->radius * 0.5f, 1.0f);
			C3D_Mtx m;
			Mtx_Identity(&m);
			Mtx_Translate(&m, h->pos.x + sinf(ang) * dd, 0, h->pos.z + cosf(ang) * dd, true);
			Mtx_RotateX(&m, -(float)M_PI / 2, true);   // cone tip (+Z) -> up
			Mtx_Scale(&m, s, s, hgt * (k == n ? 1.3f : 1.0f));
			render_mesh(&g_meshCone, 0, &m, spike, NO_EMIT);
		}
	}

	for (int i = 0; i < MAX_ENEMY_PROJ; i++) {
		EnemyProj* p = &g.enemyProj[i];
		if (!p->active) continue;
		render_mesh_at(&g_meshSphere, 0, p->pos, 0, 0.45f, COLOR(0.4f, 0.9f, 0.3f), (Color){ 0.2f, 0.5f, 0.1f, 0 });
	}

	for (int i = 0; i < MAX_LASERS; i++) {
		Laser* l = &g.lasers[i];
		if (!l->active) continue;
		Enemy* o = &g.enemies[l->owner];
		vec3 d = dir_of_yaw(l->yaw);
		vec3 a = o->pos, b = v3(o->pos.x + d.x * LASER_LENGTH, 0, o->pos.z + d.z * LASER_LENGTH);
		if (l->age < 0) {
			draw_flat_line(a, b, 0.25f, 0.15f, (Color){ 1, 0.2f, 0.1f, 0.5f }, (Color){ 0.5f, 0.05f, 0, 0 });
			continue;
		}
		float pulse = 0.85f + 0.15f * sinf(g.realTime * 30);
		C3D_Mtx m;
		Mtx_Identity(&m);
		Mtx_Translate(&m, a.x, 1.6f, a.z, true);
		Mtx_RotateY(&m, l->yaw, true);
		Mtx_RotateX(&m, (float)M_PI / 2, true);
		Mtx_Scale(&m, l->width * 0.6f * pulse, LASER_LENGTH, l->width * 0.6f * pulse);
		render_mesh(&g_meshCylinder, 0, &m, (Color){ 1, 0.25f, 0.15f, 0.8f }, (Color){ 1, 0.3f, 0.1f, 0 });
	}
}
