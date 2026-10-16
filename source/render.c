#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define FOV_Y C3D_AngleFromDegrees(55.0f)
#define Z_NEAR 0.5f
#define Z_FAR  300.0f

static DVLB_s* s_dvlb;
static shaderProgram_s s_program;
static int uLoc_projection, uLoc_modelView, uLoc_lightVec, uLoc_lightClr, uLoc_ambient, uLoc_tint, uLoc_emissive;
static int uLoc_posScale, uLoc_lerp;

static inline float clampf_(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static C3D_Mtx s_proj, s_projFlat, s_view;
static C3D_FogLut s_fogLut;
static C3D_Tex s_white;

Mesh g_meshSphere, g_meshRing, g_meshQuad, g_meshGem, g_meshCylinder, g_meshCone;

// --------------------------------------------------------------------------- caches

#define MAX_TEXTURES 384
#define MAX_MESHES 256

static struct { char name[32]; C3D_Tex tex; u16 usedW, usedH; } s_textures[MAX_TEXTURES];
static int s_numTextures;
static struct { char path[64]; Mesh* mesh; } s_meshes[MAX_MESHES];
static int s_numMeshes;

static void* read_file(const char* path, size_t* outSize)
{
	FILE* f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);
	void* data = malloc(size);
	if (data && fread(data, 1, size, f) != size) { free(data); data = NULL; }
	fclose(f);
	if (outSize) *outSize = size;
	return data;
}

typedef struct {
	char magic[4];
	u16 w, h;
	u8 fmt, pad;
	u16 pad2;
	u16 usedW, usedH;   // drawn sub-rectangle for padded UI images
} TexHeader;

C3D_Tex* texture_get(const char* name)
{
	return texture_get_sized(name, NULL, NULL);
}

C3D_Tex* texture_get_sized(const char* name, int* usedW, int* usedH)
{
	if (usedW) *usedW = 8;
	if (usedH) *usedH = 8;
	if (!name || !name[0]) return &s_white;
	for (int i = 0; i < s_numTextures; i++)
		if (strcmp(s_textures[i].name, name) == 0) {
			if (usedW) *usedW = s_textures[i].usedW;
			if (usedH) *usedH = s_textures[i].usedH;
			return &s_textures[i].tex;
		}
	if (s_numTextures >= MAX_TEXTURES) return &s_white;

	char path[96];
	snprintf(path, sizeof(path), "romfs:/tex/%s.tex", name);
	size_t size;
	u8* data = read_file(path, &size);
	if (!data) { printf("missing tex %s\n", name); return &s_white; }
	TexHeader* h = (TexHeader*)data;
	C3D_Tex* tex = &s_textures[s_numTextures].tex;
	GPU_TEXCOLOR fmt = h->fmt == 0 ? GPU_RGBA8 : GPU_RGB565;
	if (!C3D_TexInit(tex, h->w, h->h, fmt)) { free(data); return &s_white; }
	C3D_TexUpload(tex, data + sizeof(TexHeader));
	C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
	C3D_TexSetWrap(tex, GPU_REPEAT, GPU_REPEAT);
	TexHeader hdr = *h;
	h = &hdr;
	free(data);
	strncpy(s_textures[s_numTextures].name, name, 31);
	s_textures[s_numTextures].usedW = h->usedW;
	s_textures[s_numTextures].usedH = h->usedH;
	if (usedW) *usedW = h->usedW;
	if (usedH) *usedH = h->usedH;
	s_numTextures++;
	return tex;
}

typedef struct {
	char magic[4];              // "MBM2"
	u16 nverts, nframes, nparts, nanims;
	u32 nindices;
	float posScale, radius, miny, maxy;
} MeshHeader;

typedef struct {
	char tex[32];
	u32 first, count;
} MeshPartFile;

typedef struct {
	char name[16];
	u16 first, count;
	float fps;
	u8 loop, pad[3];
} MeshAnimFile;

static Mesh* mesh_cached(const char* path)
{
	for (int i = 0; i < s_numMeshes; i++)
		if (strcmp(s_meshes[i].path, path) == 0)
			return s_meshes[i].mesh;
	return NULL;
}

static Mesh* mesh_parse(const char* path, u8* data);

Mesh* mesh_load(const char* path)
{
	Mesh* m = mesh_cached(path);
	if (m) return m;
	u8* data = read_file(path, NULL);
	if (!data) { printf("missing mesh %s\n", path); return &g_meshSphere; }
	return mesh_parse(path, data);
}

// ---- background loading: the romfs read (slow) runs on a worker thread; the
// GPU-memory upload (fast) happens on the main thread in mesh_poll().
static struct {
	char path[64];       // file being read
	char next[64];       // most recent request while busy
	u8* data;
	volatile bool done;
	Thread thread;
} s_async;

static void async_worker(void* arg)
{
	(void)arg;
	s_async.data = read_file(s_async.path, NULL);
	s_async.done = true;
}

static void async_start(const char* path)
{
	strncpy(s_async.path, path, 63);
	s_async.data = NULL;
	s_async.done = false;
	s32 prio = 0x30;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	s_async.thread = threadCreate(async_worker, NULL, 16 * 1024, prio + 1, -2, false);
	if (!s_async.thread) {   // no thread available: load synchronously
		async_worker(NULL);
	}
}

void mesh_request(const char* path)
{
	if (mesh_cached(path)) return;
	if (s_async.path[0]) {                      // busy: remember only the latest request
		if (strcmp(s_async.path, path)) strncpy(s_async.next, path, 63);
		return;
	}
	async_start(path);
}

Mesh* mesh_poll(const char* path)
{
	Mesh* m = mesh_cached(path);
	if (m) return m;
	if (!s_async.path[0] || !s_async.done) {
		if (!s_async.path[0]) mesh_request(path);
		return NULL;
	}
	if (s_async.thread) { threadJoin(s_async.thread, U64_MAX); threadFree(s_async.thread); s_async.thread = NULL; }
	char finished[64];
	strncpy(finished, s_async.path, 63);
	finished[63] = 0;
	u8* data = s_async.data;
	s_async.path[0] = 0;
	if (data) mesh_parse(finished, data);
	if (s_async.next[0]) {                      // start the queued request
		char next[64];
		strncpy(next, s_async.next, 63);
		next[63] = 0;
		s_async.next[0] = 0;
		mesh_request(next);
	}
	return mesh_cached(path);
}

static Mesh* mesh_parse(const char* path, u8* data)
{
	MeshHeader* h = (MeshHeader*)data;
	if (memcmp(h->magic, "MBM2", 4)) { free(data); printf("bad mesh %s\n", path); return &g_meshSphere; }
	Mesh* m = calloc(1, sizeof(Mesh));
	m->nverts = h->nverts;
	m->nframes = h->nframes;
	m->nparts = h->nparts > MESH_MAX_PARTS ? MESH_MAX_PARTS : h->nparts;
	m->nanims = h->nanims > MESH_MAX_ANIMS ? MESH_MAX_ANIMS : h->nanims;
	m->nindices = h->nindices;
	m->posScale = h->posScale;
	m->radius = h->radius;
	m->miny = h->miny;
	m->maxy = h->maxy;

	u8* p = data + sizeof(MeshHeader);
	MeshAnimFile* anims = (MeshAnimFile*)p;
	for (int i = 0; i < m->nanims; i++) {
		memcpy(m->anims[i].name, anims[i].name, 15);
		m->anims[i].first = anims[i].first;
		m->anims[i].count = anims[i].count;
		m->anims[i].fps = anims[i].fps;
		m->anims[i].loop = anims[i].loop;
	}
	p += h->nanims * sizeof(MeshAnimFile);
	MeshPartFile* parts = (MeshPartFile*)p;
	for (int i = 0; i < m->nparts; i++) {
		m->parts[i].tex = texture_get(parts[i].tex);
		m->parts[i].first = parts[i].first;
		m->parts[i].count = parts[i].count;
	}
	p += h->nparts * sizeof(MeshPartFile);

	size_t frameBytes = (size_t)m->nframes * m->nverts * MESH_FRAME_STRIDE;
	size_t uvBytes = (size_t)m->nverts * 2 * sizeof(float);
	size_t idxBytes = (size_t)m->nindices * sizeof(u16);
	m->frames = linearAlloc(frameBytes);
	m->uvs = linearAlloc(uvBytes);
	m->indices = linearAlloc(idxBytes);
	if (!m->frames || !m->uvs || !m->indices) {
		printf("out of linear memory for %s\n", path);
		mesh_free(m); free(m); free(data);
		return &g_meshSphere;
	}
	memcpy(m->frames, p, frameBytes); p += frameBytes;
	memcpy(m->uvs, p, uvBytes); p += uvBytes;
	memcpy(m->indices, p, idxBytes);
	free(data);

	if (s_numMeshes < MAX_MESHES) {
		strncpy(s_meshes[s_numMeshes].path, path, 63);
		s_meshes[s_numMeshes].mesh = m;
		s_numMeshes++;
	}
	return m;
}

void mesh_free(Mesh* m)
{
	if (m->frames) linearFree(m->frames);
	if (m->uvs) linearFree(m->uvs);
	if (m->indices) linearFree(m->indices);
	m->frames = NULL; m->uvs = NULL; m->indices = NULL;
}

void mesh_unload(const char* path)
{
	for (int i = 0; i < s_numMeshes; i++)
		if (strcmp(s_meshes[i].path, path) == 0) {
			// the GPU may still be reading it from the frame being drawn
			C3D_FrameSync();
			mesh_free(s_meshes[i].mesh);
			free(s_meshes[i].mesh);
			s_meshes[i] = s_meshes[--s_numMeshes];
			return;
		}
}

int mesh_find_anim(const Mesh* m, const char* name)
{
	for (int i = 0; i < m->nanims; i++)
		if (strcmp(m->anims[i].name, name) == 0) return i;
	return -1;
}

void mesh_from_floats(Mesh* m, const float* pn, const float* uv, int nv, const u16* idx, int ni, C3D_Tex* tex)
{
	memset(m, 0, sizeof(*m));
	float maxAbs = 1e-4f, r = 0, miny = 1e9f, maxy = -1e9f;
	for (int i = 0; i < nv; i++) {
		const float* v = &pn[i * 6];
		for (int k = 0; k < 3; k++) if (fabsf(v[k]) > maxAbs) maxAbs = fabsf(v[k]);
		float d = sqrtf(v[0] * v[0] + v[2] * v[2]);
		if (d > r) r = d;
		if (v[1] < miny) miny = v[1];
		if (v[1] > maxy) maxy = v[1];
	}
	m->nverts = nv;
	m->nframes = 1;
	m->nparts = 1;
	m->nindices = ni;
	m->posScale = maxAbs / 32767.0f;
	m->radius = r; m->miny = miny; m->maxy = maxy;
	m->parts[0].tex = tex ? tex : &s_white;
	m->parts[0].count = ni;
	m->frames = linearAlloc(nv * MESH_FRAME_STRIDE);
	m->uvs = linearAlloc(nv * 2 * sizeof(float));
	m->indices = linearAlloc(ni * sizeof(u16));
	for (int i = 0; i < nv; i++) {
		const float* v = &pn[i * 6];
		s16* q = (s16*)(m->frames + i * MESH_FRAME_STRIDE);
		s8* n = (s8*)(m->frames + i * MESH_FRAME_STRIDE + 8);
		for (int k = 0; k < 3; k++) {
			q[k] = (s16)lroundf(v[k] / m->posScale);
			n[k] = (s8)lroundf(clampf_(v[3 + k], -1, 1) * 127.0f);
		}
		q[3] = 0; n[3] = 0;
	}
	if (uv) memcpy(m->uvs, uv, nv * 2 * sizeof(float));
	else memset(m->uvs, 0, nv * 2 * sizeof(float));
	memcpy(m->indices, idx, ni * sizeof(u16));
}

// --------------------------------------------------------------------------- primitives

#define PRIM_MAX_V 512
#define PRIM_MAX_I 1536
static float s_pv[PRIM_MAX_V * 6];
static u16 s_pi[PRIM_MAX_I];
static int s_nv, s_ni;

static int prim_vert(float x, float y, float z, float nx, float ny, float nz)
{
	float* v = &s_pv[s_nv * 6];
	v[0] = x; v[1] = y; v[2] = z; v[3] = nx; v[4] = ny; v[5] = nz;
	return s_nv++;
}

static void prim_tri(int a, int b, int c)
{
	s_pi[s_ni++] = a; s_pi[s_ni++] = b; s_pi[s_ni++] = c;
}

static void prim_finish(Mesh* m)
{
	mesh_from_floats(m, s_pv, NULL, s_nv, s_pi, s_ni, &s_white);
	s_nv = s_ni = 0;
}

static void build_primitives(void)
{
	// UV sphere, radius 1
	const int seg = 10, rings = 6;
	for (int r = 0; r <= rings; r++) {
		float phi = M_PI * r / rings;
		for (int s = 0; s <= seg; s++) {
			float th = 2 * M_PI * s / seg;
			float x = sinf(phi) * cosf(th), y = cosf(phi), z = sinf(phi) * sinf(th);
			prim_vert(x, y, z, x, y, z);
		}
	}
	for (int r = 0; r < rings; r++)
		for (int s = 0; s < seg; s++) {
			int a = r * (seg + 1) + s, b = a + seg + 1;
			prim_tri(a, b, a + 1);
			prim_tri(a + 1, b, b + 1);
		}
	prim_finish(&g_meshSphere);

	// Flat ring on XZ, outer radius 1, inner 0.8
	const int rs = 32;
	for (int s = 0; s <= rs; s++) {
		float th = 2 * M_PI * s / rs;
		prim_vert(cosf(th), 0, sinf(th), 0, 1, 0);
		prim_vert(0.8f * cosf(th), 0, 0.8f * sinf(th), 0, 1, 0);
	}
	for (int s = 0; s < rs; s++) {
		int a = s * 2;
		prim_tri(a, a + 1, a + 2);
		prim_tri(a + 1, a + 3, a + 2);
	}
	prim_finish(&g_meshRing);

	// Unit quad on XZ
	prim_vert(-0.5f, 0, -0.5f, 0, 1, 0);
	prim_vert(0.5f, 0, -0.5f, 0, 1, 0);
	prim_vert(0.5f, 0, 0.5f, 0, 1, 0);
	prim_vert(-0.5f, 0, 0.5f, 0, 1, 0);
	prim_tri(0, 1, 2); prim_tri(0, 2, 3);
	prim_finish(&g_meshQuad);

	// Octahedron gem (xp)
	static const float ov[6][3] = { {1,0,0},{-1,0,0},{0,1.4f,0},{0,-1.4f,0},{0,0,1},{0,0,-1} };
	static const int of[8][3] = { {0,2,4},{4,2,1},{1,2,5},{5,2,0},{4,3,0},{1,3,4},{5,3,1},{0,3,5} };
	for (int f = 0; f < 8; f++) {
		float* a = (float*)ov[of[f][0]]; float* b = (float*)ov[of[f][1]]; float* c = (float*)ov[of[f][2]];
		float nx = (a[0] + b[0] + c[0]), ny = (a[1] + b[1] + c[1]), nz = (a[2] + b[2] + c[2]);
		float l = sqrtf(nx * nx + ny * ny + nz * nz);
		int i0 = prim_vert(a[0], a[1], a[2], nx / l, ny / l, nz / l);
		int i1 = prim_vert(b[0], b[1], b[2], nx / l, ny / l, nz / l);
		int i2 = prim_vert(c[0], c[1], c[2], nx / l, ny / l, nz / l);
		prim_tri(i0, i1, i2);
	}
	prim_finish(&g_meshGem);

	// Open cylinder radius 1, y 0..1
	const int cs = 12;
	for (int s = 0; s <= cs; s++) {
		float th = 2 * M_PI * s / cs;
		prim_vert(cosf(th), 0, sinf(th), cosf(th), 0, sinf(th));
		prim_vert(cosf(th), 1, sinf(th), cosf(th), 0, sinf(th));
	}
	for (int s = 0; s < cs; s++) {
		int a = s * 2;
		prim_tri(a, a + 1, a + 2);
		prim_tri(a + 1, a + 3, a + 2);
	}
	prim_finish(&g_meshCylinder);

	// Cone pointing +Z (for projectiles / arrows), base radius 1, length 1
	int tip = prim_vert(0, 0, 1, 0, 0, 1);
	for (int s = 0; s <= cs; s++) {
		float th = 2 * M_PI * s / cs;
		prim_vert(cosf(th), sinf(th), 0, cosf(th), sinf(th), 0.3f);
	}
	for (int s = 0; s < cs; s++) prim_tri(tip, 1 + s, 2 + s);
	prim_finish(&g_meshCone);
}

// --------------------------------------------------------------------------- frame

void render_init(void)
{
	size_t size;
	void* sh = read_file("romfs:/shaders/scene.shbin", &size);
	if (!sh) svcBreak(USERBREAK_PANIC);
	s_dvlb = DVLB_ParseFile((u32*)sh, size);
	shaderProgramInit(&s_program);
	shaderProgramSetVsh(&s_program, &s_dvlb->DVLE[0]);

	uLoc_projection = shaderInstanceGetUniformLocation(s_program.vertexShader, "projection");
	uLoc_modelView  = shaderInstanceGetUniformLocation(s_program.vertexShader, "modelView");
	uLoc_lightVec   = shaderInstanceGetUniformLocation(s_program.vertexShader, "lightVec");
	uLoc_lightClr   = shaderInstanceGetUniformLocation(s_program.vertexShader, "lightClr");
	uLoc_ambient    = shaderInstanceGetUniformLocation(s_program.vertexShader, "ambient");
	uLoc_tint       = shaderInstanceGetUniformLocation(s_program.vertexShader, "tint");
	uLoc_emissive   = shaderInstanceGetUniformLocation(s_program.vertexShader, "emissive");
	uLoc_posScale   = shaderInstanceGetUniformLocation(s_program.vertexShader, "posScale");
	uLoc_lerp       = shaderInstanceGetUniformLocation(s_program.vertexShader, "lerp");

	Mtx_PerspTilt(&s_proj, FOV_Y, C3D_AspectRatioTop, Z_NEAR, Z_FAR, true);
	Mtx_Persp(&s_projFlat, FOV_Y, C3D_AspectRatioTop, Z_NEAR, Z_FAR, true);

	FogLut_Exp(&s_fogLut, 0.011f, 1.6f, Z_NEAR, Z_FAR);

	// 8x8 white texture for untextured draws
	C3D_TexInit(&s_white, 8, 8, GPU_RGBA8);
	u32* px = linearAlloc(8 * 8 * 4);
	for (int i = 0; i < 64; i++) px[i] = 0xFFFFFFFF;
	C3D_TexUpload(&s_white, px);
	linearFree(px);
	C3D_TexSetFilter(&s_white, GPU_NEAREST, GPU_NEAREST);

	build_primitives();
}

void render_exit(void)
{
	shaderProgramFree(&s_program);
	DVLB_Free(s_dvlb);
}

static u32 s_fogColor = 0x9D7B00;

void render_set_fog_color(float r, float g, float b)
{
	s_fogColor = ((u32)(b * 255) << 16) | ((u32)(g * 255) << 8) | (u32)(r * 255);
}

void render_begin_3d(C3D_RenderTarget* target, const C3D_Mtx* view, vec3 lightDir, Color ambient, Color light)
{
	C3D_FrameDrawOn(target);
	C3D_BindProgram(&s_program);
	s_view = *view;

	C3D_AttrInfo* attr = C3D_GetAttrInfo();
	AttrInfo_Init(attr);
	AttrInfo_AddLoader(attr, 0, GPU_SHORT, 4);   // position A (quantized)
	AttrInfo_AddLoader(attr, 1, GPU_BYTE, 4);    // normal A
	AttrInfo_AddLoader(attr, 2, GPU_FLOAT, 2);   // uv
	AttrInfo_AddLoader(attr, 3, GPU_SHORT, 4);   // position B (next frame)
	AttrInfo_AddLoader(attr, 4, GPU_BYTE, 4);    // normal B

	C3D_TexEnv* env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	for (int i = 1; i < 6; i++) C3D_TexEnvInit(C3D_GetTexEnv(i));

	C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_ALL);
	C3D_CullFace(GPU_CULL_NONE);
	C3D_AlphaTest(true, GPU_GREATER, 0x40);
	render_set_blend(false);

	C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
	C3D_FogColor(s_fogColor);
	C3D_FogLutBind(&s_fogLut);

	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_projection, &s_proj);

	// light direction (towards light) into view space
	C3D_FVec l = Mtx_MultiplyFVec4(view, FVec4_New(lightDir.x, lightDir.y, lightDir.z, 0));
	float len = sqrtf(l.x * l.x + l.y * l.y + l.z * l.z);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightVec, l.x / len, l.y / len, l.z / len, 0);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lightClr, light.r, light.g, light.b, 1);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_ambient, ambient.r, ambient.g, ambient.b, 1);
}

void render_set_blend(bool additive)
{
	if (additive)
		C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
	else
		C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
}

void render_set_depth_write(bool on)
{
	C3D_DepthTest(true, GPU_GREATER, on ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
}

static void draw_frames(const Mesh* m, int fa, int fb, float t, const C3D_Mtx* model, Color tint, Color emissive)
{
	if (!m || !m->nverts) return;
	C3D_Mtx mv;
	Mtx_Multiply(&mv, &s_view, model);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLoc_modelView, &mv);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_tint, tint.r, tint.g, tint.b, tint.a);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_emissive, emissive.r, emissive.g, emissive.b, 0);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_posScale, m->posScale, 0, 0, 0);
	C3D_FVUnifSet(GPU_VERTEX_SHADER, uLoc_lerp, t, 0, 0, 0);

	size_t frameBytes = (size_t)m->nverts * MESH_FRAME_STRIDE;
	C3D_BufInfo* buf = C3D_GetBufInfo();
	BufInfo_Init(buf);
	BufInfo_Add(buf, m->frames + fa * frameBytes, MESH_FRAME_STRIDE, 2, 0x10);   // pos A, nrm A
	BufInfo_Add(buf, m->uvs, 2 * sizeof(float), 1, 0x2);                        // uv
	BufInfo_Add(buf, m->frames + fb * frameBytes, MESH_FRAME_STRIDE, 2, 0x43);   // pos B, nrm B

	for (int i = 0; i < m->nparts; i++) {
		C3D_TexBind(0, m->parts[i].tex);
		C3D_DrawElements(GPU_TRIANGLES, m->parts[i].count, C3D_UNSIGNED_SHORT, m->indices + m->parts[i].first);
	}
}

void render_mesh(const Mesh* m, int frame, const C3D_Mtx* model, Color tint, Color emissive)
{
	if (!m || !m->nframes) return;
	if (frame < 0) frame = 0;
	frame %= m->nframes;
	draw_frames(m, frame, frame, 0, model, tint, emissive);
}

void render_mesh_anim(const Mesh* m, int anim, float t, const C3D_Mtx* model, Color tint, Color emissive)
{
	if (!m || !m->nframes) return;
	if (anim < 0 || anim >= m->nanims) { render_mesh(m, 0, model, tint, emissive); return; }
	const MeshAnim* a = &m->anims[anim];
	float f = t * a->fps;
	if (f < 0) f = 0;
	int i = (int)f;
	float frac = f - i;
	int fa, fb;
	if (a->loop) {
		fa = i % a->count;
		fb = (i + 1) % a->count;
	} else {
		fa = i < a->count ? i : a->count - 1;
		fb = fa + 1 < a->count ? fa + 1 : fa;
		if (i >= a->count) frac = 0;
	}
	draw_frames(m, a->first + fa, a->first + fb, frac, model, tint, emissive);
}

static void place(C3D_Mtx* model, vec3 pos, float yaw, float scale)
{
	Mtx_Identity(model);
	Mtx_Translate(model, pos.x, pos.y, pos.z, true);
	Mtx_RotateY(model, yaw, true);
	Mtx_Scale(model, scale, scale, scale);
}

void render_mesh_at(const Mesh* m, int frame, vec3 pos, float yaw, float scale, Color tint, Color emissive)
{
	C3D_Mtx model;
	place(&model, pos, yaw, scale);
	render_mesh(m, frame, &model, tint, emissive);
}

void render_mesh_anim_at(const Mesh* m, int anim, float t, vec3 pos, float yaw, float scale, Color tint, Color emissive)
{
	C3D_Mtx model;
	place(&model, pos, yaw, scale);
	render_mesh_anim(m, anim, t, &model, tint, emissive);
}

bool render_project(vec3 p, float* sx, float* sy)
{
	C3D_FVec v = Mtx_MultiplyFVec4(&s_view, FVec4_New(p.x, p.y, p.z, 1));
	C3D_FVec c = Mtx_MultiplyFVec4(&s_projFlat, v);
	if (c.w <= 0.01f) return false;
	*sx = (c.x / c.w * 0.5f + 0.5f) * 400.0f;
	*sy = (1.0f - (c.y / c.w * 0.5f + 0.5f)) * 240.0f;
	return true;
}
