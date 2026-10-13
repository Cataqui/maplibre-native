"""Port the three reviewed Cataqui capabilities onto an isolated upstream snapshot."""
import argparse
import re
import subprocess
from pathlib import Path

BASE = "d4bde4c93eb15650d0ddb6822d24351f792ea649"
CAPABILITIES = "20d120303397d4cce27918768ffe626863e533fb"

def prepare(repository, source):
    repository = Path(repository)
    source = Path(source)
    patch = subprocess.check_output([
        "git", "-C", str(repository), "diff", BASE, CAPABILITIES, "--", "include", "src", "shaders",
        "platform/node/src", "platform/node/index.d.ts", "platform/default",
    ], text=True)
    renamed_paths = (source / "src/mln").is_dir()
    namespace = "mln" if "namespace mln" in (source / ("src/mln" if renamed_paths else "src/mbgl") / "renderer/renderer.cpp").read_text() else "mbgl"
    if renamed_paths:
        patch = patch.replace("/mbgl/", "/mln/").replace("<mbgl/", "<mln/")
    if namespace == "mln":
        patch = patch.replace("mbgl::", "mln::").replace("namespace mbgl", "namespace mln")
    result = subprocess.run(["git", "apply", "--reject", "-"], cwd=source, input=patch, text=True, capture_output=True)
    rejected = list(source.rglob("*.rej"))
    allowed = {
        f"include/{namespace}/renderer/renderer_observer.hpp",
        f"src/{namespace}/map/map_impl.cpp", f"src/{namespace}/map/map_impl.hpp",
        f"src/{namespace}/renderer/renderer.cpp",
    }
    # Namespace and directory renames occurred in separate commits.
    if not renamed_paths:
        allowed = {p.replace("/mln/", "/mbgl/") for p in allowed}
    for reject in rejected:
        relative = str(reject.relative_to(source))[:-4]
        if relative not in allowed:
            raise RuntimeError(f"Unreviewed bridge conflict: {relative}\n{result.stderr}")
        target = source / relative
        value = target.read_text()
        if relative.endswith("renderer_observer.hpp"):
            anchor = "    /// Final frame\n"
            assert value.count(anchor) == 1
            value = value.replace(anchor, "    virtual void onDidFinishPreparingFrame() {}\n\n" + anchor)
        elif relative.endswith("map_impl.hpp"):
            anchor = "    void onWillStartRenderingMap() final;"
            assert value.count(anchor) == 1
            value = value.replace(anchor, "    void onDidFinishPreparingFrame() final;\n" + anchor)
        elif relative.endswith("map_impl.cpp"):
            anchor = "void Map::Impl::onWillStartRenderingMap() {"
            assert value.count(anchor) == 1
            value = value.replace(anchor, """void Map::Impl::onDidFinishPreparingFrame() {
    assert(mode == MapMode::Static);
    if (stillImageRequest) {
        const auto request = std::move(stillImageRequest);
        request->callback(nullptr);
    }
}

""" + anchor)
        elif relative.endswith("renderer.cpp"):
            anchor = "void Renderer::render(const std::shared_ptr<UpdateParameters>& updateParameters) {\n    MLN_TRACE_FUNC();\n"
            assert value.count(anchor) == 1
            if "invalidatePreparedPlacement();" not in value[value.index(anchor):value.index(anchor)+200]:
                value = value.replace(anchor, anchor + "    invalidatePreparedPlacement();\n")
            for header in ["renderer/renderer_observer.hpp", "util/exception.hpp"]:
                include = f"#include <{namespace}/{header}>\n"
                if include not in value:
                    value = include + value
        target.write_text(value)
        reject.unlink()
    renderer = source / ("src/mln" if renamed_paths else "src/mbgl") / "renderer/renderer.cpp"
    value = renderer.read_text()
    if "util/error_sink.hpp" in value:
        value = value.replace("void Renderer::preparePlacement(const std::shared_ptr<UpdateParameters>& updateParameters) {\n    MLN_TRACE_FUNC();", "void Renderer::preparePlacement(const std::shared_ptr<UpdateParameters>& updateParameters) {\n    MLN_TRACE_FUNC();\n    const ErrorScope errorScope{impl->observer};")
        value = value.replace("void Renderer::drawPrepared() {", "void Renderer::drawPrepared() {\n    const ErrorScope errorScope{impl->observer};")
        renderer.write_text(value)
    if result.returncode and not rejected:
        raise RuntimeError(result.stderr)
    subprocess.run(["git", "diff", "--check"], cwd=source, check=True, capture_output=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", required=True)
    parser.add_argument("--source", required=True)
    arguments = parser.parse_args()
    prepare(arguments.repository, arguments.source)
