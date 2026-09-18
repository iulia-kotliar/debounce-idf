#!/usr/bin/env python3
"""Будує графіки з CSV-виводу прошивки (OUTPUT_CSV = 1).

Використання:  python tools/plot.py [data/adc_output.csv]
Результат:     images/voltage_vs_raw.png, images/error_vs_raw.png
"""
import csv
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

root = Path(__file__).resolve().parent.parent
csv_path = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "data" / "adc_output.csv"
img_dir = root / "images"
img_dir.mkdir(exist_ok=True)

raw, u_man, u_cal, err_raw, err = [], [], [], [], []
with open(csv_path, newline="") as f:
    for row in csv.DictReader(f):
        r = int(row["raw"])
        raw.append(r)
        u_man.append(float(row["u_manual_mv"]))
        u_cal.append(float(row["u_cali_mv"]))
        if row["error_pct"]:
            err_raw.append(r)
            err.append(float(row["error_pct"]))

ZONES = [  # (від RAW, до RAW, колір, підпис)
    (0, 600, "tab:red", "нелінійність (низ)"),
    (1400, 2600, "tab:green", "лінійна ділянка"),
    (3000, 4095, "tab:orange", "нелінійність (верх)"),
]

def shade(ax):
    for lo, hi, color, label in ZONES:
        ax.axvspan(lo, hi, color=color, alpha=0.08, label=label)

# --- Графік 1: напруга від RAW ---
fig, ax = plt.subplots(figsize=(9, 5.5))
shade(ax)
order = sorted(range(len(raw)), key=lambda i: raw[i])
ax.plot([raw[i] for i in order], [u_man[i] for i in order],
        color="tab:blue", lw=1.5, label="U_manual (RAW × 3100 / 4095)")
ax.scatter(raw, u_cal, s=14, color="tab:purple", zorder=3, label="U_cali (esp_adc_cali)")
ax.set_xlabel("RAW")
ax.set_ylabel("Напруга, мВ")
ax.set_title("ESP32-S3 ADC1_CH3, 12 dB: ручна формула vs калібрування")
ax.set_xlim(0, 4095)
ax.set_ylim(0)
ax.grid(alpha=0.3)
ax.legend(loc="upper left", fontsize=9)
fig.tight_layout()
fig.savefig(img_dir / "voltage_vs_raw.png", dpi=130)

# --- Графік 2: похибка від RAW ---
fig, ax = plt.subplots(figsize=(9, 5.5))
shade(ax)
ax.scatter(err_raw, err, s=14, color="tab:red", zorder=3, label="Error(%)")
ax.axhline(0, color="gray", lw=0.8)
ax.set_xlabel("RAW")
ax.set_ylabel("Похибка, %")
ax.set_title("Похибка ручної формули відносно каліброваного значення")
ax.set_xlim(0, 4095)
ax.grid(alpha=0.3)
ax.legend(loc="lower right", fontsize=9)
fig.tight_layout()
fig.savefig(img_dir / "error_vs_raw.png", dpi=130)

print("Saved:", img_dir / "voltage_vs_raw.png", img_dir / "error_vs_raw.png")
