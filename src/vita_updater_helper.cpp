/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vita_updater_core.hpp"
#include "vita_updater_platform.hpp"

#include <vita2d.h>

#include <psp2/appmgr.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

extern "C" {
unsigned int _newlib_heap_size_user = 32u * 1024u * 1024u;
int sceUserMainThreadStackSize = 1024 * 1024;
}

namespace {

using vita::updater::Journal;
using vita::updater::JournalState;
using vita::updater::SignedManifest;

vita2d_pgf* g_font = nullptr;
bool g_graphics_initialized = false;
std::string g_status;
std::string g_detail;

bool ensure_graphics() {
    if (g_graphics_initialized) {
        return g_font != nullptr;
    }
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    if (vita2d_init() < 0) {
        return false;
    }
    g_graphics_initialized = true;
    g_font = vita2d_load_default_pgf();
    return g_font != nullptr;
}

void draw_screen(const char* instruction = nullptr) {
    if (!ensure_graphics()) {
        return;
    }
    vita2d_start_drawing();
    vita2d_clear_screen();
    vita2d_pgf_draw_text(g_font, 42.0f, 72.0f,
        RGBA8(255, 255, 255, 255), 1.25f,
        VITA_UPDATER_APP_NAME " temporary updater");
    vita2d_pgf_draw_text(g_font, 42.0f, 126.0f,
        RGBA8(116, 205, 255, 255), 1.0f, g_status.c_str());

    std::string remaining = g_detail;
    float y = 174.0f;
    while (!remaining.empty() && y < 430.0f) {
        std::size_t split = std::min<std::size_t>(remaining.size(), 86);
        if (split < remaining.size()) {
            const std::size_t space = remaining.rfind(' ', split);
            if (space != std::string::npos && space > 20) {
                split = space;
            }
        }
        const std::string line = remaining.substr(0, split);
        vita2d_pgf_draw_text(g_font, 42.0f, y,
            RGBA8(230, 230, 230, 255), 0.85f, line.c_str());
        remaining.erase(0, split);
        while (!remaining.empty() && remaining.front() == ' ') {
            remaining.erase(remaining.begin());
        }
        y += 32.0f;
    }
    if (instruction != nullptr) {
        vita2d_pgf_draw_text(g_font, 42.0f, 500.0f,
            RGBA8(255, 226, 110, 255), 0.9f, instruction);
    }
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

void set_status(std::string status, std::string detail = {}) {
    g_status = std::move(status);
    g_detail = std::move(detail);
}

unsigned wait_for_buttons(unsigned buttons) {
    SceCtrlData previous{};
    for (;;) {
        SceCtrlData current{};
        sceCtrlPeekBufferPositive(0, &current, 1);
        const unsigned pressed = current.buttons & ~previous.buttons;
        if ((pressed & buttons) != 0) {
            return pressed & buttons;
        }
        previous = current;
        draw_screen(g_status == "Recovery required" ?
            "X: roll back to the previous version" :
            "X: continue");
        sceKernelDelayThread(16 * 1000);
    }
}

bool verify_staged_update(
    SignedManifest& manifest, std::string& package_path, std::string& error) {
    std::string manifest_text;
    if (!vita::updater::read_file_limited(
            vita::updater::kManifestPath,
            vita::updater::kMaxManifestBytes, manifest_text, error) ||
        !vita::updater::parse_signed_manifest(
            manifest_text, manifest, error))
    {
        return false;
    }
    std::array<std::uint8_t, 32> public_key{};
    if (!vita::updater::parse_build_public_key(public_key, error) ||
        !vita::updater::verify_manifest_signature(
            manifest, public_key,
            vita::updater::verify_ed25519_sodium, error))
    {
        return false;
    }
    package_path =
        std::string(vita::updater::kStagingRoot) + manifest.asset_name;
    std::array<std::uint8_t, 32> digest{};
    std::uint64_t size = 0;
    if (!vita::updater::sha256_file(package_path.c_str(),
            vita::updater::kMaxPackageBytes, digest, size, error))
    {
        return false;
    }
    if (size != manifest.asset_size || digest != manifest.sha256) {
        error =
            "The staged package changed after download and was rejected.";
        return false;
    }
    return true;
}

int rollback(const Journal& source_journal, std::string& error) {
    set_status("Restoring the previous version",
        "The backup is being promoted through the Vita package manager.");
    if (!vita::updater::path_exists(
            vita::updater::kBackupPackagePath) ||
        !vita::updater::package_title_matches(
            vita::updater::kBackupPackagePath,
            vita::updater::kMainTitleId, error))
    {
        return -1;
    }
    const int result =
        vita::updater::promoter_install(
            vita::updater::kBackupPackagePath);
    Journal journal = source_journal;
    journal.error_code = result < 0 ? result : source_journal.error_code;
    journal.state = result < 0 ?
        JournalState::RecoveryFailed : JournalState::RolledBack;
    std::string journal_error;
    if (!vita::updater::write_journal(journal, journal_error)) {
        error = journal_error;
        return result < 0 ? result : -1;
    }
    if (result < 0) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer),
            "Rollback promotion failed (0x%08X).",
            static_cast<unsigned>(result));
        error = buffer;
    }
    return result;
}

bool perform_update(Journal& journal, std::string& error) {
    SignedManifest manifest;
    std::string package_path;
    set_status("Verifying the staged update",
        "The signed manifest and package SHA-256 are checked again.");
    if (!verify_staged_update(manifest, package_path, error)) {
        return false;
    }
    if (journal.version != manifest.version_text) {
        error = "The recovery journal version does not match the manifest.";
        return false;
    }

    set_status("Extracting the verified package",
        "Only regular files with safe relative paths are accepted.");
    if (!vita::updater::extract_update_vpk(
            package_path.c_str(), vita::updater::kNewPackagePath, error) ||
        !vita::updater::package_title_matches(
            vita::updater::kNewPackagePath,
            vita::updater::kMainTitleId, error) ||
        !vita::updater::package_supports_updater(
            vita::updater::kNewPackagePath, error))
    {
        return false;
    }

    set_status("Backing up the current installation",
        "Save data, the game image, and the shader cache are outside app0 "
        "and are never copied or replaced.");
    if (!vita::updater::copy_tree(
            vita::updater::kMainPackagePath,
            vita::updater::kBackupPackagePath, error) ||
        !vita::updater::package_title_matches(
            vita::updater::kBackupPackagePath,
            vita::updater::kMainTitleId, error))
    {
        return false;
    }

    journal.state = JournalState::Promoting;
    journal.error_code = 0;
    if (!vita::updater::write_journal(journal, error)) {
        return false;
    }
    set_status("Installing the signed update",
        VITA_UPDATER_APP_NAME
        " is stopped. The Vita package promoter is replacing the "
        "registered application.");
    const int result =
        vita::updater::promoter_install(
            vita::updater::kNewPackagePath);
    if (result < 0) {
        journal.error_code = result;
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer),
            "Installing the new package failed (0x%08X).",
            static_cast<unsigned>(result));
        error = buffer;
        return false;
    }
    journal.state = JournalState::AwaitingHealth;
    if (!vita::updater::write_journal(journal, error)) {
        return false;
    }

    set_status("Update installed",
        "Closing the helper before launching " VITA_UPDATER_APP_NAME
        ". The backup remains until the game confirms a healthy startup.");
    return true;
}

}  // namespace

int main() {
    bool launch_after_shutdown = false;
    sceAppMgrDestroyOtherApp();
    sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);

    Journal journal;
    std::string error;
    if (!vita::updater::read_journal(journal, error)) {
        g_status = "Recovery data is invalid";
        g_detail = error +
            " The helper will remain installed. Reinstall "
            VITA_UPDATER_APP_NAME " manually.";
        wait_for_buttons(SCE_CTRL_CROSS);
        goto shutdown;
    }

    if (journal.state == JournalState::RolledBack) {
        set_status("Previous version restored",
            "Press X to close this helper, then launch "
            VITA_UPDATER_APP_NAME " from LiveArea.");
        wait_for_buttons(SCE_CTRL_CROSS);
        goto shutdown;
    }
    if (journal.state == JournalState::CleanupPending) {
        set_status("Update already confirmed",
            "Close this helper and launch " VITA_UPDATER_APP_NAME
            " directly to finish cleanup.");
        wait_for_buttons(SCE_CTRL_CROSS);
        goto shutdown;
    }
    if (journal.state == JournalState::Promoting ||
        journal.state == JournalState::AwaitingHealth ||
        journal.state == JournalState::RecoveryFailed)
    {
        g_status = "Recovery required";
        g_detail =
            "The previous update did not confirm a healthy launch. The "
            "preserved installation can be restored without touching saves.";
        wait_for_buttons(SCE_CTRL_CROSS);
        if (rollback(journal, error) >= 0) {
            set_status("Rollback completed",
                "The previous " VITA_UPDATER_APP_NAME
                " installation was restored. Press X to close this helper, "
                "then launch it from LiveArea.");
            wait_for_buttons(SCE_CTRL_CROSS);
            goto shutdown;
        }
        goto recovery_failed;
    }

    if (journal.state != JournalState::HelperInstalled &&
        journal.state != JournalState::Staged)
    {
        error = "The updater journal is not in a runnable state.";
        goto launch_failed;
    }
    if (perform_update(journal, error)) {
        launch_after_shutdown = true;
        goto shutdown;
    }
    if (journal.state == JournalState::Promoting ||
        journal.state == JournalState::AwaitingHealth)
    {
        if (rollback(journal, error) >= 0) {
            set_status("Update failed; rollback completed",
                error + " The previous installation was restored. Close this "
                "helper and launch " VITA_UPDATER_APP_NAME " from LiveArea.");
            wait_for_buttons(SCE_CTRL_CROSS);
            goto shutdown;
        }
        goto recovery_failed;
    }

launch_failed:
    g_status = "Update could not continue";
    g_detail = error +
        " The current " VITA_UPDATER_APP_NAME
        " installation was not replaced. This helper will "
        "remain available for recovery. Press X to return to LiveArea.";
    wait_for_buttons(SCE_CTRL_CROSS);
    goto shutdown;

recovery_failed:
    g_status = "Automatic recovery failed";
    g_detail = error +
        " Do not delete the backup under " VITA_UPDATER_STAGING_DIRECTORY
        "/. Leave this "
        "helper installed and retry, or reinstall the application manually.";
    wait_for_buttons(SCE_CTRL_CROSS);

shutdown:
    if (g_font != nullptr) {
        vita2d_free_pgf(g_font);
    }
    if (g_graphics_initialized) {
        vita2d_fini();
    }
    sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
    if (launch_after_shutdown) {
        const int result =
            vita::updater::launch_title(vita::updater::kMainTitleId);
        if (result < 0) {
            sceKernelExitProcess(result);
        }
    }
    sceKernelExitProcess(0);
    return 0;
}
