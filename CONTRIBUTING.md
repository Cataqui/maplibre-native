# Contributing to Cataquí's renderer

Target `main` in `Cataqui/maplibre-native`. Keep changes focused on the Linux x64 Node.js/OpenGL renderer consumed by Cataquí Maps. Preserve public binding behavior unless the backend integration is updated with it.

Build and run the native regressions using the Docker command in [README.md](README.md). Changes to placement, shader density, resource handling or cancellation also require the Cataquí backend's Linux production-image integration gate. A passing host test does not qualify a Linux binary.

When updating upstream, start from an explicit MapLibre Native commit, review the Cataquí changes and recursive submodule revisions, and rerun native and backend image checks. Consumers may declare main; production builds must compile the exact commit resolved in their frozen lockfile. Existing package names and upstream build structure remain compatibility boundaries; package publishing is not configured for this fork.

Keep license notices and third-party attribution. The original upstream contribution guide remains available in the [upstream repository](https://github.com/maplibre/maplibre-native/blob/main/CONTRIBUTING.md).
