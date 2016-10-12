#include "mathext.h"

static u32 seed_val = 7; /* Seed value */

u32 rand_i(void){
	seed_val ^= seed_val << 13;
	seed_val ^= seed_val >> 17;
	seed_val ^= seed_val << 5;
	return (seed_val);
}


