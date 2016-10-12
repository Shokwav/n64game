#include <ultra64.h>
#include "common.h"
#include "mathext.h"

/*-----ROM-symbols-----*/

/* Code segment */
extern char _codeSegmentEnd[], _codeSegmentTextEnd[], _codeSegmentTextStart[];

/* Z buffer segment */
extern char _zbufferSegmentEnd[];

/* Static data segment */
extern char _staticSegmentRomStart[], _staticSegmentRomEnd[];

/* Texture data segment */
extern char _textureSegmentRomStart[], _textureSegmentRomEnd[];

/*-----Threads-----*/
static OSThread idle_thrd, main_thrd;

/* Thread stacks */
u64 boot_stack[STACK_SIZE / sizeof(u64)];
static u64 idle_thrd_stack[STACK_SIZE / sizeof(u64)]; /* todo: make this smaller */
static u64 main_thrd_stack[STACK_SIZE / sizeof(u64)]; /* ^ */

/* Thread entry points */
static void idle_func(void* a);
static void main_func(void* a);

/*-----DMA-Message-Passing-----*/
static OSMesgQueue rdpdone_msg_queue, retrace_msg_queue;
static OSMesg rdpdone_msg_buff, retrace_msg_buff;

/*-----PI-Message-Passing-----*/
#define PI_MSG_SIZE 8

static OSPiHandle* pi_hndl = NULL; /* PI (gamepad) handle */

static OSMesgQueue pi_msg_queue;
static OSMesg pi_msg_buff[PI_MSG_SIZE];

/*-----Gamepad-Message-Passing-----*/
static OSMesgQueue pad_msg_queue;
static OSMesg pad_msg_buff[2];

/*-----Gamepad-Status----- */
static OSContStatus pad_status_queue[MAXCONTROLLERS];
static OSContPad pad_data_buff[MAXCONTROLLERS];

/* RSP Task descriptor */
static OSTask task_list = {
    M_GFXTASK,			/* task type (M_GFXTASK or M_AUDTASK) */
    0,				/* task flags */

    NULL,			/* boot ucode pointer (fill in later) */
    0, 				/* boot ucode size (fill in later) */

    NULL, 			/* task ucode pointer (fill in later) */
    SP_UCODE_SIZE,		/* task ucode size */

    NULL, 			/* task ucode data pointer (fill in later) */
    SP_UCODE_DATA_SIZE,		/* task ucode data size */

    &dram_stack[0],		/* task dram stack pointer */
    SP_DRAM_STACK_SIZE8,	/* task dram stack size */

    /* &rdp_output[0], */		/* task output buffer ptr */
    /* &rdp_output[0] + RDP_OUTPUT_LEN, */ /* task output buffer size ptr */

    NULL,			/* task data pointer (fill in later) */
    0,				/* task data size (fill in later) */

    NULL,			/* task yield buffer ptr (unused) */
    0				/* task yield buffer size (unused) */
};

/* Dynamic data (& ptr) */
static DynamicGfx dyn_gfx, *dyn_gfx_p;

/* RSP display list pointer */
static Gfx* gfxlist_p;

/* Task list pointer */
static OSTask* task_list_p;

/* The current cfb (color frame buffer, double-buffered) */
static u32 curr_cfb;

/*-----Application-Entry-Point-----*/
u32 boot(void){
	/* Init OS */
	osInitialize();

	/* Init cart ROM */
	pi_hndl = osCartRomInit();

	/* Create idle thread */
	osCreateThread(&idle_thrd, 1, &idle_func, NULL,
		idle_thrd_stack + (STACK_SIZE / sizeof(u64)), 10);
	osStartThread(&idle_thrd);

	return (0); /* Never reached */
}

/* Idle function entry point */
void idle_func(void* args){
	/* Video setup (NTSC lo-res mode) */
	osCreateViManager(OS_PRIORITY_VIMGR);
	osViSetMode(&osViModeTable[OS_VI_NTSC_LAN1]);
	osViSetSpecialFeatures(OS_VI_GAMMA_ON | OS_VI_GAMMA_DITHER_ON | OS_VI_DIVOT_ON | 
		OS_VI_DITHER_FILTER_ON);

	/* Init PI manager */
	osCreatePiManager((OSPri)(OS_PRIORITY_PIMGR), &pi_msg_queue, pi_msg_buff, PI_MSG_SIZE);

	/* Create main thread */
	osCreateThread(&main_thrd, 3, &main_func, NULL,
		main_thrd_stack + (STACK_SIZE / sizeof(u64)), /*OS_PRIORITY_IDLE + 1*/ 10);
	osStartThread(&main_thrd);

	/* Set current thread as idle thread */
	osSetThreadPri(NULL, OS_PRIORITY_IDLE);

	while(TRUE);
}

/* Main function entry point */
void main_func(void* args){
	/* Initial pad state */
	u8 pad_init_pttrn = 0;

	/* General app vars */
	u32 i = 0;

	/* Initialize vars */
	curr_cfb = 0;

	/* Set up the message queues */
	osCreateMesgQueue(&rdpdone_msg_queue, &rdpdone_msg_buff, 1);
	osSetEventMesg(OS_EVENT_DP, &rdpdone_msg_queue, NULL);
    
	osCreateMesgQueue(&retrace_msg_queue, &retrace_msg_buff, 1);
	osViSetEvent(&retrace_msg_queue, NULL, 1);

     	/* Stick the static segment right after the code segment (using DMA) */
	{
		OSMesgQueue dma_msg_queue;
		OSMesg dma_msg_buff;
		OSIoMesg dmaIO_msg_buff;

		osCreateMesgQueue(&dma_msg_queue, &dma_msg_buff, 1);

		dmaIO_msg_buff.hdr.pri = OS_MESG_PRI_NORMAL;
		dmaIO_msg_buff.hdr.retQueue = &dma_msg_queue;
		dmaIO_msg_buff.dramAddr = _codeSegmentEnd;
		dmaIO_msg_buff.devAddr = (u32)(_staticSegmentRomStart);
		dmaIO_msg_buff.size = (u32)(_staticSegmentRomEnd) - (u32)(_staticSegmentRomStart);

		/* Begin DMA */
		osEPiStartDma(pi_hndl, &dmaIO_msg_buff, OS_READ);
    
		/* Wait for DMA to finish */
		osRecvMesg(&dma_msg_queue, NULL, OS_MESG_BLOCK);

		/* Stick texture segment right after the static segment */
	}

	/* Initialize gamepads */
	osCreateMesgQueue(&pad_msg_queue, pad_msg_buff, 2);
	osSetEventMesg(OS_EVENT_SI, &pad_msg_queue, NULL);
	osContInit(&pad_msg_queue, &pad_init_pttrn, pad_status_queue);

	/* Main loop */
	while(TRUE){
		/* Reset pointers to build the display list */
		task_list_p = &task_list;
		dyn_gfx_p = &dyn_gfx;
		gfxlist_p = dyn_gfx.gfxlist;

		/* Start gamepad read */
		osContStartReadData(&pad_msg_queue);

		/*-----Initialize-matrices-----*/

		/* Projection (ortho) */
		guOrtho(&(dyn_gfx.projection), -(float)(SCREEN_W / 2.0), 
			(float)(SCREEN_W / 2.0), -(float)(SCREEN_H / 2.0), 
			(float)(SCREEN_H / 2.0F), 1.0, 10.0, 1.0);

		/* guPerspective(&(dyn_gfx.projection), &persp_norm, 33, 1, 10, 5000, 1.0); */

		/* Rotation of model matrix */
		guRotate(&(dyn_gfx.model), 0.0, 0.0, 0.0, 1.0);

		/*-----RCP-Segment-Initialization-----*/

		/* Physical address segment; this is required */
		gSPSegment(gfxlist_p, 0, 0x0); 

		/* Static data segment */
		gSPSegment(++gfxlist_p, STATIC_SEGMENT, OS_K0_TO_PHYSICAL(_codeSegmentEnd));

		/* (Current) CFB segment */
		gSPSegment(++gfxlist_p, CFB_SEGMENT, OS_K0_TO_PHYSICAL(cfb[curr_cfb]));

		/*-----Load-Display-Lists-----*/

		/* Initialize RDP */
		gSPDisplayList(++gfxlist_p, dl_rdpinit);
	
		/* Initialize RSP */
		gSPDisplayList(++gfxlist_p, dl_rspinit);

		/* Clear color framebuffer */
		gSPDisplayList(++gfxlist_p, dl_clearcfb);

		/* Clear Z buffer */
		gSPDisplayList(++gfxlist_p, dl_clearzbuffer);

		gDPFullSync(++gfxlist_p);
		gSPEndDisplayList(++gfxlist_p);

		/*-----Build-Graphics-Task-----*/
		(task_list_p -> t).ucode_boot = (u64*)(rspbootTextStart);
		(task_list_p -> t).ucode_boot_size = (u32)(rspbootTextEnd) - (u32)(rspbootTextStart);

		/* Loads the microcode - F3DEX2 with RSP outputting commands to RDP 
		   using a FIFO buffer */
		(task_list_p -> t).ucode = (u64*)(gspF3DEX2_fifoTextStart);
		(task_list_p -> t).ucode_data = (u64*)(gspF3DEX2_fifoDataStart);

		/* Set task ptr to display lists */
		(task_list_p -> t).data_ptr = (u64*)(dyn_gfx_p -> gfxlist);
		(task_list_p -> t).data_size = (u32)((gfxlist_p - dyn_gfx_p -> gfxlist) * sizeof(Gfx));

		/* Write back dynamic data's dirty cache lines that need to be read by the RCP */
		osWritebackDCache(&dyn_gfx, sizeof(DynamicGfx));
	
		/* Start up the RSP task */
		osSpTaskStart(task_list_p);

		/* Wait for RDP completion */
		osRecvMesg(&rdpdone_msg_queue, NULL, OS_MESG_BLOCK);
	
		/* Setup to swap buffers */
		osViSwapBuffer(cfb[curr_cfb]);

		/* Make sure there isn't an old retrace in queue (assumes depth of 1) */
		if (MQ_IS_FULL(&retrace_msg_queue)){
			osRecvMesg(&retrace_msg_queue, NULL, OS_MESG_BLOCK);
		}

		/* Wait for vertical retrace to finish */
		osRecvMesg(&retrace_msg_queue, NULL, OS_MESG_BLOCK);

		/* Swap which buffer to draw to (0 -> 1 || 1 -> 0) */
		curr_cfb ^= 1;

		/* End gamepad read */
		osRecvMesg(&pad_msg_queue, NULL, OS_MESG_BLOCK);
		osContGetReadData(pad_data_buff);

		/* Process pads */
		for(i = 0; i < MAXCONTROLLERS; ++i){
			pad_update(pad_data_buff[i], i);
		}
	}
}
