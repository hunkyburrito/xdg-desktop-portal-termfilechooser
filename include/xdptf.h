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
#include <stddef.h>

struct xdptf_state {
    sd_bus *bus;
    struct config_filechooser *config;
};

struct xdptf_request {
    sd_bus_slot *slot;
    /* called when the frontend sends Request.Close while a dialog is open */
    void (*close_cb)(void *data);
    void *close_data;
};

enum {
    PORTAL_RESPONSE_SUCCESS = 0,
    PORTAL_RESPONSE_CANCELLED = 1,
    PORTAL_RESPONSE_ENDED = 2
};

int xdptf_filechooser_init(struct xdptf_state *state);

/* Dialogs run asynchronously; the main loop drives them with these. */
struct pollfd;
size_t xdptf_filechooser_pending_count(void);
size_t xdptf_filechooser_pollfds(struct pollfd *fds);
void xdptf_filechooser_reap(void);
void xdptf_filechooser_shutdown(void);

struct xdptf_request *xdptf_request_create(sd_bus *bus, const char *object_path);
void xdptf_request_destroy(struct xdptf_request *req);

#endif
