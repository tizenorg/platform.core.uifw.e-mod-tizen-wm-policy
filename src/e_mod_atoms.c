#include "e_mod_atoms.h"
#include "e_mod_utils.h"

Ecore_X_Atom E_MOD_POL_ATOM_WINDOW_OPAQUE = 0;
Ecore_X_Atom E_MOD_POL_ATOM_NOTIFICATION_LEVEL = 0;

static const char *atom_names[] = {
  "_E_ILLUME_WINDOW_REGION_OPAQUE",
  "_E_ILLUME_NOTIFICATION_LEVEL",
};

Eina_Bool
e_mod_pol_atoms_init(void)
{
   Ecore_X_Atom *atoms = NULL;
   int n = 0, i = 0;
   Eina_Bool res = EINA_FALSE;

   n = (sizeof(atom_names) / sizeof(char *));

   atoms = E_NEW(Ecore_X_Atom, n);
   E_CHECK_GOTO(atoms, cleanup);

   ecore_x_atoms_get(atom_names, n, atoms);

   E_MOD_POL_ATOM_WINDOW_OPAQUE                        = atoms[i++];
   E_MOD_POL_ATOM_NOTIFICATION_LEVEL                   = atoms[i++];

   res = EINA_TRUE;

cleanup:
   if (atoms) E_FREE(atoms);
   return res;
}
