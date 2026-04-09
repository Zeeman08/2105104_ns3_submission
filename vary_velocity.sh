#!/bin/bash

BASELINE="cerl-baseline-mobility"
OUTPUT_DIR="cerl-mobility-results-NEW"

VELOCITY="0"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
python3 plot_mobility.py --results-dir $OUTPUT_DIR --velocity=$VELOCITY



VELOCITY="5"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
python3 plot_mobility.py --results-dir $OUTPUT_DIR --velocity=$VELOCITY

VELOCITY="10"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
python3 plot_mobility.py --results-dir $OUTPUT_DIR --velocity=$VELOCITY

VELOCITY="15"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
python3 plot_mobility.py --results-dir $OUTPUT_DIR --velocity=$VELOCITY

VELOCITY="20"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
python3 plot_mobility.py --results-dir $OUTPUT_DIR --velocity=$VELOCITY

VELOCITY="25"
./ns3 run "$BASELINE --protocol=newreno --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=cerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=acerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
./ns3 run "$BASELINE --protocol=mcerl --outDir=$OUTPUT_DIR --velocity=$VELOCITY"
python3 plot_mobility.py --results-dir $OUTPUT_DIR --velocity=$VELOCITY
