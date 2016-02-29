#include "e_mod_main.h"
#ifdef HAVE_WAYLAND_ONLY
#define E_COMP_WL
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

typedef struct _Pol_Visibility Pol_Visibility;

struct _Pol_Visibility
{
   E_Client *ec;
   int       visibility;
};

static Eina_Hash *hash_pol_visibilities = NULL;

static Pol_Visibility *_visibility_add(E_Client *ec, int visibility);
static Pol_Visibility *_visibility_find(E_Client *ec);
static void            _visibility_notify_send(E_Client *ec, int visibility);
static void            _pol_cb_visibility_data_free(void *data);

static Pol_Visibility *
_visibility_add(E_Client *ec, int visibility)
{
   Pol_Visibility *pv;

   if (e_object_is_del(E_OBJECT(ec))) return NULL;

   pv = eina_hash_find(hash_pol_visibilities, &ec);
   if (pv) return NULL;

   pv = E_NEW(Pol_Visibility, 1);
   if (!pv) return NULL;

   pv->ec = ec;
   pv->visibility = visibility;

   eina_hash_add(hash_pol_visibilities, &ec, pv);

   return pv;
}

static Pol_Visibility *
_visibility_find(E_Client *ec)
{
   Pol_Visibility *pv;

   pv = eina_hash_find(hash_pol_visibilities, &ec);

   return pv;
}

static void
_visibility_notify_send(E_Client *ec, int visibility)
{
#ifdef HAVE_WAYLAND_ONLY
   e_mod_pol_wl_visibility_send(ec, visibility);
#endif
}

static void
_pol_cb_visibility_data_free(void *data)
{
   free(data);
}

void
e_mod_pol_visibility_init(void)
{
   hash_pol_visibilities = eina_hash_pointer_new(_pol_cb_visibility_data_free);
}

void
e_mod_pol_visibility_shutdown(void)
{
   E_FREE_FUNC(hash_pol_visibilities, eina_hash_free);
}

static Eina_Bool
_client_tiler_intersects(E_Client *ec, Eina_Tiler *t)
{
   Eina_Rectangle *r;
   Eina_Iterator *itr;
   Eina_Bool ret = EINA_FALSE;

   if (!ec) return EINA_FALSE;
   if (!t) return EINA_FALSE;
   if (eina_tiler_empty(t)) return EINA_FALSE;
    
   itr = eina_tiler_iterator_new(t);

   EINA_ITERATOR_FOREACH(itr, r)
     {
        if (E_INTERSECTS(ec->x, ec->y, ec->w, ec->h, r->x, r->y, r->w, r->h))
          {
             ret = EINA_TRUE;
             break;
          }
     }

   eina_iterator_free(itr);

   return ret;
}

static Eina_Bool
_e_mod_pol_check_transient_child_visible(E_Client *ancestor_ec, E_Client *ec)
{
   Eina_Bool visible = EINA_FALSE;
   Eina_List *list = NULL;
   E_Client *child_ec = NULL;
   Pol_Visibility *pv;

   if (!ancestor_ec) return EINA_FALSE;

   list = eina_list_clone(ec->transients);
   EINA_LIST_FREE(list, child_ec)
     {
        if (visible == EINA_TRUE) continue;

        pv = _visibility_find(child_ec);
        if (pv)
          {
             if (child_ec->exp_iconify.skip_iconify == EINA_TRUE)
               {
                  if (pv->visibility == E_VISIBILITY_UNOBSCURED)
                    {
                       return EINA_TRUE;
                    }
                  else
                    {
                       if ((!child_ec->iconic) &&
                           (E_CONTAINS(child_ec->x, child_ec->y, child_ec->w, child_ec->h, ancestor_ec->x, ancestor_ec->y, ancestor_ec->w, ancestor_ec->h)))
                         {
                            return EINA_TRUE;
                         }
                    }
               }
             else
               {
                  if ((!child_ec->iconic) ||
                      (pv->visibility == E_VISIBILITY_UNOBSCURED))
                    {
                       return EINA_TRUE;
                    }
               }
          }
        else
          {
             if (!child_ec->iconic)
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
   Pol_Visibility *pv;
   Eina_Bool do_iconify = EINA_TRUE;

   if (!ec) return;
   if (ec->iconic) return;
   if (ec->exp_iconify.by_client) return;
   if (ec->exp_iconify.skip_iconify) return;

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
        pv = _visibility_find(ec->parent);
        if ((pv) && (pv->visibility == E_VISIBILITY_FULLY_OBSCURED))
          _e_mod_pol_client_iconify_by_visibility(ec->parent);
     }
}

static void
_e_mod_pol_client_uniconify_by_visibility(E_Client *ec)
{
   if (!ec) return;
   if (!ec->iconic) return;
   if (ec->exp_iconify.by_client) return;
   if (ec->exp_iconify.skip_iconify) return;

   ELOGF("UNICONIFY_BY_WM", "win:0x%08x", ec->pixmap, ec, e_client_util_win_get(ec));
   ec->exp_iconify.not_raise = 1;
   e_client_uniconify(ec);
   e_mod_pol_wl_iconify_state_change_send(ec, 0);
}

void
e_mod_pol_zone_visibility_calc(E_Zone *zone)
{
   E_Client *ec;
   Eina_Tiler *t;
   Eina_Rectangle r;
   const int edge = 1;
   Pol_Visibility *pv;
#ifdef HAVE_WAYLAND_ONLY
   E_Comp_Wl_Client_Data *cdata;
#endif

   if (!zone) return;

   TRACE_DS_BEGIN(VISIBILITY CALCULATE);

   t = eina_tiler_new(zone->w + edge, zone->h + edge);
   eina_tiler_tile_size_set(t, 1, 1);

   if (zone->display_state != E_ZONE_DISPLAY_STATE_OFF)
     {
        EINA_RECTANGLE_SET(&r, zone->x, zone->y, zone->w, zone->h);
        eina_tiler_rect_add(t, &r);
     }

   E_CLIENT_REVERSE_FOREACH(ec)
     {
        if (e_object_is_del(E_OBJECT(ec))) continue;
        if (e_client_util_ignored_get(ec)) continue;
        /* check zone and skip borders not on this zone */
        if (ec->zone != zone) continue;
        /* check e_client and skip e_clients not visible */
        if (!ec->frame) continue;
#ifdef HAVE_WAYLAND_ONLY
        /* if ec is subsurface, skip this */
        cdata = (E_Comp_Wl_Client_Data *)ec->comp_data;
        if (cdata && cdata->sub.data) continue;
#endif
        if (!evas_object_visible_get(ec->frame))
          {
             if (!ec->iconic) continue;
             else
               {
                  if (ec->exp_iconify.by_client)
                    continue;
               }
          }

        if (_client_tiler_intersects(ec, t))
          {
             Eina_Bool opaque = EINA_FALSE;
             /* unobscured case */
             pv = _visibility_find(ec);
             if (pv)
               {
                  if (pv->visibility == E_VISIBILITY_UNOBSCURED)
                    {
                       /* previous state is unobscured */
                       /* do nothing */
                    }
                  else
                    {
                       /* previous state is obscured */
                       _e_mod_pol_client_uniconify_by_visibility(ec);
                       pv->visibility = E_VISIBILITY_UNOBSCURED;
                       _visibility_notify_send(ec, E_VISIBILITY_UNOBSCURED);
                    }
               }
             else
               {
                  /* previous state is none */
                  _e_mod_pol_client_uniconify_by_visibility(ec);
                  _visibility_add(ec, E_VISIBILITY_UNOBSCURED);
                  _visibility_notify_send(ec, E_VISIBILITY_UNOBSCURED);
               }

             /* check alpha window is opaque or not. */
             if ((ec->argb) &&
                 (ec->visibility.opaque == 1))
               opaque = EINA_TRUE;

             /* if e_client is not alpha or opaque then delete intersect rect */
             if (!ec->argb || opaque)
               {
                  EINA_RECTANGLE_SET(&r, ec->x, ec->y,
                                         ec->w + edge, ec->h + edge);
                  eina_tiler_rect_del(t, &r);
               }
          }
        else
          {
            /* obscured case */
             pv = _visibility_find(ec);
             if (pv)
               {
                  if (pv->visibility == E_VISIBILITY_UNOBSCURED)
                    {
                       /* previous state is unobscured */
                       pv->visibility = E_VISIBILITY_FULLY_OBSCURED;
                       _visibility_notify_send(ec, E_VISIBILITY_FULLY_OBSCURED);
                       _e_mod_pol_client_iconify_by_visibility(ec);
                    }
                  else
                    {
                       /* previous state is obscured */
                       /* do nothing */
                    }
               }
             else
               {
                  /* previous state is none */
                  _visibility_add(ec, E_VISIBILITY_FULLY_OBSCURED);
                  _visibility_notify_send(ec, E_VISIBILITY_FULLY_OBSCURED);
                  _e_mod_pol_client_iconify_by_visibility(ec);
               }
          }
     }

   eina_tiler_free(t);

   TRACE_DS_END();
}

void
e_mod_pol_visibility_calc(void)
{
   E_Zone *zone;
   Eina_List *zl;

   EINA_LIST_FOREACH(e_comp->zones, zl, zone)
     {
        e_mod_pol_zone_visibility_calc(zone);
     }
}

void
e_mod_pol_client_visibility_del(E_Client *ec)
{
   Pol_Visibility *pv;

   if (!ec) return;

   pv = eina_hash_find(hash_pol_visibilities, &ec);
   if (!pv) return;

   eina_hash_del_by_key(hash_pol_visibilities, &ec);
}
