#include "e_mod_main.h"
#include "e_mod_rotation.h"
#include "e_mod_keyboard.h"
#ifdef HAVE_WAYLAND_ONLY
#include "e_mod_wl.h"
#endif

E_API E_Module_Api e_modapi = { E_MODULE_API_VERSION, "Policy-Mobile" };

Mod *_pol_mod = NULL;
Eina_Hash *hash_pol_desks = NULL;
Eina_Hash *hash_pol_clients = NULL;

static Eina_List *handlers = NULL;
static Eina_List *hooks_ec = NULL;
static Eina_List *hooks_cp = NULL;

static Pol_Client *_pol_client_add(E_Client *ec);
static void        _pol_client_del(Pol_Client *pc);
static Eina_Bool   _pol_client_normal_check(E_Client *ec);
static void        _pol_client_maximize_policy_apply(Pol_Client *pc);
static void        _pol_client_maximize_policy_cancel(Pol_Client *pc);
static void        _pol_client_launcher_set(Pol_Client *pc);

static void        _pol_cb_hook_client_eval_pre_new_client(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_cb_hook_client_eval_pre_fetch(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_cb_hook_client_eval_pre_post_fetch(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_cb_hook_client_eval_post_fetch(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_cb_hook_client_desk_set(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_cb_hook_client_eval_end(void *d EINA_UNUSED, E_Client *ec);
static void        _pol_cb_hook_client_fullscreen_pre(void *data EINA_UNUSED, E_Client *ec);

static void        _pol_cb_hook_pixmap_del(void *data EINA_UNUSED, E_Pixmap *cp);

static void        _pol_cb_desk_data_free(void *data);
static void        _pol_cb_client_data_free(void *data);
static Eina_Bool   _pol_cb_zone_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_del(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_move_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_desk_count_set(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_zone_display_state_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_desk_show(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_client_remove(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_add(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_move(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_resize(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_stack(void *data EINA_UNUSED, int type, void *event);
static Eina_Bool   _pol_cb_client_property(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool   _pol_cb_client_vis_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED);
static Eina_Bool   _pol_cb_module_defer_job(void *data EINA_UNUSED);


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

   if (e_object_is_del(E_OBJECT(ec))) return NULL;

   pc = eina_hash_find(hash_pol_clients, &ec);
   if (pc) return NULL;

   pc = E_NEW(Pol_Client, 1);
   pc->ec = ec;

   eina_hash_add(hash_pol_clients, &ec, pc);

   return pc;
}

static void
_pol_client_del(Pol_Client *pc)
{
   eina_hash_del_by_key(hash_pol_clients, &pc->ec);
}

static Eina_Bool
_pol_client_normal_check(E_Client *ec)
{
   Pol_Client *pc;

   if ((e_client_util_ignored_get(ec)) ||
       (!ec->pixmap))
     {
        return EINA_FALSE;
     }

   if (e_mod_pol_client_is_quickpanel(ec))
     {
        return EINA_FALSE;
     }

   if (e_mod_pol_client_is_keyboard(ec) ||
       e_mod_pol_client_is_keyboard_sub(ec))
     {
        e_mod_pol_keyboard_layout_apply(ec);
        goto cancel_max;
     }
   else if (e_mod_pol_client_is_volume_tv(ec))
     goto cancel_max;
   else if (!e_util_strcmp("e_demo", ec->icccm.window_role))
     goto cancel_max;
#ifdef HAVE_WAYLAND_ONLY
   else if (e_mod_pol_client_is_subsurface(ec))
     goto cancel_max;
#endif

   if ((ec->netwm.type == E_WINDOW_TYPE_NORMAL) ||
       (ec->netwm.type == E_WINDOW_TYPE_UNKNOWN) ||
       (ec->netwm.type == E_WINDOW_TYPE_NOTIFICATION))
     {
        return EINA_TRUE;
     }

   return EINA_FALSE;

cancel_max:
   pc = eina_hash_find(hash_pol_clients, &ec);
   _pol_client_maximize_policy_cancel(pc);

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
_pol_client_maximize_policy_apply(Pol_Client *pc)
{
   E_Client *ec;

   if (pc->max_policy_state) return;
   if (pc->allow_user_geom) return;

   pc->max_policy_state = EINA_TRUE;

#undef _SET
# define _SET(a) pc->orig.a = pc->ec->a
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
_pol_client_maximize_policy_cancel(Pol_Client *pc)
{
   E_Client *ec;
   Eina_Bool changed = EINA_FALSE;

   if (!pc->max_policy_state) return;

   pc->max_policy_state = EINA_FALSE;

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
}

static void
_pol_cb_hook_client_new(void *d EINA_UNUSED, E_Client *ec)
{
   if (EINA_UNLIKELY(!ec))
     return;

   _pol_client_add(ec);
}

static void
_pol_cb_hook_client_eval_pre_new_client(void *d EINA_UNUSED, E_Client *ec)
{
   short ly;

   if (e_object_is_del(E_OBJECT(ec))) return;

   if (e_mod_pol_client_is_keyboard_sub(ec))
     {
        ec->placed = 1;
        ec->exp_iconify.skip_iconify = EINA_TRUE;

        EINA_SAFETY_ON_NULL_RETURN(ec->frame);
        if (ec->layer != E_LAYER_CLIENT_ABOVE)
          evas_object_layer_set(ec->frame, E_LAYER_CLIENT_ABOVE);
     }
   if (e_mod_pol_client_is_noti(ec))
     {
        if (ec->frame)
          {
             ly = evas_object_layer_get(ec->frame);
             ELOGF("NOTI", "         |ec->layer:%d object->layer:%d", ec->pixmap, ec, ec->layer, ly);
             if (ly != ec->layer)
               evas_object_layer_set(ec->frame, ec->layer);
          }
     }
}

static void
_pol_cb_hook_client_eval_pre_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   if (e_object_is_del(E_OBJECT(ec))) return;

   e_mod_pol_stack_hook_pre_fetch(ec);
}

static void
_pol_cb_hook_client_eval_pre_post_fetch(void *d EINA_UNUSED, E_Client *ec)
{
   if (e_object_is_del(E_OBJECT(ec))) return;

   e_mod_pol_stack_hook_pre_post_fetch(ec);
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_notification_level_fetch(ec);
   e_mod_pol_wl_eval_pre_post_fetch(ec);
#endif

}

static void
_pol_cb_hook_client_eval_post_fetch(void *d EINA_UNUSED, E_Client *ec)
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
        _pol_client_maximize_policy_cancel(pc);

        e_mod_pol_keyboard_layout_apply(ec);
     }

   if (!e_util_strcmp("wl_pointer-cursor", ec->icccm.window_role))
     {
        Pol_Client *pc;
        pc = eina_hash_find(hash_pol_clients, &ec);
        _pol_client_maximize_policy_cancel(pc);
        return;
     }

   if (e_mod_pol_client_is_noti(ec))
     e_client_util_move_without_frame(ec, 0, 0);

   if (!_pol_client_normal_check(ec)) return;

   pd = eina_hash_find(hash_pol_desks, &ec->desk);
   if (!pd) return;

   pc = eina_hash_find(hash_pol_clients, &ec);
   _pol_client_maximize_policy_apply(pc);
}

static void
_pol_cb_hook_client_desk_set(void *d EINA_UNUSED, E_Client *ec)
{
   Pol_Client *pc;
   Pol_Desk *pd;

   if (e_object_is_del(E_OBJECT(ec))) return;
   if (!_pol_client_normal_check(ec)) return;
   if (ec->internal) return;
   if (ec->new_client) return;

   pc = eina_hash_find(hash_pol_clients, &ec);
   if (EINA_UNLIKELY(!pc))
     return;

   pd = eina_hash_find(hash_pol_desks, &ec->desk);

   if (pd)
     _pol_client_maximize_policy_apply(pc);
   else
     _pol_client_maximize_policy_cancel(pc);
}

static void
_pol_cb_hook_client_eval_end(void *d EINA_UNUSED, E_Client *ec)
{
   /* calculate e_client visibility */
   e_mod_pol_visibility_calc();
}

static void
_pol_cb_hook_client_fullscreen_pre(void* data EINA_UNUSED, E_Client *ec)
{
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (!_pol_client_normal_check(ec)) return;
   if (ec->internal) return;

   ec->skip_fullscreen = 1;
}

static void
_pol_cb_hook_pixmap_del(void *data EINA_UNUSED, E_Pixmap *cp)
{
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_pixmap_del(cp);
#endif
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
                                            zone->num,
                                            zone->desks[i]->x,
                                            zone->desks[i]->y);
        if (d)
          e_mod_pol_desk_add(zone->desks[i]);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_zone_display_state_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Zone_Display_State_Change *ev;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

   e_mod_pol_zone_visibility_calc(ev->zone);

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
   EINA_SAFETY_ON_NULL_RETURN_VAL(ev, ECORE_CALLBACK_PASS_ON);

#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_client_del(ev->ec);
#endif
   e_mod_pol_stack_cb_client_remove(ev->ec);
   e_mod_pol_client_visibility_del(ev->ec);
   e_mod_pol_visibility_calc();

   pc = eina_hash_find(hash_pol_clients, &ev->ec);
   _pol_client_del(pc);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_client_add(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;

   ev = event;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ev, ECORE_CALLBACK_PASS_ON);

#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_client_add(ev->ec);
#endif

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_client_move(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;

   ev = event;
   if (!ev) return ECORE_CALLBACK_PASS_ON;

#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_position_send(ev->ec);
#endif
   e_mod_pol_visibility_calc();

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_client_resize(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   E_Event_Client *ev;
   E_Client *ec;
   int zh = 0;

   ev = (E_Event_Client *)event;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ev, ECORE_CALLBACK_PASS_ON);

   ec = ev->ec;
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec, ECORE_CALLBACK_PASS_ON);

   if (e_mod_pol_client_is_keyboard(ec))
     {
#ifdef HAVE_WAYLAND_ONLY
        e_mod_pol_wl_keyboard_geom_broadcast(ec);
#else
        ;
#endif
     }

   /* re-calculate window's position with changed size */
   if (e_mod_pol_client_is_volume_tv(ec))
     {
        e_zone_useful_geometry_get(ec->zone, NULL, NULL, NULL, &zh);
        evas_object_move(ec->frame, 0, (zh / 2) - (ec->h / 2));

        evas_object_pass_events_set(ec->frame, 1);
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
   if (!ev || (!ev->ec)) return ECORE_CALLBACK_PASS_ON;
   if (ev->property & E_CLIENT_PROPERTY_CLIENT_TYPE)
     {
        if (e_mod_pol_client_is_home_screen(ev->ec))
          {
             ev->ec->lock_client_stacking = 0;
             return ECORE_CALLBACK_PASS_ON;
          }
        else if (e_mod_pol_client_is_lock_screen(ev->ec))
          return ECORE_CALLBACK_PASS_ON;
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_pol_cb_client_vis_change(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_win_scrmode_apply();
#endif
   return ECORE_CALLBACK_PASS_ON;
}

void
e_mod_pol_allow_user_geometry_set(E_Client *ec, Eina_Bool set)
{
   Pol_Client *pc;

   if (EINA_UNLIKELY(!ec))
     return;

   pc = eina_hash_find(hash_pol_clients, &ec);
   if (EINA_UNLIKELY(!pc))
     return;

   pc->allow_user_geom = set;
}

void
e_mod_pol_desk_add(E_Desk *desk)
{
   Pol_Desk *pd;
   E_Client *ec;
   Pol_Softkey *softkey;
   Pol_Client *pc;

   pd = eina_hash_find(hash_pol_desks, &desk);
   if (pd) return;

   pd = E_NEW(Pol_Desk, 1);
   pd->desk = desk;
   pd->zone = desk->zone;

   eina_hash_add(hash_pol_desks, &desk, pd);

   /* add clients */
   E_CLIENT_FOREACH(ec)
     {
       if (pd->desk == ec->desk)
         {
            pc = eina_hash_find(hash_pol_clients, &ec);
            _pol_client_maximize_policy_apply(pc);
         }
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
        _pol_client_maximize_policy_cancel(pc);
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

   if (!e_util_strcmp(ec->icccm.title, "LOCKSCREEN"))
     return EINA_TRUE;

   if (!e_util_strcmp(ec->icccm.window_role, "lockscreen"))
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

   if (!e_util_strcmp(ec->icccm.window_role, "quickpanel"))
     return EINA_TRUE;

   return EINA_FALSE;
}

Eina_Bool
e_mod_pol_client_is_conformant(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec->comp_data, EINA_FALSE);

#ifdef HAVE_WAYLAND_ONLY
   E_Comp_Wl_Client_Data *cdata = (E_Comp_Wl_Client_Data *)ec->comp_data;
   if (cdata->conformant == 1)
     {
        return EINA_TRUE;
     }
#endif

   return EINA_FALSE;
}

Eina_Bool
e_mod_pol_client_is_volume(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   if (!e_util_strcmp(ec->netwm.name, "volume"))
     return EINA_TRUE;

   if (!e_util_strcmp(ec->icccm.title, "volume"))
     return EINA_TRUE;

   return EINA_FALSE;
}

Eina_Bool
e_mod_pol_client_is_volume_tv(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   if (!e_util_strcmp(ec->icccm.window_role, "tv-volume-popup"))
     return EINA_TRUE;

   return EINA_FALSE;
}

Eina_Bool
e_mod_pol_client_is_noti(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   if (!e_util_strcmp(ec->icccm.title, "noti_win"))
     return EINA_TRUE;

   if (ec->netwm.type == E_WINDOW_TYPE_NOTIFICATION)
     return EINA_TRUE;

   return EINA_FALSE;
}

#ifdef HAVE_WAYLAND_ONLY
Eina_Bool
e_mod_pol_client_is_subsurface(E_Client *ec)
{
   E_Comp_Wl_Client_Data *cd;

   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   cd = (E_Comp_Wl_Client_Data *)ec->comp_data;
   if (cd && cd->sub.data)
     return EINA_TRUE;

   return EINA_FALSE;
}
#endif

static Eina_Bool
_pol_cb_module_defer_job(void *data EINA_UNUSED)
{
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_defer_job();
#endif
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

#undef E_PIXMAP_HOOK_APPEND
#define E_PIXMAP_HOOK_APPEND(l, t, cb, d) \
  do                                      \
    {                                     \
       E_Pixmap_Hook *_h;                 \
       _h = e_pixmap_hook_add(t, cb, d);  \
       assert(_h);                        \
       l = eina_list_append(l, _h);       \
    }                                     \
  while (0)

E_API void *
e_modapi_init(E_Module *m)
{
   Mod *mod;
   E_Zone *zone;
   Config_Desk *d;
   const Eina_List *l;
   int i, n;
   //char buf[PATH_MAX];

   mod = E_NEW(Mod, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(mod, NULL);

   mod->module = m;
   _pol_mod = mod;

   hash_pol_clients = eina_hash_pointer_new(_pol_cb_client_data_free);
   hash_pol_desks = eina_hash_pointer_new(_pol_cb_desk_data_free);

   e_mod_pol_stack_init();
   e_mod_pol_visibility_init();
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_init();
   e_mod_pol_wl_aux_hint_init();
#endif

   /* initialize configure and config data type */
#if 0
   snprintf(buf, sizeof(buf), "%s/e-module-policy.edj",
            e_module_dir_get(m));
   e_configure_registry_category_add("windows", 50, _("Windows"), NULL,
                                     "preferences-system-windows");
   e_configure_registry_item_add("windows/policy-tizen", 150,
                                 _("Tizen Policy"), NULL, buf,
                                 e_int_config_pol_mobile);
#endif

   e_mod_pol_conf_init(mod);

   EINA_LIST_FOREACH(e_comp->zones, l, zone)
     {
        n = zone->desk_y_count * zone->desk_x_count;
        for (i = 0; i < n; i++)
          {
             d = e_mod_pol_conf_desk_get_by_nums(_pol_mod->conf,
                                                 zone->num,
                                                 zone->desks[i]->x,
                                                 zone->desks[i]->y);
             if (d)
               e_mod_pol_desk_add(zone->desks[i]);
          }
     }

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_ADD,                  _pol_cb_zone_add,                        NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_DEL,                  _pol_cb_zone_del,                        NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_MOVE_RESIZE,          _pol_cb_zone_move_resize,                NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_DESK_COUNT_SET,       _pol_cb_zone_desk_count_set,             NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_ZONE_DISPLAY_STATE_CHANGE, _pol_cb_zone_display_state_change,       NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_DESK_SHOW,                 _pol_cb_desk_show,                       NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_REMOVE,             _pol_cb_client_remove,                   NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_ADD,                _pol_cb_client_add,                      NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_MOVE,               _pol_cb_client_move,                     NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_RESIZE,             _pol_cb_client_resize,                   NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_STACK,              _pol_cb_client_stack,                    NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_PROPERTY,           _pol_cb_client_property,                 NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_CLIENT_VISIBILITY_CHANGE,  _pol_cb_client_vis_change,               NULL);
   E_LIST_HANDLER_APPEND(handlers, E_EVENT_MODULE_DEFER_JOB,          _pol_cb_module_defer_job,                NULL);

   E_CLIENT_HOOK_APPEND(hooks_ec,  E_CLIENT_HOOK_NEW_CLIENT,          _pol_cb_hook_client_new,                 NULL);
   E_CLIENT_HOOK_APPEND(hooks_ec,  E_CLIENT_HOOK_EVAL_PRE_NEW_CLIENT, _pol_cb_hook_client_eval_pre_new_client, NULL);
   E_CLIENT_HOOK_APPEND(hooks_ec,  E_CLIENT_HOOK_EVAL_PRE_FETCH,      _pol_cb_hook_client_eval_pre_fetch,      NULL);
   E_CLIENT_HOOK_APPEND(hooks_ec,  E_CLIENT_HOOK_EVAL_PRE_POST_FETCH, _pol_cb_hook_client_eval_pre_post_fetch, NULL);
   E_CLIENT_HOOK_APPEND(hooks_ec,  E_CLIENT_HOOK_EVAL_POST_FETCH,     _pol_cb_hook_client_eval_post_fetch,     NULL);
   E_CLIENT_HOOK_APPEND(hooks_ec,  E_CLIENT_HOOK_DESK_SET,            _pol_cb_hook_client_desk_set,            NULL);
   E_CLIENT_HOOK_APPEND(hooks_ec,  E_CLIENT_HOOK_EVAL_END,            _pol_cb_hook_client_eval_end,            NULL);
   E_CLIENT_HOOK_APPEND(hooks_ec,  E_CLIENT_HOOK_FULLSCREEN_PRE,      _pol_cb_hook_client_fullscreen_pre,      NULL);

   E_PIXMAP_HOOK_APPEND(hooks_cp,  E_PIXMAP_HOOK_DEL,                 _pol_cb_hook_pixmap_del,                 NULL);

   e_mod_pol_rotation_init();

   return mod;
}

E_API int
e_modapi_shutdown(E_Module *m)
{
   Mod *mod = m->data;
   Eina_Inlist *l;
   Pol_Softkey *softkey;

   eina_list_free(mod->launchers);
   EINA_INLIST_FOREACH_SAFE(mod->softkeys, l, softkey)
     e_mod_pol_softkey_del(softkey);
   E_FREE_LIST(hooks_cp, e_pixmap_hook_del);
   E_FREE_LIST(hooks_ec, e_client_hook_del);
   E_FREE_LIST(handlers, ecore_event_handler_del);

   E_FREE_FUNC(hash_pol_desks, eina_hash_free);
   E_FREE_FUNC(hash_pol_clients, eina_hash_free);

   e_mod_pol_stack_shutdonw();
   e_mod_pol_visibility_shutdown();
   e_mod_pol_rotation_shutdown();
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_shutdown();
#endif

#if 0
   e_configure_registry_item_del("windows/policy-tizen");
   e_configure_registry_category_del("windows");

   if (mod->conf_dialog)
     {
        e_object_del(E_OBJECT(mod->conf_dialog));
        mod->conf_dialog = NULL;
     }
#endif

   e_mod_pol_conf_shutdown(mod);

   E_FREE(mod);

   _pol_mod = NULL;

   return 1;
}

E_API int
e_modapi_save(E_Module *m)
{
   Mod *mod = m->data;
   e_config_domain_save("module.policy-tizen",
                        mod->conf_edd,
                        mod->conf);
   return 1;
}
