#ifndef COMMON_H
#define COMMON_H

#include "static.h"

/* 
	When using gSPVertex w/ ref.fifo, n (1-64) and v0 (0-63) 
	In regular (fifo), n (1-32) and v0 (0-31)
*/

/* Symbols used by spec */
#define	STATIC_SEGMENT 1
#define CFB_SEGMENT 2

/* RDP FIFO output buffer len (min. 0x410 for fifo, 0x600 for ref.fifo) */
#define RDP_OUTPUT_LEN ((1024 * 4) * 16)

/* Thread stack */
#define STACK_SIZE 0x2000
 
/* Threads */
#define THRD_ID_MAX 12
#define THRD_IDLE_ID OS_PRIORITY_IDLE
#define THRD_MAIN_ID (THRD_IDLE_ID + 1)

/* Screen width & height */
#define	SCREEN_W 320
#define	SCREEN_H 240

/* Macro for 32-bit RGBA color */
#define GPACK_RGBA5551_32(r, g, b, a) \
	(GPACK_RGBA5551(r, g, b, a) << 16 | GPACK_RGBA5551(r, g, b, a))

/* Length of Gfx list */
#define GFXLIST_LEN (1024 * 2)

/* Non-mipmapped textxure setup */
#define GFX_TEX_RGBA(tex, comb) \
	gsDPPipeSync(), \
	gsDPSetCombineMode((comb), (comb)), \
	gsDPSetTexturePersp(G_TP_PERSP), \
	gsDPSetTextureLOD(G_TL_TILE), \
	gsDPSetTextureFilter(G_TF_BILERP), \
	gsDPSetTextureConvert(G_TC_FILT), \
	gsDPSetTextureLUT(G_TT_NONE), \
	gsDPLoadTextureBlock((tex), G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 16, 0, \
		G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, \
		G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD), \

/* This structure holds the things which change per frame. It is advantageous
   to keep dynamic data together so that we may selectively write back dirty
   data cache lines to DRAM prior to processing by the RCP */
typedef struct {
	Mtx model, view, projection; /* MVP matrices */
	Gfx gfxlist[GFXLIST_LEN]; /* Gfx list for the RDP */
} DynamicGfx;

extern DynamicGfx dyn_gfx;

#ifdef _LANGUAGE_C /* Needed because spec includes this file */
	/* RSP matrix stack */
	extern u64 dram_stack[];

	/* RDP microcode FIFO command buffer */
	extern u64 rdp_output[];

	/* Color frame buffer */
	extern u16 cfb[][SCREEN_W * SCREEN_H];

	/* RSP color frame buffer */
	extern u16 rsp_cfb[];

	/* Z buffer */
	extern u16 zbuffer[];
#endif

#endif
