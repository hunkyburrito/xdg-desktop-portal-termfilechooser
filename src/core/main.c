#include "logger.h"
#include "xdpw.h"
#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

enum event_loop_fd {
    EVENT_LOOP_DBUS,
};

static const char service_name[] =
    "org.freedesktop.impl.portal.desktop.termfilechooser";

static int xdpw_usage(FILE *stream, int rc)
{
    static const char *usage =
        "Usage: xdg-desktop-portal-termfilechooser [options]\n"
        "\n"
        "    -l, --loglevel=<loglevel>        Select log level (default is "
        "ERROR).\n"
        "                                     QUIET, ERROR, WARN, INFO, DEBUG, "
        "TRACE\n"
        "    -c, --config=<config file>       Select config file.\n"
        "                                     (default is "
        "$XDG_CONFIG_HOME/xdg-desktop-portal-termfilechooser/config)\n"
        "    -r, --replace                    Replace a running instance.\n"
        "    -h, --help                       Get help (this text).\n"
        "\n";

    fprintf(stream, "%s", usage);
    return rc;
}

static int handle_name_lost(sd_bus_message *m, void *userdata,
                            sd_bus_error *ret_error)
{
    logprint(INFO, "dbus: lost name, closing connection");
    sd_bus_close(sd_bus_message_get_bus(m));
    return 1;
}

int main(int argc, char *argv[])
{
    struct xdpw_config config = {};
    char *configfile = NULL;
    enum LOGLEVEL loglevel = DEFAULT_LOGLEVEL;
    bool replace = false;

    static const char *shortopts = "l:o:c:f:rh";
    static const struct option longopts[] = {
        {"loglevel", required_argument, NULL, 'l'},
        {"config", required_argument, NULL, 'c'},
        {"replace", no_argument, NULL, 'r'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}};

    while (1) {
        int c = getopt_long(argc, argv, shortopts, longopts, NULL);
        if (c < 0) {
            break;
        }

        switch (c) {
            case 'l':
                loglevel = get_loglevel(optarg);
                break;
            case 'c':
                configfile = strdup(optarg);
                break;
            case 'r':
                replace = true;
                break;
            case 'h':
                return xdpw_usage(stdout, EXIT_SUCCESS);
            default:
                return xdpw_usage(stderr, EXIT_FAILURE);
        }
    }

    init_logger(stderr, loglevel);
    init_config(&configfile, &config);
    print_config(DEBUG, &config);

    int ret = 0;

    sd_bus *bus = NULL;
    ret = sd_bus_open_user(&bus);
    if (ret < 0) {
        logprint(ERROR, "dbus: failed to connect to user bus: %s",
                 strerror(-ret));
        return EXIT_FAILURE;
    }
    logprint(DEBUG, "dbus: connected");

    struct xdpw_state state = {
        .bus = bus,
        .config = &config,
    };

    xdpw_filechooser_init(&state);

    uint64_t flags = SD_BUS_NAME_ALLOW_REPLACEMENT;
    if (replace) {
        flags |= SD_BUS_NAME_REPLACE_EXISTING;
    }

    ret = sd_bus_request_name(bus, service_name, flags);
    if (ret < 0) {
        logprint(ERROR, "dbus: failed to acquire service name: %s",
                 strerror(-ret));
        goto error;
    }

    const char *unique_name;
    ret = sd_bus_get_unique_name(bus, &unique_name);
    if (ret < 0) {
        logprint(ERROR, "dbus: failed to get unique bus name: %s",
                 strerror(-ret));
        goto error;
    }

    static char match[1024];
    snprintf(match, sizeof(match),
             "sender='org.freedesktop.DBus',"
             "type='signal',"
             "interface='org.freedesktop.DBus',"
             "member='NameOwnerChanged',"
             "path='/org/freedesktop/DBus',"
             "arg0='%s',"
             "arg1='%s'",
             service_name, unique_name);

    sd_bus_slot *slot;
    ret = sd_bus_add_match(bus, &slot, match, handle_name_lost, NULL);
    if (ret < 0) {
        logprint(ERROR, "dbus: failed to add NameOwnerChanged signal match: %s",
                 strerror(-ret));
        goto error;
    }

    struct pollfd pollfds[] = {
        [EVENT_LOOP_DBUS] =
            {
                .fd = sd_bus_get_fd(state.bus),
                .events = POLLIN,
            },
    };

    while (1) {
        ret = poll(pollfds, sizeof(pollfds) / sizeof(pollfds[0]), -1);
        if (ret < 0) {
            logprint(ERROR, "poll failed: %s", strerror(errno));
            goto error;
        }

        if (pollfds[EVENT_LOOP_DBUS].revents & POLLIN) {
            logprint(TRACE, "event-loop: got dbus event");
            do {
                ret = sd_bus_process(state.bus, NULL);
            } while (ret > 0);
            if (ret < 0) {
                logprint(ERROR, "sd_bus_process failed: %s", strerror(-ret));
                goto error;
            }
        }

        logprint(TRACE, "flushing bus\n");
        sd_bus_flush(state.bus);
    }

    // TODO: cleanup
    finish_config(&config);
    free(configfile);

    return EXIT_SUCCESS;

error:
    sd_bus_unref(bus);
    return EXIT_FAILURE;
}
