# Vita Self-Updater

A reusable, opt-in updater framework for PS Vita homebrew. It checks a
published GitHub release asynchronously, verifies a signed manifest and VPK,
installs a temporary helper title, replaces the stopped application through
the Vita package promoter, and retains a rollback package until the new build
has demonstrated a healthy launch.

## Platform limitation

An ordinary Vita title cannot download an arbitrary SELF/ELF/PRX, exit, and
continue executing it. `sceAppMgrLoadExec` launches registered application
content, while a module loaded with `sceKernelLoadStartModule` dies with the
current process. This framework therefore installs a **temporary second
title**. Its bubble is briefly visible. The normal success path is headless:
the helper never initializes font or graphics resources before using the same
URI-launch-and-exit sequence as VitaShell. Graphics are initialized lazily
only when an error or rollback decision must be shown. Launching after a
graphical helper had torn down GXM caused hardware-observed GPU/SceShell
failures.

The main updater executable uses the unsafe auth ID
`0x2808000000000000`, as VitaShell does for package-promoter access. A
HENkaku/taiHEN environment is required. Stock retail systems are unsupported.

## Security properties

- GitHub TLS peer and hostname verification using the Vita CA bundle.
- No GitHub token embedded in the application.
- Only GitHub's latest published, non-prerelease release is accepted.
- Strict semantic-version comparison.
- Ed25519 signature over a canonical six-field manifest.
- Full-file SHA-256 and exact-size verification in both processes.
- Bounded metadata, manifest, download, archive-entry, and extraction sizes.
- Archive traversal, links, special files, duplicates, and unsafe names rejected.
- Package `TITLE_ID` and updater capability checked before promotion.
- Durable transaction journal, full application backup, and rollback promotion.
- Application data outside `ux0:/app/<TITLE_ID>` remains untouched.
- Cleanup begins only after the relaunched application has rendered the
  configured healthy-frame threshold. An interrupted cleanup resumes from its
  durable checkpoint on the next launch.

No updater can recover automatically if both the new application and its
temporary helper are unlaunchable. Keep a manual reinstall path.

## Repository layout

| Path | Purpose |
|---|---|
| `include/` | Configuration and public/runtime interfaces |
| `src/vita_updater_core.cpp` | Host-testable SemVer, JSON, manifest, SHA-256, and planning logic |
| `src/vita_updater_runtime.cpp` | Main-title network check, prompt, download, and helper installation |
| `src/vita_updater_helper.cpp` | Temporary-title verification, backup, promotion, rollback, and safe LiveArea handoff |
| `src/vita_updater_platform.cpp` | Vita filesystem, archive, promoter, journal, and launch operations |
| `tools/` | Ed25519 key generation, manifest signing, and package-head generation |
| `examples/` | Lifecycle, build, and GitHub Actions integration examples |
| `tests/` | Host cryptographic/core tests and package-header tests |

## Quick start

1. Read [`docs/integration.md`](docs/integration.md) and choose unique main
   and helper title IDs.
2. Generate an Ed25519 keypair:

   ```powershell
   python .\tools\generate-keypair.py `
     --private-key release-private.pem `
     --public-key-hex release-public.hex
   ```

3. Store the private PEM outside the repository. Compile its 64-character
   public-key hex into both the application and helper.
4. Copy and customize `include/vita_updater_config.hpp`, or override every
   setting using compiler definitions.
5. Integrate the four lifecycle calls shown in
   [`examples/integration/app_hooks.c`](examples/integration/app_hooks.c).
6. Build the main title as an unsafe FSELF, build the helper as a normal safe
   FSELF, and embed the helper package under `app0:/updater/`.
7. Sign each VPK:

   ```powershell
   python .\tools\sign-manifest.py --version 1.2.3 `
     --asset .\dist\ExampleApp.vpk `
     --private-key C:\secure\release-private.pem `
     --output .\dist\vita-update-manifest.txt
   ```

8. Upload the VPK and manifest to the same draft GitHub release, verify both,
   then publish a strict tag such as `v1.2.3`.

## Tests

Install Python dependencies and run:

```powershell
python -m pip install -r requirements.txt
.\test.ps1
```

The script runs host parser/hash/signature tests and Vita Release compilation
checks. It does not access a Vita.

## License

GPL-3.0-or-later. The fake-package `head.bin` transform and template are
derived from VitaShell by TheFloW; see [`NOTICE`](NOTICE) and [`COPYING`](COPYING).
