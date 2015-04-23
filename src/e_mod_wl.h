#ifndef E_MOD_WL_H
#define E_MOD_WL_H

#include "config.h"
#ifdef HAVE_WAYLAND_ONLY

#define E_COMP_WL
#include <e.h>

Eina_Bool e_mod_pol_wl_init(void);
void e_mod_pol_wl_shutdown(void);
void e_mod_pol_wl_client_del(E_Client *ec);

/* visibility */
void e_mod_pol_wl_visibility_send(E_Client *ec, int visibility);

#endif /* HAVE_WAYLAND_ONLY */
#endif /* E_MOD_WL_H */