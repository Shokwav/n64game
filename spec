/* ROM specification file (used by makerom) */

#include "common.h"

beginseg
	name "code"
	flags BOOT OBJECT
	entry boot
	stack boot_stack + STACK_SIZE
	include "codesegment.o"
	include "$(ROOT)/usr/lib/PR/rspboot.o"
	
	/* Include microcode */
	include "$(ROOT)/usr/lib/PR/gspF3DEX2.fifo.o"
	include "$(ROOT)/usr/lib/PR/gspF3DEX2.rej.fifo.o"
endseg

beginseg
	name "zbuffer"
	flags OBJECT
	address 0x80118000
	include "zbuffer.o"
endseg

beginseg
	name "cfb"
	flags OBJECT
	address 0x80300000
	include "cfb.o"
endseg

beginseg
	name "static"
	flags OBJECT
	number STATIC_SEGMENT
	include "static.o"
endseg

beginseg
	name "rsp_cfb"
	flags OBJECT
	number CFB_SEGMENT
	include "rsp_cfb.o"
endseg

beginwave
	name "game"
	include "code"
	include "zbuffer"
	include "cfb"
	include "static"
	include "rsp_cfb"
endwave
