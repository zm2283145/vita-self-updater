/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vita_updater_core.hpp"
#include "vita_updater_platform.hpp"

#include <psp2/appmgr.h>
#include <psp2/kernel/processmgr.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

extern "C" {
unsigned int _newlib_heap_size_user = 32u * 1024u * 1024u;
int sceUserMainThreadStackSize = 1024 * 1024;
}

namespace {

using vita::updater::Journal;
using vita::updater::JournalState;
using vita::updater::SignedManifest;

void set_status(const std::string&, const std::string& = {}) {}

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
    sceAppMgrDestroyOtherApp();
    sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);

    Journal journal;
    std::string error;
    if (!vita::updater::read_journal(journal, error)) {
        goto shutdown;
    }

    if (journal.state == JournalState::RolledBack) {
        goto shutdown;
    }
    if (journal.state == JournalState::CleanupPending) {
        goto shutdown;
    }
    if (journal.state == JournalState::Promoting ||
        journal.state == JournalState::AwaitingHealth ||
        journal.state == JournalState::RecoveryFailed)
    {
        rollback(journal, error);
        goto shutdown;
    }

    if (journal.state != JournalState::HelperInstalled &&
        journal.state != JournalState::Staged)
    {
        error = "The updater journal is not in a runnable state.";
        goto shutdown;
    }
    if (perform_update(journal, error)) {
        goto shutdown;
    }
    if (journal.state == JournalState::Promoting ||
        journal.state == JournalState::AwaitingHealth)
    {
        if (rollback(journal, error) >= 0) {
            goto shutdown;
        }
        goto shutdown;
    }

shutdown:
    sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
    const int result =
        vita::updater::launch_title(vita::updater::kMainTitleId);
    if (result < 0) {
        sceKernelExitProcess(result);
    }
    sceKernelExitProcess(0);
    return 0;
}
