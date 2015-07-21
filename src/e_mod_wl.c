#include "e_mod_wl.h"
#include "e_mod_notification.h"

#include <wayland-server.h>
#include <tizen-extension-server-protocol.h>

typedef struct _Pol_Wayland
{
   E_Pixmap *ep;
   Eina_List *visibility_list;
   Eina_List *position_list;
} Pol_Wayland;

typedef struct _Notification_Level
{
   int32_t level;
   struct wl_resource *interface;
   struct wl_resource *surface;
} Notification_Level;

typedef struct _Policy_Conformant
{
   struct wl_resource *interface;
   struct wl_resource *surface;
} Policy_Conformant;

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

typedef struct _Window_Screen_Mode
{
   uint32_t mode;
   struct wl_resource *surface;
   struct wl_resource *interface;
} Window_Screen_Mode;

static Eina_Hash *hash_pol_wayland = NULL;
static Eina_Hash *hash_notification_levels = NULL;
static Eina_Hash *hash_policy_conformants = NULL;
static Eina_Hash *hash_window_screen_modes = NULL;
static Eina_List *_window_screen_modes = NULL;
static Eina_List *_handlers = NULL;
static Eina_List *_hooks = NULL;

static Pol_Wayland*
_pol_wayland_get_info(E_Pixmap *ep)
{
   Pol_Wayland *pn;

   EINA_SAFETY_ON_NULL_RETURN_VAL(hash_pol_wayland, NULL);

   pn = eina_hash_find(hash_pol_wayland, &ep);
   if (!pn)
     {
        pn = E_NEW(Pol_Wayland, 1);
        EINA_SAFETY_ON_NULL_RETURN_VAL(pn, NULL);

        pn->ep = ep;
        eina_hash_add(hash_pol_wayland, &ep, pn);
     }

   return pn;
}

static void
_pol_wayland_role_handle(E_Client *ec, const char* role)
{
   /* TODO: support multiple roles */

   EINA_SAFETY_ON_NULL_RETURN(ec->frame);
   EINA_SAFETY_ON_NULL_RETURN(role);

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
}

static void
_window_screen_mode_apply(void)
{
  //traversal e_client loop
  // if  e_client is visible then apply screen_mode
  // if all e_clients are default mode then set default screen_mode
  return;
}

static void
_pol_surface_parent_set(E_Client *ec, struct wl_resource *parent_resource)
{
   E_Pixmap *pp;
   E_Client *pc;
   Ecore_Window pwin = 0;

   if (!parent_resource)
     {
        ec->icccm.fetch.transient_for = EINA_FALSE;
        ec->icccm.transient_for = 0;
        if (ec->parent)
          {
             ec->parent->transients =
                eina_list_remove(ec->parent->transients, ec);
             if (ec->parent->modal == ec) ec->parent->modal = NULL;
             ec->parent = NULL;
          }
        return;
     }
   else if (!(pp = wl_resource_get_user_data(parent_resource)))
     {
        ERR("Could not get parent resource pixmap");
        return;
     }

   pwin = e_pixmap_window_get(pp);

   /* find the parent client */
   if (!(pc = e_pixmap_client_get(pp)))
     pc = e_pixmap_find_client(E_PIXMAP_TYPE_WL, pwin);

   e_pixmap_parent_window_set(ec->pixmap, pwin);

   /* If we already have a parent, remove it */
   if (ec->parent)
     {
        if (pc != ec->parent)
          {
             ec->parent->transients =
                eina_list_remove(ec->parent->transients, ec);
             if (ec->parent->modal == ec) ec->parent->modal = NULL;
             ec->parent = NULL;
          }
        else
          pc = NULL;
     }

   if ((pc) && (pc != ec) &&
       (eina_list_data_find(pc->transients, ec) != ec))
     {
        pc->transients = eina_list_append(pc->transients, ec);
        ec->parent = pc;
     }

   ec->icccm.fetch.transient_for = EINA_TRUE;
   ec->icccm.transient_for = pwin;
}

static void
_e_tizen_visibility_destroy(struct wl_resource *resource)
{
   E_Pixmap *ep;
   Pol_Wayland *pn;

   ep = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   pn = _pol_wayland_get_info(ep);
   EINA_SAFETY_ON_NULL_RETURN(pn);

   pn->visibility_list = eina_list_remove(pn->visibility_list, resource);
}

static void
_e_tizen_visibility_cb_destroy(struct wl_client *client, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct tizen_visibility_interface _e_tizen_visibility_interface =
{
   _e_tizen_visibility_cb_destroy
};

static void
_e_tizen_position_destroy(struct wl_resource *resource)
{
   E_Pixmap *ep;
   Pol_Wayland *pn;

   ep = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   pn = _pol_wayland_get_info(ep);
   EINA_SAFETY_ON_NULL_RETURN(pn);

   pn->position_list = eina_list_remove(pn->position_list, resource);
}

static void
_e_tizen_position_cb_destroy(struct wl_client *client, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_tizen_position_cb_set(struct wl_client *client,
                         struct wl_resource *tizen_position,
                         int32_t x, int32_t y)
{
   E_Pixmap *ep;
   E_Client *ec;

   ep = wl_resource_get_user_data(tizen_position);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   ec = e_pixmap_client_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);
   
   if (!ec->lock_client_location)
     {
        ec->x = ec->client.x = x;
        ec->y = ec->client.y = y;
     }
   //e_client_util_move_without_frame(ec, x, y);
}

static const struct tizen_position_interface _e_tizen_position_interface =
{
   _e_tizen_position_cb_destroy,
   _e_tizen_position_cb_set,
};

static void
_e_tizen_policy_cb_visibility_get(struct wl_client *client, struct wl_resource *policy, uint32_t id, struct wl_resource *surface_resource)
{
   int version = wl_resource_get_version(policy);
   struct wl_resource *res;
   E_Pixmap *ep;
   Pol_Wayland *pn;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   pn = _pol_wayland_get_info(ep);
   EINA_SAFETY_ON_NULL_RETURN(pn);

   res = wl_resource_create(client, &tizen_visibility_interface, version, id);
   if (res == NULL)
     {
        wl_client_post_no_memory(client);
        return;
     }

   pn->visibility_list = eina_list_append(pn->visibility_list, res);

   wl_resource_set_implementation(res, &_e_tizen_visibility_interface,
                                  ep, _e_tizen_visibility_destroy);
}

static void
_e_tizen_policy_cb_position_get(struct wl_client *client, struct wl_resource *tizen_policy, uint32_t id, struct wl_resource *surface_resource)
{
   int version = wl_resource_get_version(tizen_policy);
   struct wl_resource *res;
   E_Pixmap *ep;
   Pol_Wayland *pn;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   pn = _pol_wayland_get_info(ep);
   EINA_SAFETY_ON_NULL_RETURN(pn);

   res = wl_resource_create(client, &tizen_position_interface, version, id);
   if (!res)
     {
        wl_client_post_no_memory(client);
        return;
     }

   pn->position_list = eina_list_append(pn->position_list, res);
   wl_resource_set_implementation(res,
                                  &_e_tizen_position_interface,
                                  ep,
                                  _e_tizen_position_destroy);

}

static void
_e_tizen_policy_cb_activate(struct wl_client *client, struct wl_resource *policy, struct wl_resource *surface_resource)
{
   int version = wl_resource_get_version(policy);
   struct wl_resource *res;
   E_Pixmap *ep;
   E_Client *ec;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   ec = e_pixmap_client_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   if ((!starting) && (!ec->focused))
     e_client_activate(ec, EINA_TRUE);
   else
     evas_object_raise(ec->frame);
}

static void
_e_tizen_policy_cb_lower(struct wl_client *client, struct wl_resource *policy, struct wl_resource *surface_resource)
{
   E_Pixmap *ep;
   E_Client *ec, *below = NULL;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   ec = e_pixmap_client_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   below = ec;
   while ((below = e_client_below_get(below)))
     {
        if ((e_client_util_ignored_get(below)) ||
            (below->iconic)) continue;

        break;
     }

   evas_object_lower(ec->frame);

   if ((!below) || (!ec->focused)) return;
   evas_object_focus_set(below->frame, 1);
}

static void
_e_tizen_policy_cb_focus_skip_set(struct wl_client *client, struct wl_resource *policy, struct wl_resource *surface_resource)
{
   E_Pixmap *ep;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   ec = e_pixmap_client_get(ep);
   if (ec)
     {
        if (ec->icccm.accepts_focus)
          {
             ec->icccm.accepts_focus = ec->icccm.take_focus = 0;
             EC_CHANGED(ec);
          }
     }
   else
     {
        cdata = e_pixmap_cdata_get(ep);
        EINA_SAFETY_ON_NULL_RETURN(cdata);
        cdata->accepts_focus = 0;
     }
}

static void
_e_tizen_policy_cb_focus_skip_unset(struct wl_client *client, struct wl_resource *policy, struct wl_resource *surface_resource)
{
   E_Pixmap *ep;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   ec = e_pixmap_client_get(ep);
   if (ec)
     {
        if (!ec->icccm.accepts_focus)
          {
             ec->icccm.accepts_focus = ec->icccm.take_focus = 1;
             EC_CHANGED(ec);
          }
     }
   else
     {
        cdata = e_pixmap_cdata_get(ep);
        EINA_SAFETY_ON_NULL_RETURN(cdata);
        cdata->accepts_focus = 1;
     }
}

static void
_e_tizen_policy_cb_role_set(struct wl_client *client EINA_UNUSED, struct wl_resource *tizen_policy EINA_UNUSED, struct wl_resource *surface_resource, const char *role)
{
   E_Pixmap *ep;
   E_Client *ec;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   ec = e_pixmap_client_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   eina_stringshare_replace(&ec->icccm.window_role, role);
   _pol_wayland_role_handle(ec, role);
}

static void
_e_tizen_policy_cb_conformant_set(struct wl_client *client, struct wl_resource *policy, struct wl_resource *surface_resource)
{
   E_Pixmap *ep;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;
   Policy_Conformant *pn;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   ec = e_pixmap_client_get(ep);
   if (ec)
     {
        if (!ec->comp_data->conformant)
          {
             ec->comp_data->conformant = 1;
             EC_CHANGED(ec);
          }
     }
   else
     {
        cdata = e_pixmap_cdata_get(ep);
        EINA_SAFETY_ON_NULL_RETURN(cdata);
        cdata->conformant = 1;
     }

   pn = eina_hash_find(hash_policy_conformants, &surface_resource);
   if (!pn)
     {
        pn = E_NEW(Policy_Conformant, 1);
        EINA_SAFETY_ON_NULL_RETURN(pn);
        eina_hash_add(hash_policy_conformants, &surface_resource, pn);
     }

   pn->interface = policy;
   pn->surface = surface_resource;
}

static void
_e_tizen_policy_cb_conformant_unset(struct wl_client *client, struct wl_resource *policy, struct wl_resource *surface_resource)
{
   E_Pixmap *ep;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;
   Policy_Conformant *pn;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   ec = e_pixmap_client_get(ep);
   if (ec)
     {
        if (ec->comp_data->conformant)
          {
             ec->comp_data->conformant = 0;
             EC_CHANGED(ec);
          }
     }
   else
     {
        cdata = e_pixmap_cdata_get(ep);
        EINA_SAFETY_ON_NULL_RETURN(cdata);
        cdata->conformant = 0;
     }

   pn = eina_hash_find(hash_policy_conformants, &surface_resource);
   if (pn)
     eina_hash_del_by_key(hash_policy_conformants, &surface_resource);
}

static void
_e_tizen_policy_cb_conformant_get(struct wl_client *client, struct wl_resource *policy, struct wl_resource *surface_resource)
{
   E_Pixmap *ep;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;
   Policy_Conformant *pn;

   ep = wl_resource_get_user_data(surface_resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   pn = eina_hash_find(hash_policy_conformants, &surface_resource);
   if (pn)
     {
        ec = e_pixmap_client_get(ep);
        if (ec)
          tizen_policy_send_conformant(pn->interface, surface_resource, ec->comp_data->conformant);
        else
          {
             cdata = e_pixmap_cdata_get(ep);
             EINA_SAFETY_ON_NULL_RETURN(cdata);
             tizen_policy_send_conformant(pn->interface, surface_resource, cdata->conformant);
          }
     }
}

static void
_e_tizen_policy_cb_notification_level_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface, int32_t level)
{
   E_Pixmap *ep;
   E_Client *ec;
   Notification_Level *nl;

   /* get the pixmap from this surface so we can find the client */
   if (!(ep = wl_resource_get_user_data(surface)))
     {
        wl_resource_post_error(surface,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Pixmap Set On Surface");
        return;
     }

   /* make sure it's a wayland pixmap */
   if (e_pixmap_type_get(ep) != E_PIXMAP_TYPE_WL) return;

   /* find the client for this pixmap */
   ec = e_pixmap_client_get(ep);
   if (ec)
     {
        /* remove not processed level set */
        nl = eina_hash_find(hash_notification_levels, &surface);
        if (nl) eina_hash_del_by_key(hash_notification_levels, &surface);

        e_mod_pol_notification_level_apply(ec, level);

        /* Add other error handling code on notification send done. */
        tizen_policy_send_notification_done(resource,
                                            surface,
                                            level,
                                            TIZEN_POLICY_ERROR_STATE_NONE);
     }
   else
     {
         nl = eina_hash_find(hash_notification_levels, &surface);
         if (!nl)
           {
              nl = E_NEW(Notification_Level, 1);
              EINA_SAFETY_ON_NULL_RETURN(nl);
              eina_hash_add(hash_notification_levels, &surface, nl);
           }

         nl->level = level;
         nl->interface = resource;
         nl->surface = surface;
     }
}

static void
_e_tizen_policy_cb_transient_for_set(struct wl_client *client, struct wl_resource *resource, uint32_t child_id, uint32_t parent_id)
{
   E_Client *ec, *pc;
   struct wl_resource *parent_res;

   DBG("chid_id: %" PRIu32 ", parent_id: %" PRIu32, child_id, parent_id);

   ec = e_pixmap_find_client_by_res_id(child_id);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   pc = e_pixmap_find_client_by_res_id(parent_id);
   EINA_SAFETY_ON_NULL_RETURN(pc);
   EINA_SAFETY_ON_NULL_RETURN(pc->comp_data);

   parent_res = pc->comp_data->surface;
   _pol_surface_parent_set(ec, parent_res);
   tizen_policy_send_transient_for_done(resource, child_id);

   EC_CHANGED(ec);
}

static void
_e_tizen_policy_cb_transient_for_unset(struct wl_client *client, struct wl_resource *resource, uint32_t child_id)
{
   E_Client *ec;

   DBG("chid_id: %" PRIu32, child_id);

   ec = e_pixmap_find_client_by_res_id(child_id);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   _pol_surface_parent_set(ec, NULL);
   tizen_policy_send_transient_for_done(resource, child_id);

   EC_CHANGED(ec);
}

static void
_e_tizen_policy_cb_window_screen_mode_set(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface, uint32_t mode)
{
   E_Pixmap *ep;
   Window_Screen_Mode *wsm;

   /* get the pixmap from this surface so we can find the client */
   if (!(ep = wl_resource_get_user_data(surface)))
     {
        wl_resource_post_error(surface,
                               WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "No Pixmap Set On Surface");
        return;
     }

   /* make sure it's a wayland pixmap */
   if (e_pixmap_type_get(ep) != E_PIXMAP_TYPE_WL) return;

   wsm = eina_hash_find(hash_window_screen_modes, &surface);
   if (!wsm)
     {
        wsm = E_NEW(Window_Screen_Mode, 1);
        EINA_SAFETY_ON_NULL_RETURN(wsm);
        eina_hash_add(hash_window_screen_modes, &surface, wsm);
        _window_screen_modes = eina_list_append(_window_screen_modes, wsm);
     }

   wsm->mode = mode;
   wsm->surface = surface;
   wsm->interface = resource;

   _window_screen_mode_apply();

   /* Add other error handling code on window_screen send done. */
   tizen_policy_send_window_screen_mode_done(resource, surface, mode, TIZEN_POLICY_ERROR_STATE_NONE);
}

static void
_e_tizen_policy_cb_subsurface_place_below_parent(struct wl_client *client, struct wl_resource *resource, struct wl_resource *subsurface)
{
   E_Client *ec;
   E_Client *epc;
   E_Comp_Wl_Subsurf_Data *sdata;

   /* try to get the client from resource data */
   if (!(ec = wl_resource_get_user_data(subsurface)))
     {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Invalid subsurface");
        return;
     }

   sdata = ec->comp_data->sub.data;
   if (!sdata)
     {
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "Not subsurface");
        return;
     }

   epc = sdata->parent;
   EINA_SAFETY_ON_NULL_RETURN(epc);

   /* check if a subsurface has already placed below a parent */
   if (eina_list_data_find(epc->comp_data->sub.below_list, ec)) return;

   epc->comp_data->sub.list = eina_list_remove(epc->comp_data->sub.list, ec);
   epc->comp_data->sub.list_pending = eina_list_remove(epc->comp_data->sub.list_pending, ec);
   epc->comp_data->sub.below_list_pending = eina_list_append(epc->comp_data->sub.below_list_pending, ec);
   epc->comp_data->sub.list_changed = EINA_TRUE;
}

static const struct tizen_policy_interface _e_tizen_policy_interface =
{
   _e_tizen_policy_cb_visibility_get,
   _e_tizen_policy_cb_position_get,
   _e_tizen_policy_cb_activate,
   _e_tizen_policy_cb_lower,
   _e_tizen_policy_cb_focus_skip_set,
   _e_tizen_policy_cb_focus_skip_unset,
   _e_tizen_policy_cb_role_set,
   _e_tizen_policy_cb_conformant_set,
   _e_tizen_policy_cb_conformant_unset,
   _e_tizen_policy_cb_conformant_get,
   _e_tizen_policy_cb_notification_level_set,
   _e_tizen_policy_cb_transient_for_set,
   _e_tizen_policy_cb_transient_for_unset,
   _e_tizen_policy_cb_window_screen_mode_set,
   _e_tizen_policy_cb_subsurface_place_below_parent,
};

static void
_e_tizen_policy_cb_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp_Data *cdata;
   struct wl_resource *res;

   if (!(cdata = data))
     {
        wl_client_post_no_memory(client);
        return;
     }

   if (!(res = wl_resource_create(client, &tizen_policy_interface, version, id)))
     {
        ERR("Could not create scaler resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_tizen_policy_interface, cdata, NULL);
}

static void
_hook_client_del(void *d EINA_UNUSED, E_Client *ec)
{
   Window_Screen_Mode *wsm;
   E_Comp_Client_Data *cdata;
   struct wl_resource *surface;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   cdata = e_pixmap_cdata_get(ec->pixmap);
   EINA_SAFETY_ON_NULL_RETURN(cdata);
   surface = cdata->wl_surface;
   EINA_SAFETY_ON_NULL_RETURN(surface);

   //remove window_screen_mode from hash
   wsm = eina_hash_find(hash_window_screen_modes, &surface);
   if (wsm)
     {
        _window_screen_modes =  eina_list_remove(_window_screen_modes, wsm);
        eina_hash_del_by_key(hash_window_screen_modes, &surface);
     }
}

static Eina_Bool
_cb_client_visibility_change(void *data EINA_UNUSED,
                             int type   EINA_UNUSED,
                             void      *event)
{
   //E_Event_Client *ev;
   //ev = event;

   _window_screen_mode_apply();

   return ECORE_CALLBACK_PASS_ON;
}

Eina_Bool
e_mod_pol_wl_init(void)
{
   E_Comp_Data *cdata;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp, EINA_FALSE);

   cdata = e_comp->wl_comp_data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata->wl.disp, EINA_FALSE);

   if (!wl_global_create(cdata->wl.disp,
                         &tizen_policy_interface,
                         1,
                         cdata,
                         _e_tizen_policy_cb_bind))
     {
        ERR("Could not add tizen_policy to wayland globals: %m");
        return EINA_FALSE;
     }

   hash_pol_wayland = eina_hash_pointer_new(free);
   hash_notification_levels = eina_hash_pointer_new(free);
   hash_policy_conformants = eina_hash_pointer_new(free);
   hash_window_screen_modes = eina_hash_pointer_new(free);

   E_CLIENT_HOOK_APPEND(_hooks, E_CLIENT_HOOK_DEL,
                        _hook_client_del, NULL);

   E_LIST_HANDLER_APPEND(_handlers, E_EVENT_CLIENT_VISIBILITY_CHANGE,
                         _cb_client_visibility_change, NULL);

   return EINA_TRUE;
}

void
e_mod_pol_wl_shutdown(void)
{
   E_FREE_FUNC(hash_pol_wayland, eina_hash_free);
   E_FREE_FUNC(hash_notification_levels, eina_hash_free);
   E_FREE_FUNC(hash_policy_conformants, eina_hash_free);

   eina_list_free(_window_screen_modes);
   E_FREE_LIST(_hooks, e_client_hook_del);
   E_FREE_LIST(_handlers, ecore_event_handler_del);
   E_FREE_FUNC(hash_window_screen_modes, eina_hash_free);
}

void
e_mod_pol_wl_client_del(E_Client *ec)
{
   Pol_Wayland *pn;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(hash_pol_wayland);

   if (!ec->pixmap) return;

   pn = eina_hash_find(hash_pol_wayland, &ec->pixmap);
   if (!pn) return;

   eina_hash_del_by_key(hash_pol_wayland, &ec->pixmap);
}

void
e_mod_pol_wl_visibility_send(E_Client *ec, int visibility)
{
   Pol_Wayland *pn;
   Eina_List *l;
   struct wl_resource *resource;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->pixmap);
   EINA_SAFETY_ON_NULL_RETURN(hash_pol_wayland);

   pn = eina_hash_find(hash_pol_wayland, &ec->pixmap);
   if (!pn) return;

   EINA_LIST_FOREACH(pn->visibility_list, l, resource)
     {
        tizen_visibility_send_notify(resource, visibility);
     }
}

void
e_mod_pol_wl_position_send(E_Client *ec)
{
   Pol_Wayland *pn;
   Eina_List *l;
   struct wl_resource *resource;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->pixmap);
   EINA_SAFETY_ON_NULL_RETURN(hash_pol_wayland);

   pn = eina_hash_find(hash_pol_wayland, &ec->pixmap);
   if (!pn) return;

   EINA_LIST_FOREACH(pn->position_list, l, resource)
     {
        tizen_position_send_changed(resource, ec->x, ec->y);
     }
}

void
e_mod_pol_wl_notification_level_fetch(E_Client *ec)
{
   Notification_Level *nl;
   E_Comp_Client_Data *cdata;
   struct wl_resource *surface;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   cdata = e_pixmap_cdata_get(ec->pixmap);
   EINA_SAFETY_ON_NULL_RETURN(cdata);
   surface = cdata->wl_surface;
   EINA_SAFETY_ON_NULL_RETURN(surface);

   nl = eina_hash_find(hash_notification_levels, &surface);
   if (nl)
     {
        e_mod_pol_notification_level_apply(ec, nl->level);

        // Add other error handling code on notification send done.
        tizen_policy_send_notification_done(nl->interface,
                                            nl->surface,
                                            nl->level,
                                            TIZEN_POLICY_ERROR_STATE_NONE);

        eina_hash_del_by_key(hash_notification_levels, &surface);
     }
}

void
e_mod_pol_wl_keyboard_geom_broadcast(E_Client *ec)
{
   E_Client *ec2;
   E_Comp_Client_Data *cdata;
   struct wl_resource *surface;
   Policy_Conformant *pn;
   Eina_Bool res;

   E_CLIENT_REVERSE_FOREACH(e_comp, ec2)
     {
        res = e_client_util_ignored_get(ec2);
        if (res) continue;

        res = e_mod_pol_client_is_conformant(ec2);
        if (!res) continue;

        cdata = e_pixmap_cdata_get(ec->pixmap);
        if (!cdata) continue;

        surface = cdata->wl_surface;
        if (!surface) continue;

        pn = eina_hash_find(hash_policy_conformants, &surface);
        if (!pn) continue;

        tizen_policy_send_conformant_area(pn->interface,
                                          pn->surface,
                                          TIZEN_POLICY_CONFORMANT_PART_KEYBOARD,
                                          ec2->visible,
                                          ec->x,
                                          ec->y,
                                          ec->client.w,
                                          ec->client.h);
     }
}
