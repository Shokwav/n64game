#include <ultra64.h>
#include "common.h"

/* Initial viewport */
static Vp vp = {
	/* Scale */
	SCREEN_W * 2.0f, SCREEN_H * 2.0f, G_MAXZ / 2.0f, 0.0f,

	/* Translation */
	SCREEN_W * 2.0f, SCREEN_H * 2.0f, G_MAXZ / 2.0f, 0.0f
};

/*-----Static-Display-Lists-----*/

/* Initialize the RSP state */
Gfx dl_rspinit[] = {
	gsSPViewport(&vp),
	gsSPClearGeometryMode(G_SHADE | G_SHADING_SMOOTH | 
		G_CULL_BOTH | G_FOG | G_LIGHTING | G_TEXTURE_GEN |
		G_TEXTURE_GEN_LINEAR | G_LOD),
	gsSPTexture(0, 0, 0, 0, G_OFF),
	gsSPSetGeometryMode(G_SHADE | G_SHADING_SMOOTH | G_ZBUFFER),
	gsSPEndDisplayList()
};

/* Initialize the RDP state */
Gfx dl_rdpinit[] = {
	/* Cycle type */
	gsDPSetCycleType(G_CYC_1CYCLE),

	/* Set pipeline mode */
	gsDPPipelineMode(G_PM_1PRIMITIVE),
	
	/* Set scissor area */
	gsDPSetScissor(G_SC_NON_INTERLACE, 0, 0, SCREEN_W, SCREEN_H),

	/* Set texture info */
	gsDPSetTextureLOD(G_TL_TILE), /* Level of detail */
	gsDPSetTextureLUT(G_TT_NONE), /* Look up texture */
	gsDPSetTextureDetail(G_TD_CLAMP), /* Detail type - clamp */
	gsDPSetTexturePersp(G_TP_PERSP), /* Perspective correction */
	gsDPSetTextureFilter(G_TF_BILERP), /* Bilinear interpolation */
	gsDPSetTextureConvert(G_TC_FILT), /* Filter convert textures */

	/* Combine mode */
	gsDPSetCombineMode(G_CC_SHADE, G_CC_SHADE),
	gsDPSetCombineKey(G_CK_NONE),

	/* Set alpha compare - none */
	gsDPSetAlphaCompare(G_AC_NONE),

	/* Set render mode - anti-aliased, opaque surfaces */
	gsDPSetRenderMode(G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2),

	gsDPSetColorDither(G_CD_DISABLE),

	gsDPSetDepthImage(zbuffer),

	gsDPPipeSync(),

	gsSPEndDisplayList()
};

/* Clear the color frame buffer */
Gfx dl_clearcfb[] = {
    gsDPSetCycleType(G_CYC_FILL),
    gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_W, rsp_cfb),
    gsDPSetFillColor(GPACK_RGBA5551_32(128, 128, 128, 1)),
    gsDPFillRectangle(0, 0, SCREEN_W - 1, SCREEN_H - 1),
    gsDPPipeSync(),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsSPEndDisplayList()
};

/* Clear the z buffer */
Gfx dl_clearzbuffer[] = {
	gsDPSetDepthImage(OS_K0_TO_PHYSICAL(zbuffer)),
	gsDPSetCycleType(G_CYC_FILL),
	gsDPSetColorImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, SCREEN_W, OS_K0_TO_PHYSICAL(zbuffer)),
	gsDPSetFillColor(GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)),
	gsDPFillRectangle(0, 0, SCREEN_W - 1, SCREEN_H - 1),
	gsSPEndDisplayList()
};

/* */
