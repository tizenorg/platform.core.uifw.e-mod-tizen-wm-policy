#ifndef E_MOD_WL_H
#define E_MOD_WL_H

#include "config.h"
#ifdef HAVE_WAYLAND_ONLY

#define E_COMP_WL
#include <e.h>

Eina_Bool e_mod_pol_wl_init(void);
void      e_mod_pol_wl_shutdown(void);
void      e_mod_pol_wl_client_add(E_Client *ec);
void      e_mod_pol_wl_client_del(E_Client *ec);
void      e_mod_pol_wl_pixmap_del(E_Pixmap *cp);

/* visibility */
void      e_mod_pol_wl_visibility_send(E_Client *ec, int vis);

/* iconify */
void      e_mod_pol_wl_iconify_state_change_send(E_Client *ec, int iconic);

/* position */
void      e_mod_pol_wl_position_send(E_Client *ec);

/* notification */
void      e_mod_pol_wl_notification_level_fetch(E_Client *ec);
void      e_mod_pol_wl_keyboard_geom_broadcast(E_Client *ec);

/* window screenmode */
void      e_mod_pol_wl_win_scrmode_apply(void);

/* aux_hint */
void      e_mod_pol_wl_aux_hint_init(void);
void      e_mod_pol_wl_eval_pre_new_client(E_Client *ec);

#endif /* HAVE_WAYLAND_ONLY */
#endif /* E_MOD_WL_H */
