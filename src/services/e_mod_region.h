#ifndef E_MOD_POL_GESTURE_REGION
#define E_MOD_POL_GESTURE_REGION

#include "e_mod_rotation.h"
#include "e_mod_gesture.h"

typedef struct _Pol_Region Pol_Region;

EINTERN Evas_Object        *e_mod_region_object_new(void);
EINTERN Eina_Bool           e_mod_region_rectangle_set(Evas_Object *ro, Rot_Idx ridx, int x, int y, int w, int h);
EINTERN Eina_Bool           e_mod_region_rectangle_get(Evas_Object *ro, Rot_Idx ridx, int *x, int *y, int *w, int *h);
EINTERN Eina_Bool           e_mod_region_cb_set(Evas_Object *ro, Pol_Gesture_Start_Cb cb_start, Pol_Gesture_Move_Cb cb_move, Pol_Gesture_End_Cb cb_end, void *data);

#endif
