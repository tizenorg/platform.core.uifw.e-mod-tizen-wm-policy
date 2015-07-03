#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

extern const struct wl_interface tizen_position_interface;
extern const struct wl_interface tizen_visibility_interface;
extern const struct wl_interface wl_surface_interface;

static const struct wl_interface *types[] = {
	NULL,
	NULL,
	&tizen_visibility_interface,
	&wl_surface_interface,
	&tizen_position_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	NULL,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	NULL,
	&wl_surface_interface,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static const struct wl_message tizen_policy_requests[] = {
	{ "get_visibility", "no", types + 2 },
	{ "get_position", "no", types + 4 },
	{ "activate", "o", types + 6 },
	{ "lower", "o", types + 7 },
	{ "focus_skip_set", "o", types + 8 },
	{ "focus_skip_unset", "o", types + 9 },
	{ "role_set", "os", types + 10 },
	{ "conformant_set", "o", types + 12 },
	{ "conformant_unset", "o", types + 13 },
	{ "conformant_get", "o", types + 14 },
};

static const struct wl_message tizen_policy_events[] = {
	{ "conformant", "ou", types + 15 },
	{ "conformant_area", "ouuiiii", types + 17 },
};

WL_EXPORT const struct wl_interface tizen_policy_interface = {
	"tizen_policy", 1,
	10, tizen_policy_requests,
	2, tizen_policy_events,
};

static const struct wl_message tizen_visibility_requests[] = {
	{ "destroy", "", types + 0 },
};

static const struct wl_message tizen_visibility_events[] = {
	{ "notify", "u", types + 0 },
};

WL_EXPORT const struct wl_interface tizen_visibility_interface = {
	"tizen_visibility", 1,
	1, tizen_visibility_requests,
	1, tizen_visibility_events,
};

static const struct wl_message tizen_position_requests[] = {
	{ "destroy", "", types + 0 },
	{ "set", "ii", types + 0 },
};

static const struct wl_message tizen_position_events[] = {
	{ "changed", "ii", types + 0 },
};

WL_EXPORT const struct wl_interface tizen_position_interface = {
	"tizen_position", 1,
	2, tizen_position_requests,
	1, tizen_position_events,
};

