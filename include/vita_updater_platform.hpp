/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "vita_updater_core.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace vita::updater {

static_assert(sizeof(VITA_UPDATER_MAIN_TITLE_ID) == 10,
    "VITA_UPDATER_MAIN_TITLE_ID must contain exactly nine characters");
static_assert(sizeof(VITA_UPDATER_HELPER_TITLE_ID) == 10,
    "VITA_UPDATER_HELPER_TITLE_ID must contain exactly nine characters");

inline constexpr const char* kMainTitleId = VITA_UPDATER_MAIN_TITLE_ID;
inline constexpr const char* kHelperTitleId = VITA_UPDATER_HELPER_TITLE_ID;
inline constexpr const char* kMainPackagePath =
    "ux0:/app/" VITA_UPDATER_MAIN_TITLE_ID;
inline constexpr const char* kStagingDirectory =
    VITA_UPDATER_STAGING_DIRECTORY;
inline constexpr const char* kJournalPath =
    VITA_UPDATER_STAGING_DIRECTORY "/transaction.pending";
inline constexpr const char* kManifestPath =
    VITA_UPDATER_STAGING_DIRECTORY "/" VITA_UPDATER_MANIFEST_ASSET;
inline constexpr const char* kHelperPackagePath =
    VITA_UPDATER_STAGING_DIRECTORY "/helper-package";
inline constexpr const char* kNewPackagePath =
    VITA_UPDATER_STAGING_DIRECTORY "/new-package";
inline constexpr const char* kBackupPackagePath =
    VITA_UPDATER_STAGING_DIRECTORY "/backup-package";
enum class JournalState {
    Invalid,
    Staged,
    HelperInstalled,
    Promoting,
    AwaitingHealth,
    CleanupPending,
    RolledBack,
    RecoveryFailed
};

struct Journal {
    JournalState state = JournalState::Invalid;
    std::string version;
    int error_code = 0;
};

bool parse_build_public_key(
    std::array<std::uint8_t, 32>& public_key, std::string& error);
bool read_file_limited(const char* path, std::size_t limit,
    std::string& contents, std::string& error);
bool write_file_atomic(
    const char* path, const void* data, std::size_t size, std::string& error);
bool write_journal(const Journal& journal, std::string& error);
bool read_journal(Journal& journal, std::string& error);
bool journal_exists();

bool path_exists(const char* path);
bool ensure_directory(const char* path, std::string& error);
bool remove_update_tree(const char* path, std::string& error);
bool copy_tree(const char* source, const char* destination, std::string& error);
bool tree_size(const char* path, std::uint64_t& size, std::string& error);
bool available_storage(std::uint64_t& bytes, std::string& error);
bool sha256_file(const char* path, std::uint64_t maximum_size,
    std::array<std::uint8_t, 32>& digest, std::uint64_t& size,
    std::string& error);
bool extract_update_vpk(const char* archive_path, const char* destination,
    std::string& error);
bool package_title_matches(
    const char* package_path, const char* title_id, std::string& error);
bool package_supports_updater(
    const char* package_path, std::string& error);

int promoter_install(const char* package_path);
int promoter_delete(const char* title_id);
int promoter_check_exists(const char* title_id, bool& exists);
int launch_title(const char* title_id);

}  // namespace vita::updater
