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
#include <vconf.h>
#endif

static Eina_List *_event_handlers = NULL;
static Eina_Bool rot_lock = EINA_FALSE;

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
_e_mod_pol_rotation_cb_info_rotation_message(void *data EINA_UNUSED, int ev_type EINA_UNUSED, E_Event_Info_Rotation_Message *ev)
{
   Eina_Bool block;
   const char *name_hint = "enlightenment_info";

   if (EINA_UNLIKELY((ev == NULL) || (ev->zone == NULL)))
     goto end;

   switch (ev->message)
     {
      case E_INFO_ROTATION_MESSAGE_SET:
         e_zone_rotation_set(ev->zone, ev->rotation);
         break;
      case E_INFO_ROTATION_MESSAGE_ENABLE:
      case E_INFO_ROTATION_MESSAGE_DISABLE:
         block = (ev->message == E_INFO_ROTATION_MESSAGE_DISABLE);
         e_zone_rotation_block_set(ev->zone, name_hint, block);
         break;
      default:
         ERR("Unknown message");
     }

end:
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_e_mod_pol_rotation_cb_idle_exiter(void *data EINA_UNUSED)
{
   if (E_EVENT_INFO_ROTATION_MESSAGE == -1)
     return ECORE_CALLBACK_RENEW;

   E_LIST_HANDLER_APPEND(_event_handlers, E_EVENT_INFO_ROTATION_MESSAGE,
                         _e_mod_pol_rotation_cb_info_rotation_message, NULL);

   return ECORE_CALLBACK_DONE;
}

#ifdef HAVE_AUTO_ROTATION
static void
_e_mod_pol_rotation_lock(int rot)
{
   E_Zone *zone;
   Eina_List *l;
   const char *name_hint = "vconf";
   Eina_Bool lock = !rot;

   if (lock == rot_lock)  return;

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        if (lock) e_zone_rotation_set(zone, 0);
        // TODO: expand/seperate auto_rotate policy (by settings or sensor)
        else e_zone_rotation_set(zone, e_mod_sensord_cur_angle_get());

        e_zone_rotation_block_set(zone, name_hint, lock);
     }
   rot_lock = lock;
}

static void
_e_mod_pol_rotation_cb_autorotate(keynode_t *node, void *data)
{

   int rot;

   rot = vconf_keynode_get_bool(node);
   if (rot < 0) return;

   _e_mod_pol_rotation_lock(rot);
}
#endif

EINTERN void
e_mod_pol_rotation_init(void)
{
#ifdef HAVE_AUTO_ROTATION
   int rot = 0;
   if (vconf_get_bool(VCONFKEY_SETAPPL_AUTO_ROTATE_SCREEN_BOOL, &rot) == VCONF_OK)
     {
        vconf_notify_key_changed(VCONFKEY_SETAPPL_AUTO_ROTATE_SCREEN_BOOL,
                                 _e_mod_pol_rotation_cb_autorotate,
                                 NULL);
        // if vconf auto_rotate is false, rotation by sensor is blocked
        if (rot == 0) _e_mod_pol_rotation_lock(rot);
     }

   e_mod_sensord_init();
#endif
#ifdef HAVE_WAYLAND_ONLY
   e_mod_rot_wl_init();
#endif

   if (E_EVENT_INFO_ROTATION_MESSAGE != -1)
     {
        E_LIST_HANDLER_APPEND(_event_handlers, E_EVENT_INFO_ROTATION_MESSAGE,
                              _e_mod_pol_rotation_cb_info_rotation_message, NULL);
     }
   else
     {
        /* NOTE:
         * If E_EVENT_INFO_ROTATION_MESSAGE is not initialized yet,
         * we should add event handler for rotation message in idle exiter,
         * becuase I expect that e_info_server will be initialized on idler
         * by quick launching.
         */
        ecore_idle_exiter_add(_e_mod_pol_rotation_cb_idle_exiter, NULL);
     }
}

EINTERN void
e_mod_pol_rotation_shutdown(void)
{
#ifdef HAVE_AUTO_ROTATION
  e_mod_sensord_deinit();
#endif
#ifdef HAVE_WAYLAND_ONLY
  e_mod_rot_wl_shutdown();
#endif

  E_FREE_LIST(_event_handlers, ecore_event_handler_del);
}
