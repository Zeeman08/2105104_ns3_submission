#!/bin/bash

./vary_error.sh
./vary_jitter.sh
./vary_velocity.sh


./run_mobility.sh
python3 plot_mobility_experimentation.py
./run_wired.sh
python3 plot_wired_experimentation.py
