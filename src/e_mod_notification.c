#include "e_mod_notification.h"
#include "e_mod_atoms.h"
#include "e_mod_utils.h"

typedef struct _Pol_Notification
{
   E_Client *ec;
   int       level;
   unsigned char prev_ec_prop;
   unsigned char prev_ec_fetch_type;
} Pol_Notification;

static Eina_Hash *hash_pol_notifications = NULL;

static Pol_Notification*
_pol_notification_get_info (E_Client *ec)
{
   Pol_Notification *pn;

   EINA_SAFETY_ON_NULL_RETURN_VAL(hash_pol_notifications, NULL);

   pn = eina_hash_find(hash_pol_notifications, &ec);
   if (!pn)
     {
        pn = E_NEW(Pol_Notification, 1);
        EINA_SAFETY_ON_NULL_RETURN_VAL(pn, NULL);

        pn->ec = ec;
        eina_hash_add(hash_pol_notifications, &ec, pn);
     }

   return pn;
}

static Eina_Bool
_pol_notification_is_notification (E_Client *ec)
{
   int i;

   /* make sure we have a border */
   if (!ec) return EINA_FALSE;

   /* check actual type */
   if (ec->netwm.type == E_WINDOW_TYPE_NOTIFICATION)
      return EINA_TRUE;

   for (i=0; i<ec->netwm.extra_types_num; i++)
      if (ec->netwm.extra_types[i] == E_WINDOW_TYPE_NOTIFICATION)
         return EINA_TRUE;

   return EINA_FALSE;
}

static int
_pol_notification_get_level(E_Client *ec)
{
#ifndef HAVE_WAYLAND_ONLY
   int level = 0;
   int ret;
   int num;
   unsigned char* prop_data = NULL;
   Ecore_Window win = 0;

   win = e_pixmap_window_get(ec->pixmap);
   if (!win) return level;

   ret = ecore_x_window_prop_property_get (win, E_MOD_POL_ATOM_NOTIFICATION_LEVEL,
                                           ECORE_X_ATOM_CARDINAL, 32, &prop_data,
                                           &num);
   if (ret && prop_data)
      memcpy (&level, prop_data, sizeof (int));

   if (prop_data) free (prop_data);

   return level;
#else
   return 0;
#endif
}

static short
_pol_notification_level_to_layer(int level)
{
   switch (level)
     {
      case E_POL_NOTIFICATION_LEVEL_HIGH:
        return E_LAYER_CLIENT_NOTIFICATION_HIGH;
      case E_POL_NOTIFICATION_LEVEL_NORMAL:
        return E_LAYER_CLIENT_NOTIFICATION_NORMAL;
      case E_POL_NOTIFICATION_LEVEL_LOW:
        return E_LAYER_CLIENT_NOTIFICATION_LOW;
      default:
        return E_LAYER_CLIENT_ABOVE;
     }
}

void
e_mod_pol_notification_init(void)
{
   hash_pol_notifications = eina_hash_pointer_new(free);
}

void
e_mod_pol_notification_shutdown(void)
{
   E_FREE_FUNC(hash_pol_notifications, eina_hash_free);
}

void
e_mod_pol_notification_client_del(E_Client *ec)
{
   Pol_Notification *pn;

   EINA_SAFETY_ON_NULL_RETURN(ec);
   EINA_SAFETY_ON_NULL_RETURN(hash_pol_notifications);

   pn = eina_hash_find(hash_pol_notifications, &ec);
   if (!pn) return;

   eina_hash_del_by_key(hash_pol_notifications, &ec);
}

void
e_mod_pol_notification_hook_pre_fetch(E_Client *ec)
{
#ifndef HAVE_WAYLAND_ONLY
   Pol_Notification *pn;

   EINA_SAFETY_ON_NULL_RETURN(ec);

   pn = _pol_notification_get_info(ec);
   EINA_SAFETY_ON_NULL_RETURN(pn);

   pn->prev_ec_prop = ec->changes.prop;
   pn->prev_ec_fetch_type = ec->netwm.fetch.type;
#else
   return;
#endif
}

void
e_mod_pol_notification_hook_pre_post_fetch(E_Client *ec)
{
#ifndef HAVE_WAYLAND_ONLY
   Pol_Notification *pn;

   EINA_SAFETY_ON_NULL_RETURN(ec);

   pn = _pol_notification_get_info(ec);
   EINA_SAFETY_ON_NULL_RETURN(pn);

   if (pn->prev_ec_prop == ec->changes.prop &&
       pn->prev_ec_fetch_type == ec->netwm.fetch.type)
      return;

   e_mod_pol_notification_level_update (ec);
#else
   return;
#endif
}

Eina_Bool
e_mod_pol_notification_level_update(E_Client *ec)
{
   Pol_Notification *pn;
   short layer;
   int level;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ec, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec->frame, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec->pixmap, EINA_FALSE);

   pn = _pol_notification_get_info(ec);
   EINA_SAFETY_ON_NULL_RETURN_VAL(pn, EINA_FALSE);

   /* 1. Check if a window is notification or not */
   if (!_pol_notification_is_notification(ec))
      return EINA_FALSE;

   /* 2. Get and Set level */
   level = _pol_notification_get_level(ec);
   layer = _pol_notification_level_to_layer(level);

   if (level == pn->level &&
       layer == evas_object_layer_get(ec->frame))
      return EINA_TRUE;

   /* 3. Change Stack */
   pn->level = level;
   evas_object_layer_set (ec->frame, layer);

   return EINA_TRUE;
}

Eina_Bool
e_mod_pol_notification_level_apply(E_Client *ec, int level)
{
   Pol_Notification *pn;
   short layer;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ec, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec->frame, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec->pixmap, EINA_FALSE);

   pn = _pol_notification_get_info(ec);
   EINA_SAFETY_ON_NULL_RETURN_VAL(pn, EINA_FALSE);
#if 0
   /* 1. Check if a window is notification or not */
   if (!_pol_notification_is_notification(ec))
      return EINA_FALSE;
#endif
   /* 2. Get and Set level */
   level = _pol_notification_get_level(ec);
   layer = _pol_notification_level_to_layer(level);

   if (level == pn->level &&
       layer == evas_object_layer_get(ec->frame))
      return EINA_TRUE;

   /* 3. Change Stack */
   pn->level = level;
   evas_object_layer_set (ec->frame, layer);

   return EINA_TRUE;
}
