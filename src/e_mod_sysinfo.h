#ifndef E_MOD_SYSINFO_H
#define E_MOD_SYSINFO_H

#include "config.h"
#ifdef HAVE_WAYLAND_ONLY

#define E_COMP_WL
#include <e.h>

Eina_Bool e_mod_pol_sysinfo_init(void);
void      e_mod_pol_sysinfo_shutdown(void);
void      e_mod_pol_sysinfo_client_add(E_Client *ec);
void      e_mod_pol_sysinfo_client_del(E_Client *ec);

#endif /* HAVE_WAYLAND_ONLY */
#endif /* E_MOD_SYSINFO_H */
