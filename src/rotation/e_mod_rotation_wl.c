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
#include "e_mod_main.h"
#include "e_mod_rotation_wl.h"
#include "e_mod_rotation_private.h"

#ifdef HAVE_WAYLAND_ONLY

#include <wayland-server.h>
#include "tizen_policy_ext-server-protocol.h"

#define TIZEN_ROTATION_ANGLE_TO_INT(angle) ((angle == TIZEN_ROTATION_ANGLE_0) ? 0 :        \
                                            (angle == TIZEN_ROTATION_ANGLE_90) ? 90 :      \
                                            (angle == TIZEN_ROTATION_ANGLE_180) ? 180 :    \
                                            (angle == TIZEN_ROTATION_ANGLE_270) ? 270 : -1)

typedef struct _Policy_Ext_Rotation Policy_Ext_Rotation;
typedef struct _E_Client_Rotation E_Client_Rotation;

struct _Policy_Ext_Rotation
{
   E_Client *ec;
   uint32_t available_angles, preferred_angle;
   enum tizen_rotation_angle cur_angle, prev_angle;
   Eina_List *rotation_list;
   Eina_Bool angle_change_done;
   Eina_Bool wait_update;
   Eina_Bool hint_fetch;
   uint32_t serial;
};

struct _E_Client_Rotation
{
   Eina_List     *list;
   Eina_List     *async_list;
   Eina_List     *force_update_list;
   Ecore_Timer   *done_timer;
   Eina_Bool      screen_lock;
   Eina_Bool      fetch;

   struct
   {
      Eina_Bool    state;
      E_Zone      *zone;
   } cancel;
};

/* local subsystem variables */
static E_Client_Rotation rot =
{
   NULL,
   NULL,
   NULL,
   NULL,
   EINA_FALSE,
   EINA_FALSE,
   {
      EINA_FALSE,
      NULL,
   }
};
static Eina_Hash *hash_policy_ext_rotation = NULL;
static Eina_List *rot_handlers = NULL;
static Eina_List *rot_hooks = NULL;
static Eina_List *rot_intercept_hooks = NULL;
static Ecore_Idle_Enterer *rot_idle_enterer = NULL;

/* local subsystem functions */
static Policy_Ext_Rotation* _policy_ext_rotation_get(E_Client *ec);

/* local subsystem wayland rotation protocol related functions */
static void _e_tizen_rotation_destroy_cb(struct wl_client *client, struct wl_resource *resource);
static void _e_tizen_rotation_set_available_angles_cb(struct wl_client *client, struct wl_resource *resource, uint32_t angles);
static void _e_tizen_rotation_set_preferred_angle_cb(struct wl_client *client, struct wl_resource *resource, uint32_t angle);
static void _e_tizen_rotation_ack_angle_change_cb(struct wl_client *client, struct wl_resource *resource, uint32_t serial);
static void _e_tizen_rotation_destroy(struct wl_resource *resource);
static void _e_tizen_rotation_send_angle_change(E_Client *ec, int angle);
static void _e_tizen_policy_ext_get_rotation_cb(struct wl_client *client, struct wl_resource *resource, uint32_t id, struct wl_resource *surface);
static void _e_tizen_policy_ext_active_angle_cb(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface);
static void _e_tizen_policy_ext_bind_cb(struct wl_client *client, void *data, uint32_t version, uint32_t id);

/* local subsystem wayland rotation protocol related variables */
static const struct tizen_rotation_interface _e_tizen_rotation_interface =
{
   _e_tizen_rotation_destroy_cb,
   _e_tizen_rotation_set_available_angles_cb,
   _e_tizen_rotation_set_preferred_angle_cb,
   _e_tizen_rotation_ack_angle_change_cb,
};
static const struct tizen_policy_ext_interface _e_tizen_policy_ext_interface =
{
   _e_tizen_policy_ext_get_rotation_cb,
   _e_tizen_policy_ext_active_angle_cb,
};

/* local subsystem e_client_rotation related functions */
static void      _e_client_rotation_list_remove(E_Client *ec);
static Eina_Bool _e_client_rotation_zone_set(E_Zone *zone, E_Client *include_ec, E_Client *exclude_ec);
static void      _e_client_rotation_change_done(void);
static Eina_Bool _e_client_rotation_change_done_timeout(void *data);
static void      _e_client_rotation_change_message_send(E_Client *ec);
static int       _e_client_rotation_curr_next_get(const E_Client *ec);
static void      _e_client_event_client_rotation_change_begin_send(E_Client *ec);
static void      _e_client_event_client_rotation_change_begin_free(void *data, void *ev);
static void      _e_client_event_client_rotation_change_end_free(void *data, void *ev);

/* local subsystem e_zone_rotation related functions */
static void      _e_zone_event_rotation_change_begin_free(void *data, void *ev);
static void      _e_zone_event_rotation_change_end_free(void *data, void *ev);
static void      _e_zone_event_rotation_change_cancel_free(void *data, void *ev);

/* e_client_roation functions */
static Eina_Bool  e_client_rotation_is_progress(const E_Client *ec);
static int        e_client_rotation_curr_angle_get(const E_Client *ec);
static int        e_client_rotation_next_angle_get(const E_Client *ec);
static Eina_Bool  e_client_rotation_is_available(const E_Client *ec, int ang);
static Eina_Bool  e_client_rotation_set(E_Client *ec, int rotation);
static void       e_client_rotation_change_request(E_Client *ec, int rotation);

/* e_zone_roation functions */
static void       e_zone_rotation_update_done(E_Zone *zone);
static void       e_zone_rotation_update_cancel(E_Zone *zone);
static void       e_zone_rotation_sub_set(E_Zone *zone, int rotation) EINA_UNUSED;

/* e_client event, hook, intercept callbacks */
static Eina_Bool _rot_cb_zone_rotation_change_begin(void *data EINA_UNUSED, int ev_type EINA_UNUSED, E_Event_Zone_Rotation_Change_Begin *ev);
static void      _rot_hook_new_client(void *d EINA_UNUSED, E_Client *ec);
static void      _rot_hook_client_del(void *d EINA_UNUSED, E_Client *ec);
static void      _rot_hook_eval_end(void *d EINA_UNUSED, E_Client *ec);
static void      _rot_hook_eval_fetch(void *d EINA_UNUSED, E_Client *ec);
static Eina_Bool _rot_intercept_hook_show_helper(void *d EINA_UNUSED, E_Client *ec);
static Eina_Bool _rot_intercept_hook_hide(void *d EINA_UNUSED, E_Client *ec);
static Eina_Bool _rot_cb_idle_enterer(void *data EINA_UNUSED);

/* local subsystem functions */
static Policy_Ext_Rotation*
_policy_ext_rotation_get(E_Client *ec)
{
   Policy_Ext_Rotation *rot;

   EINA_SAFETY_ON_NULL_RETURN_VAL(hash_policy_ext_rotation, NULL);

   rot = eina_hash_find(hash_policy_ext_rotation, &ec);
   if (!rot)
     {
        rot = E_NEW(Policy_Ext_Rotation, 1);
        EINA_SAFETY_ON_NULL_RETURN_VAL(rot, NULL);

        rot->ec = ec;
        rot->angle_change_done = EINA_TRUE;
        eina_hash_add(hash_policy_ext_rotation, &ec, rot);
     }

   return rot;
}

static void
_e_tizen_rotation_destroy_cb(struct wl_client *client EINA_UNUSED,
                             struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static void
_e_tizen_rotation_set_available_angles_cb(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t angles)
{
   E_Client *ec;
   Policy_Ext_Rotation *rot;

   rot = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   ec = rot->ec;
   if (!ec)
     return;

   rot->available_angles = angles;

   ec->e.fetch.rot.available_rots = 1;
   EC_CHANGED(ec);

   /* for clients supporting landscape mode only */
   if ((rot->available_angles) &&
       !(rot->available_angles & TIZEN_ROTATION_ANGLE_0) &&
       !(rot->available_angles & TIZEN_ROTATION_ANGLE_180))
     {
        enum tizen_rotation_angle tz_angle = 0;
        uint32_t serial;
        Eina_List *l;
        struct wl_resource *res;

        if (rot->available_angles & TIZEN_ROTATION_ANGLE_90)
          tz_angle = TIZEN_ROTATION_ANGLE_90;
        else if (rot->available_angles & TIZEN_ROTATION_ANGLE_270)
          tz_angle = TIZEN_ROTATION_ANGLE_270;
        else
          {
             ERR("What's this impossible angle?? : %d", rot->available_angles);
          }

        if (tz_angle)
          {
             EDBG(ec, "Send Change Rotation: angle %d for redering preparation of landscape only app", tz_angle);
             serial = wl_display_next_serial(e_comp_wl->wl.disp);

             EINA_LIST_FOREACH(rot->rotation_list, l, res)
               {
                  tizen_rotation_send_angle_change(res, tz_angle, serial);
               }

             ec->e.state.rot.ang.curr = TIZEN_ROTATION_ANGLE_TO_INT(tz_angle);
          }
     }
}

static void
_e_tizen_rotation_set_preferred_angle_cb(struct wl_client *client,
                                         struct wl_resource *resource,
                                         uint32_t angle)
{
   Policy_Ext_Rotation *rot;
   E_Client *ec;

   rot = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   ec = rot->ec;
   if (!ec)
     return;

   rot->preferred_angle = angle;

   ec->e.fetch.rot.preferred_rot = 1;
   EC_CHANGED(ec);


   /* for clients supporting landscape mode only */
   if ((rot->preferred_angle) &&
       !(rot->preferred_angle & TIZEN_ROTATION_ANGLE_0) &&
       !(rot->preferred_angle & TIZEN_ROTATION_ANGLE_180))
     {
        enum tizen_rotation_angle tz_angle = 0;
        uint32_t serial;
        Eina_List *l;
        struct wl_resource *res;

        if (rot->preferred_angle & TIZEN_ROTATION_ANGLE_90)
          tz_angle = TIZEN_ROTATION_ANGLE_90;
        else if (rot->preferred_angle & TIZEN_ROTATION_ANGLE_270)
          tz_angle = TIZEN_ROTATION_ANGLE_270;
        else
          {
             ERR("What's this impossible angle?? : %d", rot->preferred_angle);
          }

        if (tz_angle)
          {
             EDBG(ec, "Send Change Rotation: angle %d for redering preparation of landscape only app", tz_angle);
             serial = wl_display_next_serial(e_comp_wl->wl.disp);

             EINA_LIST_FOREACH(rot->rotation_list, l, res)
               {
                  tizen_rotation_send_angle_change(res, tz_angle, serial);
               }

             ec->e.state.rot.ang.curr = TIZEN_ROTATION_ANGLE_TO_INT(tz_angle);
          }
     }
}

static void
_e_tizen_rotation_ack_angle_change_cb(struct wl_client *client,
                                      struct wl_resource *resource,
                                      uint32_t serial)
{
   E_Client *ec;
   Policy_Ext_Rotation *rot;

   rot = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   ec = rot->ec;
   if (!ec)
     return;

   if (rot->serial == serial) // rotation success
     {
        ec->e.state.rot.ang.prev = ec->e.state.rot.ang.curr;
        ec->e.state.rot.ang.curr = TIZEN_ROTATION_ANGLE_TO_INT(rot->cur_angle);

        EDBG(ec, "Rotation Done: prev %d cur %d",
             ec->e.state.rot.ang.prev, ec->e.state.rot.ang.curr);

        if (TIZEN_ROTATION_ANGLE_TO_INT(rot->cur_angle) == ec->e.state.rot.ang.next)
          {
             ec->e.state.rot.ang.next = -1;

             if (ec->e.state.rot.ang.reserve != -1)
               {
                  _e_client_rotation_list_remove(ec);

                  e_client_rotation_set(ec, ec->e.state.rot.ang.reserve);
                  ec->e.state.rot.ang.reserve = -1;
               }
             else
               {
                  rot->wait_update = EINA_TRUE;
               }
          }
     }
   else // rotation fail
     {
        WRN("Rotation Zone Set: Rotation Done(fail case): %s(%p)",
            ec->icccm.name?:"", ec);
        _e_client_rotation_zone_set(ec->zone, ec, NULL);
     }

   // check angle change serial
   rot->angle_change_done = EINA_TRUE;
}

static void
_e_tizen_rotation_destroy(struct wl_resource *resource)
{
   Policy_Ext_Rotation *rot;

   rot = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   rot->rotation_list = eina_list_remove(rot->rotation_list, resource);
}

static void
_e_tizen_policy_ext_get_rotation_cb(struct wl_client *client,
                                    struct wl_resource *resource,
                                    uint32_t id,
                                    struct wl_resource *surface)
{
   int version = wl_resource_get_version(resource); // resource is tizen_policy_ext resource
   struct wl_resource *res;
   E_Client *ec;
   Policy_Ext_Rotation *rot;

   ec = wl_resource_get_user_data(surface);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   // Add rotation info
   rot = _policy_ext_rotation_get(ec);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   res = wl_resource_create(client, &tizen_rotation_interface, version, id);
   if (res == NULL)
     {
        wl_client_post_no_memory(client);
        return;
     }

   rot->rotation_list = eina_list_append(rot->rotation_list, res);

   wl_resource_set_implementation(res, &_e_tizen_rotation_interface,
                                  rot, _e_tizen_rotation_destroy);

   ec->e.fetch.rot.support = 1;
   EC_CHANGED(ec);
}

static void
_e_tizen_policy_ext_active_angle_cb(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface)
{
   int angle;
   E_Zone *zone;
   E_Client *ec;

   if (!surface)
     zone = e_zone_current_get();
   else
     {
        ec = wl_resource_get_user_data(surface);
        if (ec)
          zone = ec->zone;
        else
          zone = e_zone_current_get();
     }

   if (!zone)
     angle = -1;
   else
     angle = zone->rot.curr;

   tizen_policy_ext_send_active_angle(resource, angle);
}

static void
_e_tizen_policy_ext_bind_cb(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   struct wl_resource *res;

   if (!(res = wl_resource_create(client, &tizen_policy_ext_interface, version, id)))
     {
        ERR("Could not create scaler resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_tizen_policy_ext_interface, NULL, NULL);
}

static void
_e_tizen_rotation_send_angle_change(E_Client *ec, int angle)
{
   Eina_List *l;
   Policy_Ext_Rotation *rot;
   uint32_t serial;
   struct wl_resource *resource;
   enum tizen_rotation_angle tz_angle = TIZEN_ROTATION_ANGLE_0;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(hash_policy_ext_rotation);

   rot = eina_hash_find(hash_policy_ext_rotation, &ec);
   if (!rot) return;

   switch (angle)
     {
        case 0:
          tz_angle = TIZEN_ROTATION_ANGLE_0;
          break;
        case 90:
          tz_angle = TIZEN_ROTATION_ANGLE_90;
          break;
        case 180:
          tz_angle = TIZEN_ROTATION_ANGLE_180;
          break;
        case 270:
          tz_angle = TIZEN_ROTATION_ANGLE_270;
          break;
        default:
          break;
     }

   serial = wl_display_next_serial(e_comp_wl->wl.disp);

   rot->angle_change_done = EINA_FALSE;
   rot->prev_angle = rot->cur_angle;
   rot->cur_angle = tz_angle;
   rot->serial = serial;

   EDBG(ec, "Send Change Rotation: angle %d", angle);
   EINA_LIST_FOREACH(rot->rotation_list, l, resource)
     {
        tizen_rotation_send_angle_change(resource, tz_angle, serial);
     }
}

/* local subsystem e_client_rotation related functions */
static void
_e_client_rotation_list_remove(E_Client *ec)
{
   E_Event_Client_Rotation_Change_End *ev = NULL;
   Eina_Bool found = EINA_FALSE;

   if (eina_list_data_find(rot.list, ec) == ec)
     {
        found = EINA_TRUE;
        rot.list = eina_list_remove(rot.list, ec);
     }

   if (ec->e.state.rot.wait_for_done)
     {
        ec->e.state.rot.wait_for_done = 0;

        /* if we make the e_client event in the _e_client_free function,
         * then we may meet a crash problem, only work this at least e_client_hide.
         */
        if (!e_object_is_del(E_OBJECT(ec)))
          {
             ev = E_NEW(E_Event_Client_Rotation_Change_End, 1);
             if (ev)
               {
                  ev->ec = ec;
                  e_object_ref(E_OBJECT(ec));
                  ecore_event_add(E_EVENT_CLIENT_ROTATION_CHANGE_END,
                                  ev,
                                  _e_client_event_client_rotation_change_end_free,
                                  NULL);
               }
          }

        if ((found) &&
            (eina_list_count(rot.list) == 0))
          {
             _e_client_rotation_change_done();
          }
     }
   ec->e.state.rot.ang.next = -1;
   ec->changes.rotation = 0;
}

/* TODO need to optimize */
static Eina_Bool
_e_client_rotation_zone_set(E_Zone *zone, E_Client *include_ec, E_Client *exclude_ec)
{
   E_Client *ec;
   Eina_List *target_list = NULL, *l;
   int i, angle;
   Eina_Bool can_rotate = EINA_TRUE, ret = EINA_FALSE, found_bg_ec = EINA_FALSE;

   TRACE_DS_BEGIN(CLIENT ROTATION ZONE SET);

   DBG("<<< Try to set zone rotation");
   E_CLIENT_REVERSE_FOREACH(ec)
     {
        if (ec->zone != zone) continue;
        if (ec == exclude_ec) continue;
        if ((ec != include_ec) && (!evas_object_visible_get(ec->frame))) continue;
        if (e_object_is_del(E_OBJECT(ec))) continue;
        if ((ec->comp_data) && (ec->comp_data->sub.data)) continue;
        if (!e_util_strcmp("wl_pointer-cursor", ec->icccm.window_role))
          {
             e_client_cursor_map_apply(ec, zone->rot.curr, ec->x, ec->y);
             continue;
          }

        if (!found_bg_ec)
          {
             EDBG(ec, "Append to rotation target list");
             target_list = eina_list_append(target_list, ec);
          }
        else
          {
             e_client_rotation_set(ec, zone->rot.curr);
             continue;
          }

        if ((ec->x == zone->x) && (ec->y == zone->y) &&
            (ec->w == zone->w) && (ec->h == zone->h) &&
            (ec->e.state.rot.type == E_CLIENT_ROTATION_TYPE_NORMAL))
          {
             if (!ec->argb)
               {
                  EDBG(ec, "Found Topmost Fullscreen Window");
                  found_bg_ec = EINA_TRUE;
               }
             else
               {
                  if ((ec->visibility.opaque > 0) &&
                      (!ec->parent))
                    {
                       EDBG(ec, "Found Topmost Fullscreen Window");
                       found_bg_ec = EINA_TRUE;
                    }
               }
          }
     }

   angle = zone->rot.curr;
   EINA_LIST_FOREACH(target_list, l, ec)
     {
        if (!e_client_rotation_is_available(ec, angle))
          {
             EDBG(ec, "Failed to set rotation with zone: not able to rotate given angle %d", angle);
             angle = _e_client_rotation_curr_next_get(ec);
             can_rotate = EINA_FALSE;
             break;
          }
     }

   if (!can_rotate)
     {
        can_rotate = EINA_TRUE;
        EINA_LIST_FOREACH(target_list, l, ec)
          {
             if (!e_client_rotation_is_available(ec, angle))
               {
                  EDBG(ec, "Failed to set with exist client: not able to rotate given angle %d", angle);
                  can_rotate = EINA_FALSE;
                  break;
               }
          }
     }
   else
     {
        DBG("Set rotation of zone according to angle of zone: %d", angle);
        goto do_rotate;
     }

   if (!can_rotate)
     {
        for (i = 0; i < 360; i += 90)
          {
             if ((i == zone->rot.curr) || (i == angle))
               continue;

             can_rotate = EINA_TRUE;

             EINA_LIST_FOREACH(target_list, l, ec)
               {
                  if (!e_client_rotation_is_available(ec, i))
                    {
                       EDBG(ec, "Failed to set zone rotation: not able to rotate given angle %d", i);
                       can_rotate = EINA_FALSE;
                       break;
                    }
               }

             if (can_rotate == EINA_TRUE)
               {
                  angle = i;
                  DBG("Set rotation of zone according to common angle of clients: %d", angle);
                  break;
               }
          }
     }
   else
     {
        DBG("Set rotation of zone according to angle of exist clients: %d", angle);
        goto do_rotate;
     }


   if (!can_rotate)
     {
        DBG("Failed to find angle to be rotated");
        goto end;
     }

do_rotate:
   EINA_LIST_FOREACH(rot.force_update_list, l, ec)
     {
        if (!eina_list_data_find(target_list, ec))
          target_list = eina_list_append(target_list, ec);
     }

   EINA_LIST_FOREACH(target_list, l, ec)
      ret = e_client_rotation_set(ec, angle);

end:
   if (target_list)
     eina_list_free(target_list);

   TRACE_DS_END();

   DBG("End to set zone rotation: %s >>>", ret ? "Change" : "Stay");
   return ret;
}

static void
_e_client_rotation_change_done(void)
{
   E_Client *ec;

   if (rot.done_timer)
     {
        DBG("Remove rotation Timer by changing done");
        ecore_timer_del(rot.done_timer);
     }
   rot.done_timer = NULL;

   EINA_LIST_FREE(rot.list, ec)
     {
        if (ec->e.state.rot.pending_show)
          {
             ec->e.state.rot.pending_show = 0;
             evas_object_show(ec->frame); // e_client_show(ec);
             e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
          }
        ec->e.state.rot.ang.next = -1;
        ec->e.state.rot.wait_for_done = 0;
     }

   EINA_LIST_FREE(rot.async_list, ec)
     {
        _e_client_rotation_change_message_send(ec);
     }

   rot.list = NULL;
   rot.async_list = NULL;

   if (rot.screen_lock)
     {
        // do call comp_wl's screen unlock
        DBG("RESUME Rendering");
        e_comp_canvas_norender_pop();
        rot.screen_lock = EINA_FALSE;
     }
   e_zone_rotation_update_done(e_zone_current_get());
}

static Eina_Bool
_e_client_rotation_change_done_timeout(void *data __UNUSED__)
{
   WRN("Timeout ROTATION_DONE");
   _e_client_rotation_change_done();
   return ECORE_CALLBACK_CANCEL;
}

static void
_e_client_rotation_change_message_send(E_Client *ec)
{
   int rotation;

   if (!ec) return;

   rotation = ec->e.state.rot.ang.next;
   if (rotation == -1) return;
   if (ec->e.state.rot.wait_for_done) return;

   e_client_rotation_change_request(ec, rotation);
}

static int
_e_client_rotation_curr_next_get(const E_Client *ec)
{
   if (!ec) return -1;

   return ((ec->e.state.rot.ang.next == -1) ?
           ec->e.state.rot.ang.curr : ec->e.state.rot.ang.next);
}

static void
_e_client_event_client_rotation_change_begin_send(E_Client *ec)
{
   E_Event_Client_Rotation_Change_Begin *ev = NULL;
   ev = E_NEW(E_Event_Client_Rotation_Change_End, 1);
   if (ev)
     {
        ev->ec = ec;
        e_object_ref(E_OBJECT(ec));
        ecore_event_add(E_EVENT_CLIENT_ROTATION_CHANGE_BEGIN,
                        ev,
                        _e_client_event_client_rotation_change_begin_free,
                        NULL);
     }
}

static void
_e_client_event_client_rotation_change_begin_free(void *data __UNUSED__,
                                                  void      *ev)
{
   E_Event_Client_Rotation_Change_Begin *e;
   e = ev;
   e_object_unref(E_OBJECT(e->ec));
   E_FREE(e);
}

static void
_e_client_event_client_rotation_change_end_free(void *data __UNUSED__,
                                                void      *ev)
{
   E_Event_Client_Rotation_Change_End *e;
   e = ev;
   e_object_unref(E_OBJECT(e->ec));
   E_FREE(e);
}

static void
_e_zone_event_rotation_change_begin_free(void *data __UNUSED__,
                                         void      *ev)
{
   E_Event_Zone_Rotation_Change_Begin *e = ev;
   e_object_unref(E_OBJECT(e->zone));
   E_FREE(e);
}

static void
_e_zone_event_rotation_change_end_free(void *data __UNUSED__,
                                       void      *ev)
{
   E_Event_Zone_Rotation_Change_End *e = ev;
   e_object_unref(E_OBJECT(e->zone));
   E_FREE(e);
}

static void
_e_zone_event_rotation_change_cancel_free(void *data __UNUSED__,
                                          void      *ev)
{
   E_Event_Zone_Rotation_Change_Cancel *e = ev;
   e_object_unref(E_OBJECT(e->zone));
   E_FREE(e);
}

/* e_client_roation functions */
/**
 * @describe
 *  Get current rotoation state.
 * @param      ec             e_client
 * @return     EINA_FALSE     the state that does not rotating.
 *             EINA_TRUE      the state that rotating.
 */
static Eina_Bool
e_client_rotation_is_progress(const E_Client *ec)
{
   if (!ec) return EINA_FALSE;

   if (ec->e.state.rot.ang.next == -1)
     return EINA_FALSE;
   else
     return EINA_TRUE;
}

/**
 * @describe
 *  Get current rotation angle.
 * @param      ec             e_client
 * @return     int            current angle
 */
static int
e_client_rotation_curr_angle_get(const E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, -1);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, -1);

   return ec->e.state.rot.ang.curr;
}

/**
 * @describe
 *  Get being replaced rotation angle.
 * @param      ec             e_client
 * @return     int            be replaced angle.
 */
static int
e_client_rotation_next_angle_get(const E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, -1);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, -1);

   return ec->e.state.rot.ang.next;
}

/**
 * @describe
 *  Check if this e_client is rotatable to given angle.
 * @param      ec             e_client
 * @param      ang            test angle.
 * @return     EINA_FALSE     can't be rotated.
 *             EINA_TRUE      can be rotated.
 */
static Eina_Bool
e_client_rotation_is_available(const E_Client *ec, int ang)
{
   Eina_Bool ret = EINA_FALSE;
   unsigned int i;

   if (ang < 0) return EINA_FALSE;
   if (!ec->e.state.rot.support)
     goto no_hint;

   if (ec->e.state.rot.preferred_rot == -1)
     {
        if (ec->e.state.rot.available_rots &&
            ec->e.state.rot.count)
          {
             for (i = 0; i < ec->e.state.rot.count; i++)
               {
                  if (ec->e.state.rot.available_rots[i] == ang)
                    {
                       ret = EINA_TRUE;
                       break;
                    }
               }
          }
        else
          goto no_hint;
     }
   else if (ec->e.state.rot.preferred_rot == ang)
     ret = EINA_TRUE;

   return ret;
no_hint:
   return (ang == 0);
}

/**
 * @describe
 *  Set the rotation of the e_client given angle.
 * @param      ec             e_client
 * *param      rotation       angle
 * @return     EINA_TRUE      rotation starts or is already in progress.
 *             EINA_FALSE     fail
 */
static Eina_Bool
e_client_rotation_set(E_Client *ec, int rotation)
{
   Eina_List *list, *l;
   E_Client *child;
   int curr_rot;

   if (!ec) return EINA_FALSE;

   if (rotation < 0) return EINA_FALSE;
   if (!e_client_rotation_is_available(ec, rotation)) return EINA_FALSE;

   TRACE_DS_BEGIN(CLIENT ROTATION SET);
   TRACE_DS_BEGIN(CLINET CURRENT ANGLE SET);

   // in case same with current angle.
   curr_rot = e_client_rotation_curr_angle_get(ec);
   if (curr_rot == rotation)
     {
        if (e_client_rotation_is_progress(ec))
          {
             // cancel the changes in case only doesn't send request.
             if ((!ec->e.state.rot.pending_change_request) &&
                 (!ec->e.state.rot.wait_for_done))
               {
                  _e_client_rotation_list_remove(ec);
                  if (ec->e.state.rot.pending_show)
                    {
                       ec->e.state.rot.pending_show = 0;
                       if (!e_object_is_del(E_OBJECT(ec)))
                         {
                            evas_object_show(ec->frame); // e_client_show(ec);
                            e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
                         }
                    }

                  TRACE_DS_END();
                  TRACE_DS_END();
                  return EINA_FALSE;
               }
             else
               ;
          }
        else
          {
             TRACE_DS_END();
             TRACE_DS_END();
             return EINA_FALSE;
          }
     }
   TRACE_DS_END();

   // in case same with next angle.
   curr_rot = e_client_rotation_next_angle_get(ec);
   if (curr_rot == rotation)
     {
        // if there is reserve angle, remove it.
        if (ec->e.state.rot.ang.reserve != -1)
          {
             ec->e.state.rot.ang.reserve = -1;
          }
        goto finish;
     }

   /* if this e_client is rotating now,
    * it will be rotated to this angle after rotation done.
    */
   if ((ec->e.state.rot.pending_change_request) ||
       (ec->e.state.rot.wait_for_done))
     {
        ec->e.state.rot.ang.reserve = rotation;
        goto finish;
     }

   /* search rotatable window in this window's child */
   list = eina_list_clone(ec->transients);
   EINA_LIST_FOREACH(list, l, child)
     {
        // the window which type is "E_WINDOW_TYPE_NORMAL" will be rotated itself.
        // it shouldn't be rotated by rotation state of parent window.
        if (child->netwm.type == E_WINDOW_TYPE_NORMAL) continue;
        if (e_client_rotation_set(child, rotation))
          {
             ;
          }
        else
          {
             ;
          }
     }
   eina_list_free(list);

   _e_client_event_client_rotation_change_begin_send(ec);

   ec->e.state.rot.pending_change_request = 0;
   ec->e.state.rot.ang.next = rotation;
   ec->changes.rotation = 1;
   EC_CHANGED(ec);

finish:
   /* Now the WM has a rotatable window thus we unset variables about zone rotation cancel */
   if (rot.cancel.state)
     {
        rot.cancel.state = EINA_FALSE;
        rot.cancel.zone = NULL;
     }

   TRACE_DS_END();
   return EINA_TRUE;
}

static void
e_client_rotation_change_request(E_Client *ec, int rotation)
{
   if (!ec) return;
   if (rotation < 0) return;

   // if this window is in withdrawn state, change the state to NORMAL.
   // that's because the window in withdrawn state can't render its canvas.
   // eventually, this window will not send the message of rotation done,
   // even if e request to rotation this window.

   //TODO: e_hint set is really neeed?.
   e_hints_window_visible_set(ec);

   _e_tizen_rotation_send_angle_change(ec, rotation);

   ec->e.state.rot.wait_for_done = 1;

   if ((!rot.async_list) ||
       (!eina_list_data_find(rot.async_list, ec)))
     {
        if (rot.done_timer)
          ecore_timer_del(rot.done_timer);
        rot.done_timer = ecore_timer_add(4.0f,
                                         _e_client_rotation_change_done_timeout,
                                         NULL);
     }
}

/* e_zone_roation functions */
static void
_e_zone_rotation_set_internal(E_Zone *zone, int rot)
{
   E_Event_Zone_Rotation_Change_Begin *ev;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if (!e_config->wm_win_rotation) return;

   ELOGF("ROTATION", "ZONE_ROT |zone:%d|rot curr:%d, rot:%d",
         NULL, NULL, zone->num, zone->rot.curr, rot);

   if ((zone->rot.wait_for_done) ||
       (zone->rot.block_count > 0))
     {
        INF("Pending Zone Rotation: wait_for_done %d block_count %d",
            zone->rot.wait_for_done, zone->rot.block_count);
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
                        ev, _e_zone_event_rotation_change_begin_free, NULL);
     }
}

EINTERN void
e_zone_rotation_set(E_Zone *zone, int rotation)
{
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   TRACE_DS_BEGIN(ZONE ROTATION SET);

   if (rotation == -1)
     {
        zone->rot.unknown_state = EINA_TRUE;
        ELOGF("ROTATION", "ZONE_ROT |UNKOWN SET|zone:%d|rot curr:%d, rot:%d",
              NULL, NULL, zone->num, zone->rot.curr, rotation);
        return;
     }
   else
     zone->rot.unknown_state = EINA_FALSE;

   _e_zone_rotation_set_internal(zone, rotation);
   TRACE_DS_END();
}

static void
e_zone_rotation_sub_set(E_Zone *zone, int rotation)
{
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   ELOGF("ROTATION", "SUB_SET  |zone:%d|rot curr:%d, rot:%d",
         NULL, NULL, zone->num, zone->rot.curr, rotation);

   zone->rot.sub = rotation;

   if ((zone->rot.unknown_state) &&
       (zone->rot.curr != rotation))
     _e_zone_rotation_set_internal(zone, rotation);
}

EINTERN Eina_Bool
e_zone_rotation_block_set(E_Zone *zone, const char *name_hint, Eina_Bool set)
{
   E_Event_Zone_Rotation_Change_Begin *ev;

   E_OBJECT_CHECK_RETURN(zone, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, EINA_FALSE);

   if (set) zone->rot.block_count++;
   else     zone->rot.block_count--;

   ELOGF("ROTATION", "ROT_BLOCK|%s|zone:%d|count:%d|from:%s", NULL, NULL,
         set ? "PAUSE" : "RESUME", zone->num, zone->rot.block_count, name_hint);

   if (zone->rot.block_count <= 0)
     {
        zone->rot.block_count = 0;

        if ((zone->rot.pending) &&
            (zone->rot.curr != zone->rot.next))
          {
             zone->rot.prev = zone->rot.curr;
             zone->rot.curr = zone->rot.next;
             zone->rot.wait_for_done = EINA_TRUE;
             zone->rot.pending = EINA_FALSE;

             ev = E_NEW(E_Event_Zone_Rotation_Change_Begin, 1);
             if (ev)
               {
                  ev->zone = zone;
                  e_object_ref(E_OBJECT(ev->zone));
                  ecore_event_add(E_EVENT_ZONE_ROTATION_CHANGE_BEGIN,
                                  ev, _e_zone_event_rotation_change_begin_free, NULL);

                  ELOGF("ROTATION", "ROT_SET(P|zone:%d|rot:%d",
                        NULL, NULL, zone->num, zone->rot.curr);
               }
          }
     }

   return EINA_TRUE;
}

static void
e_zone_rotation_update_done(E_Zone *zone)
{
   E_Event_Zone_Rotation_Change_End *ev;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   ELOGF("ROTATION", "ROT_DONE |zone:%d|rot:%d",
         NULL, NULL, zone->num, zone->rot.curr);

   ev = E_NEW(E_Event_Zone_Rotation_Change_End, 1);
   if (ev)
     {
        ev->zone = zone;
        e_object_ref(E_OBJECT(ev->zone));
        ecore_event_add(E_EVENT_ZONE_ROTATION_CHANGE_END,
                        ev, _e_zone_event_rotation_change_end_free, NULL);
     }

   zone->rot.wait_for_done = EINA_FALSE;
   if ((zone->rot.pending) &&
       (zone->rot.block_count == 0) &&
       (zone->rot.curr != zone->rot.next))
     {
        zone->rot.prev = zone->rot.curr;
        zone->rot.curr = zone->rot.next;
        zone->rot.wait_for_done = EINA_TRUE;
        zone->rot.pending = EINA_FALSE;

        E_Event_Zone_Rotation_Change_Begin *ev2;
        ev2 = E_NEW(E_Event_Zone_Rotation_Change_Begin, 1);
        if (ev2)
          {
             ev2->zone = zone;
             e_object_ref(E_OBJECT(ev2->zone));
             ecore_event_add(E_EVENT_ZONE_ROTATION_CHANGE_BEGIN,
                             ev2, _e_zone_event_rotation_change_begin_free, NULL);

             ELOGF("ROTATION", "ROT_SET(P|zone:%d|rot:%d",
                   NULL, NULL, zone->num, zone->rot.curr);
          }
     }
}

static void
e_zone_rotation_update_cancel(E_Zone *zone)
{
   E_Event_Zone_Rotation_Change_Cancel *ev;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   zone->rot.wait_for_done = EINA_FALSE;
   if (zone->rot.pending)
     {
        zone->rot.prev = zone->rot.curr;
        zone->rot.curr = zone->rot.next;
        zone->rot.pending = EINA_FALSE;
     }

   ev = E_NEW(E_Event_Zone_Rotation_Change_Cancel, 1);
   if (ev)
     {
        ev->zone = zone;
        e_object_ref(E_OBJECT(ev->zone));
        ecore_event_add(E_EVENT_ZONE_ROTATION_CHANGE_CANCEL,
                        ev, _e_zone_event_rotation_change_cancel_free, NULL);
     }
}

static Eina_Bool
_rot_cb_zone_rotation_change_begin(void *data EINA_UNUSED, int ev_type EINA_UNUSED, E_Event_Zone_Rotation_Change_Begin *ev)
{
   if ((!ev) || (!ev->zone)) return ECORE_CALLBACK_PASS_ON;

   DBG("Rotation Zone Set: Rotation Change Begin");
   if (!_e_client_rotation_zone_set(ev->zone, NULL, NULL))
     {
        /* The WM will decide to cancel zone rotation at idle time.
         * Because, the policy module can make list of rotation windows
         */
        rot.cancel.state = EINA_TRUE;
        rot.cancel.zone = ev->zone;
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_rot_cb_buffer_change(void *data EINA_UNUSED, int ev_type EINA_UNUSED, E_Event_Client *ev)
{
   Policy_Ext_Rotation *rot;

   if (EINA_UNLIKELY(!ev))
     goto end;

   if (EINA_UNLIKELY(!ev->ec))
     goto end;

   rot = eina_hash_find(hash_policy_ext_rotation, &ev->ec);
   if (!rot)
     goto end;

   /* WORKAROUND
    * wl_buffer can be destroyed after attach/damage/frame/commit to wl_surface.
    * we have to handle this case.
    */
   if ((!rot->angle_change_done) || (!e_pixmap_resource_get(ev->ec->pixmap)))
     {
        DBG("Update Buffer in progress of rotation ec '%s'(%p)",
            ev->ec->icccm.name ? ev->ec->icccm.name : "", ev->ec);

        e_pixmap_image_clear(ev->ec->pixmap, EINA_TRUE);
        e_pixmap_resource_set(ev->ec->pixmap, NULL);
     }
   else if (rot->wait_update)
     {
        DBG("Update Buffer After Rotation Done ec '%s'(%p) b %p",
            ev->ec->icccm.name ? ev->ec->icccm.name : "", ev->ec, e_pixmap_resource_get(ev->ec->pixmap));
        _e_client_rotation_list_remove(ev->ec);
        if (ev->ec->e.state.rot.pending_show)
          {
             ev->ec->e.state.rot.pending_show = 0;
             evas_object_show(ev->ec->frame);
             e_comp_object_damage(ev->ec->frame, 0, 0, ev->ec->w, ev->ec->h);
          }
        rot->wait_update = EINA_FALSE;
     }

   if (ev->ec->e.state.rot.pending_show)
     {
        DBG("Buffer Changed: force add update list to send frame until pending show");
        /* consider e_pixmap_image_clear() instead of update_add() */
        e_pixmap_image_clear(ev->ec->pixmap, EINA_TRUE);
     }

end:
   return ECORE_CALLBACK_RENEW;
}

static void
_rot_hook_new_client(void *d EINA_UNUSED, E_Client *ec)
{
   Policy_Ext_Rotation *rot;

   ec->e.state.rot.preferred_rot = -1;
   ec->e.state.rot.type = E_CLIENT_ROTATION_TYPE_NORMAL;
   ec->e.state.rot.ang.next = -1;
   ec->e.state.rot.ang.reserve = -1;
   ec->e.state.rot.pending_show = 0;
   ec->e.state.rot.ang.curr = 0;
   ec->e.state.rot.ang.prev = 0;

   EINA_SAFETY_ON_NULL_RETURN(hash_policy_ext_rotation);

   rot = eina_hash_find(hash_policy_ext_rotation, &ec);
   if (!rot) return;

   if (rot->preferred_angle)
     ec->e.fetch.rot.preferred_rot = 1;

   if (rot->available_angles)
     ec->e.fetch.rot.available_rots = 1;
}

static void
_rot_hook_client_del(void *d EINA_UNUSED, E_Client *ec)
{
   Policy_Ext_Rotation *ext_rot;
   struct wl_resource *res;

   _e_client_rotation_list_remove(ec);
   if (rot.async_list) rot.async_list = eina_list_remove(rot.async_list, ec);

   ec->e.state.rot.preferred_rot = -1;

   if (ec->e.state.rot.available_rots)
     E_FREE(ec->e.state.rot.available_rots);

   ext_rot = eina_hash_find(hash_policy_ext_rotation, &ec);
   if (ext_rot)
     {
        EINA_LIST_FREE(ext_rot->rotation_list, res)
           wl_resource_set_user_data(res, NULL);

        eina_hash_del_by_key(hash_policy_ext_rotation, &ec);
     }
}

static void
_rot_hook_eval_end(void *d EINA_UNUSED, E_Client *ec)
{
   if (ec->changes.rotation)
     {
        E_Zone *zone = ec->zone;

        if (ec->moving) e_client_act_move_end(ec, NULL);

        if ((!zone->rot.block_count) &&
            ((!evas_object_visible_get(ec->frame)) ||
             (!E_INTERSECTS(ec->x, ec->y, ec->w, ec->h, zone->x, zone->y, zone->w, zone->h))))
          {
             // async list add
             rot.async_list = eina_list_append(rot.async_list, ec);
          }
        else
          {
             // sync list add
             rot.list = eina_list_append(rot.list, ec);
             _e_client_rotation_change_message_send(ec);
          }
        rot.fetch = EINA_TRUE;
        ec->changes.rotation = 0;
     }
}

static void
_rot_hook_eval_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   Policy_Ext_Rotation *rot;

   if (!ec) return;

   rot = eina_hash_find(hash_policy_ext_rotation, &ec);
   if (!rot) return;

   if(ec->e.fetch.rot.support)
     {
        ec->e.state.rot.support = 1;

        ec->e.fetch.rot.need_rotation = EINA_TRUE;
        ec->e.fetch.rot.support = 0;
     }
   if (ec->e.fetch.rot.preferred_rot)
     {
        int _prev_preferred_rot;
        _prev_preferred_rot = ec->e.state.rot.preferred_rot;
        ec->e.state.rot.preferred_rot = -1;

        switch (rot->preferred_angle)
          {
             case TIZEN_ROTATION_ANGLE_0:
                ec->e.state.rot.preferred_rot = 0;
                break;
             case TIZEN_ROTATION_ANGLE_90:
                ec->e.state.rot.preferred_rot = 90;
                break;
             case TIZEN_ROTATION_ANGLE_180:
                ec->e.state.rot.preferred_rot = 180;
                break;
             case TIZEN_ROTATION_ANGLE_270:
                ec->e.state.rot.preferred_rot = 270;
                break;
             default:
                break;
          }

        if (_prev_preferred_rot != ec->e.state.rot.preferred_rot)
          ec->e.fetch.rot.need_rotation = EINA_TRUE;

        EDBG(ec, "Fetch Preferred: preferred (prev %d cur %d)",
            _prev_preferred_rot, ec->e.state.rot.preferred_rot);

        ec->e.fetch.rot.preferred_rot = 0;
     }
   if (ec->e.fetch.rot.available_rots)
     {
        Eina_Bool diff = EINA_FALSE;
        int *rots = NULL;
        unsigned int _prev_count = 0, count = 0, i = 0;
        int _prev_rots[4] = { -1, };
        uint32_t available_angles = 0;

        if (ec->e.state.rot.available_rots)
          {
             memcpy(_prev_rots,
                    ec->e.state.rot.available_rots,
                    (sizeof(int) * ec->e.state.rot.count));
          }

        _prev_count = ec->e.state.rot.count;
        ec->e.state.rot.count = 0;

        /* check avilable_angles */
        if (rot->available_angles & TIZEN_ROTATION_ANGLE_0) count++;
        if (rot->available_angles & TIZEN_ROTATION_ANGLE_90) count++;
        if (rot->available_angles & TIZEN_ROTATION_ANGLE_180) count++;
        if (rot->available_angles & TIZEN_ROTATION_ANGLE_270) count++;

        if (count != 0)
          rots = (int*)E_NEW(int, count);

        if (!rots)
          {
             if (ec->e.state.rot.available_rots)
               {
                  /* restore previous rotation hints */
                  memcpy(ec->e.state.rot.available_rots, _prev_rots, (sizeof(int) * _prev_count));
               }
             goto end_fetch_rot;
          }

        if (ec->e.state.rot.available_rots)
          E_FREE(ec->e.state.rot.available_rots);

        available_angles = rot->available_angles;

        if ((count > 0) && (rots))
          {
             for (i = 0; i < count; i++)
              {
                  if (available_angles & TIZEN_ROTATION_ANGLE_0)
                    {
                       rots[i] = 0;
                       available_angles = available_angles & ~TIZEN_ROTATION_ANGLE_0;
                    }
                  else if (available_angles & TIZEN_ROTATION_ANGLE_90)
                    {
                       rots[i] = 90;
                       available_angles = available_angles & ~TIZEN_ROTATION_ANGLE_90;
                    }
                  else if (available_angles & TIZEN_ROTATION_ANGLE_180)
                    {
                       rots[i] = 180;
                       available_angles = available_angles & ~TIZEN_ROTATION_ANGLE_180;
                    }
                  else if (available_angles & TIZEN_ROTATION_ANGLE_270)
                    {
                       rots[i] = 270;
                       available_angles = available_angles & ~TIZEN_ROTATION_ANGLE_270;
                    }
               }

             ec->e.state.rot.available_rots = rots;
             ec->e.state.rot.count = count;

             if (_prev_count != count) diff = EINA_TRUE;

             for (i = 0; i < count; i++)
               {
                  if ((!diff) && (_prev_rots[i] != rots[i]))
                    {
                       diff = EINA_TRUE;
                       break;
                    }
               }
           }
        /* check avilable_angles end*/

        /* Print fetch information */
          {
             Eina_Strbuf *b = eina_strbuf_new();

             EINF(ec, "Fetch Available");
             if (_prev_count > 0)
               {
                  for (i = 0; i < _prev_count; i++)
                    eina_strbuf_append_printf(b, "%d ", _prev_rots[i]);
                  INF("\tprev %s", eina_strbuf_string_get(b));
                  eina_strbuf_reset(b);
               }

             for (i = 0; i < count; i++)
               eina_strbuf_append_printf(b, "%d ", rots[i]);
             INF("\tcur %s", eina_strbuf_string_get(b));

             eina_strbuf_free(b);
          }

        if (diff) ec->e.fetch.rot.need_rotation = EINA_TRUE;
        ec->e.fetch.rot.available_rots = 0;
     }
end_fetch_rot:

   rot->hint_fetch = 1;
   if ((ec->new_client) && (ec->e.state.rot.pending_show))
     {
        ec->e.state.rot.pending_show = 0;
        evas_object_show(ec->frame);
        if (!ec->changes.rotation)
          e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
     }
   else if ((evas_object_visible_get(ec->frame) && (ec->e.fetch.rot.need_rotation)))
     {
        DBG("Rotation Zone Set: Fetch Hint");
        _e_client_rotation_zone_set(ec->zone, NULL, NULL);
     }

   if (ec->e.fetch.rot.need_rotation)
     ec->e.fetch.rot.need_rotation = EINA_FALSE;
}

static Eina_Bool
_rot_intercept_hook_show_helper(void *d EINA_UNUSED, E_Client *ec)
{
   Policy_Ext_Rotation *rot;

   rot = eina_hash_find(hash_policy_ext_rotation, &ec);
   if (!rot)
     return EINA_TRUE;

   if (e_pixmap_type_get(ec->pixmap) == E_PIXMAP_TYPE_EXT_OBJECT)
     return EINA_TRUE;

   if (!rot->hint_fetch)
     {
        /* need to fetch rotation hint. */
        ec->e.state.rot.pending_show = 1;
        EC_CHANGED(ec);
        return EINA_FALSE;
     }

   if (ec->e.state.rot.pending_show)
     return EINA_FALSE;

   _e_client_rotation_zone_set(ec->zone, ec, NULL);
   if (ec->changes.rotation)
     {
        EDBG(ec, "Postpone show: ang %d", ec->e.state.rot.ang.next);
        e_pixmap_image_clear(ec->pixmap, 1);
        ec->e.state.rot.pending_show = 1;
        /* to be invoked 'eval_end' */
        EC_CHANGED(ec);
        return EINA_FALSE;
     }

    return EINA_TRUE;
}

static Eina_Bool
_rot_intercept_hook_hide(void *d EINA_UNUSED, E_Client *ec)
{
   // TODO: Add VKBD Hide, VKBD Parent Hide routine.
   // clear pending_show, because this window is hidden now.
   ec->e.state.rot.pending_show = 0;

   // for rotating ec in the force_update_list
   _e_client_rotation_zone_set(ec->zone, NULL, ec);

   return EINA_TRUE;
}

static Eina_Bool
_rot_cb_idle_enterer(void *data EINA_UNUSED)
{
   if (rot.cancel.state)
     {
        /* there is no border which supports window manager rotation */
        e_zone_rotation_update_cancel(rot.cancel.zone);
        rot.cancel.state = EINA_FALSE;
        rot.cancel.zone = NULL;
     }

   if (rot.fetch)
     {
        //TODO: consider rot.msgs , X WM use it for e_client message

        // if there is windows over 2 that has to be rotated or is existed window needs resizing,
        // lock the screen.
        // but, DO NOT lock the screen when rotation block state
        if (eina_list_count(rot.list) > 1)
          {
             Eina_List *l;
             E_Client *ec;
             Eina_Bool rot_block = EINA_FALSE;

             EINA_LIST_FOREACH(rot.list, l, ec)
               {
                  if (!ec->zone) continue;
                  if (ec->zone->rot.block_count)
                    {
                       rot_block = EINA_TRUE;
                    }
               }
             if ((!rot.screen_lock) && (!rot_block))
               {
                  DBG("STOP Rendering");

                  EINA_LIST_FOREACH(rot.list, l, ec)
                     e_pixmap_image_clear(ec->pixmap, 1);

                  e_comp_canvas_norender_push();
                  rot.screen_lock = EINA_TRUE;
               }
          }
        else if (eina_list_count(rot.list) == 0)
          {
             E_Client *ec;
             Eina_List *zlist = NULL;
             Eina_List *l = NULL;
             E_Zone *zone = NULL;

             if (rot.async_list)
               {
                  EINA_LIST_FREE(rot.async_list, ec)
                    {
                       if (!eina_list_data_find(zlist, ec->zone))
                         zlist = eina_list_append(zlist, ec->zone);
                       _e_client_rotation_change_message_send(ec);
                    }

                  EINA_LIST_FOREACH(zlist, l, zone)
                    e_zone_rotation_update_cancel(zone);
                  if (zlist)
                    eina_list_free(zlist);
               }
          }

        rot.fetch = EINA_FALSE;
     }

   return ECORE_CALLBACK_RENEW;
}

#endif //#ifdef HAVE_WAYLAND_ONLY

Eina_Bool
e_mod_rot_wl_init(void)
{
#ifdef HAVE_WAYLAND_ONLY
   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp_wl->wl.disp, EINA_FALSE);

   if (!wl_global_create(e_comp_wl->wl.disp, &tizen_policy_ext_interface, 1,
                         NULL, _e_tizen_policy_ext_bind_cb))
     {
        ERR("Could not add tizen_policy_ext to wayland globals: %m");
        return EINA_FALSE;
     }

   hash_policy_ext_rotation = eina_hash_pointer_new(free);

   E_LIST_HANDLER_APPEND(rot_handlers, E_EVENT_ZONE_ROTATION_CHANGE_BEGIN,
                         _rot_cb_zone_rotation_change_begin, NULL);
   E_LIST_HANDLER_APPEND(rot_handlers, E_EVENT_CLIENT_BUFFER_CHANGE,
                         _rot_cb_buffer_change, NULL);

   E_CLIENT_HOOK_APPEND(rot_hooks, E_CLIENT_HOOK_NEW_CLIENT,
                        _rot_hook_new_client, NULL);
   E_CLIENT_HOOK_APPEND(rot_hooks, E_CLIENT_HOOK_DEL,
                        _rot_hook_client_del, NULL);
   E_CLIENT_HOOK_APPEND(rot_hooks, E_CLIENT_HOOK_EVAL_END,
                        _rot_hook_eval_end, NULL);
   E_CLIENT_HOOK_APPEND(rot_hooks, E_CLIENT_HOOK_EVAL_FETCH,
                        _rot_hook_eval_fetch, NULL);

   E_COMP_OBJECT_INTERCEPT_HOOK_APPEND(rot_intercept_hooks,
                                       E_COMP_OBJECT_INTERCEPT_HOOK_SHOW_HELPER,
                                       _rot_intercept_hook_show_helper, NULL);
   E_COMP_OBJECT_INTERCEPT_HOOK_APPEND(rot_intercept_hooks,
                                       E_COMP_OBJECT_INTERCEPT_HOOK_HIDE,
                                       _rot_intercept_hook_hide, NULL);

   rot_idle_enterer = ecore_idle_enterer_add(_rot_cb_idle_enterer, NULL);
#endif
   return EINA_TRUE;
}

void
e_mod_rot_wl_shutdown(void)
{
#ifdef HAVE_WAYLAND_ONLY
   E_FREE_FUNC(hash_policy_ext_rotation, eina_hash_free);
   E_FREE_FUNC(rot.force_update_list, eina_list_free);

   E_FREE_LIST(rot_hooks, e_client_hook_del);
   E_FREE_LIST(rot_handlers, ecore_event_handler_del);
   E_FREE_LIST(rot_intercept_hooks, e_comp_object_intercept_hook_del);

   if (rot_idle_enterer)
     {
         ecore_idle_enterer_del(rot_idle_enterer);
         rot_idle_enterer = NULL;
     }
#endif
}

EINTERN void
e_mod_pol_rotation_force_update_add(E_Zone *zone EINA_UNUSED, E_Client *ec)
{
   rot.force_update_list = eina_list_append(rot.force_update_list, ec);
}

EINTERN void
e_mod_pol_rotation_force_update_del(E_Zone *zone EINA_UNUSED, E_Client *ec)
{
   rot.force_update_list = eina_list_remove(rot.force_update_list, ec);
}
