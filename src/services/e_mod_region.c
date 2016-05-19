#include "e_mod_main.h"
#include "e_mod_rotation.h"
#include "e_mod_gesture.h"
#include "e_mod_region.h"

/* FIXME: temporary use quickpanel to find out ui orientation */
#include "e_mod_quickpanel.h"

#define ENTRY(...)                                 \
   Pol_Region *region;                             \
   EINA_SAFETY_ON_NULL_RETURN_VAL(ro, EINA_FALSE); \
   region = evas_object_data_get(ro, EO_DATA_KEY); \
   if (EINA_UNLIKELY(!region))                     \
     return __VA_ARGS__

/* FIXME: Implementation for log that can access commonly */
#ifdef INF
#undef INF
#endif

#define INF(f, x...) NULL

#define EO_DATA_KEY  "pol-region"

struct _Pol_Region
{
   Evas_Object       *obj;
   Pol_Gesture       *gesture;
   Eina_List         *event_list;
   Eina_Rectangle     geom[ROT_IDX_NUM];
   Rot_Idx            rotation;
};

static void
_region_rotation_set(Pol_Region *region, int angle)
{
   if (!e_mod_rotation_angle_valid_check(angle))
     return;

   region->rotation = e_mod_rotation_angle_to_idx(angle);
}

static void
_region_obj_geometry_update(Pol_Region *region)
{
   Rot_Idx r;

   r = region->rotation;

   INF("Update Geometry: rotation %d x %d y %d w %d h %d",
       e_mod_rotation_idx_to_angle(r), region->geom[r].x, region->geom[r].y, region->geom[r].w, region->geom[r].h);

   evas_object_geometry_set(region->obj,
                            region->geom[r].x, region->geom[r].y,
                            region->geom[r].w, region->geom[r].h);
}

static Eina_Bool
_region_rotation_cb_change_end(void *data, int type, void *event)
{
   Pol_Region *region;
   E_Event_Client *ev;
   E_Client *ec;

   region = data;
   if (EINA_UNLIKELY(!region))
     goto end;

   ev = event;
   if (EINA_UNLIKELY(!ev))
     goto end;

   ec = ev->ec;
   if (EINA_UNLIKELY(!ec))
     goto end;

   if (e_mod_quickpanel_client_get() != ec)
     goto end;

   _region_rotation_set(region, ec->e.state.rot.ang.curr);
   _region_obj_geometry_update(region);

end:
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_region_rotation_init(Pol_Region *region)
{
   E_Client *ec;

   /* FIXME: temporary use quickpanel to find out ui orientation */
   ec = e_mod_quickpanel_client_get();
   if (ec)
     _region_rotation_set(region, ec->e.state.rot.ang.curr);

   E_LIST_HANDLER_APPEND(region->event_list, E_EVENT_CLIENT_ROTATION_CHANGE_END, _region_rotation_cb_change_end, region);

   return EINA_TRUE;
}

static void
_region_free(Pol_Region *region)
{
   INF("Free Instant");
   E_FREE_LIST(region->event_list, ecore_event_del);
   E_FREE_FUNC(region->gesture, e_mod_gesture_del);
   E_FREE_FUNC(region->obj, evas_object_del);
   free(region);
}

static void
_region_object_cb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Pol_Region *region;

   region = data;
   if (EINA_UNLIKELY(!region))
     return;

   _region_free(region);
}

EINTERN Evas_Object *
e_mod_region_object_new(void)
{
   Pol_Region *region;
   Evas_Object *o;

   INF("New Instant");

   region = calloc(1, sizeof(*region));
   if (!region)
     return NULL;

   o = evas_object_rectangle_add(e_comp->evas);
   evas_object_color_set(o, 0, 0, 0, 0);
   region->obj = o;

   if (!_region_rotation_init(region))
     goto err_event;

   evas_object_data_set(o, EO_DATA_KEY, region);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _region_object_cb_del, region);

   return o;
err_event:
   evas_object_del(o);
   free(region);

   return NULL;
}

EINTERN Eina_Bool
e_mod_region_cb_set(Evas_Object *ro, Pol_Gesture_Start_Cb cb_start, Pol_Gesture_Move_Cb cb_move, Pol_Gesture_End_Cb cb_end, void *data)
{
   Pol_Gesture *gesture;

   ENTRY(EINA_FALSE);

   INF("Set Callback function");
   if (!region->gesture)
     {
        gesture = e_mod_gesture_add(ro, POL_GESTURE_TYPE_LINE);
        if (!gesture)
          return EINA_FALSE;

        region->gesture = gesture;
     }

   e_mod_gesture_cb_set(region->gesture, cb_start, cb_move, cb_end, data);

   return EINA_TRUE;
}

EINTERN Eina_Bool
e_mod_region_rectangle_set(Evas_Object *ro, Rot_Idx ridx, int x, int y, int w, int h)
{
   ENTRY(EINA_FALSE);

   INF("Add Rectangle: a %d x %d y %d w %d h %d",
       e_mod_rotation_idx_to_angle(ridx), x, y, w, h);

   EINA_RECTANGLE_SET(&region->geom[ridx], x, y, w, h);

   if (ridx == region->rotation)
     _region_obj_geometry_update(region);

   return EINA_TRUE;
}

EINTERN Eina_Bool
e_mod_region_rectangle_get(Evas_Object *ro, Rot_Idx ridx, int *x, int *y, int *w, int *h)
{
   ENTRY(EINA_FALSE);

   if (x) *x = region->geom[ridx].x;
   if (y) *y = region->geom[ridx].y;
   if (w) *w = region->geom[ridx].w;
   if (h) *h = region->geom[ridx].h;

   return EINA_TRUE;
}
