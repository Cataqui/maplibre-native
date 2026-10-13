"""Build and compare fixed upstream snapshots on the same bounded Linux runner."""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time
from pathlib import Path
from prepare_version import prepare, CAPABILITIES

REPOSITORY = Path("/repository")
WORKSPACE = Path("/workspace")
SOURCE = WORKSPACE / "source"
TOOLS = REPOSITORY / "benchmark/cataqui"

def command(arguments, *, cwd=None, log=None, timeout=1800):
    if log:
        with open(log, "a") as output:
            subprocess.run(arguments, cwd=cwd, check=True, stdout=output, stderr=subprocess.STDOUT, timeout=timeout)
    else:
        subprocess.run(arguments, cwd=cwd, check=True, timeout=timeout)

def _clone_repository(repository, source):
    command(["git", "config", "--global", "--add", "safe.directory", str(repository)])
    command(["git", "config", "--global", "--add", "safe.directory", str(repository / ".git")])
    command(["git", "clone", "--shared", "--no-checkout", str(repository), str(source)])

def build(sha, output):
    log = output / f"build-{sha[:12]}.log"
    command(["git", "checkout", "--force", "--detach", sha], cwd=SOURCE, log=log)
    for reject in SOURCE.rglob("*.rej"):
        reject.unlink()
    if sha != CAPABILITIES:
        prepare(REPOSITORY, SOURCE)
    command(["git", "submodule", "update", "--init", "--recursive", "--depth", "1"], cwd=SOURCE, log=log)
    headers = SOURCE / "build/headers/node"
    headers.mkdir(parents=True, exist_ok=True)
    (headers / "index.tab").write_text("version\tdate\tfiles\tnpm\tv8\tuv\tzlib\topenssl\tmodules\tlts\tsecurity\nv24.20.0\t-\t-\t-\t-\t-\t-\t-\t137\t-\t-\n")
    command(["cmake", "--preset", "linux-opengl-node", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER_LAUNCHER=ccache", "-DBUILD_TESTING=OFF", "-DCMAKE_CXX_FLAGS=-march=x86-64", "-DCMAKE_C_FLAGS=-march=x86-64"], cwd=SOURCE, log=log)
    command(["cmake", "--build", "build", "--target", "mbgl-node.abi-137", "--parallel", "2"], cwd=SOURCE, log=log)
    engine_namespace = "mln" if (SOURCE / "include/mln").exists() else "mbgl"
    regression = (REPOSITORY / "test/cataqui/uniform-pixel-ratio.integration.cpp").read_text().replace("mbgl::", engine_namespace + "::").replace("<mbgl/", "<" + engine_namespace + "/")
    (WORKSPACE / "uniform.cpp").write_text(regression)
    command(["c++", "-std=c++20", "-O2", "-DMLN_RENDER_BACKEND_OPENGL=1", "-Iinclude", "-Ivendor/rapidjson/include", str(WORKSPACE / "uniform.cpp"), f"src/{engine_namespace}/shaders/program_parameters.cpp", f"src/{engine_namespace}/util/string.cpp", "-o", str(WORKSPACE / "uniform")], cwd=SOURCE, log=log)
    command([str(WORKSPACE / "uniform")], log=log)
    tests = SOURCE / "platform/node/test/cataqui"
    tests.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(REPOSITORY / "platform/node/test/cataqui/static-placement.test.cjs", tests / "static-placement.test.cjs")
    command(["node", "--test", "--test-timeout=30000", "platform/node/test/cataqui/static-placement.test.cjs"], cwd=SOURCE, log=log, timeout=60)
    addon = WORKSPACE / "addons" / sha
    (addon / "lib/node-v137").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(SOURCE / "platform/node/lib/node-v137/mbgl.node", addon / "lib/node-v137/mbgl.node")
    shutil.copyfile(SOURCE / "platform/node/index.js", addon / "index.js")
    (addon / "package.json").write_text('{"type":"commonjs"}\n')
    return addon

def measure(addon, output, name, blocks, frames=False):
    destination = output / f"{name}.json"
    arguments = ["node", str(TOOLS / "native.cjs"), "--addon", str(addon / "index.js"), "--fixtures", str(TOOLS / "fixtures"), "--output", str(destination), "--blocks", str(blocks)]
    if frames:
        arguments.extend(["--frames", str(output / (name + "-frames"))])
    command(arguments, log=output / "measurements.log", timeout=600)
    return str(destination.name)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--group", required=True)
    parser.add_argument("--output", default="/results")
    parser.add_argument("--blocks", type=int, default=4)
    arguments = parser.parse_args()
    output = Path(arguments.output)
    output.mkdir(parents=True, exist_ok=True)
    command(["python3", str(TOOLS / "test_run_versions.py")])
    _clone_repository(REPOSITORY, SOURCE)
    display = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "2048x4096x24", "-ac", "-nolisten", "tcp"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for attempt in range(100):
            if Path("/tmp/.X11-unix/X99").exists(): break
            time.sleep(0.05)
        else: raise RuntimeError("Xvfb did not become ready")
        baseline = build(CAPABILITIES, output)
        control_files = [measure(baseline, output, f"control-{i}", arguments.blocks) for i in range(4)]
        groups = json.loads((TOOLS / "groups.json").read_text())
        candidates = groups[arguments.group]
        report = {"baseline": CAPABILITIES, "group": arguments.group, "controls": control_files, "candidates": [], "cpu": Path("/proc/cpuinfo").read_text().split("model name", 1)[-1].split("\n", 1)[0], "node": "v24.20.0", "limits": {"cpus": 2, "memoryGiB": 8}, "blocksPerPass": arguments.blocks}
        for sha in candidates:
            entry = {"sha": sha}
            print("Building candidate", sha, flush=True)
            try:
                addon = build(sha, output)
                entry["binarySha256"] = hashlib.sha256((addon / "lib/node-v137/mbgl.node").read_bytes()).hexdigest()
                entry["passes"] = []
                for index, kind in enumerate(["baseline", "candidate", "candidate", "baseline"]):
                    file = measure(baseline if kind == "baseline" else addon, output, f"{sha[:12]}-{index}-{kind}", arguments.blocks)
                    entry["passes"].append({"kind": kind, "file": file})
                entry["status"] = "measured"
            except Exception as error:
                entry["status"] = "failed"
                entry["error"] = str(error)
            report["candidates"].append(entry)
            (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
            print("Candidate", sha, entry["status"], flush=True)
    finally:
        display.terminate()
