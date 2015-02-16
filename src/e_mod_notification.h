#ifndef E_MOD_POL_NOTIFICATION_H
#define E_MOD_POL_NOTIFICATION_H

#include <e.h>

/* wm-policy notification level */
enum
{
   E_POL_NOTIFICATION_LEVEL_LOW = 50,
   E_POL_NOTIFICATION_LEVEL_NORMAL = 100,
   E_POL_NOTIFICATION_LEVEL_HIGH = 150,
};

/* wm-policy layer level */
#define E_LAYER_NOTIFICATION_DEFAULT   E_LAYER_CLIENT_DESKTOP
#define E_LAYER_NOTIFICATION_LOW       1000
#define E_LAYER_NOTIFICATION_NORMAL    2000
#define E_LAYER_NOTIFICATION_HIGH      3000


EINTERN void      e_mod_pol_notification_init(void);
EINTERN void      e_mod_pol_notification_shutdown(void);
EINTERN void      e_mod_pol_notification_client_del(E_Client *ec);

EINTERN Eina_Bool e_mod_pol_notification_level_update(E_Client *ec);

#endif
