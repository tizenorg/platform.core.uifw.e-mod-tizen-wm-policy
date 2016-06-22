#ifndef E_MOD_QUICKPANEL_H
#define E_MOD_QUICKPANEL_H

EINTERN void          e_mod_quickpanel_client_set(E_Client *ec);
EINTERN E_Client     *e_mod_quickpanel_client_get(void);
EINTERN void          e_mod_quickpanel_show(void);
EINTERN void          e_mod_quickpanel_hide(void);
EINTERN Eina_Bool     e_mod_quickpanel_region_set(int type, int angle, Eina_Tiler *tiler);
EINTERN Evas_Object  *e_mod_quickpanel_handler_object_add(E_Client *ec, int x, int y, int w, int h);
EINTERN void          e_mod_quickpanel_handler_object_del(Evas_Object *handler);

EINTERN Eina_Bool    e_qp_visible_get(void);
EINTERN int          e_qp_orientation_get(void);

EINTERN void         e_qp_client_add(E_Client *ec);
EINTERN void         e_qp_client_del(E_Client *ec);
EINTERN void         e_qp_client_show(E_Client *ec);
EINTERN void         e_qp_client_hide(E_Client *ec);
EINTERN Eina_Bool    e_qp_client_scrollable_set(E_Client *ec, Eina_Bool set);
EINTERN Eina_Bool    e_qp_client_scrollable_get(E_Client *ec);

#endif
