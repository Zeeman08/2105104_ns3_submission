#!/bin/bash
BASELINE="cerl-baseline"
ERROR_RATES=(0 0.01 0.05 0.07 0.2)
OUTPUT_DIR="cerl-results-NEW"

for ERROR_RATE in "${ERROR_RATES[@]}"; do
    ./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --errorRate=$ERROR_RATE"
    ./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --errorRate=$ERROR_RATE"
    ./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --errorRate=$ERROR_RATE"
    ./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --errorRate=$ERROR_RATE"
    python3 plot_baseline.py --results-dir $OUTPUT_DIR --error-rate $ERROR_RATE
done
