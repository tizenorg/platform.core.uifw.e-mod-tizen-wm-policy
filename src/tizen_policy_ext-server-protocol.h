#ifndef TIZEN_POLICY_EXT_SERVER_PROTOCOL_H
#define TIZEN_POLICY_EXT_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

struct wl_client;
struct wl_resource;

struct tizen_policy_ext;
struct tizen_rotation;

extern const struct wl_interface tizen_policy_ext_interface;
extern const struct wl_interface tizen_rotation_interface;

struct tizen_policy_ext_interface {
	/**
	 * get_rotation - (none)
	 * @id: new rotation object
	 * @surface: surface object
	 */
	void (*get_rotation)(struct wl_client *client,
			     struct wl_resource *resource,
			     uint32_t id,
			     struct wl_resource *surface);
};


#ifndef TIZEN_ROTATION_ANGLE_ENUM
#define TIZEN_ROTATION_ANGLE_ENUM
enum tizen_rotation_angle {
	TIZEN_ROTATION_ANGLE_0 = 1,
	TIZEN_ROTATION_ANGLE_90 = 2,
	TIZEN_ROTATION_ANGLE_180 = 4,
	TIZEN_ROTATION_ANGLE_270 = 8,
};
#endif /* TIZEN_ROTATION_ANGLE_ENUM */

struct tizen_rotation_interface {
	/**
	 * set_available_angles - (none)
	 * @angles: (none)
	 */
	void (*set_available_angles)(struct wl_client *client,
				     struct wl_resource *resource,
				     uint32_t angles);
	/**
	 * set_preferred_angles - (none)
	 * @angles: (none)
	 */
	void (*set_preferred_angles)(struct wl_client *client,
				     struct wl_resource *resource,
				     uint32_t angles);
	/**
	 * ack_angle_change - ack a angle_change
	 * @serial: a serial to angle_change for
	 *
	 * 
	 */
	void (*ack_angle_change)(struct wl_client *client,
				 struct wl_resource *resource,
				 uint32_t serial);
};

#define TIZEN_ROTATION_AVAILABLE_ANGLES_DONE	0
#define TIZEN_ROTATION_PREFERRED_ANGLES_DONE	1
#define TIZEN_ROTATION_ANGLE_CHANGE	2

#define TIZEN_ROTATION_AVAILABLE_ANGLES_DONE_SINCE_VERSION	1
#define TIZEN_ROTATION_PREFERRED_ANGLES_DONE_SINCE_VERSION	1
#define TIZEN_ROTATION_ANGLE_CHANGE_SINCE_VERSION	1

static inline void
tizen_rotation_send_available_angles_done(struct wl_resource *resource_, uint32_t angles)
{
	wl_resource_post_event(resource_, TIZEN_ROTATION_AVAILABLE_ANGLES_DONE, angles);
}

static inline void
tizen_rotation_send_preferred_angles_done(struct wl_resource *resource_, uint32_t angles)
{
	wl_resource_post_event(resource_, TIZEN_ROTATION_PREFERRED_ANGLES_DONE, angles);
}

static inline void
tizen_rotation_send_angle_change(struct wl_resource *resource_, uint32_t angle, uint32_t serial)
{
	wl_resource_post_event(resource_, TIZEN_ROTATION_ANGLE_CHANGE, angle, serial);
}

#ifdef  __cplusplus
}
#endif

#endif
