#include <ultra64.h>
#include "common.h"

/* used for matrix stack */
u64 dram_stack[SP_DRAM_STACK_SIZE64] __attribute__((aligned (16)));

/* Output buffer for RDP's FIFO microcode */
u64 rdp_output[RDP_OUTPUT_LEN] __attribute__((aligned (16)));
