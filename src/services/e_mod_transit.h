#ifndef E_MOD_TRANSIT_H
#define E_MOD_TRANSIT_H

typedef enum
{
   POL_TRANSIT_TWEEN_MODE_LINEAR,
   POL_TRANSIT_TWEEN_MODE_SINUSOIDAL,
   POL_TRANSIT_TWEEN_MODE_DECELERATE,
   POL_TRANSIT_TWEEN_MODE_ACCELERATE,
   POL_TRANSIT_TWEEN_MODE_DIVISOR_INTERP,
   POL_TRANSIT_TWEEN_MODE_BOUNCE,
   POL_TRANSIT_TWEEN_MODE_SPRING,
   POL_TRANSIT_TWEEN_MODE_BEZIER_CURVE
} Pol_Transit_Tween_Mode;

typedef struct _Pol_Transit Pol_Transit;
typedef void                Pol_Transit_Effect;

typedef void (*Pol_Transit_Effect_Transition_Cb)(Pol_Transit_Effect *effect, Pol_Transit *transit, double progress);
typedef void (*Pol_Transit_Effect_End_Cb)(Pol_Transit_Effect *effect, Pol_Transit *transit);
typedef void (*Pol_Transit_Del_Cb)(void *data, Pol_Transit *transit);

EINTERN Pol_Transit  *pol_transit_add(void);
EINTERN void          pol_transit_del(Pol_Transit *transit);
EINTERN void          pol_transit_object_add(Pol_Transit *transit, Evas_Object *obj);
EINTERN void          pol_transit_object_remove(Pol_Transit *transit, Evas_Object *obj);
EINTERN void          pol_transit_smooth_set(Pol_Transit *transit, Eina_Bool smooth);
EINTERN Eina_Bool     pol_transit_smooth_get(const Pol_Transit *transit);
EINTERN void          pol_transit_tween_mode_set(Pol_Transit *transit, Pol_Transit_Tween_Mode tween_mode);
EINTERN void          pol_transit_duration_set(Pol_Transit *transit, double duration);
EINTERN void          pol_transit_go(Pol_Transit *transit);
EINTERN void          pol_transit_effect_add(Pol_Transit *transit, Pol_Transit_Effect_Transition_Cb transition_cb, Pol_Transit_Effect *effect, Pol_Transit_Effect_End_Cb end_cb);
EINTERN void          pol_transit_effect_del(Pol_Transit *transit, Pol_Transit_Effect_Transition_Cb transition_cb, Pol_Transit_Effect *effect);

#endif
