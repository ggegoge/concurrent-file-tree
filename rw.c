
#include <stddef.h>
#include <pthread.h>

#include <assert.h>
#include <stdio.h>

#include "rw.h"

int monit_init(Monitor* mon)
{
  int err;

  /* consciously using "||" operator's laziness */
  if ((err = pthread_mutex_init(&mon->mutex, 0)) ||
      (err = pthread_cond_init(&mon->readers, 0)) ||
      (err = pthread_cond_init(&mon->writers, 0)))
    return err;

  mon->rwait = mon->wwait = mon->wcount = mon->rcount = mon->wid = 0;
  return 0;
}

int monit_destroy(Monitor* mon)
{
  int err;

  if ((err = pthread_mutex_destroy(&mon->mutex)) ||
      (err = pthread_cond_destroy(&mon->readers)) ||
      (err = pthread_cond_destroy(&mon->writers)))
    return err;

  mon->rwait = mon->wwait = mon->wcount = mon->rcount = 0;
  return 0;
}

int writer_entry(Monitor* mon)
{
  int err;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);

  /* If I'm a writer here then I can write along as I am a sequential being
   * apart from that i wait if there are some others working */
  if (!(mon->wcount > 0 && mon->wid == pthread_self()) &&
      (mon->rwait > 0 || mon->rcount > 0 || mon->wcount > 0 || mon->wwait > 0)) {
    printf("writer %lu goes to sleep cause rw=%lu rc=%lu wc=%lu\n",
           pthread_self(), mon->rwait, mon->rcount, mon->wcount);
    ++mon->wwait;
    err = pthread_cond_wait(&mon->writers, &mon->mutex);
    printf("writer %lu woke up and rw=%lu rc=%lu wc=%lu\n",
           pthread_self(), mon->rwait, mon->rcount, mon->wcount);
    --mon->wwait;
  }

  ++mon->wcount;
  mon->wid = pthread_self();
  printf("\twentr %lu: ++wcount\n", pthread_self());
  assert(mon->wcount > 0 || mon->wid == pthread_self());
  assert(mon->rcount == 0);
  err = pthread_mutex_unlock(&mon->mutex);

  return err;
}

int writer_exit(Monitor* mon)
{
  int err;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);
  assert(mon->wcount > 0 || mon->wid == pthread_self());
  --mon->wcount;

  if (mon->wcount == 0)
    mon->wid = 0;

  printf("\twexit %lu: --wcount\n", pthread_self());

  if (mon->wcount == 0 && mon->rcount == 0 && mon->rwait > 0) {
    printf("writer %lu is waking up a reader\n", pthread_self());
    err = pthread_cond_broadcast(&mon->readers);
  } else if (mon->wcount == 0 && mon->rcount == 0 && mon->wwait > 0) {
    printf("writer %lu is waking up a writer\n", pthread_self());
    err = pthread_cond_signal(&mon->writers);
  }

  err = pthread_mutex_unlock(&mon->mutex);

  return err;
}

int reader_entry(Monitor* mon)
{
  int err;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);

  /* I wait if either im not the current owner of this or if i have other more
   * classical reasons like waiting for others to finish their business */
  if (!(mon->wcount > 0 && mon->wid == pthread_self()) &&
      (mon->wwait > 0 || mon->wcount > 0)) {
    printf("reader %lu goes to sleep cause ww=%lu wc=%lu\n",
           pthread_self(), mon->wwait, mon->wcount);
    ++mon->rwait;
    err = pthread_cond_wait(&mon->readers, &mon->mutex);
    printf("reader %lu woke up and ww=%lu wc=%lu\n",
           pthread_self(), mon->wwait, mon->wcount);
    --mon->rwait;
  }

  assert(mon->wcount == 0 || mon->wid == pthread_self());
  printf("\trentr %lu: ++rcount\n", pthread_self());
  ++mon->rcount;
  err = pthread_mutex_unlock(&mon->mutex);

  return err;
}

int reader_exit(Monitor* mon)
{
  int err;

  if (!mon)
    return 0;

  err = pthread_mutex_lock(&mon->mutex);
  printf("\trexit %lu: --rcount\n", pthread_self());
  --mon->rcount;
  assert(mon->wcount == 0 || mon->wid == pthread_self());

  if (mon->wcount == 0 && mon->rcount == 0 && mon->wwait > 0) {
    printf("reader %lu is waking up a writer\n", pthread_self());
    err = pthread_cond_signal(&mon->writers);
  } else if (mon->wcount == 0 && mon->rcount == 0) {
    printf("reader %lu is waking up a reader\n", pthread_self());
    err = pthread_cond_broadcast(&mon->readers);
  }

  err = pthread_mutex_unlock(&mon->mutex);

  return err;
}
