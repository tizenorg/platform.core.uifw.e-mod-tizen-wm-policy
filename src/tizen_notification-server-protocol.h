#ifndef TIZEN_NOTIFICATION_SERVER_PROTOCOL_H
#define TIZEN_NOTIFICATION_SERVER_PROTOCOL_H

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "wayland-server.h"

struct wl_client;
struct wl_resource;

struct tizen_notification;

extern const struct wl_interface tizen_notification_interface;

#ifndef TIZEN_NOTIFICATION_LEVEL_ENUM
#define TIZEN_NOTIFICATION_LEVEL_ENUM
enum tizen_notification_level {
	TIZEN_NOTIFICATION_LEVEL_1 = 0,
	TIZEN_NOTIFICATION_LEVEL_2 = 1,
	TIZEN_NOTIFICATION_LEVEL_3 = 2,
};
#endif /* TIZEN_NOTIFICATION_LEVEL_ENUM */

#ifndef TIZEN_NOTIFICATION_ERROR_STATE_ENUM
#define TIZEN_NOTIFICATION_ERROR_STATE_ENUM
enum tizen_notification_error_state {
	TIZEN_NOTIFICATION_ERROR_STATE_NONE = 0,
	TIZEN_NOTIFICATION_ERROR_STATE_INVALID_PARAMETER = 1,
	TIZEN_NOTIFICATION_ERROR_STATE_OUT_OF_MEMORY = 2,
	TIZEN_NOTIFICATION_ERROR_STATE_PERMISSION_DENIED = 3,
	TIZEN_NOTIFICATION_ERROR_STATE_NOT_SUPPORTED_WINDOW_TYPE = 4,
};
#endif /* TIZEN_NOTIFICATION_ERROR_STATE_ENUM */

struct tizen_notification_interface {
	/**
	 * set_level - (none)
	 * @surface: (none)
	 * @level: (none)
	 */
	void (*set_level)(struct wl_client *client,
			  struct wl_resource *resource,
			  struct wl_resource *surface,
			  uint32_t level);
};

#define TIZEN_NOTIFICATION_DONE	0

#define TIZEN_NOTIFICATION_DONE_SINCE_VERSION	1

static inline void
tizen_notification_send_done(struct wl_resource *resource_, struct wl_resource *surface, uint32_t level, uint32_t error_state)
{
	wl_resource_post_event(resource_, TIZEN_NOTIFICATION_DONE, surface, level, error_state);
}

#ifdef  __cplusplus
}
#endif

#endif
