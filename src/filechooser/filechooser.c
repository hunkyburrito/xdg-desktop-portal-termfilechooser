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
#include <sys/wait.h>
#include <unistd.h>

#define PATH_PREFIX "file://"
#define PATH_PORTAL "/tmp/termfilechooser.portal"

static const char object_path[] = "/org/freedesktop/portal/desktop";
static const char interface_name[] = "org.freedesktop.impl.portal.FileChooser";

static int exec_filechooser(void *data, bool writing, bool multiple,
                            bool directory, char *path, char ***selected_files,
                            size_t *num_selected_files) {
  struct xdpw_state *state = data;
  char *cmd_script = state->config->filechooser_conf.cmd;
  if (!cmd_script) {
    logprint(ERROR, "cmd not specified");
    return -1;
  }

  if (path == NULL) {
    path = "";
  }

  size_t str_size = snprintf(NULL, 0, "%s %d %d %d \'%s\' \'%s\'", cmd_script,
                             multiple, directory, writing, path, PATH_PORTAL) + 1;
  char *cmd = malloc(str_size);
  snprintf(cmd, str_size, "%s %d %d %d \'%s\' \'%s\'", cmd_script, multiple,
           directory, writing, path, PATH_PORTAL);

  struct environment *env = state->config->filechooser_conf.env;

  // Check if the portal file exists and have read write permission
  if (access(PATH_PORTAL, F_OK) == 0) {
    if (access(PATH_PORTAL, R_OK | W_OK) != 0) {
      logprint(ERROR,
               "failed to start portal, make sure you have permission to read "
               "and write %s",
               PATH_PORTAL);
      return -1;
    }
  }
  remove(PATH_PORTAL);
  for (int i = 0, ret = 0; i < env->num_vars; i++) {
    logprint(TRACE, "setting env: %s=%s", (env->vars + i)->name, (env->vars + i)->value);
    ret = setenv((env->vars + i)->name, (env->vars + i)->value, 1);
    if (ret) {
      logprint(WARN, "could not set env %s: %s", (env->vars + i)->name, strerror(errno));
    }
  }
  logprint(TRACE, "executing command: %s", cmd);
  int ret = system(cmd);
  if (ret) {
    logprint(ERROR, "could not execute %s: %s", cmd, strerror(errno));
    free(cmd);
    return -1;
  }
  free(cmd);

  FILE *fp = fopen(PATH_PORTAL, "r");
  if (fp == NULL) {
    logprint(ERROR, "failed to open " PATH_PORTAL);
    return -1;
  }

  size_t nchars = 0, num_lines = 0;
  int cr;
  do {
    cr = getc(fp);
    nchars++;
    if (cr == '\n') {
      if (nchars > 1)
        num_lines++;
      nchars = 0;
    }
    if (ferror(fp)) {
      return 1;
    }
  } while (cr != EOF);
  rewind(fp);

  if (num_lines == 0 && nchars > 1) {
    num_lines = 1;
  }

  if (num_lines == 0) {
    fclose(fp);
    return -1;
  }

  *num_selected_files = num_lines;
  *selected_files = malloc((num_lines + 1) * sizeof(char *));

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
      return 1;
    }
    encoded = malloc(nread * 3 + 1); // if all chars are encoded, size = orig_size * 3 + 1
    size_t nenc = uri_encode(line, nread, encoded);
    size_t str_size = nenc + strlen(PATH_PREFIX) + 1;
    if (nenc >= 3 && ! strcmp(encoded + nenc - 3, "%0A")) { // last char equal '\n'
      str_size -= 3;
    }
    (*selected_files)[i] = malloc(str_size);
    snprintf((*selected_files)[i], str_size, "%s%s", PATH_PREFIX, encoded);
    free(line);
    free(encoded);
  }
  (*selected_files)[num_lines] = NULL;

  fclose(fp);
  return 0;
}

static int method_open_file(sd_bus_message *msg, void *data,
                            sd_bus_error *ret_error) {
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

  // TODO: cleanup this
  struct xdpw_request *req =
      xdpw_request_create(sd_bus_message_get_bus(msg), handle);
  if (req == NULL) {
    return -ENOMEM;
  }

  char **selected_files = NULL;
  size_t num_selected_files = 0;
  ret = exec_filechooser(data, false, multiple, directory, NULL,
                         &selected_files, &num_selected_files);
  if (ret) {
    goto cleanup;
  }

  logprint(TRACE, "(OpenFile) Number of selected files: %d",
           num_selected_files);
  for (size_t i = 0; i < num_selected_files; i++) {
    logprint(TRACE, "%d. %s", i, selected_files[i]);
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
                            sd_bus_error *ret_error) {
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
    } else {
      logprint(WARN, "dbus: unknown option %s", key);
      sd_bus_message_skip(msg, "v");
    }

    inner_ret = sd_bus_message_exit_container(msg);
    if (inner_ret < 0) {
      return inner_ret;
    }
  }

  // TODO: cleanup this
  struct xdpw_request *req =
      xdpw_request_create(sd_bus_message_get_bus(msg), handle);
  if (req == NULL) {
    return -ENOMEM;
  }

  if (current_folder == NULL) {
    struct xdpw_state *state = data;
    char *default_dir = state->config->filechooser_conf.default_dir;
    if (!default_dir) {
      logprint(ERROR, "default_dir not specified");
      return -1;
    }
    current_folder = default_dir;
  }

  size_t path_size = snprintf(NULL, 0, "%s/%s", current_folder, current_name) + 1;
  char *path = malloc(path_size);
  snprintf(path, path_size, "%s/%s", current_folder, current_name);

  // escape ' with '\'' in path
  char *tmp = path;
  size_t escape_size = 0;
  while (*tmp) {
    escape_size += (*tmp == '\'') ? 4 : 1;
    tmp++;
  }

  char* escaped_path = malloc(escape_size + 1);
  char* ptr = escaped_path;
  tmp = path;
  while (*tmp) {
    if (*tmp == '\'') {
      *ptr++ = '\'';
      *ptr++ = '\\';
      *ptr++ = '\'';
      *ptr++ = '\'';
    }
    else {
      *ptr++ = *tmp;
    }
    tmp++;
  }
  *ptr = '\0';

  free(path);
  path = escaped_path;

  bool file_already_exists = true;
  while (file_already_exists) {
    if (access(path, F_OK) == 0) {
      char *path_tmp = malloc(path_size);
      snprintf(path_tmp, path_size, "%s", path);
      path_size += 1;
      path = realloc(path, path_size);
      snprintf(path, path_size, "%s_", path_tmp);
      free(path_tmp);
    } else {
      file_already_exists = false;
    }
  }

  char **selected_files = NULL;
  size_t num_selected_files = 0;
  ret = exec_filechooser(data, true, false, false, path, &selected_files,
                         &num_selected_files);
  if (ret || num_selected_files == 0) {
    remove(path);
    ret = -1;
    goto cleanup;
  }

  logprint(TRACE, "(SaveFile) Number of selected files: %d",
           num_selected_files);
  for (size_t i = 0; i < num_selected_files; i++) {
    logprint(TRACE, "%d. %s", i, selected_files[i]);
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

int xdpw_filechooser_init(struct xdpw_state *state) {
  // TODO: cleanup
  sd_bus_slot *slot = NULL;
  logprint(DEBUG, "dbus: init %s", interface_name);
  return sd_bus_add_object_vtable(state->bus, &slot, object_path,
                                  interface_name, filechooser_vtable, state);
}
