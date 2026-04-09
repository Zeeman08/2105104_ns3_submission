"""
plot_timeseries.py — Vertically aligned throughput + cwnd comparison
                     for all four TCP variants: NewReno, CERL, A-CERL, M-CERL

Usage:
    python3 plot_timeseries.py --results-dir cerl-results --error-rate 0.05 --jitter 2.0
"""
import argparse, os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec

PROTOCOLS = {
    "newreno": "TCP NewReno",
    "cerl":    "TCP CERL (Baseline)",
    "acerl":   "TCP A-CERL (Adaptive)",
    "mcerl":   "TCP M-CERL (Mobility)",
}

COLORS = {
    "newreno": "#E74C3C",   # red
    "cerl":    "#2ECC71",   # green
    "acerl":   "#3498DB",   # blue
    "mcerl":   "#F39C12",   # orange
}
MSS = 1460  # bytes per segment


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results-dir", default="cerl-results")
    parser.add_argument("--error-rate",  default="0.01", help="Random packet loss rate (e.g., 0.05 for 5%)")
    parser.add_argument("--jitter",      help="Jitter UDP background traffic in Mbps")
    args = parser.parse_args()

    erate = float(args.error_rate)
    jitter = int(args.jitter)

    # 2 rows: throughput (top), cwnd (bottom)
    fig = plt.figure(figsize=(14, 9))
    gs  = gridspec.GridSpec(2, 1, height_ratios=[1, 1], hspace=0.3)
    ax_tput = fig.add_subplot(gs[0])
    ax_cwnd = fig.add_subplot(gs[1])

    for proto, label in PROTOCOLS.items():
        color = COLORS[proto]

        # ── Throughput ────────────────────────────────────────────────────────
        tpath = os.path.join(args.results_dir, proto, "throughput.csv")
        if os.path.exists(tpath):
            df = pd.read_csv(tpath)
            # Smooth the throughput to make the lines readable
            smoothed = df["throughput_mbps"].rolling(5, min_periods=1).mean()
            ax_tput.plot(df["time_s"], smoothed,
                         label=label, color=color, linewidth=2.2)
        else:
            print(f"  WARNING: {tpath} not found — skipping {proto}")

        # ── Congestion Window ─────────────────────────────────────────────────
        cpath = os.path.join(args.results_dir, proto, "cwnd.csv")
        if os.path.exists(cpath):
            df = pd.read_csv(cpath)
            df["cwnd_segments"] = df["cwnd_new_bytes"] / MSS
            ax_cwnd.plot(df["time_s"], df["cwnd_segments"],
                         label=label, color=color, linewidth=1.5, alpha=0.9)
        else:
            print(f"  WARNING: {cpath} not found — skipping {proto}")

    # ── Dynamic Title Logic ───────────────────────────────────────────────────
    title_base = "NewReno vs CERL vs A-CERL vs M-CERL"
    # if jitter > 0.0:
    title_str = f"{title_base} — {erate*100:.1f}% Loss, {jitter} Mbps Jitter"
    annotation_text = "A-CERL adapts to RTT jitter; Base CERL misinterprets it as congestion."
    file_suffix = f"jitter_{jitter}mbps"
    # else:
    #     title_str = f"{title_base} — {erate*100:.1f}% Random Packet Loss"
    #     annotation_text = "CERL variants preserve cwnd on random loss; NewReno always halves."
    #     file_suffix = f"loss_{int(erate*100)}pct"

    # ── Format throughput panel ───────────────────────────────────────────────
    ax_tput.axhline(y=5.0, color="gray", ls=":", lw=1.2,
                    label="Link capacity (5 Mbps)")
    ax_tput.set_ylabel("Throughput (Mbps)", fontsize=12, fontweight="bold")
    ax_tput.set_title(title_str, fontsize=14, fontweight="bold", pad=15)
    ax_tput.legend(fontsize=10, loc="lower right")
    ax_tput.grid(True, alpha=0.3)
    ax_tput.set_ylim(0, 5.5)
    ax_tput.set_xlim(0, 30)
    ax_tput.tick_params(labelbottom=False)

    # ── Format cwnd panel ─────────────────────────────────────────────────────
    ax_cwnd.set_ylabel("Congestion Window\n(segments)", fontsize=12,
                       fontweight="bold")
    ax_cwnd.set_xlabel("Simulation Time (s)", fontsize=12, fontweight="bold")
    ax_cwnd.legend(fontsize=10, loc="upper right")
    ax_cwnd.grid(True, alpha=0.3)
    ax_cwnd.set_ylim(bottom=0)
    ax_cwnd.set_xlim(0, 30)

    # Add the contextual text box
    ax_cwnd.text(0.02, 0.97, annotation_text,
                 transform=ax_cwnd.transAxes, fontsize=10,
                 verticalalignment="top",
                 bbox=dict(boxstyle="round", facecolor="wheat", alpha=0.3))

    # Save output
    out = os.path.join(args.results_dir, f"comparison_{file_suffix}.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"\n  Plot saved: {out}\n")


if __name__ == "__main__":
    main()