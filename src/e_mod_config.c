#include "e_mod_main.h"

void
e_mod_pol_conf_init(Mod *mod)
{
   mod->conf_rot_edd = E_CONFIG_DD_NEW("Policy_Mobile_Config_Rot", Config_Rot);
#undef T
#undef D
#define T Config_Rot
#define D mod->conf_rot_edd
   E_CONFIG_VAL(D, T, enable, UCHAR);
   E_CONFIG_VAL(D, T, angle, INT);

   mod->conf_edd = E_CONFIG_DD_NEW("Policy_Mobile_Config", Config);
#undef T
#undef D
#define T Config
#define D mod->conf_edd
   E_CONFIG_LIST(D, T, rotations, mod->conf_rot_edd);
#undef T
#undef D

   mod->conf = e_config_domain_load("module.policy-tizen", mod->conf_edd);
}

void
e_mod_pol_conf_shutdown(Mod *mod)
{
   if (mod->conf)
     free(mod->conf);

   E_CONFIG_DD_FREE(mod->conf_rot_edd);
   E_CONFIG_DD_FREE(mod->conf_edd);
}

EINTERN Eina_Bool
e_mod_pol_conf_rot_enable_get(int angle)
{
   Config_Rot *rot;
   Eina_List *rots, *l;

   if (EINA_UNLIKELY(!_pol_mod))
     return EINA_FALSE;

   if (EINA_UNLIKELY(!_pol_mod->conf))
     return EINA_FALSE;

   rots = _pol_mod->conf->rotations;
   if (!rots)
     return EINA_FALSE;

   EINA_LIST_FOREACH(rots, l, rot)
     {
        if (rot->angle == angle)
          return !!rot->enable;
     }

   return EINA_FALSE;
}
