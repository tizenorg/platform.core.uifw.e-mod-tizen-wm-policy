/* 
 */

#ifndef TIZEN_WS_SHELL_SERVER_PROTOCOL_H
#define TIZEN_WS_SHELL_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

struct wl_client;
struct wl_resource;

struct tizen_ws_shell;
struct tws_quickpanel;
struct tws_region;
struct tws_service;
struct tws_tvsrv;

extern const struct wl_interface tizen_ws_shell_interface;
extern const struct wl_interface tws_quickpanel_interface;
extern const struct wl_interface tws_region_interface;
extern const struct wl_interface tws_service_interface;
extern const struct wl_interface tws_tvsrv_interface;

/**
 * tizen_ws_shell - Tizsn Shell support
 * @destroy: destroy tizen_ws_shell
 * @service_create: create new service
 * @region_create: create new region
 * @quickpanel_get: get the handle of quickpanel service
 * @tvsrv_get: get the handle of tvsrv service
 *
 * 
 */
struct tizen_ws_shell_interface {
	/**
	 * destroy - destroy tizen_ws_shell
	 *
	 * 
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * service_create - create new service
	 * @id: (none)
	 * @win: (none)
	 * @name: (none)
	 *
	 * 
	 */
	void (*service_create)(struct wl_client *client,
			       struct wl_resource *resource,
			       uint32_t id,
			       uint32_t win,
			       const char *name);
	/**
	 * region_create - create new region
	 * @id: (none)
	 *
	 * Ask the tzsh server to create a new region.
	 */
	void (*region_create)(struct wl_client *client,
			      struct wl_resource *resource,
			      uint32_t id);
	/**
	 * quickpanel_get - get the handle of quickpanel service
	 * @id: (none)
	 * @win: (none)
	 *
	 * 
	 */
	void (*quickpanel_get)(struct wl_client *client,
			       struct wl_resource *resource,
			       uint32_t id,
			       uint32_t win);
	/**
	 * tvsrv_get - get the handle of tvsrv service
	 * @id: (none)
	 * @win: (none)
	 *
	 * 
	 */
	void (*tvsrv_get)(struct wl_client *client,
			  struct wl_resource *resource,
			  uint32_t id,
			  uint32_t win);
};

#define TIZEN_WS_SHELL_SERVICE_REGISTER	0
#define TIZEN_WS_SHELL_SERVICE_UNREGISTER	1

#define TIZEN_WS_SHELL_SERVICE_REGISTER_SINCE_VERSION	1
#define TIZEN_WS_SHELL_SERVICE_UNREGISTER_SINCE_VERSION	1

static inline void
tizen_ws_shell_send_service_register(struct wl_resource *resource_, const char *name)
{
	wl_resource_post_event(resource_, TIZEN_WS_SHELL_SERVICE_REGISTER, name);
}

static inline void
tizen_ws_shell_send_service_unregister(struct wl_resource *resource_, const char *name)
{
	wl_resource_post_event(resource_, TIZEN_WS_SHELL_SERVICE_UNREGISTER, name);
}

#ifndef TWS_QUICKPANEL_ERROR_ENUM
#define TWS_QUICKPANEL_ERROR_ENUM
enum tws_quickpanel_error {
	TWS_QUICKPANEL_ERROR_REQUEST_REJECTED = 0,
};
#endif /* TWS_QUICKPANEL_ERROR_ENUM */

/**
 * tws_quickpanel - 
 * @release: release the handle of tws_quickpanel
 * @show: show the quickpanel window
 * @hide: hide the quickpanel window
 * @enable: enable the operation of quickpanel on user window
 * @disable: disable the operation of quickpanel on user window
 *
 * 
 */
struct tws_quickpanel_interface {
	/**
	 * release - release the handle of tws_quickpanel
	 *
	 * 
	 */
	void (*release)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * show - show the quickpanel window
	 *
	 * This request shows the quickpanel window immedietly, if it's
	 * possible. In other words, this request may be denied by server.
	 * Basically, this request will be denied, when application window
	 * is laying on background.
	 */
	void (*show)(struct wl_client *client,
		     struct wl_resource *resource);
	/**
	 * hide - hide the quickpanel window
	 *
	 * 
	 */
	void (*hide)(struct wl_client *client,
		     struct wl_resource *resource);
	/**
	 * enable - enable the operation of quickpanel on user window
	 *
	 * 
	 */
	void (*enable)(struct wl_client *client,
		       struct wl_resource *resource);
	/**
	 * disable - disable the operation of quickpanel on user window
	 *
	 * 
	 */
	void (*disable)(struct wl_client *client,
			struct wl_resource *resource);
};


/**
 * tws_region - region interface
 * @destroy: destroy region
 * @add: add rectangle to region
 * @subtract: subtract rectangle from region
 *
 * A region object describes an area.
 *
 * Region objects are used to describe the content and handler regions of a
 * service window.
 */
struct tws_region_interface {
	/**
	 * destroy - destroy region
	 *
	 * Destroy the region. This will invalidate the object ID.
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * add - add rectangle to region
	 * @x: (none)
	 * @y: (none)
	 * @w: (none)
	 * @h: (none)
	 *
	 * Add the specified rectangle to the region.
	 */
	void (*add)(struct wl_client *client,
		    struct wl_resource *resource,
		    int32_t x,
		    int32_t y,
		    int32_t w,
		    int32_t h);
	/**
	 * subtract - subtract rectangle from region
	 * @x: (none)
	 * @y: (none)
	 * @w: (none)
	 * @h: (none)
	 *
	 * Subtract the specified rectangle from the region.
	 */
	void (*subtract)(struct wl_client *client,
			 struct wl_resource *resource,
			 int32_t x,
			 int32_t y,
			 int32_t w,
			 int32_t h);
};


#ifndef TWS_SERVICE_REGION_TYPE_ENUM
#define TWS_SERVICE_REGION_TYPE_ENUM
enum tws_service_region_type {
	TWS_SERVICE_REGION_TYPE_HANDLER = 0,
	TWS_SERVICE_REGION_TYPE_CONTENT = 1,
};
#endif /* TWS_SERVICE_REGION_TYPE_ENUM */

/**
 * tws_service - 
 * @destroy: destroy service
 * @region_set: (none)
 *
 * 
 */
struct tws_service_interface {
	/**
	 * destroy - destroy service
	 *
	 * 
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * region_set - (none)
	 * @type: (none)
	 * @angle: (none)
	 * @region: (none)
	 */
	void (*region_set)(struct wl_client *client,
			   struct wl_resource *resource,
			   int32_t type,
			   int32_t angle,
			   struct wl_resource *region);
};


#ifndef TWS_TVSRV_ERROR_ENUM
#define TWS_TVSRV_ERROR_ENUM
enum tws_tvsrv_error {
	TWS_TVSRV_ERROR_REQUEST_REJECTED = 0,
};
#endif /* TWS_TVSRV_ERROR_ENUM */

/**
 * tws_tvsrv - 
 * @release: release the handle of tws_quickpanel
 * @bind: (none)
 *
 * 
 */
struct tws_tvsrv_interface {
	/**
	 * release - release the handle of tws_quickpanel
	 *
	 * 
	 */
	void (*release)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * bind - (none)
	 */
	void (*bind)(struct wl_client *client,
		     struct wl_resource *resource);
};


#ifdef  __cplusplus
}
#endif

#endif
