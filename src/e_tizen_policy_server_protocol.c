#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

extern const struct wl_interface tizen_visibility_interface;
extern const struct wl_interface wl_surface_interface;

static const struct wl_interface *types[] = {
	NULL,
	&tizen_visibility_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	NULL,
	NULL,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	&wl_surface_interface,
	NULL,
};

static const struct wl_message tizen_policy_requests[] = {
	{ "get_visibility", "no", types + 1 },
	{ "activate", "o", types + 3 },
	{ "position_set", "oii", types + 4 },
	{ "focus_skip_set", "o", types + 7 },
	{ "focus_skip_unset", "o", types + 8 },
	{ "conformant_set", "o", types + 9 },
	{ "conformant_unset", "o", types + 10 },
	{ "conformant_get", "o", types + 11 },
};

static const struct wl_message tizen_policy_events[] = {
	{ "conformant", "ou", types + 12 },
};

WL_EXPORT const struct wl_interface tizen_policy_interface = {
	"tizen_policy", 1,
	8, tizen_policy_requests,
	1, tizen_policy_events,
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

