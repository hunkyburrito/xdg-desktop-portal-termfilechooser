#ifndef XDPW_H
#define XDPW_H

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_LIBELOGIND
#include <elogind/sd-bus.h>
#elif HAVE_BASU
#include <basu/sd-bus.h>
#endif

#include <stdbool.h>

#include "config.h"

struct xdpw_state {
    sd_bus *bus;
    struct xdpw_config *config;
};

struct xdpw_request {
    sd_bus_slot *slot;
};

struct xdpw_session {
    sd_bus_slot *slot;
    char *session_handle;
};

typedef void (*xdpw_event_loop_timer_func_t)(void *data);

enum {
    PORTAL_RESPONSE_SUCCESS = 0,
    PORTAL_RESPONSE_CANCELLED = 1,
    PORTAL_RESPONSE_ENDED = 2
};

int xdpw_filechooser_init(struct xdpw_state *state);

struct xdpw_request *xdpw_request_create(sd_bus *bus, const char *object_path);
void xdpw_request_destroy(struct xdpw_request *req);

struct xdpw_session *xdpw_session_create(struct xdpw_state *state, sd_bus *bus, char *object_path);
void xdpw_session_destroy(struct xdpw_session *req);

struct xdpw_timer *xdpw_add_timer(struct xdpw_state *state,
        uint64_t delay_ns, xdpw_event_loop_timer_func_t func, void *data);

void xdpw_destroy_timer(struct xdpw_timer *timer);

#endif
