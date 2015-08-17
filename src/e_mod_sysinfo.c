#include "e_mod_main.h"
#include "e_mod_sysinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>

#define WIN_WIDTH    500
#define WIN_HEIGHT   1080
#define SCR_WIDTH    1920
#define SCR_HEIGHT   1080
#define SCR_MARGIN_W 30

typedef struct _E_Sysinfo
{
   Eina_Bool    show;
   E_Client    *ec;
   Evas_Object *btn;
   Evas_Object *bg;

   /* FPS */
   Evas_Object *fps;
   Evas_Object *fps_text;
   Ecore_Animator *fps_anim;
   double       fps_src;
   double       fps_target;
   double       fps_curr;
   double       fps_start_t;
   Eina_Bool    fps_update;
   Ecore_Timer *fps_timer;

   struct
   {
      Elm_Transit *trans;
   } effect;

   struct
   {
      double noti_factor;
      double noti_factor_src;
      double noti_factor_target;
      double normal_factor;
      double normal_factor_src;
      double normal_factor_target;
      Eina_List *noti_list; /* list of E_Client */
      Eina_List *normal_list; /* list of E_Client */
   } zoom;

} E_Sysinfo;

static E_Sysinfo *e_sysinfo = NULL;
static Eina_List *handlers = NULL;

static void
_win_map_apply(E_Client *ec, double factor)
{
   Evas_Map *map;

   if (!ec->frame) return;

   map = evas_map_new(4);
   evas_map_util_points_populate_from_geometry(map, ec->client.x, ec->client.y, ec->client.w, ec->client.h, 0);
   evas_map_util_zoom(map, factor, factor, SCR_WIDTH - 500, SCR_HEIGHT/2);
   evas_map_util_object_move_sync_set(map, EINA_TRUE);
   evas_object_map_set(ec->frame, map);
   evas_object_map_enable_set(ec->frame, EINA_TRUE);
   evas_map_free(map);
}

static void
_win_effect_zoom(Eina_Bool show, double progress)
{
   Eina_List *l;
   double col, zoom, factor;
   Evas_Map *map;
   E_Client *ec;

   factor = 0.4f;
   if (show) zoom = 1.0 - (factor * progress);
   else zoom = (1.0 - factor) + (factor * progress);

   EINA_LIST_FOREACH(e_sysinfo->zoom.normal_list, l, ec)
     {
        if (e_object_is_del(E_OBJECT(ec))) continue;
        _win_map_apply(ec, zoom);
     }

   factor = 0.25f;
   if (show) zoom = 1.0 - (factor * progress);
   else zoom = (1.0 - factor) + (factor * progress);

   EINA_LIST_FOREACH(e_sysinfo->zoom.noti_list, l, ec)
     {
        if (e_object_is_del(E_OBJECT(ec))) continue;
        _win_map_apply(ec, zoom);
     }

   factor = 0.1f;
   if (show)
     {
        col = 255 - (120 * progress);
        zoom = 1.0 - (factor * progress);
     }
   else
     {
        col = 135 + (120 * progress);
        zoom = (1.0 - factor) + (factor * progress);
     }

   evas_object_color_set(e_sysinfo->bg, col, col, col, 255);

   map = evas_map_new(4);
   evas_map_util_points_populate_from_geometry(map, 0, 0, SCR_WIDTH, SCR_HEIGHT, 0);
   evas_map_util_zoom(map, zoom, zoom, SCR_WIDTH/2, SCR_HEIGHT/2);
   evas_object_map_set(e_sysinfo->bg, map);
   evas_object_map_enable_set(e_sysinfo->bg, EINA_TRUE);
   evas_map_free(map);
}

static void
_win_effect_cb_trans(Elm_Transit_Effect *eff EINA_UNUSED, Elm_Transit *trans EINA_UNUSED, double progress)
{
   E_Client *ec;
   double curr, col, fps_y;

   ec = e_sysinfo->ec;
   EINA_SAFETY_ON_NULL_RETURN(ec);
   if (e_object_is_del(E_OBJECT(ec))) return;
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   if (progress < 0.0) progress = 0.0;

   if (e_sysinfo->show)
     {
        curr = -WIN_WIDTH + (WIN_WIDTH * progress);
        col = 255 * progress;
        if (col <= 0) col = 0;

        fps_y = 10 + (60 * progress);
     }
   else
     {
        curr = -(WIN_WIDTH * progress);
        col = 255 - (255 * progress);
        if (col <= 0) col = 0;

        fps_y = 70 - (60 * progress);
     }

   evas_object_color_set(ec->frame, col, col, col, col);
   evas_object_move(ec->frame, curr, 0);

   evas_object_color_set(e_sysinfo->fps, col, col, col, col);
   evas_object_move(e_sysinfo->fps, 155, fps_y);

   _win_effect_zoom(e_sysinfo->show, progress);
}

static void
_win_effect_cb_trans_end(Elm_Transit_Effect *eff EINA_UNUSED, Elm_Transit *trans EINA_UNUSED)
{
   ELOGF("SYSINFO", "EFF END  |t:0x%08x", NULL, NULL, (unsigned int)e_sysinfo->effect.trans);
   if (e_sysinfo->zoom.noti_list) eina_list_free(e_sysinfo->zoom.noti_list);
   if (e_sysinfo->zoom.normal_list) eina_list_free(e_sysinfo->zoom.normal_list);
   e_sysinfo->zoom.noti_list = NULL;
   e_sysinfo->zoom.normal_list = NULL;
   e_sysinfo->zoom.noti_factor = e_sysinfo->zoom.noti_factor_target;
   e_sysinfo->zoom.normal_factor = e_sysinfo->zoom.normal_factor_target;
   e_sysinfo->effect.trans = NULL;
}

static void
_win_effect_cb_trans_del(void *data EINA_UNUSED, Elm_Transit *transit EINA_UNUSED)
{
   ELOGF("SYSINFO", "EFF DEL  |t:0x%08x", NULL, NULL, (unsigned int)e_sysinfo->effect.trans);
   if (e_sysinfo->zoom.noti_list) eina_list_free(e_sysinfo->zoom.noti_list);
   if (e_sysinfo->zoom.normal_list) eina_list_free(e_sysinfo->zoom.normal_list);
   e_sysinfo->zoom.noti_list = NULL;
   e_sysinfo->zoom.normal_list = NULL;
   e_sysinfo->zoom.noti_factor = e_sysinfo->zoom.noti_factor_target;
   e_sysinfo->zoom.normal_factor = e_sysinfo->zoom.normal_factor_target;
   e_sysinfo->effect.trans = NULL;
}

static void
_win_effect_init(void)
{
   E_Client *ec;
   Evas_Object *o;
   Eina_Tiler *t;
   Eina_Rectangle r, *_r;
   Eina_Iterator *it;
   Eina_Bool canvas_vis = EINA_TRUE;
   Eina_Bool ec_vis, ec_opaque;

   if (e_sysinfo->effect.trans)
     {
        elm_transit_del_cb_set(e_sysinfo->effect.trans, NULL, NULL);
        elm_transit_del(e_sysinfo->effect.trans);
     }

   e_sysinfo->effect.trans = elm_transit_add();
   elm_transit_del_cb_set(e_sysinfo->effect.trans, _win_effect_cb_trans_del, NULL);

   elm_transit_effect_add(e_sysinfo->effect.trans,
                          _win_effect_cb_trans,
                          NULL,
                          _win_effect_cb_trans_end);

   elm_transit_smooth_set(e_sysinfo->effect.trans, EINA_FALSE);
   elm_transit_tween_mode_set(e_sysinfo->effect.trans, ELM_TRANSIT_TWEEN_MODE_DECELERATE);
   elm_transit_objects_final_state_keep_set(e_sysinfo->effect.trans, EINA_FALSE);
   elm_transit_duration_set(e_sysinfo->effect.trans, 0.8f);

   // add E_Client
   if (e_sysinfo->zoom.noti_list) eina_list_free(e_sysinfo->zoom.noti_list);
   if (e_sysinfo->zoom.normal_list) eina_list_free(e_sysinfo->zoom.normal_list);

   t = eina_tiler_new(SCR_WIDTH, SCR_HEIGHT);
   eina_tiler_tile_size_set(t, 1, 1);

   EINA_RECTANGLE_SET(&r, 0, 0, SCR_WIDTH, SCR_HEIGHT);
   eina_tiler_rect_add(t, &r);

   o = evas_object_top_get(e_comp->evas);
   for (; o; o = evas_object_below_get(o))
     {
        ec = evas_object_data_get(o, "E_Client");

        if (!ec) continue;
        if (e_object_is_del(E_OBJECT(ec))) continue;
        if (e_client_util_ignored_get(ec)) continue;
        if ((e_sysinfo->ec) && (e_sysinfo->ec == ec)) continue;

        ec_vis = ec_opaque = EINA_FALSE;

        /* check visible state */
        if ((!ec->visible) ||
            (ec->iconic) ||
            (!evas_object_visible_get(ec->frame)))
          {
             ; /* do nothing */
          }
        else
          {
             it = eina_tiler_iterator_new(t);
             EINA_ITERATOR_FOREACH(it, _r)
               {
                  if (E_INTERSECTS(ec->x, ec->y, ec->w, ec->h,
                                   _r->x, _r->y, _r->w, _r->h))
                    {
                       ec_vis = EINA_TRUE;
                       break;
                    }
               }
             eina_iterator_free(it);
          }

        if (ec_vis)
          {
             if ((ec->visibility.opaque > 0) && (ec->argb))
               ec_opaque = EINA_TRUE;

             if ((!ec->argb) || (ec_opaque))
               {
                  EINA_RECTANGLE_SET(&r, ec->x, ec->y, ec->w, ec->h);
                  eina_tiler_rect_del(t, &r);

                  if (eina_tiler_empty(t))
                    canvas_vis = EINA_FALSE;
               }

             if (!e_util_strcmp("e_demo", ec->icccm.window_role))
               e_sysinfo->zoom.noti_list = eina_list_append(e_sysinfo->zoom.noti_list, ec);
             else
               e_sysinfo->zoom.normal_list = eina_list_append(e_sysinfo->zoom.normal_list, ec);
          }

        if (!canvas_vis) break;
     }
   eina_tiler_free(t);
}

static void
_win_show(void)
{
   Evas_Object *o;

   if ((!e_sysinfo->ec) || (!e_sysinfo->ec->frame)) return;

   e_comp->calc_fps = EINA_TRUE;

   _win_effect_init();
   EINA_SAFETY_ON_NULL_RETURN(e_sysinfo->effect.trans);

   ELOGF("SYSINFO", "EFF SHOW |t:0x%08x", NULL, NULL, (unsigned int)e_sysinfo->effect.trans);

   if (!evas_object_visible_get(e_sysinfo->fps))
     {
        evas_object_color_set(e_sysinfo->fps, 0, 0, 0, 0);
        evas_object_show(e_sysinfo->fps);
     }

   if (!e_sysinfo->bg)
     {
        o = edje_object_add(e_comp->evas);
        edje_object_file_set(o, "/usr/share/enlightenment/data/backgrounds/Wetleaf.edj",
                             "e/desktop/background");
        if (edje_object_data_get(o, "noanimation"))
          edje_object_animation_set(o, EINA_FALSE);
        evas_object_move(o, 0, 0);
        evas_object_resize(o, SCR_WIDTH, SCR_HEIGHT);
        evas_object_layer_set(o, E_LAYER_BG);
        evas_object_show(o);
        e_sysinfo->bg = o;
     }

   e_sysinfo->zoom.noti_factor = 1.0;
   e_sysinfo->zoom.noti_factor_src = 1.0;
   e_sysinfo->zoom.noti_factor_target = 0.75;

   e_sysinfo->zoom.normal_factor = 1.0;
   e_sysinfo->zoom.normal_factor_src = 1.0;
   e_sysinfo->zoom.normal_factor_target = 0.6;

   elm_transit_go(e_sysinfo->effect.trans);
}

static void
_win_hide(void)
{
   if ((!e_sysinfo->ec) || (!e_sysinfo->ec->frame)) return;

   e_comp->calc_fps = EINA_FALSE;

   _win_effect_init();
   EINA_SAFETY_ON_NULL_RETURN(e_sysinfo->effect.trans);

   ELOGF("SYSINFO", "EFF HIDE |t:0x%08x", NULL, NULL, (unsigned int)e_sysinfo->effect.trans);

   e_sysinfo->zoom.noti_factor = 0.75;
   e_sysinfo->zoom.noti_factor_src = 0.75;
   e_sysinfo->zoom.noti_factor_target = 1.0;

   e_sysinfo->zoom.normal_factor = 0.6;
   e_sysinfo->zoom.normal_factor_src = 0.6;
   e_sysinfo->zoom.normal_factor_target = 1.0;

   elm_transit_go(e_sysinfo->effect.trans);
}

static void
_btn_cb_mouse_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   if (e_sysinfo->show) _win_hide();
   else _win_show();

   e_sysinfo->show = !e_sysinfo->show;
}

void
e_mod_pol_sysinfo_client_add(E_Client *ec)
{
   ELOGF("SYSINFO",
         "ADD      |%dx%d frame:0x%08x vis:%d",
         ec->pixmap, ec, ec->w, ec->h,
         (unsigned int)ec->frame,
         ec->frame ? evas_object_visible_get(ec->frame) : 0);

   if (e_mod_pol_client_is_sysinfo(ec))
     {
        e_sysinfo->ec = ec;
        if (e_sysinfo->show) _win_show();
     }
}

void
e_mod_pol_sysinfo_client_del(E_Client *ec)
{
   ELOGF("SYSINFO",
         "DEL      |%dx%d frame:0x%08x vis:%d",
         ec->pixmap, ec, ec->w, ec->h,
         (unsigned int)ec->frame,
         ec->frame ? evas_object_visible_get(ec->frame) : 0);

   if (e_sysinfo->ec == ec)
     {
        e_sysinfo->ec = NULL;
     }
   else
     {
        if (eina_list_data_find(e_sysinfo->zoom.normal_list, ec))
          e_sysinfo->zoom.normal_list = eina_list_remove(e_sysinfo->zoom.normal_list, ec);
        else if (eina_list_data_find(e_sysinfo->zoom.noti_list, ec))
          e_sysinfo->zoom.noti_list = eina_list_remove(e_sysinfo->zoom.noti_list, ec);
     }
}

void
e_mod_pol_sysinfo_client_resize(E_Client *ec)
{
   ELOGF("SYSINFO",
         "REZ      |%dx%d frame:0x%08x vis:%d",
         ec->pixmap, ec, ec->w, ec->h,
         (unsigned int)ec->frame,
         ec->frame ? evas_object_visible_get(ec->frame) : 0);

   if (!e_mod_pol_client_is_sysinfo(ec) &&
       (e_sysinfo->zoom.normal_factor < 1.0) &&
       (ec->w >= 300) && (ec->h >= 300) &&
       (ec->visible) &&
       (ec->frame) &&
       evas_object_visible_get(ec->frame))
     {
        if (!evas_object_data_get(ec->frame, "client_mapped"))
          {
             if (!e_util_strcmp("e_demo", ec->icccm.window_role))
               _win_map_apply(ec, e_sysinfo->zoom.noti_factor);
             else
               _win_map_apply(ec, e_sysinfo->zoom.normal_factor);
             evas_object_data_set(ec->frame, "client_mapped", (void*)1);
          }
     }
}

static Eina_Bool
_sysinfo_cb_fps_clear(void *data EINA_UNUSED)
{
   e_sysinfo->fps_src = e_sysinfo->fps_curr;
   e_sysinfo->fps_target = 0.0f;
   e_sysinfo->fps_start_t = ecore_time_get();
   e_sysinfo->fps_update = EINA_TRUE;
   e_sysinfo->fps_timer = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_sysinfo_cb_fps_anim(void *data EINA_UNUSED)
{
   double curr_t, dt;
   char buf[256];

   if (!e_sysinfo->fps_text) return ECORE_CALLBACK_RENEW;
   if (!e_sysinfo->fps_update) return ECORE_CALLBACK_RENEW;

   curr_t = ecore_time_get();
   dt = curr_t - e_sysinfo->fps_start_t;

   if (dt == 0.0f) return ECORE_CALLBACK_RENEW;

   if (dt > 1.0f)
     {
        e_sysinfo->fps_curr = e_sysinfo->fps_target;
        e_sysinfo->fps_update = EINA_FALSE;
     }
   else
     e_sysinfo->fps_curr = e_sysinfo->fps_src + ((e_sysinfo->fps_target - e_sysinfo->fps_src) * (dt));

   sprintf(buf, "%.1f", e_sysinfo->fps_curr);
   evas_object_text_text_set(e_sysinfo->fps_text, buf);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_sysinfo_cb_comp_fps_update(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   ELOGF("SYSINFO", "FPS      |%.2f", NULL, NULL, e_comp->fps);

   if (!e_sysinfo->fps_anim)
     e_sysinfo->fps_anim = ecore_animator_add(_sysinfo_cb_fps_anim, NULL);

   if (e_sysinfo->fps_timer)
     ecore_timer_del(e_sysinfo->fps_timer);
   e_sysinfo->fps_timer = ecore_timer_add(3.0f, _sysinfo_cb_fps_clear, NULL);

   e_sysinfo->fps_src = e_sysinfo->fps_curr;
   e_sysinfo->fps_target = e_comp->fps;
   e_sysinfo->fps_start_t = ecore_time_get();
   e_sysinfo->fps_update = EINA_TRUE;

   return ECORE_CALLBACK_PASS_ON;
}

Eina_Bool
e_mod_pol_sysinfo_init(void)
{
   Evas_Object *o, *comp_obj;

   e_sysinfo = E_NEW(E_Sysinfo, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e_sysinfo, EINA_FALSE);

   o = evas_object_text_add(e_comp->evas);
   evas_object_text_font_set(o, "TizenSans", 75);
   evas_object_size_hint_align_set(o, EVAS_HINT_FILL, 0.0);
   evas_object_text_text_set(o, "0.0");
   evas_object_move(o, 200, 200);
   comp_obj = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(comp_obj, E_LAYER_POPUP);
   e_sysinfo->fps_text = o;
   e_sysinfo->fps = comp_obj;
   e_sysinfo->fps_src = 0.0f;
   e_sysinfo->fps_target = 0.0f;
   e_sysinfo->fps_curr = 0.0f;

   o = evas_object_rectangle_add(e_comp->evas);
   evas_object_color_set(o, 0, 0, 0, 0);
   evas_object_resize(o, 64, 64);
   evas_object_move(o, 0, 0);
   comp_obj = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(comp_obj, E_LAYER_POPUP);
   evas_object_event_callback_add(comp_obj, EVAS_CALLBACK_MOUSE_UP, _btn_cb_mouse_up, NULL);
   evas_object_show(comp_obj);
   e_sysinfo->btn = comp_obj;

   E_LIST_HANDLER_APPEND(handlers, E_EVENT_COMPOSITOR_FPS_UPDATE, _sysinfo_cb_comp_fps_update, NULL);

   return EINA_TRUE;
}

void
e_mod_pol_sysinfo_shutdown(void)
{
   E_FREE_LIST(handlers, ecore_event_handler_del);

   if (!e_sysinfo) return;

   if (e_sysinfo->fps_anim)
     ecore_animator_del(e_sysinfo->fps_anim);

   if (e_sysinfo->effect.trans)
     {
        elm_transit_del_cb_set(e_sysinfo->effect.trans, NULL, NULL);
        elm_transit_del(e_sysinfo->effect.trans);
        e_sysinfo->effect.trans = NULL;
     }

   evas_object_del(e_sysinfo->bg);
   evas_object_del(e_sysinfo->fps);
   evas_object_del(e_sysinfo->btn);

   E_FREE(e_sysinfo);
}
