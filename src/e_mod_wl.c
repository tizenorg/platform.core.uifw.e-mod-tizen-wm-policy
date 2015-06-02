#include "e_mod_wl.h"

#include <wayland-server.h>
#include "e_tizen_policy_server_protocol.h"

typedef struct _Pol_Wayland
{
   E_Pixmap *ep;
   Eina_List *visibility_list;
} Pol_Wayland;

static Eina_Hash *hash_pol_wayland = NULL;

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
      ec->icccm.accepts_focus = ec->icccm.take_focus = 0;
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
      ec->icccm.accepts_focus = ec->icccm.take_focus = 1;
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

   hash_pol_wayland = eina_hash_pointer_new(free);

   return EINA_TRUE;
}

void
e_mod_pol_wl_shutdown(void)
{
   E_FREE_FUNC(hash_pol_wayland, eina_hash_free);
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
