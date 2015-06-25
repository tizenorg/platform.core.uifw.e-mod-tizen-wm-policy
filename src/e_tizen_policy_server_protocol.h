#ifndef TIZEN_POLICY_SERVER_PROTOCOL_H
#define TIZEN_POLICY_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

struct wl_client;
struct wl_resource;

struct tizen_policy;
struct tizen_position;
struct tizen_visibility;
struct wl_surface;

extern const struct wl_interface tizen_policy_interface;
extern const struct wl_interface tizen_visibility_interface;
extern const struct wl_interface tizen_position_interface;

struct tizen_policy_interface {
	/**
	 * get_visibility - (none)
	 * @id: new visibility object
	 * @surface: surface object
	 */
	void (*get_visibility)(struct wl_client *client,
			       struct wl_resource *resource,
			       uint32_t id,
			       struct wl_resource *surface);
	/**
	 * get_position - (none)
	 * @id: new position object
	 * @surface: surface object
	 */
	void (*get_position)(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t id,
			     struct wl_resource *surface);
	/**
	 * activate - (none)
	 * @surface: surface object
	 */
	void (*activate)(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *surface);
	/**
	 * lower - (none)
	 * @surface: surface object
	 */
	void (*lower)(struct wl_client *client,
		      struct wl_resource *resource,
		      struct wl_resource *surface);
	/**
	 * focus_skip_set - (none)
	 * @surface: surface object
	 */
	void (*focus_skip_set)(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface);
	/**
	 * focus_skip_unset - (none)
	 * @surface: surface object
	 */
	void (*focus_skip_unset)(struct wl_client *client,
				 struct wl_resource *resource,
				 struct wl_resource *surface);
};


#ifndef TIZEN_VISIBILITY_VISIBILITY_ENUM
#define TIZEN_VISIBILITY_VISIBILITY_ENUM
enum tizen_visibility_visibility {
	TIZEN_VISIBILITY_VISIBILITY_UNOBSCURED = 0,
	TIZEN_VISIBILITY_VISIBILITY_PARTIALLY_OBSCURED = 1,
	TIZEN_VISIBILITY_VISIBILITY_FULLY_OBSCURED = 2,
};
#endif /* TIZEN_VISIBILITY_VISIBILITY_ENUM */

struct tizen_visibility_interface {
	/**
	 * destroy - (none)
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
};

#define TIZEN_VISIBILITY_NOTIFY	0

#define TIZEN_VISIBILITY_NOTIFY_SINCE_VERSION	1

static inline void
tizen_visibility_send_notify(struct wl_resource *resource_, uint32_t visibility)
{
	wl_resource_post_event(resource_, TIZEN_VISIBILITY_NOTIFY, visibility);
}

struct tizen_position_interface {
	/**
	 * destroy - (none)
	 */
	void (*destroy)(struct wl_client *client,
			struct wl_resource *resource);
	/**
	 * set - (none)
	 * @x: (none)
	 * @y: (none)
	 */
	void (*set)(struct wl_client *client,
		    struct wl_resource *resource,
		    int32_t x,
		    int32_t y);
};

#define TIZEN_POSITION_CHANGED	0

#define TIZEN_POSITION_CHANGED_SINCE_VERSION	1

static inline void
tizen_position_send_changed(struct wl_resource *resource_, int32_t x, int32_t y)
{
	wl_resource_post_event(resource_, TIZEN_POSITION_CHANGED, x, y);
}

#ifdef  __cplusplus
}
#endif

#endif
