#!/usr/bin/env python3
import csv
import statistics
from collections import defaultdict
from pathlib import Path

root = Path(__file__).resolve().parent.parent
source = root / "resultados" / "tempos.csv"
destination = root / "resultados" / "medias.csv"

groups = defaultdict(list)
with source.open(newline="", encoding="utf-8") as stream:
    for row in csv.DictReader(stream):
        key = (int(row["N"]), int(row["Np"]), int(row["Nc"]), int(row["M"]))
        groups[key].append(float(row["seconds"]))

with destination.open("w", newline="", encoding="utf-8") as stream:
    writer = csv.writer(stream)
    writer.writerow(["N", "Np", "Nc", "M", "runs", "mean_seconds", "stdev_seconds"])
    for key in sorted(groups):
        values = groups[key]
        deviation = statistics.stdev(values) if len(values) > 1 else 0.0
        writer.writerow([*key, len(values), f"{statistics.mean(values):.9f}",
                         f"{deviation:.9f}"])

print(f"Médias salvas em {destination}")

