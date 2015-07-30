/* 
 */

#include <stdlib.h>
#include <stdint.h>
#include "wayland-util.h"

extern const struct wl_interface tws_quickpanel_interface;
extern const struct wl_interface tws_region_interface;
extern const struct wl_interface tws_service_interface;
extern const struct wl_interface tws_tvsrv_interface;

static const struct wl_interface *types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	&tws_service_interface,
	NULL,
	NULL,
	&tws_region_interface,
	&tws_quickpanel_interface,
	NULL,
	&tws_tvsrv_interface,
	NULL,
	NULL,
	NULL,
	&tws_region_interface,
};

static const struct wl_message tizen_ws_shell_requests[] = {
	{ "destroy", "", types + 0 },
	{ "service_create", "nus", types + 4 },
	{ "region_create", "n", types + 7 },
	{ "quickpanel_get", "nu", types + 8 },
	{ "tvsrv_get", "nu", types + 10 },
};

static const struct wl_message tizen_ws_shell_events[] = {
	{ "service_register", "s", types + 0 },
	{ "service_unregister", "s", types + 0 },
};

WL_EXPORT const struct wl_interface tizen_ws_shell_interface = {
	"tizen_ws_shell", 1,
	5, tizen_ws_shell_requests,
	2, tizen_ws_shell_events,
};

static const struct wl_message tws_quickpanel_requests[] = {
	{ "release", "", types + 0 },
	{ "show", "", types + 0 },
	{ "hide", "", types + 0 },
	{ "enable", "", types + 0 },
	{ "disable", "", types + 0 },
};

WL_EXPORT const struct wl_interface tws_quickpanel_interface = {
	"tws_quickpanel", 1,
	5, tws_quickpanel_requests,
	0, NULL,
};

static const struct wl_message tws_region_requests[] = {
	{ "destroy", "", types + 0 },
	{ "add", "iiii", types + 0 },
	{ "subtract", "iiii", types + 0 },
};

WL_EXPORT const struct wl_interface tws_region_interface = {
	"tws_region", 1,
	3, tws_region_requests,
	0, NULL,
};

static const struct wl_message tws_service_requests[] = {
	{ "destroy", "", types + 0 },
	{ "region_set", "iio", types + 12 },
};

WL_EXPORT const struct wl_interface tws_service_interface = {
	"tws_service", 1,
	2, tws_service_requests,
	0, NULL,
};

static const struct wl_message tws_tvsrv_requests[] = {
	{ "release", "", types + 0 },
	{ "bind", "", types + 0 },
};

WL_EXPORT const struct wl_interface tws_tvsrv_interface = {
	"tws_tvsrv", 1,
	2, tws_tvsrv_requests,
	0, NULL,
};

