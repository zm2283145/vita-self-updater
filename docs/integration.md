# Integration guide

## Configuration

Override the defaults in `vita_updater_config.hpp` with compiler definitions:

| Definition | Meaning |
|---|---|
| `VITA_UPDATER_APP_NAME` | User-visible application name |
| `VITA_UPDATER_MAIN_TITLE_ID` | Existing nine-character title ID |
| `VITA_UPDATER_HELPER_TITLE_ID` | Unique nine-character temporary title ID |
| `VITA_UPDATER_GITHUB_OWNER` | GitHub repository owner |
| `VITA_UPDATER_GITHUB_REPO` | GitHub repository name |
| `VITA_UPDATER_CURRENT_VERSION` | Current strict SemVer without `v` |
| `VITA_UPDATER_PUBLIC_KEY_HEX` | Raw Ed25519 public key as 64 hex characters |
| `VITA_UPDATER_STAGING_DIRECTORY` | Dedicated updater directory under `ux0:/data/` |
| `VITA_UPDATER_MANIFEST_ASSET` | Manifest release-asset filename |
| `VITA_UPDATER_HEALTH_FRAMES` | Successful frames before health confirmation |

Do not reuse a helper title ID belonging to another application.

## Main-title lifecycle

Call `vita_updater_init()` after Vita app/network prerequisites can be
initialized. Call `vita_updater_poll()` once per frame. Call
`vita_updater_render_dialog()` after scene rendering and before buffer swap so
Sony's common dialog is composited. Call `vita_updater_shutdown()` before
normal process exit.

The release check and downloads use detached pthreads. Do not call these
functions concurrently from several application threads.

## Build and package

Compile all sources with C++20, exceptions and RTTI disabled, and
`VITA_UPDATER_USE_SODIUM=1`. Link the main title with:

```text
curl mbedtls mbedx509 mbedcrypto archive sodium bz2 zstd z
SceNet_stub SceNetCtl_stub SceRtc_stub SceIofilemgr_stub
ScePromoterUtil_stub SceAppUtil_stub SceAppMgr_stub
SceCommonDialog_stub SceSysmodule_stub SceProcessmgr_stub
vita2d pthread stdc++
```

Build `vita_updater_helper.cpp`, `vita_updater_core.cpp`,
`vita_updater_crypto_sodium.cpp`, and `vita_updater_platform.cpp` as a separate
normal safe FSELF. Its package requires:

```text
eboot.bin
sce_sys/param.sfo
sce_sys/package/head.bin
```

Embed that package in the main VPK as:

```text
updater/eboot.bin
updater/sce_sys/param.sfo
updater/sce_sys/package/head.bin
```

Keep the helper's success path headless. The supplied implementation delays
`vita2d` and PGF initialization until an error or rollback prompt is required.
Do not add splash screens, progress rendering, or eager graphics
initialization before its `psgm:play` handoff; hardware testing found that
tearing down GXM immediately before launching the main title can crash the GPU
driver.

The main title must be created with:

```powershell
vita-make-fself -a 0x2808000000000000 -c app.velf eboot.bin
```

Generate title-specific package headers using `tools/make-head-bin.py`.
The helper SFO must use `VITA_UPDATER_HELPER_TITLE_ID`; the main SFO and VPK
must use `VITA_UPDATER_MAIN_TITLE_ID`.

`examples/build-helper.ps1` provides a complete helper build and produces an
`updater/` directory ready to embed plus `main-head.bin`. The host
application's existing build remains responsible for compiling
`vita_updater_runtime.cpp`, linking the listed libraries, creating its unsafe
FSELF, and adding those generated files to its VPK.

## Runtime requirements

Install VitaSDK packages:

```powershell
vdpm install libsodium curl-mbedtls libarchive
```

The framework also uses `vita2d` and the VitaSDK system stubs listed above.
Test promoter behavior on every firmware/taiHEN combination you support.
