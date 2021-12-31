/**
 * An interface for a monitor-like readers and writers locking mechanism
 *
 * aka "rwlock"
 *
 * It lets you enter and exit a monitor in both reader and writer style. Useful
 * for guarding objects from which you may want to read or write (duh...).
 *
 * Works in the classic way apart from from dealing with potential spurious
 * wakeups and possible starvation by storing additional info.
 */

#ifndef _RW_H_
#define _RW_H_

#include <stddef.h>
#include <pthread.h>

/** Using this structure to represent a r&w lock, using mutices and conds. */
typedef struct Monitor {
  pthread_mutex_t mutex;
  pthread_cond_t readers;
  pthread_cond_t writers;
  size_t rwait;
  size_t wwait;
  size_t rcount;
  size_t wcount;
  /* these two will help us with spurious wakeups and broadcasting (I hope) */
  size_t wwoken;
  size_t rwoken;
} Monitor;

/* All functions return an error code that is 0 in case of success or some errno
 * value otherwise. Usually it's what pthread_* functions returned. */

/** Initialise a monitor. */
int monit_init(Monitor* mon);

/** Destroy a monitor. */
int monit_destroy(Monitor* mon);

/** Lock the monitor as a writer. Now no-one will be granted acess to it. */
int writer_entry(Monitor* mon);

/**
 * Exit the monitor as a writer. This function should be called only if
 * the monitor got previously acquired via `writer_entry` by the same thread.
 */
int writer_exit(Monitor* mon);

/** Weak lock on the monitor, get reader privileges. */
int reader_entry(Monitor* mon);

/** Unlock the monitor as a reader. */
int reader_exit(Monitor* mon);

#endif  /* _RW_H_ */
