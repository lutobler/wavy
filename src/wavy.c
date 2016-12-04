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

static const char *wavy_version = "wavy version: 0.0.1\n";
bool debug_enabled = false;
bool wlc_output_enabled = true;

int main(int argc, char **argv) {
    const char *usage =
        "Usage: wavy [options]\n"
        "\n"
        "  -h, --help           Print this message and exit.\n"
        "  -v, --version        Print version information and exit.\n"
        "  -d  --debug          Enable additional debug output.\n"
        "  -W  --no-wlc-output  Disable output from wlc.\n"
        "\n";

    const char *optstring = "hvdW";

    const struct option long_options[] = {
        {"help",            no_argument,        NULL, 'h'},
        {"version",         no_argument,        NULL, 'v'},
        {"debug",           no_argument,        NULL, 'd'},
        {"no-wlc-output",   no_argument,        NULL, 'W'},
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

    wlc_run();

    free_commands();
    free_config();
    free_all_outputs();
    free_workspaces();
    stop_bar_threads();

    exit(EXIT_SUCCESS);
}
