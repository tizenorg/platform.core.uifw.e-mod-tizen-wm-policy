#include "e_mod_private_data.h"
#include "e_mod_main.h"
#include "e_mod_quickpanel.h"
#include "e_mod_indicator.h"
#include "e_mod_gesture.h"
#include "e_mod_transit.h"
#include "e_mod_rotation.h"

#define SMART_NAME            "quickpanel_object"
#define INTERNAL_ENTRY                    \
   Mover_Data *md;                        \
   md = evas_object_smart_data_get(obj);

#define QP_SHOW(EC)              \
do                               \
{                                \
   EC->visible = EINA_TRUE;      \
   evas_object_show(EC->frame);  \
} while (0)

#define QP_HIDE(EC)              \
do                               \
{                                \
   EC->visible = EINA_FALSE;     \
   evas_object_hide(EC->frame);  \
} while (0)

#define QP_VISIBLE_SET(EC, VIS)  \
do                               \
{                                \
   if (VIS) QP_SHOW(EC);         \
   else     QP_HIDE(EC);         \
} while(0)

typedef struct _Pol_Quickpanel Pol_Quickpanel;
typedef struct _Mover_Data Mover_Data;
typedef struct _Handler_Data Handler_Data;
typedef struct _Mover_Effect_Data Mover_Effect_Data;

struct _Pol_Quickpanel
{
   E_Client *ec;
   Evas_Object *mover;

   struct
   {
      Evas_Object *obj;
      Eina_Rectangle rect;
   } handler;

   Eina_Bool show_block;
};

struct _Mover_Data
{
   Pol_Quickpanel *qp;

   Evas_Object *smart_obj; //smart object
   Evas_Object *qp_layout_obj; // quickpanel's e_layout_object
   Evas_Object *handler_mirror_obj; // quickpanel handler mirror object
   Evas_Object *base_clip; // clipper for quickapnel base object
   Evas_Object *handler_clip; // clipper for quickpanel handler object

   struct
   {
      Handler_Data *data;
      Eina_Rectangle rect;
   } handler;

   struct
   {
      Pol_Transit *transit;
      Mover_Effect_Data *data;
      int y;
      unsigned int timestamp;
      float accel;
      Eina_Bool visible;
   } effect_info;
};

struct _Handler_Data
{
   Pol_Quickpanel *qp;
   Pol_Gesture *gesture;
   Evas_Object *handler;
   Evas_Object *mover;
};

struct _Mover_Effect_Data
{
   Pol_Transit *transit;
   Evas_Object *mover;
   int from_dy;
   int to_dy;
   Eina_Bool visible : 1;
};

static Pol_Quickpanel *_pol_quickpanel = NULL;
static Eina_List *_quickpanel_events = NULL;
static Eina_List *_quickpanel_hooks = NULL;
static Evas_Smart *_mover_smart = NULL;

static Pol_Quickpanel *
_quickpanel_find(E_Client *ec)
{
   if (!_pol_quickpanel)
     return NULL;

   if (_pol_quickpanel->ec == ec)
     return _pol_quickpanel;

   return NULL;
}

static void
_mover_intercept_show(void *data, Evas_Object *obj)
{
   Mover_Data *md = data;
   E_Client *ec = md->qp->ec;
   Evas *e;

   md->qp->show_block = EINA_FALSE;

   e = evas_object_evas_get(obj);

   QP_SHOW(ec);

   /* force update */
   e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
   e_comp_object_dirty(ec->frame);
   e_comp_object_render(ec->frame);

   // create base_clip
   md->base_clip = evas_object_rectangle_add(e);
   e_layout_pack(md->qp_layout_obj, md->base_clip);
   e_layout_child_move(md->base_clip, 0, 0);
   e_layout_child_resize(md->base_clip, ec->w, ec->h);
   evas_object_color_set(md->base_clip, 255, 255, 255, 255);
   evas_object_show(md->base_clip);
   evas_object_clip_set(ec->frame, md->base_clip);

   // create handler_mirror_obj
   md->handler_mirror_obj =  e_comp_object_util_mirror_add(ec->frame);
   e_layout_pack(md->qp_layout_obj, md->handler_mirror_obj);
   e_layout_child_move(md->handler_mirror_obj, md->handler.rect.x, md->handler.rect.y);
   e_layout_child_resize(md->handler_mirror_obj, ec->w, ec->h);
   evas_object_show(md->handler_mirror_obj);

   // create handler_clip
   md->handler_clip = evas_object_rectangle_add(e);
   e_layout_pack(md->qp_layout_obj, md->handler_clip);
   e_layout_child_move(md->handler_clip, md->handler.rect.x, md->handler.rect.y);
   e_layout_child_resize(md->handler_clip, md->handler.rect.w, md->handler.rect.h);
   evas_object_color_set(md->handler_clip, 255, 255, 255, 255);
   evas_object_show(md->handler_clip);
   evas_object_clip_set(md->handler_mirror_obj, md->handler_clip);

   evas_object_show(obj);
}

static void
_mover_smart_add(Evas_Object *obj)
{
   Mover_Data *md;

   md = E_NEW(Mover_Data, 1);
   if (EINA_UNLIKELY(!md))
     return;

   md->smart_obj = obj;
   md->qp_layout_obj = e_layout_add(evas_object_evas_get(obj));
   evas_object_color_set(md->qp_layout_obj, 255, 255, 255, 255);
   evas_object_smart_member_add(md->qp_layout_obj, md->smart_obj);

   evas_object_smart_data_set(obj, md);

   evas_object_move(obj, -1 , -1);
   evas_object_layer_set(obj, EVAS_LAYER_MAX - 1); // EVAS_LAYER_MAX :L cursor layer
   evas_object_intercept_show_callback_add(obj, _mover_intercept_show, md);
}

static void
_mover_smart_del(Evas_Object *obj)
{
   E_Client *ec;

   INTERNAL_ENTRY;

   ec = md->qp->ec;
   if (md->base_clip)
     {
        evas_object_clip_unset(md->base_clip);
        e_layout_unpack(md->base_clip);
        evas_object_del(md->base_clip);
     }
   if (md->handler_clip)
     {
        evas_object_clip_unset(md->handler_clip);
        e_layout_unpack(md->handler_clip);
        evas_object_del(md->handler_clip);
     }
   if (md->handler_mirror_obj)
     {
        e_layout_unpack(md->handler_mirror_obj);
        evas_object_del(md->handler_mirror_obj);
     }

   if (md->qp_layout_obj) evas_object_del(md->qp_layout_obj);

   evas_object_color_set(ec->frame, ec->netwm.opacity, ec->netwm.opacity, ec->netwm.opacity, ec->netwm.opacity);

   md->qp->mover = NULL;

   e_zone_rotation_block_set(md->qp->ec->zone, "quickpanel-mover", EINA_FALSE);

   free(md);
}

static void
_mover_smart_show(Evas_Object *obj)
{
   INTERNAL_ENTRY;

   evas_object_show(md->qp_layout_obj);
}

static void
_mover_smart_hide(Evas_Object *obj)
{
   INTERNAL_ENTRY;

   evas_object_hide(md->qp_layout_obj);
}

static void
_mover_smart_move(Evas_Object *obj, int x, int y)
{
   INTERNAL_ENTRY;

   evas_object_move(md->qp_layout_obj, x, y);
}

static void
_mover_smart_resize(Evas_Object *obj, int w, int h)
{
   INTERNAL_ENTRY;

   e_layout_virtual_size_set(md->qp_layout_obj, w, h);
   evas_object_resize(md->qp_layout_obj, w, h);
}

static void
_mover_smart_init(void)
{
   if (_mover_smart) return;
   {
      static const Evas_Smart_Class sc =
      {
         SMART_NAME,
         EVAS_SMART_CLASS_VERSION,
         _mover_smart_add,
         _mover_smart_del,
         _mover_smart_move,
         _mover_smart_resize,
         _mover_smart_show,
         _mover_smart_hide,
         NULL, /* color_set */
         NULL, /* clip_set */
         NULL, /* clip_unset */
         NULL, /* calculate */
         NULL, /* member_add */
         NULL, /* member_del */

         NULL, /* parent */
         NULL, /* callbacks */
         NULL, /* interfaces */
         NULL  /* data */
      };
      _mover_smart = evas_smart_class_new(&sc);
   }
}

static Eina_Bool
_mover_obj_handler_move(Mover_Data *md, int x, int y)
{
   // angle 0 case
   // do not move handler out of screen.
   if ((y + md->handler.rect.h) > md->qp->ec->zone->h) return EINA_FALSE;

   // angle 0 case
   md->handler.rect.y = y;

   e_layout_child_resize(md->base_clip, md->qp->ec->w, md->handler.rect.y); // base clip resize

   e_layout_child_move(md->handler_mirror_obj, md->handler.rect.x, md->handler.rect.y - md->qp->ec->h + md->handler.rect.h); // handler mirror object move
   e_layout_child_move(md->handler_clip, md->handler.rect.x, md->handler.rect.y); // handler mirror object move

   return EINA_TRUE;
}

static Evas_Object *
_mover_obj_new(Pol_Quickpanel *qp)
{
   Evas_Object *mover;
   Mover_Data *md;

   if (!qp->ec) return NULL;
   if (!qp->ec->frame) return NULL;

   if (qp->mover)
     return qp->mover;

   _mover_smart_init();
   mover = evas_object_smart_add(evas_object_evas_get(qp->ec->frame), _mover_smart);

   md = evas_object_smart_data_get(mover);
   md->qp = qp;
   md->handler.rect.w = qp->handler.rect.w;
   md->handler.rect.h = qp->handler.rect.h;

   evas_object_move(mover, 0, 0); // 0 angle case
   evas_object_resize(mover, qp->ec->w, qp->ec->h);
   evas_object_show(mover);

   qp->mover = mover;

   e_zone_rotation_block_set(qp->ec->zone, "quickpanel-mover", EINA_TRUE);

   return mover;
}

static Evas_Object *
_mover_obj_new_with_move(Pol_Quickpanel *qp, int x, int y, unsigned int timestamp)
{
   Evas_Object *mover;
   Mover_Data *md;

   mover = _mover_obj_new(qp);
   if (!mover)
     return NULL;

   md = evas_object_smart_data_get(mover);

   md->effect_info.y = y;
   md->effect_info.timestamp = timestamp;

   _mover_obj_handler_move(md, x, y);

   return mover;
}

static Eina_Bool
_mover_obj_move(Evas_Object *mover, int x, int y, unsigned int timestamp)
{
   Mover_Data *md;
   int dy;
   unsigned int dt;

   if (!mover) return EINA_FALSE;

   md = evas_object_smart_data_get(mover);
   if (!_mover_obj_handler_move(md, x, y)) return EINA_FALSE;

   /* Calculate the acceleration of movement,
    * determine the visibility of quickpanel based on the result. */
   dy = y - md->effect_info.y;
   dt = timestamp - md->effect_info.timestamp;
   if (dt) md->effect_info.accel = (float)dy / (float)dt;

   /* Store current information to next calculation */
   md->effect_info.y = y;
   md->effect_info.timestamp = timestamp;

   return EINA_TRUE;
}

static Pol_Transit_Effect *
_mover_obj_effect_data_new(Pol_Transit *transit, Evas_Object *mover, int from_y, int to_y, Eina_Bool visible)
{
   Mover_Effect_Data *ed;

   ed = E_NEW(Mover_Effect_Data, 1);
   if (!ed) return NULL;

   ed->transit = transit;
   ed->mover = mover;
   ed->visible = visible;
   ed->from_dy = from_y;
   ed->to_dy = to_y;

   return ed;
}

static void
_mover_obj_effect_op(Pol_Transit_Effect *effect, Pol_Transit *transit, double progress)
{
   Mover_Effect_Data *ed = effect;
   Mover_Data *md;
   int new_y;

   md = evas_object_smart_data_get(ed->mover);
   new_y = ed->from_dy + (ed->to_dy * progress);
   _mover_obj_handler_move(md, 0, new_y);
}

static void
_mover_obj_effect_cb_mover_obj_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Mover_Effect_Data *ed = data;
   Mover_Data *md;

   ed = data;
   md = evas_object_smart_data_get(ed->mover);
   QP_VISIBLE_SET(md->qp->ec, ed->visible);

   /* make sure NULL before calling pol_transit_del() */
   ed->mover = NULL;

   pol_transit_del(ed->transit);
}

static void
_mover_obj_effect_data_free(Pol_Transit_Effect *effect, Pol_Transit *transit)
{
   Mover_Data *md;
   Mover_Effect_Data *ed = effect;

   if (ed->mover)
     {
        md = evas_object_smart_data_get(ed->mover);
        QP_VISIBLE_SET(md->qp->ec, ed->visible);

        evas_object_event_callback_del(ed->mover, EVAS_CALLBACK_DEL, _mover_obj_effect_cb_mover_obj_del);
        evas_object_del(ed->mover);
     }

   free(ed);
}

static void
_mover_obj_effect_start(Evas_Object *mover, int from_y, Eina_Bool visible)
{
   Mover_Data *md;
   E_Client *ec;
   Pol_Transit *transit;
   Pol_Transit_Effect *effect;
   int to_y;
   double duration;
   const double ref = 0.1;

   md = evas_object_smart_data_get(mover);
   ec = md->qp->ec;

   transit = pol_transit_add();
   pol_transit_object_add(transit, mover);
   pol_transit_tween_mode_set(transit, POL_TRANSIT_TWEEN_MODE_DECELERATE);

   /* determine the position as a destination */
   to_y = (visible) ? (ec->zone->h - from_y) : (-from_y);

   /* determine the transit's duration */
   duration = ((double)abs(to_y) / (ec->zone->h / 2)) * ref;
   pol_transit_duration_set(transit, duration);

   /* create and add effect to transit */
   effect = _mover_obj_effect_data_new(transit, mover, from_y, to_y, visible);
   pol_transit_effect_add(transit, _mover_obj_effect_op, effect, _mover_obj_effect_data_free);

   /* start transit */
   pol_transit_go(transit);

   evas_object_event_callback_add(mover, EVAS_CALLBACK_DEL, _mover_obj_effect_cb_mover_obj_del, effect);

   md->effect_info.transit = transit;
   md->effect_info.visible = visible;
   md->effect_info.data = (Mover_Effect_Data *)effect;
}

static void
_mover_obj_effect_stop(Evas_Object *mover)
{
   Mover_Data *md;

   md = evas_object_smart_data_get(mover);
   md->effect_info.data->mover = NULL;

   evas_object_event_callback_del(mover, EVAS_CALLBACK_DEL, _mover_obj_effect_cb_mover_obj_del);

   E_FREE_FUNC(md->effect_info.transit, pol_transit_del);
}

static Eina_Bool
_mover_obj_visibility_eval(Evas_Object *mover, int x, int y, unsigned int timestamp)
{
   E_Client *ec;
   Mover_Data *md;
   const float sensitivity = 1.5; /* hard coded. (arbitrary) */

   md = evas_object_smart_data_get(mover);
   ec = md->qp->ec;

   if ((md->effect_info.accel > sensitivity) ||
       ((md->effect_info.accel > -sensitivity) && (y > ec->zone->h / 2)))
     return EINA_TRUE;

   return EINA_FALSE;
}

static Eina_Bool
_mover_obj_is_animating(Evas_Object *mover)
{
   Mover_Data *md;

   md = evas_object_smart_data_get(mover);

   return !!md->effect_info.transit;
}

static Eina_Bool
_mover_obj_effect_visible_get(Evas_Object *mover)
{
   Mover_Data *md;

   md = evas_object_smart_data_get(mover);

   return md->effect_info.visible;
}

static void
_handler_obj_cb_mover_obj_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Handler_Data *hd;

   hd = data;
   hd->mover = NULL;
}

static void
_handler_obj_cb_gesture_start(void *data, Evas_Object *handler, int x, int y, unsigned int timestamp)
{
   Handler_Data *hd;

   hd = data;
   if (hd->mover)
     {
        if (_mover_obj_is_animating(hd->mover))
          return;

        DBG("Mover object already existed");
        evas_object_del(hd->mover);
     }

   e_comp_wl_touch_cancel();

   hd->mover = _mover_obj_new_with_move(hd->qp, 0, y, timestamp);
   evas_object_event_callback_add(hd->mover, EVAS_CALLBACK_DEL, _handler_obj_cb_mover_obj_del, hd);
}

static void
_handler_obj_cb_gesture_move(void *data, Evas_Object *handler, int x, int y, unsigned int timestamp)
{
   Handler_Data *hd;

   hd = data;
   if (!hd->mover)
     return;

   if (_mover_obj_is_animating(hd->mover))
     return;

   _mover_obj_move(hd->mover, 0, y, timestamp);
}

static void
_handler_obj_cb_gesture_end(void *data EINA_UNUSED, Evas_Object *handler, int x, int y, unsigned int timestamp)
{
   Handler_Data *hd;
   Eina_Bool v;

   hd = data;
   if (!hd->mover)
     {
        DBG("Could not find quickpanel mover object");
        return;
     }

   if (_mover_obj_is_animating(hd->mover))
     return;

   v = _mover_obj_visibility_eval(hd->mover, 0, y, timestamp);
   _mover_obj_effect_start(hd->mover, y, v);
}

static void
_handler_obj_cb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Handler_Data *hd;

   hd = data;

   E_FREE_FUNC(hd->mover, evas_object_del);
   E_FREE_FUNC(hd->gesture, e_mod_gesture_del);

   free(hd);
}

static void
_quickpanel_free(Pol_Quickpanel *pol_qp)
{
   if (pol_qp->mover)
     evas_object_del(pol_qp->mover);

   E_FREE_FUNC(pol_qp->handler.obj, evas_object_del);
   E_FREE_LIST(_quickpanel_hooks, e_client_hook_del);
   E_FREE(_pol_quickpanel);
}

static void
_quickpanel_hook_client_del(void *d EINA_UNUSED, E_Client *ec)
{
   Pol_Quickpanel *pol_qp;

   if (!ec) return;

   pol_qp = _quickpanel_find(ec);
   if (!pol_qp)
     return;

   _quickpanel_free(pol_qp);
}

static void
_quickpanel_client_evas_cb_show(void *data, Evas *evas, Evas_Object *qp_obj, void *event)
{
   Evas_Object *handler = data;

   evas_object_show(handler);
   evas_object_raise(handler);
}

static void
_quickpanel_client_evas_cb_hide(void *data, Evas *evas, Evas_Object *qp_obj, void *event)
{
   evas_object_hide((Evas_Object *)data);
}

static void
_quickpanel_client_evas_cb_move(void *data, Evas *evas, Evas_Object *qp_obj, void *event)
{
   Evas_Object *handler = data;
   int x, y, hx, hy;

   hx = _pol_quickpanel->handler.rect.x;
   hy = _pol_quickpanel->handler.rect.y;

   evas_object_geometry_get(qp_obj, &x, &y, NULL, NULL);
   evas_object_move(handler, x + hx, y + hy);
}

static void
_quickpanel_handler_rect_add(Pol_Quickpanel *qp, int angle, int x, int y, int w, int h)
{
   E_Client *ec;
   Evas_Object *handler;

   ec = qp->ec;

   ELOGF("QUICKPANEL", "Handler Geo Set | x %d, y %d, w %d, h %d",
         NULL, NULL, x, y, w, h);

   handler =
      e_mod_quickpanel_handler_object_add(ec,
                                          ec->client.x + x,
                                          ec->client.y + y,
                                          w, h);

   /* Add handler object to smart member to follow the client's stack */
   evas_object_smart_member_add(handler, ec->frame);
   evas_object_propagate_events_set(handler, 0);

   if (evas_object_visible_get(ec->frame))
     evas_object_show(handler);

   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW,
                                  _quickpanel_client_evas_cb_show, handler);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_HIDE,
                                  _quickpanel_client_evas_cb_hide, handler);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOVE,
                                  _quickpanel_client_evas_cb_move, handler);

   EINA_RECTANGLE_SET(&qp->handler.rect, x, y, w, h);
   qp->handler.obj = handler;
}

static void
_quickpanel_handler_region_set(Pol_Quickpanel *qp, int angle, Eina_Tiler *tiler)
{
   Eina_Iterator *it;
   Eina_Rectangle *r;

   /* NOTE: supported single rectangle, not tiler */

   it = eina_tiler_iterator_new(tiler);
   EINA_ITERATOR_FOREACH(it, r)
     {
        _quickpanel_handler_rect_add(qp, angle, r->x, r->y, r->w, r->h);
        e_mod_indicator_create(r->w, r->h);
        break;
     }
   eina_iterator_free(it);
}

static void
_quickpanel_visibility_change(Pol_Quickpanel *qp, Eina_Bool vis, Eina_Bool with_effect)
{
   E_Client *ec;
   Evas_Object *mover;
   Mover_Data *md;
   Eina_Bool cur_vis = EINA_FALSE;
   int from_y;
   int x, y, w, h;

   ec = qp->ec;

   evas_object_geometry_get(ec->frame, &x, &y, &w, &h);

   if (E_INTERSECTS(x, y, w, h, ec->zone->x, ec->zone->y, ec->zone->w, ec->zone->h))
     cur_vis = evas_object_visible_get(ec->frame);

   if (cur_vis == vis)
     return;

   mover = qp->mover;

   if (with_effect)
     {
        if (mover)
          {
             if (_mover_obj_is_animating(mover))
               {
                  if (_mover_obj_effect_visible_get(mover) == vis)
                    return;

                  _mover_obj_effect_stop(mover);
               }

             md = evas_object_smart_data_get(mover);
             from_y = md->handler.rect.y;
          }
        else
          {
             from_y = vis ? 0 : ec->zone->h;
             mover = _mover_obj_new_with_move(qp, 0, from_y, 0);
          }

        _mover_obj_effect_start(mover, from_y, vis);
     }
   else
     {
        if (mover)
          {
             if (_mover_obj_is_animating(mover))
               _mover_obj_effect_stop(mover);
             evas_object_del(mover);
          }

        QP_VISIBLE_SET(ec, vis);
     }
}

static void
_quickpanel_client_evas_cb_show_block(void *data, Evas *evas, Evas_Object *qp_obj, void *event)
{
   Pol_Quickpanel *pol_qp = data;
   E_Client *ec = pol_qp->ec;

   if (pol_qp->show_block)
     QP_HIDE(ec);
   else
     {
        evas_object_event_callback_del_full(ec->frame,
                                            EVAS_CALLBACK_SHOW,
                                            _quickpanel_client_evas_cb_show_block,
                                            pol_qp);
     }
}

static Eina_Bool
_quickpanel_cb_buffer_change(void *data, int type, void *event)
{
   E_Event_Client *ev = event;
   E_Client *ec;

   ec = ev->ec;
   if (ec != e_mod_quickpanel_client_get())
     goto end;

   if (ec->visible)
     goto end;

   e_comp_client_post_update_add(ec);

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_quickpanel_cb_rotation_done(void *data, int type, void *event)
{
   E_Event_Client *ev = event;
   E_Client *ec;

   ec = ev->ec;
   if (ec != e_mod_quickpanel_client_get())
     goto end;

   if (!ec->visible)
     goto end;

   /* force update */
   e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
   e_comp_object_dirty(ec->frame);

end:
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

/* NOTE: supported single client for quickpanel for now. */
EINTERN void
e_mod_quickpanel_client_set(E_Client *ec)
{
   Pol_Quickpanel *pol_qp = NULL;

   if (EINA_UNLIKELY(!ec))
     {
        pol_qp = _quickpanel_find(e_mod_quickpanel_client_get());
        _quickpanel_free(pol_qp);
        return;
     }

   /* check for client being deleted */
   if (e_object_is_del(E_OBJECT(ec))) return;

   /* check for wayland pixmap */
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;

   /* if we have not setup evas callbacks for this client, do it */
   if (_pol_quickpanel) return;

   pol_qp = E_NEW(Pol_Quickpanel, 1);
   if (!pol_qp) return;

   pol_qp->ec = ec;
   pol_qp->show_block = EINA_TRUE;

   ELOGF("QUICKPANEL", "Set Client | ec %p", NULL, NULL, ec);

   eina_stringshare_replace(&ec->icccm.window_role, "quickpanel");

   // set quickpanel layer
   if (WM_POL_QUICKPANEL_LAYER != evas_object_layer_get(ec->frame))
     {
        evas_object_layer_set(ec->frame, WM_POL_QUICKPANEL_LAYER);
     }
   ec->layer = WM_POL_QUICKPANEL_LAYER;

   // set skip iconify
   ec->exp_iconify.skip_iconify = 1;

   ec->e.state.rot.type = E_CLIENT_ROTATION_TYPE_DEPENDENT;
   e_mod_pol_rotation_force_update_add(ec);

   QP_HIDE(ec);

   // to avoid that quickpanel is shawn, when it's first launched. */
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW,
                                  _quickpanel_client_evas_cb_show_block, pol_qp);

   E_CLIENT_HOOK_APPEND(_quickpanel_hooks, E_CLIENT_HOOK_DEL,
                        _quickpanel_hook_client_del, NULL);

   E_LIST_HANDLER_APPEND(_quickpanel_events, E_EVENT_CLIENT_BUFFER_CHANGE,       _quickpanel_cb_buffer_change, NULL);
   E_LIST_HANDLER_APPEND(_quickpanel_events, E_EVENT_CLIENT_ROTATION_CHANGE_END, _quickpanel_cb_rotation_done, NULL);

   _pol_quickpanel = pol_qp;
}

EINTERN E_Client *
e_mod_quickpanel_client_get(void)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(_pol_quickpanel, NULL);

   return _pol_quickpanel->ec;
}

EINTERN Eina_Bool
e_mod_quickpanel_region_set(int type, int angle, Eina_Tiler *tiler)
{
   Pol_Quickpanel *qp;
   E_Client *ec;

   ec = e_mod_quickpanel_client_get();
   if (EINA_UNLIKELY(!ec))
     return EINA_FALSE;

   qp = _quickpanel_find(ec);
   if (EINA_UNLIKELY(!qp))
     return EINA_FALSE;

   // FIXME should consider rotation.
   if (angle != 0)
     return EINA_FALSE;

   // FIXME: region type
   if (type != 0)
     return EINA_FALSE;

   _quickpanel_handler_region_set(qp, angle, tiler);

   return EINA_TRUE;
}

EINTERN Evas_Object *
e_mod_quickpanel_handler_object_add(E_Client *ec, int x, int y, int w, int h)
{
   Pol_Quickpanel *qp;
   Pol_Gesture *gesture;
   Evas_Object *handler;
   Handler_Data *hd;

   qp = _quickpanel_find(ec);
   if (!qp)
     return NULL;

   hd = E_NEW(Handler_Data, 1);
   if (EINA_UNLIKELY(!hd))
     return NULL;

   handler = evas_object_rectangle_add(evas_object_evas_get(ec->frame));

   /* make it transparent */
   evas_object_color_set(handler, 0, 0, 0, 0);

   evas_object_repeat_events_set(handler, EINA_TRUE);

   evas_object_move(handler, x, y);
   evas_object_resize(handler, w, h);

   evas_object_event_callback_add(handler, EVAS_CALLBACK_DEL, _handler_obj_cb_del, hd);

   gesture = e_mod_gesture_add(handler, POL_GESTURE_TYPE_LINE);
   e_mod_gesture_cb_set(gesture,
                        _handler_obj_cb_gesture_start,
                        _handler_obj_cb_gesture_move,
                        _handler_obj_cb_gesture_end,
                        hd);

   hd->qp = qp;
   hd->gesture = gesture;
   hd->handler = handler;
   hd->mover = NULL;

   return handler;
}

EINTERN void
e_mod_quickpanel_show(void)
{
   Pol_Quickpanel *qp;
   E_Client *ec;

   ec = e_mod_quickpanel_client_get();
   if (!ec)
     return;

   qp = _quickpanel_find(ec);
   if (!qp)
     return;

   _quickpanel_visibility_change(qp, EINA_TRUE, EINA_TRUE);
}

EINTERN void
e_mod_quickpanel_hide(void)
{
   Pol_Quickpanel *qp;
   E_Client *ec;

   ec = e_mod_quickpanel_client_get();
   if (!ec)
     return;

   qp = _quickpanel_find(ec);
   if (!qp)
     return;

   _quickpanel_visibility_change(qp, EINA_FALSE, EINA_TRUE);
}
