#include "e_mod_wl.h"
#include "e_mod_notification.h"

#include <wayland-server.h>
#include "e_tizen_policy_server_protocol.h"
#include "tizen_notification-server-protocol.h"

typedef struct _Pol_Wayland
{
   E_Pixmap *ep;
   Eina_List *visibility_list;
} Pol_Wayland;

typedef struct _Notification_Level
{
   int32_t level;
   struct wl_resource *interface;
   struct wl_resource *surface;
} Notification_Level;

static Eina_Hash *hash_pol_wayland = NULL;
static Eina_Hash *hash_notification_levels = NULL;
static Eina_Hash *hash_notification_interfaces = NULL;

static Pol_Wayland*
_pol_wayland_get_info (E_Pixmap *ep)
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
_e_tizen_policy_cb_visibility_get(struct wl_client *client,
                                  struct wl_resource *policy,
                                  uint32_t id,
                                  struct wl_resource *surface_resource)
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
_e_tizen_policy_cb_activate(struct wl_client *client,
                            struct wl_resource *policy,
                            struct wl_resource *surface_resource)
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
_e_tizen_policy_cb_lower(struct wl_client *client,
                         struct wl_resource *policy,
                         struct wl_resource *surface_resource)
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
_e_tizen_policy_cb_position_set(struct wl_client *client,
                                struct wl_resource *policy,
                                struct wl_resource *surface_resource,
                                int32_t x, int32_t y)
{
   E_Pixmap *ep;
   E_Client *ec;

   ep = wl_resource_get_user_data(surface_resource);
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

static void
_e_tizen_policy_cb_focus_skip_set(struct wl_client *client,
                                struct wl_resource *policy,
                                struct wl_resource *surface_resource)
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
_e_tizen_policy_cb_focus_skip_unset(struct wl_client *client,
                                struct wl_resource *policy,
                                struct wl_resource *surface_resource)
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

static const struct tizen_policy_interface _e_tizen_policy_interface =
{
   _e_tizen_policy_cb_visibility_get,
   _e_tizen_policy_cb_activate,
   _e_tizen_policy_cb_lower,
   _e_tizen_policy_cb_position_set,
   _e_tizen_policy_cb_focus_skip_set,
   _e_tizen_policy_cb_focus_skip_unset,
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
_e_tizen_notification_set_level_cb(struct wl_client   *client,
                                   struct wl_resource *resource,
                                   struct wl_resource *surface_resource,
                                   uint32_t            level)
{
   E_Pixmap *ep;
   E_Client *ec;
   Notification_Level *nl;

   /* get the pixmap from this surface so we can find the client */
   if (!(ep = wl_resource_get_user_data(surface_resource)))
     {
        wl_resource_post_error(surface_resource,
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
        nl = eina_hash_find(hash_notification_levels, &surface_resource);
        if (nl) eina_hash_del_by_key(hash_notification_levels, &surface_resource);

        e_mod_pol_notification_level_apply(ec, level);

        /* Add other error handling code on notification send done. */
        tizen_notification_send_done(resource, surface_resource, level, TIZEN_NOTIFICATION_ERROR_STATE_NONE);
     }
   else
     {
         nl = eina_hash_find(hash_notification_levels, &surface_resource);
         if (!nl)
           {
              nl = E_NEW(Notification_Level, 1);
              EINA_SAFETY_ON_NULL_RETURN(nl);
              eina_hash_add(hash_notification_levels, &surface_resource, nl);
           }

         nl->level = level;
         nl->interface = resource;
         nl->surface = surface_resource;
     }
}

static const struct tizen_notification_interface _e_tizen_notification_interface =
{
   _e_tizen_notification_set_level_cb
};

static void
_e_tizen_notification_destroy(struct wl_resource *resource)
{
   struct wl_resource *noti_interface = NULL;

   if (!resource) return;

   noti_interface = eina_hash_find(hash_notification_interfaces, &resource);
   if (!noti_interface) return;

   eina_hash_del_by_key(hash_notification_interfaces, &resource);
}

static void
_e_tizen_notification_bind_cb(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp_Data *cdata;
   struct wl_resource *res;
   struct wl_resource *noti_interface;

   if (!(cdata = data))
     {
        wl_client_post_no_memory(client);
        return;
     }

   if (!(res = wl_resource_create(client, &tizen_notification_interface, version, id)))
     {
        ERR("Could not create tizen_notification resource: %m");
        wl_client_post_no_memory(client);
        return;
     }
   wl_resource_set_implementation(res, &_e_tizen_notification_interface, cdata, _e_tizen_notification_destroy);

   noti_interface = eina_hash_find(hash_notification_interfaces, &res);
   if (!noti_interface)
     eina_hash_add(hash_notification_interfaces, &res, res);
}

Eina_Bool
e_mod_pol_wl_init(void)
{
   E_Comp_Data *cdata;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp, EINA_FALSE);

   cdata = e_comp->wl_comp_data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata->wl.disp, EINA_FALSE);

   if (!wl_global_create(cdata->wl.disp, &tizen_policy_interface, 1,
                         cdata, _e_tizen_policy_cb_bind))
     {
        ERR("Could not add tizen_policy to wayland globals: %m");
        return EINA_FALSE;
     }

  if (!wl_global_create(cdata->wl.disp, &tizen_notification_interface, 1,
                        cdata, _e_tizen_notification_bind_cb))
    {
       ERR("Could not add tizen_notification to wayland globals: %m");
       return EINA_FALSE;
    }

   hash_pol_wayland = eina_hash_pointer_new(free);
   hash_notification_levels = eina_hash_pointer_new(free);
   hash_notification_interfaces = eina_hash_pointer_new(NULL); // do not free by hash_del by key.

   return EINA_TRUE;
}

void
e_mod_pol_wl_shutdown(void)
{
   E_FREE_FUNC(hash_pol_wayland, eina_hash_free);
   E_FREE_FUNC(hash_notification_levels, eina_hash_free);
   E_FREE_FUNC(hash_notification_interfaces, eina_hash_free);
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
e_mod_pol_wl_notification_level_fetch(E_Client *ec)
{
   Notification_Level *nl;
   E_Comp_Client_Data *cdata;
   struct wl_resource *surface;
   struct wl_resource *noti_interface;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   cdata = e_pixmap_cdata_get(ec->pixmap);
   EINA_SAFETY_ON_NULL_RETURN(cdata);
   surface = cdata->wl_surface;
   EINA_SAFETY_ON_NULL_RETURN(surface);

   nl = eina_hash_find(hash_notification_levels, &surface);
   if (nl)
     {
        noti_interface = eina_hash_find(hash_notification_interfaces, &(nl->interface));
        if (noti_interface)
          {
             e_mod_pol_notification_level_apply(ec, nl->level);

             //Add other error handling code on notification send done.
             tizen_notification_send_done(nl->interface,
                                          nl->surface,
                                          nl->level,
                                          TIZEN_NOTIFICATION_ERROR_STATE_NONE);
          }

        eina_hash_del_by_key(hash_notification_levels, &surface);
     }
}
