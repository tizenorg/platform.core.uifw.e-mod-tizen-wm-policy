#ifndef E_MOD_ROTATION_H
#define E_MOD_ROTATION_H
#include <e.h>

EINTERN int e_client_rotation_curr_angle_get(const E_Client *ec);
EINTERN void e_mod_pol_rotation_init(void);
EINTERN void e_mod_pol_rotation_shutdown(void);
#endif
