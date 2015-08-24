/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
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
#include "e_mod_atoms.h"
#include "e_mod_utils.h"

typedef struct _E_Client_Rotation E_Client_Rotation;

struct _E_Client_Rotation
{
   Eina_List     *list;
   Eina_List     *async_list;

   Eina_Bool      wait_prepare_done;
   Ecore_Timer   *prepare_timer;
   Ecore_Timer   *done_timer;

   Ecore_Window   vkbd_ctrl_win;
   E_Client      *vkbd;
   E_Client      *vkbd_prediction;
   E_Client      *vkbd_parent;

   /* vkbd show/hide preprare */
   Eina_Bool      vkbd_show_prepare_done;
   Ecore_Timer   *vkbd_show_prepare_timer;
   Ecore_Timer   *vkbd_show_timer;

   Eina_Bool      vkbd_hide_prepare_done;
   Ecore_Timer   *vkbd_hide_prepare_timer;
   Ecore_Timer   *vkbd_hide_timer;

   Eina_Bool      screen_lock;
   Eina_Bool      fetch;
   Eina_List     *msgs;

   struct {
     Eina_Bool    state;
     E_Zone      *zone;
   } cancel;
};

/* local subsystem variables */
static E_Client_Rotation rot =
{
   NULL,
   NULL,
   EINA_FALSE,
   NULL,
   NULL,
   0,
   NULL,
   NULL,
   NULL,
   EINA_FALSE,
   NULL,
   NULL,
   EINA_FALSE,
   NULL,
   NULL,
   EINA_FALSE,
   EINA_FALSE,
   NULL,
   {
      EINA_FALSE,
      NULL,
   }
};

static Eina_List *rot_handlers = NULL;
static Eina_List *rot_hooks = NULL;
static Eina_List *rot_intercept_hooks = NULL;
static Ecore_Idle_Enterer *rot_idle_enterer = NULL;

/* local subsystem e_client_rotation related functions */
static Eina_Bool _e_client_rotation_change_prepare_timeout(void *data);
static void      _e_client_rotation_list_send(void);
static void      _e_client_rotation_change_message_send(E_Client *ec);
static Eina_Bool _e_client_rotation_change_done_timeout(void *data);
static void      _e_client_rotation_change_done(void);
static Eina_Bool _e_client_rotation_geom_get(E_Client  *ec,
                                             E_Zone    *zone,
                                             int        ang,
                                             int       *x,
                                             int       *y,
                                             int       *w,
                                             int       *h,
                                             Eina_Bool *move);
static void      _e_client_rotation_list_remove(E_Client *ec);
static Eina_Bool _e_client_rotation_prepare_send(E_Client *ec, int rotation);
static Eina_Bool _e_client_rotation_zone_set(E_Zone *zone);
static int       _e_client_rotation_curr_next_get(const E_Client *ec);
static Eina_Bool _e_client_rotation_is_dependent_parent(const E_Client *ec);
static Eina_Bool _e_client_is_vkbd(E_Client *ec);
static Eina_Bool _e_client_vkbd_show_prepare_timeout(void *data);
static Eina_Bool _e_client_vkbd_hide_prepare_timeout(void *data);
static void      _e_client_vkbd_show(E_Client *ec);
static void      _e_client_vkbd_hide(E_Client *ec, Eina_Bool clean);
static void      _e_client_event_client_rotation_change_begin_free(void *data,
                                                                   void *ev);
static void      _e_client_event_client_rotation_change_end_free(void *data,
                                                                 void *ev);
static void      _e_client_event_client_rotation_change_begin_send(E_Client *ec);

/* local subsystem e_zone_rotation related functions */
static void      _e_zone_rotation_set_internal(E_Zone *zone, int rotation);
static void      _e_zone_event_rotation_change_begin_free(void *data,
                                                          void *ev);
static void      _e_zone_event_rotation_change_cancel_free(void *data,
                                                           void *ev);
static void      _e_zone_event_rotation_change_end_free(void *data,
                                                        void *ev);
/* e_client_roation functions */
static Eina_Bool  e_client_rotation_is_progress(const E_Client *ec);
static Eina_Bool  e_client_rotation_is_available(const E_Client *ec, int ang);
static Eina_List *e_client_rotation_available_list_get(const E_Client *ec);
static int        e_client_rotation_next_angle_get(const E_Client *ec);
static int        e_client_rotation_prev_angle_get(const E_Client *ec);
static int        e_client_rotation_recommend_angle_get(const E_Client *ec);
static Eina_Bool  e_client_rotation_set(E_Client *ec, int rotation);
static void       e_client_rotation_change_request(E_Client *ec, int rotation);

/* e_zone_roation functions */
static void      e_zone_rotation_set(E_Zone *zone, int rot);
static void      e_zone_rotation_sub_set(E_Zone *zone, int rotation);
static int       e_zone_rotation_get(E_Zone *zone);
static Eina_Bool e_zone_rotation_block_set(E_Zone *zone, const char *name_hint, Eina_Bool set);
static void      e_zone_rotation_update_done(E_Zone *zone);
static void      e_zone_rotation_update_cancel(E_Zone *zone);

/* e_client rotation internal callbacks */
static Eina_Bool _rot_hook_client_free_intern(E_Client *ec);
static Eina_Bool _rot_hook_client_del_intern(E_Client *ec);
static void      _rot_cb_evas_show_intern(E_Client *ec);
static Eina_Bool _rot_hook_eval_end_intern(E_Client *ec);
static Eina_Bool _rot_cb_idle_enterer_intern(void);
static Eina_Bool _rot_hook_new_client_intern(E_Client *ec);
static Eina_Bool _rot_cb_zone_rotation_change_begin_intern(E_Event_Zone_Rotation_Change_Begin *ev);
static Eina_Bool _rot_intercept_hook_show_helper_intern(E_Client *ec);
static Eina_Bool _rot_intercept_hook_hide_intern(E_Client *ec);
static Eina_Bool _rot_cb_window_configure_intern(Ecore_X_Event_Window_Configure *ev);
static Eina_Bool _rot_cb_window_property_intern(Ecore_X_Event_Window_Property *ev);
static Eina_Bool _rot_cb_window_message_intern(Ecore_X_Event_Client_Message *ev);
static Eina_Bool _rot_hook_eval_fetch_intern(E_Client *ec);

/* e_client event, hook, intercept callbacks */
static void      _rot_hook_new_client(void *d EINA_UNUSED, E_Client *ec);
static void      _rot_hook_new_client_post(void *d EINA_UNUSED, E_Client *ec);
static void      _rot_hook_client_del(void *d EINA_UNUSED, E_Client *ec);
static void      _rot_hook_eval_end(void *d EINA_UNUSED, E_Client *ec);
static void      _rot_hook_eval_fetch(void *d EINA_UNUSED, E_Client *ec);
static Eina_Bool _rot_intercept_hook_show_helper(void *d EINA_UNUSED, E_Client *ec);
static Eina_Bool _rot_intercept_hook_hide(void *d EINA_UNUSED, E_Client *ec);
static Eina_Bool _rot_cb_window_property(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Property *ev);
static Eina_Bool _rot_cb_window_configure(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Configure *ev);
static Eina_Bool _rot_cb_window_message(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Client_Message *ev);
static Eina_Bool _rot_cb_zone_rotation_change_begin(void *data EINA_UNUSED, int ev_type EINA_UNUSED, E_Event_Zone_Rotation_Change_Begin *ev);
static Eina_Bool _rot_cb_idle_enterer(void *data EINA_UNUSED);
static void      _rot_cb_evas_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);

/* local subsystem e_client_rotation related functions */
static Eina_Bool
_e_client_vkbd_state_check(E_Client *ec,
                           Eina_Bool show)
{
   Eina_Bool res = EINA_TRUE;
   if ((rot.vkbd) && (rot.vkbd == ec))
     {
        if (show)
          {
             if ((rot.vkbd_hide_prepare_done) ||
                 (rot.vkbd_hide_prepare_timer))
               res = EINA_FALSE;
          }
        else
          {
             if ((rot.vkbd_show_prepare_done) ||
                 (rot.vkbd_show_prepare_timer))
               res = EINA_FALSE;
          }
     }
   return res;
}

static Eina_Bool
_e_client_vkbd_show_timeout(void *data)
{
   E_Client *ec = data;
   if ((ec) && ((E_OBJECT(ec)->type) == (E_CLIENT_TYPE)))
     {
        if (_e_client_vkbd_state_check(ec, EINA_TRUE))
          {
             if (rot.vkbd_ctrl_win)
               {
                  ecore_x_e_virtual_keyboard_state_set
                    (rot.vkbd_ctrl_win, ECORE_X_VIRTUAL_KEYBOARD_STATE_ON);
               }
          }
     }

   rot.vkbd_show_prepare_done = EINA_FALSE;

   if (rot.vkbd_show_prepare_timer)
     ecore_timer_del(rot.vkbd_show_prepare_timer);
   rot.vkbd_show_prepare_timer = NULL;

   if (rot.vkbd_show_timer)
     ecore_timer_del(rot.vkbd_show_timer);
   rot.vkbd_show_timer = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_client_vkbd_hide_timeout(void *data)
{
   E_Client *ec = data;
   int unref_count = 0;

   rot.vkbd_hide_timer = NULL;
   unref_count++;

   if (rot.vkbd_hide_prepare_timer)
     {
        ecore_timer_del(rot.vkbd_hide_prepare_timer);
        unref_count++;
     }
   rot.vkbd_hide_prepare_timer = NULL;

   if (_e_client_vkbd_state_check(ec, EINA_FALSE))
     {
        if (rot.vkbd_ctrl_win)
          {
             ecore_x_e_virtual_keyboard_state_set
                (rot.vkbd_ctrl_win, ECORE_X_VIRTUAL_KEYBOARD_STATE_OFF);
          }
     }

   rot.vkbd_hide_prepare_done = EINA_FALSE;

   while (unref_count)
     {
        e_object_unref(E_OBJECT(ec));
        unref_count--;
     }

   return ECORE_CALLBACK_CANCEL;
}

static void
_e_client_vkbd_show(E_Client *ec)
{
   rot.vkbd_show_prepare_done = EINA_TRUE;
   if (rot.vkbd_show_prepare_timer)
     ecore_timer_del(rot.vkbd_show_prepare_timer);
   rot.vkbd_show_prepare_timer = NULL;
   if (rot.vkbd_show_timer)
     ecore_timer_del(rot.vkbd_show_timer);
   rot.vkbd_show_timer = NULL;
   if ((ec) && (!e_object_is_del(E_OBJECT(ec))))
     {
        evas_object_show(ec->frame);// e_client_show(ec)
        rot.vkbd_show_timer = ecore_timer_add(1.0f, _e_client_vkbd_show_timeout, ec);
     }
}

static void
_e_client_vkbd_hide(E_Client *ec, Eina_Bool clean)
{
   int unref_count = 0;

   rot.vkbd_hide_prepare_done = EINA_TRUE;

   if (clean)
     {
        if (rot.vkbd_hide_prepare_timer)
          {
             ecore_timer_del(rot.vkbd_hide_prepare_timer);
             unref_count++;
          }
        rot.vkbd_hide_prepare_timer = NULL;

        if (rot.vkbd_hide_timer)
          {
             ecore_timer_del(rot.vkbd_hide_timer);
             unref_count++;
          }
        rot.vkbd_hide_timer = NULL;

        if ((ec) && ((E_OBJECT(ec)->type) == (E_CLIENT_TYPE)))
          {
             evas_object_hide(ec->frame); // e_client_hide(ec)

             e_object_ref(E_OBJECT(ec));
             if (!e_object_is_del(E_OBJECT(ec)))
               {
                  e_object_del(E_OBJECT(ec)); // is it right? e_object_unref(E_OBJECT(ec));??
               }
             rot.vkbd_hide_timer = ecore_timer_add(0.03f, _e_client_vkbd_hide_timeout, ec);
          }
     }
   else
     {
        if ((ec) && ((E_OBJECT(ec)->type) == (E_CLIENT_TYPE)))
          {
             evas_object_hide(ec->frame);// e_client_hide(ec);
          }
     }

   while (unref_count)
     {
        e_object_unref(E_OBJECT(ec));
        unref_count--;
     }
}

static Eina_Bool
_e_client_vkbd_show_prepare_timeout(void *data)
{
   E_Client *ec = data;
   if ((ec) && (!e_object_is_del(E_OBJECT(ec))))
     {
        _e_client_vkbd_show(ec);
     }
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_client_vkbd_hide_prepare_timeout(void *data)
{
   E_Client *ec = data;
   int unref_count = 0;

   rot.vkbd_hide_prepare_timer = NULL;
   unref_count++;

   if (rot.vkbd_hide_timer)
     {
        ecore_timer_del(rot.vkbd_hide_timer);
        unref_count++;
     }
   rot.vkbd_hide_timer = NULL;

   _e_client_vkbd_hide(ec, EINA_TRUE);

   while (unref_count)
     {
        e_object_unref(E_OBJECT(ec));
        unref_count--;
     }

   return ECORE_CALLBACK_CANCEL;
}

#define SIZE_EQUAL_TO_ZONE(a, z) \
   ((((a)->w) == ((z)->w)) &&    \
    (((a)->h) == ((z)->h)))

static int
_e_client_rotation_curr_next_get(const E_Client *ec)
{
   if (!ec) return -1;

   return ((ec->e.state.rot.ang.next == -1) ?
           ec->e.state.rot.ang.curr : ec->e.state.rot.ang.next);
}

static int
_prev_angle_get(Ecore_Window win)
{
   int ret, count = 0, ang = -1;
   unsigned char* data = NULL;

   ret = ecore_x_window_prop_property_get
      (win, ECORE_X_ATOM_E_ILLUME_ROTATE_WINDOW_ANGLE,
      ECORE_X_ATOM_CARDINAL, 32, &data, &count);

   if ((ret) && (data) && (count))
     ang = ((int *)data)[0];
   if (data) free(data);
   return ang;
}

/* check whether virtual keyboard is visible on the zone */
static Eina_Bool
_e_client_is_vkbd(E_Client *ec)
{
   if ((rot.vkbd_ctrl_win) &&
       (rot.vkbd == ec) &&
       (!e_object_is_del(E_OBJECT(rot.vkbd))) &&
       (rot.vkbd->zone == ec->zone) &&
       (E_INTERSECTS(ec->zone->x, ec->zone->y,
                     ec->zone->w, ec->zone->h,
                     rot.vkbd->x, rot.vkbd->y,
                     rot.vkbd->w, rot.vkbd->h)))
     {
        return EINA_TRUE;
     }

   return EINA_FALSE;
}

static Eina_Bool
_e_client_rotation_is_dependent_parent(const E_Client *ec)
{
   if (!ec) return EINA_FALSE;
   if ((!ec->parent) || (!evas_object_visible_get(ec->parent->frame))) return EINA_FALSE;
   if (ec->netwm.type == E_WINDOW_TYPE_NORMAL) return EINA_FALSE;
   if ((!ec->e.state.rot.support) &&
       (!ec->e.state.rot.app_set)) return EINA_FALSE;
   return EINA_TRUE;
}

static Eina_Bool
_e_client_rotation_zone_set(E_Zone *zone)
{
   E_Client *ec = NULL;
   Eina_Bool res = EINA_FALSE;
   Eina_Bool ret = EINA_FALSE;
   E_Zone *ez;
   Eina_List *zl;

   /* step 1. make the list needs to be rotated. */
   EINA_LIST_FOREACH(e_comp->zones, zl, ez)
     {
        Eina_List *l;

        if (ez != zone) continue;

        EINA_LIST_REVERSE_FOREACH(zone->comp->clients, l, ec)
          {
             if(ec->zone != zone) continue;

             // if this window has parent and window type isn't "ECORE_X_WINDOW_TYPE_NORMAL",
             // it will be rotated when parent do rotate itself.
             // so skip here.
             if ((ec->parent) &&
                 (ec->netwm.type != E_WINDOW_TYPE_NORMAL)) continue;

             // default type is "E_CLIENT_ROTATION_TYPE_NORMAL",
             // but it can be changed to "E_CLIENT_ROTATION_TYPE_DEPENDENT" by illume according to its policy.
             // if it's not normal type window, will be rotated by illume.
             // so skip here.
             if (ec->e.state.rot.type != E_CLIENT_ROTATION_TYPE_NORMAL) continue;

             if ((!evas_object_visible_get(ec->frame)) ||
                 (!E_INTERSECTS(ec->zone->x, ec->zone->y, ec->zone->w, ec->zone->h,
                                ec->x, ec->y, ec->w, ec->h))) continue;

             res = e_client_rotation_set(ec, zone->rot.curr);
             if (!res)
               {
                  ;
               }
             else ret = EINA_TRUE;
          }
     }

   return ret;
}

static void
_e_client_rotation_change_message_send(E_Client *ec)
{
   int rotation;
   Eina_Bool resize = EINA_FALSE;

   if (!ec) return;

   rotation = ec->e.state.rot.ang.next;
   if (rotation == -1) return;
   if (ec->e.state.rot.wait_for_done) return;

   resize = _e_client_rotation_prepare_send(ec, rotation);

   // new protocol
   if ((resize) && (!ec->e.state.rot.geom_hint))
     {
        e_client_rotation_change_request(ec, rotation);
     }
   // original protocol
   else
     {
        ec->e.state.rot.pending_change_request = resize;

        if (!resize)
          {
             e_client_rotation_change_request(ec, rotation);
          }
     }
}

static void
_e_client_rotation_list_send(void)
{
   Eina_List *l;
   E_Client *ec;

   if (!rot.list) return;
   if (eina_list_count(rot.list) <= 0) return;

   EINA_LIST_FOREACH(rot.list, l, ec)
     {
        if (ec->e.state.rot.pending_change_request) continue;
        if (ec->e.state.rot.wait_for_done) continue;

        _e_client_rotation_change_message_send(ec);
     }
}

static Eina_Bool
_e_client_rotation_change_prepare_timeout(void *data)
{
   E_Zone *zone = data;

   if ((zone) && (rot.wait_prepare_done))
     {
        if (rot.list)
          {
             _e_client_rotation_list_send();
             if (rot.prepare_timer)
               ecore_timer_del(rot.prepare_timer);
             rot.prepare_timer = NULL;
             rot.wait_prepare_done = EINA_FALSE;
          }
     }
   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_e_client_rotation_change_done_timeout(void *data __UNUSED__)
{
   _e_client_rotation_change_done();
   return ECORE_CALLBACK_CANCEL;
}

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

static void
_e_client_rotation_change_done(void)
{
   E_Manager *m = NULL;
   E_Client *ec;

   if (rot.prepare_timer) ecore_timer_del(rot.prepare_timer);
   rot.prepare_timer = NULL;

   rot.wait_prepare_done = EINA_FALSE;

   if (rot.done_timer) ecore_timer_del(rot.done_timer);
   rot.done_timer = NULL;

   EINA_LIST_FREE(rot.list, ec)
     {
        if (ec->e.state.rot.pending_show)
          {
             ec->e.state.rot.pending_show = 0;
             evas_object_show(ec->frame); // e_client_show(ec);
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

   m = e_manager_current_get();
   if (rot.screen_lock)
     {
        // do call comp_x's screen unlock
        rot.screen_lock = EINA_FALSE;
     }
   e_zone_rotation_update_done(e_util_zone_current_get(m));
}

static Eina_Bool
_e_client_rotation_prepare_send(E_Client *ec, int rotation)
{
   E_Zone *zone = ec->zone;
   int x, y, w, h;
   int cw = ec->client.w, ch = ec->client.h;
   Eina_Bool move = EINA_FALSE;
   Eina_Bool hint = EINA_FALSE;
   Eina_Bool resize = EINA_FALSE;

   x = ec->x;
   y = ec->y;
   w = cw;
   h = ch;

   if (SIZE_EQUAL_TO_ZONE(ec, zone)) goto end;

   hint = _e_client_rotation_geom_get(ec, ec->zone, rotation,
                                      &x, &y, &w, &h, &move);

   if ((!hint) || (cw != w) || (ch != h)) resize = EINA_TRUE;

end:
 
   ecore_x_e_window_rotation_change_prepare_send
      (e_client_util_win_get(ec), rotation, resize, w, h);

   if (((move) && ((ec->x !=x) || (ec->y !=y))) ||
       ((resize) && ((cw != w) || (ch != h))))
     {
#if 0
     // need call: _e_comp_x_client_move_resize_send(); ?
        _e_client_move_resize_internal(ec, x, y, w, h, EINA_TRUE, move);
#endif
        //e_client_util_move_resize_without_frame(ec, x, y, w, h); //check: this function work correct or not.
        evas_object_move(ec->frame, x, y);
        e_client_util_resize_without_frame(ec, w, h); 
        //evas_object_resize(ec->frame, w, h);
     }
   return resize;
}

static Eina_Bool
_e_client_rotation_geom_get(E_Client  *ec,
                            E_Zone    *zone,
                            int        ang,
                            int       *x,
                            int       *y,
                            int       *w,
                            int       *h,
                            Eina_Bool *move)
{
   Eina_Bool res = EINA_FALSE;
   int _x, _y, _w, _h;
   int cw = ec->client.w, ch = ec->client.h;
  
   _x = ec->x;
   _y = ec->y;
   _w = cw;
   _h = ch;

   if (x) *x = ec->x;
   if (y) *y = ec->y;
   if (w) *w = _w;
   if (h) *h = _h;
   if (move) *move = EINA_TRUE;

   if (ec->e.state.rot.geom_hint)
     {
        switch (ang)
          {
           case   0:
              _w = ec->e.state.rot.geom[0].w;
              _h = ec->e.state.rot.geom[0].h;
              if (_w == 0) _w = cw;
              if (_h == 0) _h = ch;
              _x = 0; _y = zone->h - _h;
              break;
           case  90:
              _w = ec->e.state.rot.geom[1].w;
              _h = ec->e.state.rot.geom[1].h;
              if (_w == 0) _w = cw;
              if (_h == 0) _h = ch;
              _x = zone->w - _w; _y = 0;
              break;
           case 180:
              _w = ec->e.state.rot.geom[2].w;
              _h = ec->e.state.rot.geom[2].h;
              if (_w == 0) _w = cw;
              if (_h == 0) _h = ch;
              _x = 0; _y = 0;
              break;
           case 270:
              _w = ec->e.state.rot.geom[3].w;
              _h = ec->e.state.rot.geom[3].h;
              if (_w == 0) _w = cw;
              if (_h == 0) _h = ch;
              _x = 0; _y = 0;
              break;
          }

        if (x) *x = _x;
        if (y) *y = _y;
        if (w) *w = _w;
        if (h) *h = _h;

        if (!((rot.vkbd) && (rot.vkbd == ec)))
          {
             if (x) *x = ec->x;
             if (y) *y = ec->y;
             if (move) *move = EINA_FALSE;
          }

        res = EINA_TRUE;
     }

   if (res)
     {
        _x = 0; _y = 0; _w = 0; _h = 0;
        if (x) _x = *x;
        if (y) _y = *y;
        if (w) _w = *w;
        if (h) _h = *h;
     }

   return res;
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

/* local subsystem e_zone_rotation related functions */
static void
_e_zone_rotation_set_internal(E_Zone *zone, int rot)
{
   E_Event_Zone_Rotation_Change_Begin *ev;

   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

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
                        ev, _e_zone_event_rotation_change_begin_free, NULL);
     }
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
_e_zone_event_rotation_change_cancel_free(void *data __UNUSED__,
                                          void      *ev)
{
   E_Event_Zone_Rotation_Change_Cancel *e = ev;
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

/* e_client_roation functions */
static void
e_client_rotation_change_request(E_Client *ec, int rotation)
{
   if (!ec) return;
   if (rotation < 0) return;

   // if this window is in withdrawn state, change the state to NORMAL.
   // that's because the window in withdrawn state can't render its canvas.
   // eventually, this window will not send the message of rotation done,
   // even if e request to rotation this window.
   e_hints_window_visible_set(ec);

   ecore_x_e_window_rotation_change_request_send(e_client_util_win_get(ec), rotation);

#if 0
   if (ec->e.state.deiconify_approve.pending)
     _e_client_deiconify_approve_send_pending_end(ec);
#endif
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
 *  Check if this e_client is rotatable to given angle.
 * @param      ec             e_client
 * @param      ang            test angle.
 * @return     EINA_FALSE     can't be rotated.
 *             EINA_TRUE      can be rotated.
 */
static Eina_Bool
e_client_rotation_is_available(const E_Client *ec, int ang)
{
   if (!ec) return EINA_FALSE;

   if (ang < 0) goto fail;
   if ((!ec->e.state.rot.support) && (!ec->e.state.rot.app_set)) goto fail;
   if (e_object_is_del(E_OBJECT(ec))) goto fail;

   if (ec->e.state.rot.preferred_rot == -1)
     {
        unsigned int i;

        if (ec->e.state.rot.app_set)
          {
             if (ec->e.state.rot.available_rots &&
                 ec->e.state.rot.count)
               {
                  Eina_Bool found = EINA_FALSE;
                  for (i = 0; i < ec->e.state.rot.count; i++)
                    {
                       if (ec->e.state.rot.available_rots[i] == ang)
                         {
                            found = EINA_TRUE;
                         }
                    }
                  if (found) goto success;
               }
          }
        else
          {
             goto success;
          }
     }
   else if (ec->e.state.rot.preferred_rot == ang) goto success;

fail:
   return EINA_FALSE;
success:
   return EINA_TRUE;
}

/**
 * @describe
 *  Get the list of available rotation of e_client.
 * @param      ec             e_client
 * @return     Eina_List*     the list consist of integer value of available rotation angle.
 * @caution                   the caller is responsible for freeing the list after use.
 */
static Eina_List*
e_client_rotation_available_list_get(const E_Client *ec)
{
   Eina_List *list = NULL;
   int *element;
   unsigned int i;

   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   if ((!ec->e.state.rot.support) && (!ec->e.state.rot.app_set)) return list;

   if (ec->e.state.rot.preferred_rot != -1)
     {
        element = (int *)malloc(1 * sizeof(int));
        *element = ec->e.state.rot.preferred_rot;
        list = eina_list_append(list, element);
     }
   else if (ec->e.state.rot.count > 0)
     {
        if (ec->e.state.rot.available_rots)
          {
             for (i = 0; i < ec->e.state.rot.count; i++)
               {
                  element = (int *)malloc(1 * sizeof(int));
                  *element = ec->e.state.rot.available_rots[i];
                  list = eina_list_append(list, element);
               }
          }
     }
   return list;
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
 *  Get previous angle get
 * @param      ec             e_client
 * @return     int            previous angle.
 */
static int
e_client_rotation_prev_angle_get(const E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, -1);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, -1);

   return ec->e.state.rot.ang.prev;
}

/**
 * @describe
 *  Get the angle that this e_client should be rotated.
 * @param      ec             e_client
 * @return     Eina_Bool      -1: There is no need to be rotated.
 *                            != -1: The angle that this e_client should be rotated.
 */
static int
e_client_rotation_recommend_angle_get(const E_Client *ec)
{
   E_Zone *zone = NULL;
   int ret_ang = -1;

   E_OBJECT_CHECK_RETURN(ec, ret_ang);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, ret_ang);

   zone = ec->zone;

   if (ec->e.state.rot.type == E_CLIENT_ROTATION_TYPE_DEPENDENT) goto end;
   if ((!ec->e.state.rot.app_set) && (!ec->e.state.rot.support)) goto end;

   if (ec->e.state.rot.preferred_rot != -1)
     {
        ret_ang = ec->e.state.rot.preferred_rot;
     }
   else if ((ec->e.state.rot.available_rots) &&
            (ec->e.state.rot.count))
     {
        unsigned int i;
        int current_ang = _e_client_rotation_curr_next_get(ec);
        Eina_Bool found = EINA_FALSE;
        Eina_Bool found_curr_ang = EINA_FALSE;

        if (_e_client_rotation_is_dependent_parent(ec))
          {
             ret_ang = _e_client_rotation_curr_next_get(ec->parent);
          }
        else
          {
             ret_ang = e_zone_rotation_get(zone);
          }

        for (i = 0; i < ec->e.state.rot.count; i++)
          {
             if (ec->e.state.rot.available_rots[i] == ret_ang)
               {
                  found = EINA_TRUE;
                  break;
               }
             if (ec->e.state.rot.available_rots[i] == current_ang)
               found_curr_ang = EINA_TRUE;
          }

        if (!found)
          {
             if (found_curr_ang)
               ret_ang = current_ang;
             else
               ret_ang = ec->e.state.rot.available_rots[0];
          }
     }
   else
     ;

end:
   return ret_ang;
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
                         evas_object_show(ec->frame); // e_client_show(ec);
                    }
#if 0 //force rendering?
                  if (ec->e.state.deiconify_approve.pending)
                    _e_client_deiconify_approve_send_pending_end(ec);
#endif

                  return EINA_FALSE;
               }
             else
               ;
          }
        else
          return EINA_FALSE;
     }

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
        // the window which type is "ECORE_X_WINDOW_TYPE_NORMAL" will be rotated itself.
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

   /* if there is vkbd window, send message to prepare rotation */
   if (_e_client_is_vkbd(ec))
     {
        if (rot.prepare_timer)
          ecore_timer_del(rot.prepare_timer);
        rot.prepare_timer = NULL;

        ecore_x_e_window_rotation_change_prepare_send(rot.vkbd_ctrl_win,
                                                      rotation,
                                                      EINA_FALSE, 1, 1);
        rot.prepare_timer = ecore_timer_add(4.0f,
                                            _e_client_rotation_change_prepare_timeout,
                                            NULL);

        rot.wait_prepare_done = EINA_TRUE;
     }

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
   return EINA_TRUE;
}

/* e_zone_rotation related functions */
static void
e_zone_rotation_set(E_Zone *zone,
                    int     rot)
{
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   if (rot == -1)
     {
        zone->rot.unknown_state = EINA_TRUE;
        return;
     }
   else
     zone->rot.unknown_state = EINA_FALSE;

   _e_zone_rotation_set_internal(zone, rot);
}

static void
e_zone_rotation_sub_set(E_Zone *zone, int rotation)
{
   E_OBJECT_CHECK(zone);
   E_OBJECT_TYPE_CHECK(zone, E_ZONE_TYPE);

   zone->rot.sub = rotation;

   if ((zone->rot.unknown_state) &&
       (zone->rot.curr != rotation))
     _e_zone_rotation_set_internal(zone, rotation);
}

static int
e_zone_rotation_get(E_Zone *zone)
{
   E_OBJECT_CHECK_RETURN(zone, -1);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, -1);
   if (!zone->rot.unknown_state) return zone->rot.curr;
   else return zone->rot.sub;
}

static Eina_Bool
e_zone_rotation_block_set(E_Zone *zone, const char *name_hint, Eina_Bool set)
{
   E_Event_Zone_Rotation_Change_Begin *ev;

   E_OBJECT_CHECK_RETURN(zone, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(zone, E_ZONE_TYPE, EINA_FALSE);

   if (set) zone->rot.block_count++;
   else     zone->rot.block_count--;

   if (zone->rot.block_count <= 0)
     {
        zone->rot.block_count = 0;

        if (zone->rot.pending)
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
       (zone->rot.block_count == 0))
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

/* e_client rotation internal callbacks */
static Eina_Bool
_rot_hook_client_free_intern(E_Client *ec)
{
   Eina_Bool rm_vkbd_parent = EINA_FALSE;
   int unref_count = 0;

   ec->e.fetch.rot.app_set = 0;
   ec->e.state.rot.preferred_rot = -1;

   if (ec->e.state.rot.available_rots)
     E_FREE(ec->e.state.rot.available_rots);

   _e_client_rotation_list_remove(ec);
   if ((rot.vkbd) && (rot.vkbd == ec))
     {
        rot.vkbd = NULL;
        if (rot.vkbd_ctrl_win)
          {
             ecore_x_e_virtual_keyboard_state_set
                (rot.vkbd_ctrl_win, ECORE_X_VIRTUAL_KEYBOARD_STATE_OFF);
          }

        rot.vkbd_hide_prepare_done = EINA_FALSE;
        if (rot.vkbd_hide_prepare_timer)
          {
             ecore_timer_del(rot.vkbd_hide_prepare_timer);
             unref_count++;
          }
        rot.vkbd_hide_prepare_timer = NULL;
        if (rot.vkbd_hide_timer)
          {
             ecore_timer_del(rot.vkbd_hide_timer);
             unref_count++;
          }
        rot.vkbd_hide_timer = NULL;

        rot.vkbd_show_prepare_done = EINA_FALSE;
        if (rot.vkbd_show_prepare_timer)
           ecore_timer_del(rot.vkbd_show_prepare_timer);
        rot.vkbd_show_prepare_timer = NULL;
        if (rot.vkbd_show_timer)
           ecore_timer_del(rot.vkbd_show_timer);
        rot.vkbd_show_timer = NULL;
        if ((rot.vkbd_parent) && (!rot.vkbd_prediction))
           rm_vkbd_parent = EINA_TRUE;
     }
   else if ((rot.vkbd_prediction) &&
            (rot.vkbd_prediction == ec))
     {
        rot.vkbd_prediction = NULL;
        if ((rot.vkbd_parent) && (!rot.vkbd))
           rm_vkbd_parent = EINA_TRUE;
     }
   else if ((rot.vkbd_parent) &&
            (rot.vkbd_parent == ec))
      rm_vkbd_parent = EINA_TRUE;

   if (rm_vkbd_parent)
     {
        rot.vkbd_parent = NULL;
     }

   while (unref_count)
     {
        e_object_unref(E_OBJECT(ec));
        unref_count--;
     }

   return EINA_TRUE;
}

static Eina_Bool
_rot_hook_client_del_intern(E_Client *ec)
{
   _e_client_rotation_list_remove(ec);
   if (rot.async_list) rot.async_list = eina_list_remove(rot.async_list, ec);
   return EINA_TRUE;
}

static void
_rot_cb_evas_show_intern(E_Client *ec)
{
   if (!ec->hidden)
     {
        if (rot.vkbd == ec)
          _e_client_vkbd_show_timeout(ec);
     }

   if (!ec->iconic)
     {
        if ((ec->e.state.rot.support) || (ec->e.state.rot.app_set))
          {
             if (ec->e.state.rot.ang.next == -1)
               {
                  int rotation = e_client_rotation_recommend_angle_get(ec);
                  if (rotation != -1) e_client_rotation_set(ec, rotation);
               }
          }
     }
}

static Eina_Bool
_rot_hook_eval_end_intern(E_Client *ec)
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
             if (!rot.wait_prepare_done)
               _e_client_rotation_change_message_send(ec);
          }
        rot.fetch = EINA_TRUE;
        ec->changes.rotation = 0;
     }
   return EINA_TRUE;
}

static Eina_Bool
_rot_cb_idle_enterer_intern(void)
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
        Ecore_X_Event_Client_Message *msg = NULL;
        Ecore_X_Atom t = 0;
        EINA_LIST_FREE(rot.msgs, msg)
          {
             t = msg->message_type;
             if (t == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_ON_PREPARE_DONE)
               {
                  if ((rot.vkbd_ctrl_win) &&
                      ((Ecore_X_Window)msg->data.l[0] == rot.vkbd_ctrl_win) &&
                      (rot.vkbd))
                    {
                       if (rot.vkbd_show_prepare_timer)
                         _e_client_vkbd_show(rot.vkbd);
                       else
                         ;
                    }
               }
             else if (t == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_OFF_PREPARE_DONE)
               {
                  if ((rot.vkbd_ctrl_win) &&
                      ((Ecore_X_Window)msg->data.l[0] == rot.vkbd_ctrl_win) &&
                      (rot.vkbd))
                    {
                       if (rot.vkbd_hide_prepare_timer)
                         {
                            _e_client_vkbd_hide(rot.vkbd, EINA_TRUE);
                         }
                       else
                         ;
                    }
               }
             else if (t == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_CONTROL_WINDOW)
               {
                  rot.vkbd_ctrl_win = msg->data.l[0];
               }
             else if (t == ECORE_X_ATOM_E_WINDOW_ROTATION_CHANGE_PREPARE_DONE)
               {
                  if ((rot.vkbd_ctrl_win) &&
                      (rot.vkbd_ctrl_win == (Ecore_X_Window)msg->data.l[0]))
                    {
                       E_Zone *zone = e_util_zone_current_get(e_manager_current_get());
                       if ((zone) && (rot.wait_prepare_done))
                         {
                            if (rot.list)
                              _e_client_rotation_list_send();
                            else
                              _e_client_rotation_change_done();

                            if (rot.prepare_timer)
                              ecore_timer_del(rot.prepare_timer);
                            rot.prepare_timer = NULL;
                            rot.wait_prepare_done = EINA_FALSE;
                         }
                    }
               }
             E_FREE(msg);
          }
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
                  //Do implement screen lock api on e_comp.
                  //do call comp_x's screen lock
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

        rot.msgs = NULL;
        rot.fetch = EINA_FALSE;
     }

   return EINA_TRUE;
}

static Eina_Bool
_rot_hook_new_client_intern(E_Client *ec)
{
   Ecore_X_Window win = e_client_util_win_get(ec);
   int at_num = 0, i;
   Ecore_X_Atom *atoms;

   ec->e.state.rot.preferred_rot = -1;
   ec->e.state.rot.type = E_CLIENT_ROTATION_TYPE_NORMAL;
   ec->e.state.rot.ang.next = -1;
   ec->e.state.rot.ang.reserve = -1;
   ec->e.state.rot.pending_show = 0;
   ec->e.state.rot.ang.curr = 0;
   ec->e.state.rot.ang.prev = 0;

   atoms = ecore_x_window_prop_list(win, &at_num);
   if (atoms)
     {
        for (i = 0; i < at_num; i++)
          {
            /* loop to check for wm rotation */
             if (atoms[i] == ECORE_X_ATOM_E_WINDOW_ROTATION_SUPPORTED)
               {
                    ec->e.fetch.rot.support = 1;
               }
             else if ((atoms[i] == ECORE_X_ATOM_E_WINDOW_ROTATION_0_GEOMETRY) ||
                      (atoms[i] == ECORE_X_ATOM_E_WINDOW_ROTATION_90_GEOMETRY) ||
                      (atoms[i] == ECORE_X_ATOM_E_WINDOW_ROTATION_180_GEOMETRY) ||
                      (atoms[i] == ECORE_X_ATOM_E_WINDOW_ROTATION_270_GEOMETRY))
               {
                    ec->e.fetch.rot.geom_hint = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_E_WINDOW_ROTATION_APP_SUPPORTED)
               {
                    ec->e.fetch.rot.app_set = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_E_WINDOW_ROTATION_PREFERRED_ROTATION)
               {
                    ec->e.fetch.rot.preferred_rot = 1;
               }
             else if (atoms[i] == ECORE_X_ATOM_E_WINDOW_ROTATION_AVAILABLE_LIST)
               {
                    ec->e.fetch.rot.available_rots = 1;
               }
          }
        free(atoms);
     }

   return EINA_TRUE;
}

static Eina_Bool
_rot_cb_zone_rotation_change_begin_intern(E_Event_Zone_Rotation_Change_Begin *ev)
{
   if ((!ev) || (!ev->zone)) return EINA_FALSE;

   if (!_e_client_rotation_zone_set(ev->zone))
     {
        /* The WM will decide to cancel zone rotation at idle time.
         * Because, the policy module can make list of rotation windows
         */
        rot.cancel.state = EINA_TRUE;
        rot.cancel.zone = ev->zone;
     }
   return EINA_TRUE;
}

static Eina_Bool
_rot_intercept_hook_hide_intern(E_Client *ec)
{
   if ((rot.vkbd_ctrl_win) && (rot.vkbd) &&
       (ec == rot.vkbd) &&
       (!rot.vkbd_hide_prepare_done) &&
       (!ec->iconic))
     {
        Eina_Bool need_prepare = EINA_TRUE;
        if (ec->parent)
          {
             if (e_object_is_del(E_OBJECT(ec->parent)))
               need_prepare = EINA_FALSE;
          }
        else
          need_prepare = EINA_FALSE;

        /* If E wait for VKBD_ON_PREPARE_DONE,
         * E doesn't need to wait for that no more,
         * VKBD will be destroyed soon. */
        if (rot.vkbd_show_prepare_timer)
          {
             ecore_timer_del(rot.vkbd_show_prepare_timer);
             rot.vkbd_show_prepare_timer = NULL;
          }

        if (need_prepare)
          {
             e_object_ref(E_OBJECT(ec));

             ecore_x_e_virtual_keyboard_off_prepare_request_send(rot.vkbd_ctrl_win);

             if (rot.vkbd_hide_prepare_timer)
               {
                  ecore_timer_del(rot.vkbd_hide_prepare_timer);
                  e_object_unref(E_OBJECT(rot.vkbd));
               }
             rot.vkbd_hide_prepare_timer = NULL;

             if (rot.vkbd_hide_timer)
               {
                  ecore_timer_del(rot.vkbd_hide_timer);
                  e_object_unref(E_OBJECT(rot.vkbd));
               }
             rot.vkbd_hide_timer = NULL;

             rot.vkbd_hide_prepare_timer = ecore_timer_add(1.5f,
                                                           _e_client_vkbd_hide_prepare_timeout,
                                                           ec);
             return EINA_FALSE;
          }
        else
          {
             e_object_ref(E_OBJECT(ec));

             /* In order to clear conformant area properly, WM should send keyboard off prepare request event */
             ecore_x_e_virtual_keyboard_off_prepare_request_send(rot.vkbd_ctrl_win);

             /* cleanup code from _e_client_vkbd_hide() */
             rot.vkbd_hide_prepare_done = EINA_TRUE;

             if (rot.vkbd_hide_prepare_timer)
               {
                  ecore_timer_del(rot.vkbd_hide_prepare_timer);
                  e_object_unref(E_OBJECT(rot.vkbd));
               }
             rot.vkbd_hide_prepare_timer = NULL;

             if (rot.vkbd_hide_timer)
               {
                  ecore_timer_del(rot.vkbd_hide_timer);
                  e_object_unref(E_OBJECT(rot.vkbd));
               }
             rot.vkbd_hide_timer = NULL;

             rot.vkbd_hide_timer = ecore_timer_add(0.03f, _e_client_vkbd_hide_timeout, ec);
          }
     }
   /* if vkbd's parent has to be hidden,
    * vkbd and prediction widnow will be hidden with its parent.
    */
   else if (ec == rot.vkbd_parent)
     {
        if ((rot.vkbd) && evas_object_visible_get(rot.vkbd->frame))
          {
             _e_client_vkbd_hide(rot.vkbd, EINA_FALSE);
             ecore_x_window_hide(e_client_util_win_get(rot.vkbd));
          }
        if ((rot.vkbd_prediction) && evas_object_visible_get(rot.vkbd_prediction->frame))
          {
             evas_object_hide(rot.vkbd_prediction->frame); // e_client_hide(rot.vkbd_prediction);
             ecore_x_window_hide(e_client_util_win_get(rot.vkbd_prediction));// is it really needed?
          }
     }
   // clear pending_show, because this window is hidden now.
   ec->e.state.rot.pending_show = 0;

   return EINA_TRUE;
}

static Eina_Bool
_rot_intercept_hook_show_helper_intern(E_Client *ec)
{
   // newly created window that has to be rotated will be shown after rotation done.
   // so, skip at this time. it will be called again after GETTING ROT_DONE.
   if (ec->e.state.rot.ang.next != -1)
     {
        ec->e.state.rot.pending_show = 1;
        return EINA_FALSE;
     }

   if ((rot.vkbd_ctrl_win) && (rot.vkbd) &&
       (ec == rot.vkbd) &&
       (!rot.vkbd_show_prepare_done))
     {
        ecore_x_e_virtual_keyboard_on_prepare_request_send(rot.vkbd_ctrl_win);

        if (rot.vkbd_show_prepare_timer)
          ecore_timer_del(rot.vkbd_show_prepare_timer);
        rot.vkbd_show_prepare_timer = ecore_timer_add(1.5f,
                                                      _e_client_vkbd_show_prepare_timeout,
                                                      ec);
        return EINA_FALSE;
     }

    return EINA_TRUE;
}

static Eina_Bool
_rot_cb_window_configure_intern(Ecore_X_Event_Window_Configure *ev)
{
   E_Client *ec;

   ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->win);
   if (!ec) return EINA_FALSE;

   if ((ec->e.state.rot.pending_change_request) &&
       (ec->e.state.rot.geom_hint))
     {
        if ((ev->w == ec->client.w) && (ev->h == ec->client.h))
          {
             ec->e.state.rot.pending_change_request = 0;
             if (!ec->e.state.rot.wait_for_done)
               e_client_rotation_change_request(ec, ec->e.state.rot.ang.next);
          }
     }

   return EINA_TRUE;
}

static Eina_Bool
_rot_cb_window_property_intern(Ecore_X_Event_Window_Property *ev)
{
   E_Client *ec;

   ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->win);

   if (!ec) return ECORE_CALLBACK_RENEW;

   if (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_SUPPORTED)
     {
        ec->e.fetch.rot.support = 1;
        EC_CHANGED(ec);
     }
   else if ((ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_0_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_90_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_180_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_270_GEOMETRY))
     {
        ec->e.fetch.rot.geom_hint = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_APP_SUPPORTED)
     {
         ec->e.fetch.rot.app_set = 1;
        EC_CHANGED(ec);
     }
   else if (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_PREFERRED_ROTATION)
     {
        ec->e.fetch.rot.preferred_rot = 1;
        EC_CHANGED(ec);     }
   else if (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_AVAILABLE_LIST)
     {
        ec->e.fetch.rot.available_rots = 1;
        EC_CHANGED(ec);
     }
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_rot_cb_window_message_intern(Ecore_X_Event_Client_Message *ev)
{
   E_Client *ec;

   ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->win);

   if (!ec)
     {
        Ecore_X_Event_Client_Message *msg = NULL;
        Ecore_X_Atom t = ev->message_type;
        if ((t == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_ON_PREPARE_DONE)  ||
            (t == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_OFF_PREPARE_DONE) ||
            (t == ECORE_X_ATOM_E_VIRTUAL_KEYBOARD_CONTROL_WINDOW)   ||
            (t == ECORE_X_ATOM_E_WINDOW_ROTATION_CHANGE_PREPARE_DONE))
          {
             msg = E_NEW(Ecore_X_Event_Client_Message, 1);
             if (!msg) return ECORE_CALLBACK_PASS_ON;

             msg->win = ev->win;
             msg->message_type = ev->message_type;
             msg->data.l[0] = ev->data.l[0];
             msg->data.l[1] = ev->data.l[1];
             msg->data.l[2] = ev->data.l[2];
             msg->data.l[3] = ev->data.l[3];
             msg->data.l[4] = ev->data.l[4];

             rot.msgs = eina_list_append(rot.msgs, msg);
             rot.fetch = EINA_TRUE;
          }
        return ECORE_CALLBACK_PASS_ON;
     }

   if (ev->message_type == ECORE_X_ATOM_E_WINDOW_ROTATION_CHANGE_DONE)
     {
        ec->e.state.rot.ang.prev = ec->e.state.rot.ang.curr;
        ec->e.state.rot.ang.curr = (int)ev->data.l[1];

        if ((int)ev->data.l[1] == ec->e.state.rot.ang.next)
          {
             ec->e.state.rot.ang.next = -1;

             _e_client_rotation_list_remove(ec);

             if (ec->e.state.rot.ang.reserve != -1)
               {
                  e_client_rotation_set(ec, ec->e.state.rot.ang.reserve);
                  ec->e.state.rot.ang.reserve = -1;
               }
             else if (ec->e.state.rot.pending_show)
               {
                  ec->e.state.rot.pending_show = 0;
                  evas_object_show(ec->frame); //e_client_show(ec);
               }
          }
        else
          {
             int angle = e_client_rotation_recommend_angle_get(ec);
             if (angle != -1) e_client_rotation_set(ec, angle);
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_hook_eval_fetch_intern(E_Client *ec)
{
#if 0
   //TODO: add vkbd fetch_transient_for flag
   /* workaround: since transient_for is fetched by illume in hook,
    * added below flag to know this state in this eval time
    */
   Eina_Bool fetch_transient_for = EINA_FALSE;

   if (ec->icccm.fetch.transient_for)
     {
        if (((rot.vkbd) && (rot.vkbd == ec)) ||
            ((rot.vkbd_prediction) && (rot.vkbd_prediction == ec)))
          {
             ec->e.fetch.rot.need_rotation = EINA_TRUE;
          }

        fetch_transient_for = EINA_TRUE;
     }
#endif
   //TODO: Add fetch flag for VKBD
   if ((ec->icccm.name) && (ec->icccm.class))
     {
        if ((!strcmp(ec->icccm.name, "Virtual Keyboard")) &&
            (!strcmp(ec->icccm.class, "ISF")) &&
            (ec->vkbd.win_type != E_VIRTUAL_KEYBOARD_WINDOW_TYPE_KEYPAD))
          {
             ec->vkbd.win_type = E_VIRTUAL_KEYBOARD_WINDOW_TYPE_KEYPAD;
             rot.vkbd = ec;
          }
        else if ((!strcmp(ec->icccm.name, "Prediction Window")) &&
                 (!strcmp(ec->icccm.class, "ISF")) &&
                 (ec->vkbd.win_type != E_VIRTUAL_KEYBOARD_WINDOW_TYPE_PREDICTION))
          {
             ec->vkbd.win_type = E_VIRTUAL_KEYBOARD_WINDOW_TYPE_PREDICTION;
             rot.vkbd_prediction = ec;
          }
        else if ((!strcmp(ec->icccm.name, "Key Magnifier")) &&
                 (!strcmp(ec->icccm.class, "ISF")) &&
                 (ec->vkbd.win_type != E_VIRTUAL_KEYBOARD_WINDOW_TYPE_MAGNIFIER))
          {
             ec->vkbd.win_type = E_VIRTUAL_KEYBOARD_WINDOW_TYPE_MAGNIFIER;
          }
        else if ((!strcmp(ec->icccm.name, "ISF Popup")) &&
                 (!strcmp(ec->icccm.class, "ISF")) &&
                 (ec->vkbd.win_type != E_VIRTUAL_KEYBOARD_WINDOW_TYPE_POPUP))
          {
             ec->vkbd.win_type = E_VIRTUAL_KEYBOARD_WINDOW_TYPE_POPUP;
          }
     }

   if(ec->e.fetch.rot.support)
     {
        int ret = 0;
        unsigned int support = 0;

        ret = ecore_x_window_prop_card32_get
          (e_client_util_win_get(ec),
          ECORE_X_ATOM_E_WINDOW_ROTATION_SUPPORTED,
          &support, 1);

        ec->e.state.rot.support = 0;
        if ((ret == 1) && (support == 1))
          {
             int ang = -1;

             ang = _prev_angle_get(e_client_util_win_get(ec));
             if (ang != -1)
               {
                  ec->e.state.rot.ang.curr = ang;
               }
             ec->e.state.rot.support = 1;
          }

        if (ec->e.state.rot.support)
          ec->e.fetch.rot.need_rotation = EINA_TRUE;

        ec->e.fetch.rot.support = 0;
     }
   if (ec->e.fetch.rot.geom_hint)
     {
        Eina_Rectangle r[4];
        int i, x, y, w, h;
        ec->e.state.rot.geom_hint = 0;
        for (i = 0; i < 4; i++)
          {
             r[i].x = ec->e.state.rot.geom[i].x;
             r[i].y = ec->e.state.rot.geom[i].y;
             r[i].w = ec->e.state.rot.geom[i].w;
             r[i].h = ec->e.state.rot.geom[i].h;

             ec->e.state.rot.geom[i].x = 0;
             ec->e.state.rot.geom[i].y = 0;
             ec->e.state.rot.geom[i].w = 0;
             ec->e.state.rot.geom[i].h = 0;
          }

        for (i = 0; i < 4; i++)
          {
             x = 0; y = 0; w = 0; h = 0;
             if (ecore_x_e_window_rotation_geometry_get(e_client_util_win_get(ec), i*90, &x, &y, &w, &h))
               {
                  ec->e.state.rot.geom_hint = 1;
                  ec->e.state.rot.geom[i].x = x;
                  ec->e.state.rot.geom[i].y = y;
                  ec->e.state.rot.geom[i].w = w;
                  ec->e.state.rot.geom[i].h = h;

                  if (!((r[i].x == x) && (r[i].y == y) &&
                        (r[i].w == w) && (r[i].h == h)))
                    {
                       ec->e.fetch.rot.need_rotation = EINA_TRUE;
                    }
               }
          }
        ec->e.fetch.rot.geom_hint = 0;
     }
   if (ec->e.fetch.rot.app_set)
     {
        unsigned char _prev_app_set = ec->e.state.rot.app_set;
        ec->e.state.rot.app_set = ecore_x_e_window_rotation_app_get(e_client_util_win_get(ec));

        if (_prev_app_set != ec->e.state.rot.app_set)
          {
             if (ec->e.state.rot.app_set)
               {
                  int ang = -1;

                  ang = _prev_angle_get(e_client_util_win_get(ec));

                  if (ang != -1)
                    {
                       ec->e.state.rot.ang.curr = ang;
                    }
               }
             ec->e.fetch.rot.need_rotation = EINA_TRUE;
          }

        ec->e.fetch.rot.app_set = 0;
     }
   if (ec->e.fetch.rot.preferred_rot)
     {
        int r = 0, _prev_preferred_rot;
        _prev_preferred_rot = ec->e.state.rot.preferred_rot;
        ec->e.state.rot.preferred_rot = -1;
        if (ecore_x_e_window_rotation_preferred_rotation_get(e_client_util_win_get(ec), &r))
          {
             ec->e.state.rot.preferred_rot = r;
          }
        else
          {
             ;
          }

        if (_prev_preferred_rot != ec->e.state.rot.preferred_rot)
          ec->e.fetch.rot.need_rotation = EINA_TRUE;

        ec->e.fetch.rot.preferred_rot = 0;
     }
   if (ec->e.fetch.rot.available_rots)
     {
        Eina_Bool res, diff = EINA_FALSE;
        int *rots = NULL;
        unsigned int count = 0, i = 0;
        int _prev_rots[4] = { -1, };

        if (ec->e.state.rot.available_rots)
          {
             memcpy(_prev_rots,
                    ec->e.state.rot.available_rots,
                    (sizeof(int) * ec->e.state.rot.count));

             E_FREE(ec->e.state.rot.available_rots);
          }

        ec->e.state.rot.count = 0;

        res = ecore_x_e_window_rotation_available_rotations_get(e_client_util_win_get(ec),
                                                                &rots, &count);
        if ((res) && (count > 0) && (rots))
          {
             ec->e.state.rot.available_rots = rots;
             ec->e.state.rot.count = count;

             for (i = 0; i < count; i++)
               {
                  if ((!diff) && (_prev_rots[i] != rots[i]))
                    {
                       diff = EINA_TRUE;
                    }
               }
          }
        else
          {
             diff = EINA_TRUE;
          }

        if (diff) ec->e.fetch.rot.need_rotation = EINA_TRUE;

        ec->e.fetch.rot.available_rots = 0;
     }
#if 0
   if (fetch_transient_for)
     {
        Eina_Bool need_fetch = EINA_FALSE;

        if (rot.vkbd)
          {
             if (rot.vkbd == bd) need_fetch = EINA_TRUE;
          }
        else if ((rot.vkbd_prediction) && (rot.vkbd_prediction == bd))
          need_fetch = EINA_TRUE;

        if (need_fetch)
          {
             if (bd->parent != rot.vkbd_parent)
               {
                  ;
               }
          }
     }
#endif
   if ((ec->e.fetch.rot.need_rotation) &&
       (ec->e.state.rot.type == E_CLIENT_ROTATION_TYPE_NORMAL))
     {
        Eina_Bool hint = EINA_FALSE;
        int ang = 0;
        int x, y, w, h;
        Eina_Bool move;

        ang = e_client_rotation_recommend_angle_get(ec);
        e_client_rotation_set(ec, ang);

        if (ec->e.state.rot.ang.next == -1)
          {
             ang = ec->e.state.rot.ang.curr;
             //Fix geometry change case.
             hint = _e_client_rotation_geom_get(ec, ec->zone, ang, &x, &y, &w, &h, &move);
             if (hint)
               {
#if 0
                  /// need to change api to _e_comp_x_client_move_resize_send();
                  _e_client_move_resize_internal(bd, x, y, w, h, EINA_TRUE, move);//?
#endif
                  //e_client_util_move_resize_without_frame(ec, x, y, w, h); //check: this function work correct or not.
                  evas_object_move(ec->frame, x, y);
                  e_client_util_resize_without_frame(ec, w, h); 
               }
          }
        else
          {
             EC_CHANGED(ec);
          }
     }
   if (ec->e.fetch.rot.need_rotation)
     ec->e.fetch.rot.need_rotation = EINA_FALSE;

   return EINA_TRUE;
}

static void
_rot_hook_new_client(void *d EINA_UNUSED, E_Client *ec)
{
   _rot_hook_new_client_intern(ec);
}

static void
_rot_hook_client_del(void *d EINA_UNUSED, E_Client *ec)
{
   _rot_hook_client_del_intern(ec);
   _rot_hook_client_free_intern(ec);
}

static void
_rot_hook_eval_end(void *d EINA_UNUSED, E_Client *ec)
{
   _rot_hook_eval_end_intern(ec);
}

static void
_rot_hook_eval_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   _rot_hook_eval_fetch_intern(ec);
}

static Eina_Bool
_rot_intercept_hook_show_helper(void *d EINA_UNUSED, E_Client *ec)
{
   return _rot_intercept_hook_show_helper_intern(ec);
}

static Eina_Bool
_rot_intercept_hook_hide(void *d EINA_UNUSED, E_Client *ec)
{
   return _rot_intercept_hook_hide_intern(ec);
}

static Eina_Bool
_rot_cb_window_property(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Property *ev)
{
   if ((ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_SUPPORTED) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_0_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_90_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_180_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_270_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_APP_SUPPORTED) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_PREFERRED_ROTATION) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_AVAILABLE_LIST))
     {

        _rot_cb_window_property_intern(ev);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_rot_cb_window_configure(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Configure *ev)
{
   _rot_cb_window_configure_intern(ev);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_rot_cb_window_message(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Client_Message *ev)
{
   _rot_cb_window_message_intern(ev);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_rot_cb_zone_rotation_change_begin(void *data EINA_UNUSED, int ev_type EINA_UNUSED, E_Event_Zone_Rotation_Change_Begin *ev)
{
   if ((!ev) || (!ev->zone)) return ECORE_CALLBACK_PASS_ON;

   _rot_cb_zone_rotation_change_begin_intern(ev);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_rot_cb_idle_enterer(void *data EINA_UNUSED)
{
   _rot_cb_idle_enterer_intern();

   return ECORE_CALLBACK_RENEW;
}

static void
_rot_cb_evas_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   _rot_cb_evas_show_intern(ec);
   return;
}

static void
_rot_hook_new_client_post(void *d EINA_UNUSED, E_Client *ec)
{
   if (!ec->frame)
      return;

   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW, _rot_cb_evas_show, ec);
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

EINTERN void
e_mod_pol_rotation_init(void)
{
   E_LIST_HANDLER_APPEND(rot_handlers, ECORE_X_EVENT_WINDOW_PROPERTY,
                         _rot_cb_window_property, NULL);
   E_LIST_HANDLER_APPEND(rot_handlers, ECORE_X_EVENT_WINDOW_CONFIGURE,
                         _rot_cb_window_configure, NULL);
   E_LIST_HANDLER_APPEND(rot_handlers, ECORE_X_EVENT_CLIENT_MESSAGE,
                         _rot_cb_window_message, NULL);
   E_LIST_HANDLER_APPEND(rot_handlers, E_EVENT_ZONE_ROTATION_CHANGE_BEGIN,
                         _rot_cb_zone_rotation_change_begin, NULL);

   E_CLIENT_HOOK_APPEND(rot_hooks, E_CLIENT_HOOK_NEW_CLIENT,
                        _rot_hook_new_client, NULL);
   E_CLIENT_HOOK_APPEND(rot_hooks, E_CLIENT_HOOK_NEW_CLIENT_POST,
                        _rot_hook_new_client_post, NULL);
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
}

EINTERN void
e_mod_pol_rotation_shutdown(void)
{
   E_FREE_LIST(rot_hooks, e_client_hook_del);
   E_FREE_LIST(rot_handlers, ecore_event_handler_del);
   E_FREE_LIST(rot_intercept_hooks, e_comp_object_intercept_hook_del);

   if (rot_idle_enterer)
     {
         ecore_idle_enterer_del(rot_idle_enterer);
         rot_idle_enterer = NULL;
     }
}
