"""Summarize raw paired timings without treating noisy differences as proof."""
import argparse
import csv
import json
import math
import random
import statistics
from pathlib import Path

PRIMARY = ("cold", "warm", "parallel", "update", "filtered", "resize")

def _cells(samples):
    cells = {}
    for sample in samples:
        for phase in PRIMARY:
            if phase not in sample["phases"]:
                continue
            value = sample["phases"][phase]
            if not math.isfinite(value) or value <= 0:
                raise ValueError("Invalid measured time")
            cells.setdefault((sample["case"], phase), []).append(value)
    return cells

def _compare(reference, candidate, seed=20261013):
    a, b = _cells(reference), _cells(candidate)
    if set(a) != set(b):
        raise ValueError("Compared workloads differ")
    keys = sorted(a)
    ratios = {key: statistics.median(b[key]) / statistics.median(a[key]) for key in keys}
    score = math.exp(statistics.mean(math.log(value) for value in ratios.values()))
    rng = random.Random(seed)
    def replicas(samples):
        result = {}
        for sample in samples:
            label = sample.get("_pass", "single")
            block = sample["block"] if "block" in sample else samples.index(sample)
            result.setdefault(label, {}).setdefault(block, {})
            for phase in PRIMARY:
                if phase in sample["phases"]:
                    result[label][block][(sample["case"], phase)] = sample["phases"][phase]
        for blocks in result.values():
            if any(set(values) != set(keys) for values in blocks.values()):
                raise ValueError("Incomplete measured block")
        return result
    reference_runs, candidate_runs = replicas(reference), replicas(candidate)
    def resample(runs):
        values = {key: [] for key in keys}
        # Resample entire passes and blocks so common CPU/temperature drift
        # remains correlated across all cases and phases in each draw.
        for label in rng.choices(list(runs), k=len(runs)):
            blocks = runs[label]
            for block in rng.choices(list(blocks), k=len(blocks)):
                for key in keys:
                    values[key].append(blocks[block][key])
        return values
    bootstrap = []
    cell_bootstrap = {key: [] for key in keys}
    for repetition in range(1000):
        aa, bb = resample(reference_runs), resample(candidate_runs)
        logs = []
        for key in keys:
            ratio = statistics.median(bb[key]) / statistics.median(aa[key])
            cell_bootstrap[key].append(ratio)
            logs.append(math.log(ratio))
        bootstrap.append(math.exp(statistics.mean(logs)))
    for draws in cell_bootstrap.values():
        draws.sort()
    bootstrap.sort()
    return {"scoreRatio": score, "confidence95": [bootstrap[25], bootstrap[974]], "worstCellRatio": max(ratios.values()), "cells": [{"case": key[0], "phase": key[1], "ratio": ratios[key], "confidence95": [cell_bootstrap[key][25], cell_bootstrap[key][974]], "referenceMedianMs": statistics.median(a[key]), "candidateMedianMs": statistics.median(b[key])} for key in keys]}

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--results", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    candidates = []
    controls = []
    for report_file in Path(args.results).rglob("report.json"):
        report = json.loads(report_file.read_text())
        directory = report_file.parent
        control_runs = [json.loads((directory / name).read_text()) for name in report["controls"]]
        comparison = _compare([sample for index, run in enumerate(control_runs[:2]) for sample in [{**row, "_pass": index} for row in run["samples"]]], [sample for index, run in enumerate(control_runs[2:]) for sample in [{**row, "_pass": index} for row in run["samples"]]])
        controls.append({"group": report["group"], "cpu": report["cpu"], **comparison})
        for entry in report["candidates"]:
            result = {"sha": entry["sha"], "group": report["group"], "status": entry["status"]}
            if entry["status"] != "measured":
                result["error"] = entry.get("error")
                candidates.append(result)
                continue
            runs = [(item["kind"], json.loads((directory / item["file"]).read_text())) for item in entry["passes"]]
            reference = [{**sample, "_pass": index} for index, (kind, run) in enumerate(runs) if kind == "baseline" for sample in run["samples"]]
            candidate = [{**sample, "_pass": index} for index, (kind, run) in enumerate(runs) if kind == "candidate" for sample in run["samples"]]
            result.update(_compare(reference, candidate))
            reference_frames = next(run["frames"] for kind, run in runs if kind == "baseline")
            candidate_frames = next(run["frames"] for kind, run in runs if kind == "candidate")
            reference_index = {frame["key"]: frame for frame in reference_frames}
            result["changedFrames"] = [frame["key"] for frame in candidate_frames if frame != reference_index[frame["key"]]]
            result["peakRssRatio"] = max(sample["maxRssKiB"] for sample in candidate) / max(sample["maxRssKiB"] for sample in reference)
            result["confidenceVerdict"] = "faster" if result["confidence95"][1] < 1 else "slower" if result["confidence95"][0] > 1 else "inconclusive"
            result["visualReviewRequired"] = bool(result["changedFrames"])
            result["regressedCells"] = [cell for cell in result["cells"] if cell["confidence95"][0] > 1]
            candidates.append(result)
    candidates.sort(key=lambda row: row.get("scoreRatio", float("inf")))
    (output / "comparison.json").write_text(json.dumps({"controls": controls, "candidates": candidates, "note": "Scores are balanced geometric means across cases and phases, not production traffic-weighted request latency. Confidence intervals resample whole passes and blocks and require independent finalist repetition; they do not prove hardware equivalence. Individual cell intervals are screening evidence, not multiplicity-adjusted definitive tests."}, indent=2) + "\n")
    with open(output / "comparison.csv", "w") as file:
        writer = csv.writer(file)
        writer.writerow(["sha", "status", "score_ratio", "ci_low", "ci_high", "worst_cell_ratio", "rss_ratio", "changed_frames", "verdict"])
        for row in candidates:
            writer.writerow([row["sha"], row["status"], row.get("scoreRatio"), *row.get("confidence95", [None, None]), row.get("worstCellRatio"), row.get("peakRssRatio"), len(row.get("changedFrames", [])), row.get("confidenceVerdict")])
