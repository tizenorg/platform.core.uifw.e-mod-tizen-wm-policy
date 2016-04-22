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
#include "e_mod_rotation_settings.h"
#include <vconf.h>

#ifdef HAVE_AUTO_ROTATION
#include "e_mod_sensord.h"
#endif

static Eina_Bool rot_lock = EINA_FALSE;

static void
_e_mod_rot_settings_lock(int rot)
{
   E_Zone *zone;
   Eina_List *l;
   const char *name_hint = "vconf";
   Eina_Bool lock = !rot;

   if (lock == rot_lock)  return;

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        if (lock) e_zone_rotation_set(zone, 0);
#ifdef HAVE_AUTO_ROTATION
        else e_zone_rotation_set(zone, e_mod_sensord_cur_angle_get());
#endif
        e_zone_rotation_block_set(zone, name_hint, lock);
     }
   rot_lock = lock;
}

static void
_e_mod_rot_settings_cb_autorotate(keynode_t *node, void *data)
{
   int rot;

   rot = vconf_keynode_get_bool(node);
   if (rot < 0) return;

   _e_mod_rot_settings_lock(rot);
}

/* externally accessible functions */
EINTERN void
e_mod_rot_settings_init(void)
{
   int rot = 0;
   if (vconf_get_bool(VCONFKEY_SETAPPL_AUTO_ROTATE_SCREEN_BOOL, &rot) == VCONF_OK)
     {
        vconf_notify_key_changed(VCONFKEY_SETAPPL_AUTO_ROTATE_SCREEN_BOOL,
                                 _e_mod_rot_settings_cb_autorotate,
                                 NULL);
        // if vconf auto_rotate is false, rotation is blocked
        if (rot == 0) _e_mod_rot_settings_lock(rot);
     }
}

EINTERN void
e_mod_rot_settings_shutdown(void)
{
   rot_lock = EINA_FALSE;
}
