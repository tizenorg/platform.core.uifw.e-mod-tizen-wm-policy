#ifndef E_MOD_POL_NOTIFICATION_H
#define E_MOD_POL_NOTIFICATION_H

#include <e.h>

/* wm-policy notification level */
enum
{
   E_POL_NOTIFICATION_LEVEL_1 = 0,
   E_POL_NOTIFICATION_LEVEL_2 = 1,
   E_POL_NOTIFICATION_LEVEL_3 = 2,
   E_POL_NOTIFICATION_LEVEL_NONE = -1,
   E_POL_NOTIFICATION_LEVEL_DEFAULT = 10,
   E_POL_NOTIFICATION_LEVEL_MEDIUM = 20,
   E_POL_NOTIFICATION_LEVEL_HIGH = 30,
   E_POL_NOTIFICATION_LEVEL_TOP = 40,
};

EINTERN void      e_mod_pol_notification_init(void);
EINTERN void      e_mod_pol_notification_shutdown(void);
EINTERN void      e_mod_pol_notification_client_del(E_Client *ec);

EINTERN void      e_mod_pol_notification_hook_pre_fetch(E_Client *ec);
EINTERN void      e_mod_pol_notification_hook_pre_post_fetch(E_Client *ec);

EINTERN Eina_Bool e_mod_pol_notification_level_update(E_Client *ec);
EINTERN Eina_Bool e_mod_pol_notification_level_apply(E_Client *ec, int level);

#endif
