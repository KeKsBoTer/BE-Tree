# ---
# jupyter:
#   jupytext:
#     formats: ipynb,py:light
#     text_representation:
#       extension: .py
#       format_name: light
#       format_version: '1.5'
#       jupytext_version: 1.13.1
#   kernelspec:
#     display_name: Python 3 (ipykernel)
#     language: python
#     name: python3
# ---

from matplotlib import pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns
sns.set()


def ratio_for_id(state_id):
    df_heartbeats = pd.read_csv(
        f"measurements/heartbeat_{state_id}.log", delimiter=" ", skipinitialspace=True, index_col="Beat")
    df_heartbeats = df_heartbeats.groupby("Tag").mean()
    rate = df_heartbeats["Global_Rate"].mean()
    energy = df_heartbeats["Global_Power"].mean()
    return {"heartrate":rate,"power":energy}


import glob
num_states = len(glob.glob("measurements/heartbeat_*.log"))
results = [ratio_for_id(i) for i in range(num_states)]
len(results)

# +
from utils import system_cpu_config

config = system_cpu_config()

# +
import pandas as pd

areas = {}

for cores, g in pd.DataFrame(config,dtype=int).T.groupby("cores"):
    areas[cores] = [g.index.astype("int").min(),g.index.astype("int").max()]

areas

# +
fig, ax1 = plt.subplots(figsize=(8, 5))

for cores, a in areas.items():
    ax1.axvspan(a[0], a[1], color=f"C{int(cores)+2}", alpha=0.2)
    ax1.text((a[0]+a[1])/2, 6.5, f"{int(cores)+1} Core(s)",
             color=f"C{int(cores)+2}", horizontalalignment="center", fontsize=10)
for n in range(4):
    ax1.plot([i for i in range(num_states) if config[i]["cores"] == n],
             [results[i]["heartrate"]/results[0]["heartrate"]
                 for i in range(num_states) if config[i]["cores"] == n],
             "--o", label="Throughput" if n==0 else None, color="C0")

ax1.set_xticks(range(num_states)[::4])

for n in range(4):
    ax1.plot([i for i in range(num_states) if config[i]["cores"] == n],
             [results[i]["power"]/results[0]["power"]
                 for i in range(num_states) if config[i]["cores"] == n],
             "--o", label="Energy" if n==0 else None, color="C1")

ax1.set_xlabel('State Number')
ax1.legend()
fig.savefig("figures/cpu_states.png", dpi=300)
fig.savefig("figures/cpu_states.pdf", dpi=300)


# +
import csv

with open("config/control_config","w") as f:
    writer = csv.DictWriter(f,["#id","Speedup","Power"],delimiter="\t")
    writer.writeheader()
    for i,r in enumerate(results):
        writer.writerow({
            "#id":i,
            "Speedup":round(r["heartrate"] / results[0]["heartrate"],6),
            "Power":round(r["power"] / results[0]["power"],6)
        })

# -


