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

#include "e_mod_notification.h"
#include "e_mod_atoms.h"
#include "e_mod_utils.h"

typedef struct _Pol_Notification
{
   E_Client *ec;
   int       level;
} Pol_Notification;

static Eina_Hash *hash_pol_notifications = NULL;

static Pol_Notification*
_pol_notification_get_info (E_Client *ec)
{
   Pol_Notification *pn;

   E_CHECK_RETURN(hash_pol_notifications != NULL, NULL);

   pn = eina_hash_find(hash_pol_notifications, &ec);
   if (!pn)
     {
        pn = E_NEW(Pol_Notification, 1);
        E_CHECK_RETURN(pn != NULL, NULL);

        pn->ec = ec;
        eina_hash_add(hash_pol_notifications, &ec, pn);
     }

   return pn;
}

static Eina_Bool
_pol_notification_is_notification (E_Client *ec)
{
   int i;

   /* make sure we have a border */
   if (!ec) return EINA_FALSE;

   /* check actual type */
   if (ec->netwm.type == E_WINDOW_TYPE_NOTIFICATION)
      return EINA_TRUE;

   for (i=0; i<ec->netwm.extra_types_num; i++)
      if (ec->netwm.extra_types[i] == E_WINDOW_TYPE_NOTIFICATION)
         return EINA_TRUE;

   return EINA_FALSE;
}

static int
_pol_notification_get_level(Ecore_X_Window win)
{
   int level = 0;
   int ret;
   int num;
   unsigned char* prop_data = NULL;

   ret = ecore_x_window_prop_property_get (win, E_MOD_POL_ATOM_NOTIFICATION_LEVEL,
                                           ECORE_X_ATOM_CARDINAL, 32, &prop_data,
                                           &num);
   if (ret && prop_data)
      memcpy (&level, prop_data, sizeof (int));

   if (prop_data) free (prop_data);

   return level;
}

static short
_pol_notification_level_to_layer(int level)
{
   switch (level)
     {
      case E_POL_NOTIFICATION_LEVEL_HIGH:
        return E_LAYER_NOTIFICATION_HIGH;
      case E_POL_NOTIFICATION_LEVEL_NORMAL:
        return E_LAYER_NOTIFICATION_NORMAL;
      case E_POL_NOTIFICATION_LEVEL_LOW:
        return E_LAYER_NOTIFICATION_LOW;
      default:
        return E_LAYER_NOTIFICATION_DEFAULT;
     }
}

void
e_mod_pol_notification_init(void)
{
   hash_pol_notifications = eina_hash_pointer_new(free);
}

void
e_mod_pol_notification_shutdown(void)
{
   E_FREE_FUNC(hash_pol_notifications, eina_hash_free);
}

void
e_mod_pol_notification_client_del(E_Client *ec)
{
   Pol_Notification *pn;

   E_CHECK(ec != NULL);
   E_CHECK(hash_pol_notifications != NULL);

   pn = eina_hash_find(hash_pol_notifications, &ec);
   if (!pn) return;

   eina_hash_del_by_key(hash_pol_notifications, &ec);
}

Eina_Bool
e_mod_pol_notification_level_update(E_Client *ec)
{
   Pol_Notification *pn;
   short layer;
   int level;
   uint64_t win;

   E_CHECK_RETURN(ec != NULL, EINA_FALSE);
   E_CHECK_RETURN(ec->frame != NULL, EINA_FALSE);
   E_CHECK_RETURN(ec->pixmap != NULL, EINA_FALSE);

   pn = _pol_notification_get_info(ec);
   E_CHECK_RETURN(pn != NULL, EINA_FALSE);

   win = e_pixmap_window_get(ec->pixmap);

   /* 1. Check if a window is notification or not */
   if (!_pol_notification_is_notification(ec))
     {
        printf ("%s(%d): Win(0x%07lld) is NOT notification window... IGNORE!\n",
                __func__, __LINE__, win);
        return EINA_FALSE;
     }

   /* 2. Get and Set level */
   level = _pol_notification_get_level(win);
   layer = _pol_notification_level_to_layer(level);

   if (level == pn->level &&
       layer == evas_object_layer_get(ec->frame))
      return EINA_TRUE;

   /* 3. Change Stack */
   pn->level = level;
   evas_object_layer_set (ec->frame, layer);

   printf ("%s(%d): notification level update done! Win(0x%07lld)\n",
           __func__, __LINE__, win);

   return EINA_TRUE;
}
