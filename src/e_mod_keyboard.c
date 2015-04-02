#include "e_mod_keyboard.h"
#include "e_mod_rotation.h"

EINTERN Eina_Bool
e_mod_pol_client_is_keyboard(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   if (ec->vkbd.vkbd) return EINA_TRUE;
#ifndef HAVE_WAYLAND_ONLY
   else
     {
       char *nname = NULL, *nclass = NULL;
       Eina_Bool ret = EINA_FALSE;
       ecore_x_icccm_name_class_get(e_client_util_win_get(ec), &nname, &nclass);
       if ((nname) && (!strcmp(nname,"Virtual Keyboard")) &&
           (nclass) && (!strcmp(nclass, "ISF")))
         ret = EINA_TRUE;

       if (nname) free(nname);
       if (nclass) free(nclass);

       if (ret) return ret;
     }
#endif

   return EINA_FALSE;
}

EINTERN Eina_Bool
e_mod_pol_client_is_keyboard_sub(E_Client *ec)
{
   E_OBJECT_CHECK_RETURN(ec, EINA_FALSE);
   E_OBJECT_TYPE_CHECK_RETURN(ec, E_CLIENT_TYPE, EINA_FALSE);

   if (ec->vkbd.vkbd) return EINA_FALSE;

   if ((ec->icccm.class) &&
       (!strcmp(ec->icccm.class, "ISF")))
     return EINA_TRUE;

#ifndef HAVE_WAYLAND_ONLY
   if (!ec->icccm.class)
     {
       char *nname = NULL, *nclass = NULL;
       Eina_Bool ret = EINA_FALSE;
       ecore_x_icccm_name_class_get(e_client_util_win_get(ec), &nname, &nclass);
       if ((nname) && (!strcmp(nname,"Key Magnifier") || !strcmp(nname,"ISF Popup")) &&
           (nclass) && (!strcmp(nclass, "ISF")))
         ret = EINA_TRUE;

       if (nname) free(nname);
       if (nclass) free(nclass);

       if (ret) return ret;
     }
#endif

   return EINA_FALSE;
}

EINTERN void
e_mod_pol_keyboard_layout_apply(E_Client *ec)
{
   int angle;
   int angle_id = 0;
   int kbd_x, kbd_y, kbd_w, kbd_h;

   if (!e_mod_pol_client_is_keyboard(ec) &&
       !e_mod_pol_client_is_keyboard_sub(ec))
      return;

   angle = e_client_rotation_curr_angle_get(ec);

   switch (angle)
     {
      case 0: angle_id = 0; break;
      case 90: angle_id = 1; break;
      case 180: angle_id = 2; break;
      case 270: angle_id = 3; break;
      default: angle_id = 0; break;
     }

   kbd_w = ec->e.state.rot.geom[angle_id].w;
   kbd_h = ec->e.state.rot.geom[angle_id].h;

   switch (angle)
     {
      case 0:
         kbd_x = ec->zone->w - kbd_w;
         kbd_y = ec->zone->h - kbd_h;
         break;

      case 90:
         kbd_x = ec->zone->w - kbd_w;
         kbd_y = ec->zone->h - kbd_h;
         break;

      case 180:
         kbd_x = 0;
         kbd_y = 0;
         break;

      case 270:
         kbd_x = 0;
         kbd_y = 0;
         break;

      default:
         kbd_x = ec->zone->w - kbd_w;
         kbd_y = ec->zone->h - kbd_h;
         break;
     }

   if ((ec->frame) &&
       ((ec->w != kbd_w) || (ec->h != kbd_h)))
     e_client_util_resize_without_frame(ec, kbd_w, kbd_h);

   if ((e_mod_pol_client_is_keyboard(ec)) &&
       (ec->frame) &&
       ((ec->x != kbd_x) || (ec->y != kbd_y)))
     e_client_util_move_without_frame(ec, kbd_x, kbd_y);
}

#ifndef HAVE_WAYLAND_ONLY
EINTERN void
e_mod_pol_keyboard_configure(Ecore_X_Event_Window_Configure_Request *ev)
{
   E_Client *ec;

   ec = e_pixmap_find_client(E_PIXMAP_TYPE_X, ev->win);
   if (!ec)
     {
        char *name = NULL, *class = NULL;

        ecore_x_icccm_name_class_get(ev->win, &name, &class);
        if ((class) && (!strcmp(class, "ISF")))
          {
             /* pass through configure requests if it is a request for keyboard */
             ecore_x_window_configure(ev->win, ev->value_mask,
                                      ev->x, ev->y, ev->w, ev->h,
                                      ev->border, ev->abovewin, ev->detail);
          }
     }
}
#endif
