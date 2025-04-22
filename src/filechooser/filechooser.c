#include "config.h"
#include "logger.h"
#include "uri.h"
#include "xdptf.h"
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

static const char instructions[] =
    "xdg-desktop-portal-termfilechooser saving files tutorial\n\n"
    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
    "!!!                 === WARNING! ===                 !!!\n"
    "!!! The contents of *whatever* file you open last in !!!\n"
    "!!! your file manager will be *overwritten*!         !!!\n"
    "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n"
    "Instructions:\n"
    "1) Move this file wherever you want.\n"
    "2) Rename the file if needed.\n"
    "3) Confirm your selection by opening the file.\n\n"
    "Notes:\n"
    "1) This file is provided for your convenience. You\n"
    "   could delete it and choose another file to overwrite.\n"
    "2) If you quit without opening a file, this file\n"
    "   will be removed and the save operation aborted.\n";

static const char object_path[] = "/org/freedesktop/portal/desktop";
static const char interface_name[] = "org.freedesktop.impl.portal.FileChooser";

static int exec_filechooser(void *data, bool writing, bool multiple,
                            bool directory, char *path, char ***selected_files,
                            size_t *num_selected_files)
{
    struct xdptf_state *state = data;
    char *cmd_script = state->config->cmd;
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

    struct environment *env = state->config->env;

    if (access(filename, F_OK) == 0) {
        // clear contents
        FILE *fp = fopen(filename, "w");
        if (fp == NULL) {
            logprint(ERROR, "filechooser: could not open '%s'", filename);
            free(filename);
            return -1;
        }
        if (fclose(fp) != 0) {
            logprint(ERROR, "filechooser: could not close '%s'", filename);
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
        if (ret == -1) {
            logprint(ERROR, "filechooser: could not execute '%s': %s", cmd,
                     strerror(errno));
        } else {
            // FIX: properly get exit code
            logprint(ERROR, "filechooser: could not execute '%s': exit code %d",
                     cmd, -ret / 256);
        }
        free(cmd);
        return -1;
    }
    free(cmd);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        logprint(ERROR, "filechooser: failed to open '%s'", filename);
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

static char *get_last_dir_path(void)
{
    char *home = getenv("HOME");
    char *state_home = getenv("XDG_STATE_HOME");
    char *file_path = "xdg-desktop-portal-termfilechooser";
    char *path = NULL;

    if (state_home) {
        size_t path_size =
            1 + snprintf(NULL, 0, "%s/%s", state_home, file_path);
        path = malloc(path_size);
        snprintf(path, path_size, "%s/%s", state_home, file_path);
    } else if (home) {
        size_t path_size =
            1 + snprintf(NULL, 0, "%s/.local/state/%s", home, file_path);
        path = malloc(path_size);
        snprintf(path, path_size, "%s/.local/state/%s", home, file_path);
    }

    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);
    }

    return path;
}

static char *read_last_dir(void)
{
    char *path = get_last_dir_path();
    char *filename = NULL;

    size_t filename_size = 1 + snprintf(NULL, 0, "%s/last_dir", path);
    filename = malloc(filename_size);
    snprintf(filename, filename_size, "%s/last_dir", path);

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        logprint(ERROR, "filechooser: failed to open '%s'", filename);
        free(filename);
        free(path);
        return NULL;
    }

    char *last_dir = NULL;
    size_t n = 0;
    ssize_t nread = getline(&last_dir, &n, fp);

    if (nread <= 0) {
        if (ferror(fp)) {
            logprint(ERROR, "filechooser: failed to read '%s'", filename);
        } else {
            logprint(ERROR, "filechooser: no data read from '%s'", filename);
        }
        free(last_dir);
        free(filename);
        free(path);
        fclose(fp);
        return NULL;
    }

    if (last_dir[nread - 1] == '\n') {
        last_dir[nread - 1] = '\0';
    }

    fclose(fp);
    free(filename);
    free(path);

    return last_dir;
}

static void write_last_dir(char *last_dir)
{
    if (last_dir == NULL)
        return;

    char *path = get_last_dir_path();
    char *filename = NULL;

    size_t filename_size = 1 + snprintf(NULL, 0, "%s/last_dir", path);
    filename = malloc(filename_size);
    snprintf(filename, filename_size, "%s/last_dir", path);

    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        logprint(ERROR, "filechooser: failed to open '%s': %s", filename,
                 strerror(errno));
        free(filename);
        free(path);
    }

    fputs(last_dir, fp);
    fputc('\n', fp);

    fclose(fp);
    free(filename);
    free(path);
}

static void set_last_dir(char *encoded_selection)
{
    char *encoded = encoded_selection + strlen(PATH_PREFIX);

    char *last_selected = malloc(1 + strlen(encoded));
    uri_decode(encoded, strlen(encoded), last_selected);

    struct stat path_stat = {0};
    if (stat(last_selected, &path_stat) == -1) {
        if (errno == ENOENT) {
            logprint(ERROR, "filechooser: '%s' does not exist.", last_selected);
        } else {
            logprint(ERROR, "filechooser: failed to stat '%s': %s",
                     last_selected, strerror(errno));
        }
    }

    if (S_ISDIR(path_stat.st_mode)) {
        write_last_dir(last_selected);
    } else if (S_ISREG(path_stat.st_mode)) {
        char *parent = strdup(last_selected);
        char *last_slash = strrchr(parent, '/');
        if (last_slash) {
            if (last_slash != parent) {
                *last_slash = '\0';
            } else {
                *++last_slash = '\0';
            }
        } else {
            free(parent);
            parent = NULL;
        }
        write_last_dir(parent);
        free(parent);
    }
    free(last_selected);
}

static void set_current_folder(enum Mode *mode, char **default_dir,
                               char **current_folder)
{
    switch (*mode) {
        case MODE_SUGGESTED_DIR:
            if (*current_folder != NULL)
                *current_folder = strdup(*current_folder);
            break;
        case MODE_DEFAULT_DIR:
            *current_folder = strdup(*default_dir);
            break;
        case MODE_LAST_DIR:
            *current_folder = read_last_dir();
            break;
    }

    if (*current_folder == NULL || access(*current_folder, F_OK)) {
        if (*default_dir != NULL) {
            *current_folder = strdup(*default_dir);
            logprint(
                DEBUG,
                "filechooser: could not set current_folder; fallback to '%s'",
                *current_folder);
        } else {
            logprint(WARN, "filechooser: could not set current_folder");
        }
    }
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
            inner_ret = sd_bus_message_exit_container(msg);
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

    struct xdptf_request *req =
        xdptf_request_create(sd_bus_message_get_bus(msg), handle);
    if (req == NULL) {
        return -ENOMEM;
    }

    struct xdptf_state *state = data;
    char **selected_files = NULL;
    size_t num_selected_files = 0;

    set_current_folder(&state->config->modes->open_mode,
                       &state->config->default_dir, &current_folder);
    ret = exec_filechooser(data, false, multiple, directory, current_folder,
                           &selected_files, &num_selected_files);

    free(current_folder);
    if (ret) {
        goto cleanup;
    }

    logprint(TRACE, "filechooser: (OpenFile) Number of selected files: %d",
             num_selected_files);
    for (size_t i = 0; i < num_selected_files; i++) {
        logprint(TRACE, "filechooser: %d. %s", i, selected_files[i]);
    }

    if (state->config->modes->open_mode == MODE_LAST_DIR) {
        set_last_dir(selected_files[num_selected_files - 1]);
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

    xdptf_request_destroy(req);
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
    char *current_name = NULL;
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
            inner_ret = sd_bus_message_exit_container(msg);
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
            inner_ret = sd_bus_message_exit_container(msg);
            if (inner_ret < 0) {
                return inner_ret;
            }

            current_name = (char *)p;
            logprint(DEBUG,
                     "dbus: option replace current_name with current_file: %s",
                     current_name);
        } else {
            logprint(WARN, "dbus: unknown option %s", key);
            sd_bus_message_skip(msg, "v");
        }

        inner_ret = sd_bus_message_exit_container(msg);
        if (inner_ret < 0) {
            return inner_ret;
        }
    }

    struct xdptf_state *state = data;
    struct xdptf_request *req =
        xdptf_request_create(sd_bus_message_get_bus(msg), handle);
    if (req == NULL) {
        return -ENOMEM;
    }

    set_current_folder(&state->config->modes->save_mode,
                       &state->config->default_dir, &current_folder);

    // existing file
    char *name = strrchr(current_name, '/');
    if (name) {
        current_name = ++name;
    }

    size_t path_size = 2 + strlen(current_folder) + strlen(current_name);
    char *path = malloc(path_size);
    snprintf(path, path_size, "%s/%s", current_folder, current_name);

    free(current_folder);

    // escape ' with '\'' in path
    char *tmp = path;
    size_t escaped_size = 0;
    while (*tmp) {
        escaped_size += (*tmp == '\'') ? 4 : 1;
        tmp++;
    }
    escaped_size += 1;
    char *escaped_path = malloc(escaped_size);
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
    path_size = escaped_size;

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

    FILE *temp_file = fopen(path, "w");
    if (temp_file == NULL) {
        logprint(ERROR, "filechooser: could not write temporary file");
        return -1;
    }
    fputs(instructions, temp_file);
    fclose(temp_file);

    ret = exec_filechooser(data, true, false, false, path, &selected_files,
                           &num_selected_files);

    if (ret || num_selected_files == 0) {
        remove(path);
        goto cleanup;
    }

    logprint(TRACE, "filechooser: (SaveFile) Number of selected files: %d",
             num_selected_files);
    char *decoded = NULL;
    for (size_t i = 0; i < num_selected_files; i++) {
        logprint(TRACE, "filechooser: %d. %s", i, selected_files[i]);
        decoded = malloc(1 + strlen(selected_files[i]));
        uri_decode(selected_files[i], strlen(selected_files[i]), decoded);
        if (decoded && strcmp(decoded + strlen(PATH_PREFIX), path) != 0) {
            remove(path);
        }
        free(decoded);
    }

    if (state->config->modes->save_mode == MODE_LAST_DIR) {
        set_last_dir(selected_files[num_selected_files - 1]);
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

    xdptf_request_destroy(req);
    return ret;
}

static const sd_bus_vtable filechooser_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("OpenFile", "osssa{sv}", "ua{sv}", method_open_file,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SaveFile", "osssa{sv}", "ua{sv}", method_save_file,
                  SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_VTABLE_END};

int xdptf_filechooser_init(struct xdptf_state *state)
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
