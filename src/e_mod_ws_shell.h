#ifndef E_MOD_WS_SHELL_H
#define E_MOD_WS_SHELL_H

#include "config.h"
#ifdef HAVE_WAYLAND_ONLY

#define E_COMP_WL
#include <e.h>

Eina_Bool e_mod_ws_shell_init(void);
void e_mod_ws_shell_shutdown(void);

#endif /* HAVE_WAYLAND_ONLY */
#endif /* E_MOD_WS_SHELL_H */
