#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include <sys/time.h>

#include "time.h"

#define ms_to_us(x) x * 1000

void
sleep_ms(int ms)
{
    sleep_us(ms_to_us(ms));
}

void
sleep_us(int us)
{
    usleep(us);
}

void
sleep_ns(int ns)
{
    struct timespec t = {
        .tv_sec = 0,
        .tv_nsec = ns,
    };
    struct timespec r;

    nanosleep(&t, &r);
}

time_t
get_current_timestamp_in_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;
}

struct timespec
get_current_timespec_plus_ms(time_t ms)
{
    struct timeval tp;
    struct timespec ts;
    int rc = gettimeofday(&tp, NULL);

    ts.tv_sec = tp.tv_sec;
    ts.tv_nsec = tp.tv_usec * 1000;

    ts.tv_nsec += ms * 1000000;

    ts.tv_sec += ts.tv_nsec / 1000000000L;
    ts.tv_nsec = ts.tv_nsec % 1000000000L;

    return ts;
}
