#!/bin/bash

[[ $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor) = ondemand ]] && new=userspace
[[ $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor) = userspace ]] && new=userspace
[[ $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor) = performance ]] && new=userspace
[[ $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor) = powersave ]] && new=userspace

echo -n "cpu governor: "
echo $new | tee /sys/devices/system/cpu/*/cpufreq/scaling_governor