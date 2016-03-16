#include "e_mod_main.h"

typedef struct _Pol_Stack Pol_Stack;

struct _Pol_Stack
{
   E_Client *ec;

   struct
     {
        Ecore_Window win;
        Eina_Bool fetched;
     } transient;
};

static Eina_Hash *hash_pol_stack = NULL;

static void
_pol_stack_cb_data_free(void *data)
{
   E_FREE(data);
}

Pol_Stack*
_pol_stack_data_add(E_Client *ec)
{
   Pol_Stack *ps;

   if ((ps = eina_hash_find(hash_pol_stack, &ec)))
     return ps;

   ps = E_NEW(Pol_Stack, 1);
   if (!ps) return NULL;

   ps->ec = ec;
   eina_hash_add(hash_pol_stack, &ec, ps);

   return ps;
}

void
_pol_stack_data_del(E_Client *ec)
{
   Pol_Stack *ps;

   if ((ps = eina_hash_find(hash_pol_stack, &ec)))
     {
        eina_hash_del_by_key(hash_pol_stack, &ec);
     }
}

void
_pol_stack_transient_for_apply(E_Client *ec)
{
   int raise;
   E_Client *child, *top;
   Eina_List *l;

   if (ec->parent->layer != ec->layer)
     {
        raise = e_config->transient.raise;

        ec->saved.layer = ec->layer;
        ec->layer = ec->parent->layer;
        if (e_config->transient.layer)
          {
             e_config->transient.raise = 1;
             EINA_LIST_FOREACH(ec->transients, l, child)
               {
                  if (!child) continue;
                  child->saved.layer = child->layer;
                  child->layer = ec->parent->layer;
               }
          }

        e_config->transient.raise = raise;
     }

   if (ec->transient_policy == E_TRANSIENT_ABOVE)
     {
        top = e_client_top_get();
        while (top)
          {
             if ((!ec->parent->transients) || (top == ec->parent))
               {
                  top = NULL;
                  break;
               }
             if ((top != ec) && (eina_list_data_find(ec->parent->transients, top)))
               break;

             top = e_client_below_get(top);
          }

        if (top)
          evas_object_stack_above(ec->frame, top->frame);
        else
          evas_object_stack_above(ec->frame, ec->parent->frame);
     }
   else if (ec->transient_policy == E_TRANSIENT_BELOW)
     {
        evas_object_stack_below(ec->frame, ec->parent->frame);
     }
}

Eina_Bool
_pol_stack_transient_for_tree_check(E_Client *child, E_Client *parent)
{
   E_Client *p;

   p = parent->parent;
   while (p)
     {
        if (e_object_is_del(E_OBJECT(p))) return EINA_FALSE;
        if (p == child) return EINA_TRUE;

        p = p->parent;
     }

   return EINA_FALSE;
}

void
e_mod_pol_stack_hook_pre_post_fetch(E_Client *ec)
{
   Pol_Stack *ps;
   ps = eina_hash_find(hash_pol_stack, &ec);

   if (ps)
     {
        if ((ps->transient.win) && (ps->transient.fetched))
          {
#ifndef HAVE_WAYLAND_ONLY
             if ((ec->icccm.transient_for == ps->transient.win) &&
                 (ec->parent))
#else
             if (ec->parent && ps->transient.win)
#endif
               _pol_stack_transient_for_apply(ec);
             else
               ps->transient.win = ec->icccm.transient_for;

             ps->transient.fetched = 0;
          }
     }
}

void
e_mod_pol_stack_hook_pre_fetch(E_Client *ec)
{
   Pol_Stack *ps;
   ps = eina_hash_find(hash_pol_stack, &ec);

   if (ec->icccm.fetch.transient_for)
     {
        Ecore_Window transient_for_win = 0;
        E_Client *parent = NULL;
        Eina_Bool transient_each_other = EINA_FALSE;

#ifdef HAVE_WAYLAND_ONLY
        parent = e_pixmap_find_client(E_PIXMAP_TYPE_WL, ec->icccm.transient_for);
#endif

        if (parent)
          {
             if (!ps) ps = _pol_stack_data_add(ec);

#ifdef HAVE_WAYLAND_ONLY
             ps->transient.win = e_client_util_win_get(parent);
#endif
             ps->transient.fetched = 1;

             /* clients transient for each other */
             transient_each_other = _pol_stack_transient_for_tree_check(ec, parent);
             if (transient_each_other)
               {
                  ec->icccm.transient_for = transient_for_win;
                  ec->icccm.fetch.transient_for = 0;
                  ps->transient.fetched = 0;
                  parent = NULL;
               }
          }
     }
}

void
e_mod_pol_stack_transient_for_set(E_Client *child, E_Client *parent)
{
   Ecore_Window pwin = 0;

   EINA_SAFETY_ON_NULL_RETURN(child);

   if (!parent)
     {
        child->icccm.fetch.transient_for = EINA_FALSE;
        child->icccm.transient_for = 0;
        if (child->parent)
          {
             child->parent->transients =
                eina_list_remove(child->parent->transients, child);
             if (child->parent->modal == child) child->parent->modal = NULL;
             child->parent = NULL;
          }
        return;
     }

   pwin = e_client_util_win_get(parent);

   /* If we already have a parent, remove it */
   if (child->parent)
     {
        if (parent != child->parent)
          {
             child->parent->transients =
                eina_list_remove(child->parent->transients, child);
             if (child->parent->modal == child) child->parent->modal = NULL;
             child->parent = NULL;
          }
        else
          parent = NULL;
     }

   if ((parent) && (parent != child) &&
       (eina_list_data_find(parent->transients, child) != child))
     {
        parent->transients = eina_list_append(parent->transients, child);
        child->parent = parent;
     }

   child->icccm.fetch.transient_for = EINA_TRUE;
   child->icccm.transient_for = pwin;
}

void
e_mod_pol_stack_cb_client_remove(E_Client *ec)
{
   _pol_stack_data_del(ec);
}

void
e_mod_pol_stack_shutdonw(void)
{
   eina_hash_free(hash_pol_stack);
   hash_pol_stack = NULL;
}

void
e_mod_pol_stack_init(void)
{
   hash_pol_stack = eina_hash_pointer_new(_pol_stack_cb_data_free);
}

void
e_mod_pol_stack_below(E_Client *ec, E_Client *below_ec)
{
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   EINA_SAFETY_ON_NULL_RETURN(below_ec);
   EINA_SAFETY_ON_NULL_RETURN(below_ec->frame);

   evas_object_stack_below(ec->frame, below_ec->frame);
   if (e_config->transient.iconify)
     {
        E_Client *child;
        Eina_List *list = eina_list_clone(ec->transients);

        EINA_LIST_FREE(list, child)
          {
             e_mod_pol_stack_below(child, below_ec);
          }
     }
}
