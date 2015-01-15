#ifndef E_MOD_ROTATION_H
#define E_MOD_ROTATION_H
#include <e.h>

EINTERN Eina_Bool e_mod_pol_rot_hook_client_free(E_Client *ec);
EINTERN Eina_Bool e_mod_pol_rot_hook_client_del(E_Client *ec);
EINTERN void e_mod_pol_rot_cb_evas_show(E_Client *ec);
EINTERN Eina_Bool e_mod_pol_rot_hook_eval_end(E_Client *ec);
EINTERN Eina_Bool e_mod_pol_rot_cb_idle_enterer(void);
EINTERN Eina_Bool e_mod_pol_rot_hook_new_client(E_Client *ec);
EINTERN Eina_Bool e_mod_pol_rot_cb_zone_rotation_change_begin(E_Event_Zone_Rotation_Change_Begin *ev);
EINTERN Eina_Bool e_mod_pol_rot_intercept_hook_show_helper(E_Client *ec);
EINTERN Eina_Bool e_mod_pol_rot_intercept_hook_hide(E_Client *ec);
EINTERN Eina_Bool e_mod_pol_rot_cb_window_configure(Ecore_X_Event_Window_Configure *ev);
EINTERN Eina_Bool e_mod_pol_rot_cb_window_property(Ecore_X_Event_Window_Property *ev);
EINTERN Eina_Bool e_mod_pol_rot_cb_window_message(Ecore_X_Event_Client_Message *ev);
EINTERN Eina_Bool e_mod_pol_rot_hook_eval_fetch(E_Client *ec);

#endif
