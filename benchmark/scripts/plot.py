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

# +

from matplotlib import pyplot as plt
import numpy as np
import pandas as pd
import seaborn as sns
sns.set()

# +

df_poet = pd.read_csv("experiments/with_poet/poet.log", delimiter=" ",
                      skipinitialspace=True).groupby("HB_NUM").first()
df_heartbeats = pd.read_csv(
    "experiments/with_poet/heartbeat.log", delimiter=" ", skipinitialspace=True, index_col="Beat")
df_poet
# -

df_hb_mean = df_heartbeats.groupby("Tag").mean()
df_hb_mean

# +

df = df_poet.join(df_hb_mean)
df["Timestamp"] = pd.to_datetime(df["Timestamp"] -df["Timestamp"].iloc[0])
df = df.iloc[5:]
df

# +
fig, ax1 = plt.subplots(figsize=(20, 5))

areas = [
    [1, [0, 30]],
    [2, [30, 60]],
    [3, [60, 90]],
    [4, [90, 150]],
    [3, [150, 180]],
    [2, [180, 210]],
    [1, [210, 240]],
]

for cores, a in areas:
    x_min = pd.Timestamp(0)+pd.Timedelta(a[0], unit="sec")
    x_max = pd.Timestamp(0)+pd.Timedelta(a[1], unit="sec")
    ax1.axvspan(x_min, x_max,
                color=f"C{int(cores)+2}", alpha=0.5)
    y = (df["Global_Rate"].max()+df["Global_Rate"].min())/2
    ax1.text(pd.Timestamp(0)+pd.Timedelta((a[0]+a[1])/2, unit="sec"),
             y, f"{cores} thread(s)",
             color=f"C{int(cores)+2}", horizontalalignment="center")

ax1.plot([df["Timestamp"].min(),df["Timestamp"].max()],[190,190],color="green")

ax1.plot(df["Timestamp"], df["Global_Rate"], label="Global_Rate", color="C0")
ax1.tick_params(axis='y', labelcolor="C0")
ax1.set_ylabel('Global Rate', color="C0")
ax1.set_xlabel('Time', color="black")

ax2 = ax1.twinx()
ax2.plot(df["Timestamp"], df["Global_Power"], label="Global_Power", color="C1")
ax2.tick_params(axis='y', labelcolor="C1")
ax2.set_ylabel('Global Power', color="C1")

fig.savefig("figures/benchmark.png",dpi=300)

# -

fig, ax1 = plt.subplots(figsize=(20, 5)) 
plt.plot(df["Timestamp"], df["SPEEDUP"].ewm(span=100).mean(), label="SPEEDUP")


