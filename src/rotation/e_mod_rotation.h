#ifndef E_MOD_ROTATION_H
#define E_MOD_ROTATION_H
#include <e.h>

typedef enum
{
   ROT_IDX_0 = 0,
   ROT_IDX_90,
   ROT_IDX_180,
   ROT_IDX_270,
   ROT_IDX_NUM
} Rot_Idx;

#ifdef HAVE_WAYLAND_ONLY
#include "e_mod_rotation_wl.h"
#else /* HAVE_WAYLAND_ONLY */
static inline void
e_zone_rotation_set(E_Zone *zone EINA_UNUSED, int rot EINA_UNUSED)
{
   return;
}

static inline Eina_Bool
e_zone_rotation_block_set(E_Zone *zone EINA_UNUSED, const char *name_hint EINA_UNUSED, Eina_Bool set EINA_UNUSED)
{
   return EINA_FALSE;
}
#endif /* HAVE_WAYLAND_ONLY */

EINTERN int e_client_rotation_curr_angle_get(const E_Client *ec);
EINTERN void e_mod_pol_rotation_init(void);
EINTERN void e_mod_pol_rotation_shutdown(void);

#include "e_mod_rotation.x"
#endif
