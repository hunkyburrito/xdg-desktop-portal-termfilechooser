#include "logger.h"
#include "xdptf.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static const char interface_name[] = "org.freedesktop.impl.portal.Request";

static int method_close(sd_bus_message *msg, void *data,
                        sd_bus_error *ret_error)
{
    struct xdptf_request *req = data;
    int ret = 0;
    logprint(INFO, "dbus: request closed");

    sd_bus_message *reply = NULL;
    ret = sd_bus_message_new_method_return(msg, &reply);
    if (ret < 0) {
        return ret;
    }

    ret = sd_bus_send(NULL, reply, NULL);
    if (ret < 0) {
        return ret;
    }

    sd_bus_message_unref(reply);

    xdptf_request_destroy(req);

    return 0;
}

static const sd_bus_vtable request_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Close", "", "", method_close, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

struct xdptf_request *xdptf_request_create(sd_bus *bus, const char *object_path)
{
    struct xdptf_request *req = calloc(1, sizeof(struct xdptf_request));

    int ret;
    ret = sd_bus_add_object_vtable(bus, &req->slot, object_path, interface_name,
                                   request_vtable, NULL);
    if (ret < 0) {
        free(req);
        logprint(ERROR, "dbus: sd_bus_add_object_vtable failed: %s",
                 strerror(-errno));
        return NULL;
    }

    return req;
}

void xdptf_request_destroy(struct xdptf_request *req)
{
    if (req == NULL) {
        return;
    }
    sd_bus_slot_unref(req->slot);
    free(req);
}
