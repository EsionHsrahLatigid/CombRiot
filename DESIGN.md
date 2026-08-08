# Design

## Source of truth

- Status: Active
- Last refreshed: 2026-08-08
- Primary product surfaces: macOS Standalone, VST3 editor, AUv2 editor
- Evidence reviewed: local engine, YUP wrapper, CombRiot editor, regression tests

## Brand

- Personality: unstable resonant machinery, precise enough to recall, hostile enough to overload
- Trust signals: deterministic note structures, visible parameter values, bounded output ceiling, versioned state schema
- Avoid: decorative glitch text, fake spectral meters, implied artist provenance, unreadable labels

## Product goals

- Goals: expose burst, delay structure, feedback polarity, decay, and ceiling as direct performance controls
- Non-goals: model a physical comb instrument; hide safety behavior; turn notes into normal equal-tempered pitch
- Success signals: notes reliably select different comb swarms; decay and polarity produce audible structural changes; output remains finite and bounded

## Personas and jobs

- Primary personas: experimental electronic musicians, sound designers, noise performers
- User jobs: trigger reproducible resonant bursts, automate unstable structures, find extreme but bounded source material
- Key contexts of use: loud monitoring, DAW automation, standalone improvisation, fast preset changes

## Information architecture

- Primary navigation: one-page instrument panel
- Core routes/screens: one instrument panel with standalone trigger, output activity meter, and parameter grid
- Content hierarchy: trigger/activity first, exciter and decay second, feedback structure third, stereo/output safety last

## Design principles

- Make instability explicit: parameter names describe the real DSP controls.
- Keep repetition reproducible: the same note and parameters produce the same structure.
- Preserve a hard exit: ceiling remains visible and always active.
- Favor dense control layout over decorative atmosphere.

## Visual language

- Color: near-black field with a hot orange accent
- Typography: compact system sans; numeric values remain high contrast
- Spacing/layout rhythm: fixed-ratio grid of rotary controls
- Shape/radius/elevation: hard background, circular controls, minimal shadowing
- Motion: value updates only; no decorative flicker

## Components

- Existing components to reuse: YUP `Slider`, `Label`, `AudioProcessorEditor`
- New/changed components: CombRiot momentary trigger pad and processor-polled output peak meter
- Token/component ownership: editor-local constants until YUP exposes a stable theme/token workflow

## Accessibility

- Target standard: practical desktop accessibility within current YUP capabilities
- Keyboard/focus behavior: Space gates the standalone trigger when editor focus reaches the CombRiot panel; mouse and Space holds are combined before publishing to the processor
- Contrast/readability: labels and numeric values remain readable against the dark field
- Reduced motion: no full-screen flashes or random animation

## Responsive behavior

- Supported breakpoints/devices: desktop plugin windows and macOS Standalone
- Layout adaptations: fixed aspect ratio; parameter count determines grid rows
- Touch/hover differences: rotary vertical drag remains the primary interaction

## Interaction states

- Loading: immediate deterministic initialization
- Empty: silence until MIDI note-on
- Error: invalid and non-finite parameter values clamp safely
- Success: values update visibly, trigger state/meter activity are visible, and audio changes deterministically
- Disabled: no hidden disabled controls

## Content voice

- Tone: technical, terse, direct
- Terminology: use real operations such as burst, feedback polarity, delay structure, and ceiling
- Microcopy rules: short noun phrases; do not imply historical provenance

## Implementation constraints

- Framework/styling system: C++20 and YUP GUI/audio processor modules
- Performance constraints: no allocation, file access, locks, or non-deterministic calls in the audio render path; UI commands cross to the audio thread through processor-owned atomic edge counters and a held-state latch
- Compatibility constraints: state version changes require migration
- Test expectations: engine regression tests, plugin bridge trigger test, three-format macOS build, bundle packaging

## CI and release contract

- `CI Summary` is the stable required check. A Linux classifier always runs; it skips macOS and Windows only for the documented docs-only allowlist and otherwise chooses the conservative heavy path.
- macOS and Windows each build, test, package, and upload one `latest` ZIP plus a strict single-line `SHA256SUMS.txt`. Actions artifacts expire after 14 days.
- Tag pushes never compile. The Release workflow resolves the tag to its commit, requires the tag and CMake project versions to match, locates the unique successful canonical `CI` push run on `main` with the same `head_sha`, requires exactly the two named unexpired platform artifacts, verifies SHA-256 and ZIP integrity, sanitizes the draft asset list, and only then publishes exactly the two versioned release assets.
- Release provenance failures are terminal. Missing, expired, duplicate, or mismatched artifacts must not trigger an automatic rebuild or partial release.
- GitHub actions are pinned to immutable commit SHAs. The release runner requires GitHub CLI 2.x or newer and the minimal `actions: read` / `contents: write` permissions.

## Open questions

- [ ] Which per-resonator state signals are useful to expose visually beyond the aggregate output peak?
