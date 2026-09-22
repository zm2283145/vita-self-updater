# Security

Report vulnerabilities privately through GitHub's security-advisory feature.
Do not include private signing keys, unpublished release assets, user data, or
device identifiers in a public issue.

The example configuration and tests contain no production key. Generate a
dedicated Ed25519 keypair, keep the private PEM outside the repository, and
store CI copies only in encrypted secrets. If a private key is exposed, stop
publishing releases under that trust root and ship an explicit key-rotation
transition to already installed clients.

This project depends on privileged Vita package-promoter behavior. Treat
changes to archive extraction, path validation, signature verification,
journaling, promotion, rollback, or helper cleanup as security-sensitive and
repeat hardware power-loss testing before release.
