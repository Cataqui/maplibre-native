# syntax=docker/dockerfile:1.7
FROM node:24.20.0-bookworm-slim AS node
FROM ubuntu:24.04
RUN apt-get update && apt-get install --no-install-recommends -y \
	ca-certificates git xvfb xauth libgl1-mesa-dri libegl1 \
	build-essential clang cmake ninja-build pkg-config python3 libuv1-dev \
	libglfw3-dev libglu1-mesa-dev libgl1-mesa-dev libcurl4-openssl-dev \
	libpng-dev libjpeg-dev libicu-dev libwebp-dev libx11-dev libxrandr-dev \
	libxi-dev libxcursor-dev libxinerama-dev \
	&& rm -rf /var/lib/apt/lists/*
COPY --from=node /usr/local/bin/node /usr/local/bin/node
WORKDIR /native/source
COPY . .
ENV LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe LP_NUM_THREADS=2
RUN c++ -std=c++20 -O2 -DMLN_RENDER_BACKEND_OPENGL=1 -Iinclude -Ivendor/rapidjson/include \
	test/cataqui/uniform-pixel-ratio.integration.cpp src/mbgl/shaders/program_parameters.cpp src/mbgl/util/string.cpp \
	-o /native/uniform-pixel-ratio.integration \
	&& /native/uniform-pixel-ratio.integration
RUN <<'SH'
set -eu
node --input-type=module <<'JS'
import { mkdirSync, writeFileSync } from 'node:fs';
if (process.platform !== 'linux' || process.arch !== 'x64' || process.versions.modules !== '137') {
	throw new Error('The qualified native renderer requires Linux x64 and Node ABI 137.');
}
mkdirSync('build/headers/node', { recursive: true });
const columns = [process.version, '-', '-', '-', '-', '-', '-', '-', process.versions.modules, '-', '-'];
writeFileSync('build/headers/node/index.tab', 'version\tdate\tfiles\tnpm\tv8\tuv\tzlib\topenssl\tmodules\tlts\tsecurity\n' + columns.join('\t') + '\n');
JS
cmake --preset linux-opengl-node -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER_LAUNCHER= -DBUILD_TESTING=OFF
cmake --build build --target mbgl-node.abi-137 --parallel 2
xvfb-run -a node --test platform/node/test/cataqui/static-placement.test.cjs
SH
