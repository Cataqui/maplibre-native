#include <mbgl/shaders/program_parameters.hpp>

#include <array>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>

#if !defined(MLN_RENDER_BACKEND_OPENGL) || defined(MLN_RENDER_BACKEND_METAL) || \
    defined(MLN_RENDER_BACKEND_VULKAN) || defined(MLN_RENDER_BACKEND_WEBGPU)
#error This regression must use the same OpenGL-only backend as the Maps renderer.
#endif

// Compile with the actual pinned source's program_parameters.cpp, not a substitute.
// Throw rather than assert so Release builds cannot disable the regression.
int main() {
    const std::array<float, 5> densities{1.0f, 1.333333333f, 720.0f / 328.0f, 2.0f, 3.0f};
    std::set<std::uint64_t> regularFamilies;
    std::set<std::uint64_t> overdrawFamilies;
    const mbgl::ProgramParameters regularBaseline(densities.front(), false);
    const mbgl::ProgramParameters overdrawBaseline(densities.front(), true);

    for (const auto density : densities) {
        const mbgl::ProgramParameters regular(density, false);
        const mbgl::ProgramParameters overdraw(density, true);
        regularFamilies.insert(regular.getDefinesHash());
        overdrawFamilies.insert(overdraw.getDefinesHash());
        if (regular.getDefines().at("DEVICE_PIXEL_RATIO") != "u_pixel_ratio" ||
            overdraw.getDefines().at("DEVICE_PIXEL_RATIO") != "u_pixel_ratio") {
            throw std::runtime_error("Changing output density must use the actual runtime pixel-ratio uniform.");
        }
        if (regular.getDefinesString() != regularBaseline.getDefinesString() ||
            overdraw.getDefinesString() != overdrawBaseline.getDefinesString()) {
            throw std::runtime_error("Changing output density must not create another OpenGL shader-source family.");
        }
        if (regular.getOverdrawInspectorEnabled() || !overdraw.getOverdrawInspectorEnabled()) {
            throw std::runtime_error("Uniform density must preserve the independently selected overdraw mode.");
        }
    }

    if (regularFamilies.size() != 1 || overdrawFamilies.size() != 1 ||
        regularBaseline.getDefinesHash() == overdrawBaseline.getDefinesHash() ||
        regularBaseline.getDefines().count("OVERDRAW_INSPECTOR") != 0 ||
        overdrawBaseline.getDefines().count("OVERDRAW_INSPECTOR") != 1) {
        throw std::runtime_error("OpenGL shader keys must vary by overdraw mode, not by output density.");
    }

    std::cout << "Changing output density reuses one regular and one overdraw OpenGL shader family.\n";
}
