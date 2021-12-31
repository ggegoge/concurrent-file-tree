
#include <stddef.h>
#include <pthread.h>

#include <assert.h>
#include <stdio.h>

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
  syserr(err, "wentr, mutex lock");
  
  while (mon->rwait > 0 || mon->rcount > 0 || mon->wcount > 0 || mon->wwait > 0) {
    printf("writer %lu goes to sleep cause rw=%lu rc=%lu wc=%lu\n",
           pthread_self(), mon->rwait, mon->rcount, mon->wcount);
    ++mon->wwait;
    err = pthread_cond_wait(&mon->writers, &mon->mutex);
    syserr(err, "wentr, cond wait");
    printf("writer %lu woke up and rw=%lu rc=%lu wc=%lu\n",
           pthread_self(), mon->rwait, mon->rcount, mon->wcount);
    --mon->wwait;

    /* check if the wakeup was not spurious */
    if (mon->wwoken > 0) {
      --mon->wwoken;
      break;
    }
  }

  ++mon->wcount;
  printf("\twentr %lu: ++wcount\n", pthread_self());
  assert(mon->wcount == 1);
  assert(mon->rcount == 0);
  err = pthread_mutex_unlock(&mon->mutex);
  syserr(err, "wentr, mutex unlock");

  return err;
}

int writer_exit(Monitor* mon)
{
  int err = 0;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);
  syserr(err, "wexit, mutex lock");
  --mon->wcount;
  assert(mon->wcount == 0);
  assert(mon->rcount == 0);

  printf("\twexit %lu: --wcount\n", pthread_self());

  if (mon->wcount == 0 && mon->rcount == 0 && mon->rwait > 0) {
    printf("writer %lu is waking up readers cause wc=%lu rc=%lu rw=%lu\n",
           pthread_self(), mon->wcount, mon->rcount, mon->rwait);
    mon->rwoken = mon->rwait;
    err = pthread_cond_broadcast(&mon->readers);
    syserr(err, "wexit, cond broadcast");
  } else if (mon->wcount == 0 && mon->rcount == 0 && mon->wwait > 0) {
    printf("writer %lu is waking up a writer cause wc=%lu rc=%lu ww=%lu\n",
           pthread_self(), mon->wcount, mon->rcount, mon->wwait);
    mon->wwoken = 1;
    err = pthread_cond_signal(&mon->writers);
    syserr(err, "wexit, cond signal");
  }

  err = pthread_mutex_unlock(&mon->mutex);

  return err;
}

int reader_entry(Monitor* mon)
{
  int err = 0;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);
  syserr(err, "rentr, mutex lock");

  while (mon->wwait > 0 || mon->wcount > 0) {
    printf("reader %lu goes to sleep cause ww=%lu wc=%lu\n",
           pthread_self(), mon->wwait, mon->wcount);
    ++mon->rwait;
    err = pthread_cond_wait(&mon->readers, &mon->mutex);
    syserr(err, "rentr, cond wait");
    printf("reader %lu woke up and ww=%lu wc=%lu\n",
           pthread_self(), mon->wwait, mon->wcount);
    --mon->rwait;

    /* check if the wakeup was not spurious */
    if (mon->rwoken > 0) {
      --mon->rwoken;
      break;
    }
  }

  printf("\trentr %lu: ++rcount\n", pthread_self());  
  assert(mon->wcount == 0);
  ++mon->rcount;
  err = pthread_mutex_unlock(&mon->mutex);
  syserr(err, "rentr, mutex unlock");

  return err;
}

int reader_exit(Monitor* mon)
{
  int err = 0;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);
  syserr(err, "rexit, mutex lock");
  
  printf("\trexit %lu: --rcount\n", pthread_self());
  --mon->rcount;
  assert(mon->wcount == 0);

  /* `&& mon->rwoken == 0`: in case multiple readers got broadcasted but before
   * all of them managed to enter some have already gotten here. They might like
   * to wake up a writer even though there are still readers to come!  therefore
   * check rwoken to see if a reader awakening isn't taking place */
  if (mon->wcount == 0 && mon->rcount == 0 && mon->wwait > 0 && mon->rwoken == 0) {
    printf("reader %lu is waking up a writer cause wc=%lu rc=%lu ww=%lu\n",
           pthread_self(), mon->wcount, mon->rcount, mon->wwait);
    mon->wwoken = 1;
    err = pthread_cond_signal(&mon->writers);
    syserr(err, "rexit, cond signal");
  } else if (mon->wcount == 0 && mon->rcount == 0) {
    printf("reader %lu is waking up readers cause wc=%lu, rc=%lu rw=%lu\n",
           pthread_self(), mon->wcount, mon->rcount, mon->rwait);
    mon->rwoken = mon->rwait;
    err = pthread_cond_broadcast(&mon->readers);
    syserr(err, "rexit, cond broadcast");
  }

  err = pthread_mutex_unlock(&mon->mutex);
  syserr(err, "rexit, mutex unlock");

  return err;
}
