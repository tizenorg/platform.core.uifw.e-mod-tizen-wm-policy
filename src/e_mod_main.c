#include "e_mod_main.h"
#include "e_mod_atoms.h"
#include "e_mod_rotation.h"
#include "e_mod_keyboard.h"
#include "e_mod_notification.h"
#ifdef HAVE_WAYLAND_ONLY
#include "e_mod_wl.h"
#endif

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Policy-Mobile" };

Mod *_pol_mod = NULL;
Eina_Hash *hash_pol_desks = NULL;
Eina_Hash *hash_pol_clients = NULL;

static Eina_List *handlers = NULL;
static Eina_List *hooks = NULL;

static Pol_Client *_pol_client_add(E_Client *ec);
static void        _pol_client_del(Pol_Client *pc);
static Eina_Bool   _pol_client_normal_check(E_Client *ec);
static void        _pol_client_update(Pol_Client *pc);
static void        _pol_client_launcher_set(Pol_Client *pc);

static void        _pol_hook_client_eval_pre_new_client(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_client_eval_pre_fetch(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_client_eval_pre_post_fetch(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_client_eval_post_fetch(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_client_desk_set(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_hook_eval_end(void *d EINA_UNUSED, E_Client *ec);

static void        _pol_cb_desk_data_free(void *data);
static void        _pol_cb_client_data_free(void *data);
static Eina_Bool   _pol_cb_zone_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_del(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_move_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_desk_count_set(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_desk_show(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_client_remove(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_add(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_move(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_resize(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_stack(void *data EINA_UNUSED, int type, void *event);
#ifndef HAVE_WAYLAND_ONLY
static Eina_Bool   _pol_cb_client_property(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_window_property(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Property *ev);
#endif

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
   _SET(lock_user_fullscreen);
   _SET(lock_client_fullscreen);
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
          e_client_unmaximize(ec, ec->maximized);

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
   _SET(lock_user_fullscreen);
   _SET(lock_client_fullscreen);
#undef _SET

   ec->skip_fullscreen = 0;

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

   if (e_mod_pol_client_is_quickpanel(ec))
     {
        evas_object_move(ec->frame, -10000, -10000);
        return EINA_FALSE;
     }

   if (e_mod_pol_client_is_keyboard(ec) ||
       e_mod_pol_client_is_keyboard_sub(ec))
     {
        Pol_Client *pc;
        pc = eina_hash_find(hash_pol_clients, &ec);
        if (pc) _pol_client_del(pc);

        e_mod_pol_keyboard_layout_apply(ec);
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
_pol_client_maximize_pre(Pol_Client *pc)
{
   E_Client *ec;
   int zx, zy, zw, zh;

   ec = pc->ec;

   if (ec->desk->visible)
     e_zone_useful_geometry_get(ec->zone, &zx, &zy, &zw, &zh);
   else
     {
        zx = ec->zone->x;
        zy = ec->zone->y;
        zw = ec->zone->w;
        zh = ec->zone->h;
     }

   ec->x = ec->client.x = zx;
   ec->y = ec->client.y = zy;
   ec->w = ec->client.w = zw;
   ec->h = ec->client.h = zh;

   EC_CHANGED(ec);
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
     {
        e_client_maximize(ec, E_MAXIMIZE_EXPAND | E_MAXIMIZE_BOTH);

        if (ec->changes.need_maximize)
          _pol_client_maximize_pre(pc);
     }

   /* do not allow client to change these properties */
   ec->lock_user_location = 1;
   ec->lock_client_location = 1;
   ec->lock_user_size = 1;
   ec->lock_client_size = 1;
   ec->lock_user_shade = 1;
   ec->lock_client_shade = 1;
   ec->lock_user_maximize = 1;
   ec->lock_client_maximize = 1;
   ec->lock_user_fullscreen = 1;
   ec->lock_client_fullscreen = 1;
   ec->skip_fullscreen = 1;

   if (!e_mod_pol_client_is_home_screen(ec))
     ec->lock_client_stacking = 1;
}

static void
_pol_hook_client_eval_pre_new_client(void *d EINA_UNUSED, E_Client *ec)
{
   if (e_object_is_del(E_OBJECT(ec))) return;

   if (e_mod_pol_client_is_keyboard_sub(ec))
     ec->placed = 1;
}

static void
_pol_hook_client_eval_pre_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   if (e_object_is_del(E_OBJECT(ec))) return;

   e_mod_pol_stack_hook_pre_fetch(ec);
   e_mod_pol_notification_hook_pre_fetch(ec);
}

static void
_pol_hook_client_eval_pre_post_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   if (e_object_is_del(E_OBJECT(ec))) return;

   e_mod_pol_stack_hook_pre_post_fetch(ec);
   e_mod_pol_notification_hook_pre_post_fetch(ec);
}

static void
_pol_hook_client_eval_post_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   Pol_Client *pc;
   Pol_Desk *pd;

   if (e_object_is_del(E_OBJECT(ec))) return;
   /* Following E_Clients will be added to module hash and will be managed.
    *
    *  - Not new client: Updating internal info of E_Client has been finished
    *    by e main evaluation, thus module can classify E_Client and manage it.
    *
    *  - New client that has valid buffer: This E_Client has been passed e main
    *    evaluation, and it has handled first wl_surface::commit request.
    */
   if ((ec->new_client) && (!e_pixmap_usable_get(ec->pixmap))) return;

   if (e_mod_pol_client_is_keyboard(ec) ||
       e_mod_pol_client_is_keyboard_sub(ec))
     {
        Pol_Client *pc;
        pc = eina_hash_find(hash_pol_clients, &ec);
        if (pc) _pol_client_del(pc);

        e_mod_pol_keyboard_layout_apply(ec);
     }

   if (!e_util_strcmp("wl_pointer-cursor", ec->icccm.window_role))
     {
        Pol_Client *pc;
        pc = eina_hash_find(hash_pol_clients, &ec);
        if (pc)
          {
             _pol_client_del(pc);
             e_client_unmaximize(ec, E_MAXIMIZE_BOTH);
          }
        return;
     }

   if (!_pol_client_normal_check(ec)) return;

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
   if (ec->internal) return;
   if (ec->new_client) return;

   pc = eina_hash_find(hash_pol_clients, &ec);
   pd = eina_hash_find(hash_pol_desks, &ec->desk);

   if ((!pc) && (pd))
     _pol_client_add(ec);
   else if ((pc) && (!pd))
     _pol_client_del(pc);
}

static void
_pol_hook_eval_end(void *d EINA_UNUSED, E_Client *ec)
{
   /* calculate e_client visibility */
   e_mod_pol_visibility_calc();
}

static void
_pol_hook_client_fullscreen_pre(void* data EINA_UNUSED, E_Client *ec)
{

   if (e_object_is_del(E_OBJECT(ec))) return;
   if (!_pol_client_normal_check(ec)) return;
   if (ec->internal) return;

   ec->skip_fullscreen = 1;
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

   e_mod_pol_stack_cb_client_remove(ev->ec);
   e_mod_pol_notification_client_del(ev->ec);
   e_mod_pol_client_visibility_del(ev->ec);
   e_mod_pol_visibility_calc();
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_client_del(ev->ec);
#endif

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
_pol_cb_client_move(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   /* send changed position */
   e_mod_pol_wl_position_send(ev->ec);

   /* calculate e_client visibility */
   e_mod_pol_visibility_calc();

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_client_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec;

   ev = event;
   ec = ev->ec;
   if (!ev) return ECORE_CALLBACK_PASS_ON;
      if (e_mod_pol_client_is_keyboard(ec) ||
       e_mod_pol_client_is_keyboard_sub(ec))
     {
#ifdef HAVE_WAYLAND_ONLY
        E_Client *comp_ec;
        E_CLIENT_REVERSE_FOREACH(e_comp, comp_ec)
          {
             if (e_client_util_ignored_get(comp_ec)) continue;
             if (!e_mod_pol_client_is_conformant(comp_ec)) continue;
             if (ec->visible)
               e_mod_pol_wl_keyboard_send(comp_ec, EINA_TRUE, ec->x, ec->y, ec->client.w, ec->client.h);
             else
               e_mod_pol_wl_keyboard_send(comp_ec, EINA_FALSE, ec->x, ec->y, ec->client.w, ec->client.h);
          }
#endif
     }

   /* calculate e_client visibility */
   e_mod_pol_visibility_calc();

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_client_stack(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;
   /* calculate e_client visibility */
   e_mod_pol_visibility_calc();

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_client_property(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client_Property *ev;

   ev = event;
   if (!ev || (!ev->ec)) return 0;
   if (ev->property & E_CLIENT_PROPERTY_CLIENT_TYPE)
     {
        if (e_mod_pol_client_is_home_screen(ev->ec))
          {
             ev->ec->lock_client_stacking = 0;
             return EINA_TRUE;
          }
        else if (e_mod_pol_client_is_lock_screen(ev->ec))
          return EINA_TRUE;
     }

   return EINA_FALSE;
}

#ifndef HAVE_WAYLAND_ONLY
static Eina_Bool
_pol_cb_window_property(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Property *ev)
{
   if (ev->atom == E_MOD_POL_ATOM_WINDOW_OPAQUE)
     {
        e_mod_pol_visibility_cb_window_property(ev);
     }
   else if (ev->atom == E_MOD_POL_ATOM_NOTIFICATION_LEVEL)
     {
        E_Client *ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->win);

        /* Before maprequet, ec is NULL. notification level will be updated in
         * pre_post_fetch which is called after e_hints setting. */
        if (ec)
           e_mod_pol_notification_level_update(ec);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_window_configure_request(void *data EINA_UNUSED, int type EINA_UNUSED, Ecore_X_Event_Window_Configure_Request *ev)
{
   e_mod_pol_keyboard_configure(ev);

   return ECORE_CALLBACK_PASS_ON;
}
#endif

void
e_mod_pol_desk_add(E_Desk *desk)
{
   Pol_Desk *pd;
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
   E_CLIENT_FOREACH(e_comp, ec)
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

Eina_Bool
e_mod_pol_client_is_lock_screen(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   if (ec->client_type == 2)
     return EINA_TRUE;

   return EINA_FALSE;
}

Eina_Bool
e_mod_pol_client_is_home_screen(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   if (ec->client_type == 1)
     return EINA_TRUE;


   return EINA_FALSE;
}

Eina_Bool
e_mod_pol_client_is_quickpanel(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   if (!e_util_strcmp(ec->icccm.title, "QUICKPANEL"))
     return EINA_TRUE;

   return EINA_FALSE;
}

Eina_Bool
e_mod_pol_client_is_conformant(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec->comp_data, EINA_FALSE);

#ifdef HAVE_WAYLAND_ONLY
   E_Comp_Wl_Client_Data * cdata = ec->comp_data;
   if (cdata->conformant == 1)
     {
        return EINA_TRUE;
     }
#endif

   return EINA_FALSE;
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
   E_Zone *zone;
   Config_Desk *d;
   const Eina_List *l;
   int i, n;
   char buf[PATH_MAX];

   mod = E_NEW(Mod, 1);
   mod->module = m;
   _pol_mod = mod;

   hash_pol_clients = eina_hash_pointer_new(_pol_cb_client_data_free);
   hash_pol_desks = eina_hash_pointer_new(_pol_cb_desk_data_free);

   e_mod_pol_stack_init();
   e_mod_pol_visibility_init();
   e_mod_pol_atoms_init();
   e_mod_pol_notification_init();
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_init();
#endif

   /* initialize configure and config data type */
   snprintf(buf, sizeof(buf), "%s/e-module-policy.edj",
            e_module_dir_get(m));
   e_configure_registry_category_add("windows", 50, _("Windows"), NULL,
                                     "preferences-system-windows");
   e_configure_registry_item_add("windows/policy-tizen", 150,
                                 _("Tizen Policy"), NULL, buf,
                                 e_int_config_pol_mobile);

   e_mod_pol_conf_init(mod);

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        //Eina_Bool home_add = EINA_FALSE;
        n = zone->desk_y_count * zone->desk_x_count;
        for (i = 0; i < n; i++)
          {
             d = e_mod_pol_conf_desk_get_by_nums(_pol_mod->conf,
                                                 e_comp->num,
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
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_MOVE,
                         _pol_cb_client_move, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_RESIZE,
                         _pol_cb_client_resize, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_STACK,
                         _pol_cb_client_stack, NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_PROPERTY,
                         _pol_cb_client_property, NULL);

#ifndef HAVE_WAYLAND_ONLY
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_PROPERTY,
                         _pol_cb_window_property, NULL);
   E_LIST_HANDLER_APPEND(handlers, ECORE_X_EVENT_WINDOW_CONFIGURE_REQUEST,
                         _pol_cb_window_configure_request, NULL);
#endif

   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_EVAL_PRE_NEW_CLIENT,
                        _pol_hook_client_eval_pre_new_client, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_EVAL_PRE_FETCH,
                        _pol_hook_client_eval_pre_fetch, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_EVAL_PRE_POST_FETCH,
                        _pol_hook_client_eval_pre_post_fetch, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_EVAL_POST_FETCH,
                        _pol_hook_client_eval_post_fetch, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_DESK_SET,
                        _pol_hook_client_desk_set, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_EVAL_END,
                        _pol_hook_eval_end, NULL);
   E_CLIENT_HOOK_APPEND(hooks, E_CLIENT_HOOK_FULLSCREEN_PRE,
                        _pol_hook_client_fullscreen_pre, NULL);

   e_mod_pol_rotation_init();

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

   E_FREE_FUNC(hash_pol_desks, eina_hash_free);
   E_FREE_FUNC(hash_pol_clients, eina_hash_free);

   e_mod_pol_stack_shutdonw();
   e_mod_pol_notification_shutdown();
   e_mod_pol_visibility_shutdown();
   e_mod_pol_rotation_shutdown();
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_shutdown();
#endif

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
