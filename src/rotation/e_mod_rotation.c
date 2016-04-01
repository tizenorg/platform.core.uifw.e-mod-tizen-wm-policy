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
   LOG("ROT|ec:%08p|name:%10s|"f, r->ec, r->ec?(r->ec->icccm.name?:""):"", ##x)

#define RDBG(r, f, x...)   RLOG(DBG, r, f, ##x)
#define RINF(r, f, x...)   RLOG(INF, r, f, ##x)
#define RERR(r, f, x...)   RLOG(ERR, r, f, ##x)

typedef struct _Pol_Rotation     Pol_Rotation;
typedef struct _Rot_Zone_Data    Rot_Zone_Data;
typedef struct _Rot_Client_Data  Rot_Client_Data;

struct _Pol_Rotation
{
   struct wl_global *global;

   Eina_List *events;
   Eina_List *client_hooks;
   Eina_List *comp_hooks;

   Ecore_Idle_Enterer  *idle_enterer;
   Ecore_Idle_Exiter   *idle_exiter;

   Eina_List *update_list;
};

struct _Rot_Zone_Data
{
   E_Zone *zone;
   Rot_Idx curr, prev, next, reserve;
};

struct _Rot_Client_Data
{
   E_Client *ec;
   struct wl_resource *resource;

   Rot_Idx curr, prev, next, reserve;
   Rot_Type type;

   int available_list;

   struct
   {
      int available_list;
      Rot_Idx preferred_rot;
   } info;

   struct
   {
      Eina_Bool geom;
      Rot_Idx rot;
   } changes;
};

const static char          *_dom_name = "e_rot";
static Pol_Rotation        *_pol_rotation = NULL;
static Eina_Hash           *_rot_data_hash = NULL;
static int                  _init_count = 0;
static int                  _log_dom = -1;

static inline void
_pol_rotation_register(Pol_Rotation *pr)
{
   _pol_rotation = pr;
}

static inline void
_pol_rotation_unregister(void)
{
   _pol_rotation = NULL;
}

static inline Pol_Rotation *
_pol_rotation_get(void)
{
   return _pol_rotation;
}

static Rot_Zone_Data *


static void
_rot_zone_change_list_add(E_Zone *zone)
{
   Pol_Rotation *pr;

   pr = _pol_rotation_get();
   if (EINA_UNLIKELY(!pr))
     return;

   if ((!pr->changes.zones) ||
       (!eina_list_data_find(pr->changes.zones, zone)))
     pr->changes.zones = eina_list_append(pr->changes.zones, zone);
}

static E_Zone *
_rot_zone_change_list_find(E_Zone *zone)
{
   Pol_Rotation *pr;

   pr = _pol_rotation_get();
   if (EINA_UNLIKELY(!pr))
     return;

   return eina_list_data_find(pr->changes.zones, zone)
}

static void
_rot_zone_change_list_clear(void)
{
   Pol_Rotation *pr;

   pr = _pol_rotation_get();
   if (EINA_UNLIKELY(!pr))
     return;

   E_FREE_FUNC(pr->changes.zones, eina_list_free);
}

static Rot_Client_Data *
_rot_data_new(E_Client *ec, struct wl_resource *resource)
{
   Rot_Client_Data *rdata;

   rdata = calloc(1, sizeof(Rot_Client_Data));
   if (EINA_UNLIKELY(!rdata))
     return NULL;

   rdata->ec = ec;
   rdata->resource = resource;
   rdata->type = ROT_TYPE_UNKNOWN;

   eina_hash_add(_rot_data_hash, &ec, rdata);

   return rdata;
}

static void
_rot_data_free(Rot_Client_Data *rdata)
{
   eina_hash_del(_rot_data_hash, &rdata->ec, rdata);
   free(rdata);
}

static Rot_Client_Data *
_rot_data_find(E_Client *ec)
{
   return eina_hash_find(_rot_data_hash, &ec);
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

   E_LIST_HANDLER_APPEND(pr->events, E_EVENT_INFO_ROTATION_MESSAGE, _rot_info_cb_message, NULL);

   E_FREE_FUNC(pr->idle_exiter, ecore_idle_exiter_del);

   return ECORE_CALLBACK_DONE;
}

static void
_rot_wl_destroy(struct wl_resource *resource)
{
   Rot_Client_Data *rdata;

   rdata = wl_resource_get_user_data(resource);
   if (EINA_UNLIKELY(!rdata))
     return;

   _rot_data_free(rdata);
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

   rdata->info.available_list = angles;

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

   rdata = _rot_data_find(ec);
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

   rdata = _rot_data_new(ec, new_res);
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
   Pol_Rotation *pr;
   E_Client *ec;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   pr = d;
   if (EINA_LIKELY(pr))
     pr->changes.rot = EINA_TRUE;

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_client_cb_hide(void *d, int type EINA_UNUSED, E_Event_Client *ev)
{
   Pol_Rotation *pr;
   E_Client *ec;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   pr = d;
   if (EINA_LIKELY(pr))
     pr->changes.rot = EINA_TRUE;

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_client_cb_move(void *d, int type EINA_UNUSED, E_Event_Client *ev)
{
   Pol_Rotation *pr;
   Rot_Client_Data *rdata;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   pr = d;
   if (EINA_LIKELY(pr))
     pr->changes.rot = EINA_TRUE;

   rdata = _rot_data_find(ev->ec);
   if (rdata)
     rdata->changes.geom = EINA_TRUE;

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_client_cb_resize(void *d, int type EINA_UNUSED, E_Event_Client *ev)
{
   Pol_Rotation *pr;
   Rot_Client_Data *rdata;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   pr = d;
   if (EINA_LIKELY(pr))
     pr->changes.rot = EINA_TRUE;

   rdata = _rot_data_find(ev->ec);
   if (rdata)
     rdata->changes.geom = EINA_TRUE;
end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_client_cb_stack(void *d, int type EINA_UNUSED, E_Event_Client *ev)
{
   Pol_Rotation *pr;
   Rot_Client_Data *rdata;

   if (EINA_UNLIKELY(!ev)) goto end;
   if (EINA_UNLIKELY(!ev->ec)) goto end;

   pr = d;
   if (EINA_LIKELY(pr))
     pr->changes.rot = EINA_TRUE;

   rdata = _rot_data_find(ev->ec);
   if (rdata)
     rdata->changes.geom = EINA_TRUE;
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
   // Destroy Rot_Client_Data
   Rot_Client_Data *rdata;

   if (EINA_UNLIKELY(!ec))
     return;

   rdata = _rot_data_find(ec);
   if (!rdata)
     return;

   wl_resource_set_user_data(rdata->resource, NULL);
   wl_resource_destroy(rdata->resource);
   _rot_data_free(rdata);
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

   rdata = _rot_data_find(ec);
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

        nrots = _count_ones(rdata->info.available_list);
        rots = calloc(nrots, sizeof(int));
        for (i = ROT_IDX_0; i <= ROT_IDX_270; i = i << ROT_IDX_0)
          {
             if (rdata->info.available_list & i)
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

   rdata = _rot_data_find(ec);
   if (!rdata)
     {
        if (rot == ROT_IDX_0)
          return EINA_TRUE;
        else
          return EINA_FALSE;
     }

   return rdata->available_list & rot;
}

static Eina_Bool
_rot_idle_enterer(void *data)
{
   Pol_Rotation *pr;
   Rot_Client_Data *rdata;
   E_Client *ec;
   Eina_Iterator *itr;
   static E_Client *top_bg_ec = NULL;

   pr= data;
   if (EINA_UNLIKELY(!pr))
     goto end;

   itr = eina_hash_iterator_data_new(_rot_data_hash);
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

   if (pr->changes.zones)
     {
        E_CLIENT_REVERSE_FOREACH(ec)
          {
             if (!_rot_zone_change_list_find(ec->zone)) continue;
             if (!ec->visible) continue;
             if (e_object_is_del(E_OBJECT(ec))) continue;


          }
        _rot_zone_change_list_clear();
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
_rot_wl_shutdown(Pol_Rotation *pr)
{
   E_FREE_FUNC(pr->global, wl_global_destroy);
}

static Eina_Bool
_rot_wl_init(Pol_Rotation *pr)
{
   struct wl_global *global;

   global = wl_global_create(e_comp_wl->wl.disp, &tizen_policy_ext_interface, 1, NULL, _rot_wl_cb_bind);
   if (!global)
     {
        ERR("Could not add tizen_policy_ext to wayland globals: %m");
        return EINA_FALSE;
     }

   pr->global = global;

   return EINA_TRUE;
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
_rot_event_init(Pol_Rotation *pr)
{
   E_LIST_HANDLER_APPEND(pr->events, E_EVENT_CLIENT_SHOW,                _rot_client_cb_show,         pr);
   E_LIST_HANDLER_APPEND(pr->events, E_EVENT_CLIENT_HIDE,                _rot_client_cb_hide,         pr);
   E_LIST_HANDLER_APPEND(pr->events, E_EVENT_CLIENT_MOVE,                _rot_client_cb_move,         pr);
   E_LIST_HANDLER_APPEND(pr->events, E_EVENT_CLIENT_RESIZE,              _rot_client_cb_resize,       pr);
   E_LIST_HANDLER_APPEND(pr->events, E_EVENT_CLIENT_STACK,               _rot_client_cb_stack,        pr);

   E_CLIENT_HOOK_APPEND(pr->client_hooks, E_CLIENT_HOOK_NEW_CLIENT,       _rot_client_hook_new,         pr);
   E_CLIENT_HOOK_APPEND(pr->client_hooks, E_CLIENT_HOOK_DEL,              _rot_client_hook_del,         NULL);
   E_CLIENT_HOOK_APPEND(pr->client_hooks, E_CLIENT_HOOK_EVAL_FETCH,       _rot_client_hook_eval_fetch,  NULL);
   E_CLIENT_HOOK_APPEND(pr->client_hooks, E_CLIENT_HOOK_EVAL_END,         _rot_client_hook_eval_end,    NULL);

   E_COMP_OBJECT_INTERCEPT_HOOK_APPEND(pr->comp_hooks, E_COMP_OBJECT_INTERCEPT_HOOK_SHOW_HELPER, _rot_comp_hook_show, NULL);
   E_COMP_OBJECT_INTERCEPT_HOOK_APPEND(pr->comp_hooks, E_COMP_OBJECT_INTERCEPT_HOOK_HIDE, _rot_comp_hook_hide, NULL);

   pr->idle_enterer = ecore_idle_enterer_add(_rot_idle_enterer, pr);

   if (E_EVENT_INFO_ROTATION_MESSAGE != -1)
     E_LIST_HANDLER_APPEND(pr->events, E_EVENT_INFO_ROTATION_MESSAGE, _rot_info_cb_message, NULL);
   else
     {
        /* NOTE:
         * If E_EVENT_INFO_ROTATION_MESSAGE is not initialized yet,
         * we should add event handler for pration message in idle exiter,
         * becuase I expect that e_info_server will be initialized on idler
         * by quick launching.
         */
        pr->idle_exiter = ecore_idle_exiter_add(_rot_idle_exiter, NULL);
     }
}

static void
_rot_event_shutdown(Pol_Rotation *pr)
{
   E_FREE_FUNC(pr->idle_enterer, ecore_idle_enterer_del);
   E_FREE_FUNC(pr->idle_exiter, ecore_idle_exiter_del);

   E_FREE_LIST(pr->events, ecore_event_del);
   E_FREE_LIST(pr->client_hooks, e_client_hook_del);
   E_FREE_LIST(pr->comp_hooks, e_comp_object_intercept_hook_del);
}

EINTERN int
e_mod_pol_rotation_init(void)
{
   Pol_Rotation *pr;
   Eina_List *l;
   E_Zone *zone;

   if (_init_count)
     goto end;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl, 0);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl->wl.disp, 0);

   if (EINA_UNLIKELY(_pol_rotation_get()))
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

   pr = calloc(1, sizeof(*pr));
   if (!pr)
     goto err_alloc;

   _rot_data_hash = eina_hash_pointer_new(NULL);

   if (!_rot_wl_init(pr))
     {
        ERR("Error initializing '_rot_wl_init'");
        goto err_wl;
     }

   _rot_event_init(pr);

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {

     }

   _pol_rotation_register(pr);

#ifdef HAVE_AUTO_ROTATION
   if (!e_mod_sensord_init())
     ERR("Error initializing Sensord\n");
#endif

end:
   return ++_init_count;

err_wl:
   free(pr);

err_alloc:
   eina_log_domain_unregister(_log_dom);

err_log:
   eina_shutdown();

   return 0;
}

EINTERN void
e_mod_pol_rotation_shutdown(void)
{
   Pol_Rotation *pr;

   if (!_init_count)
     return;
  --_init_count;

   pr = _pol_rotation_get();
   if (EINA_UNLIKELY(!pr))
     return;

   E_FREE_FUNC(_rot_data_hash, eina_hash_free);

   _rot_event_shutdown(pr);
   _rot_wl_shutdown(pr);
   eina_log_domain_unregister(_log_dom);
   eina_shutdown();

#ifdef HAVE_AUTO_ROTATION
  e_mod_sensord_deinit();
#endif

  free(pr);

  _pol_rotation_unregister();
}

static void
_rot_zone_event_change_begin_free(void *data EINA_UNUSED, void *ev)
{
   E_Event_Zone_Rotation_Change_Begin *e = ev;
   e_object_unref(E_OBJECT(e->zone));
   E_FREE(e);
}

static void
_rot_zone_set(Pol_Rotation *pr, E_Zone *zone, int rot)
{
   E_Event_Zone_Rotation_Change_Begin *ev;
   E_Client *ec;

   INF("ROT|zone:%d|rot curr:%d, rot:%d", zone->num, zone->rot.curr, rot);

   if ((zone->rot.wait_for_done) ||
       (zone->rot.block_count > 0))
     {
        zone->rot.next = rot;
        zone->rot.pending = EINA_TRUE;
        return;
     }

   if (zone->rot.curr == rot) return;

   zone->rot.prev = zone->rot.curr;
   zone->rot.curr = rot;
   zone->rot.wait_for_done = EINA_TRUE;

   ev = E_NEW(E_Event_Zone_Rotation_Change_Begin, 1);
   if (ev)
     {
        ev->zone = zone;
        e_object_ref(E_OBJECT(ev->zone));
        ecore_event_add(E_EVENT_ZONE_ROTATION_CHANGE_BEGIN,
                        ev, _rot_zone_event_change_begin_free, NULL);
     }

   _rot_zone_change_list_add(zone);
}

EINTERN void
e_mod_rot_zone_set(E_Zone *zone, int rot)
{
   Pol_Rotation *pr;
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if (!e_config->wm_win_rotation)
     return;

   pr = _pol_rotation_get();
   if (EINA_UNLIKELY(!pr))
     return;

   TRACE_DS_BEGIN(ZONE ROTATION SET);

   if (rot == -1)
     {
        zone->rot.unknown_state = EINA_TRUE;
        ELOGF("ROTATION", "ZONE_ROT |UNKOWN SET|zone:%d|rot curr:%d, rot:%d",
              NULL, NULL, zone->num, zone->rot.curr, rot);
        return;
     }
   else
     zone->rot.unknown_state = EINA_FALSE;

   _rot_zone_set(pr, zone, rot);

   TRACE_DS_END();
}
