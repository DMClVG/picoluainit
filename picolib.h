#ifndef PICOLIB_H
#define PICOLIB_H

#include <lua.h>

int picolib_open (lua_State *L);
int picolib_gpio_open (lua_State *L);
int picolib_usb_open (lua_State *L);
int picolib_pio_open (lua_State *L);
int picolib_time_open (lua_State *L);

#endif
