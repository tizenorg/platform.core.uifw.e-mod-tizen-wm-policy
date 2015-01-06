#include "e_mod_atoms.h"

Ecore_X_Atom E_MOD_POL_ATOM_WINDOW_OPAQUE = 0;

Eina_Bool
e_mod_pol_atoms_init(void)
{
   Eina_Bool ret = EINA_FALSE;
   E_MOD_POL_ATOM_WINDOW_OPAQUE = ecore_x_atom_get ("_E_ILLUME_WINDOW_REGION_OPAQUE");

   if (!E_MOD_POL_ATOM_WINDOW_OPAQUE)
     fprintf (stderr, "[e-mod-tizen-wm-policy] Critical Error!!! Cannot create _E_ILLUME_WINDOW_REGION_OPAQUE Atom.\n");
   else
     ret = EINA_TRUE;

   return ret;
}
