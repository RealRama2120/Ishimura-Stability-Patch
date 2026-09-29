# Backup inventory

This private repository backs up Ishimura Stability Patch source and original release artifacts. The source at the repository root is the version identified by the current Git tag.

## Releases present through v1.0.1

| Version | Install ZIP | Exact source | Original checksum sidecar | Changelog |
| --- | --- | --- | --- | --- |
| v0.1.0 Beta | Preserved in `archive/v0.1.0-beta/` | Not found | Not found | Not found |
| v0.1.1 Beta | Preserved in `archive/v0.1.1-beta/` | Not found | Not found | Not found |
| v1.0.0 | Preserved in `archive/v1.0.0/` | Not found | Not found | Not found |
| v1.0.1 | Preserved in `archive/v1.0.1/` | Root source and Git tag `v1.0.1` | Not found | Not found |

The v1.0.1 source snapshot was accepted as exact because `src/version.hpp` identifies 1.0.1 and every file in its local built package matches the original v1.0.1 install ZIP byte-for-byte. No source was inferred for earlier releases.

Files intentionally excluded include game files, saves, runtime logs, build intermediates, credentials, private test evidence, the unversioned Steam compatibility candidate, and third-party game assets.

Files named `SHA256SUMS.generated.txt` were generated during repository preparation to document the unchanged local archives. They are not original release sidecars.

