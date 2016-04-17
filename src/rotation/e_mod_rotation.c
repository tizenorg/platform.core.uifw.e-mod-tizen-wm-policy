/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * This file is a modified version of BSD licensed file and
 * licensed under the Flora License, Version 1.1 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Please, see the COPYING file for the original copyright owner and
 * license.
 */
#include "e_mod_rotation.h"
#include "e_mod_utils.h"

#ifdef HAVE_AUTO_ROTATION
#include "e_mod_sensord.h"
#endif

#ifdef DBG
#undef DBG
#endif
#ifdef INF
#undef INF
#endif
#ifdef WRN
#undef WRN
#endif
#ifdef ERR
#undef ERR
#endif
#ifdef CRI
#undef CRI
#endif

#define DBG(...)     EINA_LOG_DOM_DBG(_log_dom, __VA_ARGS__)
#define INF(...)     EINA_LOG_DOM_INFO(_log_dom, __VA_ARGS__)
#define WRN(...)     EINA_LOG_DOM_WARN(_log_dom, __VA_ARGS__)
#define ERR(...)     EINA_LOG_DOM_ERR(_log_dom, __VA_ARGS__)
#define CRI(...)     EINA_LOG_DOM_CRIT(_log_dom, __VA_ARGS__)

#define RLOG(LOG, r, f, x...) \
   LOG("ROT|ec:%08p|name:%10s|"f, (r?r->ec:NULL), (r?r->ec?(r->ec->icccm.name?:""):"":""), ##x)

#define RDBG(r, f, x...)   RLOG(DBG, r, f, ##x)
#define RINF(r, f, x...)   RLOG(INF, r, f, ##x)
#define RERR(r, f, x...)   RLOG(ERR, r, f, ##x)

#define CHANGED(h)               \
do {                             \
   h = EINA_TRUE;                \
   p_rot->changed = EINA_TRUE;   \
} while (0)

typedef struct _Pol_Rotation     Pol_Rotation;
typedef struct _Rot_Zone_Data    Rot_Zone_Data;
typedef struct _Rot_Client_Data  Rot_Client_Data;

struct _Pol_Rotation
{
   struct wl_global *global;

   Rot_Zone_Data *zdata;
   Eina_Hash *zdata_hash;
   Eina_Hash *cdata_hash;

   Eina_List *events;
   Eina_List *client_hooks;
   Eina_List *comp_hooks;

   Ecore_Idle_Enterer  *idle_enterer;
   Ecore_Idle_Exiter   *idle_exiter;

   Eina_List *update_list;

   Eina_Bool changed;
};

struct _Rot_Zone_Data
{
   E_Zone *zone;
   Rot_Idx curr, prev, next, reserve;

   struct
   {
      Rot_Idx curr, prev, next, reserve;
   } angle;

   int block_count;

   Eina_Bool unknown_state;
   Eina_Bool wait_for_done;
   Eina_Bool pending;
   Eina_Bool changed;
};

struct _Rot_Client_Data
{
   E_Client *ec;
   struct wl_resource *resource;

   Rot_Idx curr, prev, next, reserve;
   Rot_Type type;

   Rot_Idx preferred_rot;
   int available_list;

   struct
   {
      Eina_Bool geom;
      Rot_Idx rot;
   } changes;
};

const static char          *_dom_name = "e_rot";

static Pol_Rotation        *p_rot = NULL;
static int                  _init_count = 0;
static int                  _log_dom = -1;

static void
_rot_zone_change_list_add(Rot_Zone_Data *zdata)
{
   if ((!p_rot->changes.zones) ||
       (!eina_list_data_find(p_rot->changes.zones, zone)))
     p_rot->changes.zones = eina_list_append(p_rot->changes.zones, zone);
}

static E_Zone *
_rot_zone_change_list_find(E_Zone *zone)
{
   return eina_list_data_find(p_rot->changes.zones, zone)
}

static void
_rot_zone_change_list_clear(void)
{
   E_FREE_FUNC(p_rot->changes.zones, eina_list_free);
}

static Rot_Client_Data *
_rot_client_add(E_Client *ec, struct wl_resource *resource)
{
   Rot_Client_Data *rdata;

   rdata = calloc(1, sizeof(Rot_Client_Data));
   if (EINA_UNLIKELY(!rdata))
     return NULL;

   rdata->ec = ec;
   rdata->resource = resource;
   rdata->type = ROT_TYPE_UNKNOWN;

   if (!p_rot->cdata_hash)
     p_rot->cdata_hash = eina_hash_pointer_new((Eina_Free_Cb)free);

   eina_hash_add(p_rot->cdata_hash, &ec, rdata);

   return rdata;
}

static void
_rot_client_del(Rot_Client_Data *rdata)
{
   eina_hash_del(p_rot->cdata_hash, &rdata->ec, rdata);
}

static void
_rot_client_clear(void)
{
   E_FREE_FUNC(p_rot->cdata_hash, eina_hash_free);
}

static Rot_Client_Data *
_rot_client_get(E_Client *ec)
{
   return eina_hash_find(p_rot->cdata_hash, &ec);
}

/* externally accessible functions */
/**
 * @describe
 *  Get current rotation angle.
 * @param      ec             e_client
 * @return     int            current angle
 */
EINTERN int
e_client_rotation_curr_angle_get(const E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, -1);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, -1);

   return ec->e.state.rot.ang.curr;
}

#ifndef HAVE_WAYLAND_ONLY
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
#endif

static Eina_Bool
_rot_info_cb_message(void *data EINA_UNUSED, int ev_type EINA_UNUSED, E_Event_Info_Rotation_Message *ev)
{
   Eina_Bool block;
   const char *name_hint = "enlightenment_info";

   if (EINA_UNLIKELY((ev == NULL) || (ev->zone == NULL)))
     goto end;

   switch (ev->message)
     {
      case E_INFO_ROTATION_MESSAGE_SET:
         //e_zone_rotation_set(ev->zone, ev->rotation);
         break;
      case E_INFO_ROTATION_MESSAGE_ENABLE:
      case E_INFO_ROTATION_MESSAGE_DISABLE:
         block = (ev->message == E_INFO_ROTATION_MESSAGE_DISABLE);
         //e_zone_rotation_block_set(ev->zone, name_hint, block);
         break;
      default:
         ERR("Unknown message");
     }

end:
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_rot_idle_exiter(void *data EINA_UNUSED)
{
   if (E_EVENT_INFO_ROTATION_MESSAGE == -1)
     return ECORE_CALLBACK_RENEW;

   E_LIST_HANDLER_APPEND(p_rot->events, E_EVENT_INFO_ROTATION_MESSAGE, _rot_info_cb_message, NULL);

   E_FREE_FUNC(p_rot->idle_exiter, ecore_idle_exiter_del);

   return ECORE_CALLBACK_DONE;
}

static void
_rot_wl_destroy(struct wl_resource *resource)
{
   Rot_Client_Data *rdata;

   rdata = wl_resource_get_user_data(resource);
   if (EINA_UNLIKELY(!rdata))
     return;

   _rot_client_del(rdata);
}

static void
_rot_wl_cb_destroy(struct wl_client *c EINA_UNUSED, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static int
_count_ones(int x)
{
   if (x == 0) return 0;
   return 1 + _count_ones(x & (x - 1));
}

static void
_rot_wl_cb_available_set(struct wl_client *c EINA_UNUSED, struct wl_resource *resource, uint32_t angles)
{
   Rot_Client_Data *rdata;
   E_Client *ec;
   int *rots = NULL, *prev_rots = NULL;
   int nrots;
   unsigned int prev_count = 0, i = 0, j = 0;
   uint32_t available_angles = 0;
   Eina_Bool diff = EINA_FALSE;
   char tmpstr[1024] = {0,}, tmpnum[80] = {0,};

   rdata = wl_resource_get_user_data(resource);
   if (EINA_UNLIKELY(!rdata))
     return;

   ec = rdata->ec;
   if (EINA_UNLIKELY(!ec))
     return;

   prev_count = ec->e.state.rot.count;
   if (prev_count != 0)
     prev_rots = calloc(prev_count, sizeof(int));
   if (ec->e.state.rot.available_rots)
     {
        memcpy(prev_rots, ec->e.state.rot.available_rots, (sizeof(int) * prev_count));
        E_FREE(ec->e.state.rot.available_rots);
     }

   nrots = _count_ones(angles);
   rots = calloc(nrots, sizeof(int));
   for (i = ROT_IDX_0; i <= ROT_IDX_270; i = i << ROT_IDX_0)
     {
        if (angles & i)
          rots[j++] = (ffs(i) - 1) * 90;
     }

   ec->e.state.rot.available_rots = rots;
   ec->e.state.rot.count = nrots;

   if (prev_count != nrots) diff = EINA_TRUE;

   // calc diff

   free(prev_rots);

   for (i = 0; i < nrots; i++)
     {
        snprintf(tmpnum, sizeof(tmpnum), "%d ", rots[i]);
        strcat(tmpstr, (const char *)tmpnum);
     }

   rdata->available_list = angles;

   RINF(rdata, "Set Available list: %s", tmpstr);
}

static void
_rot_wl_cb_preferred_set(struct wl_client *c EINA_UNUSED, struct wl_resource *resource, uint32_t angle)
{
   /* Deprecated */
   Rot_Client_Data *rdata;
   E_Client *ec;

   rdata = wl_resource_get_user_data(resource);
   if (EINA_UNLIKELY(!rdata))
     return;

   ec = rdata->ec;
   if (EINA_UNLIKELY(!ec))
     return;

   rdata->info.preferred_rot = (Rot_Idx)angle;
   ec->e.state.rot.preferred_rot = (ffs(angle) - 1) * 90;

   RINF(rdata, "Set Preferred Rotation: %d", ec->e.state.rot.preferred_rot);
}

static void
_rot_wl_cb_change_ack(struct wl_client *c EINA_UNUSED, struct wl_resource *resource, uint32_t serial)
{
   Rot_Client_Data *rdata;

   rdata = wl_resource_get_user_data(resource);
   if (EINA_UNLIKELY(!rdata))
     return;

   RDBG(rdata, "Rotation DONE: %d", rdata->ec ? rdata->ec->e.state.rot.ang.curr : -1);
}

static const struct tizen_rotation_interface _rot_wl_implementation =
{
   _rot_wl_cb_destroy,
   _rot_wl_cb_available_set,
   _rot_wl_cb_preferred_set,
   _rot_wl_cb_change_ack,
};

static void
_rot_wl_cb_rotation_get(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface)
{
   E_Client *ec;
   Rot_Client_Data *rdata;
   struct wl_resource *new_res;

   ec = wl_resource_get_user_data(surface);
   if (!ec)
     {
        ERR("failed to get user data from surface");
        return;
     }

   rdata = _rot_client_get(ec);
   if (rdata)
     {
        ERR("interface object already existed");
        wl_resource_post_error(resource, WL_DISPLAY_ERROR_INVALID_OBJECT,
                               "interface object already existed");
        return;
     }

   new_res = wl_resource_create(client, &tizen_rotation_interface, wl_resource_get_version(resource), id);
   if (!new_res)
     {
        ERR("failed to create resource 'tizen_rotation_interface'");
        wl_resource_post_no_memory(resource);
        return;
     }

   rdata = _rot_client_add(ec, new_res);
   if (EINA_UNLIKELY(!rdata))
     {
        wl_resource_post_no_memory(resource);
        wl_resource_destroy(new_res);
        return;
     }

   wl_resource_set_implementation(new_res, &_rot_wl_implementation, rdata, _rot_wl_destroy);

   RINF(rdata, "Setup Rotation Protocol");
}

static const struct tizen_policy_ext_interface _rot_wl_policy_ext_interface =
{
   _rot_wl_cb_rotation_get,
};

static void
_rot_wl_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   res = wl_resource_create(client, &tizen_policy_ext_interface, version, id);
   if (!res)
     {
        ERR("could not create 'tizen_policy_ext' resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_rot_wl_policy_ext_interface, NULL, NULL);
}

static Eina_Bool
_rot_client_cb_show(void *d, int type EINA_UNUSED, E_Event_Client *ev)
{
   E_Client *ec;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   p_rot->changes.rot = EINA_TRUE;

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_client_cb_hide(void *d, int type EINA_UNUSED, E_Event_Client *ev)
{
   E_Client *ec;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   p_rot->changes.rot = EINA_TRUE;

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_client_cb_move(void *d, int type EINA_UNUSED, E_Event_Client *ev)
{
   Rot_Client_Data *rdata;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   p_rot->changes.rot = EINA_TRUE;

   rdata = _rot_client_get(ev->ec);
   if (rdata)
     CHANGED(rdata->changes.geom);

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_client_cb_resize(void *d, int type EINA_UNUSED, E_Event_Client *ev)
{
   Rot_Client_Data *rdata;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   p_rot->changes.rot = EINA_TRUE;

   rdata = _rot_client_get(ev->ec);
   if (rdata)
     CHANGED(rdata->changes.geom);

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_client_cb_stack(void *d, int type EINA_UNUSED, E_Event_Client *ev)
{
   Rot_Client_Data *rdata;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   p_rot->changes.rot = EINA_TRUE;

   rdata = _rot_client_get(ev->ec);
   if (rdata)
     CHANGED(rdata->changes.geom);

end:
   return ECORE_CALLBACK_PASS_ON;
}

static void
_rot_client_hook_new(void *d EINA_UNUSED, E_Client *ec)
{
   if (EINA_UNLIKELY(!ec))
     return;

   ec->e.state.rot.available_rots = NULL;
   ec->e.state.rot.preferred_rot = -1;
   ec->e.state.rot.type = E_CLIENT_ROTATION_TYPE_NORMAL;
   ec->e.state.rot.ang.next = -1;
   ec->e.state.rot.ang.reserve = -1;
   ec->e.state.rot.pending_show = 0;
   ec->e.state.rot.ang.curr = 0;
   ec->e.state.rot.ang.prev = 0;
}

static void
_rot_client_hook_del(void *d EINA_UNUSED, E_Client *ec)
{
   // Do consider E_EVENT_CLIENT_REMOVE
   Rot_Client_Data *rdata;

   if (EINA_UNLIKELY(!ec))
     return;

   rdata = _rot_client_get(ec);
   if (!rdata)
     return;

   ec->e.fetch.rot.app_set = 0;
   ec->e.state.rot.preferred_rot = -1;

   if (ec->e.state.rot.available_rots)
     E_FREE(ec->e.state.rot.available_rots);

   wl_resource_set_user_data(rdata->resource, NULL);
   wl_resource_destroy(rdata->resource);

   _rot_client_del(rdata);
}

static void
_rot_client_hook_eval_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   Rot_Client_Data *rdata;
   int *rots = NULL, *prev_rots = NULL;
   int nrots;
   unsigned int prev_count = 0, i = 0, j = 0;
   uint32_t available_angles = 0;
   Eina_Bool diff = EINA_FALSE;

   rdata = _rot_client_get(ec);
   if (!rdata)
     return;

   if (rdata->changes.available_list)
     {
        rdata->changes.available_list = EINA_FALSE;

        prev_count = ec->e.state.rot.count;
        prev_rots = calloc(prev_count, sizeof(int));
        if (ec->e.state.rot.available_rots)
          {
             memcpy(prev_rots, ec->e.state.rot.available_rots, (sizeof(int) * prev_count));
             E_FREE(ec->e.state.rot.available_rots);
          }

        nrots = _count_ones(rdata->available_list);
        rots = calloc(nrots, sizeof(int));
        for (i = ROT_IDX_0; i <= ROT_IDX_270; i = i << ROT_IDX_0)
          {
             if (rdata->available_list & i)
               rots[j++] = (ffs(i) - 1) * 90;
          }

        ec->e.state.rot.available_rots = rots;
        ec->e.state.rot.count = nrots;

        if (prev_count != nrots) diff = EINA_TRUE;

        // calc diff

        free(prev_rots);
     }
}

static void
_rot_client_hook_eval_end(void *d EINA_UNUSED, E_Client *ec)
{
   // Send Rotation Message
}

static Eina_Bool
_rot_comp_hook_show(void *d EINA_UNUSED, E_Client *ec)
{
   // Pending show

   return EINA_TRUE;
}

static Eina_Bool
_rot_comp_hook_hide(void *d EINA_UNUSED, E_Client *ec)
{
   // TODO: Add VKBD Hide, VKBD Parent Hide routine.
   // clear pending_show, because this window is hidden now.

   return EINA_TRUE;
}

static void
_rot_type_update(Rot_Client_Data *rdata)
{
   E_Zone *zone;
   E_Client *ec;

   ec = rdata->ec;
   if (EINA_UNLIKELY(!ec))
     return;

   zone = ec->zone;
   if (EINA_UNLIKELY(!zone))
     return;

   if ((ec->argb) ||
       ((zone->x != ec->x) || (zone->y != ec->y) ||
        (zone->w != ec->w) || (zone->h != ec->h)))

     {
        rdata->type = ROT_TYPE_DEPENDENT;
        ec->e.state.rot.type = E_CLIENT_ROTATION_TYPE_DEPENDENT;
     }
   else
     {
        rdata->type = ROT_TYPE_NORMAL;
        ec->e.state.rot.type = E_CLIENT_ROTATION_TYPE_NORMAL;
     }

   RDBG(rdata, "Update Rotation Type: %s",
        (rdata->type == ROT_TYPE_NORMAL ? "NORMAL" : "DEPENDENT"));
}

static Eina_Bool
_rot_client_available_check(E_Client *ec, Rot_Idx rot)
{
   E_Zone *zone;
   Rot_Client_Data *rdata;

   rdata = _rot_client_get(ec);
   if (!rdata)
     {
        if (rot == ROT_IDX_0)
          return EINA_TRUE;
        else
          return EINA_FALSE;
     }

   return rdata->available_list & rot;
}

static Eina_List *
_rot_zone_target_client_list_get(Rot_Zone_Data *zdata)
{
   Rot_Client_Data *rdata;
   E_Client *ec;
   Eina_List *list;
   int x, y, w, h;

   E_CLIENT_REVERSE_FOREACH(ec)
     {
        if (zdata->zone != ec->zone) continue;
        if (!ec->visible) continue;
        if (e_object_is_del(E_OBJECT(ec))) continue;

        list = eina_list_append(list, ec);

        e_client_geometry_get(ec, &x, &y, &w, &h);
        if (E_CONTAINS(ec->zone->x, ec->zone->y, ec->zone->w, ec->zone->h, x, y, w, h))
          break;
     }

   return list;
}

static Eina_Bool
_rot_idle_enterer(void *data EINA_UNUSED)
{
   Rot_Zone_Data *zdata;
   Rot_Client_Data *rdata;
   E_Client *ec;
   Eina_Iterator *itr;
   Eina_List *target_list;
   static E_Client *top_bg_ec = NULL;

   if (!p_rot->changed)
     goto end;

   p_rot->changed = EINA_FALSE;

   itr = eina_hash_iterator_data_new(p_rot->cdata_hash);
   EINA_ITERATOR_FOREACH(itr, rdata)
     {
        /* step 1. determine the rotation type according to geometry */
        if (rdata->changes.geom)
          {
             if (EINA_LIKELY(rdata->ec))
               {
                  RDBG(rdata, "Geometry Changed: x %d y %d w %d h %d",
                       rdata->ec->x, rdata->ec->y, rdata->ec->w, rdata->ec->h);
               }

             _rot_type_update(rdata);

             rdata->changes.geom = EINA_FALSE;
          }
     }
   eina_iterator_free(itr);

   itr = eina_hash_iterator_data_new(p_rot->zdata_hash);
   EINA_ITERATOR_FOREACH(itr, zdata);
     {
        if (!zdata->changed) continue;
        zdata->changed = EINA_FALSE;

        target_list = _rot_zone_target_client_list_get(zdata);
     }

   EINA_ITERATOR_FOREACH(itr, rdata)
     {
        if (rdata->changes.rot)
          {
             /* Send rotation message to client */
          }
     }
   eina_iterator_free(itr);

end:
   return ECORE_CALLBACK_RENEW;
}

static void
_rot_wl_shutdown(void)
{
   E_FREE_FUNC(p_rot->global, wl_global_destroy);
}

static Eina_Bool
_rot_wl_init(void)
{
   struct wl_global *global;

   global = wl_global_create(e_comp_wl->wl.disp, &tizen_policy_ext_interface, 1, NULL, _rot_wl_cb_bind);
   if (!global)
     {
        ERR("Could not add tizen_policy_ext to wayland globals: %m");
        return EINA_FALSE;
     }

   p_rot->global = global;

   return EINA_TRUE;
}

static Eina_Bool
_rot_zone_add(E_Zone *zone)
{
   Rot_Zone_Data *zdata;

   zdata = calloc(1, sizeof(*zdata));
   if (!zdata)
     return EINA_FALSE;

   zdata->zone = zone;

   /* first initializing */
   if (!p_rot->zdata_hash)
     p_rot->zdata_hash = eina_hash_pointer_new((Eina_Free_Cb)free);

   eina_hash_add(p_rot->zdata_hash, &zone, zdata);

   return EINA_TRUE;
}

static void
_rot_zone_del(E_Zone *zone)
{
   eina_hash_del_by_key(p_rot->zdata_hash, &ev->zone);
}

static void
_rot_zone_clear(void)
{
   E_FREE_FUNC(p_rot->zdata_hash, eina_hash_free);
}

static Rot_Zone_Data *
_rot_zone_get(E_Zone *zone)
{
   return eina_hash_find(p_rot->zdata_hash, &zone);
}

static Eina_Bool
_rot_zone_cb_add(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Zone_Add *ev)
{
   _rot_zone_add(ev->zone);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_zone_cb_del(void *d EINA_UNUSED, int type EINA_UNUSED, E_Event_Zone_Add *ev)
{
   _rot_zone_del(ev->zone);

   return ECORE_CALLBACK_PASS_ON;
}

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

static void
_rot_event_init(void)
{
   E_LIST_HANDLER_APPEND(p_rot->events, E_EVENT_ZONE_ADD,                   _rot_zone_cb_add,            NULL);
   E_LIST_HANDLER_APPEND(p_rot->events, E_EVENT_ZONE_DEL,                   _rot_zone_cb_del,            NULL);

   E_LIST_HANDLER_APPEND(p_rot->events, E_EVENT_CLIENT_SHOW,                _rot_client_cb_show,         NULL);
   E_LIST_HANDLER_APPEND(p_rot->events, E_EVENT_CLIENT_HIDE,                _rot_client_cb_hide,         NULL);
   E_LIST_HANDLER_APPEND(p_rot->events, E_EVENT_CLIENT_MOVE,                _rot_client_cb_move,         NULL);
   E_LIST_HANDLER_APPEND(p_rot->events, E_EVENT_CLIENT_RESIZE,              _rot_client_cb_resize,       NULL);
   E_LIST_HANDLER_APPEND(p_rot->events, E_EVENT_CLIENT_STACK,               _rot_client_cb_stack,        NULL);

   E_CLIENT_HOOK_APPEND(p_rot->client_hooks, E_CLIENT_HOOK_NEW_CLIENT,       _rot_client_hook_new,         NULL);
   E_CLIENT_HOOK_APPEND(p_rot->client_hooks, E_CLIENT_HOOK_DEL,              _rot_client_hook_del,         NULL);
   E_CLIENT_HOOK_APPEND(p_rot->client_hooks, E_CLIENT_HOOK_EVAL_FETCH,       _rot_client_hook_eval_fetch,  NULL);
   E_CLIENT_HOOK_APPEND(p_rot->client_hooks, E_CLIENT_HOOK_EVAL_END,         _rot_client_hook_eval_end,    NULL);

   E_COMP_OBJECT_INTERCEPT_HOOK_APPEND(p_rot->comp_hooks, E_COMP_OBJECT_INTERCEPT_HOOK_SHOW_HELPER, _rot_comp_hook_show, NULL);
   E_COMP_OBJECT_INTERCEPT_HOOK_APPEND(p_rot->comp_hooks, E_COMP_OBJECT_INTERCEPT_HOOK_HIDE, _rot_comp_hook_hide, NULL);

   p_rot->idle_enterer = ecore_idle_enterer_add(_rot_idle_enterer, NULL);

   if (E_EVENT_INFO_ROTATION_MESSAGE != -1)
     E_LIST_HANDLER_APPEND(p_rot->events, E_EVENT_INFO_ROTATION_MESSAGE, _rot_info_cb_message, NULL);
   else
     {
        /* NOTE:
         * If E_EVENT_INFO_ROTATION_MESSAGE is not initialized yet,
         * we should add event handler for pration message in idle exiter,
         * becuase I expect that e_info_server will be initialized on idler
         * by quick launching.
         */
        p_rot->idle_exiter = ecore_idle_exiter_add(_rot_idle_exiter, NULL);
     }
}

static void
_rot_event_shutdown(void)
{
   E_FREE_FUNC(p_rot->idle_enterer, ecore_idle_enterer_del);
   E_FREE_FUNC(p_rot->idle_exiter, ecore_idle_exiter_del);

   E_FREE_LIST(p_rot->events, ecore_event_del);
   E_FREE_LIST(p_rot->client_hooks, e_client_hook_del);
   E_FREE_LIST(p_rot->comp_hooks, e_comp_object_intercept_hook_del);
}

static Eina_Bool
_rot_zone_init(void)
{
   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        if (!_rot_zone_add(zone))
          goto err;
     }

   return EINA_TRUE;
err:
   _rot_zone_clear();

   return EINA_FALSE;
}

static void
_rot_zone_shutdown(void)
{
   E_FREE_FUNC(p_rot->zdata_hash, eina_hash_free);
}

EINTERN int
e_mod_pol_rotation_init(void)
{
   Eina_List *l;
   E_Zone *zone;

   if (_init_count)
     goto end;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl, 0);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl->wl.disp, 0);

   if (EINA_UNLIKELY(p_rot))
     return 0;

   if (!eina_init())
     {
        fprintf(stderr, "Error initializing Eina %s\n", __FUNCTION__);
        return 0;
     }

   _log_dom = eina_log_domain_register(_dom_name, EINA_COLOR_LIGHTCYAN);
   if (_log_dom < 0)
     {
        EINA_LOG_ERR("Unable to register '%s' log domain", _dom_name);
        goto err_log;
     }

   p_rot = calloc(1, sizeof(*p_rot));
   if (!p_rot)
     goto err_alloc;

   if (!_rot_wl_init())
     {
        ERR("Error initializing '_rot_wl_init'");
        goto err_wl;
     }

   if (!_rot_zone_init())
     {
        ERR("Error initializing '_rot_zone_init'");
        goto err_zdata;
     }

   _rot_event_init();

#ifdef HAVE_AUTO_ROTATION
   if (!e_mod_sensord_init())
     ERR("Error initializing Sensord\n");
#endif

end:
   return ++_init_count;

err_cdata:
   _rot_zone_shutdown();

err_zdata:
   _rot_wl_shutdown();

err_wl:
   free(p_rot);

err_alloc:
   eina_log_domain_unregister(_log_dom);

err_log:
   eina_shutdown();

   return 0;
}

EINTERN void
e_mod_pol_rotation_shutdown(void)
{
   Pol_Rotation *p_rot;

   if (!_init_count)
     return;
   --_init_count;

   _rot_client_clear();

   _rot_event_shutdown();
   _rot_wl_shutdown();
   _rot_zone_shutdown();

   eina_log_domain_unregister(_log_dom);
   eina_shutdown();

#ifdef HAVE_AUTO_ROTATION
   e_mod_sensord_deinit();
#endif

   E_FREE(p_rot);
}

static void
_rot_zone_event_change_begin_free(void *data EINA_UNUSED, void *ev)
{
   E_Event_Zone_Rotation_Change_Begin *e = ev;
   e_object_unref(E_OBJECT(e->zone));
   E_FREE(e);
}

static void
_rot_zone_rot_set(Rot_Zone_Data *zdata, int rot)
{
   E_Event_Zone_Rotation_Change_Begin *ev;
   E_Client *ec;
   Rot_Idx next;

   RINF(NULL, "zone:%d|rot curr:%d, rot:%d", zdata->zone->num, zdata->zone->rot.curr, rot);

   next = 1 << ((rot / 90) + 1);

   if ((zdata->wait_for_done) ||
       (zdata->block_count > 0))
     {
        zdata->angle.next = next;
        zdata->pending = EINA_TRUE;
        return;
     }

   if (zdata->rot.curr == next) return;

   zdata->angle.prev = zdata->angle.curr;
   zdata->angle.curr = next;
   zdata->wait_for_done = EINA_TRUE;

   ev = E_NEW(E_Event_Zone_Rotation_Change_Begin, 1);
   if (ev)
     {
        ev->zone = zdata->zone;
        e_object_ref(E_OBJECT(ev->zone));
        ecore_event_add(E_EVENT_ZONE_ROTATION_CHANGE_BEGIN,
                        ev, _rot_zone_event_change_begin_free, NULL);
     }

   CHANGED(zdata->changed);
}

EINTERN void
e_mod_rot_zone_rot_set(E_Zone *zone, int rot)
{
   Rot_Zone_Data *zdata;
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if (!e_config->wm_win_rotation)
     return;

   zdata = _rot_zone_get(zone);
   if (!zdata)
     return;

   TRACE_DS_BEGIN(ZONE ROTATION SET);

   if (rot == -1)
     {
        zdata->unknown_state = EINA_TRUE;
        RINF(NULL, "ZONE_ROT |UNKOWN SET|zone:%d|rot curr:%d, rot:%d",
             zone->num, zone->rot.curr, rot);
        return;
     }
   else
     zdata->unknown_state = EINA_FALSE;

   _rot_zone_rot_set(zdata, rot);

   TRACE_DS_END();
}
