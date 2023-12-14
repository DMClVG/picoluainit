#include "picolib.h"
#include <lauxlib.h>

static const luaL_Reg libs[] = { { "gpio", picolib_gpio_open },
                                 { "pio", picolib_pio_open },
                                 { "usb", picolib_usb_open },
                                 { "time", picolib_time_open },
                                 { NULL, NULL } };

static const luaL_Reg lib_gpio[] = {

{NULL, NULL}
}

int
picolib_gpio_open (L)
{
  luaL_newlib (L, l)
}

int
picolib_open (lua_State *L)
{
  for (const luaL_Reg *lib = &libs[0]; lib->func; lib++)
    {
      luaL_requiref (L, lib->name, lib->func, 1);
      lua_pop (L, 1);
    }
}
