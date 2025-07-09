#ifndef CONFIG_H
#define CONFIG_H

#include "logger.h"

struct env_var {
    char *name;
    char *value;
};

struct environment {
    int num_vars;
    int capacity;
    struct env_var *vars;
};

enum Mode { MODE_SUGGESTED_DIR, MODE_DEFAULT_DIR, MODE_LAST_DIR };

struct modes {
    enum Mode open_mode;
    enum Mode save_mode;
};

struct config_filechooser {
    char *cmd;
    char *default_dir;
    char create_help_file;
    struct modes *modes;
    struct environment *env;
};

void print_config(enum LOGLEVEL loglevel, struct config_filechooser *config);
void free_config(struct config_filechooser *config);
void init_config(char **const configfile, struct config_filechooser *config);

#endif
