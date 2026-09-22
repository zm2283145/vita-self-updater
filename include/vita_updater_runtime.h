/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VITA_UPDATER_RUNTIME_H
#define VITA_UPDATER_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

int vita_updater_init(void);
void vita_updater_poll(void);
void vita_updater_render_dialog(void);
void vita_updater_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
