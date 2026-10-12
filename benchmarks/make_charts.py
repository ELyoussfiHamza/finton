#!/usr/bin/env python3
"""Turns the tables in benchmarks/results/ into SVG charts in benchmarks/charts/.

Run from the repository root, after benchmarks/run_all.sh :
    python3 benchmarks/make_charts.py
Standard library only.
"""
import glob
import math
import os
import statistics

RESULTS = "benchmarks/results"
CHARTS = "benchmarks/charts"

SURFACE = "#fcfcfb"
TEXT = "#0b0b0b"
TEXT_SECONDARY = "#52514e"
GRID = "#e6e5e1"
SERIES = ["#2a78d6", "#eb6834"]
SERIES_TINT = ["#c4daf4", "#f9d3c3"]
FONT = "Inter, 'Segoe UI', Helvetica, Arial, sans-serif"

W, H = 1200, 675
LEFT, RIGHT, TOP, BOTTOM = 96, 230, 150, 128


def read_table(name):
    """Returns (settings line, rows, number of runs). Each row is a dict keyed by
    column name and holds the median of every run found for that sweep."""
    columns = ["value", "rate", "mean", "p50", "p95", "p99", "avg_batch", "rejected"]
    paths = sorted(glob.glob(os.path.join(RESULTS, name + ".run*.txt")))
    runs, settings = [], ""
    for path in paths:
        lines = [l.rstrip("\n") for l in open(path) if l.strip()]
        settings = lines[0]
        runs.append([{c: float(v) for c, v in zip(columns, line.split())} for line in lines[2:]])
    rows = []
    for i in range(len(runs[0])):
        rows.append({c: statistics.median(run[i][c] for run in runs) for c in columns})
        rows[-1]["rate_min"] = min(run[i]["rate"] for run in runs)
        rows[-1]["rate_max"] = max(run[i]["rate"] for run in runs)
    return settings, rows, len(runs)


def axis_ticks(peak):
    """Round tick values from 0 to just above the peak, at most six steps."""
    magnitude = 10 ** math.floor(math.log10(peak))
    for factor in (0.1, 0.2, 0.25, 0.5, 1, 2, 2.5, 5, 10):
        step = factor * magnitude
        if math.ceil(peak / step) <= 6:
            break
    count = math.ceil(peak / step)
    return [step * i for i in range(count + 1)]


def fmt(value):
    if value >= 1000:
        return f"{value:,.0f}"
    if value >= 100:
        return f"{value:.0f}"
    return f"{value:.1f}"


def line_chart(filename, title, subtitle, x_label, y_label, x_values, series, footer, ranges=None, labels=None):
    """series : list of (name, [y values]). x is categorical, evenly spaced."""
    plot_w, plot_h = W - LEFT - RIGHT, H - TOP - BOTTOM
    peak = max(max(ys) for _, ys in series)
    if ranges:
        peak = max(peak, max(high for r in ranges for _, high in r))
    ticks = axis_ticks(peak * 1.04)
    top_value = ticks[-1]

    def x_at(i):
        return LEFT + (plot_w * i / (len(x_values) - 1) if len(x_values) > 1 else plot_w / 2)

    def y_at(v):
        return TOP + plot_h - plot_h * v / top_value

    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}" '
           f'font-family="{FONT}" role="img" aria-label="{title}">',
           f'<rect width="{W}" height="{H}" fill="{SURFACE}"/>',
           f'<text x="{LEFT}" y="62" font-size="34" font-weight="700" fill="{TEXT}">{title}</text>',
           f'<text x="{LEFT}" y="100" font-size="21" fill="{TEXT_SECONDARY}">{subtitle}</text>']

    for tick in ticks:
        y = y_at(tick)
        out.append(f'<line x1="{LEFT}" y1="{y:.1f}" x2="{LEFT + plot_w}" y2="{y:.1f}" stroke="{GRID}" stroke-width="1"/>')
        out.append(f'<text x="{LEFT - 14}" y="{y + 6:.1f}" font-size="18" text-anchor="end" fill="{TEXT_SECONDARY}">{f"{tick:,.0f}" if tick == int(tick) else f"{tick:g}"}</text>')
    for i, value in enumerate(x_values):
        out.append(f'<text x="{x_at(i):.1f}" y="{TOP + plot_h + 32}" font-size="18" text-anchor="middle" fill="{TEXT_SECONDARY}">{value}</text>')
    out.append(f'<text x="{LEFT + plot_w / 2:.1f}" y="{TOP + plot_h + 66}" font-size="19" text-anchor="middle" fill="{TEXT_SECONDARY}">{x_label}</text>')
    out.append(f'<text x="{LEFT}" y="{TOP - 18}" font-size="18" fill="{TEXT_SECONDARY}">{y_label}</text>')

    for index in range(len(series)) if ranges else []:
        for i, (low, high) in enumerate(ranges[index]):
            out.append(f'<line x1="{x_at(i):.1f}" y1="{y_at(low):.1f}" x2="{x_at(i):.1f}" y2="{y_at(high):.1f}" stroke="{SERIES_TINT[index]}" stroke-width="10" stroke-linecap="round"/>')

    ends = []
    for index, (name, ys) in enumerate(series):
        color = SERIES[index]
        points = " ".join(f"{x_at(i):.1f},{y_at(v):.1f}" for i, v in enumerate(ys))
        out.append(f'<polyline points="{points}" fill="none" stroke="{color}" stroke-width="3" stroke-linejoin="round" stroke-linecap="round"/>')
        for i, v in enumerate(ys):
            out.append(f'<circle cx="{x_at(i):.1f}" cy="{y_at(v):.1f}" r="6" fill="{color}" stroke="{SURFACE}" stroke-width="2"/>')
        for series_index, point, side in (labels or []):
            if series_index != index:
                continue
            dy = -16 if side == "above" else 32
            anchor, dx = ("start", 12) if point == 0 else ("middle", 0)
            out.append(f'<text x="{x_at(point) + dx:.1f}" y="{y_at(ys[point]) + dy:.1f}" font-size="18" font-weight="600" text-anchor="{anchor}" fill="{TEXT}">{fmt(ys[point])}</text>')
        ends.append([y_at(ys[-1]), name, fmt(ys[-1]), color])

    ends.sort()
    for i in range(1, len(ends)):
        if ends[i][0] - ends[i - 1][0] < 52:
            ends[i][0] = ends[i - 1][0] + 52
    for y, name, value, color in ends:
        x = LEFT + plot_w + 18
        out.append(f'<circle cx="{x + 6}" cy="{y - 6:.1f}" r="6" fill="{color}"/>')
        out.append(f'<text x="{x + 20}" y="{y:.1f}" font-size="19" font-weight="600" fill="{TEXT}">{value}</text>')
        if len(series) > 1:
            out.append(f'<text x="{x + 20}" y="{y + 22:.1f}" font-size="16" fill="{TEXT_SECONDARY}">{name}</text>')

    out.append(f'<text x="{LEFT}" y="{H - 20}" font-size="14" fill="{TEXT_SECONDARY}">{footer}</text>')
    out.append("</svg>")
    os.makedirs(CHARTS, exist_ok=True)
    with open(os.path.join(CHARTS, filename), "w") as f:
        f.write("\n".join(out) + "\n")
    print("wrote", os.path.join(CHARTS, filename))


MACHINE = "Intel Core i5-8265U laptop (4 cores), load generator on the same machine."


def spread(rows):
    return [(r["rate_min"], r["rate_max"]) for r in rows]


def main():
    _, with_batching, runs = read_table("mnist_load_batch8")
    _, without, _ = read_table("mnist_load_batch1")
    note = f"Points are the median of {runs} runs, shaded bars show the lowest and highest run. " + MACHINE
    line_chart("mnist_load.svg",
               "Dynamic batching pays off only under load",
               "MNIST on finton, 2 workers. Batching : up to 8 requests, waiting at most 1 ms",
               "clients sending at the same time", "requests per second",
               [f"{r['value']:.0f}" for r in with_batching],
               [("batch up to 8", [r["rate"] for r in with_batching]),
                ("no batching", [r["rate"] for r in without])],
               note, [spread(with_batching), spread(without)],
               [(1, 0, "above"), (0, 0, "below"), (1, 1, "above"), (0, 1, "below")])

    _, rows, _ = read_table("mnist_batch")
    line_chart("mnist_batch.svg",
               "A batch larger than the load can fill costs throughput",
               "MNIST on finton, 32 clients, 2 workers, 1 ms maximum delay",
               "maximum batch size", "requests per second",
               [f"{r['value']:.0f}" for r in rows],
               [("throughput", [r["rate"] for r in rows])],
               note, [spread(rows)], [(0, 0, "below"), (0, 1, "above"), (0, 3, "above"), (0, 4, "above")])

    if glob.glob(os.path.join(RESULTS, "mobilenet_threads.run*.txt")):
        _, threads, _ = read_table("mobilenet_threads")
        _, workers, _ = read_table("mobilenet_workers")
        line_chart("mobilenet_cores.svg",
                   "On a heavier model, cores matter more than batching",
                   "MobileNetV2 on finton, 16 clients. Two ways of giving the model more cores",
                   "cores given to the model", "requests per second",
                   [f"{r['value']:.0f}" for r in threads],
                   [("1 worker, N threads", [r["rate"] for r in threads]),
                    ("N workers, 1 thread", [r["rate"] for r in workers])],
                   note, [spread(threads), spread(workers)], [(0, 0, "above"), (1, 0, "below"), (0, 1, "above"), (1, 1, "below")])


TABLES = [
    ("mnist_load_batch8", "MNIST, batch up to 8 with a 1 ms delay, by number of clients", "clients"),
    ("mnist_load_batch1", "MNIST, no batching, by number of clients", "clients"),
    ("mnist_batch", "MNIST, by maximum batch size", "batch"),
    ("mnist_delay", "MNIST, by maximum delay", "delay ms"),
    ("mobilenet_batch_1w4t", "MobileNetV2, 1 worker x 4 threads, by maximum batch size", "batch"),
    ("mobilenet_batch_2w1t", "MobileNetV2, 2 workers x 1 thread, by maximum batch size", "batch"),
    ("mobilenet_threads", "MobileNetV2, 1 worker, batch 8, by threads per inference", "threads"),
    ("mobilenet_workers", "MobileNetV2, 1 thread per inference, batch 4, by number of workers", "workers"),
]


def write_summary():
    out = ["# Benchmark results", "",
           "Generated by `benchmarks/make_charts.py` from the tables in `benchmarks/results/`.",
           "Every number is the median of the runs of that sweep ; the range column shows the lowest and highest throughput seen.",
           "", MACHINE + " Linux, GCC 13, ONNX Runtime 1.22, release build.", ""]
    for name, title, first in TABLES:
        if not glob.glob(os.path.join(RESULTS, name + ".run*.txt")):
            continue
        settings, rows, runs = read_table(name)
        out += [f"## {title}", "", f"`{settings.strip()}`, {runs} runs", "",
                f"| {first} | requests/s | range | mean ms | p50 ms | p95 ms | p99 ms | average batch |",
                "|---|---|---|---|---|---|---|---|"]
        for r in rows:
            out.append(f"| {r['value']:g} | {fmt(r['rate'])} | {fmt(r['rate_min'])} to {fmt(r['rate_max'])} | {r['mean']:.2f} | "
                       f"{r['p50']:.2f} | {r['p95']:.2f} | {r['p99']:.2f} | {r['avg_batch']:.2f} |")
        out.append("")
    with open("benchmarks/RESULTS.md", "w") as f:
        f.write("\n".join(out))
    print("wrote benchmarks/RESULTS.md")


if __name__ == "__main__":
    write_summary()
    main()
