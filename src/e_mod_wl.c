#include "e_mod_wl.h"
#include "e_mod_main.h"
#include "e_mod_notification.h"

#include <wayland-server.h>
#include <tizen-extension-server-protocol.h>

typedef struct _Pol_Wl_Tzpol
{
   struct wl_resource *res_tzpol; /* tizen_policy_interface */
   Eina_List          *psurfs;    /* list of Pol_Wl_Surface */
} Pol_Wl_Tzpol;

typedef struct _Pol_Wl_Surface
{
   struct wl_resource *surf;
   Pol_Wl_Tzpol       *tzpol;
   E_Pixmap           *cp;
   E_Client           *ec;
   Eina_Bool           pending_notilv;
   int32_t             notilv;
   Eina_List          *vislist; /* list of tizen_visibility_interface resources */
   Eina_List          *poslist; /* list of tizen_position_inteface resources */
} Pol_Wl_Surface;

typedef struct _Pol_Wl
{
   Eina_Hash *tzpols; /* list of Pol_Wl_Tzpol */
} Pol_Wl;

static Pol_Wl *polwl = NULL;

static void _pol_wl_surf_del(Pol_Wl_Surface *psurf);

// --------------------------------------------------------
// Pol_Wl_Tzpol
// --------------------------------------------------------
static Pol_Wl_Tzpol *
_pol_wl_tzpol_add(struct wl_resource *res_tzpol)
{
   Pol_Wl_Tzpol *tzpol;

   tzpol = E_NEW(Pol_Wl_Tzpol, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzpol, NULL);

   eina_hash_add(polwl->tzpols, &res_tzpol, tzpol);

   tzpol->res_tzpol = res_tzpol;

   ELOGF("TZPOL",
         "HASH_ADD |res:0x%08x|tzpol:0x%08x",
         NULL, NULL,
         (unsigned int)res_tzpol,
         (unsigned int)tzpol);

   return tzpol;
}

static void
_pol_wl_tzpol_del(void *data)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;

   tzpol = (Pol_Wl_Tzpol *)data;

   EINA_LIST_FREE(tzpol->psurfs, psurf)
     {
        _pol_wl_surf_del(psurf);
     }

   ELOGF("TZPOL",
         "HASH_DEL |res:0x%08x|tzpol:0x%08x",
         NULL, NULL,
         (unsigned int)tzpol->res_tzpol,
         (unsigned int)tzpol);

   memset(tzpol, 0x0, sizeof(Pol_Wl_Tzpol));
   E_FREE(tzpol);
}

static Pol_Wl_Tzpol *
_pol_wl_tzpol_get(struct wl_resource *res_tzpol)
{
   return (Pol_Wl_Tzpol *)eina_hash_find(polwl->tzpols, &res_tzpol);
}

static Pol_Wl_Surface *
_pol_wl_tzpol_surf_find(Pol_Wl_Tzpol *tzpol, E_Pixmap *cp)
{
   Eina_List *l;
   Pol_Wl_Surface *psurf;

   EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
     {
        if (psurf->cp == cp)
          return psurf;
     }

   return NULL;
}

static Eina_Bool
_pol_wl_surf_is_valid(Pol_Wl_Surface *psurf)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf2;
   Eina_Iterator *it;
   Eina_List *l;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf2)
       {
          if (psurf2 == psurf)
            {
               eina_iterator_free(it);
               return EINA_TRUE;
            }
       }
   eina_iterator_free(it);

   return EINA_FALSE;
}

// --------------------------------------------------------
// Pol_Wl_Surface
// --------------------------------------------------------
static Pol_Wl_Surface *
_pol_wl_surf_add(E_Pixmap *cp, struct wl_resource *res_tzpol)
{
   Pol_Wl_Surface *psurf = NULL;
   E_Comp_Client_Data *cdata = NULL;
   struct wl_resource *surf = NULL;

   Pol_Wl_Tzpol *tzpol;

   tzpol = _pol_wl_tzpol_get(res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN_VAL(tzpol, NULL);

   psurf = _pol_wl_tzpol_surf_find(tzpol, cp);
   if (psurf) return psurf;

   psurf = E_NEW(Pol_Wl_Surface, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(psurf, NULL);

   cdata = e_pixmap_cdata_get(cp);
   if (cdata)
     surf = cdata->wl_surface;

   psurf->surf = surf;
   psurf->tzpol = tzpol;
   psurf->cp = cp;
   psurf->ec = e_pixmap_client_get(cp);

   ELOGF("POLSURF",
         "LIST_ADD |s:0x%08x|tzpol:0x%08x|ps:0x%08x",
         psurf->cp,
         psurf->ec,
         (unsigned int)surf,
         (unsigned int)tzpol,
         (unsigned int)psurf);

   tzpol->psurfs = eina_list_append(tzpol->psurfs, psurf);

   return psurf;
}

static void
_pol_wl_surf_del(Pol_Wl_Surface *psurf)
{
   ELOGF("POLSURF",
         "LIST_DEL |s:0x%08x|tzpol:0x%08x|ps:0x%08x|pending_notilv:%d",
         psurf->cp,
         psurf->ec,
         (unsigned int)psurf->surf,
         (unsigned int)psurf->tzpol,
         (unsigned int)psurf,
         psurf->pending_notilv);

   eina_list_free(psurf->vislist);
   eina_list_free(psurf->poslist);

   memset(psurf, 0x0, sizeof(Pol_Wl_Surface));
   E_FREE(psurf);
}

// --------------------------------------------------------
// visibility
// --------------------------------------------------------
static void
_tzvis_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzvis)
{
   wl_resource_destroy(res_tzvis);
}

static const struct tizen_visibility_interface _tzvis_iface =
{
   _tzvis_iface_cb_destroy
};

static void
_tzvis_iface_cb_vis_destroy(struct wl_resource *res_tzvis)
{
   Pol_Wl_Surface *psurf;
   Eina_Bool r;

   psurf = wl_resource_get_user_data(res_tzvis);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   r = _pol_wl_surf_is_valid(psurf);
   if (!r) return;

   psurf->vislist = eina_list_remove(psurf->vislist, res_tzvis);
}

static void
_tzpol_iface_cb_vis_get(struct wl_client *client, struct wl_resource *res_tzpol, uint32_t id, struct wl_resource *surf)
{
   E_Pixmap *cp;
   Pol_Wl_Surface *psurf;
   struct wl_resource *res_tzvis;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_add(cp, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   res_tzvis = wl_resource_create(client,
                                  &tizen_visibility_interface,
                                  wl_resource_get_version(res_tzpol),
                                  id);
   if (!res_tzvis)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res_tzvis,
                                  &_tzvis_iface,
                                  psurf,
                                  _tzvis_iface_cb_vis_destroy);

   psurf->vislist = eina_list_append(psurf->vislist, res_tzvis);
}

void
e_mod_pol_wl_visibility_send(E_Client *ec, int vis)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   struct wl_resource *res_tzvis;
   Eina_List *l, *ll;
   Eina_Iterator *it;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          if (psurf->ec != ec) continue;

          EINA_LIST_FOREACH(psurf->poslist, ll, res_tzvis)
            tizen_visibility_send_notify(res_tzvis, vis);
       }
   eina_iterator_free(it);
}

// --------------------------------------------------------
// position
// --------------------------------------------------------
static void
_tzpos_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpos)
{
   wl_resource_destroy(res_tzpos);
}

static void
_tzpos_iface_cb_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpos, int32_t x, int32_t y)
{
   E_Client *ec;
   Pol_Wl_Surface *psurf;

   psurf = wl_resource_get_user_data(res_tzpos);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   ec = e_pixmap_client_get(psurf->cp);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);
   
   if (!ec->lock_client_location)
     {
        ec->x = ec->client.x = x;
        ec->y = ec->client.y = y;
     }
}

static const struct tizen_position_interface _tzpos_iface =
{
   _tzpos_iface_cb_destroy,
   _tzpos_iface_cb_set,
};

static void
_tzpol_iface_cb_pos_destroy(struct wl_resource *res_tzpos)
{
   Pol_Wl_Surface *psurf;
   Eina_Bool r;

   psurf = wl_resource_get_user_data(res_tzpos);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   r = _pol_wl_surf_is_valid(psurf);
   if (!r) return;

   psurf->poslist = eina_list_remove(psurf->poslist, res_tzpos);
}

static void
_tzpol_iface_cb_pos_get(struct wl_client *client, struct wl_resource *res_tzpol, uint32_t id, struct wl_resource *surf)
{
   E_Pixmap *cp;
   Pol_Wl_Surface *psurf;
   struct wl_resource *res_tzpos;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_add(cp, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   res_tzpos = wl_resource_create(client,
                                  &tizen_position_interface,
                                  wl_resource_get_version(res_tzpol),
                                  id);
   if (!res_tzpos)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(res_tzpos,
                                  &_tzpos_iface,
                                  psurf,
                                  _tzpol_iface_cb_pos_destroy);

   psurf->poslist = eina_list_append(psurf->poslist, res_tzpos);
}

void
e_mod_pol_wl_position_send(E_Client *ec)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   struct wl_resource *res_tzpos;
   Eina_List *l, *ll;
   Eina_Iterator *it;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          if (psurf->ec != ec) continue;

          EINA_LIST_FOREACH(psurf->poslist, ll, res_tzpos)
            tizen_position_send_changed(res_tzpos, ec->x, ec->y);
       }
   eina_iterator_free(it);
}

// --------------------------------------------------------
// stack: activate, raise, lower
// --------------------------------------------------------
static void
_tzpol_iface_cb_activate(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   ec = e_pixmap_client_get(cp);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   if ((!starting) && (!ec->focused))
     e_client_activate(ec, EINA_TRUE);
   else
     evas_object_raise(ec->frame);
}

static void
_tzpol_iface_cb_raise(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   ec = e_pixmap_client_get(cp);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   if ((!starting) && (!ec->focused))
     e_client_activate(ec, EINA_TRUE);
   else
     evas_object_raise(ec->frame);
}

static void
_tzpol_iface_cb_lower(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec, *below = NULL;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   ec = e_pixmap_client_get(cp);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   below = ec;
   while ((below = e_client_below_get(below)))
     {
        if ((e_client_util_ignored_get(below)) ||
            (below->iconic))
          continue;

        break;
     }

   evas_object_lower(ec->frame);

   if ((!below) || (!ec->focused)) return;

   evas_object_focus_set(below->frame, 1);
}

// --------------------------------------------------------
// focus
// --------------------------------------------------------
static void
_tzpol_iface_cb_focus_skip_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   ec = e_pixmap_client_get(cp);
   if (ec)
     {
        if (ec->icccm.accepts_focus)
          {
             ec->icccm.accepts_focus = ec->icccm.take_focus = 0;
             EC_CHANGED(ec);
          }
     }
   else
     {
        cdata = e_pixmap_cdata_get(cp);
        EINA_SAFETY_ON_NULL_RETURN(cdata);
        cdata->accepts_focus = 0;
     }
}

static void
_tzpol_iface_cb_focus_skip_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   ec = e_pixmap_client_get(cp);
   if (ec)
     {
        if (!ec->icccm.accepts_focus)
          {
             ec->icccm.accepts_focus = ec->icccm.take_focus = 1;
             EC_CHANGED(ec);
          }
     }
   else
     {
        cdata = e_pixmap_cdata_get(cp);
        EINA_SAFETY_ON_NULL_RETURN(cdata);
        cdata->accepts_focus = 1;
     }
}

// --------------------------------------------------------
// role
// --------------------------------------------------------
static void
_tzpol_iface_cb_role_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf, const char *role)
{
   E_Pixmap *cp;
   E_Client *ec;

   EINA_SAFETY_ON_NULL_RETURN(role);

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   ec = e_pixmap_client_get(cp);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   eina_stringshare_replace(&ec->icccm.window_role, role);

   /* TODO: support multiple roles */
   if (!e_util_strcmp("notification-low", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_LOW);
     }
   else if (!e_util_strcmp("notification-normal", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_NORMAL);
     }
   else if (!e_util_strcmp("notification-high", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_HIGH);
     }
   else if (!e_util_strcmp("alert", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_ALERT);
     }
   else if (!e_util_strcmp("tv-volume-popup", role))
     {
        evas_object_layer_set(ec->frame, E_LAYER_CLIENT_NOTIFICATION_LOW);
        ec->lock_client_location = 1;
     }
}

// --------------------------------------------------------
// conformant
// --------------------------------------------------------
static void
_tzpol_iface_cb_conformant_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;
   Pol_Wl_Surface *psurf;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_add(cp, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   ec = e_pixmap_client_get(cp);
   if (ec)
     {
        if (!ec->comp_data->conformant)
          {
             ec->comp_data->conformant = 1;
             EC_CHANGED(ec);
          }
     }
   else
     {
        cdata = e_pixmap_cdata_get(cp);
        EINA_SAFETY_ON_NULL_RETURN(cdata);
        cdata->conformant = 1;
     }
}

static void
_tzpol_iface_cb_conformant_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;
   Pol_Wl_Surface *psurf;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_add(cp, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   ec = e_pixmap_client_get(cp);
   if (ec)
     {
        if (ec->comp_data->conformant)
          {
             ec->comp_data->conformant = 0;
             EC_CHANGED(ec);
          }
     }
   else
     {
        cdata = e_pixmap_cdata_get(cp);
        EINA_SAFETY_ON_NULL_RETURN(cdata);
        cdata->conformant = 0;
     }
}

static void
_tzpol_iface_cb_conformant_get(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;
   Pol_Wl_Surface *psurf;
   unsigned char conformant;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_add(cp, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   ec = e_pixmap_client_get(cp);
   if (ec)
     conformant = ec->comp_data->conformant;
   else
     {
        cdata = e_pixmap_cdata_get(cp);
        EINA_SAFETY_ON_NULL_RETURN(cdata);

        conformant = cdata->conformant;
     }

   tizen_policy_send_conformant(res_tzpol, surf, conformant);
}

void
e_mod_pol_wl_keyboard_geom_broadcast(E_Client *ec)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   E_Client *ec2;
   Eina_Bool r;
   Eina_List *l;
   Eina_Iterator *it;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          ec2 = psurf->ec;
          if (!ec2) continue;

          r = e_client_util_ignored_get(ec2);
          if (r) continue;

          r = e_mod_pol_client_is_conformant(ec2);
          if (!r) continue;

          tizen_policy_send_conformant_area
            (tzpol->res_tzpol,
             psurf->surf,
             TIZEN_POLICY_CONFORMANT_PART_KEYBOARD,
             ec->visible, ec->x, ec->y,
             ec->client.w, ec->client.h);
       }
   eina_iterator_free(it);
}

// --------------------------------------------------------
// notification level
// --------------------------------------------------------
static void
_tzpol_iface_cb_notilv_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf, int32_t lv)
{
   E_Pixmap *cp;
   E_Client *ec;
   Pol_Wl_Surface *psurf;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_add(cp, res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   ec = e_pixmap_client_get(cp);
   if (ec)
     e_mod_pol_notification_level_apply(ec, lv);
   else
     psurf->pending_notilv = EINA_TRUE;

   psurf->notilv = lv;

   ELOGF("TZPOL",
         "NOTI_DONE|s:0x%08x|res_tzpol:0x%08x|psurf:0x%08x|lv%d",
         cp, ec,
         (unsigned int)surf,
         (unsigned int)res_tzpol,
         (unsigned int)psurf,
         lv);

   tizen_policy_send_notification_done
     (res_tzpol, surf, lv, TIZEN_POLICY_ERROR_STATE_NONE);
}

void
e_mod_pol_wl_notification_level_fetch(E_Client *ec)
{
   E_Pixmap *cp;
   Pol_Wl_Surface *psurf;
   Pol_Wl_Tzpol *tzpol;
   Eina_Iterator *it;
   Eina_List *l;

   EINA_SAFETY_ON_NULL_RETURN(ec);

   cp = ec->pixmap;
   EINA_SAFETY_ON_NULL_RETURN(cp);

   // TODO: use pending_notilv_list instead of loop
   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH(tzpol->psurfs, l, psurf)
       {
          if (psurf->cp != cp) continue;
          if (!psurf->pending_notilv) continue;

          psurf->pending_notilv = EINA_FALSE;
          e_mod_pol_notification_level_apply(ec, psurf->notilv);
       }
   eina_iterator_free(it);
}

// --------------------------------------------------------
// transient for
// --------------------------------------------------------
static void
_pol_wl_parent_surf_set(E_Client *ec, struct wl_resource *parent_surf)
{
   E_Pixmap *pp;
   E_Client *pc = NULL;
   Ecore_Window pwin = 0;

   if (parent_surf)
     {
        if (!(pp = wl_resource_get_user_data(parent_surf)))
          {
             ERR("Could not get parent res pixmap");
             return;
          }

        pwin = e_pixmap_window_get(pp);

        /* find the parent client */
        if (!(pc = e_pixmap_client_get(pp)))
          pc = e_pixmap_find_client(E_PIXMAP_TYPE_WL, pwin);
     }

   e_mod_pol_stack_transient_for_set(ec, pc);
}

static void
_tzpol_iface_cb_transient_for_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, uint32_t child_id, uint32_t parent_id)
{
   E_Client *ec, *pc;
   struct wl_resource *parent_surf;

   ELOGF("TZPOL",
         "TF_SET   |res_tzpol:0x%08x|parent:%d|child:%d",
         NULL, NULL, (unsigned int)res_tzpol, parent_id, child_id);

   ec = e_pixmap_find_client_by_res_id(child_id);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   pc = e_pixmap_find_client_by_res_id(parent_id);
   EINA_SAFETY_ON_NULL_RETURN(pc);
   EINA_SAFETY_ON_NULL_RETURN(pc->comp_data);

   parent_surf = pc->comp_data->surface;

   _pol_wl_parent_surf_set(ec, parent_surf);

   ELOGF("TZPOL",
         "         |parent|s:0x%08x",
         pc->pixmap, pc, (unsigned int)parent_surf);

   ELOGF("TZPOL",
         "         |child |s:0x%08x",
         ec->pixmap, ec,
         (unsigned int)(ec->comp_data ? ec->comp_data->surface : NULL));

   tizen_policy_send_transient_for_done(res_tzpol, child_id);

   EC_CHANGED(ec);
}

static void
_tzpol_iface_cb_transient_for_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, uint32_t child_id)
{
   E_Client *ec;

   ELOGF("TZPOL",
         "TF_UNSET |res_tzpol:0x%08x|child:%d",
         NULL, NULL, (unsigned int)res_tzpol, child_id);

   ec = e_pixmap_find_client_by_res_id(child_id);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   _pol_wl_parent_surf_set(ec, NULL);

   tizen_policy_send_transient_for_done(res_tzpol, child_id);

   EC_CHANGED(ec);
}

// --------------------------------------------------------
// window screen mode
// --------------------------------------------------------
static void
_tzpol_iface_cb_win_scrmode_set(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol, struct wl_resource *surf, uint32_t mode)
{
   E_Pixmap *cp;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   e_mod_pol_wl_win_scrmode_apply();

   tizen_policy_send_window_screen_mode_done
     (res_tzpol, surf, mode, TIZEN_POLICY_ERROR_STATE_NONE);
}

void
e_mod_pol_wl_win_scrmode_apply(void)
{
   // TODO: update screen mode for ec which was changed to be visible
   ;
}

// --------------------------------------------------------
// subsurface
// --------------------------------------------------------
static void
_tzpol_iface_cb_subsurf_place_below_parent(struct wl_client *client EINA_UNUSED, struct wl_resource *res_tzpol EINA_UNUSED, struct wl_resource *subsurf)
{
   E_Client *ec;
   E_Client *epc;
   E_Comp_Wl_Subsurf_Data *sdata;

   ec = wl_resource_get_user_data(subsurf);
   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(ec->comp_data);

   sdata = ec->comp_data->sub.data;
   EINA_SAFETY_ON_NULL_RETURN(sdata);

   epc = sdata->parent;
   EINA_SAFETY_ON_NULL_RETURN(epc);

   /* check if a subsurface has already placed below a parent */
   if (eina_list_data_find(epc->comp_data->sub.below_list, ec)) return;

   epc->comp_data->sub.list = eina_list_remove(epc->comp_data->sub.list, ec);
   epc->comp_data->sub.list_pending = eina_list_remove(epc->comp_data->sub.list_pending, ec);
   epc->comp_data->sub.below_list_pending = eina_list_append(epc->comp_data->sub.below_list_pending, ec);
   epc->comp_data->sub.list_changed = EINA_TRUE;
}

static const struct tizen_policy_interface _tzpol_iface =
{
   _tzpol_iface_cb_vis_get,
   _tzpol_iface_cb_pos_get,
   _tzpol_iface_cb_activate,
   _tzpol_iface_cb_raise,
   _tzpol_iface_cb_lower,
   _tzpol_iface_cb_focus_skip_set,
   _tzpol_iface_cb_focus_skip_unset,
   _tzpol_iface_cb_role_set,
   _tzpol_iface_cb_conformant_set,
   _tzpol_iface_cb_conformant_unset,
   _tzpol_iface_cb_conformant_get,
   _tzpol_iface_cb_notilv_set,
   _tzpol_iface_cb_transient_for_set,
   _tzpol_iface_cb_transient_for_unset,
   _tzpol_iface_cb_win_scrmode_set,
   _tzpol_iface_cb_subsurf_place_below_parent
};

static void
_tzpol_cb_unbind(struct wl_resource *res_tzpol)
{
   Pol_Wl_Tzpol *tzpol;
   struct wl_client *client;

   tzpol = _pol_wl_tzpol_get(res_tzpol);
   EINA_SAFETY_ON_NULL_RETURN(tzpol);

   eina_hash_del_by_key(polwl->tzpols, &res_tzpol);

   client = wl_resource_get_client(res_tzpol);

   ELOGF("TZPOL",
         "UNBIND   |client:0x%08x",
         NULL, NULL, (unsigned int)client);
}

static void
_tzpol_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t ver, uint32_t id)
{
   struct wl_resource *res_tzpol;

   EINA_SAFETY_ON_NULL_GOTO(polwl, err);

   if (!(res_tzpol = wl_resource_create(client,
                                        &tizen_policy_interface,
                                        ver,
                                        id)))
     {
        goto err;
     }

   wl_resource_set_implementation(res_tzpol,
                                  &_tzpol_iface,
                                  NULL,
                                  _tzpol_cb_unbind);

   ELOGF("TZPOL",
         "BIND     |client:0x%08x",
         NULL, NULL, (unsigned int)client);

   _pol_wl_tzpol_add(res_tzpol);
   return;

err:
   ERR("Could not create tizen_policy_interface res: %m");
   wl_client_post_no_memory(client);
}

void
e_mod_pol_wl_client_del(E_Client *ec)
{
   EINA_SAFETY_ON_NULL_RETURN(ec);
   if (!ec->pixmap) return;

   e_mod_pol_wl_pixmap_del(ec->pixmap);
}

void
e_mod_pol_wl_pixmap_del(E_Pixmap *cp)
{
   Pol_Wl_Tzpol *tzpol;
   Pol_Wl_Surface *psurf;
   Eina_List *l, *ll;
   Eina_Iterator *it;

   it = eina_hash_iterator_data_new(polwl->tzpols);
   EINA_ITERATOR_FOREACH(it, tzpol)
     EINA_LIST_FOREACH_SAFE(tzpol->psurfs, l, ll, psurf)
       {
          if (psurf->cp != cp) continue;
          tzpol->psurfs = eina_list_remove_list(tzpol->psurfs, l);
          _pol_wl_surf_del(psurf);
       }
   eina_iterator_free(it);
}

Eina_Bool
e_mod_pol_wl_init(void)
{
   E_Comp_Data *cdata;

   EINA_SAFETY_ON_NULL_RETURN_VAL(e_comp, EINA_FALSE);

   cdata = e_comp->wl_comp_data;
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cdata->wl.disp, EINA_FALSE);

   if (!wl_global_create(cdata->wl.disp,
                         &tizen_policy_interface,
                         1,
                         NULL,
                         _tzpol_cb_bind))
     {
        ERR("Could not add tizen_policy to wayland globals: %m");
        return EINA_FALSE;
     }

   polwl = E_NEW(Pol_Wl, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(polwl, EINA_FALSE);

   polwl->tzpols = eina_hash_pointer_new(_pol_wl_tzpol_del);

   return EINA_TRUE;
}

void
e_mod_pol_wl_shutdown(void)
{
   EINA_SAFETY_ON_NULL_RETURN(polwl);

   E_FREE_FUNC(polwl->tzpols, eina_hash_free);
   E_FREE(polwl);
}
