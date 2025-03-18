#include "config.h"
#include "logger.h"
#include <ini.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>

static char *shell_expand(const char *input)
{
    wordexp_t p;
    if (wordexp(input, &p, 0) != 0) {
        // return original if expansion fails
        return strdup(input);
    }

    size_t expanded_size = 0;
    for (size_t i = 0; i < p.we_wordc; i++) {
        expanded_size += 1 + strlen(p.we_wordv[i]);
    }
    char *expanded = malloc(expanded_size);

    // null beginning
    expanded[0] = '\0';
    for (size_t i = 0; i < p.we_wordc; i++) {
        if (i > 0)
            strcat(expanded, " ");
        strcat(expanded, p.we_wordv[i]);
    }

    wordfree(&p);
    return expanded;
}

void print_config(enum LOGLEVEL loglevel, struct xdpw_config *config)
{
    logprint(loglevel, "config: cmd:  %s", config->filechooser_conf.cmd);
    logprint(loglevel, "config: default_dir:  %s",
             config->filechooser_conf.default_dir);
    for (int i = 0; i < config->filechooser_conf.env->num_vars; i++) {
        logprint(loglevel, "config: env:  %s=%s",
                 config->filechooser_conf.env->vars[i].name,
                 config->filechooser_conf.env->vars[i].value);
    }
}

// NOTE: calling free_config won't prepare the config to be read again from
// config file with init_config since to pointers and other values won't be
// reset to NULL, or 0
void free_config(struct xdpw_config *config)
{
    logprint(DEBUG, "config: freeing config");
    free(config->filechooser_conf.cmd);
    free(config->filechooser_conf.default_dir);
    for (int i = 0; i < config->filechooser_conf.env->num_vars; i++) {
        free(config->filechooser_conf.env->vars[i].name);
        free(config->filechooser_conf.env->vars[i].value);
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
    *dest = shell_expand(value);
}

static void parse_env(struct environment *env, const char *envstr)
{
    if (envstr == NULL || *envstr == '\0') {
        logprint(TRACE, "config: skipping env in config file");
        return;
    }

    char *sep = strchr(envstr, '=');
    if (sep == NULL || sep == envstr) {
        logprint(TRACE, "config: skipping corrupt env in config file");
        return;
    }

    char *name = strndup(envstr, sep - envstr);
    char *value = strdup(sep + 1);
    char *expanded = shell_expand(value);
    free(value);

    // dynamically allocate more vars
    if (env->num_vars == env->capacity) {
        env->capacity += 5;
        env->vars = realloc(env->vars, sizeof(struct env_var) * env->capacity);
    }

    // append to env
    env->vars[env->num_vars].name = name;
    env->vars[env->num_vars].value = expanded;
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
        parse_env(filechooser_conf->env, value);
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

    logprint(TRACE, "config: skipping invalid section in config file");
    return 0;
}

static bool file_exists(const char *path)
{
    return path && access(path, R_OK) != -1;
}

static void set_default_config(struct xdpw_config *config)
{
    const char *default_cmd = "yazi-wrapper.sh";
    const char *default_dir = "/tmp";

    if (access(default_cmd, F_OK) && access(default_cmd, R_OK | X_OK)) {
        config->filechooser_conf.cmd = strdup(default_cmd);
    } else {
        logprint(ERROR, "config: default cmd '%s' is not executable",
                 default_cmd);
    }

    config->filechooser_conf.default_dir = strdup(default_dir);

    struct environment *env = malloc(sizeof(struct environment));
    env->num_vars = 0;
    env->capacity = 10;
    env->vars = calloc(env->capacity, sizeof(struct env_var));
    config->filechooser_conf.env = env;
}

static char *build_config_path(const char *base, const char *filename)
{
    if (!base || !base[0] || !filename || !filename[0]) {
        return NULL;
    }

    const char *config_folder = "xdg-desktop-portal-termfilechooser";
    size_t size = 3 + strlen(base) + strlen(config_folder) + strlen(filename);
    char *path = malloc(size);
    snprintf(path, size, "%s/%s/%s", base, config_folder, filename);
    return path;
}

static char *get_config_path(void)
{
    const char *home = getenv("HOME");
    char *config_home_fallback = NULL;
    if (home && home[0]) {
        size_t size_fallback = 1 + strlen(home) + strlen("/.config");
        config_home_fallback = malloc(size_fallback);
        snprintf(config_home_fallback, size_fallback, "%s/.config", home);
    }

    const char *config_home = getenv("XDG_CONFIG_HOME");
    if (!config_home || !config_home[0]) {
        config_home = config_home_fallback;
    }

    const char *prefixes[] = {config_home, SYSCONFDIR "/xdg"};
    const char *xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
    const char *config_fallback = "config";

    for (size_t i = 0; i < 2; i++) {
        if (xdg_current_desktop) {
            char *config_list = strdup(xdg_current_desktop);
            char *config = strtok(config_list, ":");
            while (config) {
                char *path = build_config_path(prefixes[i], config);
                logprint(TRACE, "config: trying config file %s", path);
                if (path && file_exists(path)) {
                    free(config_home_fallback);
                    free(config_list);
                    return path;
                }
                free(path);
                config = strtok(NULL, ":");
            }
            free(config_list);
        }

        char *path = build_config_path(prefixes[i], config_fallback);
        logprint(TRACE, "config: trying config file %s", path);
        if (path && file_exists(path)) {
            free(config_home_fallback);
            return path;
        }
        free(path);
    }

    free(config_home_fallback);
    return NULL;
}

static void init_wrapper_path(struct environment *env,
                              const char *const configfile)
{
    char *config_path = NULL;
    // get config directory
    if (configfile) {
        config_path = strdup(configfile);
        char *last_slash = strrchr(config_path, '/');
        if (last_slash) {
            *last_slash = '\0';
        }
    } else {
        config_path = "";
    }

    const char *const wrapper_paths =
        "/usr/local/share/xdg-desktop-portal-termfilechooser:/usr/share/"
        "xdg-desktop-portal-termfilechooser";

    const char *sys_path = getenv("PATH");
    if (!sys_path)
        sys_path = "";

    size_t path_size =
        8 + strlen(config_path) + strlen(wrapper_paths) + strlen(sys_path);
    char *path_env = malloc(path_size);
    snprintf(path_env, path_size, "PATH=%s:%s:%s", config_path, wrapper_paths,
             sys_path);

    parse_env(env, path_env);

    free(config_path);
    free(path_env);
}

void init_config(char **const configfile, struct xdpw_config *config)
{
    if (!*configfile)
        *configfile = get_config_path();

    set_default_config(config);
    init_wrapper_path(config->filechooser_conf.env, *configfile);

    if (!*configfile) {
        logprint(ERROR, "config: no config file found, using the default");
        return;
    }

    if (ini_parse(*configfile, handle_ini_config, config) < 0) {
        logprint(ERROR, "config: unable to load config file '%s'", *configfile);
    }
}
