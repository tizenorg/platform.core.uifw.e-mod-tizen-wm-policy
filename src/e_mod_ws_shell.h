#ifndef E_MOD_WS_SHELL_H
#define E_MOD_WS_SHELL_H

#include "config.h"
#ifdef HAVE_WAYLAND_ONLY

#define E_COMP_WL
#include <e.h>

typedef enum _E_Tizen_Ws_Shell_Service_Role
{
   E_TIZEN_WS_SHELL_SERVICE_ROLE_UNKNOWN = -1,
   E_TIZEN_WS_SHELL_SERVICE_ROLE_CALL,
   E_TIZEN_WS_SHELL_SERVICE_ROLE_VOLUME,
   E_TIZEN_WS_SHELL_SERVICE_ROLE_QUICKPANEL,
   E_TIZEN_WS_SHELL_SERVICE_ROLE_LOCKSCREEN,
   E_TIZEN_WS_SHELL_SERVICE_ROLE_INDICATOR,
   E_TIZEN_WS_SHELL_SERVICE_ROLE_MAX,
} E_Tizen_Ws_Shell_Service_Role;

Eina_Bool e_mod_ws_shell_init(void);
void e_mod_ws_shell_shutdown(void);

#endif /* HAVE_WAYLAND_ONLY */
#endif /* E_MOD_WS_SHELL_H */
