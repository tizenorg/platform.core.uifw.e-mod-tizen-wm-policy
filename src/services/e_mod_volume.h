#ifndef E_MOD_VOLUME_H
#define E_MOD_VOLUME_H

#include "config.h"
#ifdef HAVE_WAYLAND_ONLY

#define E_COMP_WL
#undef E_TYPEDEFS
#include <e.h>

EINTERN Eina_Bool     e_mod_volume_client_set(E_Client *ec);
EINTERN E_Client     *e_mod_volume_client_get(void);
EINTERN Eina_Bool     e_mod_volume_region_set(int region_type, int angle, Eina_Tiler *tiler);

#endif /* HAVE_WAYLAND_ONLY */
#endif /* E_MOD_VOLUME_H */
