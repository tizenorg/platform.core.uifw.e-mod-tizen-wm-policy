#ifndef E_MOD_ROTATION_WL_H
#define E_MOD_ROTATION_WL_H

#include "config.h"
#ifdef HAVE_WAYLAND_ONLY

#define E_COMP_WL
#include <e.h>

Eina_Bool          e_mod_rot_wl_init(void);
void               e_mod_rot_wl_shutdown(void);
EINTERN void       e_zone_rotation_set(E_Zone *zone, int rotation);
EINTERN Eina_Bool  e_zone_rotation_block_set(E_Zone *zone, const char *name_hint, Eina_Bool set);

#endif /* HAVE_WAYLAND_ONLY */
#endif /* E_MOD_ROTATION_WL_H */
