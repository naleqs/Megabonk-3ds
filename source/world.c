#include "game.h"
#include "mathutil.h"
#include <string.h>

#define GROUND_TILES 26
#define GROUND_TILE  8.0f
#define GROUND_UV    0.125f   // texture repeats every 8 units
#define CLIFF_HEIGHT 30.0f
#define DRAW_DIST    105.0f

static Mesh s_ground, s_cliffs;
static bool s_built;

#define GN (GROUND_TILES + 1)
#define CLIFF_SEGS 14
static float s_pn[GN * GN * 6], s_uv[GN * GN * 2];
static u16 s_idx[GROUND_TILES * GROUND_TILES * 6];

static void build_ground(void)
{
	int n = GN;
	float half = GROUND_TILES * GROUND_TILE * 0.5f;
	for (int z = 0; z < n; z++)
		for (int x = 0; x < n; x++) {
			int i = z * n + x;
			float px = x * GROUND_TILE - half, pz = z * GROUND_TILE - half;
			float* v = &s_pn[i * 6];
			v[0] = px; v[1] = 0; v[2] = pz; v[3] = 0; v[4] = 1; v[5] = 0;
			s_uv[i * 2] = px * GROUND_UV;
			s_uv[i * 2 + 1] = pz * GROUND_UV;
		}
	u16* idx = s_idx;
	for (int z = 0; z < GROUND_TILES; z++)
		for (int x = 0; x < GROUND_TILES; x++) {
			int a = z * n + x;
			*idx++ = a; *idx++ = a + n; *idx++ = a + 1;
			*idx++ = a + 1; *idx++ = a + n; *idx++ = a + n + 1;
		}
	mesh_from_floats(&s_ground, s_pn, s_uv, n * n, s_idx, GROUND_TILES * GROUND_TILES * 6, texture_get("ground"));
}

static void build_cliffs(void)
{
	// four inward-facing walls, split into segments so UVs tile nicely
	const int segs = CLIFF_SEGS;
	float h = MAP_HALF;
	int vi = 0, ii = 0;
	for (int side = 0; side < 4; side++) {
		for (int s = 0; s < segs; s++) {
			float t0 = -h + 2 * h * s / segs, t1 = -h + 2 * h * (s + 1) / segs;
			float x0, z0, x1, z1, nx, nz;
			switch (side) {
			case 0: x0 = t0; z0 = h; x1 = t1; z1 = h; nx = 0; nz = -1; break;
			case 1: x0 = t1; z0 = -h; x1 = t0; z1 = -h; nx = 0; nz = 1; break;
			case 2: x0 = h; z0 = t1; x1 = h; z1 = t0; nx = -1; nz = 0; break;
			default: x0 = -h; z0 = t0; x1 = -h; z1 = t1; nx = 1; nz = 0; break;
			}
			float seglen = 2 * h / segs;
			float corners[4][3] = { { x0, -1, z0 }, { x1, -1, z1 }, { x1, CLIFF_HEIGHT, z1 }, { x0, CLIFF_HEIGHT, z0 } };
			float uvs[4][2] = { { 0, 0 }, { seglen / 10.0f, 0 }, { seglen / 10.0f, CLIFF_HEIGHT / 10.0f }, { 0, CLIFF_HEIGHT / 10.0f } };
			for (int c = 0; c < 4; c++) {
				float* v = &s_pn[(vi + c) * 6];
				v[0] = corners[c][0]; v[1] = corners[c][1]; v[2] = corners[c][2];
				v[3] = nx; v[4] = 0.3f; v[5] = nz;
				s_uv[(vi + c) * 2] = uvs[c][0];
				s_uv[(vi + c) * 2 + 1] = uvs[c][1];
			}
			s_idx[ii++] = vi; s_idx[ii++] = vi + 1; s_idx[ii++] = vi + 2;
			s_idx[ii++] = vi; s_idx[ii++] = vi + 2; s_idx[ii++] = vi + 3;
			vi += 4;
		}
	}
	mesh_from_floats(&s_cliffs, s_pn, s_uv, vi, s_idx, ii, texture_get("cliffs"));
}

static float prop_collision_radius(int def, float scale)
{
	const char* m = g_propDefs[def].mesh;
	float r = g_propDefs[def].radius;
	if (strstr(m, "tree")) return 0.9f * scale;
	if (strstr(m, "boulder")) return r * 0.75f * scale;
	if (strstr(m, "rock_medium")) return r * 0.6f * scale;
	if (strstr(m, "house")) return r * 0.6f * scale;
	if (strstr(m, "landmark") || strstr(m, "ladnmark")) return r * 0.5f * scale;
	if (strstr(m, "cactus") || strstr(m, "lightpost") || strstr(m, "brazier") || strstr(m, "barrel"))
		return 0.7f * scale;
	return 0;
}

void world_restore(void)
{
	if (!s_built) {
		build_ground();
		build_cliffs();
		s_built = true;
	}
	s_ground.parts[0].tex = texture_get(STAGE->groundTex);
	s_cliffs.parts[0].tex = texture_get(STAGE->cliffTex);
}

void world_generate(void)
{
	world_restore();
	g.numProps = 0;
	for (int d = STAGE->propFirst; d < STAGE->propFirst + STAGE->propCount; d++) {
		int amount = (int)(g_propDefs[d].amount * 0.6f);
		for (int i = 0; i < amount && g.numProps < MAX_PROPS; i++) {
			Prop* p = &g.props[g.numProps];
			p->def = d;
			p->pos = v3(randrange(-MAP_HALF + 4, MAP_HALF - 4), 0, randrange(-MAP_HALF + 4, MAP_HALF - 4));
			if (fabsf(p->pos.x) < 10 && fabsf(p->pos.z) < 10) continue; // keep spawn clear
			p->yaw = randf() * 6.2832f;
			p->scale = randrange(g_propDefs[d].scaleMin, g_propDefs[d].scaleMax);
			p->colRadius = prop_collision_radius(d, p->scale);
			const Mesh* mesh = g.propMeshes[d];
			p->top = mesh ? mesh->maxy * p->scale : 0;
			p->topRadius = p->colRadius;
			if (mesh && strstr(g_propDefs[d].mesh, "tree")) {   // stand on the canopy, not the trunk
				p->top *= 0.92f;
				p->topRadius = fmaxf(p->colRadius, mesh->radius * p->scale * 0.45f);
			}
			g.numProps++;
		}
	}
}

void world_clear_area(vec3 pos, float radius)
{
	for (int i = 0; i < g.numProps; i++) {
		float r = radius + g.props[i].colRadius;
		if (dist2_xz(g.props[i].pos, pos) < r * r)
			g.props[i--] = g.props[--g.numProps];
	}
}

// Solids: props first (index < numProps), then interactables.
bool world_solid(int i, vec3* pos, float* radius, float* top)
{
	if (i < g.numProps) {
		const Prop* p = &g.props[i];
		if (p->colRadius <= 0) return false;
		*pos = p->pos;
		*radius = p->colRadius;
		*top = p->top;
		return true;
	}
	i -= g.numProps;
	if (!interact_solid(i, radius, top)) return false;
	*pos = g.interact[i].pos;
	return true;
}

int world_collide(vec3* pos, float radius)
{
	int hit = -1;
	for (int i = 0; i < g.numProps + g.numInteract; i++) {
		vec3 c;
		float r, top;
		if (!world_solid(i, &c, &r, &top) || pos->y >= top - 0.3f) continue;   // above it: no wall
		float rr = r + radius;
		float dx = pos->x - c.x, dz = pos->z - c.z;
		float d2 = dx * dx + dz * dz;
		if (d2 >= rr * rr) continue;
		float d = sqrtf(d2);
		if (d < 1e-3f) { dx = 1; dz = 0; d = 1; }
		pos->x = c.x + dx / d * rr;
		pos->z = c.z + dz / d * rr;
		hit = i;
	}
	return hit;
}

float world_ground(vec3 pos, float radius)
{
	float best = 0;
	for (int i = 0; i < g.numProps + g.numInteract; i++) {
		vec3 c;
		float r, top;
		if (!world_solid(i, &c, &r, &top) || top <= best || pos.y < top - 0.8f) continue;
		if (i < g.numProps) r = g.props[i].topRadius;
		float rr = r + radius * 0.5f;
		if (dist2_xz(pos, c) < rr * rr) best = top;
	}
	return best;
}

// true if the prop blocks the line from the camera to the player
static bool occludes(const Prop* p, const Mesh* mesh, vec3 camPos)
{
	float r = mesh->radius * p->scale * 0.8f;
	return mesh->maxy * p->scale > 1.5f && seg_dist2_xz(camPos, g.pos, p->pos) < r * r;
}

void world_draw(vec3 camPos, vec3 camFwd)
{
	Color white = COLOR(1, 1, 1), none = { 0, 0, 0, 0 };

	// ground follows the player, snapped to the texture period so it doesn't swim
	float snap = GROUND_TILE;
	C3D_Mtx m;
	Mtx_Identity(&m);
	float gx = floorf(g.pos.x / snap) * snap, gz = floorf(g.pos.z / snap) * snap;
	Mtx_Translate(&m, gx, 0, gz, true);
	render_mesh(&s_ground, 0, &m, white, none);

	Mtx_Identity(&m);
	render_mesh(&s_cliffs, 0, &m, COLOR(0.85f, 0.85f, 0.85f), none);

	float maxd2 = DRAW_DIST * DRAW_DIST;
	int faded[32], nfaded = 0;
	bool fadeOn = g.state != STATE_TITLE && g.state != STATE_CHARSELECT;
	for (int i = 0; i < g.numProps; i++) {
		Prop* p = &g.props[i];
		float dx = p->pos.x - camPos.x, dz = p->pos.z - camPos.z;
		float d2 = dx * dx + dz * dz;
		if (d2 > maxd2) continue;
		Mesh* mesh = g.propMeshes[p->def];
		float r = mesh->radius * p->scale + 2;
		if (dx * camFwd.x + dz * camFwd.z < -r) continue; // behind camera
		if (fadeOn && nfaded < 32 && occludes(p, mesh, camPos)) { faded[nfaded++] = i; continue; }
		render_mesh_at(mesh, 0, p->pos, p->yaw, p->scale, white, none);
	}
	// see-through pass for occluders (drawn without depth writes so the player shows)
	render_set_depth_write(false);
	for (int k = 0; k < nfaded; k++) {
		Prop* p = &g.props[faded[k]];
		render_mesh_at(g.propMeshes[p->def], 0, p->pos, p->yaw, p->scale, (Color){ 1, 1, 1, 0.3f }, none);
	}
	render_set_depth_write(true);
}
