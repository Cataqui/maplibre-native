"""Account for every frozen upstream version without inventing missing timings."""
import argparse
import csv
import json
from pathlib import Path
from prepare_version import CAPABILITIES

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--comparison", required=True)
    parser.add_argument("--output", required=True)
    arguments = parser.parse_args()
    root = Path(__file__).parent
    inventory = json.loads((root / "inventory.json").read_text())
    comparison = json.loads(Path(arguments.comparison).read_text())
    measured = {row["sha"]: row for row in comparison["candidates"]}
    previous = CAPABILITIES
    rows = []
    for version in inventory:
        if version["requiresBuild"]:
            previous = version["sha"]
        result = measured.get(previous)
        rows.append({
            "upstreamSha": version["sha"], "title": version["title"],
            "measurementSha": previous, "requiresCompiledComparison": version["requiresBuild"],
            "independentlyMeasured": bool(version["requiresBuild"] and result and result["status"] == "measured"),
            "status": "baseline-equivalent" if previous == CAPABILITIES else result["status"] if result else "unmeasured",
            "scoreRatio": 1 if previous == CAPABILITIES else result.get("scoreRatio") if result else None,
            "verdict": "same-build-inputs" if not version["requiresBuild"] else result.get("confidenceVerdict") if result else "pending",
        })
    output = Path(arguments.output)
    output.mkdir(parents=True, exist_ok=True)
    (output / "version-coverage.json").write_text(json.dumps(rows, indent=2) + "\n")
    with open(output / "version-coverage.csv", "w") as file:
        writer = csv.DictWriter(file, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    print(json.dumps({"versions": len(rows), "unmeasured": sum(row["status"] == "unmeasured" for row in rows), "failed": sum(row["status"] == "failed" for row in rows)}))
