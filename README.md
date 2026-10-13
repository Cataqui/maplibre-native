# Cataquí MapLibre Native

Cataquí maintains this MapLibre Native fork for the Linux Node.js renderer used by its Maps service. The supported build uses Linux x64, Node.js 24.20.0 (ABI 137), OpenGL and Mesa software rendering.

The renderer extends the qualified Native 6.5.0-pre.1 snapshot with GeoJSON source updates from Node.js, static label-placement preparation followed by drawing the same scene, and runtime pixel density in OpenGL shaders. Cataquí's backend owns map styles, geographic data, application policy and the production container.

## Build and test

Install Docker and Git, then clone with the pinned submodules:

```sh
git clone --recurse-submodules https://github.com/Cataqui/maplibre-native.git
cd maplibre-native
docker build --platform linux/amd64 -f docker/cataqui/Dockerfile -t cataqui-maplibre-native:test .
```

The build compiles the Node addon and runs the native placement/source-update and shader-density regressions. The addon is generated at `/native/source/platform/node/lib/node-v137/mbgl.node` inside the image. See [native regression instructions](test/cataqui/README.md) for individual commands.

The backend declares the main branch and consumes its exact lockfile-resolved commit and separately checks the compiled binary, retained scenes, resource failures, cancellation and output pixels in its production image. This repository does not publish npm packages or deploy Cataquí services.

## Maintenance

`main` is Cataquí's maintained branch. See [CONTRIBUTING.md](CONTRIBUTING.md) for changes and upstream updates. Other platform source and upstream reference documentation remain available for compatibility and future merges; this fork's CI qualifies the Linux Node renderer.

The [2026-10-13 version comparison](benchmark/cataqui/results/2026-10-13/README.md) records the selected upstream snapshot, workload, complete commit coverage and repeat measurements.

MapLibre Native and its original contributors retain their attribution. The [BSD-2-Clause license](LICENSE.md) and [dependency licenses](LICENSES.core.md) remain in force. [Upstream project](https://github.com/maplibre/maplibre-native).
