#include "uri.h"
#include "xdpw.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PATH_PREFIX "file://"
#define PATH_PORTAL_BASE "/tmp/termfilechooser"

static const char object_path[] = "/org/freedesktop/portal/desktop";
static const char interface_name[] = "org.freedesktop.impl.portal.FileChooser";

static int exec_filechooser(void *data, bool writing, bool multiple,
                            bool directory, char *path, char ***selected_files,
                            size_t *num_selected_files)
{
    struct xdpw_state *state = data;
    char *cmd_script = state->config->filechooser_conf.cmd;
    if (!cmd_script) {
        logprint(ERROR, "filechooser: cmd not specified");
        return -1;
    }

    if (path == NULL) {
        path = "";
    }

    uid_t uid = getuid();
    size_t filename_size =
        1 + snprintf(NULL, 0, "%s-%u.portal", PATH_PORTAL_BASE, uid);
    char *filename = malloc(filename_size);
    snprintf(filename, filename_size, "%s-%u.portal", PATH_PORTAL_BASE, uid);

    size_t str_size =
        1 + snprintf(NULL, 0, "%s %d %d %d \'%s\' \'%s\'", cmd_script, multiple,
                     directory, writing, path, filename);
    char *cmd = malloc(str_size);
    snprintf(cmd, str_size, "%s %d %d %d \'%s\' \'%s\'", cmd_script, multiple,
             directory, writing, path, filename);

    struct environment *env = state->config->filechooser_conf.env;

    if (access(filename, F_OK) == 0) {
        // clear contents
        FILE *fp = fopen(filename, "w");
        if (fp == NULL) {
            logprint(ERROR, "filechooser: could not open '%s' for clearing.");
            free(filename);
            return -1;
        }
        if (fclose(fp) != 0) {
            logprint(ERROR,
                     "filechooser: could not close '%s' after clearing.");
            free(filename);
            return -1;
        }
    }

    for (int i = 0, ret = 0; i < env->num_vars; i++) {
        logprint(TRACE, "filechooser: setting env: %s=%s", env->vars[i].name,
                 env->vars[i].value);
        ret = setenv(env->vars[i].name, env->vars[i].value, 1);
        if (ret) {
            logprint(WARN, "filechooser: could not set env '%s': %s",
                     env->vars[i].name, strerror(errno));
        }
    }

    logprint(TRACE, "filechooser: executing command '%s'", cmd);
    int ret = system(cmd);
    if (ret) {
        logprint(ERROR, "filechooser: could not execute '%s': %s", cmd,
                 strerror(errno));
        free(cmd);
        return -1;
    }
    free(cmd);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        logprint(ERROR, "filechooser: failed to open '%s' in read mode.",
                 filename);
        free(filename);
        return -1;
    }

    size_t nchars = 0, num_lines = 0;
    int cr;
    while ((cr = getc(fp)) != EOF) {
        if (cr == '\n') {
            if (nchars > 0)
                num_lines++;
            nchars = 0;
        } else {
            nchars++;
        }

        if (ferror(fp)) {
            fclose(fp);
            return -1;
        }
    }

    // last line
    if (nchars > 0)
        num_lines++;

    // rewind
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    if (num_lines == 0) {
        fclose(fp);
        return -1;
    }

    *num_selected_files = num_lines;
    *selected_files = malloc((1 + num_lines) * sizeof(char *));

    for (size_t i = 0; i < num_lines; i++) {
        size_t n = 0;
        char *line = NULL;
        char *encoded = NULL;
        ssize_t nread = getline(&line, &n, fp);
        if (ferror(fp)) {
            free(line);
            for (size_t j = 0; j < i - 1; j++) {
                free((*selected_files)[j]);
            }
            free(*selected_files);
            fclose(fp);
            return -1;
        }
        // if all chars are encoded, size = orig_size * 3 + 1
        encoded = malloc(1 + nread * 3);
        size_t nenc = uri_encode(line, nread, encoded);
        size_t str_size = 1 + nenc + strlen(PATH_PREFIX);
        // check last char equal '\n'
        if (nenc >= 3 && !strcmp(encoded + nenc - 3, "%0A")) {
            str_size -= 3;
        }
        (*selected_files)[i] = malloc(str_size);
        snprintf((*selected_files)[i], str_size, "%s%s", PATH_PREFIX, encoded);
        free(line);
        free(encoded);
    }
    (*selected_files)[num_lines] = NULL;

    fclose(fp);
    free(filename);
    return 0;
}

static int method_open_file(sd_bus_message *msg, void *data,
                            sd_bus_error *ret_error)
{
    int ret = 0;

    char *handle, *app_id, *parent_window, *title;
    ret = sd_bus_message_read(msg, "osss", &handle, &app_id, &parent_window,
                              &title);
    if (ret < 0) {
        return ret;
    }

    ret = sd_bus_message_enter_container(msg, 'a', "{sv}");
    if (ret < 0) {
        return ret;
    }
    char *key;
    int inner_ret = 0;
    int multiple = 0, directory = 0;
    char *current_folder = NULL;
    while ((ret = sd_bus_message_enter_container(msg, 'e', "sv")) > 0) {
        inner_ret = sd_bus_message_read(msg, "s", &key);
        if (inner_ret < 0) {
            return inner_ret;
        }

        logprint(DEBUG, "dbus: option %s", key);
        if (strcmp(key, "multiple") == 0) {
            sd_bus_message_read(msg, "v", "b", &multiple);
            logprint(DEBUG, "dbus: option multiple: %d", multiple);
        } else if (strcmp(key, "modal") == 0) {
            int modal;
            sd_bus_message_read(msg, "v", "b", &modal);
            logprint(DEBUG, "dbus: option modal: %d", modal);
        } else if (strcmp(key, "directory") == 0) {
            sd_bus_message_read(msg, "v", "b", &directory);
            logprint(DEBUG, "dbus: option directory: %d", directory);
        } else if (strcmp(key, "current_folder") == 0) {
            const void *p = NULL;
            size_t sz = 0;
            inner_ret = sd_bus_message_enter_container(msg, 'v', "ay");
            if (inner_ret < 0) {
                return inner_ret;
            }
            inner_ret = sd_bus_message_read_array(msg, 'y', &p, &sz);
            if (inner_ret < 0) {
                return inner_ret;
            }
            current_folder = (char *)p;
            logprint(DEBUG, "dbus: option current_folder: %s", current_folder);
        } else {
            logprint(WARN, "dbus: unknown option %s", key);
            sd_bus_message_skip(msg, "v");
        }

        inner_ret = sd_bus_message_exit_container(msg);
        if (inner_ret < 0) {
            return inner_ret;
        }
    }
    if (ret < 0) {
        return ret;
    }
    ret = sd_bus_message_exit_container(msg);
    if (ret < 0) {
        return ret;
    }

    struct xdpw_request *req =
        xdpw_request_create(sd_bus_message_get_bus(msg), handle);
    if (req == NULL) {
        return -ENOMEM;
    }

    char **selected_files = NULL;
    size_t num_selected_files = 0;
    if (current_folder == NULL) {
        struct xdpw_state *state = data;
        char *default_dir = state->config->filechooser_conf.default_dir;
        if (!default_dir) {
            logprint(ERROR, "filechooser: default_dir not specified");
            return -1;
        }
        current_folder = default_dir;
    }
    ret = exec_filechooser(data, false, multiple, directory, current_folder,
                           &selected_files, &num_selected_files);
    if (ret) {
        goto cleanup;
    }

    logprint(TRACE, "filechooser: (OpenFile) Number of selected files: %d",
             num_selected_files);
    for (size_t i = 0; i < num_selected_files; i++) {
        logprint(TRACE, "filechooser: %d. %s", i, selected_files[i]);
    }

    sd_bus_message *reply = NULL;
    ret = sd_bus_message_new_method_return(msg, &reply);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_append(reply, "u", PORTAL_RESPONSE_SUCCESS, 1);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_open_container(reply, 'a', "{sv}");
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_open_container(reply, 'e', "sv");
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_append_basic(reply, 's', "uris");
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_open_container(reply, 'v', "as");
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_append_strv(reply, selected_files);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_close_container(reply);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_close_container(reply);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_close_container(reply);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_send(NULL, reply, NULL);
    if (ret < 0) {
        goto cleanup;
    }

    sd_bus_message_unref(reply);

cleanup:
    for (size_t i = 0; i < num_selected_files; i++) {
        free(selected_files[i]);
    }
    free(selected_files);

    xdpw_request_destroy(req);
    return ret;
}

static int method_save_file(sd_bus_message *msg, void *data,
                            sd_bus_error *ret_error)
{
    int ret = 0;

    char *handle, *app_id, *parent_window, *title;
    ret = sd_bus_message_read(msg, "osss", &handle, &app_id, &parent_window,
                              &title);
    if (ret < 0) {
        return ret;
    }

    ret = sd_bus_message_enter_container(msg, 'a', "{sv}");
    if (ret < 0) {
        return ret;
    }
    char *key;
    int inner_ret = 0;
    char *current_name;
    char *current_folder = NULL;
    while ((ret = sd_bus_message_enter_container(msg, 'e', "sv")) > 0) {
        inner_ret = sd_bus_message_read(msg, "s", &key);
        if (inner_ret < 0) {
            return inner_ret;
        }

        logprint(DEBUG, "dbus: option %s", key);
        if (strcmp(key, "current_name") == 0) {
            sd_bus_message_read(msg, "v", "s", &current_name);
            logprint(DEBUG, "dbus: option current_name: %s", current_name);
        } else if (strcmp(key, "current_folder") == 0) {
            const void *p = NULL;
            size_t sz = 0;
            inner_ret = sd_bus_message_enter_container(msg, 'v', "ay");
            if (inner_ret < 0) {
                return inner_ret;
            }
            inner_ret = sd_bus_message_read_array(msg, 'y', &p, &sz);
            if (inner_ret < 0) {
                return inner_ret;
            }
            current_folder = (char *)p;
            logprint(DEBUG, "dbus: option current_folder: %s", current_folder);
        } else if (strcmp(key, "current_file") == 0) {
            // saving existing file
            const void *p = NULL;
            size_t sz = 0;
            inner_ret = sd_bus_message_enter_container(msg, 'v', "ay");
            if (inner_ret < 0) {
                return inner_ret;
            }
            inner_ret = sd_bus_message_read_array(msg, 'y', &p, &sz);
            if (inner_ret < 0) {
                return inner_ret;
            }
            current_name = (char *)p;
            logprint(DEBUG, "dbus: option replace current_name with current_file: %s", current_name);
        } else {
            logprint(WARN, "dbus: unknown option %s", key);
            sd_bus_message_skip(msg, "v");
        }

        inner_ret = sd_bus_message_exit_container(msg);
        if (inner_ret < 0) {
            return inner_ret;
        }
    }

    struct xdpw_request *req =
        xdpw_request_create(sd_bus_message_get_bus(msg), handle);
    if (req == NULL) {
        return -ENOMEM;
    }

    if (current_folder == NULL) {
        struct xdpw_state *state = data;
        char *default_dir = state->config->filechooser_conf.default_dir;
        if (!default_dir) {
            logprint(ERROR, "filechooser: default_dir not specified");
            return -1;
        }
        current_folder = default_dir;
    }

    size_t path_size = 2 + strlen(current_folder) + strlen(current_name);
    char *path = malloc(path_size);
    snprintf(path, path_size, "%s/%s", current_folder, current_name);

    // escape ' with '\'' in path
    char *tmp = path;
    size_t escape_size = 0;
    while (*tmp) {
        escape_size += (*tmp == '\'') ? 4 : 1;
        tmp++;
    }
    escape_size += 1;
    char *escaped_path = malloc(escape_size);
    char *ptr = escaped_path;
    tmp = path;
    while (*tmp) {
        if (*tmp == '\'') {
            *ptr++ = '\'';
            *ptr++ = '\\';
            *ptr++ = '\'';
            *ptr++ = '\'';
        } else {
            *ptr++ = *tmp;
        }
        tmp++;
    }
    *ptr = '\0';

    free(path);
    path = escaped_path;
    path_size = escape_size;

    while (access(path, F_OK) == 0) {
        char *path_tmp = malloc(path_size);
        snprintf(path_tmp, path_size, "%s", path);
        path_size += 1;
        path = realloc(path, path_size);
        snprintf(path, path_size, "%s_", path_tmp);
        free(path_tmp);
    }

    char **selected_files = NULL;
    size_t num_selected_files = 0;
    ret = exec_filechooser(data, true, false, false, path, &selected_files,
                           &num_selected_files);
    if (ret || num_selected_files == 0) {
        remove(path);
        goto cleanup;
    }

    logprint(TRACE, "filechooser: (SaveFile) Number of selected files: %d",
             num_selected_files);
    for (size_t i = 0; i < num_selected_files; i++) {
        logprint(TRACE, "filechooser: %d. %s", i, selected_files[i]);
    }

    sd_bus_message *reply = NULL;
    ret = sd_bus_message_new_method_return(msg, &reply);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_append(reply, "u", PORTAL_RESPONSE_SUCCESS, 1);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_open_container(reply, 'a', "{sv}");
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_open_container(reply, 'e', "sv");
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_append_basic(reply, 's', "uris");
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_open_container(reply, 'v', "as");
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_append_strv(reply, selected_files);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_close_container(reply);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_close_container(reply);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_message_close_container(reply);
    if (ret < 0) {
        goto cleanup;
    }

    ret = sd_bus_send(NULL, reply, NULL);
    if (ret < 0) {
        goto cleanup;
    }

    sd_bus_message_unref(reply);

cleanup:
    for (size_t i = 0; i < num_selected_files; i++) {
        free(selected_files[i]);
    }
    free(selected_files);
    free(path);

    xdpw_request_destroy(req);
    return ret;
}

static const sd_bus_vtable filechooser_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("OpenFile", "osssa{sv}", "ua{sv}", method_open_file,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SaveFile", "osssa{sv}", "ua{sv}", method_save_file,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

int xdpw_filechooser_init(struct xdpw_state *state)
{
    sd_bus_slot *slot = NULL;
    logprint(DEBUG, "dbus: init %s", interface_name);
    int ret;
    ret = sd_bus_add_object_vtable(state->bus, &slot, object_path,
                                   interface_name, filechooser_vtable, state);
    if (ret < 0) {
        logprint(ERROR, "dbus: filechooser init failed: %s", strerror(-ret));
    }

    return ret;
}
