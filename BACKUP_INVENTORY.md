# Backup inventory

This private repository backs up Ishimura Stability Patch source and original release artifacts. The source at the repository root is the version identified by the current Git tag.

## Releases present through v1.0.3

| Version | Install ZIP | Exact source | Original checksum sidecar | Changelog |
| --- | --- | --- | --- | --- |
| v0.1.0 Beta | Preserved in `archive/v0.1.0-beta/` | Not found | Not found | Not found |
| v0.1.1 Beta | Preserved in `archive/v0.1.1-beta/` | Not found | Not found | Not found |
| v1.0.0 | Preserved in `archive/v1.0.0/` | Not found | Not found | Not found |
| v1.0.1 | Preserved in `archive/v1.0.1/` | Root source and Git tag `v1.0.1` | Not found | Not found |
| v1.0.2 | Preserved in `archive/v1.0.2/` | Original source ZIP and Git tag `v1.0.2` | Both original sidecars preserved | Original Nexus changelog preserved |
| v1.0.3 | Preserved in `archive/v1.0.3/` | Original source ZIP, root source, and Git tag `v1.0.3` | No archive sidecars found; complete source manifest preserved | Original Nexus changelog preserved |

The v1.0.1 source snapshot was accepted as exact because `src/version.hpp` identifies 1.0.1 and every file in its local built package matches the original v1.0.1 install ZIP byte-for-byte. No source was inferred for earlier releases.

The v1.0.2 source archive contains a complete 36-entry `SOURCE_CHECKSUMS.txt`; every entry was verified before upload.

The newest version is v1.0.3. This is established independently by the release filename, DLL file/product version, source version header, README, source ZIP directory, and Nexus changelog. Its source archive also contains a complete 36-entry `SOURCE_CHECKSUMS.txt`; every entry was verified before upload.

## Original archive SHA-256 values

| Version | File | SHA-256 |
| --- | --- | --- |
| v0.1.0 Beta | `Ishimura_Stability_Patch_v0.1.0_Beta_Rama2120.zip` | `C6750C12F64EC7124A2768B5ECF059B86C0020C2FD19E11A00208F4A367EEFE2` |
| v0.1.1 Beta | `Ishimura_Stability_Patch_v0.1.1_Beta_Rama2120.zip` | `01DD4FEF2F7CE6D4697C328EC33B93142EA5CDDD2C6AD20531A4CAE682030558` |
| v1.0.0 | `Ishimura_Stability_Patch_v1.0.0_Rama2120.zip` | `DA53D5365E3571BE065E38268F8CBCA13E8C304879535B2E15312B17FC0BBDE3` |
| v1.0.1 | `Ishimura_Stability_Patch_v1.0.1_Rama2120.zip` | `A6B3031BD30C300624A3C2D3C82DF1041EDF1CC94A6FCF7B0CCF5BCFC6F389B6` |
| v1.0.2 | `Ishimura_Stability_Patch_v1.0.2_Rama2120.zip` | `A22E3B0F0376FE8E349FDF6B6E26512759D6551063592E21B1CF3421CCE5D2FF` |
| v1.0.2 | `Ishimura_Stability_Patch_v1.0.2_Source.zip` | `857E8EB3383A08710317E765001B266EA12D7384D0AA07563C8AC1EDDF7792EB` |
| v1.0.3 | `Ishimura_Stability_Patch_v1.0.3_Rama2120.zip` | `50BE7D09E7DDA3A6DB5252CE892D835DA2F008ABBC23F168BB7D37A9BF2170B8` |
| v1.0.3 | `Ishimura_Stability_Patch_v1.0.3_Source.zip` | `04042F1F8C70C9E231AA18E69B9E54E48EFE7BB84A8E2949FC2FA91A3138E8E8` |

Files intentionally excluded include game files, saves, runtime logs, build intermediates, credentials, private test evidence, the unversioned Steam compatibility candidate, and third-party game assets.

Files named `SHA256SUMS.generated.txt` were generated during repository preparation to document the unchanged local archives. They are not original release sidecars.

