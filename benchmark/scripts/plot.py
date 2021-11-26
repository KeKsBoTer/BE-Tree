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


def read_heartbeats_log(filename: str):
    df = pd.read_csv(filename, delimiter=" ",
                     skipinitialspace=True, index_col="Beat")
    df = df.groupby("Tag").mean()
    return df


# # Benchmark AVX2 (without POET )

# +
import numpy as np
from sklearn.linear_model import LinearRegression
from utils import system_cpu_freqs

def rate_power(folder,freq):
    df_hb = read_heartbeats_log(f"../experiments/{folder}/heartbeat_{freq}.log")
    power = df_hb["Global_Power"].mean()
    rate =  df_hb["Global_Rate"].mean()
    return rate,power

freqs = np.array(list(system_cpu_freqs()))

without_avx2_32 = np.array([rate_power("without_avx2_32",f) for f in freqs])
without_avx2_64 = np.array([rate_power("without_avx2_64",f) for f in freqs])

with_avx2_32 = np.array([rate_power("with_avx2_32",f) for f in freqs])/without_avx2_32
with_avx2_64 = np.array([rate_power("with_avx2_64",f) for f in freqs])/without_avx2_64

reg32 = LinearRegression().fit(np.array(freqs).reshape(-1,1), with_avx2_32[:,0])
reg64 = LinearRegression().fit(np.array(freqs).reshape(-1,1), with_avx2_64[:,0])

plt.figure(figsize=(9,5))
plt.plot(freqs,with_avx2_32[:,0],"--o",label="AVX2 32-bit",color="C0")
plt.plot(freqs,with_avx2_64[:,0],"--o",label="AVX2 64-bit",color="C1")

plt.plot(freqs,np.array(freqs)*reg32.coef_+reg32.intercept_,"-",color="C0")
plt.text(1.8e6,1.11,"avg. "+str(with_avx2_32[:,0].mean().round(3)),ha="center",color="C0")

plt.plot(freqs,np.array(freqs)*reg64.coef_+reg64.intercept_,"-",color="C1")
plt.text(2.3e6,1.055,"avg. "+str(with_avx2_64[:,0].mean().round(3)),ha="center",color="C1")
plt.xticks(freqs)
plt.ylabel("Speedup")
plt.xlabel("CPU Frequency (GHz)")
plt.legend()
plt.savefig("../figures/avx2_speedup.pdf",dpi=300,bbox_inches='tight')

# +
reg32 = LinearRegression().fit(np.array(freqs).reshape(-1,1), with_avx2_32[:,1])
reg64 = LinearRegression().fit(np.array(freqs).reshape(-1,1), with_avx2_64[:,1])

plt.figure(figsize=(9,5))
plt.plot(freqs,with_avx2_32[:,1],"o",label="AVX2 32-bit",color="C0")
plt.plot(freqs,with_avx2_64[:,1],"o",label="AVX2 64-bit",color="C1")

plt.plot(freqs,np.array(freqs)*reg32.coef_+reg32.intercept_,"-",color="C0")
plt.text(2.3e6,1.055,"avg. "+str(with_avx2_32[:,1].mean().round(3)),ha="center",color="C0")

plt.plot(freqs,np.array(freqs)*reg64.coef_+reg64.intercept_,"-",color="C1")
plt.text(1.8e6,1.0,"avg. "+str(with_avx2_64[:,1].mean().round(3)),ha="center",color="C1")
plt.xticks(freqs)
plt.ylabel("Powerup")
plt.xlabel("CPU Frequency (GHz)")
plt.legend()
plt.savefig("../figures/avx2_powerup.pdf",dpi=300,bbox_inches='tight')
# -

# # Benchmark with POET

# +
df_poet = pd.read_csv("../experiments/poet_no_avx2_32/poet.log", delimiter=" ",
                      skipinitialspace=True).groupby("HB_NUM").first()
df_heartbeats = read_heartbeats_log("../experiments/poet_no_avx2_32/heartbeat.log")

df_poet_avx2 = pd.read_csv("../experiments/poet_avx2_32/poet.log", delimiter=" ",
                      skipinitialspace=True).groupby("HB_NUM").first()
df_heartbeats_avx2 = read_heartbeats_log("../experiments/poet_avx2_32/heartbeat.log")

# +
df_avx2 = df_poet_avx2.join(df_heartbeats_avx2)
df_avx2["Timestamp"] = pd.to_datetime(df_avx2["Timestamp"] - df_avx2["Timestamp"].iloc[0]).iloc[5:]

df = df_poet.join(df_heartbeats)
df["Timestamp"] = pd.to_datetime(df["Timestamp"] - df["Timestamp"].iloc[0]).iloc[5:]

# +
fig, ax1 = plt.subplots(figsize=(20, 5))

ax1.plot([df["Timestamp"].min(),df["Timestamp"].max()],[180]*2,label="Target Rate",color="green")

ax1.plot(df["Timestamp"], df["Global_Rate"], label="Rate", color="C0")
ax1.plot(df_avx2["Timestamp"], df_avx2["Global_Rate"], label="Rate (AVX2)", color="C2")

ax1.tick_params(axis='y', labelcolor="C0")
ax1.set_ylabel('Rate', color="C0")
ax1.set_xlabel('Time', color="black")

ax2 = ax1.twinx()
ax2.plot(df["Timestamp"], df["Global_Power"], label="Power", color="C1")
ax2.plot(df_avx2["Timestamp"], df_avx2["Global_Power"], label="Power (AVX2)", color="C3")

ax2.tick_params(axis='y', labelcolor="C1")
ax2.set_ylabel('Power (Watt)', color="C1")
ax2.grid(False)


step=20
num_threads = [1,2,3,4,5,6,5,4,3,2,1]
areas = [[t,[i*step,(i+1)*step]] for i,t in enumerate(num_threads)]

for cores, a in areas:
    x_min = pd.Timestamp(0)+pd.Timedelta(a[0], unit="sec")
    x_max = pd.Timestamp(0)+pd.Timedelta(a[1], unit="sec")
    ax1.axvspan(x_min, x_max,
                color=f"C{int(cores)+2}", alpha=0.3)
    y = sum(ax1.get_ylim())/2
    ax1.text(pd.Timestamp(0)+pd.Timedelta((a[0]+a[1])/2, unit="sec"),
             y, f"{cores} thread(s)",
             color=f"C{int(cores)+2}", horizontalalignment="center")

fig.legend(ncol=5,loc="upper center")
fig.savefig("../figures/poet_benchmark.pdf",dpi=300,bbox_inches='tight')
# -

# # Datasets Benchmark

# +
from os import path
import glob

ds_n = []

for fn in glob.glob("../experiments/datasets_32/heartbeat_*.log"):
    parts = path.basename(fn).split(".")[0].split("_")[2:5]
    dc = dict(zip(["Size","Distribution","Ratio"],parts))
    df_h = read_heartbeats_log(fn)
    dc["Operations / Second"] = df_h["Instant_Rate"].mean() * 10000
    watt = df_h["Instant_Power"].mean()
    delta =pd.to_datetime(df_h.iloc[-1]["Timestamp"])-pd.to_datetime(df_h.iloc[0]["Timestamp"])

    joules = watt*delta.total_seconds()
    dc["Operations/Joule"] = int(dc["Operations / Second"]/joules)
    dc["Operations / Second"] = int(dc['Operations / Second'])
    ds_n.append(dc)

df_ds = pd.DataFrame(ds_n)
df_ds
# -

df_ds_n = df_ds.sort_values(["Size","Distribution","Ratio"],ascending=False).reset_index()
df_ds_n.index = list(range(1,len(df_ds)+1))
del df_ds_n["index"]
df_ds_n

print(df_ds_n.to_latex())


