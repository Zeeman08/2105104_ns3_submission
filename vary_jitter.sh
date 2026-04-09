#!/bin/bash

BASELINE="cerl-baseline-jitter"
OUTPUT_DIR="cerl-jitter-results-NEW"

JITTERMBPS="0"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
python3 plot_jitter.py --results-dir $OUTPUT_DIR --jitter=$JITTERMBPS

JITTERMBPS="1"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
python3 plot_jitter.py --results-dir $OUTPUT_DIR --jitter=$JITTERMBPS

JITTERMBPS="2"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
python3 plot_jitter.py --results-dir $OUTPUT_DIR --jitter=$JITTERMBPS

JITTERMBPS="3"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
python3 plot_jitter.py --results-dir $OUTPUT_DIR --jitter=$JITTERMBPS

JITTERMBPS="4"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
python3 plot_jitter.py --results-dir $OUTPUT_DIR --jitter=$JITTERMBPS

JITTERMBPS="5"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --jitterMbps=$JITTERMBPS"
python3 plot_jitter.py --results-dir $OUTPUT_DIR --jitter=$JITTERMBPS
