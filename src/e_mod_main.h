#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H
#include <e.h>

typedef struct _Config_Rot   Config_Rot;
typedef struct _Config       Config;
typedef struct _Mod          Mod;

struct _Config_Rot
{
   unsigned char    enable;
   int              angle;
};

struct _Config
{
   Eina_List       *rotations;
};

struct _Mod
{
   E_Module        *module;
   E_Config_DD     *conf_edd;
   E_Config_DD     *conf_rot_edd;
   Config          *conf;
};

#undef E_CLIENT_HOOK_APPEND
#define E_CLIENT_HOOK_APPEND(l, t, cb, d) \
  do                                      \
    {                                     \
       E_Client_Hook *_h;                 \
       _h = e_client_hook_add(t, cb, d);  \
       assert(_h);                        \
       l = eina_list_append(l, _h);       \
    }                                     \
  while (0)

#undef E_COMP_OBJECT_INTERCEPT_HOOK_APPEND
#define E_COMP_OBJECT_INTERCEPT_HOOK_APPEND(l, t, cb, d) \
  do                                                     \
    {                                                    \
       E_Comp_Object_Intercept_Hook *_h;                 \
       _h = e_comp_object_intercept_hook_add(t, cb, d);  \
       assert(_h);                                       \
       l = eina_list_append(l, _h);                      \
    }                                                    \
  while (0)

extern Mod *_pol_mod;

EINTERN void             e_mod_pol_conf_init(Mod *mod);
EINTERN void             e_mod_pol_conf_shutdown(Mod *mod);
EINTERN Eina_Bool        e_mod_pol_conf_rot_enable_get(int angle);
#endif
