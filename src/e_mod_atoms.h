#ifndef E_MOD_ATOMS_H
#define E_MOD_ATOMS_H
#include <e.h>

#ifndef HAVE_WAYLAND_ONLY
extern Ecore_X_Atom E_MOD_POL_ATOM_WINDOW_OPAQUE;
extern Ecore_X_Atom E_MOD_POL_ATOM_NOTIFICATION_LEVEL;
#endif

EINTERN Eina_Bool e_mod_pol_atoms_init(void);

#endif
