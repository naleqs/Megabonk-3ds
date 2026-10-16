#pragma once
#include <citro3d.h>
#include "game_types.h"

#define MESH_MAX_PARTS 8

typedef struct {
	C3D_Tex* tex;
	u32 first, count;
} MeshPart;

#define MESH_MAX_ANIMS 4

typedef struct {
	char name[16];
	u16 first, count;   // frame range
	float fps;
	bool loop;
} MeshAnim;

// Vertex frame layout (12 bytes): s16 x,y,z,pad (scaled by posScale) + s8 nx,ny,nz,pad
#define MESH_FRAME_STRIDE 12

typedef struct {
	u16 nverts, nframes, nparts, nanims;
	u32 nindices;
	float posScale, radius, miny, maxy;
	MeshPart parts[MESH_MAX_PARTS];
	MeshAnim anims[MESH_MAX_ANIMS];
	u8* frames;      // linear mem: nframes * nverts * MESH_FRAME_STRIDE
	float* uvs;      // linear mem: nverts * 2
	u16* indices;    // linear mem
} Mesh;

typedef struct {
	float r, g, b, a;
} Color;

#define COLOR(r, g, b) ((Color){ (r), (g), (b), 1.0f })

// Lifecycle
void render_init(void);
void render_exit(void);

// Assets
Mesh* mesh_load(const char* path);            // cached by path
void mesh_unload(const char* path);           // free a cached mesh (big character meshes)
// Background loading: request starts reading the file on a worker thread; poll returns the
// mesh once it's ready (NULL until then). Call poll outside C3D_FrameBegin/End.
void mesh_request(const char* path);
Mesh* mesh_poll(const char* path);
// Build a static mesh from float data: posnrm = nverts * (x,y,z,nx,ny,nz); uv may be NULL.
void mesh_from_floats(Mesh* m, const float* posnrm, const float* uv, int nverts, const u16* idx, int nidx, C3D_Tex* tex);
int  mesh_find_anim(const Mesh* m, const char* name);   // -1 if missing
C3D_Tex* texture_get(const char* name);       // romfs:/tex/<name>.tex, cached; NULL name -> white
C3D_Tex* texture_get_sized(const char* name, int* usedW, int* usedH);
void mesh_free(Mesh* m);

// Procedural primitives (untextured, use tint)
extern Mesh g_meshSphere, g_meshRing, g_meshQuad, g_meshGem, g_meshCylinder, g_meshCone;

// Frame
void render_begin_3d(C3D_RenderTarget* target, const C3D_Mtx* view, vec3 lightDir, Color ambient, Color light);
void render_set_blend(bool additive);
void render_set_fog_color(float r, float g, float b);
void render_set_depth_write(bool on);

// Draw a mesh. frame selects the animation frame. tint.a < 1 for translucency.
void render_mesh(const Mesh* m, int frame, const C3D_Mtx* model, Color tint, Color emissive);
// Animated draw: plays anim (index) at time t seconds, interpolating between baked frames.
void render_mesh_anim(const Mesh* m, int anim, float t, const C3D_Mtx* model, Color tint, Color emissive);
// Convenience: translate + yaw + uniform scale
void render_mesh_at(const Mesh* m, int frame, vec3 pos, float yaw, float scale, Color tint, Color emissive);
void render_mesh_anim_at(const Mesh* m, int anim, float t, vec3 pos, float yaw, float scale, Color tint, Color emissive);

// Projection helpers for 2D overlays (returns false if behind camera)
bool render_project(vec3 world, float* sx, float* sy);
