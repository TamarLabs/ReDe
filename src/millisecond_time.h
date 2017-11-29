#ifndef MILLISECONDS_TIME_H
#define MILLISECONDS_TIME_H

typedef long long mstime_t;

/*
 * @return current time in milliseconds
 */
mstime_t current_time_ms (void);

#ifdef REDIS_MODULE_TARGET /* Set this when compiling your code as a module */

static inline mstime_t rm_current_time_ms(void) {
  return RedisModule_Milliseconds();
}

#endif

#ifndef REDIS_MODULE_TARGET
/* for non redis module targets */
#define rm_current_time_ms current_time_ms

#endif

#endif // MILLISECONDS_TIME_H // 
