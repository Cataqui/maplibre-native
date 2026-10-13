# Cataquí native version comparison

This experiment compares the maintained Native 6.4.1 engine with complete upstream snapshots through commit `49537ef244ba08d730cf707c67f16da06956f3ee`. It ports the three existing Cataquí capabilities onto each snapshot; it does not remove our features to make upstream appear faster.

The frozen inventory covers every intervening commit. Linux/Node/core build changes, generator changes and build dependency changes require compiled comparisons. Versions that change only unrelated platform or documentation paths are marked equivalent to the preceding measured build inputs, rather than reported as independently timed.

## Workload

The fixtures contain real public Protomaps Brazil tiles, Cataquí's production basemap style and generated category badges, and verified Noto glyph blocks. Controlled reference markers exercise collision placement and GeoJSON updates; they are synthetic, not real POIs. See [fixture attribution](fixtures/ATTRIBUTION.md).

Eight cases cover São Paulo, Rio, Sorocaba and a broad Brazil view at two fractional densities. Each measured block covers fresh map construction/loading/placement/drawing, retained-map placement/drawing, two concurrent retained maps, GeoJSON changes, selection-filter redraws, viewport resizing and release. Resource lookup is offline and in memory. Raster bytes and visible reference IDs are recorded; repeated unchanged maps must produce identical pixels. API, R2, HTTP, geographic import and image encoding time are outside these native-engine timings.

The runner fixes Linux x64, Ubuntu 24.04, Node 24.20.0 / ABI 137, portable x86-64 compiler flags, OpenGL/Mesa llvmpipe, two software-rendering threads, two vCPUs and 8 GiB RAM. Standard GitHub runners are a performance proxy for the configured Cloudflare container size, not measurements on Cloudflare hardware.

Each group builds a baseline on the same runner and measures four control passes. Each candidate is measured in baseline/candidate/candidate/baseline order. Warm-up blocks are excluded. Keep raw samples and control variability; do not select a winner from one noisy median or compare absolute timings from different runners.

## Run

Dispatch `.github/workflows/native-versions.yml` in `pilot` mode to qualify the latest port, then `sweep` for the frozen inventory. `finalists` uses the explicit shortlist in groups.json, increases block counts and retains annotated PNG outputs for visual review. The initial shortlist contains the newest endpoint; update it with sweep leaders before final selection. The workflow runs native placement/source-update tests and the real OpenGL shader-density regression before any candidate measurement. Build, test or fixture failures disqualify that candidate until resolved; they are not performance results.

Results are short-lived workflow artifacts containing build logs, controls and candidate raw measurements. They do not publish packages or deploy services. Review visual differences and remeasure finalists before updating the fork's main branch or the backend commit pin.
