#pragma once
#include <pthread.h>

// workaround for macos
// macos does not offer pthread spin locks
// so we just polyfill it with mutex

#ifndef __USE_XOPEN2K
#define pthread_spinlock_t pthread_mutex_t
#define pthread_spin_init(a, b) pthread_mutex_init(a, b)
#define pthread_spin_lock(a) pthread_mutex_lock(a)
#define pthread_spin_unlock(a) pthread_mutex_unlock(a)
#endif