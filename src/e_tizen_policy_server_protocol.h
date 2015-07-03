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
extern const struct wl_interface tizen_position_interface;
extern const struct wl_interface tizen_visibility_interface;
extern const struct wl_interface wl_surface_interface;

#ifndef TIZEN_POLICY_CONFORMANT_PART_ENUM
#define TIZEN_POLICY_CONFORMANT_PART_ENUM
enum tizen_policy_conformant_part {
	TIZEN_POLICY_CONFORMANT_PART_INDICATOR = 0,
	TIZEN_POLICY_CONFORMANT_PART_KEYBOARD = 1,
	TIZEN_POLICY_CONFORMANT_PART_CLIPBOARD = 2,
};
#endif /* TIZEN_POLICY_CONFORMANT_PART_ENUM */

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
	/**
	 * role_set - (none)
	 * @surface: surface object
	 * @role: (none)
	 */
	void (*role_set)(struct wl_client *client,
			 struct wl_resource *resource,
			 struct wl_resource *surface,
			 const char *role);
	/**
	 * conformant_set - (none)
	 * @surface: surface object
	 */
	void (*conformant_set)(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface);
	/**
	 * conformant_unset - (none)
	 * @surface: surface object
	 */
	void (*conformant_unset)(struct wl_client *client,
				 struct wl_resource *resource,
				 struct wl_resource *surface);
	/**
	 * conformant_get - (none)
	 * @surface: surface object
	 */
	void (*conformant_get)(struct wl_client *client,
			       struct wl_resource *resource,
			       struct wl_resource *surface);
};

#define TIZEN_POLICY_CONFORMANT	0
#define TIZEN_POLICY_CONFORMANT_AREA	1

#define TIZEN_POLICY_CONFORMANT_SINCE_VERSION	1
#define TIZEN_POLICY_CONFORMANT_AREA_SINCE_VERSION	1

static inline void
tizen_policy_send_conformant(struct wl_resource *resource_, struct wl_resource *surface, uint32_t is_conformant)
{
	wl_resource_post_event(resource_, TIZEN_POLICY_CONFORMANT, surface, is_conformant);
}

static inline void
tizen_policy_send_conformant_area(struct wl_resource *resource_, struct wl_resource *surface, uint32_t conformant_part, uint32_t state, int32_t x, int32_t y, int32_t w, int32_t h)
{
	wl_resource_post_event(resource_, TIZEN_POLICY_CONFORMANT_AREA, surface, conformant_part, state, x, y, w, h);
}

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
