#include <sys/time.h>
#include <string.h>
#include <stdio.h>

void sw_init();

void sw_start();

void sw_stop();

int readDays();

int readHours();

int readMinutes();

int readSeconds();

int readmSeconds();

double readTotalSeconds();

void sw_timeString(char *buf);
