#!/usr/bin/env python3
"""Parse runtime PERF_US / BLIT_US logs and report frame-drop hotspots.

Examples:
  python3 tools/perf_drop_report.py Tests/uts.log
  python3 tools/perf_drop_report.py Tests/uts.log --fps-threshold 20 --window 30
"""

from __future__ import annotations

import argparse
import math
import re
import statistics
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional

PERF_KV_RE = re.compile(r"([a-zA-Z_]+)=(-?\d+(?:\.\d+)?)")

PERF_MOVE_RE = re.compile(r"([a-zA-Z_]+)=(-?\d+(?:\.\d+)?)")

BLIT_RE = re.compile(
    r"\[BLIT_US\]\s+"
    r"frame=(?P<frame>\d+)\s+"
    r"layer=(?P<layer>\d+)\s+"
    r"lines=(?P<lines>\d+)\s+"
    r"panel_dma=(?P<panel_dma>\d+)\s+\|\s+"
    r"calls=(?P<calls>\d+)\s+"
    r"uploads=(?P<uploads>\d+)\s+"
    r"up=(?P<up>\d+)\s+"
    r"draw=(?P<draw>\d+)"
)


@dataclass
class PerfSample:
    line_no: int
    move: int
    smoke: int
    draw: int
    flip: int
    sync: int
    total: int
    fps: float
    tick_ms: int
    frame: int
    render: int
    rdiv: int
    fixed: int
    stalled: int
    status: int
    dbuf_off: int


@dataclass
class PerfMoveSample:
    line_no: int
    frames: int
    titleMenu: int
    gameOver: int
    stageClear: int
    pause: int
    background: int
    addBullets: int
    shots: int
    ship: int
    foes: int
    frags: int
    bonuses: int


@dataclass
class BlitSample:
    line_no: int
    frame: int
    layer: int
    lines: int
    panel_dma: int
    calls: int
    uploads: int
    up: int
    draw: int


def percentile(values: List[float], p: float) -> float:
    if not values:
        return math.nan
    if len(values) == 1:
        return values[0]
    ordered = sorted(values)
    idx = (len(ordered) - 1) * p
    lo = int(math.floor(idx))
    hi = int(math.ceil(idx))
    if lo == hi:
        return ordered[lo]
    w = idx - lo
    return ordered[lo] * (1.0 - w) + ordered[hi] * w


def parse_log(path: Path) -> tuple[List[PerfSample], List[BlitSample], Dict[int, PerfMoveSample]]:
    perf_samples: List[PerfSample] = []
    blit_samples: List[BlitSample] = []
    perf_move_map: Dict[int, PerfMoveSample] = {}  # Maps PERF line_no to its PERF_MOVE sample

    with path.open("r", encoding="utf-8", errors="replace") as fh:
        for idx, line in enumerate(fh, start=1):
            if "[PERF_US]" in line:
                kv = {k: v for k, v in PERF_KV_RE.findall(line)}
                required = ("move", "smoke", "draw", "flip", "sync", "total", "fps")
                if all(key in kv for key in required):
                    perf_samples.append(
                        PerfSample(
                            line_no=idx,
                            move=int(float(kv["move"])),
                            smoke=int(float(kv["smoke"])),
                            draw=int(float(kv["draw"])),
                            flip=int(float(kv["flip"])),
                            sync=int(float(kv["sync"])),
                            total=int(float(kv["total"])),
                            fps=float(kv["fps"]),
                            tick_ms=int(float(kv.get("tick_ms", "0"))),
                            frame=int(float(kv.get("frame", "-1"))),
                            render=int(float(kv.get("render", "-1"))),
                            rdiv=int(float(kv.get("rdiv", "-1"))),
                            fixed=int(float(kv.get("fixed", "-1"))),
                            stalled=int(float(kv.get("stalled", "-1"))),
                            status=int(float(kv.get("status", "-1"))),
                            dbuf_off=int(float(kv.get("dbuf_off", "-1"))),
                        )
                    )
                continue

            if "[PERF_MOVE]" in line:
                kv = {k: v for k, v in PERF_MOVE_RE.findall(line)}

                def kv_int(*keys: str, default: int = 0) -> int:
                    for key in keys:
                        if key in kv:
                            return int(float(kv[key]))
                    return default

                if "frames" in kv or "f" in kv:
                    perf_move_map[idx] = PerfMoveSample(
                        line_no=idx,
                        frames=kv_int("frames", "f"),
                        titleMenu=kv_int("titleMenu", "t"),
                        gameOver=kv_int("gameover", "gameOver", "go"),
                        stageClear=kv_int("stageClear", "sc"),
                        pause=kv_int("pause", "p"),
                        background=kv_int("background", "bg"),
                        addBullets=kv_int("addBullets", "add"),
                        shots=kv_int("shots", "sh"),
                        ship=kv_int("ship"),
                        foes=kv_int("foes"),
                        frags=kv_int("frags", "fr"),
                        bonuses=kv_int("bonuses", "bn"),
                    )
                continue

            blit = BLIT_RE.search(line)
            if blit:
                blit_samples.append(
                    BlitSample(
                        line_no=idx,
                        frame=int(blit.group("frame")),
                        layer=int(blit.group("layer")),
                        lines=int(blit.group("lines")),
                        panel_dma=int(blit.group("panel_dma")),
                        calls=int(blit.group("calls")),
                        uploads=int(blit.group("uploads")),
                        up=int(blit.group("up")),
                        draw=int(blit.group("draw")),
                    )
                )

    return perf_samples, blit_samples, perf_move_map


def dominant_component(sample: PerfSample) -> str:
    parts = {
        "move": sample.move,
        "draw": sample.draw,
        "flip": sample.flip,
        "sync": sample.sync,
        "smoke": sample.smoke,
    }
    return max(parts, key=parts.get)


def summarize_window(samples: List[PerfSample], center: int, radius: int) -> Dict[str, float]:
    lo = max(0, center - radius)
    hi = min(len(samples), center + radius + 1)
    window = samples[lo:hi]
    result: Dict[str, float] = {
        "count": float(len(window)),
        "avg_total": 0.0,
        "avg_fps": 0.0,
        "avg_move": 0.0,
        "avg_draw": 0.0,
        "avg_flip": 0.0,
        "avg_sync": 0.0,
    }
    if not window:
        return result

    inv = 1.0 / float(len(window))
    result["avg_total"] = sum(s.total for s in window) * inv
    result["avg_fps"] = sum(s.fps for s in window) * inv
    result["avg_move"] = sum(s.move for s in window) * inv
    result["avg_draw"] = sum(s.draw for s in window) * inv
    result["avg_flip"] = sum(s.flip for s in window) * inv
    result["avg_sync"] = sum(s.sync for s in window) * inv
    return result


def print_report(
    perf_samples: List[PerfSample],
    blit_samples: List[BlitSample],
    perf_move_map: Dict[int, PerfMoveSample],
    fps_threshold: float,
    total_threshold: int,
    window_radius: int,
    top: int,
) -> None:
    if not perf_samples:
        print("No [PERF_US] entries found. Ensure runtime profiling logs are enabled.")
        if blit_samples:
            print()
            print("BLIT Hotspots")
            print("=============")
            worst_blit = sorted(blit_samples, key=lambda b: (b.layer + b.lines + b.panel_dma), reverse=True)[:top]
            for rank, b in enumerate(worst_blit, start=1):
                total = b.layer + b.lines + b.panel_dma
                print(
                    f"#{rank}: log_line={b.line_no} frame={b.frame} blit_total_us={total} "
                    f"layer={b.layer} lines={b.lines} panel_dma={b.panel_dma} calls={b.calls} uploads={b.uploads}"
                )
        return

    fps_values = [s.fps for s in perf_samples]
    frame_us_values = [float(s.total) for s in perf_samples]

    print("Performance Summary")
    print("===================")
    print(f"PERF samples: {len(perf_samples)}")
    print(f"BLIT samples: {len(blit_samples)}")
    print(f"PERF_MOVE samples: {len(perf_move_map)}")
    print(f"FPS avg/p50/p90/p99: {statistics.fmean(fps_values):.2f} / {percentile(fps_values, 0.5):.2f} / {percentile(fps_values, 0.9):.2f} / {percentile(fps_values, 0.99):.2f}")
    print(f"Frame us avg/p50/p90/p99: {statistics.fmean(frame_us_values):.1f} / {percentile(frame_us_values, 0.5):.1f} / {percentile(frame_us_values, 0.9):.1f} / {percentile(frame_us_values, 0.99):.1f}")

    drops: List[tuple[int, PerfSample]] = []
    for i, sample in enumerate(perf_samples):
        if sample.fps < fps_threshold or sample.total > total_threshold:
            drops.append((i, sample))

    print()
    print("Drop Detection")
    print("==============")
    print(f"Thresholds: fps < {fps_threshold:.2f} or total_us > {total_threshold}")
    print(f"Detected drops: {len(drops)}")

    if not drops:
        return

    print()
    print(f"Top {min(top, len(drops))} worst samples")
    print("--------------------------")
    worst = sorted(drops, key=lambda x: (x[1].fps, -x[1].total))[:top]
    for rank, (idx, sample) in enumerate(worst, start=1):
        win = summarize_window(perf_samples, idx, window_radius)
        print(
            f"#{rank}: log_line={sample.line_no} fps={sample.fps:.2f} total_us={sample.total} "
            f"dominant={dominant_component(sample)} status={sample.status} frame={sample.frame}"
        )
        print(
            f"    window(+/-{window_radius}): avg_fps={win['avg_fps']:.2f} avg_total={win['avg_total']:.1f} "
            f"avg_move={win['avg_move']:.1f} avg_draw={win['avg_draw']:.1f} avg_flip={win['avg_flip']:.1f} avg_sync={win['avg_sync']:.1f}"
        )
        
        # Find corresponding PERF_MOVE sample (nearest line after PERF line)
        for move_line, move_sample in sorted(perf_move_map.items()):
            if move_line >= sample.line_no:
                print(
                    f"    [PERF_MOVE] frames={move_sample.frames} foes={move_sample.foes} shots={move_sample.shots} ship={move_sample.ship} "
                    f"background={move_sample.background} bonuses={move_sample.bonuses} frags={move_sample.frags} "
                    f"titleMenu={move_sample.titleMenu} gameover={move_sample.gameOver} stageClear={move_sample.stageClear} pause={move_sample.pause} "
                    f"addBullets={move_sample.addBullets}"
                )
                break

    if blit_samples:
        print()
        print("BLIT Hotspots")
        print("=============")
        worst_blit = sorted(blit_samples, key=lambda b: (b.layer + b.lines + b.panel_dma), reverse=True)[:top]
        for rank, b in enumerate(worst_blit, start=1):
            total = b.layer + b.lines + b.panel_dma
            print(
                f"#{rank}: log_line={b.line_no} frame={b.frame} blit_total_us={total} "
                f"layer={b.layer} lines={b.lines} panel_dma={b.panel_dma} calls={b.calls} uploads={b.uploads}"
            )


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze PERF_US / BLIT_US logs and detect frame drops.")
    parser.add_argument("log_path", nargs="?", default="Tests/uts.log", help="Path to log file (default: Tests/uts.log)")
    parser.add_argument("--fps-threshold", type=float, default=20.0, help="Drop threshold on FPS")
    parser.add_argument("--total-threshold", type=int, default=50000, help="Drop threshold on total_us")
    parser.add_argument("--window", type=int, default=30, help="Context radius in PERF samples around each drop")
    parser.add_argument("--top", type=int, default=10, help="Top N drop / blit spikes to print")
    args = parser.parse_args()

    path = Path(args.log_path)
    if not path.exists():
        print(f"Log file not found: {path}")
        return 2

    perf_samples, blit_samples, perf_move_map = parse_log(path)
    print_report(
        perf_samples=perf_samples,
        blit_samples=blit_samples,
        perf_move_map=perf_move_map,
        fps_threshold=args.fps_threshold,
        total_threshold=args.total_threshold,
        window_radius=max(0, args.window),
        top=max(1, args.top),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
