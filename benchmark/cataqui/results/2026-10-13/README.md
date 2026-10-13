# Native version comparison — 2026-10-13

Cataquí retains upstream `a34723ff4a53d9622f7e42fc3f10197d5acb94bd`, with its three native extensions. The old base was `d4bde4c93eb15650d0ddb6822d24351f792ea649`. The frozen comparison ends at `49537ef244ba08d730cf707c67f16da06956f3ee`; subsequent upstream commits are outside this comparison.

## Coverage and workload

All 256 upstream versions in the frozen range are accounted for: 140 distinct build candidates were compiled and measured, and 116 versions reused the preceding equivalent build inputs. There are zero failed or unmeasured versions after three helper failures were fixed and rerun. See [version coverage](version-coverage.csv), [full comparison](comparison.csv), and [sweep plot](sweep.png).

The offline workload uses 300 public Protomaps Brazil tiles from the 2026-10-13 archive, the backend production basemap style, official Noto glyphs, and the actual Mateo badge assets. Controlled reference markers exercise São Paulo, Rio, Sorocaba and broad central Brazil at two fractional pixel densities. Fixture hashes are recorded in the benchmark inputs. API calls, network transfers, R2, image encoding and process/container startup are excluded.

Linux x64, Node 24.20.0 / ABI 137, OpenGL Mesa llvmpipe, two render threads and a 2 CPU / 8 GiB container were used. Every measured candidate passed the real native placement/source-update and shader-density regressions. Cold measures fresh map creation inside an existing Node process; retaining the GL display explains much of the cold gain, which may be smaller when another map already keeps that display alive.

The sweep used paired ABBA ordering with four blocks and control repeats. Finalist runs used twelve blocks and four baseline control passes. The score equally weights six phases across eight cases using a geometric mean; it is not weighted by production traffic. Intervals use whole-pass/block bootstrap resampling. Individual-cell screening is not corrected for multiple comparisons.

## First longer finalist comparison

Ratios below 1 mean faster than the old base.

| Upstream snapshot | Balanced ratio | 95% interval | Screened slower cells |
| --- | ---: | --- | ---: |
| `a34723f` | 0.9473 | 0.9348–0.9575 | 0 |
| `ccb9af1` | 0.9534 | 0.9463–0.9588 | 0 |
| `b288c6d` | 0.9560 | 0.9442–0.9630 | 0 |
| `49537ef` | 0.9590 | 0.9463–0.9716 | 2 |
| `d9664ee` | 0.9695 | 0.9499–0.9826 | 1 |

The selected snapshot was about 5.3% faster than the old base in this run. The frozen endpoint was about 4.1% faster, or roughly 1% slower than the selected snapshot. Their intervals overlap, so these measurements do not establish a uniquely fastest engine.

The common selected snapshot was repeated in six additional paired groups, leading five of six point estimates; its sole loss was about 0.15%, within the observed noise. Each group has its own comparison and visual report in the directories here. Analyze independent repeats separately rather than treating repeated observations as separate versions.

Queried feature IDs and image dimensions matched. Exact image hashes vary even across baseline processes; numeric comparison and control frames found small pixel differences rather than an exact-pixel guarantee. Cell regression flags in some later versions were around 0.5–1.3% and should be treated as screening evidence, not proof that every retained render becomes slower. See the visual reports for actual per-frame differences.

## Provenance and reproduction

- [Complete sweep](https://github.com/Cataqui/maplibre-native/actions/runs/38412620486)
- [Three candidate retries](https://github.com/Cataqui/maplibre-native/actions/runs/38432897107)
- [First longer finalists](https://github.com/Cataqui/maplibre-native/actions/runs/38427016291)
- [Six additional paired groups](https://github.com/Cataqui/maplibre-native/actions/runs/38437498111)
- [Earlier endpoint comparison](https://github.com/Cataqui/maplibre-native/actions/runs/38415116981)

`raw-measurements.tar.gz` preserves the JSON measurements, reports and metadata from these runs; images and build logs remain in their original Actions artifacts. The archive includes independent repeats and must be analyzed by run/group. The benchmark harness, frozen inventory, fixture manifest and analysis commands live in the parent benchmark directory.

Future upstream changes remain deferred until they pass native and backend correctness checks and a comparison against the retained engine. Keeping a fork behind upstream is intentional; ancestry alone is not a performance or correctness result.
