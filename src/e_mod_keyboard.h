#ifndef E_MOD_KEYBOARD_H
#define E_MOD_KEYBOARD_H
#include <e.h>

EINTERN Eina_Bool e_mod_pol_client_is_keyboard(E_Client *ec);
EINTERN Eina_Bool e_mod_pol_client_is_keyboard_sub(E_Client *ec);
EINTERN void      e_mod_pol_keyboard_layout_apply(E_Client *ec);
#ifndef HAVE_WAYLAND_ONLY
EINTERN void      e_mod_pol_keyboard_configure(Ecore_X_Event_Window_Configure_Request *ev);
#endif

#endif
