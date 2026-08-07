# Design

## Source of truth

- Status: Active
- Last refreshed: 2026-08-08
- Primary product surfaces: macOS Standalone, VST3 editor, AUv2 editor
- Evidence reviewed: local engine, YUP wrapper, generic parameter-grid editor, regression tests

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
- Core routes/screens: parameter grid only
- Content hierarchy: exciter and decay first, feedback structure second, stereo/output safety last

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
- New/changed components: optional future resonator activity and burst envelope visualization
- Token/component ownership: editor-local constants until YUP exposes a stable theme/token workflow

## Accessibility

- Target standard: practical desktop accessibility within current YUP capabilities
- Keyboard/focus behavior: host/YUP defaults
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
- Success: values update visibly and audio changes deterministically
- Disabled: no hidden disabled controls

## Content voice

- Tone: technical, terse, direct
- Terminology: use real operations such as burst, feedback polarity, delay structure, and ceiling
- Microcopy rules: short noun phrases; do not imply historical provenance

## Implementation constraints

- Framework/styling system: C++20 and YUP GUI/audio processor modules
- Performance constraints: no allocation, file access, locks, or non-deterministic calls in the audio render path
- Compatibility constraints: state version changes require migration
- Test expectations: engine regression tests, three-format macOS build, bundle packaging

## Open questions

- [ ] Which resonator state signals are useful to expose visually without unsafe audio-thread synchronization?

