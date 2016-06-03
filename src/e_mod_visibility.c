#include "e_mod_main.h"
#ifdef HAVE_WAYLAND_ONLY
#include "e_comp_wl.h"
#include "e_mod_wl.h"
#endif

#ifdef ENABLE_TTRACE
# include <ttrace.h>
# undef TRACE_DS_BEGIN
# undef TRACE_DS_END

# define TRACE_DS_BEGIN(NAME) traceBegin(TTRACE_TAG_WINDOW_MANAGER, "DS:POL:"#NAME)
# define TRACE_DS_END() traceEnd(TTRACE_TAG_WINDOW_MANAGER)
#else
# define TRACE_DS_BEGIN(NAME)
# define TRACE_DS_END()
#endif

static Eina_Bool
_e_mod_pol_check_transient_child_visible(E_Client *ancestor_ec, E_Client *ec)
{
   Eina_Bool visible = EINA_FALSE;
   Eina_List *list = NULL;
   E_Client *child_ec = NULL;
   int anc_x, anc_y, anc_w, anc_h;
   int child_x, child_y, child_w, child_h;

   if (!ancestor_ec) return EINA_FALSE;

   e_client_geometry_get(ancestor_ec, &anc_x, &anc_y, &anc_w, &anc_h);

   list = eina_list_clone(ec->transients);
   EINA_LIST_FREE(list, child_ec)
     {
        if (visible == EINA_TRUE) continue;

        if (child_ec->exp_iconify.skip_iconify == EINA_TRUE)
          {
             if (child_ec->visibility.obscured == E_VISIBILITY_UNOBSCURED)
               {
                  return EINA_TRUE;
               }
             else
               {
                  if (!child_ec->iconic)
                    {
                       e_client_geometry_get(child_ec, &child_x, &child_y, &child_w, &child_h);
                       if (E_CONTAINS(child_x, child_y, child_w, child_h, anc_x, anc_y, anc_w, anc_h))
                         {
                            return EINA_TRUE;
                         }
                    }
               }
          }
        else
          {
             if ((!child_ec->iconic) ||
                 (child_ec->visibility.obscured == E_VISIBILITY_UNOBSCURED))
               {
                  return EINA_TRUE;
               }
          }

        visible = _e_mod_pol_check_transient_child_visible(ancestor_ec, child_ec);
     }

   return visible;
}

static void
_e_mod_pol_client_iconify_by_visibility(E_Client *ec)
{
   Eina_Bool do_iconify = EINA_TRUE;

   if (!ec) return;
   if (ec->iconic) return;
   if (ec->exp_iconify.by_client) return;
   if (ec->exp_iconify.skip_iconify) return;
#ifdef HAVE_WAYLAND_ONLY
   E_Comp_Wl_Client_Data *cdata = (E_Comp_Wl_Client_Data *)ec->comp_data;
   if (cdata && !cdata->mapped) return;
#endif

   if (e_config->transient.iconify)
     {
        if (_e_mod_pol_check_transient_child_visible(ec, ec))
          {
             do_iconify = EINA_FALSE;
          }
     }

   if (!do_iconify)
     {
        ELOGF("SKIP.. ICONIFY_BY_WM", "win:0x%08x", ec->pixmap, ec, e_client_util_win_get(ec));
        return;
     }

   ELOGF("ICONIFY_BY_WM", "win:0x%08x", ec->pixmap, ec, e_client_util_win_get(ec));
   e_mod_pol_wl_iconify_state_change_send(ec, 1);
   e_client_iconify(ec);

   /* if client has obscured parent, try to iconify the parent also */
   if (ec->parent)
     {
        if (ec->parent->visibility.obscured == E_VISIBILITY_FULLY_OBSCURED)
          _e_mod_pol_client_iconify_by_visibility(ec->parent);
     }
}

static void
_e_mod_pol_client_ancestor_uniconify(E_Client *ec)
{
   Eina_List *list = NULL;
   Eina_List *l = NULL;
   E_Client *parent = NULL;
   int transient_iconify = 0;
   int count = 0;

   if (!ec) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (!ec->iconic) return;
   if (ec->exp_iconify.by_client) return;
   if (ec->exp_iconify.skip_iconify) return;

   parent = ec->parent;
   while (parent)
     {
        if (count > 10)
          {
             // something strange state.
             ELOGF("CHECK transient_for tree", "win:0x%08x, parent:0x%08x", NULL, NULL, e_client_util_win_get(ec), e_client_util_win_get(parent));
             break;
          }

        if (e_object_is_del(E_OBJECT(parent))) break;
        if (!parent->iconic) break;
        if (parent->exp_iconify.by_client) break;
        if (parent->exp_iconify.skip_iconify) break;

        if (eina_list_data_find(list, parent))
          {
             // very bad. there are loop for parenting
             ELOGF("Very BAD. Circling transient_for window", "win:0x%08x, parent:0x%08x", NULL, NULL, e_client_util_win_get(ec), e_client_util_win_get(parent));
             break;
          }

        list = eina_list_prepend(list, parent);
        parent = parent->parent;

        // for preventing infiniting loop
        count++;
     }

   transient_iconify = e_config->transient.iconify;
   e_config->transient.iconify = 0;

   parent = NULL;
   EINA_LIST_FOREACH(list, l, parent)
     {
        ELOGF("UNICONIFY_BY_WM", "parent_win:0x%08x", parent->pixmap, parent, e_client_util_win_get(parent));
        parent->exp_iconify.not_raise = 1;
        e_client_uniconify(parent);
        e_mod_pol_wl_iconify_state_change_send(parent, 0);
     }
   eina_list_free(list);

   e_config->transient.iconify = transient_iconify;
}

static void
_e_mod_pol_client_uniconify_by_visibility(E_Client *ec)
{
   if (!ec) return;
   if (e_object_is_del(E_OBJECT(ec))) return;
   if (!ec->iconic) return;
   if (ec->exp_iconify.by_client) return;
   if (ec->exp_iconify.skip_iconify) return;

   E_Comp_Wl_Client_Data *cdata = (E_Comp_Wl_Client_Data *)ec->comp_data;
   if (cdata && !cdata->mapped) return;

   _e_mod_pol_client_ancestor_uniconify(ec);

   ELOGF("UNICONIFY_BY_WM", "win:0x%08x", ec->pixmap, ec, e_client_util_win_get(ec));
   ec->exp_iconify.not_raise = 1;
   e_client_uniconify(ec);
   e_mod_pol_wl_iconify_state_change_send(ec, 0);
}

void
e_mod_pol_client_visibility_send(E_Client *ec)
{
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_visibility_send(ec, ec->visibility.obscured);
#endif
}

void
e_mod_pol_client_iconify_by_visibility(E_Client *ec)
{
   if (!ec) return;
   _e_mod_pol_client_iconify_by_visibility(ec);
}

void
e_mod_pol_client_uniconify_by_visibility(E_Client *ec)
{
   if (!ec) return;
   _e_mod_pol_client_uniconify_by_visibility(ec);
}
