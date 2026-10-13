# Benchmark data attribution

The vector tiles contain public Brazil geography from OpenStreetMap contributors, distributed as a Protomaps Produced Work under the Open Database License. Source archive: https://build.protomaps.com/20261013.pmtiles (Protomaps 4.15.2). These are selected tile bodies, not a verified copy of the complete planet archive. Each stored body is SHA-256 pinned in manifest.json.

The font PBFs come from https://github.com/openmaptiles/fonts/releases/download/v2.0/noto-sans.zip, acquired archive SHA-256 d117316544b43a5dde7ee761b36e17701e9f85574e181d76a74814240fdbaf34. The exact required blocks are retained with their source references and body hashes. See noto-OFL.txt and noto-NOTICE.txt for font notices.

The production basemap style and category badge artwork are Cataquí assets generated from its Maps checkout. Reference-marker names and positions in the benchmark are controlled synthetic inputs for collision and update stress, not asserted real POIs. Geographic basemap tiles are real.
