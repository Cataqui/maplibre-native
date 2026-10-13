# Cataquí native version comparison

This experiment compares the maintained Native 6.4.1 engine with complete upstream snapshots through commit `49537ef244ba08d730cf707c67f16da06956f3ee`. It ports the three existing Cataquí capabilities onto each snapshot; it does not remove our features to make upstream appear faster.

The frozen inventory covers every intervening commit. Linux/Node/core build changes, generator changes and build dependency changes require compiled comparisons. Versions that change only unrelated platform or documentation paths are marked equivalent to the preceding measured build inputs, rather than reported as independently timed.

## Workload

The fixtures contain real public Protomaps Brazil tiles, Cataquí's production basemap style and generated category badges, and verified Noto glyph blocks. Controlled reference markers exercise collision placement and GeoJSON updates; they are synthetic, not real POIs. See [fixture attribution](fixtures/ATTRIBUTION.md).

Eight cases cover São Paulo, Rio, Sorocaba and a broad Brazil view at two fractional densities. Each measured block covers fresh map construction/loading/placement/drawing, retained-map placement/drawing, two concurrent retained maps, GeoJSON changes, selection-filter redraws, viewport resizing and release. Resource lookup is offline and in memory. Raster bytes and visible reference IDs are recorded; repeated unchanged maps must produce identical pixels. API, R2, HTTP, geographic import and image encoding time are outside these native-engine timings.

The `cold` phase creates a map inside an already running Node process, after the preceding case's maps have been released. It does not measure process or container startup. A display retained by newer upstream versions can improve this phase; the same improvement may be smaller when other production maps already keep the display alive. Evaluate retained-map phases separately rather than projecting the aggregate score onto request latency.

The runner fixes Linux x64, Ubuntu 24.04, Node 24.20.0 / ABI 137, portable x86-64 compiler flags, OpenGL/Mesa llvmpipe, two software-rendering threads, two vCPUs and 8 GiB RAM. Standard GitHub runners are a performance proxy for the configured Cloudflare container size, not measurements on Cloudflare hardware.

Each group builds a baseline on the same runner and measures four control passes. Each candidate is measured in baseline/candidate/candidate/baseline order. Warm-up blocks are excluded. Keep raw samples and control variability; do not select a winner from one noisy median or compare absolute timings from different runners.

## Run

Dispatch `.github/workflows/native-versions.yml` in `pilot` mode to qualify the latest port, then `sweep` for the frozen inventory. `finalists` uses the explicit shortlist in groups.json, increases block counts and retains annotated PNG outputs for visual review. The initial shortlist contains the newest endpoint; update it with sweep leaders before final selection. The workflow runs native placement/source-update tests and the real OpenGL shader-density regression before any candidate measurement. Build, test or fixture failures disqualify that candidate until resolved; they are not performance results.

`retry` reruns only the explicit failed-version list in groups.json. Put retry artifacts beside sweep artifacts for analysis: a successful retry supersedes its build failure while preserving that failure in the report. Analyze independent measured repeats separately rather than overwriting or pooling them silently.

When `finalists-*` groups exist, finalist dispatch runs those groups in parallel. The second qualification stage repeats additional close contenders alongside the first stage's leader on each runner. This supplies direct comparisons on the same runner as well as each version's paired original-baseline measurement.

Results are short-lived workflow artifacts containing build logs, controls and candidate raw measurements. They do not publish packages or deploy services. Review visual differences and remeasure finalists before updating the fork's main branch or the backend commit pin.

After downloading results, `analyze.py` produces paired timing summaries and `coverage.py` accounts for every frozen version. For finalist artifacts, `visual_compare.py --results <artifact-directory> --output <report.json>` uses Pillow and NumPy to quantify image differences against four baseline control passes. These measurements inform visual review; observed baseline variation is not an automatic correctness tolerance.

`plot.py --comparison <comparison.json> --output <chart.png>` renders aggregate confidence intervals and phase ratios with Matplotlib and NumPy. Gray columns have no independent measurement; consult the coverage report to distinguish equivalent build inputs from pending or failed candidates.
