#ifndef E_MOD_MAIN_H
#define E_MOD_MAIN_H
#include <e.h>

#ifndef _
#   ifdef HAVE_GETTEXT
#define _(str) gettext(str)
#   else
#define _(str) (str)
#   endif
#endif

typedef struct _Pol_Desk     Pol_Desk;
typedef struct _Pol_Client   Pol_Client;
typedef struct _Pol_Softkey  Pol_Softkey;
typedef struct _Config_Match Config_Match;
typedef struct _Config_Desk  Config_Desk;
typedef struct _Config_Rot   Config_Rot;
typedef struct _Config       Config;
typedef struct _Mod          Mod;
typedef struct _Pol_System_Info Pol_System_Info;

struct _Pol_Desk
{
   E_Desk          *desk;
   E_Zone          *zone;
};

struct _Pol_Client
{
   E_Client        *ec;
   struct
   {
      E_Maximize    maximized;
      unsigned int  fullscreen : 1;
      unsigned char borderless : 1;
      unsigned int  lock_user_location : 1;
      unsigned int  lock_client_location : 1;
      unsigned int  lock_user_size : 1;
      unsigned int  lock_client_size : 1;
      unsigned int  lock_client_stacking : 1;
      unsigned int  lock_user_shade : 1;
      unsigned int  lock_client_shade : 1;
      unsigned int  lock_user_maximize : 1;
      unsigned int  lock_client_maximize : 1;
      unsigned int  lock_user_fullscreen : 1;
      unsigned int  lock_client_fullscreen : 1;
   } orig;

   struct
   {
      unsigned int vkbd_state;
      unsigned int already_hide;
   } changes;

   Eina_Bool max_policy_state;
   Eina_Bool flt_policy_state;
   Eina_Bool allow_user_geom;
   int       user_geom_ref;
};

struct _Pol_Softkey
{
   EINA_INLIST;

   E_Zone          *zone;
   Evas_Object     *home;
   Evas_Object     *back;
};

struct _Config_Match
{
   const char      *title; /* icccm.title or netwm.name */
   const char      *clas;  /* icccm.class */
   unsigned int     type;  /* netwm.type  */
};

struct _Config_Desk
{
   unsigned int     zone_num;
   int              x, y;
   int              enable;
};

struct _Config_Rot
{
   unsigned char    enable;
   int              angle;
};

struct _Config
{
   Config_Match     launcher;
   Eina_List       *desks;
   Eina_List       *rotations;
   int              use_softkey;
   int              softkey_size;
};

struct _Mod
{
   E_Module        *module;
   E_Config_DD     *conf_edd;
   E_Config_DD     *conf_desk_edd;
   E_Config_DD     *conf_rot_edd;
   Config          *conf;
   Eina_List       *launchers; /* launcher window per zone */
   Eina_Inlist     *softkeys; /* softkey ui object per zone */
};

struct _E_Config_Dialog_Data
{
   Config          *conf;
   Evas_Object     *o_list;
   Evas_Object     *o_desks;
};

struct _Pol_System_Info
{
   struct
   {
      E_Client  *ec;
      Eina_Bool  show;
   } lockscreen;

   struct
   {
      int system;
      int client;
      Eina_Bool use_client;
   } brightness;
};

extern Mod *_pol_mod;
extern Eina_Hash *hash_pol_desks;
extern Eina_Hash *hash_pol_clients;
extern Pol_System_Info g_system_info;


EINTERN void             e_mod_pol_conf_init(Mod *mod);
EINTERN void             e_mod_pol_conf_shutdown(Mod *mod);
EINTERN Config_Desk     *e_mod_pol_conf_desk_get_by_nums(Config *conf, unsigned int zone_num, int x, int y);
//EINTERN E_Config_Dialog *e_int_config_pol_mobile(Evas_Object *o EINA_UNUSED, const char *params EINA_UNUSED);
EINTERN Pol_Client      *e_mod_pol_client_get(E_Client *ec);
EINTERN void             e_mod_pol_allow_user_geometry_set(E_Client *ec, Eina_Bool set);
EINTERN void             e_mod_pol_desk_add(E_Desk *desk);
EINTERN void             e_mod_pol_desk_del(Pol_Desk *pd);
EINTERN Pol_Client      *e_mod_pol_client_launcher_get(E_Zone *zone);
EINTERN Eina_Bool        e_mod_pol_client_is_lockscreen(E_Client *ec);
EINTERN Eina_Bool        e_mod_pol_client_is_home_screen(E_Client *ec);
EINTERN Eina_Bool        e_mod_pol_client_is_quickpanel(E_Client *ec);
EINTERN Eina_Bool        e_mod_pol_client_is_conformant(E_Client *ec);
EINTERN Eina_Bool        e_mod_pol_client_is_volume(E_Client *ec);
EINTERN Eina_Bool        e_mod_pol_client_is_volume_tv(E_Client *ec);
EINTERN Eina_Bool        e_mod_pol_client_is_noti(E_Client *ec);
EINTERN Eina_Bool        e_mod_pol_client_is_floating(E_Client *ec);
#ifdef HAVE_WAYLAND_ONLY
EINTERN Eina_Bool        e_mod_pol_client_is_subsurface(E_Client *ec);
#endif

EINTERN Pol_Softkey     *e_mod_pol_softkey_add(E_Zone *zone);
EINTERN void             e_mod_pol_softkey_del(Pol_Softkey *softkey);
EINTERN void             e_mod_pol_softkey_show(Pol_Softkey *softkey);
EINTERN void             e_mod_pol_softkey_hide(Pol_Softkey *softkey);
EINTERN void             e_mod_pol_softkey_update(Pol_Softkey *softkey);
EINTERN Pol_Softkey     *e_mod_pol_softkey_get(E_Zone *zone);

EINTERN void             e_mod_pol_client_visibility_send(E_Client *ec);
EINTERN void             e_mod_pol_client_iconify_by_visibility(E_Client *ec);
EINTERN void             e_mod_pol_client_uniconify_by_visibility(E_Client *ec);

EINTERN Eina_Bool        e_mod_pol_client_maximize(E_Client *ec);

EINTERN void             e_mod_pol_client_window_opaque_set(E_Client *ec);

EINTERN void             e_mod_pol_stack_init(void);
EINTERN void             e_mod_pol_stack_shutdonw(void);
EINTERN void             e_mod_pol_stack_transient_for_set(E_Client *child, E_Client *parent);
EINTERN void             e_mod_pol_stack_cb_client_remove(E_Client *ec);
EINTERN void             e_mod_pol_stack_hook_pre_fetch(E_Client *ec);
EINTERN void             e_mod_pol_stack_hook_pre_post_fetch(E_Client *ec);

EINTERN void             e_mod_pol_stack_below(E_Client *ec, E_Client *below_ec);

EINTERN void             e_mod_pol_stack_clients_restack_above_lockscreen(E_Client *ec_lock, Eina_Bool show);
EINTERN Eina_Bool        e_mod_pol_stack_check_above_lockscreen(E_Client *ec, E_Layer layer, E_Layer *new_layer, Eina_Bool set_layer);

EINTERN Eina_Bool        e_mod_pol_conf_rot_enable_get(int angle);
#endif
