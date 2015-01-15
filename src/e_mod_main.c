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
#include "e_mod_main.h"
#include "e_mod_atoms.h"
#include "e_mod_rotation.h"

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Policy-Mobile" };

Mod *_pol_mod = NULL;
Eina_Hash *hash_pol_desks = NULL;
Eina_Hash *hash_pol_clients = NULL;

static Eina_List *handlers = NULL;
static Eina_List *hooks = NULL;
static Eina_List *intercept_hooks = NULL;
static Ecore_Idle_Enterer *_idle_enterer = NULL;

static Pol_Client *_pol_client_add(E_Client *ec);
static void        _pol_client_del(Pol_Client *pc);
static Eina_Bool   _pol_client_normal_check(E_Client *ec);
static void        _pol_client_update(Pol_Client *pc);
static void        _pol_client_launcher_set(Pol_Client *pc);

static void        _pol_hook_client_eval_post_fetch(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_client_desk_set(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_new_client(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_new_client_post(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_client_del(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_eval_end(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_eval_fetch(void *d EINA_UNUSED, E_Client *ec);

static Eina_Bool   _pol_intercept_hook_show_helper(void *d EINA_UNUSED, E_Client *ec);
static Eina_Bool   _pol_intercept_hook_hide(void *d EINA_UNUSED, E_Client *ec);

static void        _pol_cb_desk_data_free(void *data);
static void        _pol_cb_client_data_free(void *data);
static Eina_Bool   _pol_cb_zone_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_del(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_move_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_desk_count_set(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_desk_show(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_client_remove(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_add(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_window_property(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Property *ev);

static Eina_Bool   _pol_cb_window_configure(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Configure *ev);
static Eina_Bool   _pol_cb_window_message(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Client_Message *ev);
static Eina_Bool   _pol_cb_zone_rotation_change_begin(void *data EINA_UNUSED, int ev_type EINA_UNUSED, E_Event_Zone_Rotation_Change_Begin *ev);

static Eina_Bool   _pol_cb_idle_enterer(void *data EINA_UNUSED);

static void        _pol_cb_evas_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);

static void
_pol_client_launcher_set(Pol_Client *pc)
{
   Pol_Client *pc2;

   pc2 = e_mod_pol_client_launcher_get(pc->ec->zone);
   if (pc2) return;

   if (pc->ec->netwm.type != _pol_mod->conf->launcher.type)
     return;

   if (e_util_strcmp(pc->ec->icccm.class,
                     _pol_mod->conf->launcher.clas))
     return;


   if (e_util_strcmp(pc->ec->icccm.title,
                     _pol_mod->conf->launcher.title))
     {
        /* check netwm name instead, because comp_x had ignored
         * icccm name when fetching */
        if (e_util_strcmp(pc->ec->netwm.name,
                          _pol_mod->conf->launcher.title))
          {
             return;
          }
     }

   _pol_mod->launchers = eina_list_append(_pol_mod->launchers, pc);
}

static Pol_Client *
_pol_client_add(E_Client *ec)
{
   Pol_Client *pc;
   Pol_Desk *pd;

   if (e_object_is_del(E_OBJECT(ec))) return NULL;
   if (!_pol_client_normal_check(ec)) return NULL;

   pc = eina_hash_find(hash_pol_clients, &ec);
   if (pc) return NULL;

   pd = eina_hash_find(hash_pol_desks, &ec->desk);
   if (!pd) return NULL;

   pc = E_NEW(Pol_Client, 1);
   pc->ec = ec;

#undef _SET
# define _SET(a) pc->orig.a = ec->a
   _SET(borderless);
   _SET(fullscreen);
   _SET(maximized);
   _SET(lock_user_location);
   _SET(lock_client_location);
   _SET(lock_user_size);
   _SET(lock_client_size);
   _SET(lock_client_stacking);
   _SET(lock_user_shade);
   _SET(lock_client_shade);
   _SET(lock_user_maximize);
   _SET(lock_client_maximize);
#undef _SET

   _pol_client_launcher_set(pc);

   eina_hash_add(hash_pol_clients, &ec, pc);

   _pol_client_update(pc);

   return pc;
}

static void
_pol_client_del(Pol_Client *pc)
{
   E_Client *ec;
   Eina_Bool changed = EINA_FALSE;

   ec = pc->ec;

   if (pc->orig.borderless != ec->borderless)
     {
        ec->border.changed = 1;
        changed = EINA_TRUE;
     }

   if ((pc->orig.fullscreen != ec->fullscreen) &&
       (pc->orig.fullscreen))
     {
        ec->need_fullscreen = 1;
        changed = EINA_TRUE;
     }

   if (pc->orig.maximized != ec->maximized)
     {
        if (pc->orig.maximized)
          ec->changes.need_maximize = 1;
        else
          ec->changes.need_unmaximize = 1;
        changed = EINA_TRUE;
     }

#undef _SET
# define _SET(a) ec->a = pc->orig.a
   _SET(borderless);
   _SET(fullscreen);
   _SET(maximized);
   _SET(lock_user_location);
   _SET(lock_client_location);
   _SET(lock_user_size);
   _SET(lock_client_size);
   _SET(lock_client_stacking);
   _SET(lock_user_shade);
   _SET(lock_client_shade);
   _SET(lock_user_maximize);
   _SET(lock_client_maximize);
#undef _SET

   /* only set it if the border is changed or fullscreen/maximize has changed */
   if (changed)
     EC_CHANGED(pc->ec);

   _pol_mod->launchers = eina_list_remove(_pol_mod->launchers, pc);

   eina_hash_del_by_key(hash_pol_clients, &pc->ec);
}

static Eina_Bool
_pol_client_normal_check(E_Client *ec)
{
   if ((e_client_util_ignored_get(ec)) ||
       (!ec->pixmap))
     {
        return EINA_FALSE;
     }

   if ((ec->netwm.type == E_WINDOW_TYPE_NORMAL) ||
       (ec->netwm.type == E_WINDOW_TYPE_UNKNOWN))
     {
        return EINA_TRUE;
     }

   return EINA_FALSE;
}

static void
_pol_client_update(Pol_Client *pc)
{
   E_Client *ec;

   ec = pc->ec;

   if (ec->remember)
     {
        e_remember_del(ec->remember);
        ec->remember = NULL;
     }

   /* skip hooks of e_remeber for eval_pre_post_fetch and eval_post_new_client */
   ec->internal_no_remember = 1;

   if (!ec->borderless)
     {
        ec->borderless = 1;
        ec->border.changed = 1;
        EC_CHANGED(pc->ec);
     }

   if (!ec->maximized)
     e_client_maximize(ec, E_MAXIMIZE_EXPAND | E_MAXIMIZE_BOTH);

   /* do not allow client to change these properties */
   ec->lock_user_location = 1;
   ec->lock_client_location = 1;
   ec->lock_user_size = 1;
   ec->lock_client_size = 1;
   ec->lock_client_stacking = 1;
   ec->lock_user_shade = 1;
   ec->lock_client_shade = 1;
   ec->lock_user_maximize = 1;
   ec->lock_client_maximize = 1;
}

static void
_pol_hook_client_eval_post_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   Pol_Client *pc;
   Pol_Desk *pd;

   if (e_object_is_del(E_OBJECT(ec))) return;

   e_mod_pol_visibility_calc();

   if (!_pol_client_normal_check(ec)) return;
   if (ec->new_client) return;

   pd = eina_hash_find(hash_pol_desks, &ec->desk);
   if (!pd) return;

   pc = eina_hash_find(hash_pol_clients, &ec);
   if (pc)
     {
        _pol_client_launcher_set(pc);
        return;
     }

   _pol_client_add(ec);
}

static void
_pol_hook_client_desk_set(void *d EINA_UNUSED, E_Client *ec)
{
   Pol_Client *pc;
   Pol_Desk *pd;

   if (e_object_is_del(E_OBJECT(ec))) return;
   if (!_pol_client_normal_check(ec)) return;

   pc = eina_hash_find(hash_pol_clients, &ec);
   pd = eina_hash_find(hash_pol_desks, &ec->desk);

   if ((!pc) && (pd))
     _pol_client_add(ec);
   else if ((pc) && (!pd))
     _pol_client_del(pc);
}

static void
_pol_hook_new_client(void *d EINA_UNUSED, E_Client *ec)
{
   e_mod_pol_rot_hook_new_client(ec);
}

static void
_pol_hook_new_client_post(void *d EINA_UNUSED, E_Client *ec)
{
   if (ec->frame)
     {
        evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW, _pol_cb_evas_show, ec);
     }
   return;
}


static void
_pol_hook_client_del(void *d EINA_UNUSED, E_Client *ec)
{
   e_mod_pol_rot_hook_client_del(ec);
   e_mod_pol_rot_hook_client_free(ec);
}

static void
_pol_hook_eval_end(void *d EINA_UNUSED, E_Client *ec)
{
   e_mod_pol_rot_hook_eval_end(ec);
}

static void
_pol_hook_eval_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   e_mod_pol_rot_hook_eval_fetch(ec);
}

static Eina_Bool
_pol_intercept_hook_show_helper(void *d EINA_UNUSED, E_Client *ec)
{
   return e_mod_pol_rot_intercept_hook_show_helper(ec);
}

static Eina_Bool
_pol_intercept_hook_hide(void *d EINA_UNUSED, E_Client *ec)
{
   return e_mod_pol_rot_intercept_hook_hide(ec);
}

static void
_pol_cb_desk_data_free(void *data)
{
   free(data);
}

static void
_pol_cb_client_data_free(void *data)
{
   free(data);
}

static Eina_Bool
_pol_cb_zone_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Zone_Add *ev;
   E_Zone *zone;
   Config_Desk *d;
   int i, n;

   ev = event;
   zone = ev->zone;
   n = zone->desk_y_count * zone->desk_x_count;
   for (i = 0; i < n; i++)
     {
        d = e_mod_pol_conf_desk_get_by_nums(_pol_mod->conf,
                                            zone->comp->num,
                                            zone->num,
                                            zone->desks[i]->x,
                                            zone->desks[i]->y);
        if (d)
          e_mod_pol_desk_add(zone->desks[i]);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_zone_del(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Zone_Del *ev;
   E_Zone *zone;
   Pol_Desk *pd;
   int i, n;

   ev = event;
   zone = ev->zone;
   n = zone->desk_y_count * zone->desk_x_count;
   for (i = 0; i < n; i++)
     {
        pd = eina_hash_find(hash_pol_desks, &zone->desks[i]);
        if (pd) e_mod_pol_desk_del(pd);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_zone_move_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Zone_Move_Resize *ev;
   Pol_Softkey *softkey;

   ev = event;

   if (_pol_mod->conf->use_softkey)
     {
        softkey = e_mod_pol_softkey_get(ev->zone);
        e_mod_pol_softkey_update(softkey);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_zone_desk_count_set(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Zone_Desk_Count_Set *ev;
   E_Zone *zone;
   E_Desk *desk;
   Eina_Iterator *it;
   Pol_Desk *pd;
   Config_Desk *d;
   int i, n;
   Eina_Bool found;
   Eina_List *desks_del = NULL;

   ev = event;
   zone = ev->zone;

   /* remove deleted desk from hash */
   it = eina_hash_iterator_data_new(hash_pol_desks);
   while (eina_iterator_next(it, (void **)&pd))
     {
        if (pd->zone != zone) continue;

        found = EINA_FALSE;
        n = zone->desk_y_count * zone->desk_x_count;
        for (i = 0; i < n; i++)
          {
             if (pd->desk == zone->desks[i])
               {
                  found = EINA_TRUE;
                  break;
               }
          }
        if (!found)
          desks_del = eina_list_append(desks_del, pd->desk);
     }
   eina_iterator_free(it);

   EINA_LIST_FREE(desks_del, desk)
     {
        pd = eina_hash_find(hash_pol_desks, &desk);
        if (pd) e_mod_pol_desk_del(pd);
     }

   /* add newly added desk to hash */
   n = zone->desk_y_count * zone->desk_x_count;
   for (i = 0; i < n; i++)
     {
        d = e_mod_pol_conf_desk_get_by_nums(_pol_mod->conf,
                                            zone->comp->num,
                                            zone->num,
                                            zone->desks[i]->x,
                                            zone->desks[i]->y);
        if (d)
          e_mod_pol_desk_add(zone->desks[i]);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_desk_show(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Desk_Show *ev;
   Pol_Softkey *softkey;

   ev = event;

   if (_pol_mod->conf->use_softkey)
     {
        softkey = e_mod_pol_softkey_get(ev->desk->zone);
        if (eina_hash_find(hash_pol_desks, &ev->desk))
          e_mod_pol_softkey_show(softkey);
        else
          e_mod_pol_softkey_hide(softkey);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_client_remove(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   Pol_Client *pc;

   ev = event;
   pc = eina_hash_find(hash_pol_clients, &ev->ec);
   if (!pc) return ECORE_CALLBACK_PASS_ON;

   eina_hash_del_by_key(hash_pol_clients, &ev->ec);

   e_mod_pol_client_visibility_del(ev->ec);
   e_mod_pol_visibility_calc();

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_client_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   e_mod_pol_client_window_opaque_set(ev->ec);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_window_property(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Property *ev)
{
   if (ev->atom == E_MOD_POL_ATOM_WINDOW_OPAQUE)
     {
        e_mod_pol_visibility_cb_window_property(ev);
     }
   else if ((ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_SUPPORTED) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_0_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_90_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_180_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_270_GEOMETRY) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_APP_SUPPORTED) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_PREFERRED_ROTATION) ||
            (ev->atom == ECORE_X_ATOM_E_WINDOW_ROTATION_AVAILABLE_LIST))
     {

        e_mod_pol_rot_cb_window_property(ev);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_window_configure(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Configure *ev)
{
   e_mod_pol_rot_cb_window_configure(ev);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_pol_cb_zone_rotation_change_begin(void *data EINA_UNUSED, int ev_type EINA_UNUSED, E_Event_Zone_Rotation_Change_Begin *ev)
{
   if ((!ev) || (!ev->zone)) return ECORE_CALLBACK_PASS_ON;

   e_mod_pol_rot_cb_zone_rotation_change_begin(ev);
   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_pol_cb_window_message(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Client_Message *ev)
{
   e_mod_pol_rot_cb_window_message(ev);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_pol_cb_idle_enterer(void *data EINA_UNUSED)
{
   e_mod_pol_rot_cb_idle_enterer();

   return ECORE_CALLBACK_RENEW;
}

static void
_pol_cb_evas_show(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   E_Client *ec = data;
   e_mod_pol_rot_cb_evas_show(ec);
   return;
}

void
e_mod_pol_desk_add(E_Desk *desk)
{
   Pol_Desk *pd;
   E_Comp *comp;
   E_Client *ec;
   Pol_Softkey *softkey;
   const Eina_List *l;

   pd = eina_hash_find(hash_pol_desks, &desk);
   if (pd) return;

   pd = E_NEW(Pol_Desk, 1);
   pd->desk = desk;
   pd->zone = desk->zone;

   eina_hash_add(hash_pol_desks, &desk, pd);

   /* add clients */
   EINA_LIST_FOREACH(e_comp_list(), l, comp)
     E_CLIENT_FOREACH(comp, ec)
       {
          if (pd->desk == ec->desk)
            _pol_client_add(ec);
       }

   /* add and show softkey */
   if (_pol_mod->conf->use_softkey)
     {
        softkey = e_mod_pol_softkey_get(desk->zone);
        if (!softkey)
          softkey = e_mod_pol_softkey_add(desk->zone);
        if (e_desk_current_get(desk->zone) == desk)
          e_mod_pol_softkey_show(softkey);
     }
}

void
e_mod_pol_desk_del(Pol_Desk *pd)
{
   Eina_Iterator *it;
   Pol_Client *pc;
   E_Client *ec;
   Eina_List *clients_del = NULL;
   Pol_Softkey *softkey;
   Eina_Bool keep = EINA_FALSE;
   int i, n;

   /* hide and delete softkey */
   if (_pol_mod->conf->use_softkey)
     {
        softkey = e_mod_pol_softkey_get(pd->zone);
        if (e_desk_current_get(pd->zone) == pd->desk)
          e_mod_pol_softkey_hide(softkey);

        n = pd->zone->desk_y_count * pd->zone->desk_x_count;
        for (i = 0; i < n; i++)
          {
             if (eina_hash_find(hash_pol_desks, &pd->zone->desks[i]))
               {
                  keep = EINA_TRUE;
                  break;
               }
          }

        if (!keep)
          e_mod_pol_softkey_del(softkey);
     }

   /* remove clients */
   it = eina_hash_iterator_data_new(hash_pol_clients);
   while (eina_iterator_next(it, (void **)&pc))
     {
        if (pc->ec->desk == pd->desk)
          clients_del = eina_list_append(clients_del, pc->ec);
     }
   eina_iterator_free(it);

   EINA_LIST_FREE(clients_del, ec)
     {
        pc = eina_hash_find(hash_pol_clients, &ec);
        if (pc) _pol_client_del(pc);
     }

   eina_hash_del_by_key(hash_pol_desks, &pd->desk);
}

Pol_Client *
e_mod_pol_client_launcher_get(E_Zone *zone)
{
   Pol_Client *pc;
   Eina_List *l;

   EINA_LIST_FOREACH(_pol_mod->launchers, l, pc)
     {
        if (pc->ec->zone == zone)
          return pc;
     }
   return NULL;
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

EAPI void *
e_modapi_init(E_Module *m)
{
   Mod *mod;
   E_Comp *comp;
   E_Zone *zone;
   Config_Desk *d;
   const Eina_List *l, *ll;
   int i, n;
   char buf[PATH_MAX];

   mod = E_NEW(Mod, 1);
   mod->module = m;
   _pol_mod = mod;

   hash_pol_clients = eina_hash_pointer_new(_pol_cb_client_data_free);
   hash_pol_desks = eina_hash_pointer_new(_pol_cb_desk_data_free);

   e_mod_pol_visibility_init();
   e_mod_pol_atoms_init();

   /* initialize configure and config data type */
   snprintf(buf, sizeof(buf), "%s/e-module-policy.edj",
            e_module_dir_get(m));
   e_configure_registry_category_add("windows", 50, _("Windows"), NULL,
                                     "preferences-system-windows");
   e_configure_registry_item_add("windows/policy-tizen", 150,
                                 _("Tizen Policy"), NULL, buf,
                                 e_int_config_pol_mobile);

   e_mod_pol_conf_init(mod);

   EINA_LIST_FOREACH(e_comp_list(), l, comp)
     EINA_LIST_FOREACH(comp->zones, ll, zone)
       {
          //Eina_Bool home_add = EINA_FALSE;
          n = zone->desk_y_count * zone->desk_x_count;
          for (i = 0; i < n; i++)
            {
               d = e_mod_pol_conf_desk_get_by_nums(_pol_mod->conf,
                                                   comp->num,
                                                   zone->num,
                                                   zone->desks[i]->x,
                                                   zone->desks[i]->y);
               if (d)
                 {
                    e_mod_pol_desk_add(zone->desks[i]);
                    //home_add = EINA_TRUE;
                 }
            }

          /* FIXME: should consider the case that illume-home module
           * is not loaded yet and make it configurable.
           * and also, this code will be enabled when e_policy stuff lands in e.
           */
          //if (home_add)
          //  e_policy_zone_home_add_request(zone);
       }

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_ADD,
                         _pol_cb_zone_add, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_DEL,
                         _pol_cb_zone_del, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_MOVE_RESIZE,
                         _pol_cb_zone_move_resize, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_DESK_COUNT_SET,
                         _pol_cb_zone_desk_count_set, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_DESK_SHOW,
                         _pol_cb_desk_show, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_REMOVE,
                         _pol_cb_client_remove, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_ADD,
                         _pol_cb_client_add, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_PROPERTY,
                         _pol_cb_window_property, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_CONFIGURE,
                         _pol_cb_window_configure, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_CLIENT_MESSAGE,
                         _pol_cb_window_message, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_ROTATION_CHANGE_BEGIN,
                         _pol_cb_zone_rotation_change_begin, NULL);

   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_EVAL_POST_FETCH,
                        _pol_hook_client_eval_post_fetch, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_DESK_SET,
                        _pol_hook_client_desk_set, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_NEW_CLIENT,
                        _pol_hook_new_client, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_NEW_CLIENT_POST,
                        _pol_hook_new_client_post, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_DEL,
                        _pol_hook_client_del, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_EVAL_END,
                        _pol_hook_eval_end, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_EVAL_FETCH,
                        _pol_hook_eval_fetch, NULL);

   E_COMP_OBJECT_INTERCEPT_HOOK_APPEND(intercept_hooks,
                                       E_COMP_OBJECT_INTERCEPT_HOOK_SHOW_HELPER,
                                       _pol_intercept_hook_show_helper, NULL);
   E_COMP_OBJECT_INTERCEPT_HOOK_APPEND(intercept_hooks,
                                       E_COMP_OBJECT_INTERCEPT_HOOK_HIDE,
                                       _pol_intercept_hook_hide, NULL);

   _idle_enterer = ecore_idle_enterer_add(_pol_cb_idle_enterer, NULL);
   // change to ecore_idle_enterer_before_add()?

   return mod;
}

EAPI int
e_modapi_shutdown(E_Module *m)
{
   Mod *mod = m->data;
   Eina_Inlist *l;
   Pol_Softkey *softkey;

   eina_list_free(mod->launchers);
   EINA_INLIST_FOREACH_SAFE(mod->softkeys, l, softkey)
     e_mod_pol_softkey_del(softkey);
   E_FREE_LIST(hooks, e_client_hook_del);
   E_FREE_LIST(handlers, ecore_event_handler_del);
   E_FREE_LIST(intercept_hooks, e_comp_object_intercept_hook_del);

   if (_idle_enterer)
     {
         ecore_idle_enterer_del(_idle_enterer);
         _idle_enterer = NULL;
     }

   E_FREE_FUNC(hash_pol_desks, eina_hash_free);
   E_FREE_FUNC(hash_pol_clients, eina_hash_free);

   e_mod_pol_viisibility_shutdown();

   e_configure_registry_item_del("windows/policy-tizen");
   e_configure_registry_category_del("windows");

   if (mod->conf_dialog)
     {
        e_object_del(E_OBJECT(mod->conf_dialog));
        mod->conf_dialog = NULL;
     }

   e_mod_pol_conf_shutdown(mod);

   E_FREE(mod);

   _pol_mod = NULL;

   return 1;
}

EAPI int
e_modapi_save(E_Module *m)
{
   Mod *mod = m->data;
   e_config_domain_save("module.policy-tizen",
                        mod->conf_edd,
                        mod->conf);
   return 1;
}
