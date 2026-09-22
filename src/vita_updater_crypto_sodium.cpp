/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vita_updater_core.hpp"

#if defined(VITA_UPDATER_USE_SODIUM)
#include <sodium.h>
#endif

namespace vita::updater {

int verify_ed25519_sodium(const std::uint8_t public_key[32],
    const std::uint8_t signature[64], const std::uint8_t* message,
    std::size_t message_size) {
#if defined(VITA_UPDATER_USE_SODIUM)
    if (public_key == nullptr || signature == nullptr ||
        (message == nullptr && message_size != 0))
    {
        return false;
    }
    return crypto_sign_verify_detached(
               signature, message, static_cast<unsigned long long>(message_size), public_key) == 0;
#else
    (void) public_key;
    (void) signature;
    (void) message;
    (void) message_size;
    return false;
#endif
}

}  // namespace vita::updater
