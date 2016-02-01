static inline Eina_Bool
e_mod_rotation_angle_valid_check(int angle)
{
   return ((angle >= 0) && (angle <= 270) && !(angle % 90));
}

static inline Rot_Idx
e_mod_rotation_angle_to_idx(int angle)
{
   if (!e_mod_rotation_angle_valid_check(angle))
     return -1;

   return angle / 90;
}

static inline int
e_mod_rotation_idx_to_angle(int idx)
{
   if ((idx < ROT_IDX_0) || (idx > ROT_IDX_NUM))
     return -1;

   return idx * 90;
}
