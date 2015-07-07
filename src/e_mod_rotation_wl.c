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
#include "e_mod_rotation_wl.h"

#include <wayland-server.h>
#include "tizen_policy_ext-server-protocol.h"

static Eina_Hash *hash_policy_ext_rotation = NULL;

typedef struct _Policy_Ext_Rototation
{
  E_Pixmap *ep;
  uint32_t available_angles, preferred_angles;
  enum tizen_rotation_angle cur_angle, prev_angle;
  Eina_List *rotation_list;
  Eina_Bool angle_change_done;
} Policy_Ext_Rototation;

/* local subsystem functions */
static void _e_tizen_rotation_send_angle_change(E_Client *ec, enum tizen_rotation_angle angle);

static Policy_Ext_Rototation*
_policy_ext_rotation_get(E_Pixmap *ep)
{
   Policy_Ext_Rototation *rot;

   EINA_SAFETY_ON_NULL_RETURN_VAL(hash_policy_ext_rotation, NULL);

   rot = eina_hash_find(hash_policy_ext_rotation, &ep);
   if (!rot)
     {
        rot = E_NEW(Policy_Ext_Rototation, 1);
        EINA_SAFETY_ON_NULL_RETURN_VAL(rot, NULL);

        rot->ep = ep;
        eina_hash_add(hash_policy_ext_rotation, &ep, rot);
     }

   return rot;
}

static void
_e_tizen_rotation_set_available_angles_cb(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t angles)
{
   // implementation;
   E_Pixmap *ep;
   Policy_Ext_Rototation *rot;

   ep = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   rot = _policy_ext_rotation_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   rot->available_angles = angles;
}

static void
_e_tizen_rotation_set_preferred_angles_cb(struct wl_client *client,
                                          struct wl_resource *resource,
                                          uint32_t angles)
{
   // implementation;
   E_Pixmap *ep;
   Policy_Ext_Rototation *rot;
   E_Client *ec;
   Eina_Bool rotation_change = EINA_FALSE;

   ep = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   rot = _policy_ext_rotation_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   rot->preferred_angles = angles;

   //FIXME: move processing preferred angle logic to e-mod-rotation
   ec = e_pixmap_client_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   switch ((angles & rot->available_angles))
     {
        case TIZEN_ROTATION_ANGLE_0:
          rotation_change = EINA_TRUE;
          break;
        case TIZEN_ROTATION_ANGLE_90:
          rotation_change = EINA_TRUE;
          break;
        case TIZEN_ROTATION_ANGLE_180:
          rotation_change = EINA_TRUE;
          break;
        case TIZEN_ROTATION_ANGLE_270:
          rotation_change = EINA_TRUE;
          break;
        default:
          break;
     }

   if (rotation_change) _e_tizen_rotation_send_angle_change(ec, angles);
}

static void
_e_tizen_rotation_ack_angle_change_cb(struct wl_client *client,
                                      struct wl_resource *resource,
                                      uint32_t serial)
{
   // implementation;
   E_Pixmap *ep;
   Policy_Ext_Rototation *rot;

   ep = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   rot = _policy_ext_rotation_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   // check angle change serial
   rot->angle_change_done = EINA_TRUE;
}

static const struct tizen_rotation_interface _e_tizen_rotation_interface =
{
   _e_tizen_rotation_set_available_angles_cb,
   _e_tizen_rotation_set_preferred_angles_cb,
   _e_tizen_rotation_ack_angle_change_cb,
   /* need rotation destroy request? */
};

static void
_e_tizen_rotation_destroy(struct wl_resource *resource)
{
   E_Pixmap *ep;
   Policy_Ext_Rototation *rot;

   ep = wl_resource_get_user_data(resource);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   rot = _policy_ext_rotation_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   rot->rotation_list = eina_list_remove(rot->rotation_list, resource);
}

static void
_e_tizen_policy_ext_get_rotation_cb(struct wl_client *client,
                                    struct wl_resource *resource,
                                    uint32_t id,
                                    struct wl_resource *surface)
{
   // implementation;
   int version = wl_resource_get_version(resource); // resource is tizen_policy_ext resource
   struct wl_resource *res;
   E_Pixmap *ep;
   Policy_Ext_Rototation *rot;

   ep = wl_resource_get_user_data(surface);
   EINA_SAFETY_ON_NULL_RETURN(ep);

   // Add rotation info
   rot = _policy_ext_rotation_get(ep);
   EINA_SAFETY_ON_NULL_RETURN(rot);

   res = wl_resource_create(client, &tizen_rotation_interface, version, id);
   if (res == NULL)
     {
        wl_client_post_no_memory(client);
        return;
     }

  rot->rotation_list = eina_list_append(rot->rotation_list, res);

  wl_resource_set_implementation(res, &_e_tizen_rotation_interface,
                                 ep, _e_tizen_rotation_destroy);
}

static const struct tizen_policy_ext_interface _e_tizen_policy_ext_interface =
{
   _e_tizen_policy_ext_get_rotation_cb
};

static void
_e_tizen_policy_ext_bind_cb(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   E_Comp_Data *cdata;
   struct wl_resource *res;

   if (!(cdata = data))
     {
        wl_client_post_no_memory(client);
        return;
     }

   if (!(res = wl_resource_create(client, &tizen_policy_ext_interface, version, id)))
     {
        ERR("Could not create scaler resource: %m");
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res, &_e_tizen_policy_ext_interface, cdata, NULL);
}

static void
_e_tizen_rotation_send_angle_change(E_Client *ec,
                                    enum tizen_rotation_angle angle)
{
   Eina_List *l;
   Policy_Ext_Rototation *rot;
   uint32_t serial;

   struct wl_resource *resource;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->pixmap);
   EINA_SAFETY_ON_NULL_RETURN(hash_policy_ext_rotation);

   rot = eina_hash_find(hash_policy_ext_rotation, &ec->pixmap);
   if (!rot) return;

   serial = wl_display_next_serial(ec->comp->wl_comp_data->wl.disp);

   rot->angle_change_done = EINA_FALSE;
   // set angle info moves to ack_change_angle_cb?
   rot->prev_angle = rot->cur_angle;
   rot->cur_angle = angle;

   EINA_LIST_FOREACH(rot->rotation_list, l, resource)
     {
        tizen_rotation_send_angle_change(resource, angle, serial);
     }
}

Eina_Bool
e_mod_rot_wl_init(void)
{
   E_Comp_Data *cdata;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp, EINA_FALSE);

   cdata = e_comp->wl_comp_data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata->wl.disp, EINA_FALSE);

   if (!wl_global_create(cdata->wl.disp, &tizen_policy_ext_interface, 1,
                         cdata, _e_tizen_policy_ext_bind_cb))
     {
        ERR("Could not add tizen_policy_ext to wayland globals: %m");
        return EINA_FALSE;
     }

   hash_policy_ext_rotation = eina_hash_pointer_new(free);

   return EINA_TRUE;
}


void
e_mod_rot_wl_shutdown(void)
{
   E_FREE_FUNC(hash_policy_ext_rotation, eina_hash_free);
}

void
e_mod_rot_wl_angle_change_send(E_Client *ec,
                               int angle)
{
   enum tizen_rotation_angle ang;
   switch (angle)
     {
        case 0:
          ang = TIZEN_ROTATION_ANGLE_0;
          break;
        case 90:
          ang = TIZEN_ROTATION_ANGLE_90;
          break;
        case 180:
          ang = TIZEN_ROTATION_ANGLE_180;
          break;
        case 270:
          ang = TIZEN_ROTATION_ANGLE_270;
          break;
        default:
          ang = TIZEN_ROTATION_ANGLE_0;
          break;
     }

   _e_tizen_rotation_send_angle_change(ec, ang);
}
