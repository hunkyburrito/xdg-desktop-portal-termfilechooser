#include "config.h"
#include "logger.h"
#include "xdptf.h"
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile bool keep_running = true;

void handle_sigterm(int sig) { keep_running = false; }

static const char service_name[] =
    "org.freedesktop.impl.portal.desktop.termfilechooser";

static int xdptf_usage(FILE *stream, int rc)
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
        "    -v, --version                    Print the current version.\n"
        "    -h, --help                       Get help (this text).\n"
        "\n";

    fprintf(stream, "%s", usage);
    return rc;
}

static int print_version(int rc)
{
    // Retrieve version from meson.build
    fprintf(stdout, "xdg-desktop-portal-termfilechooser %s\n", TERMFILECHOOSER_VERSION);
    return rc;
}

static int handle_name_lost(sd_bus_message *m, void *userdata,
                            sd_bus_error *ret_error)
{
    logprint(INFO, "dbus: lost name, closing connection");
    sd_bus_close(sd_bus_message_get_bus(m));
    return 1;
}

static int setup_sd_bus(sd_bus **bus, sd_bus_slot **slot,
                        const char *service_name, bool replace)
{
    int ret;

    ret = sd_bus_open_user(bus);
    if (ret < 0) {
        logprint(ERROR, "dbus: failed to connect to user bus: %s",
                 strerror(-ret));
        return ret;
    }
    logprint(DEBUG, "dbus: connected");

    uint64_t flags = SD_BUS_NAME_ALLOW_REPLACEMENT;
    if (replace) {
        flags |= SD_BUS_NAME_REPLACE_EXISTING;
    }

    ret = sd_bus_request_name(*bus, service_name, flags);
    if (ret < 0) {
        logprint(ERROR, "dbus: failed to acquire service name: %s",
                 strerror(-ret));
    }

    const char *unique_name;
    ret = sd_bus_get_unique_name(*bus, &unique_name);
    if (ret < 0) {
        logprint(ERROR, "dbus: failed to get unique bus name: %s",
                 strerror(-ret));
        return ret;
    }

    static char match[1024];
    snprintf(match, sizeof(match),
             "sender='org.freedesktop.DBus',"
             "type='signal',"
             "interface='org.freedesktop.DBus',"
             "member='NameOwnerChanged',"
             "path='/org/freedesktop/DBus',"
             "arg0='%s',"
             "arg1='%s',",
             service_name, unique_name);

    ret = sd_bus_add_match(*bus, slot, match, handle_name_lost, NULL);
    if (ret < 0) {
        logprint(ERROR, "dbus: failed to add NameOwnerChanged signal match: %s",
                 strerror(-ret));
        return ret;
    }

    return 0;
}

static void cleanup(sd_bus **bus, sd_bus_slot **slot, void *config,
                    char **configfile)
{
    sd_bus_slot_unref(*slot);
    *slot = NULL;

    sd_bus_close(*bus);
    sd_bus_unref(*bus);
    *bus = NULL;

    free_config(config);
    free(*configfile);
}

int main(int argc, char *argv[])
{
    signal(SIGTERM, handle_sigterm);
    signal(SIGINT, handle_sigterm);

    struct config_filechooser config = {0};
    char *configfile = NULL;
    enum LOGLEVEL loglevel = DEFAULT_LOGLEVEL;
    bool replace = false;

    static const char *shortopts = "l:c:rhv";
    static const struct option longopts[] = {
        {"loglevel", required_argument, NULL, 'l'},
        {"config", required_argument, NULL, 'c'},
        {"replace", no_argument, NULL, 'r'},
        {"version", no_argument, NULL, 'v'},
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
            case 'v':
                return print_version(EXIT_SUCCESS);
                break;
            case 'h':
                return xdptf_usage(stdout, EXIT_SUCCESS);
            default:
                return xdptf_usage(stderr, EXIT_FAILURE);
        }
    }

    init_logger(stderr, loglevel);
    init_config(&configfile, &config);
    print_config(DEBUG, &config);

    int ret;

    sd_bus *bus = NULL;
    sd_bus_slot *slot = NULL;
    ret = setup_sd_bus(&bus, &slot, service_name, replace);
    if (ret < 0) {
        cleanup(&bus, &slot, &config, &configfile);
        return EXIT_FAILURE;
    }

    struct xdptf_state state = {
        .bus = bus,
        .config = &config,
    };

    xdptf_filechooser_init(&state);

    while (keep_running) {
        ret = sd_bus_process(state.bus, NULL);
        if (ret < 0) {
            logprint(ERROR, "dbus: sd_bus_process failed: %s", strerror(-ret));
            break;
        }

        if (ret > 0)
            continue;

        ret = sd_bus_wait(state.bus, (uint64_t)-1);
        if (ret < 0) {
            logprint(ERROR, "dbus: sd_bus_wait failed: %s", strerror(-ret));
            break;
        }

        logprint(TRACE, "dbus: flushing bus");
        sd_bus_flush(state.bus);
    }

    cleanup(&bus, &slot, &config, &configfile);
    return EXIT_SUCCESS;
}
