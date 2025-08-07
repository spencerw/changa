#!/usr/bin/env python3

import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

filename = sys.argv[1]
columns = ['Grav', 'uDot', 'DD', 'LoadB', 'TBuild', 'Adjust', 'EAdjust', 'Kick', 'Drift', 'Cache']

def parse_timings_file(filename):
    data = []

    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('Totals:'):
                parts = line.split()
                values = list(map(float, parts[1:]))
                data.append(values)

    df = pd.DataFrame(data, columns=columns)
    df.drop(['uDot', 'EAdjust'], axis=1, inplace=True)
    return df

df = parse_timings_file(filename)

x = np.arange(len(df.columns))

fig, ax = plt.subplots(figsize=(10, 6))
width = 0.5
ax.bar(x, df[1:].mean().values, width)

# Labeling
ax.set_ylabel('Time (s)')
ax.set_xticks(x)
ax.set_xticklabels(df.columns, rotation=45)
ax.set_yscale('log')

plt.tight_layout()
plt.savefig('plot.png')
