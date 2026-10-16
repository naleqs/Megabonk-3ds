#include "game.h"
#include "mathutil.h"
#include "terrain.h"
#include <string.h>

// Ground height field. Megabonk builds its maps from a tile grid (MapParameters: 40-unit tiles,
// a height level per tile, cliffs and ramps between levels; Desert uses a noise heightfield).
// That generator is compiled code, so this is the port's own version of the same idea:
//   forest / graveyard: 7 x 7 tiles at levels 0..2, cliffs between levels, ramps keep every
//                       plateau reachable from the (flat) spawn tile
//   desert:             rolling dunes from value noise (TerrainDesert.heightMultiplier = 11)
#define TN        49                         // cells per side (7 per tile)
#define TV        (TN + 1)                   // vertices per side
#define TILES     7
#define CELLS_PER_TILE (TN / TILES)
#define STEP      10.0f                      // height of one plateau level
#define RAMP_LEN  4                          // cells a ramp runs into the lower tile
#define RAMP_HALF 1.6f                       // ramp half-width in cells
#define UV_SCALE  0.125f

static float s_h[TV][TV];
static Mesh s_flat, s_steep;
static bool s_meshes;
static float s_cell;                         // world units per cell
static u32 s_rng;

static u32 rnd(void) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static float rndf(void) { return (rnd() & 0xFFFFFF) / (float)0x1000000; }

static float vx(int i) { return -MAP_HALF + i * s_cell; }

// ---------------------------------------------------------------- generation

static void gen_plateaus(float volatility)
{
	int lv[TILES][TILES];
	for (int z = 0; z < TILES; z++)
		for (int x = 0; x < TILES; x++)
			lv[z][x] = (int)(rndf() * 3.0f * volatility + 0.25f);
	// smooth so plateaus form groups instead of noise
	int sm[TILES][TILES];
	for (int z = 0; z < TILES; z++)
		for (int x = 0; x < TILES; x++) {
			int sum = 0, n = 0;
			for (int dz = -1; dz <= 1; dz++)
				for (int dx = -1; dx <= 1; dx++) {
					int xx = x + dx, zz = z + dz;
					if (xx < 0 || zz < 0 || xx >= TILES || zz >= TILES) continue;
					sum += lv[zz][xx] * (dx || dz ? 1 : 2);
					n += dx || dz ? 1 : 2;
				}
			sm[z][x] = (int)(sum / (float)n + rndf() * 0.9f);
			if (sm[z][x] > 2) sm[z][x] = 2;
		}
	sm[TILES / 2][TILES / 2] = 0;   // spawn tile is flat ground

	// ramps between neighbours one level apart; make sure every tile is reachable from the spawn
	// edge e: tile (x,z) to (x+1,z) = horizontal, (x,z) to (x,z+1) = vertical
	bool rampH[TILES][TILES] = { { 0 } }, rampV[TILES][TILES] = { { 0 } };
	for (int z = 0; z < TILES; z++)
		for (int x = 0; x < TILES; x++) {
			if (x + 1 < TILES && abs(sm[z][x] - sm[z][x + 1]) == 1 && rndf() < 0.4f) rampH[z][x] = true;
			if (z + 1 < TILES && abs(sm[z][x] - sm[z + 1][x]) == 1 && rndf() < 0.4f) rampV[z][x] = true;
		}
	for (int pass = 0; pass < 64; pass++) {
		bool reach[TILES][TILES] = { { 0 } };
		int qx[TILES * TILES], qz[TILES * TILES], head = 0, tail = 0;
		reach[TILES / 2][TILES / 2] = true;
		qx[tail] = TILES / 2; qz[tail++] = TILES / 2;
		while (head < tail) {
			int x = qx[head], z = qz[head++];
			const int dx[4] = { 1, -1, 0, 0 }, dz[4] = { 0, 0, 1, -1 };
			for (int k = 0; k < 4; k++) {
				int xx = x + dx[k], zz = z + dz[k];
				if (xx < 0 || zz < 0 || xx >= TILES || zz >= TILES || reach[zz][xx]) continue;
				bool ok = sm[zz][xx] == sm[z][x] ||
				          (k == 0 && rampH[z][x]) || (k == 1 && rampH[z][xx]) ||
				          (k == 2 && rampV[z][x]) || (k == 3 && rampV[zz][x]);
				if (!ok) continue;
				reach[zz][xx] = true;
				qx[tail] = xx; qz[tail++] = zz;
			}
		}
		if (tail == TILES * TILES) break;
		// connect one unreachable tile next to a reachable one
		bool fixed = false;
		for (int z = 0; z < TILES && !fixed; z++)
			for (int x = 0; x < TILES && !fixed; x++) {
				if (reach[z][x]) continue;
				const int dx[4] = { 1, -1, 0, 0 }, dz[4] = { 0, 0, 1, -1 };
				for (int k = 0; k < 4 && !fixed; k++) {
					int xx = x + dx[k], zz = z + dz[k];
					if (xx < 0 || zz < 0 || xx >= TILES || zz >= TILES || !reach[zz][xx]) continue;
					if (abs(sm[z][x] - sm[zz][xx]) > 1) sm[z][x] = sm[zz][xx] + (sm[z][x] > sm[zz][xx] ? 1 : -1);
					if (k == 0) rampH[z][x] = true;
					if (k == 1) rampH[z][xx] = true;
					if (k == 2) rampV[z][x] = true;
					if (k == 3) rampV[zz][x] = true;
					fixed = true;
				}
			}
		if (!fixed) break;
	}

	// heights: a vertex takes the highest tile it touches (so cliffs fall into the lower tile)
	for (int j = 0; j < TV; j++)
		for (int i = 0; i < TV; i++) {
			int best = 0;
			for (int dz = -1; dz <= 0; dz++)
				for (int dx = -1; dx <= 0; dx++) {
					int tx = (i + dx) / CELLS_PER_TILE, tz = (j + dz) / CELLS_PER_TILE;
					if (i + dx < 0 || j + dz < 0 || tx >= TILES || tz >= TILES) continue;
					if (sm[tz][tx] > best) best = sm[tz][tx];
				}
			s_h[j][i] = best * STEP;
		}
	// carve ramps into the lower tile of each ramp edge
	for (int z = 0; z < TILES; z++)
		for (int x = 0; x < TILES; x++)
			for (int dir = 0; dir < 2; dir++) {
				bool r = dir == 0 ? (x + 1 < TILES && rampH[z][x]) : (z + 1 < TILES && rampV[z][x]);
				if (!r) continue;
				int x2 = dir == 0 ? x + 1 : x, z2 = dir == 0 ? z : z + 1;
				int la = sm[z][x], lb = sm[z2][x2];
				if (la == lb) continue;
				bool lowIsFirst = la < lb;
				float lo = (la < lb ? la : lb) * STEP, hi = (la > lb ? la : lb) * STEP;
				int edge = (dir == 0 ? x + 1 : z + 1) * CELLS_PER_TILE;               // boundary vertex index
				float mid = ((dir == 0 ? z : x) + 0.5f) * CELLS_PER_TILE;             // along the edge
				for (int a = -(int)RAMP_HALF - 1; a <= (int)RAMP_HALF + 1; a++)
					for (int d = 0; d <= RAMP_LEN; d++) {
						int along = (int)(mid + a);
						int across = lowIsFirst ? edge - d : edge + d;
						if (along < 0 || along >= TV || across < 0 || across >= TV) continue;
						float w = clampf(RAMP_HALF + 0.5f - fabsf(along - mid), 0, 1);   // soft ramp sides
						float t = 1.0f - d / (float)RAMP_LEN;
						float h = lo + (hi - lo) * t;
						float* v = dir == 0 ? &s_h[along][across] : &s_h[across][along];
						if (d == 0) h = hi;
						*v = lerpf(*v, h, w);
					}
			}
}

static float noise2(int x, int z)
{
	u32 h = (u32)x * 374761393u + (u32)z * 668265263u + s_rng;
	h = (h ^ (h >> 13)) * 1274126177u;
	return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

static float value_noise(float x, float z)
{
	int x0 = (int)floorf(x), z0 = (int)floorf(z);
	float fx = x - x0, fz = z - z0;
	fx = fx * fx * (3 - 2 * fx);
	fz = fz * fz * (3 - 2 * fz);
	float a = noise2(x0, z0), b = noise2(x0 + 1, z0), c = noise2(x0, z0 + 1), d = noise2(x0 + 1, z0 + 1);
	return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fz);
}

static void gen_dunes(void)
{
	for (int j = 0; j < TV; j++)
		for (int i = 0; i < TV; i++) {
			float x = vx(i), z = vx(j);
			float n = value_noise(x / 45.0f, z / 45.0f) * 0.7f + value_noise(x / 18.0f, z / 18.0f) * 0.3f;
			float h = n * n * 11.0f;   // TerrainDesert.heightMultiplier; squaring keeps flats between dunes
			float r = sqrtf(x * x + z * z);
			s_h[j][i] = h * clampf((r - 10.0f) / 20.0f, 0, 1);   // flat around the spawn
		}
}

// ---------------------------------------------------------------- mesh

static float s_pn[TV * TV * 6], s_uv[TV * TV * 2];
static u16 s_idxFlat[TN * TN * 6], s_idxSteep[TN * TN * 6];

static void build_meshes(void)
{
	if (s_meshes) {
		C3D_FrameSync();   // the GPU may still be reading last frame's buffers
		mesh_free(&s_flat);
		mesh_free(&s_steep);
	}
	for (int j = 0; j < TV; j++)
		for (int i = 0; i < TV; i++) {
			float hl = s_h[j][i > 0 ? i - 1 : i], hr = s_h[j][i < TN ? i + 1 : i];
			float hd = s_h[j > 0 ? j - 1 : j][i], hu = s_h[j < TN ? j + 1 : j][i];
			vec3 n = v3_norm(v3(hl - hr, 2 * s_cell, hd - hu));
			float* v = &s_pn[(j * TV + i) * 6];
			v[0] = vx(i); v[1] = s_h[j][i]; v[2] = vx(j);
			v[3] = n.x; v[4] = n.y; v[5] = n.z;
			s_uv[(j * TV + i) * 2] = v[0] * UV_SCALE;
			s_uv[(j * TV + i) * 2 + 1] = v[2] * UV_SCALE;
		}
	int nf = 0, ns = 0;
	for (int j = 0; j < TN; j++)
		for (int i = 0; i < TN; i++) {
			int a = j * TV + i;
			float h0 = s_h[j][i], h1 = s_h[j][i + 1], h2 = s_h[j + 1][i], h3 = s_h[j + 1][i + 1];
			float hmin = fminf(fminf(h0, h1), fminf(h2, h3)), hmax = fmaxf(fmaxf(h0, h1), fmaxf(h2, h3));
			bool steep = hmax - hmin > s_cell * 0.9f;
			u16* out = steep ? &s_idxSteep[ns] : &s_idxFlat[nf];
			out[0] = a; out[1] = a + TV; out[2] = a + 1;
			out[3] = a + 1; out[4] = a + TV; out[5] = a + TV + 1;
			if (steep) ns += 6; else nf += 6;
		}
	mesh_from_floats(&s_flat, s_pn, s_uv, TV * TV, s_idxFlat, nf, texture_get(STAGE->groundTex));
	mesh_from_floats(&s_steep, s_pn, s_uv, TV * TV, s_idxSteep, ns ? ns : 3, texture_get(STAGE->cliffTex));
	if (!ns) s_steep.parts[0].count = 0;
	s_meshes = true;
}

void terrain_generate(u32 seed)
{
	s_cell = 2 * MAP_HALF / TN;
	s_rng = seed ? seed : 0x9E3779B9u;
	if (STAGE->mapBit == 2) gen_dunes();                 // desert
	else gen_plateaus(STAGE->mapBit == 8 ? 1.0f : 0.8f);  // graveyard / forest (MapParameters.volatility)
	build_meshes();
}

void terrain_flat(void)
{
	s_cell = 2 * MAP_HALF / TN;
	memset(s_h, 0, sizeof(s_h));
	build_meshes();
}

// ---------------------------------------------------------------- queries

float terrain_height(float x, float z)
{
	if (s_cell <= 0) return 0;
	float fx = (x + MAP_HALF) / s_cell, fz = (z + MAP_HALF) / s_cell;
	fx = clampf(fx, 0, TN - 0.001f);
	fz = clampf(fz, 0, TN - 0.001f);
	int i = (int)fx, j = (int)fz;
	float u = fx - i, v = fz - j;
	// same triangle split as the mesh (a, a+TV, a+1) / (a+1, a+TV, a+TV+1)
	float h00 = s_h[j][i], h10 = s_h[j][i + 1], h01 = s_h[j + 1][i], h11 = s_h[j + 1][i + 1];
	if (u + v <= 1) return h00 + (h10 - h00) * u + (h01 - h00) * v;
	return h11 + (h01 - h11) * (1 - u) + (h10 - h11) * (1 - v);
}

float terrain_slope(float x, float z)
{
	float d = s_cell * 0.5f;
	float gx = (terrain_height(x + d, z) - terrain_height(x - d, z)) / (2 * d);
	float gz = (terrain_height(x, z + d) - terrain_height(x, z - d)) / (2 * d);
	return sqrtf(gx * gx + gz * gz);
}

bool terrain_wall(vec3 from, vec3 to)
{
	float h0 = terrain_height(from.x, from.z), h1 = terrain_height(to.x, to.z);
	float run = sqrtf(dist2_xz(from, to));
	if (run < 1e-4f) return false;
	return h1 > from.y + 0.35f && (h1 - h0) / run > 1.0f;   // steeper than 45 degrees and above our feet
}

void terrain_draw(void)
{
	C3D_Mtx m;
	Mtx_Identity(&m);
	Color white = COLOR(1, 1, 1), none = { 0, 0, 0, 0 };
	render_mesh(&s_flat, 0, &m, white, none);
	if (s_steep.parts[0].count) render_mesh(&s_steep, 0, &m, COLOR(0.9f, 0.9f, 0.9f), none);
}
