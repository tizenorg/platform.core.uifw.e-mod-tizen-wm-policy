#ifndef E_MOD_ROTATION_WL_H
#define E_MOD_ROTATION_WL_H

#include "config.h"
#ifdef HAVE_WAYLAND_ONLY

#define E_COMP_WL
#include <e.h>

Eina_Bool e_mod_rot_wl_init(void);
void e_mod_rot_wl_shutdown(void);

void e_mod_rot_wl_angle_change_send(E_Client *ec, int angle);

#endif /* HAVE_WAYLAND_ONLY */
#endif /* E_MOD_ROTATION_WL_H */
