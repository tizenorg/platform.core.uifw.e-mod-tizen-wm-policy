#include "e_mod_main.h"
#ifdef HAVE_WAYLAND_ONLY
#define E_COMP_WL
#include "e_comp_wl.h"
#include "e_mod_wl.h"
#else
#include "e_mod_atoms.h"
#include <X11/Xlib.h>
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
#ifndef HAVE_WAYLAND_ONLY
static Eina_Bool       _win_opaque_prop_get(Ecore_X_Window win, int *opaque);
#endif

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
#else
   XEvent event;

   if (!ec) return;

   event.type = VisibilityNotify;
   event.xvisibility.display = ecore_x_display_get();
   event.xvisibility.send_event = EINA_TRUE;
   event.xvisibility.state = visibility;
   event.xvisibility.window = e_client_util_win_get(ec);

   //printf("[e-mod-tizen-wm-policy] visibility_send: win:0x%x, visibility:%s\n", event.xvisibility.window,(event.xvisibility.state?"OBSCURED":"UNOBSCURED"));
   XSendEvent(event.xvisibility.display,
              event.xvisibility.window,
              False,
              VisibilityChangeMask, &event);
#endif
}

static void
_pol_cb_visibility_data_free(void *data)
{
   free(data);
}

#ifndef HAVE_WAYLAND_ONLY
static Eina_Bool
_win_opaque_prop_get(Ecore_X_Window win, int *opaque)
{
   int ret = -1;
   unsigned int val = 0;
   if (!opaque) return EINA_FALSE;

   ret = ecore_x_window_prop_card32_get(win, E_MOD_POL_ATOM_WINDOW_OPAQUE, &val, 1);
   if (ret == -1 ) return EINA_FALSE;

   *opaque = (int)val;
   return EINA_TRUE;
}
#endif

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

void
e_mod_pol_zone_visibility_calc(E_Zone *zone)
{
   E_Client *ec;
   Eina_Tiler *t;
   Eina_Rectangle r;
   const int edge = 1;
   const int OBSCURED = 2; // 2: Fully Obscured, Currently Ecore_X treats Fully Obscured only.
   const int UNOBSCURED  = 0;
   Pol_Visibility *pv;

   if (!zone) return;

   t = eina_tiler_new(zone->w + edge, zone->h + edge);
   eina_tiler_tile_size_set(t, 1, 1);

   if (zone->display_state != E_ZONE_DISPLAY_STATE_OFF)
     {
        EINA_RECTANGLE_SET(&r, zone->x, zone->y, zone->w, zone->h);
        eina_tiler_rect_add(t, &r);
     }

   E_CLIENT_REVERSE_FOREACH(zone->comp, ec)
     {
        if (e_object_is_del(E_OBJECT(ec))) continue;
        if (e_client_util_ignored_get(ec)) continue;
        /* check zone and skip borders not on this zone */
        if (ec->zone != zone) continue;
        /* check e_client and skip e_clients not visible */
        if (!ec->frame) continue;
        if (!evas_object_visible_get(ec->frame)) continue;
        /* check e_client and skip e_clinets not intersects with zone */
        if (!E_INTERSECTS(ec->x, ec->y, ec->w, ec->h, zone->x, zone->y, zone->w, zone->h))
          continue;

        if (_client_tiler_intersects(ec, t))
          {
             Eina_Bool opaque = EINA_FALSE;
             /* unobscured case */
             pv = _visibility_find(ec);
             if (pv)
               {
                  if (pv->visibility == UNOBSCURED)
                    {
                       /* previous state is unobscured */
                       /* do nothing */
                    }
                  else
                    {
                       /* previous state is obscured */
                       pv->visibility = UNOBSCURED;
                       _visibility_notify_send(ec, UNOBSCURED);
                    }
               }
             else
               {
                  /* previous state is none */
                  _visibility_add(ec, UNOBSCURED);
                  _visibility_notify_send(ec, UNOBSCURED);
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
                  if (pv->visibility == UNOBSCURED)
                    {
                       /* previous state is unobscured */
                       pv->visibility = OBSCURED;
                       _visibility_notify_send(ec, OBSCURED);
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
                  _visibility_add(ec, OBSCURED);
                  _visibility_notify_send(ec, OBSCURED);
               }
          }
     }

   eina_tiler_free(t);
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

void
e_mod_pol_client_window_opaque_set(E_Client *ec)
{
   int opaque = 0;

   if (!ec) return;

#ifndef HAVE_WAYLAND_ONLY
   Ecore_X_Window win = e_client_util_win_get(ec);

   if (!_win_opaque_prop_get(win, &opaque)) return;
#else
   E_Comp_Wl_Client_Data *cdata;

   if (!ec->pixmap) return;
   if (!(cdata = (E_Comp_Wl_Client_Data *)e_pixmap_cdata_get(ec->pixmap))) return;

   opaque = cdata->opaque_state;
#endif

   ec->visibility.opaque = opaque;
}

#ifndef HAVE_WAYLAND_ONLY
Eina_Bool
e_mod_pol_visibility_cb_window_property(Ecore_X_Event_Window_Property *ev)
{
   Ecore_X_Window win;
   int opaque = 0;
   E_Client *ec;

   if (!ev) return EINA_FALSE;

   ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->win);
   if (!ec) return EINA_FALSE;

   win = ev->win;

   if (_win_opaque_prop_get(win, &opaque))
     {
        ec->visibility.opaque = opaque;
     }
   else
     {
        ec->visibility.opaque = -1;
     }

   e_mod_pol_visibility_calc();

   return EINA_TRUE;
}
#endif
