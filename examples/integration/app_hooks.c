/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vita_updater_runtime.h"

int example_app_startup(void) {
    return vita_updater_init();
}

void example_app_frame(void) {
    vita_updater_poll();

    /* Render the application scene here. */

    vita_updater_render_dialog();

    /* Swap the application's display buffers here. */
}

void example_app_shutdown(void) {
    vita_updater_shutdown();
}
