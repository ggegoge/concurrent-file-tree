
#include <stddef.h>
#include <pthread.h>
#include <assert.h>

#include "err.h"
#include "rw.h"

int monit_init(Monitor* mon)
{
  int err = 0;

  /* consciously using "||" operator's laziness */
  if ((err = pthread_mutex_init(&mon->mutex, 0)) ||
      (err = pthread_cond_init(&mon->readers, 0)) ||
      (err = pthread_cond_init(&mon->writers, 0)))
    return err;

  mon->rwait = mon->wwait = mon->wcount = mon->rcount = 0;
  mon->wwoken = mon->rwoken = 0;

  return 0;
}

int monit_destroy(Monitor* mon)
{
  int err = 0;

  if ((err = pthread_mutex_destroy(&mon->mutex)) ||
      (err = pthread_cond_destroy(&mon->readers)) ||
      (err = pthread_cond_destroy(&mon->writers)))
    return err;

  mon->rwait = mon->wwait = mon->wcount = mon->rcount = 0;
  return 0;
}

int writer_entry(Monitor* mon)
{
  int err = 0;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);

  if (err)
    return err;

  while (mon->rwait > 0 || mon->rcount > 0 || mon->wcount > 0 || mon->wwait > 0) {
    ++mon->wwait;
    err = pthread_cond_wait(&mon->writers, &mon->mutex);
    syserr(err, "writer_entry, cond wait");
    --mon->wwait;

    /* check if the wakeup was not spurious */
    if (mon->wwoken > 0) {
      --mon->wwoken;
      break;
    }
  }

  ++mon->wcount;
  assert(mon->wcount == 1);
  assert(mon->rcount == 0);
  err = pthread_mutex_unlock(&mon->mutex);
  syserr(err, "writer_entry, mutex unlock");

  return 0;
}

int writer_exit(Monitor* mon)
{
  int err = 0;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);

  if (err)
    return err;

  --mon->wcount;
  assert(mon->wcount == 0);
  assert(mon->rcount == 0);

  if (mon->wcount == 0 && mon->rcount == 0 && mon->rwait > 0) {
    mon->rwoken = mon->rwait;
    err = pthread_cond_broadcast(&mon->readers);
    syserr(err, "writer_exit, cond broadcast");
  } else if (mon->wcount == 0 && mon->rcount == 0 && mon->wwait > 0) {
    mon->wwoken = 1;
    err = pthread_cond_signal(&mon->writers);
    syserr(err, "writer_exit, cond signal");
  }

  err = pthread_mutex_unlock(&mon->mutex);
  syserr(err, "writer_exit, mutex unlock failed");

  return 0;
}

int reader_entry(Monitor* mon)
{
  int err = 0;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);

  if (err)
    return err;

  while (mon->wwait > 0 || mon->wcount > 0) {
    ++mon->rwait;
    err = pthread_cond_wait(&mon->readers, &mon->mutex);
    syserr(err, "reader_entry, cond wait");
    --mon->rwait;

    /* check if the wakeup was not spurious */
    if (mon->rwoken > 0) {
      --mon->rwoken;
      break;
    }
  }

  assert(mon->wcount == 0);
  ++mon->rcount;
  err = pthread_mutex_unlock(&mon->mutex);
  syserr(err, "reader_entry, mutex unlock");

  return 0;
}

int reader_exit(Monitor* mon)
{
  int err = 0;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);

  if (err)
    return err;

  --mon->rcount;
  assert(mon->wcount == 0);

  /* `&& mon->rwoken == 0`: in case multiple readers got broadcasted but before
   * all of them managed to enter some have already gotten here. They might like
   * to wake up a writer even though there are still readers to come!  therefore
   * check rwoken to see if a reader awakening isn't taking place */
  if (mon->wcount == 0 && mon->rcount == 0 && mon->wwait > 0 &&
      mon->rwoken == 0) {
    mon->wwoken = 1;
    err = pthread_cond_signal(&mon->writers);
    syserr(err, "reader_exit, cond signal");
  } else if (mon->wcount == 0 && mon->rcount == 0) {
    mon->rwoken = mon->rwait;
    err = pthread_cond_broadcast(&mon->readers);
    syserr(err, "reader_exit, cond broadcast");
  }

  err = pthread_mutex_unlock(&mon->mutex);
  syserr(err, "reader_exit, mutex unlock");

  return 0;
}
