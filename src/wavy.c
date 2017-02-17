#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <stdbool.h>
#include <wlc/wlc.h>

#include "callbacks.h"
#include "commands.h"
#include "config.h"
#include "layout.h"
#include "bar.h"
#include "extensions.h"
#include "wallpaper.h"

static const char *wavy_version = "wavy version: 0.0.1\n";
bool debug_enabled = false;
bool wlc_output_enabled = true;
char *cmdline_config_file = NULL;

int main(int argc, char **argv) {
    const char *usage =
        "Usage: wavy [options]\n"
        "\n"
        "  -h,          --help           Print this message and exit\n"
        "  -v,          --version        Print version information and exit\n"
        "  -d           --debug          Enable additional debug output\n"
        "  -W           --no-wlc-output  Disable output from wlc\n"
        "  -c <file>    --config <file>  Select a config file\n"
        "\n";

    const char *optstring = "hvdWc:";

    const struct option long_options[] = {
        {"help",            no_argument,        NULL, 'h'},
        {"version",         no_argument,        NULL, 'v'},
        {"debug",           no_argument,        NULL, 'd'},
        {"no-wlc-output",   no_argument,        NULL, 'W'},
        {"config",          required_argument,  NULL, 'c'},
        {0, 0, 0, 0}
    };

    int c;
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, optstring, long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            fprintf(stdout, "%s", usage);
            exit(EXIT_SUCCESS);
            break;
        case 'v':
            fprintf(stdout, "%s", wavy_version);
            exit(EXIT_SUCCESS);
            break;
        case 'd':
            debug_enabled = true;
            break;
        case 'W':
            wlc_output_enabled = false;
            break;
        case 'c':
            cmdline_config_file = optarg;
            break;
        default:
            fprintf(stderr, "%s", usage);
            exit(EXIT_FAILURE);
        }
    }

    init_commands();
    init_bar_config();
    init_config();
    set_wlc_callbacks();
    init_layout();
    init_bar_threads();

    if (!wlc_init()) {
        exit(EXIT_FAILURE);
    }

    // wayland protocol extensions
    register_extensions();

    wlc_run();

    stop_bar_threads();
    free_commands();
    free_config();
    free_all_outputs();
    free_workspaces();

    exit(EXIT_SUCCESS);
}
