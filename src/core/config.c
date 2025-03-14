#include "config.h"
#include "logger.h"
#include <ini.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FILECHOOSER_DEFAULT_CMD                                                \
    "/usr/local/share/xdg-desktop-portal-termfilechooser/yazi-wrapper.sh"
#define FILECHOOSER_DEFAULT_DIR "/tmp"

void print_config(enum LOGLEVEL loglevel, struct xdpw_config *config)
{
    logprint(loglevel, "config: cmd:  %s", config->filechooser_conf.cmd);
    logprint(loglevel, "config: default_dir:  %s",
             config->filechooser_conf.default_dir);
    for (int i = 0; i < config->filechooser_conf.env->num_vars; i++) {
        logprint(loglevel, "config: env:  %s=%s",
                 (config->filechooser_conf.env->vars + i)->name,
                 (config->filechooser_conf.env->vars + i)->value);
    }
}

// NOTE: calling finish_config won't prepare the config to be read again from
// config file with init_config since to pointers and other values won't be
// reset to NULL, or 0
void finish_config(struct xdpw_config *config)
{
    logprint(DEBUG, "config: destroying config");
    free(config->filechooser_conf.cmd);
    free(config->filechooser_conf.default_dir);
    for (int i = 0; i < config->filechooser_conf.env->num_vars; i++) {
        free((config->filechooser_conf.env->vars + i)->name);
        free((config->filechooser_conf.env->vars + i)->value);
    }
    free(config->filechooser_conf.env->vars);
    free(config->filechooser_conf.env);
}

static void parse_string(char **dest, const char *value)
{
    if (value == NULL || *value == '\0') {
        logprint(TRACE, "config: skipping empty value in config file");
        return;
    }
    free(*dest);
    *dest = strdup(value);
}

static void parse_env(struct environment *env, char *envstr)
{
    if (envstr == NULL || *envstr == '\0') {
        logprint(TRACE, "config: skipping env in config file");
        return;
    }

    char *sep = strchr(envstr, '=');
    char *name = NULL;
    char *value = NULL;

    if (sep == NULL || sep == envstr) {
        logprint(TRACE, "config: skipping corrupt env in config file");
        return;
    } else {
        name = strtok(envstr, "=");
        value = strtok(NULL, "\n");
        if (value == NULL)
            value = "";
    }

    // dynamically allocate more vars
    if (env->num_vars == env->capacity) {
        env->capacity += 5;
        env->vars = realloc(env->vars, sizeof(struct env_var) * env->capacity);
    }

    // create env_var and add to env
    struct env_var *var = (env->vars + env->num_vars);
    var->name = strdup(name);
    var->value = strdup(value);
    env->num_vars++;
}

static int handle_ini_filechooser(struct config_filechooser *filechooser_conf,
                                  const char *key, const char *value)
{
    if (strcmp(key, "cmd") == 0) {
        parse_string(&filechooser_conf->cmd, value);
    } else if (strcmp(key, "default_dir") == 0) {
        parse_string(&filechooser_conf->default_dir, value);
    } else if (strcmp(key, "env") == 0) {
        char *envstr = strdup(value);
        parse_env(filechooser_conf->env, envstr);
        free(envstr);
    } else {
        logprint(TRACE, "config: skipping invalid key in config file");
        return 0;
    }
    return 1;
}

static int handle_ini_config(void *data, const char *section, const char *key,
                             const char *value)
{
    struct xdpw_config *config = (struct xdpw_config *)data;
    logprint(TRACE, "config: parsing section %s, key %s, value %s", section,
             key, value);

    if (strcmp(section, "filechooser") == 0) {
        return handle_ini_filechooser(&config->filechooser_conf, key, value);
    }

    logprint(TRACE, "config: skipping invalid key in config file");
    return 0;
}

static void default_config(struct xdpw_config *config)
{
    size_t size = snprintf(NULL, 0, "%s", FILECHOOSER_DEFAULT_CMD) + 1;
    config->filechooser_conf.cmd = malloc(size);
    snprintf(config->filechooser_conf.cmd, size, "%s", FILECHOOSER_DEFAULT_CMD);
    size = snprintf(NULL, 0, "%s", FILECHOOSER_DEFAULT_DIR) + 1;
    config->filechooser_conf.default_dir = malloc(size);
    snprintf(config->filechooser_conf.default_dir, size, "%s",
             FILECHOOSER_DEFAULT_DIR);

    struct environment *env = malloc(sizeof(struct environment));
    env->num_vars = 0;
    env->capacity = 10;
    env->vars = malloc(sizeof(struct env_var) * env->capacity);
    config->filechooser_conf.env = env;
}

static bool file_exists(const char *path)
{
    return path && access(path, R_OK) != -1;
}

static char *config_path(const char *prefix, const char *filename)
{
    if (!prefix || !prefix[0] || !filename || !filename[0]) {
        return NULL;
    }

    char *config_folder = "xdg-desktop-portal-termfilechooser";

    size_t size = 3 + strlen(prefix) + strlen(config_folder) + strlen(filename);
    char *path = calloc(size, sizeof(char));
    snprintf(path, size, "%s/%s/%s", prefix, config_folder, filename);
    return path;
}

static char *get_config_path(void)
{
    const char *home = getenv("HOME");
    char *config_home_fallback = NULL;
    if (home != NULL && home[0] != '\0') {
        size_t size_fallback = 1 + strlen(home) + strlen("/.config");
        config_home_fallback = calloc(size_fallback, sizeof(char));
        snprintf(config_home_fallback, size_fallback, "%s/.config", home);
    }

    const char *config_home = getenv("XDG_CONFIG_HOME");
    if (config_home == NULL || config_home[0] == '\0') {
        config_home = config_home_fallback;
    }

    const char *prefix[2];
    prefix[0] = config_home;
    prefix[1] = SYSCONFDIR "/xdg";

    const char *xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
    const char *config_fallback = "config";

    char *config_list = NULL;
    for (size_t i = 0; i < 2; i++) {
        if (xdg_current_desktop) {
            config_list = strdup(xdg_current_desktop);
            char *config = strtok(config_list, ":");
            while (config) {
                char *path = config_path(prefix[i], config);
                if (!path) {
                    config = strtok(NULL, ":");
                    continue;
                }
                logprint(TRACE, "config: trying config file %s", path);
                if (file_exists(path)) {
                    free(config_list);
                    free(config_home_fallback);
                    return path;
                }
                free(path);
                config = strtok(NULL, ":");
            }
            free(config_list);
        }
        char *path = config_path(prefix[i], config_fallback);
        if (!path) {
            continue;
        }
        logprint(TRACE, "config: trying config file %s", path);
        if (file_exists(path)) {
            free(config_home_fallback);
            return path;
        }
        free(path);
    }

    free(config_home_fallback);
    return NULL;
}

void init_wrapper_path(struct environment *env, char *const configfile)
{
    char *sys_path = getenv("PATH");
    char *config = configfile;
    char *substr = strrchr(config, '/');
    size_t cutoff = (size_t)(substr - config);
    char *config_path = strndup(config, cutoff);
    const char *const wrapper_paths =
        "/usr/local/share/xdg-desktop-portal-termfilechooser:/usr/share/"
        "xdg-desktop-portal-termfilechooser";

    size_t path_size = 0;
    char *path_env = NULL;

    path_size = 1 + snprintf(NULL, 0, "PATH=%s:%s:%s", config_path,
                             wrapper_paths, sys_path);
    path_env = malloc(path_size);
    snprintf(path_env, path_size, "PATH=%s:%s:%s", config_path, wrapper_paths,
             sys_path);

    parse_env(env, path_env);

    free(path_env);
    free(config_path);
}

void init_config(char **const configfile, struct xdpw_config *config)
{
    if (*configfile == NULL) {
        *configfile = get_config_path();
    }

    default_config(config);
    if (*configfile == NULL) {
        logprint(ERROR, "config: no config file found");
        return;
    }
    if (ini_parse(*configfile, handle_ini_config, config) < 0) {
        logprint(ERROR, "config: unable to load config file %s", *configfile);
    }
    init_wrapper_path(config->filechooser_conf.env, *configfile);
}
