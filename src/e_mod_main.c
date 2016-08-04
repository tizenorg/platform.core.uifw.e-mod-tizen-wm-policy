#include "e_mod_main.h"
#include "e_mod_rotation.h"

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Policy-Mobile" };

Mod *_pol_mod = NULL;

E_API void *
e_modapi_init(E_Module *m)
{
   Mod *mod;

   mod = E_NEW(Mod, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(mod, NULL);

   mod->module = m;
   _pol_mod = mod;

   if (!e_config->use_e_policy)
     ERR("No policy system!");

   e_mod_pol_conf_init(mod);
   e_mod_pol_rotation_init();

   return mod;
}

E_API int
e_modapi_shutdown(E_Module *m)
{
   Mod *mod = m->data;

   e_mod_pol_rotation_shutdown();
   e_mod_pol_conf_shutdown(mod);
   E_FREE(mod);
   _pol_mod = NULL;
   return 1;
}

E_API int
e_modapi_save(E_Module *m)
{
   Mod *mod = m->data;
   e_config_domain_save("module.policy-tizen",
                        mod->conf_edd,
                        mod->conf);
   return 1;
}
