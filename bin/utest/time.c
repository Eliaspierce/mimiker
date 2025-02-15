#include "utest.h"
#include "util.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

TEST_ADD(gettimeofday) {
  timeval_t time1, time2;
  const int64_t start_of_century = 94684800, end_of_century = 4102444799;

  for (int g = 0; g < 100; g++) {
    assert(gettimeofday(&time1, NULL) == 0);
    assert(gettimeofday(&time2, NULL) == 0);

    /* Time can't move backwards */
    assert(timercmp(&time2, &time1, <) == 0);

    /* Time belong to XXI century */
    assert(time1.tv_sec >= start_of_century);
    assert(time1.tv_sec <= end_of_century);
    assert(time2.tv_sec >= start_of_century);
    assert(time2.tv_sec <= end_of_century);
  }

  return 0;
}

TEST_ADD(nanosleep) {
  /* Requested and remaining time */
  timespec_t rqt, rmt;
  timeval_t time1, time2, diff;
  int ret;

  /* Incorrect arguments */
  rqt.tv_sec = -1;

  rqt.tv_nsec = 1;
  syscall_fail(nanosleep(&rqt, NULL), EINVAL);
  syscall_fail(nanosleep(&rqt, &rmt), EINVAL);

  rqt.tv_sec = 0;
  rqt.tv_nsec = -1;
  syscall_fail(nanosleep(&rqt, NULL), EINVAL);
  syscall_fail(nanosleep(&rqt, &rmt), EINVAL);

  rqt.tv_nsec = 1000000000;
  syscall_fail(nanosleep(&rqt, NULL), EINVAL);
  syscall_fail(nanosleep(&rqt, &rmt), EINVAL);

  rqt.tv_nsec = 1000;
  syscall_fail(nanosleep(NULL, NULL), EFAULT);
  syscall_fail(nanosleep(NULL, &rmt), EFAULT);

  /* Check if sleept at least requested time.
   * Please note that system clock has resolution of one milisecond! */
  for (int g = 0; g < 6; g++) {
    rqt.tv_nsec = (1000000 << g);
    diff.tv_sec = rqt.tv_sec;
    diff.tv_usec = rqt.tv_nsec / 1000;

    syscall_ok(gettimeofday(&time1, NULL));
    while ((ret = nanosleep(&rqt, &rmt)) == EINTR)
      rqt = rmt;
    assert(ret == 0);
    syscall_ok(gettimeofday(&time2, NULL));

    printf("time1: %d.%06d, time2: %d.%06d\n", (int)time1.tv_sec, time1.tv_usec,
           (int)time2.tv_sec, time2.tv_usec);
    timeradd(&time1, &diff, &time1);
    assert(timercmp(&time1, &time2, <=));
  }
  return 0;
}

TEST_ADD(itimer) {
  signal_setup(SIGALRM);
  struct itimerval it, it2;
  memset(&it, 0, sizeof(it));
  memset(&it2, 0, sizeof(it2));

  /* Try non-canonical timevals.  */
  it.it_value.tv_sec = -1;
  syscall_fail(setitimer(ITIMER_REAL, &it, NULL), EINVAL);
  it.it_value.tv_sec = 0;
  it.it_value.tv_usec = -1;
  syscall_fail(setitimer(ITIMER_REAL, &it, NULL), EINVAL);
  it.it_value.tv_usec = 1000000;
  syscall_fail(setitimer(ITIMER_REAL, &it, NULL), EINVAL);
  it.it_value.tv_usec = 0;
  it.it_value.tv_sec = 1;
  it.it_interval.tv_sec = -1;
  syscall_fail(setitimer(ITIMER_REAL, &it, NULL), EINVAL);
  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = -1;
  syscall_fail(setitimer(ITIMER_REAL, &it, NULL), EINVAL);
  it.it_interval.tv_usec = 1000000;
  syscall_fail(setitimer(ITIMER_REAL, &it, NULL), EINVAL);

  /* No timer should be currently set. */
  assert(getitimer(ITIMER_REAL, &it2) == 0);
  assert(!timerisset(&it2.it_value));
  assert(!timerisset(&it2.it_interval));

  /* Set one-shot timer for 1ms in the future. */
  timeval_t t, t2;
  assert(gettimeofday(&t, NULL) == 0);
  memset(&it, 0, sizeof(it));
  it.it_value.tv_usec = 1000;
  assert(setitimer(ITIMER_REAL, &it, NULL) == 0);
  wait_for_signal(SIGALRM);
  assert(gettimeofday(&t2, NULL) == 0);
  timersub(&t2, &t, &t);
  assert(timercmp(&t, &it.it_value, >=));

  /* No timer should be currently set. */
  assert(getitimer(ITIMER_REAL, &it2) == 0);
  assert(!timerisset(&it2.it_value));
  assert(!timerisset(&it2.it_interval));

  /* Set periodic timer with period of 1ms. */
  assert(gettimeofday(&t, NULL) == 0);
  memset(&it, 0, sizeof(it));
  it.it_value.tv_usec = 1000;
  it.it_interval.tv_usec = 1000;
  assert(setitimer(ITIMER_REAL, &it, NULL) == 0);
  /* The timer is periodic, so we should get multiple signals. */
  wait_for_signal(SIGALRM);
  wait_for_signal(SIGALRM);
  assert(gettimeofday(&t2, NULL) == 0);
  timersub(&t2, &t, &t);
  it.it_value.tv_usec = 2000;
  assert(timercmp(&t, &it.it_value, >=));

  /* The timer is still set, so getitimer() should return non-zero timer. */
  assert(getitimer(ITIMER_REAL, &it2) == 0);
  assert(it2.it_interval.tv_sec == 0 &&
         it2.it_interval.tv_usec == 1000); /* it_value can be zero */

  /* Clear the timer */
  memset(&it, 0, sizeof(it));
  assert(setitimer(ITIMER_REAL, &it, &it2) == 0);
  /* it2 should have the old timer value. */
  assert(it2.it_interval.tv_sec == 0 && it2.it_interval.tv_usec == 1000);

  /* No timer should be currently set. */
  assert(getitimer(ITIMER_REAL, &it2) == 0);
  assert(!timerisset(&it2.it_value));
  assert(!timerisset(&it2.it_interval));
  return 0;
}
