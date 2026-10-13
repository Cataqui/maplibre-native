# Cataquí renderer regressions

This fork retains the Native 6.4.1 base and adds Node GeoJSON updates, static placement preparation and drawing, and OpenGL runtime pixel density.

After compiling the Linux OpenGL Node addon, run:

```sh
xvfb-run -a node --test platform/node/test/cataqui/static-placement.test.cjs
c++ -std=c++20 -O2 -DMLN_RENDER_BACKEND_OPENGL=1 -Iinclude -Ivendor/rapidjson/include test/cataqui/uniform-pixel-ratio.integration.cpp src/mbgl/shaders/program_parameters.cpp src/mbgl/util/string.cpp -o /tmp/uniform-pixel-ratio.integration
/tmp/uniform-pixel-ratio.integration
```

Cataquí's backend separately qualifies the compiled addon with its production rendering service, fractional-density pixel checks, cancellation, resource failures and retained scenes. These application tests remain in that repository.
