#include "e_mod_main.h"
#include "e_mod_sysinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>

#define WIN_WIDTH  500
#define WIN_HEIGHT 1080

typedef struct _E_Sysinfo
{
   Eina_Bool    show;
   E_Client    *ec;
   Evas_Object *btn;

   struct
   {
      Elm_Transit *trans;
   } effect;

} E_Sysinfo;

static E_Sysinfo *e_sysinfo = NULL;

static void
_win_effect_cb_trans(Elm_Transit_Effect *eff EINA_UNUSED, Elm_Transit *trans EINA_UNUSED, double progress)
{
   E_Client *ec;
   double curr, col;

   ec = e_sysinfo->ec;
   EINA_SAFETY_ON_NULL_RETURN(ec);
   if (e_object_is_del(E_OBJECT(ec))) return;
   EINA_SAFETY_ON_NULL_RETURN(ec->frame);

   if (progress < 0.0) progress = 0.0;

   if (e_sysinfo->show)
     {
        curr = -WIN_WIDTH + (WIN_WIDTH * progress);
        col = 255 * progress;
        if (col <= 0) col = 0;
     }
   else
     {
        curr = -(WIN_WIDTH * progress);
        col = 255 - (255 * progress);
        if (col <= 0) col = 0;
     }

   evas_object_color_set(ec->frame, col, col, col, col);
   evas_object_move(ec->frame, curr, 0);

   ELOGF("SYSINFO", "EFF DO   |t:0x%08x prog:%.3f", NULL, NULL, (unsigned int)e_sysinfo->effect.trans, progress);
}

static void
_win_effect_cb_trans_end(Elm_Transit_Effect *eff EINA_UNUSED, Elm_Transit *trans EINA_UNUSED)
{
   ELOGF("SYSINFO", "EFF END  |t:0x%08x", NULL, NULL, (unsigned int)e_sysinfo->effect.trans);
   e_sysinfo->effect.trans = NULL;
}

static void
_win_effect_cb_trans_del(void *data EINA_UNUSED, Elm_Transit *transit EINA_UNUSED)
{
   ELOGF("SYSINFO", "EFF DEL  |t:0x%08x", NULL, NULL, (unsigned int)e_sysinfo->effect.trans);
   e_sysinfo->effect.trans = NULL;
}

static void
_win_effect_init(void)
{
   if (e_sysinfo->effect.trans)
     {
        elm_transit_del_cb_set(e_sysinfo->effect.trans, NULL, NULL);
        elm_transit_del(e_sysinfo->effect.trans);
     }

   e_sysinfo->effect.trans = elm_transit_add();
   elm_transit_del_cb_set(e_sysinfo->effect.trans, _win_effect_cb_trans_del, NULL);

   elm_transit_effect_add(e_sysinfo->effect.trans,
                          _win_effect_cb_trans,
                          NULL,
                          _win_effect_cb_trans_end);

   elm_transit_smooth_set(e_sysinfo->effect.trans, EINA_FALSE);
   elm_transit_tween_mode_set(e_sysinfo->effect.trans, ELM_TRANSIT_TWEEN_MODE_DECELERATE);
   elm_transit_objects_final_state_keep_set(e_sysinfo->effect.trans, EINA_FALSE);
   elm_transit_duration_set(e_sysinfo->effect.trans, 0.8f);
}

static void
_win_show(void)
{
   if ((!e_sysinfo->ec) || (!e_sysinfo->ec->frame)) return;

   _win_effect_init();
   EINA_SAFETY_ON_NULL_RETURN(e_sysinfo->effect.trans);

   ELOGF("SYSINFO", "EFF SHOW |t:0x%08x", NULL, NULL, (unsigned int)e_sysinfo->effect.trans);

   elm_transit_go(e_sysinfo->effect.trans);
}

static void
_win_hide(void)
{
   if ((!e_sysinfo->ec) || (!e_sysinfo->ec->frame)) return;

   _win_effect_init();
   EINA_SAFETY_ON_NULL_RETURN(e_sysinfo->effect.trans);

   ELOGF("SYSINFO", "EFF HIDE |t:0x%08x", NULL, NULL, (unsigned int)e_sysinfo->effect.trans);

   elm_transit_go(e_sysinfo->effect.trans);
}

static void
_btn_cb_mouse_up(void *data, Evas *evas EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   if (e_sysinfo->show) _win_hide();
   else _win_show();

   e_sysinfo->show = !e_sysinfo->show;
}

void
e_mod_pol_sysinfo_client_add(E_Client *ec)
{
   if (!e_mod_pol_client_is_sysinfo(ec)) return;

   ELOGF("SYSINFO",
         "ADD      |internale_elm_win:0x%08x %dx%d",
         ec->pixmap, ec,
         (unsigned int)ec->internal_elm_win,
         ec->w, ec->h);

   e_sysinfo->ec = ec;

   if (e_sysinfo->show) _win_show();
}

void
e_mod_pol_sysinfo_client_del(E_Client *ec)
{
   if (e_sysinfo->ec != ec) return;

   ELOGF("SYSINFO",
         "DEL      |internale_elm_win:0x%08x %dx%d",
         ec->pixmap, ec,
         (unsigned int)ec->internal_elm_win,
         ec->w, ec->h);

   e_sysinfo->ec = NULL;
}

Eina_Bool
e_mod_pol_sysinfo_init(void)
{
   Evas_Object *o, *comp_obj;

   e_sysinfo = E_NEW(E_Sysinfo, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e_sysinfo, EINA_FALSE);

   o = evas_object_rectangle_add(e_comp->evas);
   evas_object_color_set(o, 0, 0, 0, 0);
   evas_object_resize(o, 64, 64);
   evas_object_move(o, 0, 0);

   comp_obj = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(comp_obj, E_LAYER_POPUP);

   evas_object_event_callback_add(comp_obj, EVAS_CALLBACK_MOUSE_UP, _btn_cb_mouse_up, NULL);

   evas_object_show(comp_obj);

   e_sysinfo->btn = comp_obj;

   return EINA_TRUE;
}

void
e_mod_pol_sysinfo_shutdown(void)
{
   if (!e_sysinfo) return;

   if (e_sysinfo->effect.trans)
     {
        elm_transit_del_cb_set(e_sysinfo->effect.trans, NULL, NULL);
        elm_transit_del(e_sysinfo->effect.trans);
        e_sysinfo->effect.trans = NULL;
     }

   evas_object_del(e_sysinfo->btn);

   E_FREE(e_sysinfo);
}
