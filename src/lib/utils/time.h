#ifndef TIME__H
#define TIME__H

void sleep_ms(int ms);
void sleep_us(int us);
void sleep_ns(int ns);

time_t get_current_timestamp_in_ms();
struct timespec get_current_timespec_plus_ms(time_t ms);

#endif
