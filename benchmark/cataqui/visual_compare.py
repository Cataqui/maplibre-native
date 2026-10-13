"""Measure finalist pixel differences against repeated baseline images.

This supplies review evidence, not an automatic visual correctness verdict.
Run with Pillow and NumPy installed, after downloading finalist artifacts.
"""
import argparse
import itertools
import json
from pathlib import Path

import numpy as np
from PIL import Image


def _difference(left, right):
    if left.shape != right.shape:
        raise ValueError("Compared image dimensions differ")
    delta = np.abs(left.astype(np.int16) - right.astype(np.int16))
    return {
        "meanAbsoluteChannelDifference": float(delta.mean()),
        "maximumChannelDifference": int(delta.max()),
        "changedPixelFraction": float(np.any(delta != 0, axis=2).mean()),
    }


def _read(path):
    with Image.open(path) as picture:
        return np.asarray(picture.convert("RGBA"))


def _compare(directory):
    report = json.loads((directory / "report.json").read_text())
    controls = [directory / (Path(name).stem + "-frames") for name in report["controls"]]
    if any(not path.is_dir() for path in controls):
        raise ValueError("Four captured baseline control passes are required")
    filenames = {path.name for path in controls[0].glob("*.png")}
    if not filenames or any({path.name for path in control.glob("*.png")} != filenames for control in controls):
        raise ValueError("Control image sets differ or are empty")
    variation = {}
    for filename in sorted(filenames):
        pictures = [_read(control / filename) for control in controls]
        differences = [_difference(left, right) for left, right in itertools.combinations(pictures, 2)]
        variation[filename] = {key: max(row[key] for row in differences) for key in differences[0]}
    candidates = []
    for entry in report["candidates"]:
        if entry["status"] != "measured":
            candidates.append({"sha": entry["sha"], "status": entry["status"]})
            continue
        paired = {}
        for kind in ("baseline", "candidate"):
            name = next(item["file"] for item in entry["passes"] if item["kind"] == kind)
            paired[kind] = directory / (Path(name).stem + "-frames")
            if {path.name for path in paired[kind].glob("*.png")} != filenames:
                raise ValueError("Candidate image set differs from controls")
        images = []
        for filename in sorted(filenames):
            difference = _difference(_read(paired["baseline"] / filename), _read(paired["candidate"] / filename))
            images.append({
                "file": filename,
                "difference": difference,
                "baselineVariationMaximum": variation[filename],
                "exceedsObservedBaselineVariation": any(difference[key] > variation[filename][key] for key in difference),
            })
        candidates.append({"sha": entry["sha"], "status": "review-required", "images": images})
    return {"candidates": candidates, "note": "Observed baseline maxima describe these four runs only; they are not guaranteed tolerance bounds. Inspect images and semantic placement evidence before deciding correctness."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", required=True)
    parser.add_argument("--output", required=True)
    arguments = parser.parse_args()
    result = _compare(Path(arguments.results))
    Path(arguments.output).write_text(json.dumps(result, indent=2) + "\n")
