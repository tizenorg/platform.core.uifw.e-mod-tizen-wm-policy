#include "e_mod_ws_shell.h"

#include <wayland-server.h>
#include "tizen_ws_shell-server-protocol.h"

typedef struct _WS_Shell_Service
{
   const char *name;
   uint64_t sid;
   E_Client *ec;

   struct wl_resource *resource;
} WS_Shell_Service;

typedef struct _WS_Shell_Region
{
   Eina_Tiler *tiler;

   struct wl_listener destroy_listener;
   struct wl_resource *resource;
} WS_Shell_Region;

typedef struct _WS_Shell_Quickpanel
{
   E_Client *ec;
} WS_Shell_Quickpanel;

static Eina_List *wssh_ifaces = NULL;
static Eina_Hash *wssh_services = NULL;

static void
_e_tizen_ws_shell_region_cb_shell_destroy(struct wl_listener *listener, void *data)
{
   WS_Shell_Region *region = NULL;

   region = container_of(listener, WS_Shell_Region, destroy_listener);
   if (!region) return;

   wl_resource_destroy(region->resource);
}

static void
_e_tizen_ws_shell_region_destroy(struct wl_resource *resource)
{
   WS_Shell_Region *region = NULL;

   region = wl_resource_get_user_data(resource);
   if (!region) return;

   wl_list_remove(&region->destroy_listener.link);
   eina_tiler_free(region->tiler);
   E_FREE(region);
}

static void
_e_tizen_ws_shell_region_cb_destroy(struct wl_client *client EINA_UNUSED,
                                    struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_tizen_ws_shell_region_cb_add(struct wl_client *client EINA_UNUSED,
                                struct wl_resource *resource,
                                int32_t x, int32_t y, int32_t w, int32_t h)
{
   WS_Shell_Region *region = NULL;
   Eina_Tiler *src;
   int area_w = 0, area_h = 0;

   if (!(region = wl_resource_get_user_data(resource)))
       return;

   if (region->tiler)
     {
        eina_tiler_area_size_get(region->tiler, &area_w, &area_h);
        src = eina_tiler_new(area_w, area_h);
        eina_tiler_tile_size_set(src, 1, 1);
        eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});
        eina_tiler_union(region->tiler, src);
        eina_tiler_free(src);
     }
}

static void
_e_tizen_ws_shell_region_cb_subtract(struct wl_client *client EINA_UNUSED,
                                     struct wl_resource *resource,
                                     int32_t x, int32_t y, int32_t w, int32_t h)
{
   WS_Shell_Region *region = NULL;
   Eina_Tiler *src;
   int area_w = 0, area_h = 0;

   if (!(region = wl_resource_get_user_data(resource)))
       return;

   if (region->tiler)
     {
        eina_tiler_area_size_get(region->tiler, &area_w, &area_h);
        src = eina_tiler_new(area_w, area_h);
        eina_tiler_tile_size_set(src, 1, 1);
        eina_tiler_rect_add(src, &(Eina_Rectangle){x, y, w, h});
        eina_tiler_subtract(region->tiler, src);
        eina_tiler_free(src);
     }
}

static const struct tws_region_interface _e_tizen_ws_shell_region_interface =
{
   _e_tizen_ws_shell_region_cb_destroy,
   _e_tizen_ws_shell_region_cb_add,
   _e_tizen_ws_shell_region_cb_subtract,
};


static void
_e_tizen_ws_shell_service_cb_free(void *data)
{
   WS_Shell_Service *service = data;

   if (!service) return;

   wl_resource_destroy(service->resource);
}

static void
_e_tizen_ws_shell_service_destroy(struct wl_resource *resource)
{
   WS_Shell_Service *service = NULL;
   struct wl_resource *res;
   Eina_List *l;

   if (!resource) return;

   service = wl_resource_get_user_data(resource);
   if (!service) return;

   /* TODO: do something for removed service */

   /* send service unregister */
   EINA_LIST_FOREACH(wssh_ifaces, l, res)
     {
        tizen_ws_shell_send_service_unregister(res, service->name);
     }

   eina_hash_del_by_key(wssh_services, service->name);
   E_FREE(service);
}

static void
_e_tizen_ws_shell_service_cb_destroy(struct wl_client *client EINA_UNUSED,
                                     struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_tizen_ws_shell_service_cb_region_set(struct wl_client *client,
                                        struct wl_resource *resource,
                                        int32_t type,
                                        int32_t angle,
                                        struct wl_resource *region_resource)
{
   WS_Shell_Service *service = NULL;
   WS_Shell_Region *region = NULL;

   service = wl_resource_get_user_data(resource);
   if (!service) return;

   region = wl_resource_get_user_data(region_resource);
   if (!region) return;

   /* TODO: process region set */
}

static const struct tws_service_interface _e_tizen_ws_shell_service_interface =
{
   _e_tizen_ws_shell_service_cb_destroy,
   _e_tizen_ws_shell_service_cb_region_set,
};

static void
_e_tizen_ws_shell_quickpanel_destroy(struct wl_resource *resource)
{
   WS_Shell_Quickpanel *quickpanel = NULL;

   quickpanel = wl_resource_get_user_data(resource);
   if (quickpanel)
     E_FREE(quickpanel);
}

static void
_e_tizen_ws_shell_quickpanel_cb_release(struct wl_client *client EINA_UNUSED,
                                        struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_tizen_ws_shell_quickpanel_cb_show(struct wl_client *client,
                                     struct wl_resource *resource)
{
   /* TODO: request quickpanel show */
}

static void
_e_tizen_ws_shell_quickpanel_cb_hide(struct wl_client *client,
                                        struct wl_resource *resource)
{
   /* TODO: request quickpanel hide */
}

static void
_e_tizen_ws_shell_quickpanel_cb_enable(struct wl_client *client,
                                        struct wl_resource *resource)
{
   /* TODO: request quickpanel enable */
}

static void
_e_tizen_ws_shell_quickpanel_cb_disable(struct wl_client *client,
                                        struct wl_resource *resource)
{
   /* TODO: request quickpanel disable */
}

static const struct tws_quickpanel_interface _e_tizen_ws_shell_quickpanel_interface =
{
   _e_tizen_ws_shell_quickpanel_cb_release,
   _e_tizen_ws_shell_quickpanel_cb_show,
   _e_tizen_ws_shell_quickpanel_cb_hide,
   _e_tizen_ws_shell_quickpanel_cb_enable,
   _e_tizen_ws_shell_quickpanel_cb_disable,
};

static void
_e_tizen_ws_shell_destroy(struct wl_resource *resource)
{
   if (!resource) return;

   wssh_ifaces = eina_list_remove(wssh_ifaces, resource);
}

static void
_e_tizen_ws_shell_cb_destroy(struct wl_client *client EINA_UNUSED,
                             struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_tizen_ws_shell_cb_service_create(struct wl_client *client,
                                    struct wl_resource *tizen_ws_shell,
                                    uint32_t id,
                                    uint32_t surface_id,
                                    const char *name)
{
   int version = wl_resource_get_version(tizen_ws_shell);
   struct wl_resource *res;
   WS_Shell_Service *service = NULL;
   uint64_t sid;
   pid_t pid;
   E_Client *ec = NULL;
   Eina_List *l;

   if (eina_hash_find(wssh_services, name) != NULL)
     {
        ERR("Service %s is already created.", name);
        return;
     }

   if (!(res = wl_resource_create(client, &tws_service_interface, version, id)))
     {
        ERR("Could not create tws_service resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   /* TODO: use new id rule */
   wl_client_get_credentials(client, &pid, NULL, NULL);
   sid = e_comp_wl_id_get(surface_id, pid);
   ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, sid);

   service = E_NEW(WS_Shell_Service, 1);
   service->resource = res;
   service->name = eina_stringshare_add(name);
   service->sid = sid;
   service->ec = ec;

   wl_resource_set_implementation(res,
                                  &_e_tizen_ws_shell_service_interface,
                                  service,
                                  _e_tizen_ws_shell_service_destroy);

   eina_hash_add(wssh_services, name, service);

   /* TODO: do something for new service  */

   /* send service register */
   EINA_LIST_FOREACH(wssh_ifaces, l, res)
     {
        tizen_ws_shell_send_service_register(res, name);
     }
}

static void
_e_tizen_ws_shell_cb_region_create(struct wl_client *client,
                                   struct wl_resource *tizen_ws_shell,
                                   uint32_t id)
{
   int version = wl_resource_get_version(tizen_ws_shell);
   struct wl_resource *res;
   WS_Shell_Region *region = NULL;
   int zw = 0, zh = 0;

   region = E_NEW(WS_Shell_Region, 1);

   e_zone_useful_geometry_get(e_zone_current_get(e_comp), NULL, NULL, &zw, &zh);
   region->tiler = eina_tiler_new(zw, zh);
   eina_tiler_tile_size_set(region->tiler, 1, 1);

   if (!(res = wl_resource_create(client, &tws_region_interface, version, id)))
     {
        ERR("Could not create tws_service resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res,
                                  &_e_tizen_ws_shell_region_interface,
                                  region,
                                  _e_tizen_ws_shell_region_destroy);

   region->resource = res;
   region->destroy_listener.notify = _e_tizen_ws_shell_region_cb_shell_destroy;
   wl_resource_add_destroy_listener(tizen_ws_shell, &region->destroy_listener);
}

static void
_e_tizen_ws_shell_cb_quickpanel_get(struct wl_client *client,
                                    struct wl_resource *tizen_ws_shell,
                                    uint32_t id,
                                    uint32_t surface_id)
{
   int version = wl_resource_get_version(tizen_ws_shell);
   struct wl_resource *res;
   WS_Shell_Quickpanel *quickpanel = NULL;
   uint64_t sid;
   pid_t pid;
   E_Client *ec = NULL;

   if (!(res = wl_resource_create(client, &tws_quickpanel_interface, version, id)))
     {
        ERR("Could not create tws_quickpanel resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   /* TODO: use new id rule */
   wl_client_get_credentials(client, &pid, NULL, NULL);
   sid = e_comp_wl_id_get(surface_id, pid);
   ec = e_pixmap_find_client(E_PIXMAP_TYPE_WL, sid);

   quickpanel = E_NEW(WS_Shell_Quickpanel, 1);
   quickpanel->ec = ec;

   wl_resource_set_implementation(res,
                                  &_e_tizen_ws_shell_quickpanel_interface,
                                  quickpanel,
                                  _e_tizen_ws_shell_quickpanel_destroy);
}

static const struct tizen_ws_shell_interface _e_tizen_ws_shell_interface =
{
   _e_tizen_ws_shell_cb_destroy,
   _e_tizen_ws_shell_cb_service_create,
   _e_tizen_ws_shell_cb_region_create,
   _e_tizen_ws_shell_cb_quickpanel_get,
};

static void
_e_tizen_ws_shell_bind_cb(struct wl_client *client,
                          void *data,
                          uint32_t version,
                          uint32_t id)
{
   E_Comp_Data *cdata;
   struct wl_resource *res;

   if (!(cdata = data))
     {
        wl_client_post_no_memory(client);
        return;
     }

   if (!(res = wl_resource_create(client,
                                  &tizen_ws_shell_interface,
                                  version,
                                  id)))
     {
        ERR("Could not create tizen_ws_shell resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res,
                                  &_e_tizen_ws_shell_interface,
                                  cdata,
                                  _e_tizen_ws_shell_destroy);

   wssh_ifaces = eina_list_append(wssh_ifaces, res);
}

Eina_Bool
e_mod_ws_shell_init(void)
{
   E_Comp_Data *cdata;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp, EINA_FALSE);

   cdata = e_comp->wl_comp_data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata->wl.disp, EINA_FALSE);

  if (!wl_global_create(cdata->wl.disp, &tizen_ws_shell_interface, 1,
                        cdata, _e_tizen_ws_shell_bind_cb))
    {
       ERR("Could not add tizen_wl_shell to wayland globals: %m");
       return EINA_FALSE;
    }

  wssh_services = eina_hash_string_superfast_new(NULL);

  return EINA_TRUE;
}

void
e_mod_ws_shell_shutdown(void)
{
   struct wl_resource *res;

   E_FREE_FUNC(wssh_services, _e_tizen_ws_shell_service_cb_free);

   EINA_LIST_FREE(wssh_ifaces, res)
      wl_resource_destroy(res);

}
