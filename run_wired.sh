#!/bin/bash

# Ensure we're in the ns-3 root directory to run waf/ns3
# Create output directory and clear old master file if it exists
mkdir -p wired-results
rm -f wired-results/master_results.csv

PROTOCOLS=("cerl" "acerl" "mcerl" "newreno")

# Baseline constants
BASE_NODES=40
BASE_FLOWS=20
BASE_PPS=300

# Arrays for variations
VAR_NODES=(20 40 60 80 100)
VAR_FLOWS=(10 20 30 40 50)
VAR_PPS=(100 200 300 400 500)

echo "Starting Wired Topolgy Simulations..."

for proto in "${PROTOCOLS[@]}"; do
    
    # 1. Vary Nodes (Keep Flows and PPS at baseline)
    for nodes in "${VAR_NODES[@]}"; do
        # Skip 40 here so we don't run the exact baseline scenario 3 times
        if [ "$nodes" -eq 40 ]; then continue; fi
        echo "Running $proto | Nodes: $nodes | Flows: $BASE_FLOWS | PPS: $BASE_PPS"
        ./ns3 run "cerl-baseline-experimentation --protocol=$proto --nNodes=$nodes --nFlows=$BASE_FLOWS --pps=$BASE_PPS --outDir=wired-results"
    done

    # 2. Vary Flows (Keep Nodes and PPS at baseline)
    for flows in "${VAR_FLOWS[@]}"; do
        if [ "$flows" -eq 20 ]; then continue; fi
        echo "Running $proto | Nodes: $BASE_NODES | Flows: $flows | PPS: $BASE_PPS"
        ./ns3 run "cerl-baseline-experimentation --protocol=$proto --nNodes=$BASE_NODES --nFlows=$flows --pps=$BASE_PPS --outDir=wired-results"
    done

    # 3. Vary PPS (Keep Nodes and Flows at baseline)
    for pps in "${VAR_PPS[@]}"; do
        # We will run the baseline (40 nodes, 20 flows, 300 pps) here so we have it exactly once per protocol
        echo "Running $proto | Nodes: $BASE_NODES | Flows: $BASE_FLOWS | PPS: $pps"
        ./ns3 run "cerl-baseline-experimentation --protocol=$proto --nNodes=$BASE_NODES --nFlows=$BASE_FLOWS --pps=$pps --outDir=wired-results"
    done

done

echo "Simulations complete! Results saved to wired-results/master_results.csv"