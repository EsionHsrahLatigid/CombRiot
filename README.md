# CombRiot

CombRiot is a MIDI-triggered stereo noise instrument built with [YUP](https://github.com/kunitoki/yup). It excites eight fractional comb resonators with a deterministic note-seeded burst, then lets the resonator bank decay through bounded feedback. The Standalone/editor surface also includes a built-in momentary trigger for quick auditioning without external MIDI.

This project is independent and self-contained except for an adjacent YUP checkout at `../yup`. It builds the CombRiot Standalone app, VST3, AUv2, and deterministic DSP regression tests.

## Identity

| Surface | Value |
| --- | --- |
| App ID | `audio.2bit.combriot` |
| Plugin ID | `audio.2bit.CombRiot` |
| AU subtype | `CmRt` |
| AU manufacturer | `2Bit` |
| Formats | Standalone, VST3, AUv2 on macOS |
| Type | Synth, stereo output, MIDI input |

## Standalone/editor controls

- The `TRIGGER` pad is momentary. Hold it to fire the same deterministic internal note used for standalone auditioning.
- The Space key also gates the standalone trigger when the editor has keyboard focus. Mouse and Space are combined, so releasing one input leaves the trigger held while the other remains down.
- External MIDI note-on/off handling is preserved. MIDI notes continue to select their own deterministic structures and take priority over the built-in trigger while held.
- The horizontal activity meter shows recent post-ceiling output peak from the audio processor.

## Sound engine

CombRiot does not track conventional pitch. MIDI note number selects the exciter seed and comb-delay structure. Velocity scales the burst amplitude. The audio path performs no allocation, locking, file access, or random-device calls.

Parameters:

| Parameter | Function |
| --- | --- |
| Burst | Seeded noise-burst exciter duration |
| Decay | Comb feedback decay target |
| Feedback | Decay-derived feedback amount, clamped below unity |
| Polarity | Negative-to-positive comb feedback morph |
| Structure | Deterministic delay layout selector |
| Delay Slew | Movement rate toward changed delay targets |
| Stereo Spread | Fixed voice pan width |
| Output | Post-bank gain in decibels |
| Ceiling | Always-on absolute output peak limit |

## Requirements

- macOS 11 or newer for plugin bundle builds
- Apple Clang with C++20 support
- CMake 3.31 or newer
- Ninja
- Xcode / macOS SDK for AU builds
- A local YUP checkout at `../yup`, or network access for the pinned fallback checkout

YUP is pinned to commit `9a1c9bc699b6a714f6f52486462d98a140c8bf95` when the adjacent checkout is absent.

## Build and test

Fast DSP-only loop:

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug
```

Release app and plugins:

```sh
cmake --preset plugin-release
cmake --build --preset plugin-release --parallel --target combriot_release_bundles
ctest --preset plugin-release
```

Artifacts:

- `combriot_release_bundles`
- `build/plugin-release/combriot_standalone_plugin.app`
- `build/plugin-release/VST3/Release/combriot_vst3_plugin.vst3`
- `build/plugin-release/combriot_au_plugin.component`

Local installation is intentionally separate from the build:

```sh
cp -R build/plugin-release/VST3/Release/combriot_vst3_plugin.vst3 "$HOME/Library/Audio/Plug-Ins/VST3/"
cp -R build/plugin-release/combriot_au_plugin.component "$HOME/Library/Audio/Plug-Ins/Components/"
```

The local macOS build ad-hoc signs all three bundles. Distribution still requires a Developer ID signing and notarization workflow.

## CI and releases

`.github/workflows/ci.yml` is the required CI entrypoint for pushes to `main`, pull requests, and manual runs. A lightweight Linux classifier always runs. Changes limited to `README.md`, `DESIGN.md`, `LICENSE`, `docs/**`, or `.github/ISSUE_TEMPLATE/**` skip the heavy jobs; every other change runs Debug tests and Release bundle builds on macOS 26 arm64 and Windows 2025 x64. Manual dispatches default to forcing both heavy jobs.

Successful heavy runs upload two immutable, 14-day artifacts:

- `CombRiot-latest-macos-arm64`, containing `CombRiot-latest-macos-arm64.zip` and `SHA256SUMS.txt`
- `CombRiot-latest-windows-x64`, containing `CombRiot-latest-windows-x64.zip` and `SHA256SUMS.txt`

`.github/workflows/release.yml` is the only `v*` tag workflow. It performs no compilation. The Ubuntu release job resolves lightweight or annotated tags to a commit, requires the tag version to match the CMake project version, requires one successful `CI` push run on `main` for that exact SHA, downloads exactly the two expected unexpired artifacts, verifies their strict single-line SHA-256 manifests and ZIP integrity, then publishes versioned assets such as `CombRiot-0.2.0-macos-arm64.zip` and `CombRiot-0.2.0-windows-x64.zip`. Publication uses a draft release whose asset list is sanitized and rechecked to contain exactly those two ZIPs. Missing, expired, ambiguous, or mismatched provenance fails closed.

Release operator sequence: merge or push the version commit to `main`, wait for both platform jobs and `CI Summary` to pass, then create and push the version tag. GitHub CLI 2.x or newer is required by the release runner. Never move or reuse a published tag; correct the source and use the next patch version instead.

## Verification covered

The engine tests cover deterministic same-note output, note-selected structure changes, silence before trigger, velocity-zero silence, comb tail decay, longer-decay energy, polarity changes, extreme-parameter finiteness, output ceiling behavior, and stereo buffer rendering. The plugin bridge test covers the processor-owned synthetic standalone trigger and proves it produces nonzero audio through the wrapper path.

## Current limits

- The editor is still intentionally dense and utility-first; product-specific graphics beyond the trigger and output activity meter are future work.
- The verified local artifacts are arm64.
- DAW scanning, AU/VST3 host validation, listening tests, and calibrated loudness tests remain host-specific follow-up work.
