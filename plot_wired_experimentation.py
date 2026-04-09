import pandas as pd
import matplotlib.pyplot as plt
import os
import sys

# Define the path to your results and baseline values
CSV_FILE = "wired-results/master_results.csv"
BASELINES = {
    'nodes': 40,
    'flows': 20,
    'pps': 300
}

def main():
    if not os.path.exists(CSV_FILE):
        print(f"Error: Could not find {CSV_FILE}. Run your simulations first.")
        sys.exit(1)

    # Load the data
    df = pd.read_csv(CSV_FILE)
    
    # Normalize protocol names to lowercase for easier matching
    df['Protocol'] = df['Protocol'].str.lower()

    # 1. Get user input for protocols
    print("Available protocols in data:", df['Protocol'].unique().tolist())
    # proto_input = input("Enter the protocols you want to plot (comma-separated, e.g., cerl, newreno): ")
    proto_input = "cerl,acerl,mcerl,newreno"
    selected_protos = [p.strip().lower() for p in proto_input.split(',')]

    # Filter dataframe by selected protocols
    df = df[df['Protocol'].isin(selected_protos)]
    if df.empty:
        print("No data found for the selected protocols.")
        sys.exit(1)

    # 2. Get user input for the varying parameter
    print("\nAvailable parameters to vary: nodes, flows, pps")
    # var_param = input("Enter the parameter you want to vary on the X-axis: ").strip().lower()

    for var_param in ['nodes', 'flows', 'pps']:
        # 3. Filter data to isolate the variation and gather baseline strings
        baseline_text_parts = []
        
        if var_param == 'nodes':
            filtered_df = df[(df['Flows'] == BASELINES['flows']) & (df['PPS'] == BASELINES['pps'])]
            x_col = 'Nodes'
            x_label = 'Number of Nodes'
        elif var_param == 'flows':
            filtered_df = df[(df['Nodes'] == BASELINES['nodes']) & (df['PPS'] == BASELINES['pps'])]
            x_col = 'Flows'
            x_label = 'Number of Flows'
        elif var_param == 'pps':
            filtered_df = df[(df['Nodes'] == BASELINES['nodes']) & (df['Flows'] == BASELINES['flows'])]
            x_col = 'PPS'
            x_label = 'Packets Per Second (per flow)'

        # Generate the subtitle string for the non-varying parameters
        for k, v in BASELINES.items():
            if k != var_param:
                # Format nicely, e.g., "Flows: 20"
                if k == 'pps':
                    baseline_text_parts.append(f"PPS: {v}")
                else:
                    baseline_text_parts.append(f"{k.capitalize()}: {v}")
                
        baseline_str = " | ".join(baseline_text_parts)

        if filtered_df.empty:
            print(f"No baseline data found to plot {var_param} variation. Check your CSV.")
            sys.exit(1)

        # Ensure data is sorted by the X-axis so the lines draw correctly
        filtered_df = filtered_df.sort_values(by=x_col)

        # 4. Plotting the 2x2 grid
        fig, axs = plt.subplots(2, 2, figsize=(14, 10))
        
        # Add the main title and the baseline settings subtitle
        fig.suptitle(f'Impact of Varying {x_col}', fontsize=16, fontweight='bold')
        fig.text(0.5, 0.93, f'Baseline Constants: [{baseline_str}]', ha='center', fontsize=13, color='dimgray')

        metrics = [
            ('Throughput_Mbps', 'Network Throughput (Mbps)', axs[0, 0]),
            ('AvgDelay_ms', 'End-to-End Delay (ms)', axs[0, 1]),
            ('PDR', 'Packet Delivery Ratio', axs[1, 0]),
            ('DropRatio', 'Packet Drop Ratio', axs[1, 1])
        ]

        # Assign markers to easily distinguish lines
        markers = ['o', 's', '^', 'd', 'x']

        for i, (metric_col, ylabel, ax) in enumerate(metrics):
            for j, proto in enumerate(selected_protos):
                proto_data = filtered_df[filtered_df['Protocol'] == proto]
                if not proto_data.empty:
                    ax.plot(proto_data[x_col], proto_data[metric_col], 
                            label=proto.upper(), 
                            marker=markers[j % len(markers)], 
                            linewidth=2, markersize=8)
            
            ax.set_xlabel(x_label, fontsize=12)
            ax.set_ylabel(ylabel, fontsize=12)
            ax.set_title(ylabel, fontsize=14)
            ax.grid(True, linestyle='--', alpha=0.7)
            ax.legend()

        plt.tight_layout()
        # Adjust top margin to ensure the title and subtitle don't overlap the subplots
        plt.subplots_adjust(top=0.88) 
        
        # Save the figure automatically before showing
        plot_filename = f"wired_{var_param}_variation.png"
        plt.savefig(plot_filename, dpi=300)
        print(f"\nPlot saved as {plot_filename}")
        
        plt.show()


if __name__ == "__main__":
    main()