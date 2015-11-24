#ifndef E_MOD_QUICKPANEL_H
#define E_MOD_QUICKPANEL_H

#define E_COMP_WL
#include <e.h>

EINTERN void          e_mod_quickpanel_client_set(E_Client *ec);
EINTERN E_Client     *e_mod_quickpanel_client_get(void);
EINTERN void          e_mod_quickpanel_show(void);
EINTERN void          e_mod_quickpanel_hide(void);
EINTERN Eina_Bool     e_mod_quickpanel_handler_region_set(int x, int y, int w, int h);
EINTERN Evas_Object  *e_mod_quickpanel_handler_object_add(int x, int y, int w, int h);
EINTERN void          e_mod_quickpanel_handler_object_del(Evas_Object *handler);

#endif
