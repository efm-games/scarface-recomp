# Project notes

Background notes that are not a work queue. `v1.2.0` is the current release; with the graphics defect below resolved
the project is considered definitive, and further work is limited to bug fixes.

## First-mission lighting/texture flash

A flash was reported in the first mission at Tony's mansion. The player's captures `20261118_232001_827.bmp` (bright)
and `20261118_232002_978.bmp` (the same view looking normal a second later) show the same scene. The bright frame has
mean RGB `[103.5, 85.5, 64.0]`, the normal frame `[30.6, 22.4, 14.9]`, and the full-frame difference mean is
`[77.1, 66.5, 51.8]`.

The F2 draw histories isolate the cause. The bright frame draws the world with stage 0 set to `SELECTARG2` and
`arg2 = TEXTURE` (`c3/1,1,2`), which selects the raw texture and drops the vertex diffuse that carries the console's
lighting. Every fixed-function `c3` draw in that frame has no pixel shader and a texture factor sweeping `B4...` to
`F7...`. The normal frame draws the same geometry with stage 0 disabled (`c1/1,1,2`, texture factor `FE000000`), which
the renderer already turns into `MODULATE(diffuse, texture)`. The other ten captured frames have no fixed-function
`c3` draws at all, so the pattern is unique to the flash. Moving Tony returns the state to the steady `c1` path.

The fix is in `MakePixelKey()` in `sdk/runtime/gpu/shaders.cpp`: stage 0 with a bound texture, `ColorOp = SELECTARG2`,
`arg1 = CURRENT` and `arg2 = TEXTURE` is now rewritten to `MODULATE(CURRENT, TEXTURE)`, exactly like the existing
disabled-stage and white-UI-panel rules. The sweep itself is not applied by those draws, so it does not need changing;
only the combiner that discarded the lighting did.

## Declined: DLSS 5

DLSS 5 is not planned for this project. The official SDK is not publicly available, and a proper integration would
require renderer changes this project does not intend to make, including engine motion vectors, jitter,
exposure/HDR data, semantic and engine masks, NVIDIA runtime packaging, and hardware gating. External user-installed
overlay solutions are outside this project: they are not bundled, supported, or required, and the base renderer keeps
working without an NVIDIA card or extra redistributables.
