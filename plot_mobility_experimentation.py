import pandas as pd
import matplotlib.pyplot as plt
import os
import sys

CSV_FILE = "mobility-results/master_results_mobility.csv"
BASELINES = {
    'nodes': 40,
    'flows': 20,
    'pps': 300,
    'velocity': 10
}

def main():
    if not os.path.exists(CSV_FILE):
        print(f"Error: Could not find {CSV_FILE}.")
        sys.exit(1)

    df = pd.read_csv(CSV_FILE)
    df['Protocol'] = df['Protocol'].str.lower()

    print("Available protocols:", df['Protocol'].unique().tolist())
    # proto_input = input("Enter protocols to plot (comma-separated): ")
    proto_input = "cerl,acerl,mcerl,newreno"
    selected_protos = [p.strip().lower() for p in proto_input.split(',')]

    df = df[df['Protocol'].isin(selected_protos)]
    
    print("\nAvailable parameters to vary: nodes, flows, pps, velocity")
    # var_param = input("Enter the parameter you want to vary: ").strip().lower()
    # if var_param not in BASELINES.keys():
    #     print("Invalid parameter.")
    #     sys.exit(1)

    for var_param in ['nodes', 'flows', 'pps', 'velocity']:
        # Filter to isolate the variation
        filtered_df = df.copy()
        for key, base_val in BASELINES.items():
            if key != var_param:
                col_name = key.capitalize() if key != 'pps' else 'PPS'
                filtered_df = filtered_df[filtered_df[col_name] == base_val]

        # Map var_param to DataFrame column name
        x_col_map = {'nodes': 'Nodes', 'flows': 'Flows', 'pps': 'PPS', 'velocity': 'Velocity'}
        x_col = x_col_map[var_param]
        
        x_labels = {'nodes': 'Number of Nodes', 'flows': 'Number of Flows', 
                    'pps': 'Packets Per Second', 'velocity': 'Velocity (m/s)'}
        x_label = x_labels[var_param]

        baseline_str = " | ".join([f"{k.capitalize() if k != 'pps' else 'PPS'}: {v}" 
                                for k, v in BASELINES.items() if k != var_param])

        filtered_df = filtered_df.sort_values(by=x_col)

        # 2x3 Grid layout for 5 metrics
        fig, axs = plt.subplots(2, 3, figsize=(18, 10))
        fig.suptitle(f'Mobile Impact of Varying {x_col}', fontsize=16, fontweight='bold')
        fig.text(0.5, 0.93, f'Baseline Constants: [{baseline_str}]', ha='center', fontsize=13, color='dimgray')

        metrics = [
            ('Throughput_Mbps', 'Network Throughput (Mbps)', axs[0, 0]),
            ('AvgDelay_ms', 'End-to-End Delay (ms)', axs[0, 1]),
            ('PDR', 'Packet Delivery Ratio', axs[0, 2]),
            ('DropRatio', 'Packet Drop Ratio', axs[1, 0]),
            ('AvgEnergy_J', 'Avg Energy Consumed (Joules)', axs[1, 1])
        ]

        # Hide the 6th empty subplot
        axs[1, 2].set_visible(False)

        markers = ['o', 's', '^', 'd', 'x']

        for metric_col, ylabel, ax in metrics:
            for j, proto in enumerate(selected_protos):
                proto_data = filtered_df[filtered_df['Protocol'] == proto]
                if not proto_data.empty:
                    ax.plot(proto_data[x_col], proto_data[metric_col], 
                            label=proto.upper(), marker=markers[j % len(markers)], 
                            linewidth=2, markersize=8)
            
            ax.set_xlabel(x_label, fontsize=12)
            ax.set_ylabel(ylabel, fontsize=12)
            ax.set_title(ylabel, fontsize=14)
            ax.grid(True, linestyle='--', alpha=0.7)
            ax.legend()

        plt.tight_layout()
        plt.subplots_adjust(top=0.88) 
        
        plot_filename = f"mobile_{var_param}_variation.png"
        plt.savefig(plot_filename, dpi=300)
        print(f"\nPlot saved as {plot_filename}")
        plt.show()


if __name__ == "__main__":
    main()