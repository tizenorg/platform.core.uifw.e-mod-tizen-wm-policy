#ifndef E_MOD_WL_DISPLAY_CONTROL_H
#define E_MOD_WL_H

#define E_COMP_WL
#include <e.h>

typedef enum _E_Display_Screen_Mode
{
   E_DISPLAY_SCREEN_MODE_DEFAULT = 0,
   E_DISPLAY_SCREEN_MODE_ALWAYS_ON = 1,
} E_Display_Screen_Mode;

Eina_Bool e_mod_pol_display_init(void);
void      e_mod_pol_display_shutdown(void);

void      e_mod_pol_display_screen_mode_set(E_Client *ec, int mode);
void      e_mod_pol_display_screen_mode_apply(void);

#endif
