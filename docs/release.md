# Release process

## Manifest format

The manifest contains exactly six LF-terminated lines:

```text
format=1
version=1.2.3
asset=ExampleApp.vpk
size=1234567
sha256=<64 lowercase hexadecimal characters>
signature=<128 lowercase hexadecimal characters>
```

The Ed25519 signature covers the first five lines, including the LF after
`sha256`.

## Safe publication

1. Build the updater-enabled Release VPK with the public key.
2. Generate the manifest with the private key.
3. Create a draft GitHub release using a strict tag such as `v1.2.3`.
4. Upload exactly one VPK matching the manifest and exactly one configured
   manifest asset.
5. Download both assets and verify their hashes.
6. Publish the draft only after verification.

GitHub's `/releases/latest` endpoint excludes drafts and prereleases, and the
runtime also rejects prerelease metadata. Publishing the draft is therefore
the activation step.

Keep the Ed25519 private key in an encrypted CI secret. Never write it into
source, artifacts, workflow logs, release notes, or a VPK. Preserve the same
key while deployed clients trust its public half; key rotation requires a
transition release understood by old clients.

Every target VPK must include the updater again. Otherwise users can install
that release but cannot update from it later.
