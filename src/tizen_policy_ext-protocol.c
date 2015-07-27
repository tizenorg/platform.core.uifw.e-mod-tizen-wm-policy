#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

extern const struct wl_interface tizen_rotation_interface;
extern const struct wl_interface wl_surface_interface;

static const struct wl_interface *types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	&tizen_rotation_interface,
	&wl_surface_interface,
};

static const struct wl_message tizen_policy_ext_requests[] = {
	{ "get_rotation", "no", types + 4 },
};

WL_EXPORT const struct wl_interface tizen_policy_ext_interface = {
	"tizen_policy_ext", 1,
	1, tizen_policy_ext_requests,
	0, NULL,
};

static const struct wl_message tizen_rotation_requests[] = {
	{ "set_available_angles", "u", types + 0 },
	{ "set_preferred_angle", "u", types + 0 },
	{ "ack_angle_change", "u", types + 0 },
	{ "set_geometry_hints", "a", types + 0 },
};

static const struct wl_message tizen_rotation_events[] = {
	{ "available_angles_done", "u", types + 0 },
	{ "preferred_angle_done", "u", types + 0 },
	{ "angle_change", "uiiu", types + 0 },
};

WL_EXPORT const struct wl_interface tizen_rotation_interface = {
	"tizen_rotation", 1,
	4, tizen_rotation_requests,
	3, tizen_rotation_events,
};

