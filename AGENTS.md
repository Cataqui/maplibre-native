# Cataquí MapLibre Native

This repository owns Cataquí's native rendering engine changes. The backend Maps service owns provider integrations, map style and assets, geographic data, API contracts, privacy policy and deployment.

- Maintain `main` as the Cataquí branch. The qualified engine base is upstream commit `a34723ff4a53d9622f7e42fc3f10197d5acb94bd` (Native Node 6.5.0-pre.1), selected by the 2026-10-13 performance comparison.
- Qualify Linux x64, Node.js 24.20.0 / ABI 137, OpenGL and Mesa software rendering. Other platform source is retained for upstream compatibility.
- Preserve prepared-tree ownership, invalidation on scene mutations, single consumption, cancellation and GeoJSON binding validation.
- Shader changes must update both GLSL source under `shaders/` and generated OpenGL headers under `include/mbgl/shaders/gl/`.
- Preserve existing upstream C++ and binding conventions, public package names, licenses and submodule commit pins.
- Run `docker build --platform linux/amd64 -f docker/cataqui/Dockerfile -t cataqui-maplibre-native:test .` after native changes and `git diff --check` before handoff. Native regressions live in `test/cataqui/` and `platform/node/test/cataqui/`.
- Test bugs through the real native implementation and verify the regression fails before the fix. Backend-dependent rendering scenarios stay in the Cataquí backend and must be qualified with the production Linux image.
- Keep automation under `.github`. Do not add upstream mobile releases, package publishing or deployments without an explicit request.
- Keep future upstream updates separate from Cataquí behavior changes and declare the maintained main branch in consumers and freeze its resolved commit in the lockfile.
