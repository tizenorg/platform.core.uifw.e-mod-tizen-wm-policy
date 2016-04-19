#ifndef E_MOD_ROTATION_PRIVATE_H
#define E_MOD_ROTATION_PRIVATE_H

#ifdef DBG
# undef DBG
#endif

#ifdef INF
# undef INF
#endif

#ifdef WRN
# undef WRN
#endif

#ifdef ERR
# undef ERR
#endif

#ifdef CRI
# undef CRI
#endif

extern int _wr_log_dom;
#define DBG(...)  EINA_LOG_DOM_DBG(_wr_log_dom, __VA_ARGS__)
#define INF(...)  EINA_LOG_DOM_INFO(_wr_log_dom, __VA_ARGS__)
#define WRN(...)  EINA_LOG_DOM_WARN(_wr_log_dom, __VA_ARGS__)
#define ERR(...)  EINA_LOG_DOM_ERR(_wr_log_dom, __VA_ARGS__)
#define CRI(...)  EINA_LOG_DOM_CRIT(_wr_log_dom, __VA_ARGS__)

#define EINF(ec, f, x...)  \
   INF(f"|'%s'(RcsID %d)", ##x, ec->icccm.name?:"", e_pixmap_res_id_get(ec->pixmap));

#endif /* E_MOD_ROTATION_PRIVATE_H */
