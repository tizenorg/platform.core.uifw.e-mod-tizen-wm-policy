#include "e_mod_private_data.h"
#include "e_mod_main.h"
#include "e_mod_quickpanel.h"
#include "e_mod_gesture.h"
#include "e_mod_region.h"
#include "e_mod_transit.h"
#include "e_mod_rotation.h"

#define SMART_NAME            "quickpanel_object"
#define INTERNAL_ENTRY                    \
   Mover_Data *md;                        \
   md = evas_object_smart_data_get(obj)

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
typedef struct _Mover_Effect_Data Mover_Effect_Data;

struct _Pol_Quickpanel
{
   E_Client *ec;
   Evas_Object *mover;
   Evas_Object *indi_obj;
   Evas_Object *handler_obj;

   Eina_List *hooks;
   Eina_List *events;

   Rot_Idx rotation;

   Eina_Bool show_block;
};

struct _Mover_Data
{
   Pol_Quickpanel *qp;
   E_Client *ec;

   Evas_Object *smart_obj; //smart object
   Evas_Object *qp_layout_obj; // quickpanel's e_layout_object
   Evas_Object *handler_mirror_obj; // quickpanel handler mirror object
   Evas_Object *base_clip; // clipper for quickapnel base object
   Evas_Object *handler_clip; // clipper for quickpanel handler object

   Eina_Rectangle handler_rect;
   Rot_Idx rotation;

   struct
   {
      Pol_Transit *transit;
      Mover_Effect_Data *data;
      int x, y;
      unsigned int timestamp;
      float accel;
      Eina_Bool visible;
   } effect_info;
};

struct _Mover_Effect_Data
{
   Pol_Transit *transit;
   Evas_Object *mover;
   int from;
   int to;
   Eina_Bool visible : 1;
};

static Pol_Quickpanel *_pol_quickpanel = NULL;
static Evas_Smart *_mover_smart = NULL;

static Pol_Quickpanel *
_quickpanel_get()
{
   return _pol_quickpanel;
}

static void
_mover_intercept_show(void *data, Evas_Object *obj)
{
   Mover_Data *md;
   E_Client *ec;
   Evas *e;

   md = data;
   md->qp->show_block = EINA_FALSE;

   ec = md->ec;
   QP_SHOW(ec);

   /* force update */
   e_comp_object_damage(ec->frame, 0, 0, ec->w, ec->h);
   e_comp_object_dirty(ec->frame);
   e_comp_object_render(ec->frame);

  // create base_clip
   e = evas_object_evas_get(obj);
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
   e_layout_child_move(md->handler_mirror_obj, ec->x, ec->y);
   e_layout_child_resize(md->handler_mirror_obj, ec->w, ec->h);
   evas_object_show(md->handler_mirror_obj);

   // create handler_clip
   md->handler_clip = evas_object_rectangle_add(e);
   e_layout_pack(md->qp_layout_obj, md->handler_clip);
   e_layout_child_move(md->handler_clip, md->handler_rect.x, md->handler_rect.y);
   e_layout_child_resize(md->handler_clip, md->handler_rect.w, md->handler_rect.h);
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
   E_Zone *zone;
   E_Client *ec;

   ec = md->ec;
   zone = ec->zone;
   switch (md->rotation)
     {
      case ROT_IDX_90:
         if ((x + md->handler_rect.w) > zone->w) return EINA_FALSE;

         md->handler_rect.x = x;
         e_layout_child_resize(md->base_clip, md->handler_rect.x, ec->h);
         e_layout_child_move(md->handler_mirror_obj, md->handler_rect.x - ec->w + md->handler_rect.w, md->handler_rect.y);
         e_layout_child_move(md->handler_clip, md->handler_rect.x, md->handler_rect.y);
         break;
      case ROT_IDX_180:
         if ((y - md->handler_rect.h) < 0) return EINA_FALSE;

         md->handler_rect.y = y;
         e_layout_child_move(md->base_clip, md->handler_rect.x, md->handler_rect.y);
         e_layout_child_resize(md->base_clip, ec->w, ec->h - md->handler_rect.y);
         e_layout_child_move(md->handler_mirror_obj, md->handler_rect.x, md->handler_rect.y - md->handler_rect.h);
         e_layout_child_move(md->handler_clip, md->handler_rect.x, md->handler_rect.y - md->handler_rect.h);
         break;
      case ROT_IDX_270:
         if ((x - md->handler_rect.w) < 0) return EINA_FALSE;

         md->handler_rect.x = x;
         e_layout_child_move(md->base_clip, md->handler_rect.x, md->handler_rect.y);
         e_layout_child_resize(md->base_clip, ec->w - md->handler_rect.x, ec->h);
         e_layout_child_move(md->handler_mirror_obj, md->handler_rect.x - md->handler_rect.w, md->handler_rect.y);
         e_layout_child_move(md->handler_clip, md->handler_rect.x - md->handler_rect.w, md->handler_rect.y);
         break;
      default:
        if ((y + md->handler_rect.h) > zone->h) return EINA_FALSE;

        md->handler_rect.y = y;
        e_layout_child_resize(md->base_clip, ec->w, md->handler_rect.y);
        e_layout_child_move(md->handler_mirror_obj, md->handler_rect.x, md->handler_rect.y - ec->h + md->handler_rect.h);
        e_layout_child_move(md->handler_clip, md->handler_rect.x, md->handler_rect.y);
     }

   return EINA_TRUE;
}

static Evas_Object *
_mover_obj_new(Pol_Quickpanel *qp)
{
   Evas_Object *mover;
   Mover_Data *md;
   int x, y, w, h;

   /* Pause WM Rotation during mover object is working. */
   e_zone_rotation_block_set(qp->ec->zone, "quickpanel-mover", EINA_TRUE);

   _mover_smart_init();
   mover = evas_object_smart_add(evas_object_evas_get(qp->ec->frame), _mover_smart);

   /* Should setup 'md' before call evas_object_show() */
   md = evas_object_smart_data_get(mover);
   md->qp = qp;
   md->ec = qp->ec;
   md->rotation = qp->rotation;

   e_mod_region_rectangle_get(qp->handler_obj, qp->rotation, &x, &y, &w, &h);
   EINA_RECTANGLE_SET(&md->handler_rect, x, y, w, h);

   evas_object_move(mover, 0, 0);
   evas_object_resize(mover, qp->ec->w, qp->ec->h);
   evas_object_show(mover);

   qp->show_block = EINA_FALSE;

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
   md->effect_info.x = x;
   md->effect_info.y = y;
   md->effect_info.timestamp = timestamp;

   _mover_obj_handler_move(md, x, y);

   return mover;
}

static void
_mover_obj_visible_set(Evas_Object *mover, Eina_Bool visible)
{
   Mover_Data *md;
   E_Client *ec;
   int x = 0, y = 0;

   md = evas_object_smart_data_get(mover);
   ec = md->ec;

   switch (md->rotation)
     {
      case ROT_IDX_90:
         x = visible ? ec->zone->w : 0;
         break;
      case ROT_IDX_180:
         y = visible ? 0 : ec->zone->h;
         break;
      case ROT_IDX_270:
         x = visible ? 0 : ec->zone->w;
         break;
      default:
         y = visible ? ec->zone->h : 0;
         break;
     }

   _mover_obj_handler_move(md, x, y);
}

static Eina_Bool
_mover_obj_move(Evas_Object *mover, int x, int y, unsigned int timestamp)
{
   Mover_Data *md;
   int dp;
   unsigned int dt;

   if (!mover) return EINA_FALSE;

   md = evas_object_smart_data_get(mover);
   if (!_mover_obj_handler_move(md, x, y)) return EINA_FALSE;

   /* Calculate the acceleration of movement,
    * determine the visibility of quickpanel based on the result. */
   dt = timestamp - md->effect_info.timestamp;
   switch (md->rotation)
     {
      case ROT_IDX_90:
         dp = x - md->effect_info.x;
         break;
      case ROT_IDX_180:
         dp = md->effect_info.y - y;
         break;
      case ROT_IDX_270:
         dp = md->effect_info.x - x;
         break;
      default:
         dp = y - md->effect_info.y;
         break;
     }
   if (dt) md->effect_info.accel = (float)dp / (float)dt;

   /* Store current information to next calculation */
   md->effect_info.x = x;
   md->effect_info.y = y;
   md->effect_info.timestamp = timestamp;

   return EINA_TRUE;
}

static Pol_Transit_Effect *
_mover_obj_effect_data_new(Pol_Transit *transit, Evas_Object *mover, int from, int to, Eina_Bool visible)
{
   Mover_Effect_Data *ed;

   ed = E_NEW(Mover_Effect_Data, 1);
   if (!ed) return NULL;

   ed->transit = transit;
   ed->mover = mover;
   ed->visible = visible;
   ed->from = from;
   ed->to = to;

   return ed;
}

static void
_mover_obj_effect_op(Pol_Transit_Effect *effect, Pol_Transit *transit, double progress)
{
   Mover_Effect_Data *ed = effect;
   Mover_Data *md;
   int new_x = 0, new_y = 0;

   md = evas_object_smart_data_get(ed->mover);

   switch (md->rotation)
     {
      case ROT_IDX_90:
      case ROT_IDX_270:
         new_x = ed->from + (ed->to * progress);
         break;
      default:
      case ROT_IDX_180:
         new_y = ed->from + (ed->to * progress);
         break;
     }

   _mover_obj_handler_move(md, new_x, new_y);
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
_mover_obj_effect_start(Evas_Object *mover, Eina_Bool visible)
{
   Mover_Data *md;
   E_Client *ec;
   Pol_Transit *transit;
   Pol_Transit_Effect *effect;
   int from;
   int to;
   double duration;
   const double ref = 0.1;

   md = evas_object_smart_data_get(mover);
   ec = md->qp->ec;

   switch (md->rotation)
     {
      case ROT_IDX_90:
         from = md->handler_rect.x;
         to = (visible) ? (ec->zone->w - from) : (-from);
         duration = ((double)abs(to) / (ec->zone->w / 2)) * ref;
         break;
      case ROT_IDX_180:
         from = md->handler_rect.y;
         to = (visible) ? (-from) : (ec->zone->h - from);
         duration = ((double)abs(to) / (ec->zone->h / 2)) * ref;
         break;
      case ROT_IDX_270:
         from = md->handler_rect.x;
         to = (visible) ? (-from) : (ec->zone->w - from);
         duration = ((double)abs(to) / (ec->zone->w / 2)) * ref;
         break;
      default:
         from = md->handler_rect.y;
         to = (visible) ? (ec->zone->h - from) : (-from);
         duration = ((double)abs(to) / (ec->zone->h / 2)) * ref;
         break;
     }

   transit = pol_transit_add();
   pol_transit_object_add(transit, mover);
   pol_transit_tween_mode_set(transit, POL_TRANSIT_TWEEN_MODE_DECELERATE);

   pol_transit_duration_set(transit, duration);

   /* create and add effect to transit */
   effect = _mover_obj_effect_data_new(transit, mover, from, to, visible);
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
_mover_obj_visibility_eval(Evas_Object *mover)
{
   E_Client *ec;
   Mover_Data *md;
   Eina_Bool threshold;
   const float sensitivity = 1.5; /* hard coded. (arbitrary) */

   md = evas_object_smart_data_get(mover);
   ec = md->ec;

   switch (md->rotation)
     {
        case ROT_IDX_90:
           threshold = (md->handler_rect.x > (ec->zone->w / 2));
           break;
        case ROT_IDX_180:
           threshold = (md->handler_rect.y < (ec->zone->h / 2));
           break;
        case ROT_IDX_270:
           threshold = (md->handler_rect.x < (ec->zone->w / 2));
           break;
        default:
           threshold = (md->handler_rect.y > (ec->zone->h / 2));
           break;
     }

   if ((md->effect_info.accel > sensitivity) ||
       ((md->effect_info.accel > -sensitivity) && threshold))
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
_region_obj_cb_gesture_start(void *data, Evas_Object *handler, int x, int y, unsigned int timestamp)
{
   Pol_Quickpanel *qp;

   qp = data;
   if (EINA_UNLIKELY(!qp))
     return;

   if (EINA_UNLIKELY(!qp->ec))
     return;

   if (e_object_is_del(E_OBJECT(qp->ec)))
     return;

   if (qp->mover)
     {
        if (_mover_obj_is_animating(qp->mover))
          return;

        DBG("Mover object already existed");
        evas_object_del(qp->mover);
     }

   e_comp_wl_touch_cancel();

   qp->mover = _mover_obj_new_with_move(qp, x, y, timestamp);
}

static void
_region_obj_cb_gesture_move(void *data, Evas_Object *handler, int x, int y, unsigned int timestamp)
{
   Pol_Quickpanel *qp;

   qp = data;
   if (!qp->mover)
     return;

   if (_mover_obj_is_animating(qp->mover))
     return;

   _mover_obj_move(qp->mover, x, y, timestamp);
}

static void
_region_obj_cb_gesture_end(void *data EINA_UNUSED, Evas_Object *handler, int x, int y, unsigned int timestamp)
{
   Pol_Quickpanel *qp;
   Eina_Bool v;

   qp = data;
   if (!qp->mover)
     {
        DBG("Could not find quickpanel mover object");
        return;
     }

   if (_mover_obj_is_animating(qp->mover))
     return;

   v = _mover_obj_visibility_eval(qp->mover);
   _mover_obj_effect_start(qp->mover, v);
}

static void
_quickpanel_free(Pol_Quickpanel *qp)
{
   E_FREE_FUNC(qp->mover, evas_object_del);
   E_FREE_FUNC(qp->indi_obj, evas_object_del);
   E_FREE_FUNC(qp->handler_obj, evas_object_del);
   E_FREE_LIST(qp->events, ecore_event_handler_del);
   E_FREE_LIST(qp->hooks, e_client_hook_del);
   E_FREE(_pol_quickpanel);
}

static void
_quickpanel_hook_client_del(void *d, E_Client *ec)
{
   Pol_Quickpanel *qp;

   qp = d;
   if (EINA_UNLIKELY(!qp))
     return;

   if (!ec) return;

   if (qp->ec != ec)
     return;

   _quickpanel_free(qp);

   e_mod_pol_rotation_force_update_del(ec);
}

static void
_quickpanel_client_evas_cb_show(void *data, Evas *evas, Evas_Object *obj, void *event)
{
   Pol_Quickpanel *qp;

   qp = data;
   if (EINA_UNLIKELY(!qp))
     return;

   if (qp->show_block)
     {
        QP_HIDE(qp->ec);
        evas_object_show(qp->indi_obj);
        return;
     }

   evas_object_show(qp->handler_obj);
   evas_object_raise(qp->handler_obj);

   evas_object_hide(qp->indi_obj);
}

static void
_quickpanel_client_evas_cb_hide(void *data, Evas *evas, Evas_Object *obj, void *event)
{
   Pol_Quickpanel *qp;

   qp = data;
   if (EINA_UNLIKELY(!qp))
     return;

   evas_object_hide(qp->handler_obj);
   evas_object_show(qp->indi_obj);
}

static void
_quickpanel_client_evas_cb_move(void *data, Evas *evas, Evas_Object *obj, void *event)
{
   Pol_Quickpanel *qp;
   int x, y, hx, hy;

   qp = data;
   if (EINA_UNLIKELY(!qp))
     return;

   e_mod_region_rectangle_get(qp->handler_obj, qp->rotation, &hx, &hy, NULL, NULL);
   evas_object_geometry_get(obj, &x, &y, NULL, NULL);
   evas_object_move(qp->handler_obj, x + hx, y + hy);
}

static void
_quickpanel_handler_rect_add(Pol_Quickpanel *qp, Rot_Idx ridx, int x, int y, int w, int h)
{
   E_Client *ec;
   Evas_Object *obj;

   ec = qp->ec;

   ELOGF("QUICKPANEL", "Handler Geo Set | x %d, y %d, w %d, h %d",
         NULL, NULL, x, y, w, h);

   if (qp->handler_obj)
     goto end;

   obj = e_mod_region_object_new();
   if (!obj)
     return;

   e_mod_region_cb_set(obj,
                       _region_obj_cb_gesture_start,
                       _region_obj_cb_gesture_move,
                       _region_obj_cb_gesture_end, qp);

   /* Add handler object to smart member to follow the client's stack */
   evas_object_smart_member_add(obj, ec->frame);
   evas_object_propagate_events_set(obj, 0);
   if (evas_object_visible_get(ec->frame))
     evas_object_show(obj);

   qp->handler_obj = obj;

end:
   e_mod_region_rectangle_set(qp->handler_obj, ridx, x, y, w, h);
}

static void
_quickpanel_handler_region_set(Pol_Quickpanel *qp, Rot_Idx ridx, Eina_Tiler *tiler)
{
   Eina_Iterator *it;
   Eina_Rectangle *r;
   int x = 0, y = 0;

   /* FIXME supported single rectangle, not tiler */

   it = eina_tiler_iterator_new(tiler);
   EINA_ITERATOR_FOREACH(it, r)
     {
        _quickpanel_handler_rect_add(qp, ridx, r->x, r->y, r->w, r->h);

        /* FIXME: this should be set by another way like indicator */
        if (ridx == ROT_IDX_180)
          {
             x = 0;
             y = qp->ec->zone->h - r->h;
          }
        else if (ridx == ROT_IDX_270)
          {
             x = qp->ec->zone->w - r->w;
             y = 0;
          }
        e_mod_region_rectangle_set(qp->indi_obj, ridx, x, y, r->w, r->h);

        break;
     }
   eina_iterator_free(it);
}

static void
_quickpanel_visibility_change(Pol_Quickpanel *qp, Eina_Bool vis, Eina_Bool with_effect)
{
   E_Client *ec;
   Evas_Object *mover;
   Eina_Bool cur_vis = EINA_FALSE;
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
          }
        else
          {
             mover = _mover_obj_new(qp);
             _mover_obj_visible_set(mover, !vis);
          }

        _mover_obj_effect_start(mover, vis);
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
_quickpanel_cb_rotation_begin(void *data, int type, void *event)
{
   Pol_Quickpanel *qp;
   E_Event_Client *ev = event;
   E_Client *ec;

   qp = data;
   if (EINA_UNLIKELY(!qp))
     goto end;

   ec = ev->ec;
   if (EINA_UNLIKELY(!ec))
     goto end;

   if (qp->ec != ec)
     goto end;

   E_FREE_FUNC(qp->mover, evas_object_del);

   evas_object_hide(qp->indi_obj);
   evas_object_hide(qp->handler_obj);

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_quickpanel_cb_rotation_cancel(void *data, int type, void *event)
{
   Pol_Quickpanel *qp;
   E_Event_Client *ev = event;
   E_Client *ec;

   qp = data;
   if (EINA_UNLIKELY(!qp))
     goto end;

   ec = ev->ec;
   if (EINA_UNLIKELY(!ec))
     goto end;

   if (qp->ec != ec)
     goto end;

   if (evas_object_visible_get(ec->frame))
     evas_object_show(qp->handler_obj);
   else
     evas_object_show(qp->indi_obj);

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_quickpanel_cb_rotation_done(void *data, int type, void *event)
{
   Pol_Quickpanel *qp;
   E_Event_Client *ev = event;
   E_Client *ec;

   qp = data;
   if (EINA_UNLIKELY(!qp))
     goto end;

   ec = ev->ec;
   if (EINA_UNLIKELY(!ec))
     goto end;

   if (qp->ec != ec)
     goto end;

   qp->rotation = e_mod_rotation_angle_to_idx(ec->e.state.rot.ang.curr);

   if (evas_object_visible_get(ec->frame))
     evas_object_show(qp->handler_obj);
   else
     evas_object_show(qp->indi_obj);

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Evas_Object *
_quickpanel_indicator_object_new(Pol_Quickpanel *qp)
{
   Evas_Object *indi_obj;

   indi_obj = e_mod_region_object_new();
   if (!indi_obj)
     return NULL;

   evas_object_repeat_events_set(indi_obj, EINA_TRUE);
   /* FIXME: make me move to explicit layer something like POL_LAYER */
   evas_object_layer_set(indi_obj, EVAS_LAYER_MAX - 1);

   e_mod_region_cb_set(indi_obj,
                       _region_obj_cb_gesture_start,
                       _region_obj_cb_gesture_move,
                       _region_obj_cb_gesture_end, qp);

   return indi_obj;
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
   Pol_Quickpanel *qp;

   if (EINA_UNLIKELY(!ec))
     {
        qp = _quickpanel_get();
        if (qp)
          _quickpanel_free(qp);
        return;
     }

   /* check for client being deleted */
   if (e_object_is_del(E_OBJECT(ec))) return;

   /* check for wayland pixmap */
   if (e_pixmap_type_get(ec->pixmap) != E_PIXMAP_TYPE_WL) return;

   /* if we have not setup evas callbacks for this client, do it */
   if (_pol_quickpanel) return;

   ELOGF("QUICKPANEL", "Set Client | ec %p", NULL, NULL, ec);

   qp = calloc(1, sizeof(*qp));
   if (!qp)
     return;

   _pol_quickpanel = qp;

   qp->ec = ec;
   qp->show_block = EINA_TRUE;
   qp->indi_obj = _quickpanel_indicator_object_new(qp);
   if (!qp->indi_obj)
     {
        free(qp);
        return;
     }

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

   /* force update rotation for quickpanel */
   e_mod_pol_rotation_force_update_add(ec);

   QP_HIDE(ec);

   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_SHOW, _quickpanel_client_evas_cb_show, qp);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_HIDE, _quickpanel_client_evas_cb_hide, qp);
   evas_object_event_callback_add(ec->frame, EVAS_CALLBACK_MOVE, _quickpanel_client_evas_cb_move, qp);

   E_CLIENT_HOOK_APPEND(qp->hooks,   E_CLIENT_HOOK_DEL,                       _quickpanel_hook_client_del,  qp);
   E_LIST_HANDLER_APPEND(qp->events, E_EVENT_CLIENT_BUFFER_CHANGE,            _quickpanel_cb_buffer_change, qp);
   E_LIST_HANDLER_APPEND(qp->events, E_EVENT_CLIENT_ROTATION_CHANGE_BEGIN,    _quickpanel_cb_rotation_begin, qp);
   E_LIST_HANDLER_APPEND(qp->events, E_EVENT_CLIENT_ROTATION_CHANGE_CANCEL,   _quickpanel_cb_rotation_cancel, qp);
   E_LIST_HANDLER_APPEND(qp->events, E_EVENT_CLIENT_ROTATION_CHANGE_END,      _quickpanel_cb_rotation_done, qp);
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
   Rot_Idx ridx;

   qp = _quickpanel_get();
   if (EINA_UNLIKELY(!qp))
     return EINA_FALSE;

   if (EINA_UNLIKELY(!qp->ec))
     return EINA_FALSE;

   if (e_object_is_del(E_OBJECT(qp->ec)))
     return EINA_FALSE;

   // FIXME: region type
   if (type != 0)
     return EINA_FALSE;

   ridx = e_mod_rotation_angle_to_idx(angle);
   _quickpanel_handler_region_set(qp, ridx, tiler);

   return EINA_TRUE;
}

EINTERN void
e_mod_quickpanel_show(void)
{
   Pol_Quickpanel *qp;

   qp = _quickpanel_get();
   if (EINA_UNLIKELY(!qp))
     return;

   if (EINA_UNLIKELY(!qp->ec))
     return;

   if (e_object_is_del(E_OBJECT(qp->ec)))
     return;

   _quickpanel_visibility_change(qp, EINA_TRUE, EINA_TRUE);
}

EINTERN void
e_mod_quickpanel_hide(void)
{
   Pol_Quickpanel *qp;

   qp = _quickpanel_get();
   if (EINA_UNLIKELY(!qp))
     return;

   if (EINA_UNLIKELY(!qp->ec))
     return;

   if (e_object_is_del(E_OBJECT(qp->ec)))
     return;

   _quickpanel_visibility_change(qp, EINA_FALSE, EINA_TRUE);
}
