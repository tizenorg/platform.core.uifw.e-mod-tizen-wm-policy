#include "e_mod_wl.h"
#include "e_mod_main.h"
#include "e_mod_quickpanel.h"
#include "e_mod_indicator.h"
#include "e_mod_volume.h"

#include <wayland-server.h>
#include <tizen-extension-server-protocol.h>
#include <tzsh_server.h>

#ifdef ENABLE_CYNARA
# include <cynara-session.h>
# include <cynara-client.h>
# include <cynara-creds-socket.h>
#endif

#define APP_DEFINE_GROUP_NAME "effect"

typedef enum _Tzsh_Srv_Role
{
   TZSH_SRV_ROLE_UNKNOWN = -1,
   TZSH_SRV_ROLE_CALL,
   TZSH_SRV_ROLE_VOLUME,
   TZSH_SRV_ROLE_QUICKPANEL,
   TZSH_SRV_ROLE_LOCKSCREEN,
   TZSH_SRV_ROLE_INDICATOR,
   TZSH_SRV_ROLE_TVSERVICE,
   TZSH_SRV_ROLE_MAX
} Tzsh_Srv_Role;

typedef enum _Tzsh_Type
{
   TZSH_TYPE_UNKNOWN = 0,
   TZSH_TYPE_SRV,
   TZSH_TYPE_CLIENT
} Tzsh_Type;

typedef struct _Pol_Wl_Tzpol
{
   struct wl_resource *res_tzpol; /* tizen_policy_interface */
   Eina_List          *psurfs;    /* list of Pol_Wl_Surface */
} Pol_Wl_Tzpol;

typedef struct _Pol_Wl_Tzsh
{
   struct wl_resource *res_tzsh; /* tizen_ws_shell_interface */
   Tzsh_Type           type;
   E_Pixmap           *cp;
   E_Client           *ec;
} Pol_Wl_Tzsh;

typedef struct _Pol_Wl_Tzsh_Srv
{
   Pol_Wl_Tzsh        *tzsh;
   struct wl_resource *res_tzsh_srv;
   Tzsh_Srv_Role       role;
   const char         *name;
} Pol_Wl_Tzsh_Srv;

typedef struct _Pol_Wl_Tzsh_Client
{
   Pol_Wl_Tzsh        *tzsh;
   struct wl_resource *res_tzsh_client;
} Pol_Wl_Tzsh_Client;

typedef struct _Pol_Wl_Tzsh_Region
{
   Pol_Wl_Tzsh        *tzsh;
   struct wl_resource *res_tzsh_reg;
   Eina_Tiler         *tiler;
   struct wl_listener  destroy_listener;
} Pol_Wl_Tzsh_Region;

typedef struct _Pol_Wl_Surface
{
   struct wl_resource *surf;
   Pol_Wl_Tzpol       *tzpol;
   E_Pixmap           *cp;
   E_Client           *ec;
   Eina_Bool           pending_notilv;
   int32_t             notilv;
   Eina_List          *vislist; /* list of tizen_visibility_interface resources */
   Eina_List          *poslist; /* list of tizen_position_inteface resources */
} Pol_Wl_Surface;

typedef struct _Pol_Wl_Tzlaunch
{
   struct wl_resource *res_tzlaunch;     /* tizen_launchscreen */
   Eina_List          *imglist;          /* list of Pol_Wl_Tzlaunch_Img */
} Pol_Wl_Tzlaunch;

typedef struct _Pol_Wl_Tzlaunch_Img
{
   struct wl_resource  *res_tzlaunch_img; /* tizen_launch_image */
   Pol_Wl_Tzlaunch     *tzlaunch;         /* launcher */

   const char          *path;             /* image resource path */
   uint32_t            type;              /* 0: image, 1: edc */
   uint32_t            indicator;         /* 0: off, 1: on */
   uint32_t            angle;             /* 0, 90, 180, 270 : rotation angle */
   uint32_t            pid;

   Evas_Object         *obj;              /* launch screen image */
   Eina_Bool           valid;
} Pol_Wl_Tzlaunch_Img;

typedef enum _E_Launch_Img_File_type
{
   E_LAUNCH_FILE_TYPE_ERROR = -1,
   E_LAUNCH_FILE_TYPE_IMAGE = 0,
   E_LAUNCH_FILE_TYPE_EDJ
} E_Launch_Img_File_type;

typedef struct _Pol_Wl
{
   Eina_List       *globals;                 /* list of wl_global */
   Eina_Hash       *tzpols;                  /* list of Pol_Wl_Tzpol */

   /* tizen_ws_shell_interface */
   Eina_List       *tzshs;                   /* list of Pol_Wl_Tzsh */
   Eina_List       *tzsh_srvs;               /* list of Pol_Wl_Tzsh_Srv */
   Eina_List       *tzsh_clients;            /* list of Pol_Wl_Tzsh_Client */
   Pol_Wl_Tzsh_Srv *srvs[TZSH_SRV_ROLE_MAX]; /* list of registered Pol_Wl_Tzsh_Srv */
   Eina_List       *tvsrv_bind_list;         /* list of activated Pol_Wl_Tzsh_Client */

   /* tizen_launchscreen_interface */
   Eina_List       *tzlaunchs;                   /* list of Pol_Wl_Tzlaunch */
#ifdef ENABLE_CYNARA
   cynara          *p_cynara;
#endif
} Pol_Wl;

static Pol_Wl *polwl = NULL;

E_Launch_Screen   *launch_scrn=NULL;

enum _WM_Policy_Hint_Type
{
   WM_POLICY_HINT_USER_GEOMETRY = 0,
   WM_POLICY_HINT_FIXED_RESIZE = 1,
   WM_POLICY_HINT_DEICONIFY_APPROVE_DISABLE = 2,
   WM_POLICY_HINT_ICONIFY = 3,
};

static const char *hint_names[] =
{
   "wm.policy.win.user.geometry",
   "wm.policy.win.fixed.resize",
   "wm.policy.win.deiconify.approve.disable",
   "wm.policy.win.iconify",
};

static void                _pol_wl_surf_del(Pol_Wl_Surface *psurf);
static void                _pol_wl_tzsh_srv_register_handle(Pol_Wl_Tzsh_Srv *tzsh_srv);
static void                _pol_wl_tzsh_srv_unregister_handle(Pol_Wl_Tzsh_Srv *tzsh_srv);
static void                _pol_wl_tzsh_srv_state_broadcast(Pol_Wl_Tzsh_Srv *tzsh_srv, Eina_Bool reg);
static void                _pol_wl_tzsh_srv_tvsrv_bind_update(void);
static Eina_Bool           _pol_wl_e_client_is_valid(E_Client *ec);
static Pol_Wl_Tzsh_Srv    *_pol_wl_tzsh_srv_add(Pol_Wl_Tzsh *tzsh, Tzsh_Srv_Role role, struct wl_resource *res_tzsh_srv, const char *name);
static void                _pol_wl_tzsh_srv_del(Pol_Wl_Tzsh_Srv *tzsh_srv);
static Pol_Wl_Tzsh_Client *_pol_wl_tzsh_client_add(Pol_Wl_Tzsh *tzsh, struct wl_resource *res_tzsh_client);
static void                _pol_wl_tzsh_client_del(Pol_Wl_Tzsh_Client *tzsh_client);
static void                _launchscreen_hide(uint32_t pid);
static void                _launch_img_off(Pol_Wl_Tzlaunch_Img *tzlaunchimg);

// --------------------------------------------------------
// Pol_Wl_Tzpol
// --------------------------------------------------------
static Pol_Wl_Tzpol *
_pol_wl_tzpol_add(struct wl_resource *res_tzpol)
{
   Pol_Wl_Tzpol *tzpol;

   tzpol = E_NEW(Pol_Wl_Tzpol, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzpol, NULL);

   eina_hash_add(polwl->tzpols, &res_tzpol, tzpol);

   tzpol->res_tzpol = res_tzpol;

   return tzpol;
}

static void
_pol_wl_tzpol_del(void *data)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;

   tzpol = (Pol_Wl_Tzpol *)data;

   EINA_LIST_FREE(tzpol->psurfs, psurf)
     {
        _pol_wl_surf_del(psurf);
     }

   memset(tzpol, 0x0, sizeof(Pol_Wl_Tzpol));
   E_FREE(tzpol);
}

static Pol_Wl_Tzpol *
_pol_wl_tzpol_get(struct wl_resource *res_tzpol)
{
   return (Pol_Wl_Tzpol *)eina_hash_find(polwl->tzpols, &res_tzpol);
}

static Pol_Wl_Surface *
_pol_wl_tzpol_surf_find(Pol_Wl_Tzpol *tzpol, E_Client *ec)
{
   Eina_List *l;
   Pol_Wl_Surface *psurf;

   EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
     {
        if (psurf->ec == ec)
          return psurf;
     }

   return NULL;
}

static Eina_Bool
_pol_wl_surf_is_valid(Pol_Wl_Surface *psurf)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf2;
   Eina_Iterator *it;
   Eina_List *l;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf2)
       {
          if (psurf2 == psurf)
            {
               eina_iterator_free(it);
               return EINA_TRUE;
            }
       }
   eina_iterator_free(it);

   return EINA_FALSE;
}

// --------------------------------------------------------
// Pol_Wl_Tzsh
// --------------------------------------------------------
static Pol_Wl_Tzsh *
_pol_wl_tzsh_add(struct wl_resource *res_tzsh)
{
   Pol_Wl_Tzsh *tzsh;

   tzsh = E_NEW(Pol_Wl_Tzsh, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzsh, NULL);

   tzsh->res_tzsh = res_tzsh;
   tzsh->type = TZSH_TYPE_UNKNOWN;

   polwl->tzshs = eina_list_append(polwl->tzshs, tzsh);

   return tzsh;
}

static void
_pol_wl_tzsh_del(Pol_Wl_Tzsh *tzsh)
{
   Pol_Wl_Tzsh_Srv *tzsh_srv;
   Pol_Wl_Tzsh_Client *tzsh_client;
   Eina_List *l, *ll;

   polwl->tzshs = eina_list_remove(polwl->tzshs, tzsh);

   if (tzsh->type == TZSH_TYPE_SRV)
     {
        EINA_LIST_FOREACH_SAFE(polwl->tzsh_srvs, l, ll, tzsh_srv)
          {
             if (tzsh_srv->tzsh != tzsh) continue;
             _pol_wl_tzsh_srv_del(tzsh_srv);
             break;
          }
     }
   else
     {
        EINA_LIST_FOREACH_SAFE(polwl->tzsh_clients, l, ll, tzsh_client)
          {
             if (tzsh_client->tzsh != tzsh) continue;
             _pol_wl_tzsh_client_del(tzsh_client);
             break;
          }
     }

   memset(tzsh, 0x0, sizeof(Pol_Wl_Tzsh));
   E_FREE(tzsh);
}

static void
_pol_wl_tzsh_data_set(Pol_Wl_Tzsh *tzsh, Tzsh_Type type, E_Pixmap *cp, E_Client *ec)
{
   tzsh->type = type;
   tzsh->cp = cp;
   tzsh->ec = ec;
}

/* notify current registered services to the client */
static void
_pol_wl_tzsh_registered_srv_send(Pol_Wl_Tzsh *tzsh)
{
   int i;

   for (i = 0; i < TZSH_SRV_ROLE_MAX; i++)
     {
        if (!polwl->srvs[i]) continue;

        tizen_ws_shell_send_service_register
          (tzsh->res_tzsh, polwl->srvs[i]->name);
     }
}

static Pol_Wl_Tzsh *
_pol_wl_tzsh_get_from_client(E_Client *ec)
{
   Pol_Wl_Tzsh *tzsh = NULL;
   Eina_List *l;

   EINA_LIST_FOREACH(polwl->tzshs, l, tzsh)
     {
        if (tzsh->cp == ec->pixmap)
          {
             if ((tzsh->ec) &&
                 (tzsh->ec != ec))
               {
                  ELOGF("TZSH",
                        "CRI ERR!!|tzsh_cp:0x%08x|tzsh_ec:0x%08x|tzsh:0x%08x",
                        ec->pixmap, ec,
                        (unsigned int)tzsh->cp,
                        (unsigned int)tzsh->ec,
                        (unsigned int)tzsh);
               }

             return tzsh;
          }
     }

   return NULL;
}

static Pol_Wl_Tzsh_Client *
_pol_wl_tzsh_client_get_from_tzsh(Pol_Wl_Tzsh *tzsh)
{
   Pol_Wl_Tzsh_Client *tzsh_client;
   Eina_List *l;

   EINA_LIST_FOREACH(polwl->tvsrv_bind_list, l, tzsh_client)
     {
        if (tzsh_client->tzsh == tzsh)
          return tzsh_client;
     }

   return NULL;
}

static void
_pol_wl_tzsh_client_set(E_Client *ec)
{
   Pol_Wl_Tzsh *tzsh, *tzsh2;
   Pol_Wl_Tzsh_Srv *tzsh_srv;

   tzsh = _pol_wl_tzsh_get_from_client(ec);
   if (!tzsh) return;

   tzsh->ec = ec;

   if (tzsh->type == TZSH_TYPE_SRV)
     {
        tzsh_srv = polwl->srvs[TZSH_SRV_ROLE_TVSERVICE];
        if (tzsh_srv)
          {
             tzsh2 = tzsh_srv->tzsh;
             if (tzsh2 == tzsh)
               _pol_wl_tzsh_srv_register_handle(tzsh_srv);
          }
     }
   else
     {
        if (_pol_wl_tzsh_client_get_from_tzsh(tzsh))
          _pol_wl_tzsh_srv_tvsrv_bind_update();
     }
}

static void
_pol_wl_tzsh_client_unset(E_Client *ec)
{
   Pol_Wl_Tzsh *tzsh, *tzsh2;
   Pol_Wl_Tzsh_Srv *tzsh_srv;

   tzsh = _pol_wl_tzsh_get_from_client(ec);
   if (!tzsh) return;

   tzsh->ec = NULL;

   if (tzsh->type == TZSH_TYPE_SRV)
     {
        tzsh_srv = polwl->srvs[TZSH_SRV_ROLE_TVSERVICE];
        if (tzsh_srv)
          {
             tzsh2 = tzsh_srv->tzsh;
             if (tzsh2 == tzsh)
               _pol_wl_tzsh_srv_unregister_handle(tzsh_srv);
          }
     }
   else
     {
        if (_pol_wl_tzsh_client_get_from_tzsh(tzsh))
          _pol_wl_tzsh_srv_tvsrv_bind_update();
     }
}

// --------------------------------------------------------
// Pol_Wl_Tzsh_Srv
// --------------------------------------------------------
static Pol_Wl_Tzsh_Srv *
_pol_wl_tzsh_srv_add(Pol_Wl_Tzsh *tzsh, Tzsh_Srv_Role role, struct wl_resource *res_tzsh_srv, const char *name)
{
   Pol_Wl_Tzsh_Srv *tzsh_srv;

   tzsh_srv = E_NEW(Pol_Wl_Tzsh_Srv, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzsh_srv, NULL);

   tzsh_srv->tzsh = tzsh;
   tzsh_srv->res_tzsh_srv = res_tzsh_srv;
   tzsh_srv->role = role;
   tzsh_srv->name = eina_stringshare_add(name);

   polwl->srvs[role] = tzsh_srv;
   polwl->tzsh_srvs = eina_list_append(polwl->tzsh_srvs, tzsh_srv);

   _pol_wl_tzsh_srv_register_handle(tzsh_srv);
   _pol_wl_tzsh_srv_state_broadcast(tzsh_srv, EINA_TRUE);

   return tzsh_srv;
}

static void
_pol_wl_tzsh_srv_del(Pol_Wl_Tzsh_Srv *tzsh_srv)
{
   polwl->tzsh_srvs = eina_list_remove(polwl->tzsh_srvs, tzsh_srv);

   if (polwl->srvs[tzsh_srv->role] == tzsh_srv)
     polwl->srvs[tzsh_srv->role] = NULL;

   _pol_wl_tzsh_srv_state_broadcast(tzsh_srv, EINA_TRUE);
   _pol_wl_tzsh_srv_unregister_handle(tzsh_srv);

   if (tzsh_srv->name)
     eina_stringshare_del(tzsh_srv->name);

   memset(tzsh_srv, 0x0, sizeof(Pol_Wl_Tzsh_Srv));
   E_FREE(tzsh_srv);
}

static int
_pol_wl_tzsh_srv_role_get(const char *name)
{
   Tzsh_Srv_Role role = TZSH_SRV_ROLE_UNKNOWN;

   if      (!e_util_strcmp(name, "call"      )) role = TZSH_SRV_ROLE_CALL;
   else if (!e_util_strcmp(name, "volume"    )) role = TZSH_SRV_ROLE_VOLUME;
   else if (!e_util_strcmp(name, "quickpanel")) role = TZSH_SRV_ROLE_QUICKPANEL;
   else if (!e_util_strcmp(name, "lockscreen")) role = TZSH_SRV_ROLE_LOCKSCREEN;
   else if (!e_util_strcmp(name, "indicator" )) role = TZSH_SRV_ROLE_INDICATOR;
   else if (!e_util_strcmp(name, "tvsrv"     )) role = TZSH_SRV_ROLE_TVSERVICE;

   return role;
}

static E_Client *
_pol_wl_tzsh_srv_parent_client_pick(void)
{
   Pol_Wl_Tzsh *tzsh = NULL;
   Pol_Wl_Tzsh_Client *tzsh_client;
   E_Client *ec = NULL, *ec2;
   Eina_List *l;

   EINA_LIST_REVERSE_FOREACH(polwl->tvsrv_bind_list, l, tzsh_client)
     {
        tzsh = tzsh_client->tzsh;
        if (!tzsh) continue;

        ec2 = tzsh->ec;
        if (!ec2) continue;
        if (!_pol_wl_e_client_is_valid(ec2)) continue;

        ec = ec2;
        break;
     }

   return ec;
}

static void
_pol_wl_tzsh_srv_tvsrv_bind_update(void)
{
   Pol_Wl_Tzsh_Srv *tzsh_srv;
   E_Client *tzsh_client_ec = NULL;
   E_Client *tzsh_srv_ec = NULL;

   tzsh_srv = polwl->srvs[TZSH_SRV_ROLE_TVSERVICE];
   if ((tzsh_srv) && (tzsh_srv->tzsh))
     tzsh_srv_ec = tzsh_srv->tzsh->ec;

   tzsh_client_ec = _pol_wl_tzsh_srv_parent_client_pick();

   if ((tzsh_srv_ec) &&
       (tzsh_srv_ec->parent == tzsh_client_ec))
     return;

   if ((tzsh_client_ec) && (tzsh_srv_ec))
     {
        ELOGF("TZSH",
              "TR_SET   |parent_ec:0x%08x|child_ec:0x%08x",
              NULL, NULL,
              (unsigned int)e_client_util_win_get(tzsh_client_ec),
              (unsigned int)e_client_util_win_get(tzsh_srv_ec));

        e_mod_pol_stack_transient_for_set(tzsh_srv_ec, tzsh_client_ec);
        evas_object_stack_below(tzsh_srv_ec->frame, tzsh_client_ec->frame);
     }
   else
     {
        if (tzsh_srv_ec)
          {
             ELOGF("TZSH",
                   "TR_UNSET |                    |child_ec:0x%08x",
                   NULL, NULL,
                   (unsigned int)e_client_util_win_get(tzsh_srv_ec));

             e_mod_pol_stack_transient_for_set(tzsh_srv_ec, NULL);
          }
     }
}

static void
_pol_wl_tzsh_srv_register_handle(Pol_Wl_Tzsh_Srv *tzsh_srv)
{
   Pol_Wl_Tzsh *tzsh;

   EINA_SAFETY_ON_NULL_RETURN(tzsh_srv);

   tzsh = tzsh_srv->tzsh;
   EINA_SAFETY_ON_NULL_RETURN(tzsh);

   switch (tzsh_srv->role)
     {
      case TZSH_SRV_ROLE_TVSERVICE:
         if (tzsh->ec) tzsh->ec->transient_policy = E_TRANSIENT_BELOW;
         _pol_wl_tzsh_srv_tvsrv_bind_update();
         break;

      default:
         break;
     }
}

static void
_pol_wl_tzsh_srv_unregister_handle(Pol_Wl_Tzsh_Srv *tzsh_srv)
{
   Pol_Wl_Tzsh *tzsh;

   EINA_SAFETY_ON_NULL_RETURN(tzsh_srv);

   tzsh = tzsh_srv->tzsh;
   EINA_SAFETY_ON_NULL_RETURN(tzsh);

   switch (tzsh_srv->role)
     {
      case TZSH_SRV_ROLE_TVSERVICE:
         _pol_wl_tzsh_srv_tvsrv_bind_update();
         break;

      default:
         break;
     }
}

/* broadcast state of registered service to all subscribers */
static void
_pol_wl_tzsh_srv_state_broadcast(Pol_Wl_Tzsh_Srv *tzsh_srv, Eina_Bool reg)
{
   Pol_Wl_Tzsh *tzsh;
   Eina_List *l;

   EINA_LIST_FOREACH(polwl->tzshs, l, tzsh)
     {
        if (tzsh->type == TZSH_TYPE_SRV) continue;

        if (reg)
          tizen_ws_shell_send_service_register
            (tzsh->res_tzsh, tzsh_srv->name);
        else
          tizen_ws_shell_send_service_unregister
            (tzsh->res_tzsh, tzsh_srv->name);
     }
}

// --------------------------------------------------------
// Pol_Wl_Tzsh_Client
// --------------------------------------------------------
static Pol_Wl_Tzsh_Client *
_pol_wl_tzsh_client_add(Pol_Wl_Tzsh *tzsh, struct wl_resource *res_tzsh_client)
{
   Pol_Wl_Tzsh_Client *tzsh_client;

   tzsh_client = E_NEW(Pol_Wl_Tzsh_Client, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzsh_client, NULL);

   tzsh_client->tzsh = tzsh;
   tzsh_client->res_tzsh_client = res_tzsh_client;

   /* TODO: add tzsh_client to list or hash */

   polwl->tzsh_clients = eina_list_append(polwl->tzsh_clients, tzsh_client);

   return tzsh_client;
}

static void
_pol_wl_tzsh_client_del(Pol_Wl_Tzsh_Client *tzsh_client)
{
   if (!tzsh_client) return;

   polwl->tzsh_clients = eina_list_remove(polwl->tzsh_clients, tzsh_client);
   polwl->tvsrv_bind_list = eina_list_remove(polwl->tvsrv_bind_list, tzsh_client);

   memset(tzsh_client, 0x0, sizeof(Pol_Wl_Tzsh_Client));
   E_FREE(tzsh_client);
}

// --------------------------------------------------------
// Pol_Wl_Surface
// --------------------------------------------------------
static Pol_Wl_Surface *
_pol_wl_surf_add(E_Client *ec, struct wl_resource *res_tzpol)
{
   Pol_Wl_Surface *psurf = NULL;

   Pol_Wl_Tzpol *tzpol;

   tzpol = _pol_wl_tzpol_get(res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzpol, NULL);

   psurf = _pol_wl_tzpol_surf_find(tzpol, ec);
   if (psurf) return psurf;

   psurf = E_NEW(Pol_Wl_Surface, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(psurf, NULL);

   psurf->surf = ec->comp_data->surface;
   psurf->tzpol = tzpol;
   psurf->cp = ec->pixmap;
   psurf->ec = ec;

   tzpol->psurfs = eina_list_append(tzpol->psurfs, psurf);

   return psurf;
}

static void
_pol_wl_surf_del(Pol_Wl_Surface *psurf)
{
   eina_list_free(psurf->vislist);
   eina_list_free(psurf->poslist);

   memset(psurf, 0x0, sizeof(Pol_Wl_Surface));
   E_FREE(psurf);
}

static void
_pol_wl_surf_client_set(E_Client *ec)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   Eina_Iterator *it;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     {
        psurf = _pol_wl_tzpol_surf_find(tzpol, ec);
        if (psurf)
          {
             if ((psurf->ec) && (psurf->ec != ec))
               {
                  ELOGF("POLSURF",
                        "CRI ERR!!|s:0x%08x|tzpol:0x%08x|ps:0x%08x|new_ec:0x%08x|new_cp:0x%08x",
                        psurf->cp,
                        psurf->ec,
                        (unsigned int)psurf->surf,
                        (unsigned int)psurf->tzpol,
                        (unsigned int)psurf,
                        (unsigned int)ec,
                        (unsigned int)ec->pixmap);
               }

             psurf->ec = ec;
          }
     }
   eina_iterator_free(it);

   return;
}

static E_Pixmap *
_pol_wl_e_pixmap_get_from_id(struct wl_client *client, uint32_t id)
{
   E_Pixmap *cp;
   E_Client *ec;
   struct wl_resource *res_surf;

   res_surf = wl_client_get_object(client, id);
   if (!res_surf)
     {
        ERR("Could not get surface resource");
        return NULL;
     }

   ec = wl_resource_get_user_data(res_surf);
   if (!ec)
     {
        ERR("Could not get surface's user data");
        return NULL;
     }

   /* check E_Pixmap */
   cp = e_pixmap_find(E_PIXMAP_TYPE_WL, (uintptr_t)res_surf);
   if (cp != ec->pixmap)
     {
        ELOGF("POLWL",
              "CRI ERR!!|cp2:0x%08x|ec2:0x%08x|res_surf:0x%08x",
              ec->pixmap, ec,
              (unsigned int)cp,
              (unsigned int)e_pixmap_client_get(cp),
              (unsigned int)res_surf);
        return NULL;
     }

   return cp;
}

static Eina_Bool
_pol_wl_e_client_is_valid(E_Client *ec)
{
   E_Client *ec2;
   Eina_List *l;
   Eina_Bool del = EINA_FALSE;
   Eina_Bool found = EINA_FALSE;

   EINA_LIST_FOREACH(e_comp->clients, l, ec2)
     {
        if (ec2 == ec)
          {
             if (e_object_is_del(E_OBJECT(ec2)))
               del = EINA_TRUE;
             found = EINA_TRUE;
             break;
          }
     }

   return ((!del) && (found));
}

// --------------------------------------------------------
// visibility
// --------------------------------------------------------
static void
_tzvis_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzvis)
{
   wl_resource_destroy(res_tzvis);
}

static const struct tizen_visibility_interface _tzvis_iface =
{
   _tzvis_iface_cb_destroy
};

static void
_tzvis_iface_cb_vis_destroy(struct wl_resource *res_tzvis)
{
   Pol_Wl_Surface *psurf;
   Eina_Bool r;

   psurf = wl_resource_get_user_data(res_tzvis);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   r = _pol_wl_surf_is_valid(psurf);
   if (!r) return;

   psurf->vislist = eina_list_remove(psurf->vislist, res_tzvis);
}

static void
_tzpol_iface_cb_vis_get(struct wl_client *client, struct wl_resource *res_tzpol, uint32_t id, struct wl_resource *surf)
{
   E_Client *ec;
   Pol_Wl_Surface *psurf;
   struct wl_resource *res_tzvis;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   psurf = _pol_wl_surf_add(ec, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   res_tzvis = wl_resource_create(client,
                                  &tizen_visibility_interface,
                                  wl_resource_get_version(res_tzpol),
                                  id);
   if (!res_tzvis)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res_tzvis,
                                  &_tzvis_iface,
                                  psurf,
                                  _tzvis_iface_cb_vis_destroy);

   psurf->vislist = eina_list_append(psurf->vislist, res_tzvis);
}

void
e_mod_pol_wl_visibility_send(E_Client *ec, int vis)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   struct wl_resource *res_tzvis;
   Eina_List *l, *ll;
   Eina_Iterator *it;
   E_Client *ec2;
   Ecore_Window win;

   win = e_client_util_win_get(ec);

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          ec2 = e_pixmap_client_get(psurf->cp);
          if (ec2 != ec) continue;

          EINA_LIST_FOREACH(psurf->vislist, ll, res_tzvis)
            {
               tizen_visibility_send_notify(res_tzvis, vis);
               ELOGF("TZVIS",
                     "SEND     |win:0x%08x|res_tzvis:0x%08x|v:%d",
                     ec->pixmap, ec,
                     (unsigned int)win,
                     (unsigned int)res_tzvis,
                     vis);
               _launchscreen_hide(ec->netwm.pid);
            }
       }
   eina_iterator_free(it);
}

void
e_mod_pol_wl_iconify_state_change_send(E_Client *ec, int iconic)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   E_Client *ec2;
   Eina_List *l;
   Eina_Iterator *it;
   Ecore_Window win;

   if (e_config->transient.iconify)
     {
        E_Client *child;
        Eina_List *list = eina_list_clone(ec->transients);

        EINA_LIST_FREE(list, child)
          {
             if ((child->iconic == ec->iconic) &&
                 (child->exp_iconify.by_client == ec->exp_iconify.by_client))
               e_mod_pol_wl_iconify_state_change_send(child, iconic);

          }
     }

   win = e_client_util_win_get(ec);

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          ec2 = e_pixmap_client_get(psurf->cp);
          if (ec2 != ec) continue;

          tizen_policy_send_iconify_state_changed(tzpol->res_tzpol, psurf->surf, iconic, 1);
          ELOGF("ICONIFY",
                "SEND     |win:0x%08x|iconic:%d |sur:%p",
                ec->pixmap, ec,
                (unsigned int)win,
                iconic, psurf->surf);
          break;
       }
   eina_iterator_free(it);
}

// --------------------------------------------------------
// position
// --------------------------------------------------------
static void
_tzpos_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpos)
{
   wl_resource_destroy(res_tzpos);
}

static void
_tzpos_iface_cb_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpos, int32_t x, int32_t y)
{
   E_Client *ec;
   Pol_Wl_Surface *psurf;

   psurf = wl_resource_get_user_data(res_tzpos);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   ec = e_pixmap_client_get(psurf->cp);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   if(!E_INTERSECTS(ec->zone->x, ec->zone->y,
                    ec->zone->w, ec->zone->h,
                    x, y,
                    ec->w, ec->h))
     {
        e_mod_pol_wl_position_send(ec);
        return;
     }

   if (!ec->lock_client_location)
     {
        ec->x = ec->client.x = x;
        ec->y = ec->client.y = y;
     }
}

static const struct tizen_position_interface _tzpos_iface =
{
   _tzpos_iface_cb_destroy,
   _tzpos_iface_cb_set,
};

static void
_tzpol_iface_cb_pos_destroy(struct wl_resource *res_tzpos)
{
   Pol_Wl_Surface *psurf;
   Eina_Bool r;

   psurf = wl_resource_get_user_data(res_tzpos);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   r = _pol_wl_surf_is_valid(psurf);
   if (!r) return;

   psurf->poslist = eina_list_remove(psurf->poslist, res_tzpos);
}

static void
_tzpol_iface_cb_pos_get(struct wl_client *client, struct wl_resource *res_tzpol, uint32_t id, struct wl_resource *surf)
{
   E_Client *ec;
   Pol_Wl_Surface *psurf;
   struct wl_resource *res_tzpos;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   psurf = _pol_wl_surf_add(ec, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   res_tzpos = wl_resource_create(client,
                                  &tizen_position_interface,
                                  wl_resource_get_version(res_tzpol),
                                  id);
   if (!res_tzpos)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res_tzpos,
                                  &_tzpos_iface,
                                  psurf,
                                  _tzpol_iface_cb_pos_destroy);

   psurf->poslist = eina_list_append(psurf->poslist, res_tzpos);
}

void
e_mod_pol_wl_position_send(E_Client *ec)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   struct wl_resource *res_tzpos;
   Eina_List *l, *ll;
   Eina_Iterator *it;
   Ecore_Window win;

   win = e_client_util_win_get(ec);

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          if (e_pixmap_client_get(psurf->cp) != ec) continue;

          EINA_LIST_FOREACH(psurf->poslist, ll, res_tzpos)
            {
               tizen_position_send_changed(res_tzpos, ec->client.x, ec->client.y);
               ELOGF("TZPOS",
                     "SEND     |win:0x%08x|res_tzpos:0x%08x|ec->x:%d, ec->y:%d, ec->client.x:%d, ec->client.y:%d",
                     ec->pixmap, ec,
                     (unsigned int)win,
                     (unsigned int)res_tzpos,
                     ec->x, ec->y,
                     ec->client.x, ec->client.y);
            }
       }
   eina_iterator_free(it);
}

// --------------------------------------------------------
// stack: activate, raise, lower
// --------------------------------------------------------
static void
_tzpol_iface_cb_activate(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   if ((!starting) && (!ec->focused))
     {
        if ((ec->iconic) && (!ec->exp_iconify.by_client))
          e_mod_pol_wl_iconify_state_change_send(ec, 0);
        e_client_activate(ec, EINA_TRUE);
     }
   else
     evas_object_raise(ec->frame);
}

static void
_tzpol_iface_cb_raise(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   if ((!starting) && (!ec->focused))
     e_client_activate(ec, EINA_TRUE);
   else
     evas_object_raise(ec->frame);
}

static void
_tzpol_iface_cb_lower(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Client *ec, *below = NULL;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   below = ec;
   while ((below = e_client_below_get(below)))
     {
        if ((e_client_util_ignored_get(below)) ||
            (below->iconic))
          continue;

        break;
     }

   evas_object_lower(ec->frame);

   if ((!below) || (!ec->focused)) return;

   evas_object_focus_set(below->frame, 1);
}

static void
_tzpol_iface_cb_lower_by_res_id(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol,  uint32_t res_id)
{
   E_Client *ec, *below = NULL;

   ELOGF("TZPOL",
         "LOWER_RES|res_tzpol:0x%08x|res_id:%d",
         NULL, NULL, (unsigned int)res_tzpol, res_id);

   ec = e_pixmap_find_client_by_res_id(res_id);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   below = ec;
   while ((below = e_client_below_get(below)))
     {
        if ((e_client_util_ignored_get(below)) ||
            (below->iconic))
          continue;

        break;
     }

   evas_object_lower(ec->frame);

   if ((!below) || (!ec->focused)) return;

   if ((below->icccm.accepts_focus) ||(below->icccm.take_focus))
     evas_object_focus_set(below->frame, 1);
}

// --------------------------------------------------------
// focus
// --------------------------------------------------------
static void
_tzpol_iface_cb_focus_skip_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   if (ec->icccm.accepts_focus)
     {
        ec->icccm.accepts_focus = ec->icccm.take_focus = 0;
        EC_CHANGED(ec);
     }
}

static void
_tzpol_iface_cb_focus_skip_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   if (!ec->icccm.accepts_focus)
     {
        ec->icccm.accepts_focus = ec->icccm.take_focus = 1;
        EC_CHANGED(ec);
     }
}

// --------------------------------------------------------
// role
// --------------------------------------------------------
static void
_tzpol_iface_cb_role_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf, const char *role)
{
   E_Client *ec;

   EINA_SAFETY_ON_NULL_RETURN(role);

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   eina_stringshare_replace(&ec->icccm.window_role, role);

   /* TODO: support multiple roles */
   if (!e_util_strcmp("notification-low", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_LOW);
     }
   else if (!e_util_strcmp("notification-normal", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_NORMAL);
     }
   else if (!e_util_strcmp("notification-high", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_HIGH);
     }
   else if (!e_util_strcmp("alert", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_ALERT);
     }
   else if (!e_util_strcmp("tv-volume-popup", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_LOW);
        ec->lock_client_location = 1;
     }
   else if (!e_util_strcmp("e_demo", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_HIGH);
        ec->lock_client_location = 1;
     }
}

static void
_tzpol_iface_cb_type_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf, uint32_t type)
{
   E_Client *ec;
   E_Window_Type win_type;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   switch (type)
     {
      /* TODO: support other types */
      case TIZEN_POLICY_WIN_TYPE_NOTIFICATION: win_type = E_WINDOW_TYPE_NOTIFICATION; break;
      default: return;
     }

   ELOGF("TZPOL",
         "TYPE_SET |win:0x%08x|s:0x%08x|res_tzpol:0x%08x|tizen_win_type:%d, e_win_type:%d NOTI",
         ec->pixmap, ec,
         (unsigned int)e_client_util_win_get(ec),
         (unsigned int)surf,
         (unsigned int)res_tzpol,
         type, win_type);

   ec->netwm.type = win_type;

   EC_CHANGED(ec);
}
// --------------------------------------------------------
// conformant
// --------------------------------------------------------
static void
_tzpol_iface_cb_conformant_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf)
{
   E_Client *ec;
   Pol_Wl_Surface *psurf;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   psurf = _pol_wl_surf_add(ec, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   if (!ec->comp_data->conformant)
     {
        ec->comp_data->conformant = 1;
        EC_CHANGED(ec);
     }
}

static void
_tzpol_iface_cb_conformant_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Client *ec;
   Pol_Wl_Surface *psurf;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   psurf = _pol_wl_surf_add(ec, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   if (ec->comp_data->conformant)
     {
        ec->comp_data->conformant = 0;
        EC_CHANGED(ec);
     }
}

static void
_tzpol_iface_cb_conformant_get(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf)
{
   E_Client *ec;
   Pol_Wl_Surface *psurf;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   psurf = _pol_wl_surf_add(ec, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   tizen_policy_send_conformant(res_tzpol, surf, ec->comp_data->conformant);
}

void
e_mod_pol_wl_keyboard_geom_broadcast(E_Client *ec)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   E_Client *ec2;
   Eina_Bool r;
   Eina_List *l;
   Eina_Iterator *it;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          ec2 = e_pixmap_client_get(psurf->cp);
          if (!ec2) continue;

          r = e_client_util_ignored_get(ec2);
          if (r) continue;

          r = e_mod_pol_client_is_conformant(ec2);
          if (!r) continue;

          tizen_policy_send_conformant_area
            (tzpol->res_tzpol,
             psurf->surf,
             TIZEN_POLICY_CONFORMANT_PART_KEYBOARD,
             ec->visible, ec->x, ec->y,
             ec->client.w, ec->client.h);
       }
   eina_iterator_free(it);
}

// --------------------------------------------------------
// notification level
// --------------------------------------------------------
static Eina_Bool
_pol_wl_notilv_privilege_check(int fd)
{
#ifdef ENABLE_CYNARA
   static Eina_Bool first = EINA_TRUE;
   char *client_smack = NULL, *uid = NULL, *client_session = NULL;
   int ret, pid;

   if ((!polwl->p_cynara))
     {
        if (first)
          {
             int i;

             //try five times to tolerate temporary failure
             for (i = 0; i < 5; i++)
               if (cynara_initialize(&polwl->p_cynara, NULL) == CYNARA_API_SUCCESS)
                 break;

             first = EINA_FALSE;

             if (!polwl->p_cynara)
               {
                  ERR("cynara_initialize completely failed.");
                  return EINA_TRUE;
               }
          }
        else
          return EINA_TRUE;
     }

   ret = cynara_creds_socket_get_client(fd, CLIENT_METHOD_SMACK, &client_smack);
   if (ret != CYNARA_API_SUCCESS) goto cynara_failed;

   ret = cynara_creds_socket_get_user(fd, USER_METHOD_UID, &uid);
   if (ret != CYNARA_API_SUCCESS) goto cynara_failed;

   ret = cynara_creds_socket_get_pid(fd, &pid);
   if (ret != CYNARA_API_SUCCESS) goto cynara_failed;

   client_session = cynara_session_from_pid(pid);
   if (!client_session) goto cynara_failed;

   ret = cynara_check(polwl->p_cynara,
                      client_smack,
                      client_session,
                      uid,
                      "http://tizen.org.privilege/window.notification.set");

   if (ret == CYNARA_API_ACCESS_ALLOWED)
     return EINA_TRUE;

   return EINA_FALSE;

cynara_failed:
   if (client_session) free(client_session);
   if (uid) free(uid);
   if (client_smack) free(client_smack);

   return EINA_TRUE;
#else
   (void)ec;
   return EINA_TRUE;
#endif
}

static void
_tzpol_notilv_set(E_Client *ec, int lv)
{
   short ly;

   switch (lv)
     {
      case  0: ly = E_LAYER_CLIENT_NOTIFICATION_LOW;    break;
      case  1: ly = E_LAYER_CLIENT_NOTIFICATION_NORMAL; break;
      case  2: ly = E_LAYER_CLIENT_NOTIFICATION_TOP;    break;
      case -1: ly = E_LAYER_CLIENT_NORMAL;              break;
      case 10: ly = E_LAYER_CLIENT_NOTIFICATION_LOW;    break;
      case 20: ly = E_LAYER_CLIENT_NOTIFICATION_NORMAL; break;
      case 30: ly = E_LAYER_CLIENT_NOTIFICATION_HIGH;   break;
      case 40: ly = E_LAYER_CLIENT_NOTIFICATION_TOP;    break;
      default: ly = E_LAYER_CLIENT_NOTIFICATION_LOW;    break;
     }

   if (ly != evas_object_layer_get(ec->frame))
     {
        evas_object_layer_set(ec->frame, ly);
     }

   ec->layer = ly;
}

static void
_tzpol_iface_cb_notilv_set(struct wl_client *client, struct wl_resource *res_tzpol, struct wl_resource *surf, int32_t lv)
{
   E_Client *ec;
   Pol_Wl_Surface *psurf;
   int fd;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   psurf = _pol_wl_surf_add(ec, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   fd = wl_client_get_fd(client);
   if (!_pol_wl_notification_privilege_check(fd))
     {
        tizen_policy_send_notification_done
           (res_tzpol,
            surf,
            -1,
            TIZEN_POLICY_ERROR_STATE_PERMISSION_DENIED);
        return;
     }

   _tzpol_notilv_set(ec, lv);

   psurf->notilv = lv;

   tizen_policy_send_notification_done
     (res_tzpol, surf, lv, TIZEN_POLICY_ERROR_STATE_NONE);
}

void
e_mod_pol_wl_notification_level_fetch(E_Client *ec)
{
   E_Pixmap *cp;
   Pol_Wl_Surface *psurf;
   Pol_Wl_Tzpol *tzpol;
   Eina_Iterator *it;
   Eina_List *l;

   EINA_SAFETY_ON_NULL_RETURN(ec);

   cp = ec->pixmap;
   EINA_SAFETY_ON_NULL_RETURN(cp);

   // TODO: use pending_notilv_list instead of loop
   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          if (psurf->cp != cp) continue;
          if (!psurf->pending_notilv) continue;

          psurf->pending_notilv = EINA_FALSE;
          _tzpol_notilv_set(ec, psurf->notilv);
       }
   eina_iterator_free(it);
}

// --------------------------------------------------------
// transient for
// --------------------------------------------------------
static void
_pol_wl_parent_surf_set(E_Client *ec, struct wl_resource *parent_surf)
{
   E_Client *pc = NULL;

   if (parent_surf)
     {
        if (!(pc = wl_resource_get_user_data(parent_surf)))
          {
             ERR("Could not get parent res e_client");
             return;
          }
     }

   e_mod_pol_stack_transient_for_set(ec, pc);
}

static void
_tzpol_iface_cb_transient_for_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, uint32_t child_id, uint32_t parent_id)
{
   E_Client *ec, *pc;
   struct wl_resource *parent_surf;

   ELOGF("TZPOL",
         "TF_SET   |res_tzpol:0x%08x|parent:%d|child:%d",
         NULL, NULL, (unsigned int)res_tzpol, parent_id, child_id);

   ec = e_pixmap_find_client_by_res_id(child_id);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   pc = e_pixmap_find_client_by_res_id(parent_id);
   EINA_SAFETY_ON_NULL_RETURN(pc);
   EINA_SAFETY_ON_NULL_RETURN(pc->comp_data);

   parent_surf = pc->comp_data->surface;

   _pol_wl_parent_surf_set(ec, parent_surf);

   ELOGF("TZPOL",
         "         |win:0x%08x|parent|s:0x%08x",
         pc->pixmap, pc,
         (unsigned int)e_client_util_win_get(pc),
         (unsigned int)parent_surf);

   ELOGF("TZPOL",
         "         |win:0x%08x|child |s:0x%08x",
         ec->pixmap, ec,
         (unsigned int)e_client_util_win_get(ec),
         (unsigned int)(ec->comp_data ? ec->comp_data->surface : NULL));

   tizen_policy_send_transient_for_done(res_tzpol, child_id);

   EC_CHANGED(ec);
}

static void
_tzpol_iface_cb_transient_for_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, uint32_t child_id)
{
   E_Client *ec;

   ELOGF("TZPOL",
         "TF_UNSET |res_tzpol:0x%08x|child:%d",
         NULL, NULL, (unsigned int)res_tzpol, child_id);

   ec = e_pixmap_find_client_by_res_id(child_id);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   _pol_wl_parent_surf_set(ec, NULL);

   tizen_policy_send_transient_for_done(res_tzpol, child_id);

   EC_CHANGED(ec);
}

// --------------------------------------------------------
// window screen mode
// --------------------------------------------------------
static void
_tzpol_iface_cb_win_scrmode_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf, uint32_t mode)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   e_mod_pol_wl_win_scrmode_apply();

   tizen_policy_send_window_screen_mode_done
     (res_tzpol, surf, mode, TIZEN_POLICY_ERROR_STATE_NONE);
}

void
e_mod_pol_wl_win_scrmode_apply(void)
{
   // TODO: update screen mode for ec which was changed to be visible
   ;
}

// --------------------------------------------------------
// subsurface
// --------------------------------------------------------
static void
_tzpol_iface_cb_subsurf_place_below_parent(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *subsurf)
{
   E_Client *ec;
   E_Client *epc;
   E_Comp_Wl_Subsurf_Data *sdata;

   ec = wl_resource_get_user_data(subsurf);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->comp_data);

   sdata = ec->comp_data->sub.data;
   EINA_SAFETY_ON_NULL_RETURN(sdata);

   epc = sdata->parent;
   EINA_SAFETY_ON_NULL_RETURN(epc);

   /* check if a subsurface has already placed below a parent */
   if (eina_list_data_find(epc->comp_data->sub.below_list, ec)) return;

   epc->comp_data->sub.list = eina_list_remove(epc->comp_data->sub.list, ec);
   epc->comp_data->sub.list_pending = eina_list_remove(epc->comp_data->sub.list_pending, ec);
   epc->comp_data->sub.below_list_pending = eina_list_append(epc->comp_data->sub.below_list_pending, ec);
   epc->comp_data->sub.list_changed = EINA_TRUE;
}

static void
_tzpol_iface_cb_subsurface_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface, uint32_t parent_id)
{
   E_Client *ec, *epc;

   ELOGF("TZPOL",
         "SUBSURF   |wl_surface@%d|parent_id:%d",
         NULL, NULL, wl_resource_get_id(surface), parent_id);

   ec = wl_resource_get_user_data(surface);
   if (!ec)
     {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "tizen_policy failed: wrong wl_surface@%d resource",
                               wl_resource_get_id(surface));
        return;
     }

   if (e_object_is_del(E_OBJECT(ec))) return;

   epc = e_pixmap_find_client_by_res_id(parent_id);
   if (!epc)
     {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "tizen_policy failed: wrong parent_id(%d)", parent_id);
        return;
     }

   if (e_object_is_del(E_OBJECT(epc))) return;

   /* check if this surface is already a sub-surface */
   if ((ec->comp_data) && (ec->comp_data->sub.data))
     {
        wl_resource_post_error(resource,
                               WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                               "wl_surface@%d is already a sub-surface",
                               wl_resource_get_id(surface));
        return;
     }

   /* try to create a new subsurface */
   if (!e_comp_wl_subsurface_create(ec, epc, id, surface))
     ERR("Failed to create subsurface for surface@%d",
         wl_resource_get_id(surface));

   /* ec's parent comes from another process */
   ec->comp_data->has_extern_parent = EINA_TRUE;
}

static void
_tzpol_iface_cb_opaque_state_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface, int32_t state)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surface);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   ec->visibility.opaque = state;
}

// --------------------------------------------------------
// iconify
// --------------------------------------------------------
static void
_tzpol_iface_cb_iconify(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   ELOG("Set ICONIFY BY CLIENT", ec->pixmap, ec);
   ec->exp_iconify.by_client = 1;
   e_client_iconify(ec);
}

static void
_tzpol_iface_cb_uniconify(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Client *ec;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   if ((ec->iconic) && (!ec->exp_iconify.by_client))
     e_mod_pol_wl_iconify_state_change_send(ec, 0);

   ELOG("Un-Set ICONIFY BY CLIENT", ec->pixmap, ec);
   ec->exp_iconify.by_client = 0;
   e_client_uniconify(ec);
}

static void
_pol_wl_allowed_aux_hint_send(E_Client *ec, int id)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   Eina_List *l;
   Eina_Iterator *it;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          if (e_pixmap_client_get(psurf->cp) != ec) continue;
          tizen_policy_send_allowed_aux_hint
            (tzpol->res_tzpol,
             psurf->surf,
             id);
          ELOGF("TZPOL",
                "SEND     |res_tzpol:0x%08x|allowed hint->id:%d",
                ec->pixmap, ec,
                (unsigned int)tzpol->res_tzpol,
                id);
       }
   eina_iterator_free(it);
}

void
e_mod_pol_wl_eval_pre_new_client(E_Client *ec)
{
   E_Comp_Wl_Aux_Hint *hint;
   Eina_List *l;
   Eina_Bool send;

   EINA_LIST_FOREACH(ec->comp_data->aux_hint.hints, l, hint)
     {
        send = EINA_FALSE;
        if (!strcmp(hint->hint, hint_names[WM_POLICY_HINT_USER_GEOMETRY]))
          {
             if (!strcmp(hint->val, "1") && (ec->lock_client_location || ec->lock_client_size || !ec->placed))
               {
                  if (!e_mod_pol_client_is_noti(ec))
                    {
                       ec->netwm.type = E_WINDOW_TYPE_UTILITY;
                       ec->lock_client_location = EINA_FALSE;
                    }

                  e_mod_pol_allow_user_geometry_set(ec, EINA_TRUE);

                  ec->lock_client_size = EINA_FALSE;
                  ec->placed = 1;
                  send = EINA_TRUE;
                  EC_CHANGED(ec);
               }
             else if (strcmp(hint->val, "1") && (!ec->lock_client_location || !ec->lock_client_size || ec->placed))
               {
                  e_mod_pol_allow_user_geometry_set(ec, EINA_FALSE);

                  ec->lock_client_location = EINA_TRUE;
                  ec->lock_client_size = EINA_TRUE;
                  ec->placed = 0;
                  ec->netwm.type = E_WINDOW_TYPE_NORMAL;
                  send = EINA_TRUE;
                  EC_CHANGED(ec);
               }
          }
        else if (!strcmp(hint->hint, hint_names[WM_POLICY_HINT_FIXED_RESIZE]))
          {
             /* TODO: support other aux_hints */
          }
        else if (!strcmp(hint->hint, hint_names[WM_POLICY_HINT_DEICONIFY_APPROVE_DISABLE]))
          {
             /* TODO: would implement after deiconify approve protocol provided */
          }
        else if (!strcmp(hint->hint, hint_names[WM_POLICY_HINT_ICONIFY]))
          {
             if (!strcmp(hint->val, "disable"))
               {
                  ec->exp_iconify.skip_iconify = 1;
                  EC_CHANGED(ec);
               }
             else if (!strcmp(hint->val, "enable"))
               {
                  ec->exp_iconify.skip_iconify = 0;
                  EC_CHANGED(ec);
               }
          }

        if (send)
          _pol_wl_allowed_aux_hint_send(ec, hint->id);
     }
}

static void
_tzpol_iface_cb_aux_hint_add(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf, int32_t id, const char *name, const char *value)
{
   E_Client *ec;
   Eina_Bool res = EINA_FALSE;


   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   res = e_hints_aux_hint_add_with_pixmap(ec->pixmap, id, name, value);

   ELOGF("TZPOL", "HINT_ADD|res_tzpol:0x%08x|id:%d, name:%s, val:%s, res:%d", NULL, NULL, (unsigned int)res_tzpol, id, name, value, res);

   if (res)
     tizen_policy_send_allowed_aux_hint(res_tzpol, surf, id);
}

static void
_tzpol_iface_cb_aux_hint_change(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf, int32_t id, const char *value)
{
   E_Client *ec;
   Eina_Bool res = EINA_FALSE;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   res = e_hints_aux_hint_change_with_pixmap(ec->pixmap, id, value);

   ELOGF("TZPOL", "HINT_CHANGE|res_tzpol:0x%08x|id:%d, val:%s, result:%d", NULL, NULL,  (unsigned int)res_tzpol, id, value, res);

   if (res)
     tizen_policy_send_allowed_aux_hint(res_tzpol, surf, id);
}

static void
_tzpol_iface_cb_aux_hint_del(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf, int32_t id)
{
   E_Client *ec;
   unsigned int res = -1;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   res = e_hints_aux_hint_del_with_pixmap(ec->pixmap, id);
   ELOGF("TZPOL", "HINT_DEL|res_tzpol:0x%08x|id:%d, result:%d", NULL, NULL,  (unsigned int)res_tzpol, id, res);
}

static void
_tzpol_iface_cb_supported_aux_hints_get(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf)
{
   E_Client *ec;
   const Eina_List *hints_list;
   const Eina_List *l;
   struct wl_array hints;
   const char *hint_name;
   int len;
   char *p;

   ec = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   hints_list = e_hints_aux_hint_supported_get();

   wl_array_init(&hints);
   EINA_LIST_FOREACH(hints_list, l, hint_name)
     {
        len = strlen(hint_name) + 1;
        p = wl_array_add(&hints, len);

        if (p == NULL)
          break;
        strncpy(p, hint_name, len);
     }

   tizen_policy_send_supported_aux_hints(res_tzpol, surf, &hints, eina_list_count(hints_list));
   ELOGF("TZPOL",
         "SEND     |res_tzpol:0x%08x|supported_hints size:%d",
         ec->pixmap, ec,
         (unsigned int)res_tzpol,
         eina_list_count(hints_list));
   wl_array_release(&hints);
}

// --------------------------------------------------------
// tizen_policy_interface
// --------------------------------------------------------
static const struct tizen_policy_interface _tzpol_iface =
{
   _tzpol_iface_cb_vis_get,
   _tzpol_iface_cb_pos_get,
   _tzpol_iface_cb_activate,
   _tzpol_iface_cb_raise,
   _tzpol_iface_cb_lower,
   _tzpol_iface_cb_lower_by_res_id,
   _tzpol_iface_cb_focus_skip_set,
   _tzpol_iface_cb_focus_skip_unset,
   _tzpol_iface_cb_role_set,
   _tzpol_iface_cb_type_set,
   _tzpol_iface_cb_conformant_set,
   _tzpol_iface_cb_conformant_unset,
   _tzpol_iface_cb_conformant_get,
   _tzpol_iface_cb_notilv_set,
   _tzpol_iface_cb_transient_for_set,
   _tzpol_iface_cb_transient_for_unset,
   _tzpol_iface_cb_win_scrmode_set,
   _tzpol_iface_cb_subsurf_place_below_parent,
   _tzpol_iface_cb_subsurface_get,
   _tzpol_iface_cb_opaque_state_set,
   _tzpol_iface_cb_iconify,
   _tzpol_iface_cb_uniconify,
   _tzpol_iface_cb_aux_hint_add,
   _tzpol_iface_cb_aux_hint_change,
   _tzpol_iface_cb_aux_hint_del,
   _tzpol_iface_cb_supported_aux_hints_get,
};

static void
_tzpol_cb_unbind(struct wl_resource *res_tzpol)
{
   Pol_Wl_Tzpol *tzpol;

   tzpol = _pol_wl_tzpol_get(res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(tzpol);

   eina_hash_del_by_key(polwl->tzpols, &res_tzpol);
}

static void
_tzpol_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t ver, uint32_t id)
{
   Pol_Wl_Tzpol *tzpol;
   struct wl_resource *res_tzpol;

   EINA_SAFETY_ON_NULL_GOTO(polwl, err);

   res_tzpol = wl_resource_create(client,
                                  &tizen_policy_interface,
                                  ver,
                                  id);
   EINA_SAFETY_ON_NULL_GOTO(res_tzpol, err);

   tzpol = _pol_wl_tzpol_add(res_tzpol);
   EINA_SAFETY_ON_NULL_GOTO(tzpol, err);

   wl_resource_set_implementation(res_tzpol,
                                  &_tzpol_iface,
                                  NULL,
                                  _tzpol_cb_unbind);
   return;

err:
   ERR("Could not create tizen_policy_interface res: %m");
   wl_client_post_no_memory(client);
}

// --------------------------------------------------------
// tizen_ws_shell_interface::service
// --------------------------------------------------------
static void
_tzsh_srv_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_srv)
{
   wl_resource_destroy(res_tzsh_srv);
}

static void
_tzsh_srv_iface_cb_region_set(struct wl_client *client, struct wl_resource *res_tzsh_srv, int32_t type, int32_t angle, struct wl_resource *res_reg)
{
   Pol_Wl_Tzsh_Srv *tzsh_srv;
   Pol_Wl_Tzsh_Region *tzsh_reg;

   tzsh_srv = wl_resource_get_user_data(res_tzsh_srv);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_srv);

   if (!eina_list_data_find(polwl->tzsh_srvs, tzsh_srv))
     return;

   tzsh_reg = wl_resource_get_user_data(res_reg);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_reg);

   if (tzsh_srv->role == TZSH_SRV_ROLE_QUICKPANEL)
     e_mod_quickpanel_region_set(type, angle, tzsh_reg->tiler);
   else if (tzsh_srv->role == TZSH_SRV_ROLE_VOLUME)
     e_mod_volume_region_set(type, angle, tzsh_reg->tiler);
}

static void
_tzsh_srv_iface_cb_indicator_get(struct wl_client *client, struct wl_resource *res_tzsh_srv, uint32_t id)
{
   Pol_Wl_Tzsh_Srv *tzsh_srv;

   tzsh_srv = wl_resource_get_user_data(res_tzsh_srv);

   if (!eina_list_data_find(polwl->tzsh_srvs, tzsh_srv))
     return;

   /* TODO: create tws_indicator_service resource. */
}

static void
_tzsh_srv_qp_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_tzsh_srv_qp_cb_msg(struct wl_client *client EINA_UNUSED, struct wl_resource *resource, uint32_t msg)
{
   Pol_Wl_Tzsh_Srv *tzsh_srv;

   tzsh_srv = wl_resource_get_user_data(resource);

   EINA_SAFETY_ON_NULL_RETURN(tzsh_srv);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_srv->tzsh);

#define EC  tzsh_srv->tzsh->ec
   EINA_SAFETY_ON_NULL_RETURN(EC);

   switch (msg)
     {
      case TWS_SERVICE_QUICKPANEL_MSG_SHOW:
         e_mod_quickpanel_show();
         break;
      case TWS_SERVICE_QUICKPANEL_MSG_HIDE:
         e_mod_quickpanel_hide();
         break;
      default:
         ERR("Unknown message!! msg %d", msg);
         break;
     }
#undef EC
}

static const struct tws_service_quickpanel_interface _tzsh_srv_qp_iface =
{
   _tzsh_srv_qp_cb_destroy,
   _tzsh_srv_qp_cb_msg
};

static void
_tzsh_srv_iface_cb_quickpanel_get(struct wl_client *client, struct wl_resource *res_tzsh_srv, uint32_t id)
{
   Pol_Wl_Tzsh_Srv *tzsh_srv;
   struct wl_resource *res;

   tzsh_srv = wl_resource_get_user_data(res_tzsh_srv);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_srv);

   if (!eina_list_data_find(polwl->tzsh_srvs, tzsh_srv))
     return;

   res = wl_resource_create(client, &tws_service_quickpanel_interface, 1, id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_tzsh_srv_qp_iface, tzsh_srv, NULL);
}

static const struct tws_service_interface _tzsh_srv_iface =
{
   _tzsh_srv_iface_cb_destroy,
   _tzsh_srv_iface_cb_region_set,
   _tzsh_srv_iface_cb_indicator_get,
   _tzsh_srv_iface_cb_quickpanel_get
};

static void
_tzsh_cb_srv_destroy(struct wl_resource *res_tzsh_srv)
{
   Pol_Wl_Tzsh_Srv *tzsh_srv;

   tzsh_srv = wl_resource_get_user_data(res_tzsh_srv);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_srv);

   if (!eina_list_data_find(polwl->tzsh_srvs, tzsh_srv))
     return;

   _pol_wl_tzsh_srv_del(tzsh_srv);
}

static void
_tzsh_iface_cb_srv_create(struct wl_client *client, struct wl_resource *res_tzsh, uint32_t id, uint32_t surf_id, const char *name)
{
   Pol_Wl_Tzsh *tzsh;
   Pol_Wl_Tzsh_Srv *tzsh_srv;
   struct wl_resource *res_tzsh_srv;
   E_Client *ec;
   E_Pixmap *cp;
   int role;

   role = _pol_wl_tzsh_srv_role_get(name);
   if (role == TZSH_SRV_ROLE_UNKNOWN)
     {
        wl_resource_post_error
          (res_tzsh,
           WL_DISPLAY_ERROR_INVALID_OBJECT,
           "Invalid res_tzsh");
        return;
     }

   tzsh = wl_resource_get_user_data(res_tzsh);
   if (!tzsh)
     {
        wl_resource_post_error
          (res_tzsh,
           WL_DISPLAY_ERROR_INVALID_OBJECT,
           "Invalid res_tzsh's user data");
        return;
     }

   cp = _pol_wl_e_pixmap_get_from_id(client, surf_id);
   if (!cp)
     {
        wl_resource_post_error
          (res_tzsh,
           WL_DISPLAY_ERROR_INVALID_OBJECT,
           "Invalid surface id");
        return;
     }

   ec = e_pixmap_client_get(cp);
   if (ec)
     {
        if (!_pol_wl_e_client_is_valid(ec))
          {
             wl_resource_post_error
               (res_tzsh,
                WL_DISPLAY_ERROR_INVALID_OBJECT,
                "Invalid surface id");
             return;
          }
     }

   res_tzsh_srv = wl_resource_create(client,
                                     &tws_service_interface,
                                     wl_resource_get_version(res_tzsh),
                                     id);
   if (!res_tzsh_srv)
     {
        ERR("Could not create tws_service resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   _pol_wl_tzsh_data_set(tzsh, TZSH_TYPE_SRV, cp, ec);

   tzsh_srv = _pol_wl_tzsh_srv_add(tzsh,
                                   role,
                                   res_tzsh_srv,
                                   name);
   if (!tzsh_srv)
     {
        ERR("Could not create WS_Shell_Service");
        wl_client_post_no_memory(client);
        wl_resource_destroy(res_tzsh_srv);
        return;
     }

   wl_resource_set_implementation(res_tzsh_srv,
                                  &_tzsh_srv_iface,
                                  tzsh_srv,
                                  _tzsh_cb_srv_destroy);

   if (role == TZSH_SRV_ROLE_QUICKPANEL)
     e_mod_quickpanel_client_set(tzsh->ec);
   else if (role == TZSH_SRV_ROLE_VOLUME)
     e_mod_volume_client_set(tzsh->ec);
}

// --------------------------------------------------------
// tizen_ws_shell_interface::region
// --------------------------------------------------------
static void
_tzsh_reg_cb_shell_destroy(struct wl_listener *listener, void *data)
{
   Pol_Wl_Tzsh_Region *tzsh_reg;

   tzsh_reg = container_of(listener, Pol_Wl_Tzsh_Region, destroy_listener);

   if (tzsh_reg->res_tzsh_reg)
     {
        wl_resource_destroy(tzsh_reg->res_tzsh_reg);
        tzsh_reg->res_tzsh_reg = NULL;
     }
}

static void
_tzsh_reg_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_reg)
{
   wl_resource_destroy(res_tzsh_reg);
}

static void
_tzsh_reg_iface_cb_add(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_reg, int32_t x, int32_t y, int32_t w, int32_t h)
{
   Pol_Wl_Tzsh_Region *tzsh_reg;
   Eina_Tiler *src;
   int area_w = 0, area_h = 0;

   tzsh_reg = wl_resource_get_user_data(res_tzsh_reg);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_reg);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_reg->tiler);

   eina_tiler_area_size_get(tzsh_reg->tiler, &area_w, &area_h);
   src = eina_tiler_new(area_w, area_h);
   eina_tiler_tile_size_set(src, 1, 1);
   eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});
   eina_tiler_union(tzsh_reg->tiler, src);
   eina_tiler_free(src);
}

static void
_tzsh_reg_iface_cb_subtract(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_reg, int32_t x, int32_t y, int32_t w, int32_t h)
{
   Pol_Wl_Tzsh_Region *tzsh_reg;
   Eina_Tiler *src;
   int area_w = 0, area_h = 0;

   tzsh_reg = wl_resource_get_user_data(res_tzsh_reg);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_reg);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_reg->tiler);

   eina_tiler_area_size_get(tzsh_reg->tiler, &area_w, &area_h);
   src = eina_tiler_new(area_w, area_h);
   eina_tiler_tile_size_set(src, 1, 1);
   eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});
   eina_tiler_subtract(tzsh_reg->tiler, src);
   eina_tiler_free(src);
}

static const struct tws_region_interface _tzsh_reg_iface =
{
   _tzsh_reg_iface_cb_destroy,
   _tzsh_reg_iface_cb_add,
   _tzsh_reg_iface_cb_subtract
};

static void
_tzsh_reg_cb_destroy(struct wl_resource *res_tzsh_reg)
{
   Pol_Wl_Tzsh_Region *tzsh_reg;

   tzsh_reg = wl_resource_get_user_data(res_tzsh_reg);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_reg);

   wl_list_remove(&tzsh_reg->destroy_listener.link);
   eina_tiler_free(tzsh_reg->tiler);

   E_FREE(tzsh_reg);
}

static void
_tzsh_iface_cb_reg_create(struct wl_client *client, struct wl_resource *res_tzsh, uint32_t id)
{
   Pol_Wl_Tzsh *tzsh;
   Pol_Wl_Tzsh_Region *tzsh_reg = NULL;
   Eina_Tiler *tz = NULL;
   struct wl_resource *res_tzsh_reg;
   int zw = 0, zh = 0;

   tzsh = wl_resource_get_user_data(res_tzsh);
   if (!tzsh)
     {
        wl_resource_post_error
          (res_tzsh,
           WL_DISPLAY_ERROR_INVALID_OBJECT,
           "Invalid res_tzsh's user data");
        return;
     }

   tzsh_reg = E_NEW(Pol_Wl_Tzsh_Region, 1);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_reg);

   e_zone_useful_geometry_get(e_zone_current_get(),
                              NULL, NULL, &zw, &zh);

   tz = eina_tiler_new(zw, zh);
   EINA_SAFETY_ON_NULL_GOTO(tz, err);
   tzsh_reg->tiler = tz;

   eina_tiler_tile_size_set(tzsh_reg->tiler, 1, 1);

   if (!(res_tzsh_reg = wl_resource_create(client,
                                           &tws_region_interface,
                                           wl_resource_get_version(res_tzsh),
                                           id)))
     {
        ERR("Could not create tws_service resource: %m");
        wl_client_post_no_memory(client);
        goto err;
     }

   wl_resource_set_implementation(res_tzsh_reg,
                                  &_tzsh_reg_iface,
                                  tzsh_reg,
                                  _tzsh_reg_cb_destroy);

   tzsh_reg->tzsh = tzsh;
   tzsh_reg->res_tzsh_reg = res_tzsh_reg;
   tzsh_reg->destroy_listener.notify = _tzsh_reg_cb_shell_destroy;

   wl_resource_add_destroy_listener(res_tzsh,
                                    &tzsh_reg->destroy_listener);
   return;

err:
   if (tzsh_reg->tiler) eina_tiler_free(tzsh_reg->tiler);
   E_FREE(tzsh_reg);
}

// --------------------------------------------------------
// tizen_ws_shell_interface::quickpanel
// --------------------------------------------------------
static void
_tzsh_qp_iface_cb_release(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_qp)
{
   wl_resource_destroy(res_tzsh_qp);
}

static void
_tzsh_qp_iface_cb_show(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_qp EINA_UNUSED)
{
   /* TODO: request quickpanel show */
   ;
}

static void
_tzsh_qp_iface_cb_hide(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_qp EINA_UNUSED)
{
   /* TODO: request quickpanel hide */
   ;
}

static void
_tzsh_qp_iface_cb_enable(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_qp EINA_UNUSED)
{
   /* TODO: request quickpanel enable */
   ;
}

static void
_tzsh_qp_iface_cb_disable(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_qp EINA_UNUSED)
{
   /* TODO: request quickpanel disable */
   ;
}

static const struct tws_quickpanel_interface _tzsh_qp_iface =
{
   _tzsh_qp_iface_cb_release,
   _tzsh_qp_iface_cb_show,
   _tzsh_qp_iface_cb_hide,
   _tzsh_qp_iface_cb_enable,
   _tzsh_qp_iface_cb_disable
};

static void
_tzsh_cb_qp_destroy(struct wl_resource *res_tzsh_qp)
{
   Pol_Wl_Tzsh_Client *tzsh_client;

   tzsh_client = wl_resource_get_user_data(res_tzsh_qp);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_client);

   _pol_wl_tzsh_client_del(tzsh_client);
}

static void
_tzsh_iface_cb_qp_get(struct wl_client *client, struct wl_resource *res_tzsh, uint32_t id, uint32_t surf_id)
{
   Pol_Wl_Tzsh *tzsh;
   Pol_Wl_Tzsh_Client *tzsh_client;
   struct wl_resource *res_tzsh_qp;
   E_Client *ec;
   E_Pixmap *cp;

   tzsh = wl_resource_get_user_data(res_tzsh);
   if (!tzsh)
     {
        wl_resource_post_error
          (res_tzsh,
           WL_DISPLAY_ERROR_INVALID_OBJECT,
           "Invalid res_tzsh's user data");
        return;
     }

   cp = _pol_wl_e_pixmap_get_from_id(client, surf_id);
   if (!cp)
     {
        wl_resource_post_error
          (res_tzsh,
           WL_DISPLAY_ERROR_INVALID_OBJECT,
           "Invalid surface id");
        return;
     }

   ec = e_pixmap_client_get(cp);
   if (ec)
     {
        if (!_pol_wl_e_client_is_valid(ec))
          {
             wl_resource_post_error
               (res_tzsh,
                WL_DISPLAY_ERROR_INVALID_OBJECT,
                "Invalid surface id");
             return;
          }
     }

   res_tzsh_qp = wl_resource_create(client,
                                    &tws_quickpanel_interface,
                                    wl_resource_get_version(res_tzsh),
                                    id);
   if (!res_tzsh_qp)
     {
        ERR("Could not create tws_quickpanel resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   _pol_wl_tzsh_data_set(tzsh, TZSH_TYPE_CLIENT, cp, ec);

   tzsh_client = _pol_wl_tzsh_client_add(tzsh, res_tzsh_qp);
   if (!tzsh_client)
     {
        ERR("Could not create tzsh_client");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res_tzsh_qp,
                                  &_tzsh_qp_iface,
                                  tzsh_client,
                                  _tzsh_cb_qp_destroy);
}

// --------------------------------------------------------
// tizen_ws_shell_interface::tvservice
// --------------------------------------------------------
static void
_tzsh_tvsrv_iface_cb_release(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_tvsrv)
{
   wl_resource_destroy(res_tzsh_tvsrv);
}

static void
_tzsh_tvsrv_iface_cb_bind(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_tvsrv)
{
   Pol_Wl_Tzsh_Client *tzsh_client;

   tzsh_client = wl_resource_get_user_data(res_tzsh_tvsrv);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_client);

   if (!eina_list_data_find(polwl->tzsh_clients, tzsh_client))
     return;

   polwl->tvsrv_bind_list = eina_list_append(polwl->tvsrv_bind_list, tzsh_client);

   _pol_wl_tzsh_srv_tvsrv_bind_update();
}

static void
_tzsh_tvsrv_iface_cb_unbind(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh_tvsrv)
{
   Pol_Wl_Tzsh_Client *tzsh_client;

   tzsh_client = wl_resource_get_user_data(res_tzsh_tvsrv);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_client);

   if (!eina_list_data_find(polwl->tzsh_clients, tzsh_client))
     return;

   polwl->tvsrv_bind_list = eina_list_remove(polwl->tvsrv_bind_list, tzsh_client);

   _pol_wl_tzsh_srv_tvsrv_bind_update();
}

static const struct tws_tvsrv_interface _tzsh_tvsrv_iface =
{
   _tzsh_tvsrv_iface_cb_release,
   _tzsh_tvsrv_iface_cb_bind,
   _tzsh_tvsrv_iface_cb_unbind
};

static void
_tzsh_cb_tvsrv_destroy(struct wl_resource *res_tzsh_tvsrv)
{
   Pol_Wl_Tzsh_Client *tzsh_client;

   tzsh_client = wl_resource_get_user_data(res_tzsh_tvsrv);
   EINA_SAFETY_ON_NULL_RETURN(tzsh_client);

   if (!eina_list_data_find(polwl->tzsh_clients, tzsh_client))
     return;

   polwl->tvsrv_bind_list = eina_list_remove(polwl->tvsrv_bind_list, tzsh_client);

   _pol_wl_tzsh_srv_tvsrv_bind_update();
   _pol_wl_tzsh_client_del(tzsh_client);
}

static void
_tzsh_iface_cb_tvsrv_get(struct wl_client *client, struct wl_resource *res_tzsh, uint32_t id, uint32_t surf_id)
{
   Pol_Wl_Tzsh *tzsh;
   Pol_Wl_Tzsh_Client *tzsh_client;
   struct wl_resource *res_tzsh_tvsrv;
   E_Pixmap *cp;
   E_Client *ec;

   tzsh = wl_resource_get_user_data(res_tzsh);
   if (!tzsh)
     {
        wl_resource_post_error
          (res_tzsh,
           WL_DISPLAY_ERROR_INVALID_OBJECT,
           "Invalid res_tzsh's user data");
        return;
     }

   cp = _pol_wl_e_pixmap_get_from_id(client, surf_id);
   if (!cp)
     {
        wl_resource_post_error
          (res_tzsh,
           WL_DISPLAY_ERROR_INVALID_OBJECT,
           "Invalid surface id");
        return;
     }

   ec = e_pixmap_client_get(cp);
   if (ec)
     {
        if (!_pol_wl_e_client_is_valid(ec))
          {
             wl_resource_post_error
               (res_tzsh,
                WL_DISPLAY_ERROR_INVALID_OBJECT,
                "Invalid surface id");
             return;
          }
     }

   res_tzsh_tvsrv = wl_resource_create(client,
                                       &tws_tvsrv_interface,
                                       wl_resource_get_version(res_tzsh),
                                       id);
   if (!res_tzsh_tvsrv)
     {
        ERR("Could not create tws_tvsrv resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   _pol_wl_tzsh_data_set(tzsh, TZSH_TYPE_CLIENT, cp, ec);

   tzsh_client = _pol_wl_tzsh_client_add(tzsh, res_tzsh_tvsrv);
   if (!tzsh_client)
     {
        ERR("Could not create tzsh_client");
        wl_client_post_no_memory(client);
        wl_resource_destroy(res_tzsh_tvsrv);
        return;
     }

   wl_resource_set_implementation(res_tzsh_tvsrv,
                                  &_tzsh_tvsrv_iface,
                                  tzsh_client,
                                  _tzsh_cb_tvsrv_destroy);
}

// --------------------------------------------------------
// tizen_ws_shell_interface
// --------------------------------------------------------
static void
_tzsh_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzsh)
{
   wl_resource_destroy(res_tzsh);
}

static const struct tizen_ws_shell_interface _tzsh_iface =
{
   _tzsh_iface_cb_destroy,
   _tzsh_iface_cb_srv_create,
   _tzsh_iface_cb_reg_create,
   _tzsh_iface_cb_qp_get,
   _tzsh_iface_cb_tvsrv_get
};

static void
_tzsh_cb_unbind(struct wl_resource *res_tzsh)
{
   Pol_Wl_Tzsh *tzsh;

   tzsh = wl_resource_get_user_data(res_tzsh);
   EINA_SAFETY_ON_NULL_RETURN(tzsh);

   _pol_wl_tzsh_del(tzsh);
}

static void
_tzsh_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t ver, uint32_t id)
{
   Pol_Wl_Tzsh *tzsh;
   struct wl_resource *res_tzsh;

   EINA_SAFETY_ON_NULL_GOTO(polwl, err);

   res_tzsh = wl_resource_create(client,
                                 &tizen_ws_shell_interface,
                                 ver,
                                 id);
   EINA_SAFETY_ON_NULL_GOTO(res_tzsh, err);

   tzsh = _pol_wl_tzsh_add(res_tzsh);
   EINA_SAFETY_ON_NULL_GOTO(tzsh, err);

   wl_resource_set_implementation(res_tzsh,
                                  &_tzsh_iface,
                                  tzsh,
                                  _tzsh_cb_unbind);

   _pol_wl_tzsh_registered_srv_send(tzsh);
   return;

err:
   ERR("Could not create tizen_ws_shell_interface res: %m");
   wl_client_post_no_memory(client);
}

// --------------------------------------------------------
// tizen_launchscreen_interface
// --------------------------------------------------------
static void
_launchscreen_hide(uint32_t pid)
{
   Eina_List *l, *ll;
   Pol_Wl_Tzlaunch *plauncher;
   Pol_Wl_Tzlaunch_Img *pimg;

   if(pid <= 0) return;

   EINA_LIST_FOREACH(polwl->tzlaunchs, l, plauncher)
     {
        EINA_LIST_FOREACH(plauncher->imglist, ll, pimg)
           if (pimg->pid == pid)
             {
                DBG("Launch Screen hide | pid %d", pid);
                _launch_img_off(pimg);
             }
     }

   return;
}

static void
_launch_img_off(Pol_Wl_Tzlaunch_Img *tzlaunchimg)
{
   Evas_Object *old_o;

   if (!launch_scrn) return;
   if(!tzlaunchimg->valid) return;

   old_o = edje_object_part_swallow_get(launch_scrn->shobj, "e.swallow.content");
   if ((!old_o)&& ( old_o == tzlaunchimg->obj))
     {
        //edje_object_signal_emit(launch_scrn->shobj, "e,action,go,hide", "e");
        edje_object_part_unswallow(launch_scrn->shobj, tzlaunchimg->obj);
     }
   evas_object_hide(tzlaunchimg->obj);

   if (launch_scrn->timeout) ecore_timer_del(launch_scrn->timeout);
   launch_scrn->timeout = NULL;
}

static Eina_Bool
_launch_timeout(void *data)
{
   Pol_Wl_Tzlaunch_Img *tzlaunchimg;
   tzlaunchimg = (Pol_Wl_Tzlaunch_Img *)data;

   EINA_SAFETY_ON_NULL_RETURN_VAL(tzlaunchimg, 0);

   _launch_img_off(tzlaunchimg);

   return ECORE_CALLBACK_CANCEL;
}

static void
_tzlaunchimg_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzlaunchimg)
{
   wl_resource_destroy(res_tzlaunchimg);
}

static void
_tzlaunchimg_iface_cb_launch(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzlaunchimg, const char *pfname, uint32_t ftype, uint32_t indicator, uint32_t angle)
{
   Pol_Wl_Tzlaunch_Img *tzlaunchimg;
   Evas_Load_Error err;
   Eina_Bool res = EINA_TRUE;
   Evas_Object *old_o = NULL;
   if (!launch_scrn) return;

   tzlaunchimg = wl_resource_get_user_data(res_tzlaunchimg);
   EINA_SAFETY_ON_NULL_RETURN(tzlaunchimg);

   // TO DO
   // invaid parameter handle
   DBG("%s | path %s(%d), indicator(%d), angle(%d)", __FUNCTION__, pfname, ftype, indicator, angle);
   tzlaunchimg->path = pfname;
   tzlaunchimg->type = ftype;
   tzlaunchimg->indicator = indicator;
   tzlaunchimg->angle = angle;

   old_o = edje_object_part_swallow_get(launch_scrn->shobj, "e.swallow.content");
   if (old_o)
     {
        edje_object_part_unswallow(launch_scrn->shobj, old_o);
        evas_object_hide(old_o);
        DBG("%s | unswallow %p", __FUNCTION__, old_o);
     }

   if (tzlaunchimg->obj) evas_object_del(tzlaunchimg->obj);

   if (tzlaunchimg->type == E_LAUNCH_FILE_TYPE_IMAGE)
     {
        tzlaunchimg->obj = evas_object_image_add(e_comp->evas);
        EINA_SAFETY_ON_NULL_GOTO(tzlaunchimg->obj, error);
        evas_object_image_file_set(tzlaunchimg->obj, tzlaunchimg->path, NULL);

        err = evas_object_image_load_error_get(tzlaunchimg->obj);
        EINA_SAFETY_ON_FALSE_GOTO(err == EVAS_LOAD_ERROR_NONE, error);

        evas_object_image_fill_set(tzlaunchimg->obj, 0, 0,  e_comp->w, e_comp->h);
        evas_object_image_filled_set(tzlaunchimg->obj, EINA_TRUE);
     }
   else
     {
        tzlaunchimg->obj = edje_object_add(e_comp->evas);
        EINA_SAFETY_ON_NULL_GOTO(tzlaunchimg->obj, error);
        edje_object_file_set (tzlaunchimg->obj, tzlaunchimg->path, APP_DEFINE_GROUP_NAME);

        evas_object_move(tzlaunchimg->obj, 0, 0);
        evas_object_resize(tzlaunchimg->obj, e_comp->w, e_comp->h);
     }

   res = edje_object_part_swallow(launch_scrn->shobj, "e.swallow.content", tzlaunchimg->obj);
   EINA_SAFETY_ON_FALSE_GOTO(res, error);

   evas_object_show(tzlaunchimg->obj);
   evas_object_show(launch_scrn->shobj);
   edje_object_signal_emit(launch_scrn->shobj, "e,action,go,visible", "e");

   if (launch_scrn->timeout) ecore_timer_del(launch_scrn->timeout);
   launch_scrn->timeout = ecore_timer_add(4.0f, _launch_timeout, tzlaunchimg);

   tzlaunchimg->valid = EINA_TRUE;

   return;

error:
   ERR("Could not complete %s", __FUNCTION__);
   return;
}

static void
_tzlaunchimg_iface_cb_show(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzlaunchimg)
{
   /* TODO: request launch img show */

}

static void
_tzlaunchimg_iface_cb_hide(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzlaunchimg)
{
   /* TODO: request launch img hide */
}

static void
_tzlaunchimg_iface_cb_owner(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzlaunchimg, uint32_t pid)
{
   Pol_Wl_Tzlaunch_Img *tzlaunchimg;

   DBG("Launch img(%d) pid: %d", wl_resource_get_id(res_tzlaunchimg), pid);

   tzlaunchimg = wl_resource_get_user_data(res_tzlaunchimg);
   EINA_SAFETY_ON_NULL_RETURN(tzlaunchimg);

   tzlaunchimg->pid = pid;
}


static const struct tizen_launch_image_interface _tzlaunchimg_iface =
{
   _tzlaunchimg_iface_cb_destroy,
   _tzlaunchimg_iface_cb_launch,
   _tzlaunchimg_iface_cb_owner,
   _tzlaunchimg_iface_cb_show,
   _tzlaunchimg_iface_cb_hide
};

static Pol_Wl_Tzlaunch_Img *
_tzlaunch_img_add(struct wl_resource *res_tzlaunch, struct wl_resource *res_tzlaunch_img)
{
   Pol_Wl_Tzlaunch *tzlaunch;
   Pol_Wl_Tzlaunch_Img *tzlaunchimg;

   tzlaunchimg = E_NEW(Pol_Wl_Tzlaunch_Img, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzlaunchimg, NULL);

   tzlaunch = wl_resource_get_user_data(res_tzlaunch);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzlaunch, NULL);

   tzlaunch->imglist = eina_list_append(tzlaunch->imglist, tzlaunchimg);

   tzlaunchimg->tzlaunch  = tzlaunch;
   tzlaunchimg->res_tzlaunch_img = res_tzlaunch_img;

   return tzlaunchimg;
}

static void
_tzlaunch_img_destroy(struct wl_resource *res_tzlaunchimg)
{
   Pol_Wl_Tzlaunch_Img *tzlaunchimg;
   Pol_Wl_Tzlaunch *tzlaunch;

   EINA_SAFETY_ON_NULL_RETURN(res_tzlaunchimg);
   tzlaunchimg = wl_resource_get_user_data(res_tzlaunchimg);

   _launch_img_off(tzlaunchimg);
   if(tzlaunchimg->obj) evas_object_del(tzlaunchimg->obj);

   tzlaunch = tzlaunchimg->tzlaunch;
   tzlaunch->imglist = eina_list_remove(tzlaunch->imglist, tzlaunchimg);

   memset(tzlaunchimg, 0x0, sizeof(Pol_Wl_Tzlaunch_Img));
   E_FREE(tzlaunchimg);
}


static void
_tzlaunch_iface_cb_create_img(struct wl_client *client, struct wl_resource *res_tzlaunch, uint32_t id)
{

   Pol_Wl_Tzlaunch_Img *plaunchimg;
   struct wl_resource *res_tzlaunch_img;

   res_tzlaunch_img = wl_resource_create(client,
                                         &tizen_launch_image_interface,
                                         wl_resource_get_version(res_tzlaunch),
                                         id);
   if (!res_tzlaunch_img)
     {
        wl_resource_post_error
           (res_tzlaunch_img,
            WL_DISPLAY_ERROR_INVALID_OBJECT,
            "Invalid res_tzlaunch's user data");
        return;
     }

   plaunchimg = _tzlaunch_img_add(res_tzlaunch, res_tzlaunch_img);
   EINA_SAFETY_ON_NULL_GOTO(plaunchimg, err);

   wl_resource_set_implementation(res_tzlaunch_img,
                                  &_tzlaunchimg_iface,
                                  plaunchimg,
                                  _tzlaunch_img_destroy);

   return;

err:
   ERR("Could not create tizen_launch_image_interface res: %m");
   wl_client_post_no_memory(client);
}


static const struct tizen_launchscreen_interface _tzlaunch_iface =
{
   _tzlaunch_iface_cb_create_img
};

static void
_tzlaunch_del(Pol_Wl_Tzlaunch *tzlaunch)
{
   Pol_Wl_Tzlaunch_Img *plaunchimg;
   Eina_List *l, *ll;

   EINA_SAFETY_ON_NULL_RETURN(tzlaunch);

   // remove tzlaunch created imglist
   EINA_LIST_FOREACH_SAFE(tzlaunch->imglist, l, ll, plaunchimg)
     {
        if (plaunchimg->tzlaunch != tzlaunch) continue;
        wl_resource_destroy(plaunchimg->res_tzlaunch_img);
        break;
     }

   polwl->tzlaunchs = eina_list_remove(polwl->tzlaunchs, tzlaunch);

   memset(tzlaunch, 0x0, sizeof(Pol_Wl_Tzlaunch));
   E_FREE(tzlaunch);
}

static Pol_Wl_Tzlaunch *
_tzlaunch_add(struct wl_resource *res_tzlaunch)
{
   Pol_Wl_Tzlaunch *tzlaunch;

   tzlaunch = E_NEW(Pol_Wl_Tzlaunch, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzlaunch, NULL);

   tzlaunch->res_tzlaunch = res_tzlaunch;

   polwl->tzlaunchs = eina_list_append(polwl->tzlaunchs, tzlaunch);

   return tzlaunch;
}

static void
_tzlaunch_cb_unbind(struct wl_resource *res_tzlaunch)
{
   Pol_Wl_Tzlaunch *tzlaunch = NULL;
   Eina_List *l, *ll;

   EINA_LIST_FOREACH_SAFE(polwl->tzlaunchs, l, ll, tzlaunch)
     {
        if (tzlaunch->res_tzlaunch != res_tzlaunch) continue;
        _tzlaunch_del(tzlaunch);
        break;
     }
}

static void
_tzlaunch_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t ver, uint32_t id)
{
   Pol_Wl_Tzlaunch *tzlaunch = NULL;
   struct wl_resource *res_tzlaunch;

   if(!launch_scrn)launch_scrn = e_comp->launchscrn;

   EINA_SAFETY_ON_NULL_GOTO(launch_scrn, err);
   EINA_SAFETY_ON_NULL_GOTO(polwl, err);

   res_tzlaunch = wl_resource_create(client,
                                     &tizen_launchscreen_interface,
                                     ver,
                                     id);
   EINA_SAFETY_ON_NULL_GOTO(res_tzlaunch, err);

   tzlaunch = _tzlaunch_add(res_tzlaunch);
   EINA_SAFETY_ON_NULL_GOTO(tzlaunch, err);

   wl_resource_set_implementation(res_tzlaunch,
                                  &_tzlaunch_iface,
                                  tzlaunch,
                                  _tzlaunch_cb_unbind);

   return;

err:
   ERR("Could not create tizen_launchscreen_interface res: %m");
   wl_client_post_no_memory(client);
}


// --------------------------------------------------------
// public functions
// --------------------------------------------------------
void
e_mod_pol_wl_client_add(E_Client *ec)
{
   EINA_SAFETY_ON_NULL_RETURN(ec);
   if (!ec->pixmap) return;

   _pol_wl_surf_client_set(ec);
   _pol_wl_tzsh_client_set(ec);
}

void
e_mod_pol_wl_client_del(E_Client *ec)
{
   EINA_SAFETY_ON_NULL_RETURN(ec);
   if (!ec->pixmap) return;

   e_mod_pol_wl_pixmap_del(ec->pixmap);
   _pol_wl_tzsh_client_unset(ec);
}

void
e_mod_pol_wl_pixmap_del(E_Pixmap *cp)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   Eina_List *l, *ll;
   Eina_Iterator *it;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH_SAFE(tzpol->psurfs, l, ll, psurf)
       {
          if (psurf->cp != cp) continue;
          tzpol->psurfs = eina_list_remove_list(tzpol->psurfs, l);
          _pol_wl_surf_del(psurf);
       }
   eina_iterator_free(it);
}

void
e_mod_pol_wl_aux_hint_init(void)
{
   int i, n;
   n = (sizeof(hint_names) / sizeof(char *));

   for (i = 0; i < n; i++)
     {
        e_hints_aux_hint_supported_add(hint_names[i]);
     }
   return;
}

Eina_Bool
e_mod_pol_wl_defer_job(void)
{
   struct wl_global *global = NULL;
   EINA_SAFETY_ON_NULL_GOTO(polwl, err);

   global = wl_global_create(e_comp_wl->wl.disp,
                             &tizen_launchscreen_interface,
                             1,
                             NULL,
                             _tzlaunch_cb_bind);
   EINA_SAFETY_ON_NULL_GOTO(global, err);

   polwl->globals = eina_list_append(polwl->globals, global);

   return EINA_TRUE;

err:
   if (global)
     {
        wl_global_destroy(global);
     }
   return EINA_FALSE;
}

Eina_Bool
e_mod_pol_wl_init(void)
{
   struct wl_global *global;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl->wl.disp, EINA_FALSE);

   polwl = E_NEW(Pol_Wl, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(polwl, EINA_FALSE);

   global = wl_global_create(e_comp_wl->wl.disp,
                             &tizen_policy_interface,
                             1,
                             NULL,
                             _tzpol_cb_bind);
   EINA_SAFETY_ON_NULL_GOTO(global, err);
   polwl->globals = eina_list_append(polwl->globals, global);

   global = wl_global_create(e_comp_wl->wl.disp,
                             &tizen_ws_shell_interface,
                             1,
                             NULL,
                             _tzsh_cb_bind);
   EINA_SAFETY_ON_NULL_GOTO(global, err);
   polwl->globals = eina_list_append(polwl->globals, global);

   polwl->tzpols = eina_hash_pointer_new(_pol_wl_tzpol_del);

#ifdef ENABLE_CYNARA
   if (cynara_initialize(&polwl->p_cynara, NULL) != CYNARA_API_SUCCESS)
     ERR("cynara_initialize failed.");
#endif
   return EINA_TRUE;

err:
   if (polwl)
     {
        EINA_LIST_FREE(polwl->globals, global)
          wl_global_destroy(global);

        E_FREE(polwl);
     }
   return EINA_FALSE;
}

void
e_mod_pol_wl_shutdown(void)
{
   Pol_Wl_Tzsh *tzsh;
   Pol_Wl_Tzsh_Srv *tzsh_srv;
   Pol_Wl_Tzlaunch *tzlaunch;
   struct wl_global *global;
   int i;

   EINA_SAFETY_ON_NULL_RETURN(polwl);

   for (i = 0; i < TZSH_SRV_ROLE_MAX; i++)
     {
        tzsh_srv = polwl->srvs[i];
        if (!tzsh_srv) continue;

        wl_resource_destroy(tzsh_srv->res_tzsh_srv);
     }

   EINA_LIST_FREE(polwl->tzshs, tzsh)
     wl_resource_destroy(tzsh->res_tzsh);

   EINA_LIST_FREE(polwl->tzlaunchs, tzlaunch)
     wl_resource_destroy(tzlaunch->res_tzlaunch);

   EINA_LIST_FREE(polwl->globals, global)
     wl_global_destroy(global);

   E_FREE_FUNC(polwl->tzpols, eina_hash_free);

#ifdef ENABLE_CYNARA
   if (polwl->p_cynara)
     cynara_finish(polwl->p_cynara);
#endif

   E_FREE(polwl);
}
