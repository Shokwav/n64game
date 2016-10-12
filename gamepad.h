#ifndef GAMEPAD_H
#define GAMEPAD_H

#include <ultra64.h>

/*------------Macros------------*/
#define PAD_1 0
#define PAD_2 1
#define PAD_3 2
#define PAD_4 3

/*------------Functions------------*/
void pad_update(const OSContPad* pad, u32 which);

#endif
