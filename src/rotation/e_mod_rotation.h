#ifndef E_MOD_ROTATION_H
#define E_MOD_ROTATION_H
#include <e.h>
#include "tizen_policy_ext-server-protocol.h"

#define ROT_IDX_NUM  4

typedef enum
{
   ROT_IDX_NONE = TIZEN_ROTATION_ANGLE_NONE,
   ROT_IDX_0 = TIZEN_ROTATION_ANGLE_0,
   ROT_IDX_90 = TIZEN_ROTATION_ANGLE_90,
   ROT_IDX_180 = TIZEN_ROTATION_ANGLE_180,
   ROT_IDX_270 = TIZEN_ROTATION_ANGLE_270,
} Rot_Idx;

typedef enum
{
   ROT_TYPE_UNKNOWN = -1,
   ROT_TYPE_NORMAL = E_CLIENT_ROTATION_TYPE_NORMAL,
   ROT_TYPE_DEPENDENT = E_CLIENT_ROTATION_TYPE_DEPENDENT,
} Rot_Type;

EINTERN void   e_mod_rot_zone_set(E_Zone *zone, int rot);
EINTERN int    e_client_rotation_curr_angle_get(const E_Client *ec);
EINTERN int    e_mod_pol_rotation_init(void);
EINTERN void   e_mod_pol_rotation_shutdown(void);

#include "e_mod_rotation.x"
#endif
