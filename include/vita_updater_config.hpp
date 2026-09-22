/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

// Override these definitions with compiler -D flags or a project-specific
// wrapper header. Title IDs must contain exactly nine uppercase alphanumerics.
#ifndef VITA_UPDATER_APP_NAME
#define VITA_UPDATER_APP_NAME "Example Vita App"
#endif

#ifndef VITA_UPDATER_MAIN_TITLE_ID
#define VITA_UPDATER_MAIN_TITLE_ID "EXAMPLE01"
#endif

#ifndef VITA_UPDATER_HELPER_TITLE_ID
#define VITA_UPDATER_HELPER_TITLE_ID "EXUPD0001"
#endif

#ifndef VITA_UPDATER_GITHUB_OWNER
#define VITA_UPDATER_GITHUB_OWNER "example-owner"
#endif

#ifndef VITA_UPDATER_GITHUB_REPO
#define VITA_UPDATER_GITHUB_REPO "example-repository"
#endif

#ifndef VITA_UPDATER_CURRENT_VERSION
#define VITA_UPDATER_CURRENT_VERSION "0.0.0"
#endif

#ifndef VITA_UPDATER_PUBLIC_KEY_HEX
#define VITA_UPDATER_PUBLIC_KEY_HEX ""
#endif

#ifndef VITA_UPDATER_STAGING_DIRECTORY
#define VITA_UPDATER_STAGING_DIRECTORY "ux0:/data/example-app/update"
#endif

#ifndef VITA_UPDATER_MANIFEST_ASSET
#define VITA_UPDATER_MANIFEST_ASSET "vita-update-manifest.txt"
#endif

#ifndef VITA_UPDATER_USER_AGENT
#define VITA_UPDATER_USER_AGENT "vita-self-updater/1"
#endif

#ifndef VITA_UPDATER_CA_BUNDLE
#define VITA_UPDATER_CA_BUNDLE "vs0:data/external/cert/CA_LIST.cer"
#endif

#ifndef VITA_UPDATER_HEALTH_FRAMES
#define VITA_UPDATER_HEALTH_FRAMES 300
#endif

#ifndef VITA_UPDATER_PROTECTED_PATH_1
#define VITA_UPDATER_PROTECTED_PATH_1 "ux0:/data/example-app/save/"
#endif

#ifndef VITA_UPDATER_PROTECTED_PATH_2
#define VITA_UPDATER_PROTECTED_PATH_2 "ux0:/data/example-app/cache/"
#endif
