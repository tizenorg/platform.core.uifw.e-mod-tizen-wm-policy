#ifndef E_MOD_GESTURE
#define E_MOD_GESTURE

typedef struct _Pol_Gesture Pol_Gesture;

typedef enum
{
   POL_GESTURE_TYPE_NONE,
   POL_GESTURE_TYPE_LINE,
   POL_GESTURE_TYPE_FLICK,
} Pol_Gesture_Type;

typedef void (*Pol_Gesture_Start_Cb)(void *data, Evas_Object *obj, int x, int y, unsigned int timestamp);
typedef void (*Pol_Gesture_Move_Cb)(void *data, Evas_Object *obj, int x, int y, unsigned int timestamp);
typedef void (*Pol_Gesture_End_Cb)(void *data, Evas_Object *obj, int x, int y, unsigned int timestamp);

EINTERN Pol_Gesture  *e_mod_gesture_add(Evas_Object *obj, Pol_Gesture_Type type);
EINTERN void          e_mod_gesture_del(Pol_Gesture *gesture);
EINTERN void          e_mod_gesture_cb_set(Pol_Gesture *gesture, Pol_Gesture_Start_Cb cb_start, Pol_Gesture_Move_Cb cb_move, Pol_Gesture_End_Cb cb_end, void *data);

#endif /* E_MOD_GESTURE */
