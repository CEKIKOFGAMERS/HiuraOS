#ifndef PS2_H
#define PS2_H

#include "types.h"

void ps2_init(void);
void ps2_poll(void);   /* calls gui_mouse_update / gui_key internally */

#endif
