#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"

#define MUSIC_CH        0
#define SFX_CH_FIRST    1
#define SFX_CH_COUNT    10
#define STREAM_SAMPLES  4096
#define STREAM_BUFS     3

static const char* const s_sfxFiles[SFX_COUNT] = {
	"hit", "enemy_die", "player_hurt", "xp", "levelup", "jump", "fireball",
	"explosion", "throw", "sword", "gun", "lightning", "heal", "chest", "menu", "select",
	"pot", "shrine", "swarm", "boss", "purchase", "gold",
};
static const float s_sfxVolume[SFX_COUNT] = {
	0.35f, 0.4f, 0.8f, 0.3f, 0.8f, 0.6f, 0.5f, 0.5f, 0.5f, 0.6f, 0.4f, 0.6f, 0.7f, 0.8f, 0.6f, 0.7f,
	0.7f, 0.7f, 0.9f, 0.9f, 0.8f, 0.35f,
};
static const char* const s_musicFiles[MUSIC_COUNT] = {
	"menu", "forest", "forest_boss", "desert", "desert_boss", "graveyard", "graveyard_boss",
};

typedef struct {
	s16* data;
	u32 samples;
	u32 rate;
} Sound;

static bool s_ok;
static Sound s_sfx[SFX_COUNT];
static u64 s_sfxLast[SFX_COUNT];
static ndspWaveBuf s_sfxBufs[SFX_CH_COUNT];
static int s_nextSfxCh;

static FILE* s_music;
static u32 s_musicRate, s_musicSamples, s_musicPos;
static bool s_musicAdpcm;
static int s_adpcmPred, s_adpcmIndex;
static s16* s_streamMem;
static ndspWaveBuf s_streamBufs[STREAM_BUFS];

typedef struct { char magic[4]; u32 rate; u32 samples; } SndHeader;

// IMA ADPCM (music files are "SND2": 4 bits per sample, low nibble first)
static const s8 s_imaIndex[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };
static const s16 s_imaStep[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
	107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
	876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
	5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623,
	27086, 29794, 32767,
};

static s16 ima_decode(int code)
{
	int step = s_imaStep[s_adpcmIndex];
	int delta = step >> 3;
	if (code & 4) delta += step;
	if (code & 2) delta += step >> 1;
	if (code & 1) delta += step >> 2;
	s_adpcmPred += (code & 8) ? -delta : delta;
	if (s_adpcmPred > 32767) s_adpcmPred = 32767;
	if (s_adpcmPred < -32768) s_adpcmPred = -32768;
	s_adpcmIndex += s_imaIndex[code & 7];
	if (s_adpcmIndex < 0) s_adpcmIndex = 0;
	if (s_adpcmIndex > 88) s_adpcmIndex = 88;
	return (s16)s_adpcmPred;
}

static void music_rewind(void)
{
	fseek(s_music, sizeof(SndHeader), SEEK_SET);
	s_musicPos = 0;
	s_adpcmPred = 0;
	s_adpcmIndex = 0;
}

// Reads up to `want` samples into dst (PCM16 or decoded ADPCM); returns samples produced.
static u32 music_read(s16* dst, u32 want)
{
	if (!s_musicAdpcm) return fread(dst, sizeof(s16), want, s_music);
	static u8 packed[STREAM_SAMPLES / 2];
	u32 bytes = fread(packed, 1, (want + 1) / 2, s_music);
	u32 n = 0;
	for (u32 i = 0; i < bytes && n < want; i++) {
		dst[n++] = ima_decode(packed[i] & 15);
		if (n < want) dst[n++] = ima_decode(packed[i] >> 4);
	}
	return n;
}

static bool load_sound(const char* path, Sound* out)
{
	FILE* f = fopen(path, "rb");
	if (!f) return false;
	SndHeader h;
	if (fread(&h, sizeof(h), 1, f) != 1 || memcmp(h.magic, "SND1", 4)) { fclose(f); return false; }
	out->data = linearAlloc(h.samples * sizeof(s16));
	out->samples = h.samples;
	out->rate = h.rate;
	if (!out->data || fread(out->data, sizeof(s16), h.samples, f) != h.samples) { fclose(f); return false; }
	fclose(f);
	DSP_FlushDataCache(out->data, h.samples * sizeof(s16));
	return true;
}

void audio_init(void)
{
	if (R_FAILED(ndspInit())) return;
	s_ok = true;
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	char path[64];
	for (int i = 0; i < SFX_COUNT; i++) {
		snprintf(path, sizeof(path), "romfs:/sfx/%s.snd", s_sfxFiles[i]);
		load_sound(path, &s_sfx[i]);
	}
	for (int c = 0; c < SFX_CH_COUNT; c++) {
		int ch = SFX_CH_FIRST + c;
		ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
		ndspChnSetFormat(ch, NDSP_FORMAT_MONO_PCM16);
	}
	s_streamMem = linearAlloc(STREAM_SAMPLES * STREAM_BUFS * sizeof(s16));
}

bool audio_available(void) { return s_ok; }

void audio_exit(void)
{
	if (!s_ok) return;
	audio_stop_music();
	ndspExit();
	for (int i = 0; i < SFX_COUNT; i++)
		if (s_sfx[i].data) linearFree(s_sfx[i].data);
	linearFree(s_streamMem);
	s_ok = false;
}

void audio_play_sfx(SfxId id)
{
	if (!s_ok || !s_sfx[id].data) return;
	// don't let rapid-fire sounds (hits, xp) stack up
	u64 now = osGetTime();
	if (now - s_sfxLast[id] < 45) return;
	s_sfxLast[id] = now;

	int c = s_nextSfxCh;
	s_nextSfxCh = (s_nextSfxCh + 1) % SFX_CH_COUNT;
	int ch = SFX_CH_FIRST + c;
	ndspChnWaveBufClear(ch);
	ndspChnSetRate(ch, (float)s_sfx[id].rate);
	float mix[12] = { 0 };
	mix[0] = mix[1] = s_sfxVolume[id];
	ndspChnSetMix(ch, mix);
	ndspWaveBuf* wb = &s_sfxBufs[c];
	memset(wb, 0, sizeof(*wb));
	wb->data_vaddr = s_sfx[id].data;
	wb->nsamples = s_sfx[id].samples;
	ndspChnWaveBufAdd(ch, wb);
}

static void stream_fill(ndspWaveBuf* wb)
{
	s16* dst = (s16*)wb->data_vaddr;
	u32 got = 0;
	while (got < STREAM_SAMPLES) {
		u32 want = STREAM_SAMPLES - got;
		if (want > s_musicSamples - s_musicPos) want = s_musicSamples - s_musicPos;
		u32 n = music_read(dst + got, want);
		got += n;
		s_musicPos += n;
		if (s_musicPos >= s_musicSamples || n == 0) music_rewind();   // loop
	}
	wb->nsamples = STREAM_SAMPLES;
	DSP_FlushDataCache(dst, STREAM_SAMPLES * sizeof(s16));
	ndspChnWaveBufAdd(MUSIC_CH, wb);
}

void audio_stop_music(void)
{
	if (!s_ok) return;
	ndspChnWaveBufClear(MUSIC_CH);
	if (s_music) fclose(s_music);
	s_music = NULL;
}

void audio_play_music(MusicId id)
{
	if (!s_ok) return;
	audio_stop_music();
	char path[64];
	snprintf(path, sizeof(path), "romfs:/music/%s.snd", s_musicFiles[id]);
	s_music = fopen(path, "rb");
	if (!s_music) return;
	SndHeader h;
	if (fread(&h, sizeof(h), 1, s_music) != 1) { fclose(s_music); s_music = NULL; return; }
	s_musicRate = h.rate;
	s_musicSamples = h.samples;
	s_musicAdpcm = memcmp(h.magic, "SND2", 4) == 0;
	music_rewind();
	ndspChnReset(MUSIC_CH);
	ndspChnSetInterp(MUSIC_CH, NDSP_INTERP_LINEAR);
	ndspChnSetFormat(MUSIC_CH, NDSP_FORMAT_MONO_PCM16);
	ndspChnSetRate(MUSIC_CH, (float)s_musicRate);
	float mix[12] = { 0 };
	mix[0] = mix[1] = 0.55f;
	ndspChnSetMix(MUSIC_CH, mix);
	for (int i = 0; i < STREAM_BUFS; i++) {
		memset(&s_streamBufs[i], 0, sizeof(ndspWaveBuf));
		s_streamBufs[i].data_vaddr = s_streamMem + i * STREAM_SAMPLES;
		stream_fill(&s_streamBufs[i]);
	}
}

void audio_update(void)
{
	if (!s_ok || !s_music) return;
	for (int i = 0; i < STREAM_BUFS; i++)
		if (s_streamBufs[i].status == NDSP_WBUF_DONE)
			stream_fill(&s_streamBufs[i]);
}
