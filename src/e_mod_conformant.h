#ifndef _E_MOD_CONFORMANT_H_
#define _E_MOD_CONFORMANT_H_

EINTERN Eina_Bool  e_mod_conformant_init(void);
EINTERN void       e_mod_conformant_shutdown(void);
EINTERN void       e_mod_conformant_client_add(E_Client *ec, struct wl_resource *res);
EINTERN void       e_mod_conformant_client_del(E_Client *ec);
EINTERN Eina_Bool  e_mod_conformant_client_check(E_Client *ec);

#endif
