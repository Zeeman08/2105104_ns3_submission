#!/bin/bash

mkdir -p mobility-results
rm -f mobility-results/master_results_mobility.csv

PROTOCOLS=("cerl" "acerl" "mcerl" "newreno")

BASE_NODES=40
BASE_FLOWS=20
BASE_PPS=300
BASE_VELOCITY=10

VAR_NODES=(20 40 60 80 100)
VAR_FLOWS=(10 20 30 40 50)
VAR_PPS=(100 200 300 400 500)
VAR_VEL=(5 10 15 20 25)

echo "Starting Mobile Topolgy Simulations..."

for proto in "${PROTOCOLS[@]}"; do
    
    # 1. Vary Nodes
    for nodes in "${VAR_NODES[@]}"; do
        if [ "$nodes" -eq 40 ]; then continue; fi
        echo "Running $proto | Nodes: $nodes | Flows: $BASE_FLOWS | PPS: $BASE_PPS | Vel: $BASE_VELOCITY"
        ./ns3 run "cerl-mobility-experimentation --protocol=$proto --nNodes=$nodes --nFlows=$BASE_FLOWS --pps=$BASE_PPS --velocity=$BASE_VELOCITY --outDir=mobility-results"
    done

    # 2. Vary Flows
    for flows in "${VAR_FLOWS[@]}"; do
        if [ "$flows" -eq 20 ]; then continue; fi
        echo "Running $proto | Nodes: $BASE_NODES | Flows: $flows | PPS: $BASE_PPS | Vel: $BASE_VELOCITY"
        ./ns3 run "cerl-mobility-experimentation --protocol=$proto --nNodes=$BASE_NODES --nFlows=$flows --pps=$BASE_PPS --velocity=$BASE_VELOCITY --outDir=mobility-results"
    done

    # 3. Vary PPS
    for pps in "${VAR_PPS[@]}"; do
        if [ "$pps" -eq 300 ]; then continue; fi
        echo "Running $proto | Nodes: $BASE_NODES | Flows: $BASE_FLOWS | PPS: $pps | Vel: $BASE_VELOCITY"
        ./ns3 run "cerl-mobility-experimentation --protocol=$proto --nNodes=$BASE_NODES --nFlows=$BASE_FLOWS --pps=$pps --velocity=$BASE_VELOCITY --outDir=mobility-results"
    done

    # 4. Vary Velocity (Run baseline here so it runs exactly once per protocol)
    for vel in "${VAR_VEL[@]}"; do
        echo "Running $proto | Nodes: $BASE_NODES | Flows: $BASE_FLOWS | PPS: $BASE_PPS | Vel: $vel"
        ./ns3 run "cerl-mobility-experimentation --protocol=$proto --nNodes=$BASE_NODES --nFlows=$BASE_FLOWS --pps=$BASE_PPS --velocity=$vel --outDir=mobility-results"
    done
done

echo "Simulations complete! Results saved to mobility-results/master_results_mobility.csv"