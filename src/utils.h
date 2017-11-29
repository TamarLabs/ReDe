
/* for redis module targets */
#ifdef REDIS_MODULE_TARGET /* Set this when compiling your code as a module */
#define MALLOC RedisModule_Alloc
#endif
/* for non redis module targets */
#ifndef REDIS_MODULE_TARGET
#define MALLOC malloc
#endif

//##########################################################
//#
//#                    C Utilities
//#
//#########################################################

#define ID_LENGTH 31
#define ALLOWED_ID_CHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

char* string_append(char* a, const char* b);

long long current_time_ms (void);

// Assumes 0 <= max <= RAND_MAX
// Returns in the closed interval [0, max]
long random_at_most(long max);

char* generate_id(void);