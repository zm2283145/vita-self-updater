# Architecture

## Why a helper title is required

Writable Vita storage is not an executable handoff mechanism. A SUPRX loaded
by `sceKernelLoadStartModule` remains part of the main process, and
`sceAppMgrLoadExec` does not turn an arbitrary downloaded SELF into a
post-exit bootstrap. The updater must promote a temporary installed title
before the main application exits.

## Transaction

1. The main title fetches GitHub's `/releases/latest` endpoint on a detached
   pthread. Offline errors are ignored; malformed or insecure responses are
   shown.
2. It parses strict SemVer metadata and downloads the small signed manifest.
3. After user consent, it checks storage, downloads the VPK, and verifies its
   size and SHA-256.
4. It copies the embedded helper package to staging, validates its title ID,
   writes `state=staged`, promotes the helper, then writes
   `state=helper-installed`.
5. The helper re-verifies the signature, size, and hash. It safely extracts
   the VPK, validates the target title ID and embedded updater capability, and
   copies the stopped live application into `backup-package`.
6. Immediately before replacement it writes `state=promoting`. It promotes
   the new package and writes `state=awaiting-health`.
7. On the normal success path the helper never initializes PGF,
   `vita2d`, or GXM. It launches the main title through the `psgm:play` URI
   and immediately exits. Errors and rollback results remain in the durable
   journal for the main application to display.
8. After the configured healthy-frame threshold, the new title writes
   `state=cleanup-pending`, then deletes the helper and staging tree.

This mirrors VitaShell's headless URI-launch-and-exit handoff. Hardware testing
showed that initializing graphics, tearing them down, and immediately launching
the main title can race the GPU driver. The health delay keeps rollback
available until the new build is demonstrably running, and the durable cleanup
checkpoint allows an interrupted deletion to resume on the next launch.

## Rollback

If promotion fails after `state=promoting`, the helper promotes
`backup-package`. If the new title never confirms health, launching the helper
again offers rollback. Recovery failures retain the helper, backup, journal,
and error code for manual intervention.

The updater only replaces `ux0:/app/<MAIN_TITLE_ID>`. Saves and writable
caches should live under `ux0:/data/<application>/` and must not be placed
inside the application package.
