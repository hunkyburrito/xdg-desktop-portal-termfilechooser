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

struct config_filechooser {
    char *cmd;
    char *default_dir;
    struct environment *env;
};

struct xdpw_config {
    struct config_filechooser filechooser_conf;
};

void print_config(enum LOGLEVEL loglevel, struct xdpw_config *config);
void finish_config(struct xdpw_config *config);
void init_config(char ** const configfile, struct xdpw_config *config);

#endif
