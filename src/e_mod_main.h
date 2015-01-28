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
typedef struct _Config       Config;
typedef struct _Mod          Mod;
typedef struct _Pol_Client_Type_Match  Pol_Client_Type_Match;

typedef enum _Pol_Client_Type
{
   POL_CLIENT_TYPE_UNKNOWN = 0,
   POL_CLIENT_TYPE_DESKTOP,
   POL_CLIENT_TYPE_DOCK,
   POL_CLIENT_TYPE_TOOLBAR,
   POL_CLIENT_TYPE_MENU,
   POL_CLIENT_TYPE_UTILITY,
   POL_CLIENT_TYPE_SPLASH,
   POL_CLIENT_TYPE_DIALOG,
   POL_CLIENT_TYPE_NORMAL,
   POL_CLIENT_TYPE_DROPDOWN_MENU,
   POL_CLIENT_TYPE_POPUP_MENU,
   POL_CLIENT_TYPE_TOOLTIP,
   POL_CLIENT_TYPE_NOTIFICATION,
   POL_CLIENT_TYPE_COMBO,
   POL_CLIENT_TYPE_DND,
   /* added type */
   POL_CLIENT_TYPE_MENUSCREEN,
   POL_CLIENT_TYPE_QUICKPANEL,
   POL_CLIENT_TYPE_VIDEOCALL,
   POL_CLIENT_TYPE_TASKMANAGER,
   POL_CLIENT_TYPE_LIVEMAGAZINE,
   POL_CLIENT_TYPE_LOCKSCREEN,
   POL_CLIENT_TYPE_INDICATOR,
   POL_CLIENT_TYPE_TICKERNOTI,
   POL_CLIENT_TYPE_APPTRAY,
   POL_CLIENT_TYPE_VOLUME,
   POL_CLIENT_TYPE_BACKGROUND,
   POL_CLIENT_TYPE_SETUP_WIZARD,
   POL_CLIENT_TYPE_ISF_KEYBOARD,
   POL_CLIENT_TYPE_ISF_SUB,
   POL_CLIENT_TYPE_APP_POPUP,
} Pol_Client_Type;

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
   } orig;
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
   unsigned int     comp_num;
   unsigned int     zone_num;
   int              x, y;
   int              enable;
};

struct _Config
{
   Config_Match     launcher;
   Eina_List       *desks;
   Eina_List       *client_types;
   int              use_softkey;
   int              softkey_size;
};

struct _Mod
{
   E_Module        *module;
   E_Config_DD     *conf_edd;
   E_Config_DD     *conf_desk_edd;
   E_Config_DD     *conf_client_type_edd;
   Config          *conf;
   E_Config_Dialog *conf_dialog;
   Eina_List       *launchers; /* launcher window per zone */
   Eina_Inlist     *softkeys; /* softkey ui object per zone */
};

struct _E_Config_Dialog_Data
{
   Config          *conf;
   Evas_Object     *o_list;
   Evas_Object     *o_desks;
};

struct _Pol_Client_Type_Match
{
   const char     *name; /* icccm.class_name */
   const char     *clas; /* icccm.class */
   E_Window_Type   e_window_type; /* netwm.type: Ecore_X_Window_Type, E_Window_Type */
   Pol_Client_Type pol_client_type; /* E_Client_Type */
};

extern Mod *_pol_mod;
extern Eina_Hash *hash_pol_desks;
extern Eina_Hash *hash_pol_clients;

EINTERN void             e_mod_pol_conf_init(Mod *mod);
EINTERN void             e_mod_pol_conf_shutdown(Mod *mod);
EINTERN Config_Desk     *e_mod_pol_conf_desk_get_by_nums(Config *conf, unsigned int comp_num, unsigned int zone_num, int x, int y);
EINTERN E_Config_Dialog *e_int_config_pol_mobile(E_Comp *c EINA_UNUSED, const char *params EINA_UNUSED);
EINTERN void             e_mod_pol_desk_add(E_Desk *desk);
EINTERN void             e_mod_pol_desk_del(Pol_Desk *pd);
EINTERN Pol_Client      *e_mod_pol_client_launcher_get(E_Zone *zone);

EINTERN Pol_Softkey     *e_mod_pol_softkey_add(E_Zone *zone);
EINTERN void             e_mod_pol_softkey_del(Pol_Softkey *softkey);
EINTERN void             e_mod_pol_softkey_show(Pol_Softkey *softkey);
EINTERN void             e_mod_pol_softkey_hide(Pol_Softkey *softkey);
EINTERN void             e_mod_pol_softkey_update(Pol_Softkey *softkey);
EINTERN Pol_Softkey     *e_mod_pol_softkey_get(E_Zone *zone);

EINTERN void             e_mod_pol_visibility_init(void);
EINTERN void             e_mod_pol_visibility_shutdown(void);
EINTERN void             e_mod_pol_zone_visibility_calc(E_Zone *zone);
EINTERN void             e_mod_pol_visibility_calc(void);
EINTERN void             e_mod_pol_client_visibility_del(E_Client *ec);
EINTERN void             e_mod_pol_client_window_opaque_set(E_Client *ec);
EINTERN Eina_Bool        e_mod_pol_visibility_cb_window_property(Ecore_X_Event_Window_Property *ev);

EINTERN void             e_mod_pol_client_type_setup(E_Client *ec);
EINTERN void             e_mod_pol_client_type_init(void);
EINTERN void             e_mod_pol_client_type_shutdown(void);
EINTERN void             e_mod_pol_client_type_del(E_Client *ec);

#endif
