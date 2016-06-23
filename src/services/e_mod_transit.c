/* this source code is clone of elm_transit */

#include <Ecore.h>

#include "e_mod_main.h"
#include "e_mod_transit.h"

struct _Pol_Transit
{
   Ecore_Animator *animator;
   Eina_Inlist *effect_list;
   Eina_List *objs;
   Pol_Transit_Tween_Mode tween_mode;
   struct
   {
      Pol_Transit_Del_Cb func;
      void *arg;
   } del_data;
   struct
   {
      double delayed;
      double paused;
      double duration;
      double begin;
      double current;
   } time;
   struct
   {
      int count;
      int current;
      Eina_Bool reverse;
   } repeat;
   double progress;
   unsigned int effects_pending_del;
   int walking;
   double v[4];
   Eina_Bool auto_reverse : 1;
   Eina_Bool event_enabled : 1;
   Eina_Bool deleted : 1;
   Eina_Bool finished : 1;
   Eina_Bool smooth : 1;
};

struct _Pol_Transit_Effect_Module
{
   EINA_INLIST;
   Pol_Transit_Effect_Transition_Cb transition_cb;
   Pol_Transit_Effect_End_Cb end_cb;
   Pol_Transit_Effect *effect;
   Eina_Bool deleted : 1;
};

struct _Pol_Transit_Obj_Data
{
   struct {
      Evas_Coord x, y, w, h;
      int r,g,b,a;
      Evas_Map *map;
      Eina_Bool map_enabled : 1;
      Eina_Bool visible : 1;
      Eina_Bool freeze_events : 1;
   } state;
   int ref;
};

typedef struct _Pol_Transit_Effect_Module Pol_Transit_Effect_Module;
typedef struct _Pol_Transit_Obj_Data Pol_Transit_Obj_Data;

static void _transit_obj_remove_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED);

static void
_remove_obj_from_list(Pol_Transit *transit, Evas_Object *obj)
{
   //Remove duplicated objects
   //TODO: Need to consider about optimizing here
   while (1)
     {
        if (!eina_list_data_find_list(transit->objs, obj))
          break;
        transit->objs = eina_list_remove(transit->objs, obj);
        evas_object_event_callback_del_full(obj, EVAS_CALLBACK_DEL,
                                       _transit_obj_remove_cb,
                                       transit);
     }
}

static void
_transit_obj_remove_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Pol_Transit *transit = data;
   _remove_obj_from_list(transit, obj);
   if (!transit->objs && !transit->deleted) pol_transit_del(transit);
}

static void
_transit_obj_remove(Pol_Transit *transit, Evas_Object *obj)
{
   _remove_obj_from_list(transit, obj);
}

static void
_transit_effect_del(Pol_Transit *transit, Pol_Transit_Effect_Module *effect_module)
{
   if (effect_module->end_cb)
     effect_module->end_cb(effect_module->effect, transit);
   free(effect_module);
}

static void
_transit_del(Pol_Transit *transit)
{
   Pol_Transit_Effect_Module *effect_module;

   transit->deleted = EINA_TRUE;

   ecore_animator_del(transit->animator);
   transit->animator = NULL;

   //remove effects
   while (transit->effect_list)
     {
        effect_module = EINA_INLIST_CONTAINER_GET(transit->effect_list, Pol_Transit_Effect_Module);
        transit->effect_list = eina_inlist_remove(transit->effect_list, transit->effect_list);
        _transit_effect_del(transit, effect_module);
     }

   //remove objects.
   while (transit->objs)
     _transit_obj_remove(transit, eina_list_data_get(transit->objs));

   if (transit->del_data.func)
     transit->del_data.func(transit->del_data.arg, transit);

   free(transit);
}

static void
_transit_remove_dead_effects(Pol_Transit *transit)
{
   Pol_Transit_Effect_Module *effect_module = NULL;
   Eina_Inlist *ll;

   EINA_INLIST_FOREACH_SAFE(transit->effect_list, ll, effect_module)
     {
        if (effect_module->deleted)
          {
             _transit_effect_del(transit, effect_module);
             transit->effects_pending_del--;
             if (!transit->effects_pending_del) return;
          }
     }
}

//If the transit is deleted then EINA_FALSE is retruned.
static Eina_Bool
_transit_animate_op(Pol_Transit *transit, double progress)
{
   Pol_Transit_Effect_Module *effect_module;

   transit->walking++;
   EINA_INLIST_FOREACH(transit->effect_list, effect_module)
     {
        if (transit->deleted) break;
        if (!effect_module->deleted)
          effect_module->transition_cb(effect_module->effect, transit, progress);
     }
   transit->walking--;

   if (transit->walking) return EINA_TRUE;

   if (transit->deleted)
     {
        _transit_del(transit);
        return EINA_FALSE;
     }

   else if (transit->effects_pending_del) _transit_remove_dead_effects(transit);

   return EINA_TRUE;
}

static Eina_Bool
_transit_animate_cb(void *data)
{
   Pol_Transit *transit = data;
   double elapsed_time, duration;

   transit->time.current = ecore_loop_time_get();
   elapsed_time = transit->time.current - transit->time.begin;
   duration = transit->time.duration + transit->time.delayed;

   if (elapsed_time > duration)
     elapsed_time = duration;

   transit->progress = elapsed_time / duration;
   switch (transit->tween_mode)
     {
      case POL_TRANSIT_TWEEN_MODE_LINEAR:
         transit->progress = ecore_animator_pos_map(transit->progress,
                                                    ECORE_POS_MAP_LINEAR,
                                                    0, 0);
         break;
      case POL_TRANSIT_TWEEN_MODE_ACCELERATE:
         transit->progress =
            ecore_animator_pos_map(transit->progress,
                                   ECORE_POS_MAP_ACCELERATE_FACTOR,
                                   transit->v[0], 0);
         break;
      case POL_TRANSIT_TWEEN_MODE_DECELERATE:
         transit->progress =
            ecore_animator_pos_map(transit->progress,
                                   ECORE_POS_MAP_DECELERATE_FACTOR,
                                   transit->v[0], 0);
         break;
      case POL_TRANSIT_TWEEN_MODE_SINUSOIDAL:
         transit->progress =
            ecore_animator_pos_map(transit->progress,
                                   ECORE_POS_MAP_SINUSOIDAL_FACTOR,
                                   transit->v[0], 0);
         break;
      case POL_TRANSIT_TWEEN_MODE_DIVISOR_INTERP:
         transit->progress = ecore_animator_pos_map(transit->progress,
                                                    ECORE_POS_MAP_DIVISOR_INTERP,
                                                    transit->v[0], transit->v[1]);
         break;
      case POL_TRANSIT_TWEEN_MODE_BOUNCE:
         transit->progress = ecore_animator_pos_map(transit->progress,
                                                    ECORE_POS_MAP_BOUNCE,
                                                    transit->v[0], transit->v[1]);
         break;
      case POL_TRANSIT_TWEEN_MODE_SPRING:
         transit->progress = ecore_animator_pos_map(transit->progress,
                                                    ECORE_POS_MAP_SPRING,
                                                    transit->v[0], transit->v[1]);
         break;
      case POL_TRANSIT_TWEEN_MODE_BEZIER_CURVE:
         transit->progress = ecore_animator_pos_map_n(transit->progress,
                                                      ECORE_POS_MAP_CUBIC_BEZIER,
                                                      4, transit->v);
         break;
      default:
         break;
     }

   /* Reverse? */
   if (transit->repeat.reverse) transit->progress = 1 - transit->progress;

   if (transit->time.duration > 0)
     {
        if (!_transit_animate_op(transit, transit->progress))
          return ECORE_CALLBACK_CANCEL;
     }

   /* Not end. Keep going. */
   if (elapsed_time < duration) return ECORE_CALLBACK_RENEW;

   /* Repeat and reverse and time done! */
   if ((transit->repeat.count >= 0) &&
       (transit->repeat.current == transit->repeat.count) &&
       ((!transit->auto_reverse) || transit->repeat.reverse))
     {
        transit->finished = EINA_TRUE;
        pol_transit_del(transit);
        return ECORE_CALLBACK_CANCEL;
     }

   /* Repeat Case */
   if (!transit->auto_reverse || transit->repeat.reverse)
     {
        transit->repeat.current++;
        transit->repeat.reverse = EINA_FALSE;
     }
   else transit->repeat.reverse = EINA_TRUE;

   transit->time.begin = ecore_loop_time_get();

   return ECORE_CALLBACK_RENEW;
}

EINTERN Pol_Transit *
pol_transit_add(void)
{
   Pol_Transit *transit;

   transit = calloc(1, sizeof(*transit));
   if (!transit)
     return NULL;

   pol_transit_tween_mode_set(transit, POL_TRANSIT_TWEEN_MODE_LINEAR);

   transit->v[0] = 1.0;
   transit->v[1] = 0.0;
   transit->smooth = EINA_TRUE;

   return transit;
}

EINTERN void
pol_transit_del(Pol_Transit *transit)
{
   if (!transit) return;

   transit->deleted = EINA_TRUE;
   if (transit->walking) return;
   _transit_del(transit);
}

EINTERN void
pol_transit_object_add(Pol_Transit *transit, Evas_Object *obj)
{
   if (EINA_UNLIKELY(!transit))
     return;

   if (transit->animator)
     {
        evas_object_freeze_events_set(obj, EINA_TRUE);
     }
   evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL,
                                  _transit_obj_remove_cb,
                                  transit);

   transit->objs = eina_list_append(transit->objs, obj);
}

EINTERN void
pol_transit_object_remove(Pol_Transit *transit, Evas_Object *obj)
{
   EINA_SAFETY_ON_NULL_RETURN(transit);
   EINA_SAFETY_ON_NULL_RETURN(obj);

   _transit_obj_remove(transit, obj);
   if (!transit->objs) pol_transit_del(transit);
}

EINTERN void
pol_transit_effect_add(Pol_Transit *transit, Pol_Transit_Effect_Transition_Cb transition_cb, Pol_Transit_Effect *effect, Pol_Transit_Effect_End_Cb end_cb)
{
   Pol_Transit_Effect_Module *effect_module;

   EINA_SAFETY_ON_NULL_RETURN(transit);
   EINA_SAFETY_ON_NULL_RETURN(transition_cb);

   EINA_INLIST_FOREACH(transit->effect_list, effect_module)
     if ((effect_module->transition_cb == transition_cb) && (effect_module->effect == effect))
       {
          WRN("pol_transit does not allow to add the duplicated effect! : transit=%p", transit);
          return;
       }

   effect_module = calloc(1, sizeof(*effect_module));
   if (!effect_module)
     {
        ERR("Failed to allocate a new effect!: transit=%p", transit);
        return;
     }

   effect_module->end_cb = end_cb;
   effect_module->transition_cb = transition_cb;
   effect_module->effect = effect;

   transit->effect_list = eina_inlist_append(transit->effect_list, (Eina_Inlist*) effect_module);
}

EINTERN void
pol_transit_effect_del(Pol_Transit *transit, Pol_Transit_Effect_Transition_Cb transition_cb, Pol_Transit_Effect *effect)
{
   Pol_Transit_Effect_Module *effect_module;

   EINA_SAFETY_ON_NULL_RETURN(transit);
   EINA_SAFETY_ON_NULL_RETURN(transition_cb);

   EINA_INLIST_FOREACH(transit->effect_list, effect_module)
     {
        if ((effect_module->transition_cb == transition_cb) && (effect_module->effect == effect))
          {
             if (transit->walking)
               {
                  effect_module->deleted = EINA_TRUE;
                  transit->effects_pending_del++;
               }
             else
               {
                  _transit_effect_del(transit, effect_module);
                  if (!transit->effect_list) pol_transit_del(transit);
               }
             return;
          }
     }
}

EINTERN void
pol_transit_smooth_set(Pol_Transit *transit, Eina_Bool smooth)
{
   EINA_SAFETY_ON_NULL_RETURN(transit);
   transit->smooth = !!smooth;
}

EINTERN Eina_Bool
pol_transit_smooth_get(const Pol_Transit *transit)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(transit, EINA_FALSE);
   return transit->smooth;
}

EINTERN void
pol_transit_tween_mode_set(Pol_Transit *transit, Pol_Transit_Tween_Mode tween_mode)
{
   EINA_SAFETY_ON_NULL_RETURN(transit);
   transit->tween_mode = tween_mode;
}

EINTERN void
pol_transit_duration_set(Pol_Transit *transit, double duration)
{
   EINA_SAFETY_ON_NULL_RETURN(transit);
   if (transit->animator)
     {
        WRN("pol_transit does not allow to set the duration time in operating! : transit=%p", transit);
        return;
     }
   transit->time.duration = duration;
}

EINTERN void
pol_transit_go(Pol_Transit *transit)
{
   EINA_SAFETY_ON_NULL_RETURN(transit);

   Eina_List *elist;
   Evas_Object *obj;

   ecore_animator_del(transit->animator);
   transit->animator = NULL;

   if (!transit->event_enabled)
     {
        EINA_LIST_FOREACH(transit->objs, elist, obj)
          evas_object_freeze_events_set(obj, EINA_TRUE);
     }

   transit->time.paused = 0;
   transit->time.delayed = 0;
   transit->time.begin = ecore_loop_time_get();
   transit->animator = ecore_animator_add(_transit_animate_cb, transit);

   _transit_animate_cb(transit);
}
