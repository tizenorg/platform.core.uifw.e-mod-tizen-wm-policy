#include "e_mod_main.h"

typedef struct _Client_Type Client_Type;

struct _Client_Type
{
   E_Client *ec;
   Pol_Client_Type pol_client_type;
};

static Eina_Hash *hash_pol_client_types = NULL;

static E_Window_Type _e_window_type_get(E_Client* ec);
static Eina_Bool     _client_type_match(E_Client *ec, Pol_Client_Type_Match *m);
static Client_Type  *_client_type_get(E_Client *ec);
static void          _cb_client_type_data_free(void *data);

static E_Window_Type
_e_window_type_get(E_Client* ec)
{
   E_Window_Type type = E_WINDOW_TYPE_UNKNOWN;
   Ecore_X_Window_Type *types = NULL;
   int num, i;

   if (!e_pixmap_is_x(ec->pixmap)) return type;

   num = ecore_x_netwm_window_types_get(e_client_util_win_get(ec), &types);
   if (num == 0)
     return type;
   else
     {
        i = 0;
        type = types[i];
        i++;
        while ((i < num) && (type == E_WINDOW_TYPE_UNKNOWN))
          {
             i++;
             type = types[i];
          }
        free(types);
     }

   return type;
}

static Eina_Bool
_client_type_match(E_Client *ec, Pol_Client_Type_Match *m)
{
   char *nname = NULL, *nclass = NULL;

   if (!ec || !m) return EINA_FALSE;
   if (e_object_is_del(E_OBJECT(ec))) return EINA_FALSE;

   ecore_x_icccm_name_class_get(e_client_util_win_get(ec), &nname, &nclass);

   if (((m->clas) && (!nclass)) ||
       ((nclass) && (m->clas) && (!e_util_glob_match(nclass, m->clas))))
     goto error_cleanup;

   if (((m->name) && (!nname)) ||
       ((nname) && (m->name) && (!e_util_glob_match(nname, m->name))))
     goto error_cleanup;

   if ((int)_e_window_type_get(ec) != m->e_window_type)
     goto error_cleanup;

   if (nname) free(nname);
   if (nclass) free(nclass);
   return EINA_TRUE;

error_cleanup:
   if (nname) free(nname);
   if (nclass) free(nclass);
   return EINA_FALSE;
}

static Client_Type *
_client_type_get(E_Client *ec)
{
   Client_Type *ct;
   ct = eina_hash_find(hash_pol_client_types, &ec);
   return ct;
}

static void
_cb_client_type_data_free(void *data)
{
   free(data);
}

EINTERN void
e_mod_pol_client_type_setup(E_Client *ec)
{
   Eina_List *list = NULL, *l;
   Pol_Client_Type_Match *m;
   Client_Type *ct;

   if (!_pol_mod) return;
   if (!_pol_mod->conf) return;
   if (!_pol_mod->conf->client_types) return;

   list =  _pol_mod->conf->client_types;

   ct = eina_hash_find(hash_pol_client_types, &ec);
   if (!ct)
     {
        ct = E_NEW(Client_Type, 1);
        if (!ct) return;
     }

   ct->ec = ec;
   ct->pol_client_type = POL_CLIENT_TYPE_UNKNOWN;

   EINA_LIST_FOREACH(list, l, m)
     {
        if (!_client_type_match(ec, m)) continue;
        else
          {
             ct->pol_client_type = m->pol_client_type;
             break;
          }
     }

   eina_hash_add(hash_pol_client_types, &ec, ct);
}

EINTERN void
e_mod_pol_client_type_init(void)
{
   hash_pol_client_types = eina_hash_pointer_new(_cb_client_type_data_free);
}

EINTERN void
e_mod_pol_client_type_shutdown(void)
{
   E_FREE_FUNC(hash_pol_client_types, eina_hash_free);
}

EINTERN void
e_mod_pol_client_type_del(E_Client *ec)
{
   Client_Type *ct;
   if (!ec) return;

   ct = eina_hash_find(hash_pol_client_types, &ec);
   if (!ct) return;

   eina_hash_del_by_key(hash_pol_client_types, &ec);
}
