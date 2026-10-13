"""Render the frozen sweep, preserving pending versions and confidence limits."""
import argparse
import json
import math
import statistics
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--comparison", required=True)
    parser.add_argument("--output", required=True)
    arguments = parser.parse_args()
    inventory = json.loads((Path(__file__).parent / "inventory.json").read_text())
    comparison = json.loads(Path(arguments.comparison).read_text())
    measured = {row["sha"]: row for row in comparison["candidates"] if row["status"] == "measured"}
    phases = ("cold", "warm", "parallel", "update", "filtered", "resize")
    rows = [(index + 1, measured[version["sha"]]) for index, version in enumerate(inventory) if version["sha"] in measured]
    if not rows:
        raise ValueError("No measured comparisons")
    x = [index for index, row in rows]
    scores = np.array([row["scoreRatio"] for index, row in rows])
    low = np.array([row["confidence95"][0] for index, row in rows])
    high = np.array([row["confidence95"][1] for index, row in rows])
    figure, axes = plt.subplots(2, 1, figsize=(13, 8), sharex=True, layout="constrained")
    axes[0].vlines(x, low, high, color="#245a86", alpha=0.7, linewidth=0.8)
    axes[0].scatter(x, scores, color="#245a86", s=9)
    axes[0].axhline(1, color="#777777", linewidth=1)
    axes[0].set_ylabel("Candidate / paired baseline time")
    axes[0].set_title(f"Cataquí native engine sweep: {len(rows)} / {sum(version['requiresBuild'] for version in inventory)} compiled versions measured")
    axes[0].text(0.01, 0.04, "Lower is faster. Bars: clustered bootstrap 95% intervals.\nBalanced workload score; independent finalist repetition required.", transform=axes[0].transAxes, fontsize=9)
    values = np.full((len(phases), len(inventory)), np.nan)
    for index, row in rows:
        for phase_index, phase in enumerate(phases):
            values[phase_index, index - 1] = math.exp(statistics.mean(math.log(cell["ratio"]) for cell in row["cells"] if cell["phase"] == phase))
    palette = plt.get_cmap("RdYlGn_r").copy()
    palette.set_bad("#e9e9e9")
    graphic = axes[1].imshow(values, aspect="auto", cmap=palette, vmin=0.8, vmax=1.2, extent=(0.5, len(inventory) + 0.5, len(phases) - 0.5, -0.5), interpolation="nearest")
    axes[1].set_yticks(range(len(phases)), phases)
    axes[1].set_xlabel("Upstream version in frozen chronological inventory (1–256)")
    axes[1].set_title("Phase ratios; gray = no independent measurement")
    figure.colorbar(graphic, ax=axes[1], label="Candidate / baseline time", shrink=0.8)
    axes[0].set_xlim(0.5, len(inventory) + 0.5)
    figure.savefig(arguments.output, dpi=180)
