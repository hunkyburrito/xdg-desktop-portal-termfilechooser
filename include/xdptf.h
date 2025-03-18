#ifndef XDPTF_H
#define XDPTF_H

#ifdef HAVE_LIBSYSTEMD
#include <systemd/sd-bus.h>
#elif HAVE_LIBELOGIND
#include <elogind/sd-bus.h>
#elif HAVE_BASU
#include <basu/sd-bus.h>
#endif

#include "config.h"

struct xdptf_state {
    sd_bus *bus;
    struct config_filechooser *config;
};

struct xdptf_request {
    sd_bus_slot *slot;
};

enum {
    PORTAL_RESPONSE_SUCCESS = 0,
    PORTAL_RESPONSE_CANCELLED = 1,
    PORTAL_RESPONSE_ENDED = 2
};

int xdptf_filechooser_init(struct xdptf_state *state);

struct xdptf_request *xdptf_request_create(sd_bus *bus, const char *object_path);
void xdptf_request_destroy(struct xdptf_request *req);

#endif
