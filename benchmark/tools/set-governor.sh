#!/bin/bash

[[ $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor) = ondemand ]] && new=performance
[[ $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor) = userspace ]] && new=performance
[[ $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor) = performance ]] && new=performance
[[ $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor) = powersave ]] && new=performance

echo -n "cpu governor: "
echo $new | tee /sys/devices/system/cpu/*/cpufreq/scaling_governor