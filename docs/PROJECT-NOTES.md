# Project notes

Background notes that are not a work queue. `v1.1.1` is the current release; after the remaining graphics defect is
resolved the project is considered definitive, and further work is limited to bug fixes.

## First-mission lighting/texture flash

A flash was reported in the first mission at Tony's mansion. The player's captures `20261118_232001_827.bmp` (bright)
and `20261118_232002_978.bmp` (the same view looking normal a second later) show the same scene. The bright frame has
mean RGB `[103.5, 85.5, 64.0]`, the normal frame `[30.6, 22.4, 14.9]`, and the full-frame difference mean is
`[77.1, 66.5, 51.8]`. Their draw histories show the bright frame drawing the world with texture factors sweeping
through `B6...` to `CE...`, while the normal frame holds `FE000000`. Moving Tony can bring the steady value back. This
looks like a scene-tint state leaking into world draws, not a missing texture. This is the current focus.

## Declined: DLSS 5

DLSS 5 is not planned for this project. The official SDK is not publicly available, and a proper integration would
require renderer changes this project does not intend to make, including engine motion vectors, jitter,
exposure/HDR data, semantic and engine masks, NVIDIA runtime packaging, and hardware gating. External user-installed
overlay solutions are outside this project: they are not bundled, supported, or required, and the base renderer keeps
working without an NVIDIA card or extra redistributables.
