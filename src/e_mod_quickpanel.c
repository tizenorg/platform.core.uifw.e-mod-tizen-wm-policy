#include "e_mod_main.h"
#include "e_mod_quickpanel.h"

#define SMART_NAME      "quickpanel_object"
#define QP_DATA_KEY     "qp_mover"
#define QP_EC           _pol_quickpanel->ec
#define INTERNAL_ENTRY                    \
   Mover_Data *md;                        \
   md = evas_object_smart_data_get(obj);

typedef struct _Pol_Quickpanel Pol_Quickpanel;
typedef struct _Mover_Data Mover_Data;
typedef struct _Mover_Effect_Data Mover_Effect_Data;
typedef struct _Flick_Info Flick_Info;

struct _Pol_Quickpanel
{
   E_Client *ec;

   struct
   {
      Evas_Object *obj;
      Eina_Rectangle rect;
   } handler;
};

struct _Mover_Effect_Data
{
   Evas_Object *mover;
   int from_dy;
   int to_dy;
   Eina_Bool visible : 1;
};

struct _Mover_Data
{
   E_Client    *ec; // quickpanel's e_client

   Evas_Object *smart_obj; //smart object
   Evas_Object *qp_layout_obj; // quickpanel's e_layout_object
   Evas_Object *base_mirror_obj; // quickpanel base mirror object
   Evas_Object *handler_mirror_obj; // quickpanel handler mirror object
   Evas_Object *base_clip; // clipper for quickapnel base object
   Evas_Object *handler_clip; // clipper for quickpanel handler object

   Eina_Bool    qp_layout_init: 1;

   struct
   {
      int x;
      int y;
      int w;
      int h;
   } handler_geo; // handler geometry

   struct
   {
      Elm_Transit *transit;
      int y;
      unsigned int timestamp;
      float accel;
   } effect_info;
};

struct _Flick_Info
{
   int y;
   int timestamp;
   Eina_Bool active;
};

static Pol_Quickpanel *_pol_quickpanel = NULL;
static Eina_List *_quickpanel_hooks = NULL;
static Evas_Smart *_quickpanel_smart = NULL;

static void
_quickpanel_object_intercept_show(void *data, Evas_Object *obj EINA_UNUSED)
{
   Mover_Data *md = data;
   E_Client *ec = md->ec;

   if (md->qp_layout_init)
     {
        evas_object_show(md->smart_obj);
        return;
     }
   else
     {
        evas_object_color_set(md->ec->frame, 0, 0, 0, 0);

        md->base_mirror_obj =  e_comp_object_util_mirror_add(ec->frame);
        e_layout_pack(md->qp_layout_obj, md->base_mirror_obj);
        e_layout_child_move(md->base_mirror_obj, 0, 0);
        e_layout_child_resize(md->base_mirror_obj, ec->w, ec->h);
        evas_object_show(md->base_mirror_obj);

        md->base_clip = evas_object_rectangle_add(e_comp->evas);
        e_layout_pack(md->qp_layout_obj, md->base_clip);
        e_layout_child_move(md->base_clip, 0, 0);
        e_layout_child_resize(md->base_clip, ec->w, ec->h);
        evas_object_color_set(md->base_clip, 255, 255, 255, 255);
        evas_object_show(md->base_clip);

        evas_object_clip_set(md->base_mirror_obj, md->base_clip);

        // for handler object
        md->handler_mirror_obj =  e_comp_object_util_mirror_add(ec->frame);
        e_layout_pack(md->qp_layout_obj, md->handler_mirror_obj);
        e_layout_child_move(md->handler_mirror_obj, md->handler_geo.x, md->handler_geo.y);
        e_layout_child_resize(md->handler_mirror_obj, ec->w, ec->h);
        evas_object_show(md->handler_mirror_obj);

        md->handler_clip = evas_object_rectangle_add(e_comp->evas);
        e_layout_pack(md->qp_layout_obj, md->handler_clip);
        e_layout_child_move(md->handler_clip, md->handler_geo.x, md->handler_geo.y);
        e_layout_child_resize(md->handler_clip, md->handler_geo.w, md->handler_geo.h);
        evas_object_color_set(md->handler_clip, 255, 255, 255, 255);
        evas_object_show(md->handler_clip);

        evas_object_clip_set(md->handler_mirror_obj, md->handler_clip);

        md->qp_layout_init = EINA_TRUE;
     }

   evas_object_show(md->smart_obj);
}

static void
_quickpanel_smart_add(Evas_Object *obj)
{
   Mover_Data *md;
   md = E_NEW(Mover_Data, 1);
   md->smart_obj = obj;
   md->qp_layout_obj = e_layout_add(e_comp->evas);
   evas_object_color_set(md->qp_layout_obj, 255, 255, 255, 255);
   evas_object_smart_member_add(md->qp_layout_obj, md->smart_obj);

   evas_object_layer_set(md->smart_obj, EVAS_LAYER_MAX - 1); // EVAS_LAYER_MAX :L cursor layer

   evas_object_smart_data_set(obj, md);

   evas_object_move(obj, -1 , -1);

   evas_object_intercept_show_callback_add(obj, _quickpanel_object_intercept_show, md);
}

static void
_quickpanel_smart_del(Evas_Object *obj)
{
   E_Client *ec;

   INTERNAL_ENTRY;

   ec = md->ec;
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
   if (md->base_mirror_obj)
     {
        e_layout_unpack(md->base_mirror_obj);
        evas_object_del(md->base_mirror_obj);
     }
   if (md->handler_mirror_obj)
     {
        e_layout_unpack(md->handler_mirror_obj);
        evas_object_del(md->handler_mirror_obj);
     }

   if (md->qp_layout_obj) evas_object_del(md->qp_layout_obj);

   evas_object_color_set(ec->frame, ec->netwm.opacity, ec->netwm.opacity, ec->netwm.opacity, ec->netwm.opacity);
   evas_object_data_del(ec->frame, QP_DATA_KEY);

   free(md);
}

static void
_quickpanel_smart_show(Evas_Object *obj)
{
   INTERNAL_ENTRY;

   evas_object_show(md->qp_layout_obj);
}

static void
_quickpanel_smart_hide(Evas_Object *obj)
{
   INTERNAL_ENTRY;

   evas_object_hide(md->qp_layout_obj);
}

static void
_quickpanel_smart_move(Evas_Object *obj, int x, int y)
{
   INTERNAL_ENTRY;

   evas_object_move(md->qp_layout_obj, x, y);
}

static void
_quickpanel_smart_resize(Evas_Object *obj, int w, int h)
{
   INTERNAL_ENTRY;

   e_layout_virtual_size_set(md->qp_layout_obj, w, h);
   evas_object_resize(md->qp_layout_obj, w, h);
}

static void
_quickpanel_smart_init(void)
{
   if (_quickpanel_smart) return;
   {
      static const Evas_Smart_Class sc =
      {
         SMART_NAME,
         EVAS_SMART_CLASS_VERSION,
         _quickpanel_smart_add,
         _quickpanel_smart_del,
         _quickpanel_smart_move,
         _quickpanel_smart_resize,
         _quickpanel_smart_show,
         _quickpanel_smart_hide,
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
      _quickpanel_smart = evas_smart_class_new(&sc);
   }
}

static Eina_Bool
_quickpanel_mover_handler_move(Mover_Data *md, int x, int y)
{
   // angle 0 case
   // do not move handler out of screen.
   if ((y + md->handler_geo.h) > md->ec->zone->h) return EINA_FALSE;

   // angle 0 case
   md->handler_geo.y = y;

   e_layout_child_resize(md->base_clip, md->ec->w, md->handler_geo.y); // base clip resize

   e_layout_child_move(md->handler_mirror_obj, md->handler_geo.x, md->handler_geo.y - md->ec->h + md->handler_geo.h); // handler mirror object move
   e_layout_child_move(md->handler_clip, md->handler_geo.x, md->handler_geo.y); // handler mirror object move

   return EINA_TRUE;
}

static Evas_Object *
_quickpanel_mover_begin(E_Client *ec, int x, int y, unsigned int timestamp)
{
   Evas_Object *mover;
   Mover_Data *md;

   EINA_SAFETY_ON_NULL_RETURN_VAL(_pol_quickpanel, NULL);

   if (!ec->frame) return NULL; // if quickpanel's frame object is not exist then return;

   if (evas_object_data_get(ec->frame, QP_DATA_KEY))
     {
        DBG("Already used mover object");
        return NULL;
     }

   _quickpanel_smart_init();
   mover = evas_object_smart_add(e_comp->evas, _quickpanel_smart);

   md = evas_object_smart_data_get(mover);
   md->ec = ec;
   md->handler_geo.w = _pol_quickpanel->handler.rect.w;
   md->handler_geo.h = _pol_quickpanel->handler.rect.h;
   md->effect_info.y = y;
   md->effect_info.timestamp = timestamp;

   evas_object_data_set(mover, "E_Client", ec);
   evas_object_data_set(ec->frame, QP_DATA_KEY, mover);

   evas_object_move(mover, 0, 0); // 0 angle case
   evas_object_resize(mover, ec->w, ec->h);
   evas_object_show(mover);

   _quickpanel_mover_handler_move(md, x, y);

   return mover;
}

static Eina_Bool
_quickpanel_mover_move(Evas_Object *mover, int x, int y, unsigned int timestamp)
{
   Mover_Data *md;
   int dy;
   unsigned int dt;

   if (!mover) return EINA_FALSE;

   md = evas_object_smart_data_get(mover);
   if (!_quickpanel_mover_handler_move(md, x, y)) return EINA_FALSE;

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

static Elm_Transit_Effect *
_quickpanel_mover_effect_data_new(Evas_Object *mover, int from_y, int to_y, Eina_Bool visible)
{
   Mover_Effect_Data *ed;

   ed = E_NEW(Mover_Effect_Data, 1);
   if (!ed) return NULL;

   ed->mover = mover;
   ed->visible = visible;
   ed->from_dy = from_y;
   ed->to_dy = to_y;

   return ed;
}

static void
_quickpanel_mover_effect_data_free(Elm_Transit_Effect *effect, Elm_Transit *transit)
{
   E_Client *ec;
   Mover_Effect_Data *ed = effect;
   int pos;

   ec = evas_object_data_get(ed->mover, "E_Client");
   pos = (ed->visible) ? 0 : -10000;
   evas_object_move(ec->frame, pos, pos);

   evas_object_hide(ed->mover);
   evas_object_del(ed->mover);

   free(ed);
}

static void
_quickpanel_mover_effect_op(Elm_Transit_Effect *effect, Elm_Transit *transit, double progress)
{
   Mover_Effect_Data *ed = effect;
   Mover_Data *md;
   int new_y;

   md = evas_object_smart_data_get(ed->mover);
   new_y = ed->from_dy + (ed->to_dy * progress);
   _quickpanel_mover_handler_move(md, 0, new_y);
}

static void
_quickpanel_mover_effect_begin(Evas_Object *mover, int from_y, Eina_Bool visible)
{
   Elm_Transit *trans;
   Elm_Transit_Effect *effect;
   Mover_Data *md;
   int to_y;
   double duration;
   const double ref = 0.1;

   trans = elm_transit_add();
   elm_transit_object_add(trans, mover);
   elm_transit_tween_mode_set(trans, ELM_TRANSIT_TWEEN_MODE_DECELERATE);

   /* determine the position as a destination */
   to_y = (visible) ? (QP_EC->zone->h - from_y) : (-from_y);

   /* determine the transit's duration */
   duration = ((double)abs(to_y) / (QP_EC->zone->h / 2)) * ref;
   elm_transit_duration_set(trans, duration);

   /* create and add effect to transit */
   effect = _quickpanel_mover_effect_data_new(mover, from_y, to_y, visible);
   elm_transit_effect_add(trans, _quickpanel_mover_effect_op, effect, _quickpanel_mover_effect_data_free);

   /* start transit */
   elm_transit_go(trans);

   md = evas_object_smart_data_get(mover);
   md->effect_info.transit = trans;
}

static void
_quickpanel_mover_end(Evas_Object *mover, int x, int y, unsigned int timestamp)
{
   E_Client *ec;
   Mover_Data *md;
   Eina_Bool visible = EINA_FALSE;
   const float sensitivity = 1.5; /* hard coded. (arbitrary) */

   EINA_SAFETY_ON_NULL_RETURN(mover);

   md = evas_object_smart_data_get(mover);
   ec = evas_object_data_get(mover, "E_Client");

   if ((md->effect_info.accel > sensitivity) ||
       ((md->effect_info.accel > -sensitivity) && (y > ec->zone->h / 2)))
     visible = EINA_TRUE;

   /* Start show/hide effect */
   _quickpanel_mover_effect_begin(mover, y, visible);
}

static void
_quickpanel_mover_free(Evas_Object *mover)
{
   Mover_Data *md;

   md = evas_object_smart_data_get(mover);

   if (md->effect_info.transit)
     {
        /* NOTE: the mover will be deleted when effect data is freed. */
        elm_transit_del(md->effect_info.transit);
     }
   else
     {
        evas_object_hide(mover);
        evas_object_del(mover);
     }
}

static void
_quickpanel_data_free(void)
{
   Evas_Object *mover;

   if (!_pol_quickpanel)
     return;

   mover = evas_object_data_get(QP_EC->frame, QP_DATA_KEY);
   if (mover)
     _quickpanel_mover_free(mover);

   E_FREE_FUNC(_pol_quickpanel->handler.obj, e_mod_quickpanel_handler_object_del);
   E_FREE_LIST(_quickpanel_hooks, e_client_hook_del);
   E_FREE(_pol_quickpanel);
}

static void
_quickpanel_hook_client_del(void *d EINA_UNUSED, E_Client *ec)
{
   if (!ec) return;
   if (!_pol_quickpanel) return;
   if (QP_EC != ec) return;

   _quickpanel_data_free();
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
        _quickpanel_data_free();
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

   ELOGF("QUICKPANEL", "Set Client | ec %p", NULL, NULL, ec);

   eina_stringshare_replace(&ec->icccm.window_role, "quickpanel");

   /* maximizing will prevent to move the object in comp's intercept policy,
    * so we should unmaximize it to move object to out of screen. */
   e_client_unmaximize(ec, E_MAXIMIZE_BOTH);

   /* stacking it on the lock_screen, set layer to notification-high. */
   evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_HIGH);

   /* since we unmaximized it,
    * so we should resize it directly as fullscreen. */
   evas_object_resize(ec->frame, ec->zone->w, ec->zone->h);

   E_CLIENT_HOOK_APPEND(_quickpanel_hooks, E_CLIENT_HOOK_DEL,
                        _quickpanel_hook_client_del, NULL);

   _pol_quickpanel = pol_qp;
}

EINTERN E_Client *
e_mod_quickpanel_client_get(void)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(_pol_quickpanel, NULL);

   return QP_EC;
}

static void _quickpanel_handler_cb_mouse_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *handler, void *event);

static void
_quickpanel_handler_cb_mouse_move(void *data EINA_UNUSED, Evas *evas EINA_UNUSED, Evas_Object *handler, void *event)
{
   Evas_Event_Mouse_Move *ev = event;
   Evas_Object *mover;
   Flick_Info *fi = data;
   int dy;
   int hx, hy, hw, hh;
   unsigned int dt;
   float vel = 0.0;
   /* FIXME: hard coded, it sould be configurable. */
   const float sensitivity = 0.25;

   if (!fi->active)
     {
        evas_object_geometry_get(handler, &hx, &hy, &hw, &hh);
        if (!E_INSIDE(ev->cur.canvas.x, ev->cur.canvas.y, hx, hy, hw, hh))
          goto fin_ev;

        dy = ev->cur.canvas.y - fi->y;
        dt = ev->timestamp - fi->timestamp;
        if (dt) vel = (float)dy / (float)dt;

        if (fabs(vel) < sensitivity)
          return;

        fi->active = EINA_TRUE;
     }

   mover = evas_object_data_get(handler, QP_DATA_KEY);
   if (!mover)
     {
        mover = _quickpanel_mover_begin(QP_EC, 0, ev->cur.canvas.y, ev->timestamp);
        if (!mover)
          goto fin_ev;

        evas_object_data_set(handler, QP_DATA_KEY, mover);

        return;
     }

   _quickpanel_mover_move(mover, 0, ev->cur.canvas.y, ev->timestamp);

   return;
fin_ev:
   evas_object_event_callback_del(handler, EVAS_CALLBACK_MOUSE_MOVE,
                                  _quickpanel_handler_cb_mouse_move);
   evas_object_event_callback_del(handler, EVAS_CALLBACK_MOUSE_UP,
                                  _quickpanel_handler_cb_mouse_up);

   free(fi);
}

static void
_quickpanel_handler_cb_mouse_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *handler, void *event)
{
   Evas_Event_Mouse_Up *ev = event;
   Evas_Object *mover;
   Flick_Info *fi = data;

   mover = evas_object_data_get(handler, QP_DATA_KEY);
   if (!mover)
     {
        DBG("Could not find quickpanel mover object");
        goto end;
     }

   _quickpanel_mover_end(mover, 0, ev->canvas.y, ev->timestamp);

   evas_object_data_del(handler, QP_DATA_KEY);
end:
   evas_object_event_callback_del(handler, EVAS_CALLBACK_MOUSE_MOVE,
                                  _quickpanel_handler_cb_mouse_move);
   evas_object_event_callback_del(handler, EVAS_CALLBACK_MOUSE_UP,
                                  _quickpanel_handler_cb_mouse_up);

   free(fi);
}

static void
_quickpanel_handler_cb_mouse_down(void *data, Evas *evas EINA_UNUSED, Evas_Object *handler, void *event)
{
   Evas_Event_Mouse_Down *ev = event;
   Flick_Info *fi;

   fi = E_NEW(Flick_Info, 1);
   fi->y = ev->canvas.y;
   fi->timestamp = ev->timestamp;

   evas_object_event_callback_add(handler, EVAS_CALLBACK_MOUSE_MOVE,
                                  _quickpanel_handler_cb_mouse_move, fi);
   evas_object_event_callback_add(handler, EVAS_CALLBACK_MOUSE_UP,
                                  _quickpanel_handler_cb_mouse_up, fi);
}

EINTERN Evas_Object *
e_mod_quickpanel_handler_object_add(int x, int y, int w, int h)
{
   Evas_Object *handler;

   EINA_SAFETY_ON_NULL_RETURN_VAL(_pol_quickpanel, NULL);

   handler = evas_object_rectangle_add(evas_object_evas_get(QP_EC->frame));

   /* make it transparent */
   evas_object_color_set(handler, 0, 0, 0, 0);

   evas_object_repeat_events_set(handler, EINA_TRUE);

   evas_object_move(handler, x, y);
   evas_object_resize(handler, w, h);

   evas_object_event_callback_add(handler, EVAS_CALLBACK_MOUSE_DOWN,
                                  _quickpanel_handler_cb_mouse_down, NULL);

   return handler;
}

EINTERN void
e_mod_quickpanel_handler_object_del(Evas_Object *handler)
{
   Evas_Object *mover;

   mover = evas_object_data_get(handler, QP_DATA_KEY);
   if (mover)
     _quickpanel_mover_free(mover);

   evas_object_del(handler);
}

static void
_quickpanel_client_evas_cb_show(void *data, Evas *evas, Evas_Object *qp_obj, void *event)
{
   Evas_Object *handler = data;

   evas_object_show(handler);
}

static void
_quickpanel_client_evas_cb_hide(void *data, Evas *evas, Evas_Object *qp_obj, void *event)
{
   Evas_Object *handler = data;

   evas_object_hide(handler);
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
   evas_object_raise(handler);
}

EINTERN Eina_Bool
e_mod_quickpanel_handler_region_set(int x, int y, int w, int h)
{
   Evas_Object *handler;

   EINA_SAFETY_ON_NULL_RETURN_VAL(_pol_quickpanel, EINA_FALSE);

   ELOGF("QUICKPANEL", "Handler Geo Set | x %d, y %d, w %d, h %d",
         NULL, NULL, x, y, w, h);

   handler = e_mod_quickpanel_handler_object_add(QP_EC->client.x + x,
                                                 QP_EC->client.y + y,
                                                 w, h);

   /* Add handler object to smart member to follow the client's stack */
   evas_object_smart_member_add(handler, QP_EC->frame);
   evas_object_propagate_events_set(handler, 0);

   if (evas_object_visible_get(QP_EC->frame))
     evas_object_show(handler);

   evas_object_event_callback_add(QP_EC->frame, EVAS_CALLBACK_SHOW,
                                  _quickpanel_client_evas_cb_show, handler);
   evas_object_event_callback_add(QP_EC->frame, EVAS_CALLBACK_HIDE,
                                  _quickpanel_client_evas_cb_hide, handler);
   evas_object_event_callback_add(QP_EC->frame, EVAS_CALLBACK_MOVE,
                                  _quickpanel_client_evas_cb_move, handler);

   EINA_RECTANGLE_SET(&_pol_quickpanel->handler.rect, x, y, w, h);
   _pol_quickpanel->handler.obj = handler;

   return EINA_TRUE;
}

EINTERN void
e_mod_quickpanel_show(void)
{
   Evas_Object *mover;

   EINA_SAFETY_ON_NULL_RETURN(_pol_quickpanel);

   mover = evas_object_data_get(QP_EC->frame, QP_DATA_KEY);
   if (mover)
     _quickpanel_mover_free(mover);

   evas_object_move(QP_EC->frame, 0, 0);
}

EINTERN void
e_mod_quickpanel_hide(void)
{
   Evas_Object *mover;

   EINA_SAFETY_ON_NULL_RETURN(_pol_quickpanel);

   mover = evas_object_data_get(QP_EC->frame, QP_DATA_KEY);
   if (mover)
     _quickpanel_mover_free(mover);

   evas_object_move(QP_EC->frame, -10000, -10000);
}
