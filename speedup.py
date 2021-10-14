from matplotlib import pyplot as plt
import numpy as np
import seaborn as sns

sns.set()

x = np.arange(30, step=0.1)
x2 = np.arange(1, 30, step=0.1)

plt.plot(x, x, label="linear search")
plt.plot(x, np.log(x+1), label="binary search")
plt.plot(x, 1/8*x, label="SIMD search (factor 8)")
plt.legend()
plt.show()
