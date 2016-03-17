#include "e_mod_main.h"
#include "e_mod_quickpanel.h"

typedef struct _Pol_Indicator Pol_Indicator;

struct _Pol_Indicator
{
   Evas_Object *handler;
   E_Client *qp_ec;
};

static Eina_List *indicator_hooks = NULL;

static void
_indicator_hook_client_del(void *d, E_Client *ec)
{
   Pol_Indicator *pi = d;

   if (pi->qp_ec != ec) return;

   DBG("DEL Indicator Handler: obj %p", pi->handler);

   evas_object_del(pi->handler);
   free(pi);

   E_FREE_LIST(indicator_hooks, e_client_hook_del);
}

static void
_quickpanel_client_evas_cb_show(void *data, Evas *evas, Evas_Object *qp_obj, void *event)
{
   evas_object_hide(((Pol_Indicator*)data)->handler);
}

static void
_quickpanel_client_evas_cb_hide(void *data, Evas *evas, Evas_Object *qp_obj, void *event)
{
   evas_object_show(((Pol_Indicator*)data)->handler);
}

#undef E_CLIENT_HOOK_APPEND
#define E_CLIENT_HOOK_APPEND(l, t, cb, d) \
  do                                      \
    {                                     \
       E_Client_Hook *_h;                 \
       _h = e_client_hook_add(t, cb, d);  \
       assert(_h);                        \
       l = eina_list_append(l, _h);       \
    }                                     \
  while (0)

EINTERN void
e_mod_indicator_create(int w, int h)
{
   E_Client *qp_ec;
   Pol_Indicator *pi;

   DBG("Create Indicator Region: w %d, h %d", w, h);

   qp_ec = e_mod_quickpanel_client_get(/* passing zone? */);
   if (!qp_ec)
     return;

   pi = E_NEW(Pol_Indicator, 1);
   pi->qp_ec = qp_ec;
   pi->handler = e_mod_quickpanel_handler_object_add(qp_ec, 0, 0, w, h);

   /* FIXME set stacking as a max temporally. */
   evas_object_layer_set(pi->handler, EVAS_LAYER_MAX);

   /* show handler object */
   if (!evas_object_visible_get(qp_ec->frame))
     evas_object_show(pi->handler);

   evas_object_event_callback_add(qp_ec->frame, EVAS_CALLBACK_SHOW,
                                  _quickpanel_client_evas_cb_show, pi);

   evas_object_event_callback_add(qp_ec->frame, EVAS_CALLBACK_HIDE,
                                  _quickpanel_client_evas_cb_hide, pi);

   E_CLIENT_HOOK_APPEND(indicator_hooks, E_CLIENT_HOOK_DEL,
                        _indicator_hook_client_del, pi);
}
