#include "e_mod_main.h"
#include "e_mod_sysinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>

#define WIN_WIDTH       500
#define WIN_HEIGHT      1080
#define PROC_CNT        5
#define REFRESH_TIMEOUT 1.5

typedef struct _E_Sysinfo
{
   Eina_Bool    show;
   Evas_Object *win;
   E_Client    *ec;
   Evas_Object *btn;

   struct
   {
      Elm_Transit *trans;
   } effect;

} E_Sysinfo;

typedef struct _process_info
{
   char name[128];

   float usage;
   float anim_usage;
   float prev_usage;
} process_info;

typedef struct _cpu_info
{
   Evas_Object *tbl;
   Evas_Object *lb[PROC_CNT], *pbs[PROC_CNT];

   process_info pinfos[PROC_CNT];

   Ecore_Timer *refresh_timer;

   Ecore_Animator *animator;
   double refresh_time;
} cpu_info;

static E_Sysinfo *e_sysinfo = NULL;

static void _cpu_prog_stop(cpu_info *cinfo);

static void
_cpu_prog_get_pinfos(process_info *pinfos, int read_count)
{
   FILE *fp;
   char buff[512];
   int line = 0, readline = 0;;

   fp = popen("top -b -n 1", "r");
   if (!fp) return;

   while (fgets(buff, 512, fp) && readline < read_count)
     {
        if (line++ < 7) continue;
          sscanf(buff, "%*s %*s %*s %*s %*s %*s %*s %*s %f %*s %*s %s",
                 &pinfos[readline].usage, pinfos[readline].name);
        if (!strcmp(pinfos[readline].name, "top")) continue;
        if (pinfos[readline].usage > 100.0)
          pinfos[readline].usage = 100.0;
        pinfos[readline].prev_usage = 0.0;
        readline++;
     }

   pclose(fp);
}

static Eina_Bool
_cpu_prog_frame_animate(void *data)
{
   cpu_info *cinfo = data;
   double pos;
   int i;

   pos = (ecore_time_get() - cinfo->refresh_time) / REFRESH_TIMEOUT;
   if (pos > 1.0) pos = 1.0;

   for (i = 0; i < PROC_CNT; i++)
     {
        double base = cinfo->pinfos[i].prev_usage;
        double delta = cinfo->pinfos[i].usage - base;
        double progress;

        cinfo->pinfos[i].anim_usage = base + delta * pos;
        progress = cinfo->pinfos[i].anim_usage / 100;

        elm_progressbar_part_value_set(cinfo->pbs[i], "elm.cur.progressbar", progress);
        elm_progressbar_part_value_set(cinfo->pbs[i], "elm.cur.progressbar1", progress);
     }

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_cpu_prog_pb_refresh(void *data)
{
   cpu_info *cinfo = data;
   process_info pinfos[PROC_CNT];
   int i, j;

   _cpu_prog_get_pinfos(pinfos, PROC_CNT);

   for (i = 0; i < PROC_CNT; i++)
     for (j = 0; j < PROC_CNT; j++)
       if (!strcmp(cinfo->pinfos[j].name, pinfos[i].name))
         {
            pinfos[i].prev_usage = cinfo->pinfos[j].anim_usage;
            break;
         }

   memcpy(cinfo->pinfos, pinfos, sizeof(process_info) * PROC_CNT);

   for (i = 0; i < PROC_CNT; i++)
     {
        double progress = cinfo->pinfos[i].prev_usage / 100;

        elm_object_text_set(cinfo->lb[i], cinfo->pinfos[i].name);
        elm_progressbar_part_value_set(cinfo->pbs[i], "elm.cur.progressbar", progress);
        elm_progressbar_part_value_set(cinfo->pbs[i], "elm.cur.progressbar1", progress);
     }

   cinfo->refresh_time = ecore_time_get();

   if (!cinfo->refresh_timer)
     cinfo->refresh_timer = ecore_timer_add(REFRESH_TIMEOUT, _cpu_prog_pb_refresh, cinfo);

   return ECORE_CALLBACK_RENEW;
}

static Eina_Bool
_cpu_prog_widget_create(cpu_info *cinfo, Evas_Object *win)
{
   int i;

   cinfo->tbl = elm_table_add(win);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cinfo->tbl, EINA_FALSE);
   evas_object_size_hint_weight_set(cinfo->tbl, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, cinfo->tbl);
   evas_object_show(cinfo->tbl);

   for (i = 0; i < PROC_CNT; i++)
     {
        cinfo->lb[i] = elm_label_add(win);
        elm_table_pack(cinfo->tbl, cinfo->lb[i], 0, i, 1, 1);
        evas_object_show(cinfo->lb[i]);

        cinfo->pbs[i] = elm_progressbar_add(win);
        elm_object_style_set(cinfo->pbs[i], "double");
        elm_progressbar_unit_format_set(cinfo->pbs[i], "%.1f%%");
        evas_object_size_hint_weight_set(cinfo->pbs[i], EVAS_HINT_EXPAND, 0.0);
        evas_object_size_hint_align_set(cinfo->pbs[i], EVAS_HINT_FILL, 0.0);
        elm_progressbar_span_size_set(cinfo->pbs[i], ELM_SCALE_SIZE(100));
        elm_table_pack(cinfo->tbl, cinfo->pbs[i], 1, i, 1, 1);
        evas_object_show(cinfo->pbs[i]);
     }

   return EINA_TRUE;
}

static Eina_Bool
_cpu_prog_start(cpu_info *cinfo, Evas_Object *win)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(cinfo, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(win, EINA_FALSE);

   if (!_cpu_prog_widget_create(cinfo, win))
     return EINA_FALSE;

   _cpu_prog_pb_refresh(cinfo);

   ecore_animator_frametime_set(1. / 30);
   cinfo->animator = ecore_animator_add(_cpu_prog_frame_animate, cinfo);

   return EINA_TRUE;
}

static void
_cpu_prog_stop(cpu_info *cinfo)
{
   int i;

   for (i = 0; i < PROC_CNT; i++)
      evas_object_del(cinfo->pbs[i]);

   evas_object_del(cinfo->tbl);

   if (cinfo->refresh_timer)
     ecore_timer_del(cinfo->refresh_timer);

   if (cinfo->animator)
     ecore_animator_del(cinfo->animator);

   free(cinfo);
}

static void
_win_create(void)
{
   Evas_Object *win, *bg, *popup, *btn;

   if (e_sysinfo->win)
     {
        ELOGF("SYSINFO",
              "ERROR!!  |win:0x%08x",
              NULL, NULL, (unsigned int)e_sysinfo->win);
        return;
     }

   elm_scale_set(3.0f);

   win = elm_win_add(NULL, "E System Info", ELM_WIN_BASIC);
   elm_win_title_set(win, "E System Info");
   elm_win_autodel_set(win, EINA_TRUE);
   elm_win_borderless_set(win, EINA_TRUE);
   elm_win_alpha_set(win, EINA_TRUE);
   elm_win_role_set(win, "e_sysinfo");
   elm_win_prop_focus_skip_set(win, EINA_TRUE);

   bg = evas_object_rectangle_add(evas_object_evas_get(win));
   evas_object_size_hint_weight_set(bg, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   elm_win_resize_object_add(win, bg);
   evas_object_color_set(bg, 0, 0, 0, 0);
   evas_object_show(bg);

#if 0
   popup = elm_popup_add(win);
   elm_object_text_set(popup, _("system information"));
   elm_object_part_text_set(popup, "title,text", "E System Info 1");

   btn = elm_button_add(popup);
   elm_object_text_set(btn, "Close");
   elm_object_part_content_set(popup, "button1", btn);
   evas_object_show(btn);

   evas_object_show(popup);
#endif
   cpu_info *cinfo = calloc(sizeof(cpu_info), 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cinfo, -1);

   _cpu_prog_start(cinfo, win);

   evas_object_resize(win, WIN_WIDTH, WIN_HEIGHT);
   evas_object_show(win);

   e_sysinfo->win = win;

   ELOGF("SYSINFO",
         "CREATE   |win:0x%08x",
         NULL, NULL, (unsigned int)win);


   return;
}

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
   if (!e_sysinfo->win)
     {
        _win_create();
        return;
     }

   if ((!e_sysinfo->ec) || (!e_sysinfo->ec->frame)) return;

   _win_effect_init();
   EINA_SAFETY_ON_NULL_RETURN(e_sysinfo->effect.trans);

   ELOGF("SYSINFO", "EFF SHOW |t:0x%08x", NULL, NULL, (unsigned int)e_sysinfo->effect.trans);

   elm_transit_go(e_sysinfo->effect.trans);
}

static void
_win_hide(void)
{
   if ((!e_sysinfo->win) || (!e_sysinfo->ec) || (!e_sysinfo->ec->frame))
     return;

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
   if (!e_sysinfo->win) return;
   if (ec->internal_elm_win != e_sysinfo->win) return;

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
   if (!e_sysinfo->win) return;
   if (e_sysinfo->ec != ec) return;

   ELOGF("SYSINFO",
         "DEL      |internale_elm_win:0x%08x %dx%d",
         ec->pixmap, ec,
         (unsigned int)ec->internal_elm_win,
         ec->w, ec->h);

   e_sysinfo->win = NULL;
   e_sysinfo->ec = NULL;
}

Eina_Bool
e_mod_pol_sysinfo_init(void)
{
   Evas_Object *o, *comp_obj;

   e_sysinfo = E_NEW(E_Sysinfo, 1);
   EINA_SAFETY_ON_NULL_RETURN_VAL(e_sysinfo, EINA_FALSE);

   /////////////////////////////////////////////////////////////////////////////////////////////
   o = evas_object_rectangle_add(e_comp->evas);
   evas_object_color_set(o, 0, 0, 0, 0);
   evas_object_resize(o, 64, 64);
   evas_object_move(o, 0, 0);

   comp_obj = e_comp_object_util_add(o, E_COMP_OBJECT_TYPE_NONE);
   evas_object_layer_set(comp_obj, E_LAYER_POPUP);

   evas_object_event_callback_add(comp_obj, EVAS_CALLBACK_MOUSE_UP, _btn_cb_mouse_up, NULL);

   evas_object_show(comp_obj);

   e_sysinfo->btn = comp_obj;
   /////////////////////////////////////////////////////////////////////////////////////////////

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
