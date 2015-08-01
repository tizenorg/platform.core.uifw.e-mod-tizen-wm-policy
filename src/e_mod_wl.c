#include "e_mod_wl.h"
#include "e_mod_main.h"
#include "e_mod_notification.h"

#include <wayland-server.h>
#include <tizen-extension-server-protocol.h>

#undef VALIDATION
#define VALIDATION(s, p) do { if (!_pol_wl_surf_valid_check(__func__, __LINE__, s, p)) return; } while(0)

typedef struct _Pol_Wl_Surface
{
   struct wl_resource *surf;
   struct wl_resource *tzpol;
   E_Pixmap           *cp;
   E_Client           *ec;
   Eina_Bool           pending_notilv;
   int32_t             notilv;
   uint32_t            scrmode;
   Eina_List          *vislist; /* list of tizen_visibility_interface resources */
   Eina_List          *poslist; /* list of tizen_position_inteface resources */
} Pol_Wl_Surface;

typedef struct _Pol_Wl
{
   Eina_List *resources; /* list of tizen_policy_interface resources */
   Eina_Hash *surfaces; /* hash for Pol_Wl_Surface */
} Pol_Wl;

static Pol_Wl *polwl = NULL;

// --------------------------------------------------------
// Pol_Wl_Surface
// --------------------------------------------------------
static Pol_Wl_Surface *
_pol_wl_surf_add(E_Pixmap *cp, struct wl_resource *tzpol)
{
   Pol_Wl_Surface *psurf = NULL;
   E_Comp_Client_Data *cdata = NULL;
   struct wl_resource *surf = NULL;
   struct wl_resource *res;
   Eina_List *l;
   Eina_Bool found = EINA_FALSE;

   psurf = eina_hash_find(polwl->surfaces, &cp);
   EINA_SAFETY_ON_TRUE_RETURN_VAL(psurf, psurf);

   EINA_LIST_FOREACH(polwl->resources, l, res)
     {
        if (res == tzpol)
          {
             found = EINA_TRUE;
             break;
          }
     }
   EINA_SAFETY_ON_FALSE_RETURN_VAL(found, NULL);

   psurf = E_NEW(Pol_Wl_Surface, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(psurf, NULL);

   eina_hash_add(hash_surfaces, &cp, psurf);

   cdata = e_pixmap_cdata_get(cp);
   if (cdata)
     surf = cdata->wl_surface;

   psurf->surf = surf;
   psurf->tzpol = tzpol;
   psurf->cp = cp;
   psurf->ec = e_pixmap_client_get(cp);

   PLOGF("POLSURF",
         "HASH_ADD |s:0x%08x|tzpol:0x%08x|ps:0x%08x",
         psurf->cp,
         psurf->ec,
         (unsigned int)surf,
         (unsigned int)tzpol,
         (unsigned int)psurf);

   return psurf;
}

static void
_pol_wl_surf_del(Pol_Wl_Surface *psurf)
{
   PLOGF("POLSURF",
         "HASH_DEL |s:0x%08x|ps:0x%08x|tzpol:0x%08x",
         psurf->cp,
         psurf->ec,
         (unsigned int)psurf->surf,
         (unsigned int)psurf->tzpol,
         (unsigned int)psurf);

   eina_list_free(psurf->vislist);
   eina_list_free(psurf->poslist);

   memset(psurf, 0x0, sizeof(Pol_Wl_Surface));
   E_FREE(psurf);
}

static Pol_Wl_Surface *
_pol_wl_surf_get(E_Pixmap *cp)
{
   return (Pol_Wl_Surface *)eina_hash_find(polwl->surfaces, &cp);
}

static Eina_Bool
_pol_wl_surf_valid_check(const char *func, unsigned int line, Pol_Wl_Surface *psurf, struct wl_resource *tzpol)
{
   Eina_List *l;
   struct wl_resource *res;
   Eina_Bool found_psurf = EINA_FALSE;
   Eina_Bool found_tzpol = EINA_FALSE;

   EINA_LIST_FOREACH(polwl->resources, l, res)
     {
        if (res == psurf->tzpol) found_psurf = EINA_TRUE;
        if (res == tzpol) found_tzpol = EINA_TRUE;
        if ((found_psurf) && (found_tzpol)) break;
     }

   if ((!found_psurf) ||
       (!found_tzpol) ||
       (psurf->tzpol != tzpol))
     {
        PLOGF("POLSURF",
              "INVALID!!|s:0x%08x|tzpol:0x%08x|ps:0x%08x|tzpol2:0x%08x|%d|%d",
              psurf->cp,
              psurf->ec,
              (unsigned int)psurf->surf,
              (unsigned int)psurf->tzpol,
              (unsigned int)psurf,
              (unsigned int)tzpol,
              found_psurf,
              found_tzpol);

        return EINA_FALSE;
     }

   return EINA_TRUE;
}

// --------------------------------------------------------
// util funcs
// --------------------------------------------------------
static const char*
_pol_wl_pname_print(pid_t pid)
{
   FILE *h;
   char proc[512], pname[512];
   size_t len;

   sprintf(proc, "/proc/%d/cmdline", pid);

   h = fopen(proc, "r");
   if (!h) return;

   len = fread(pname, sizeof(char), 512, h);
   if (len > 0)
     {
        if ('\n' == pname[len - 1])
          pname[len - 1] = '\0';
     }

   fclose(h);

   PLOGF("TZPOL", "         |%s", NULL, NULL, pname);
}

// --------------------------------------------------------
// visibility
// --------------------------------------------------------
static void
_tzvis_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *tzvis)
{
   wl_resource_destroy(tzvis);
}

static const struct tizen_visibility_interface _tzvis_iface =
{
   _tzvis_iface_cb_destroy
};

static void
_tzvis_iface_cb_vis_destroy(struct wl_resource *tzvis)
{
   E_Pixmap *cp;
   Pol_Wl_Surface *psurf;

   cp = wl_resource_get_user_data(tzvis);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   psurf->vislist = eina_list_remove(psurf->vislist, tzvis);
}

static void
_tzpol_iface_cb_vis_get(struct wl_client *client, struct wl_resource *tzpol, uint32_t id, struct wl_resource *surf)
{
   E_Pixmap *cp;
   Pol_Wl_Surface *psurf;
   struct wl_resource *tzvis;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   if (psurf)
     VALIDATION(psurf, tzpol);
   else
     psurf = _pol_wl_surf_add(cp, tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   tzvis = wl_resource_create(client,
                              &tizen_visibility_interface,
                              wl_resource_get_version(tzpol),
                              id);
   if (!tzvis)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(tzvis,
                                  &_tzvis_iface,
                                  cp,
                                  _tzvis_iface_cb_vis_destroy);

   psurf->vislist = eina_list_append(psurf->vislist, tzvis);
}

void
e_mod_pol_wl_visibility_send(E_Client *ec, int vis)
{
   Pol_Wl_Surface *psurf;
   E_Pixmap *cp;
   struct wl_resource *tzvis;
   Eina_List *l;

   EINA_SAFETY_ON_NULL_RETURN(ec);

   cp = ec->pixmap;
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   EINA_LIST_FOREACH(psurf->vislist, l, tzvis)
     {
        tizen_visibility_send_notify(tzvis, vis);
     }
}

// --------------------------------------------------------
// position
// --------------------------------------------------------
static void
_tzpos_iface_cb_destroy(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpos)
{
   wl_resource_destroy(tzpos);
}

static void
_tzpos_iface_cb_set(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpos, int32_t x, int32_t y)
{
   E_Pixmap *cp;
   E_Client *ec;

   cp = wl_resource_get_user_data(tzpos);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   ec = e_pixmap_client_get(cp);
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
_tzpol_iface_cb_pos_destroy(struct wl_resource *tzpos)
{
   E_Pixmap *cp;
   Pol_Wl_Surface *psurf;

   cp = wl_resource_get_user_data(tzpos);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   psurf->poslist = eina_list_remove(psurf->poslist, tzpos);
}

static void
_tzpol_iface_cb_pos_get(struct wl_client *client, struct wl_resource *tzpol, uint32_t id, struct wl_resource *surf)
{
   E_Pixmap *cp;
   Pol_Wl_Surface *psurf;
   struct wl_resource *tzpos;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   if (psurf)
     VALIDATION(psurf, tzpol);
   else
     psurf = _pol_wl_surf_add(cp, tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   tzpos = wl_resource_create(client,
                              &tizen_position_interface,
                              wl_resource_get_version(tzpol),
                              id);
   if (!tzpos)
     {
        wl_client_post_no_memory(client);
        return;
     }

   wl_resource_set_implementation(tzpos,
                                  &_tzpos_iface,
                                  cp,
                                  _tzpol_iface_cb_pos_destroy);

   psurf->poslist = eina_list_append(psurf->poslist, tzpos);
}

void
e_mod_pol_wl_position_send(E_Client *ec)
{
   Pol_Wl_Surface *psurf;
   E_Pixmap *cp;
   struct wl_resource *tzpos;
   Eina_List *l;

   EINA_SAFETY_ON_NULL_RETURN(ec);

   cp = ec->pixmap;
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   EINA_LIST_FOREACH(psurf->poslist, l, tzpos)
     {
        tizen_position_send_changed(tzpos, ec->x, ec->y);
     }
}

// --------------------------------------------------------
// stack: activate, raise, lower
// --------------------------------------------------------
static void
_tzpol_iface_cb_activate(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol EINA_UNUSED, struct wl_resource *surf)
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
_tzpol_iface_cb_raise(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol EINA_UNUSED, struct wl_resource *surf)
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
_tzpol_iface_cb_lower(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol EINA_UNUSED, struct wl_resource *surf)
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
_tzpol_iface_cb_focus_skip_set(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol EINA_UNUSED, struct wl_resource *surf)
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
_tzpol_iface_cb_focus_skip_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol EINA_UNUSED, struct wl_resource *surf)
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
_tzpol_iface_cb_role_set(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol EINA_UNUSED, struct wl_resource *surf, const char *role)
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
_tzpol_iface_cb_conformant_set(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;
   Pol_Wl_Surface *psurf;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   if (psurf)
     VALIDATION(psurf, tzpol);
   else
     psurf = _pol_wl_surf_add(cp, tzpol);
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
_tzpol_iface_cb_conformant_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol EINA_UNUSED, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;
   Pol_Wl_Surface *psurf;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   if (psurf)
     VALIDATION(psurf, tzpol);
   else
     psurf = _pol_wl_surf_add(cp, tzpol);
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
_tzpol_iface_cb_conformant_get(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol, struct wl_resource *surf)
{
   E_Pixmap *cp;
   E_Client *ec;
   E_Comp_Wl_Client_Data *cdata;
   Pol_Wl_Surface *psurf;
   unsigned char conformant;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   if (psurf)
     VALIDATION(psurf, tzpol);
   else
     psurf = _pol_wl_surf_add(cp, tzpol);
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

   tizen_policy_send_conformant(tzpol, surf, conformant);
}

void
e_mod_pol_wl_keyboard_geom_broadcast(E_Client *ec)
{
   E_Client *ec2;
   Pol_Wl_Surface *psurf;
   Eina_Bool r;
   struct wl_resource *res;
   Eina_List *l;
   Eina_Bool found;

   E_CLIENT_REVERSE_FOREACH(e_comp, ec2)
     {
        r = e_client_util_ignored_get(ec2);
        if (r) continue;

        r = e_mod_pol_client_is_conformant(ec2);
        if (!r) continue;

        psurf = eina_hash_find(polwl->surfaces, &ec2->pixmap);
        if (!psurf) continue;

        found = EINA_FALSE;
        EINA_LIST_FOREACH(polwl->resources, l, res)
          {
             if (res == psurf->tzpol)
               {
                  found = EINA_TRUE;
                  break;
               }
          }

        if (!found)
          {
             PLOGF("TZPOL",
                   "CONF ERR |s:0x%08x|tzpol:0x%08x|ps:0x%08x",
                   psurf->cp, psurf->ec,
                   psurf->surf, psurf->tzpol, psurf);
             continue;
          }

        tizen_policy_send_conformant_area
           (psurf->tzpol, psurf->surf,
            TIZEN_POLICY_CONFORMANT_PART_KEYBOARD,
            ec->visible, ec->x, ec->y,
            ec->client.w, ec->client.h);
     }
}

// --------------------------------------------------------
// notification level
// --------------------------------------------------------
static void
_tzpol_iface_cb_notilv_set(struct wl_client *client, struct wl_resource *tzpol, struct wl_resource *surf, int32_t lv)
{
   E_Pixmap *cp;
   E_Client *ec;
   Pol_Wl_Surface *psurf;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   if (psurf)
     VALIDATION(psurf, tzpol);
   else
     psurf = _pol_wl_surf_add(cp, tzpol);
   EINA_SAFETY_ON_NULL_RETURN(psurf);

   ec = e_pixmap_client_get(cp);
   if (ec)
     {
        e_mod_pol_notification_level_apply(ec, lv);

        PLOGF("TZPOL",
              "NOTISEND1|s:0x%08x|tzpol:0x%08x|psurf:0x%08x|lv%d",
              cp, ec, (unsigned int)surf, (unsigned int)tzpol,
              (unsigned int)psurf, lv);

        tizen_policy_send_notification_done
           (tzpol, surf, lv,
            TIZEN_POLICY_ERROR_STATE_NONE);

        psurf->pending_notilv = EINA_FALSE;
     }
   else
     {
        PLOGF("TZPOL",
              "NOTIPEND |s:0x%08x|tzpol:0x%08x|psurf:0x%08x|lv%d",
              cp, ec, (unsigned int)surf, (unsigned int)tzpol,
              (unsigned int)psurf, lv);

        psurf->pending_notilv = EINA_TRUE;
        psurf->notilv = lv;
     }
}

void
e_mod_pol_wl_notification_level_fetch(E_Client *ec)
{
   E_Pixmap *cp;
   Pol_Wl_Surface *psurf;
   struct wl_resource *res;
   Eina_List *l;
   Eina_Bool found;

   EINA_SAFETY_ON_NULL_RETURN(ec);

   cp = ec->pixmap;
   EINA_SAFETY_ON_NULL_RETURN(cp);

   psurf = _pol_wl_surf_get(cp);
   if (!psurf) return;

   if (!psurf->pending_notilv) return;

   psurf->pending_notilv = EINA_FALSE;

   e_mod_pol_notification_level_apply(ec, psurf->notilv);

   PLOGF("TZPOL",
         "NOTISEND2|s:0x%08x|tzpol:0x%08x|ps:0x%08x|lv%d",
         ec->pixmap, ec, (unsigned int)psurf->surf,
         (unsigned int)psurf->tzpol, (unsigned int)psurf,
         psurf->notilv);

   found = EINA_FALSE;
   EINA_LIST_FOREACH(polwl->resources, l, res)
     {
        if (res == psurf->tzpol)
          {
             found = EINA_TRUE;
             break;
          }
     }

   if (!found)
     {
        PLOGF("TZPOL",
              "NOTI ERR |s:0x%08x|tzpol:0x%08x|ps:0x%08x",
              psurf->cp, psurf->ec,
              psurf->surf, psurf->tzpol, psurf);
        return;
     }

   tizen_policy_send_notification_done
      (psurf->tzpol, psurf->surf, psurf->notilv,
       TIZEN_POLICY_ERROR_STATE_NONE);
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
_tzpol_iface_cb_transient_for_set(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol, uint32_t child_id, uint32_t parent_id)
{
   E_Client *ec, *pc;
   struct wl_resource *parent_surf;

   PLOGF("TZPOL",
         "TF_SET   |tzpol:0x%08x|parent:%d|child:%d",
         NULL, NULL, (unsigned int)tzpol, parent_id, child_id);

   ec = e_pixmap_find_client_by_res_id(child_id);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   pc = e_pixmap_find_client_by_res_id(parent_id);
   EINA_SAFETY_ON_NULL_RETURN(pc);
   EINA_SAFETY_ON_NULL_RETURN(pc->comp_data);

   parent_surf = pc->comp_data->surf;

   _pol_wl_parent_surf_set(ec, parent_surf);

   PLOGF("TZPOL",
         "         |parent|s:0x%08x",
         pc->pixmap, pc, (unsigned int)parent_surf);

   PLOGF("TZPOL",
         "         |child |s:0x%08x",
         ec->pixmap, ec,
         (unsigned int)(ec->comp_data ? ec->comp_data->surf : NULL));

   tizen_policy_send_transient_for_done(tzpol, child_id);

   EC_CHANGED(ec);
}

static void
_tzpol_iface_cb_transient_for_unset(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol, uint32_t child_id)
{
   E_Client *ec;

   PLOGF("TZPOL",
         "TF_UNSET |tzpol:0x%08x|child:%d",
         NULL, NULL, (unsigned int)tzpol, child_id);

   ec = e_pixmap_find_client_by_res_id(child_id);
   EINA_SAFETY_ON_NULL_RETURN(ec);

   _pol_wl_parent_surf_set(ec, NULL);

   tizen_policy_send_transient_for_done(tzpol, child_id);

   EC_CHANGED(ec);
}

// --------------------------------------------------------
// window screen mode
// --------------------------------------------------------
static void
_tzpol_iface_cb_win_scrmode_set(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol, struct wl_resource *surf, uint32_t mode)
{
   E_Pixmap *cp;

   cp = wl_resource_get_user_data(surf);
   EINA_SAFETY_ON_NULL_RETURN(cp);

   e_mod_pol_wl_win_scrmode_apply();

   tizen_policy_send_window_screen_mode_done
      (tzpol, surf, mode, TIZEN_POLICY_ERROR_STATE_NONE);
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
_tzpol_iface_cb_subsurf_place_below_parent(struct wl_client *client EINA_UNUSED, struct wl_resource *tzpol EINA_UNUSED, struct wl_resource *subsurf)
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
_tzpol_cb_unbind(struct wl_resource *res)
{
   struct wl_client *client;
   pid_t pid = 0;
   uid_t uid = 0;
   gid_t gid = 0;

   if (!polwl) return;
   if (!polwl->resources) return;

   client = wl_resource_get_client(res);
   if (client) wl_client_get_credentials(client, &pid, &uid, &gid);

   PLOGF("TZPOL",
         "UNBIND   |tzpol:0x%08x|client:0x%08x|%d|%d|%d",
         NULL, NULL,
         (unsigned int)res,
         (unsigned int)client,
         pid, uid, gid);

   polwl->resources = eina_list_remove(polwl->resources, res);
}

static void
_tzpol_cb_bind(struct wl_client *client, void *data EINA_UNUSED, uint32_t ver, uint32_t id)
{
   struct wl_resource *res;
   pid_t pid = 0;
   uid_t uid = 0;
   gid_t gid = 0;

   EINA_SAFETY_ON_NULL_GOTO(polwl, err);

   if (!(res = wl_resource_create(client,
                                  &tizen_policy_interface,
                                  ver,
                                  id)))
     {
        goto err;
     }

   wl_resource_set_implementation(res,
                                  &_tzpol_iface,
                                  NULL,
                                  _tzpol_cb_unbind);

   polwl->resources = eina_list_append(polwl->resources, res);

   wl_client_get_credentials(client, &pid, &uid, &gid);

   PLOGF("TZPOL",
         "BIND     |tzpol:0x%08x|client:0x%08x|%d|%d|%d",
         NULL, NULL,
         (unsigned int)res,
         (unsigned int)client,
         pid, uid, gid);

   _pol_wl_pname_print(pid);
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
   Pol_Wl_Surface *psurf = NULL;

   psurf = eina_hash_find(polwl->surfaces, &cp);
   if (psurf)
     eina_hash_del_by_key(polwl->surfaces, &cp);
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

   polwl->surfaces = eina_hash_pointer_new(_pol_wl_surf_del);

   return EINA_TRUE;
}

void
e_mod_pol_wl_shutdown(void)
{
   EINA_SAFETY_ON_NULL_RETURN(polwl);

   E_FREE_FUNC(polwl->surfaces, eina_hash_free);
   E_FREE(polwl);
}
