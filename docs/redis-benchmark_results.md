<<<<<<< HEAD
# Redis-Benchmark Resultsthe code used to generate these results is available in [`src/redis-benchmark.c`](../src/redis-benchmark.c)```$ ./redis-benchmark -t rede -n 1000000====== REDE.PUSH ======  1000000 requests completed in 5.88 seconds  50 parallel clients  3 bytes payload  keep alive: 199.94% <= 1 milliseconds99.97% <= 2 milliseconds99.98% <= 3 milliseconds99.98% <= 4 milliseconds99.98% <= 5 milliseconds99.99% <= 6 milliseconds99.99% <= 7 milliseconds100.00% <= 8 milliseconds100.00% <= 8 milliseconds170183.80 requests per second====== REDE.PUSH ======  1000000 requests completed in 5.97 seconds  50 parallel clients  3 bytes payload  keep alive: 199.91% <= 1 milliseconds99.97% <= 2 milliseconds99.97% <= 3 milliseconds99.99% <= 4 milliseconds99.99% <= 5 milliseconds100.00% <= 6 milliseconds100.00% <= 7 milliseconds100.00% <= 8 milliseconds100.00% <= 8 milliseconds167532.25 requests per second====== REDE.PUSH - 10000 second elements (needed for POLL) ======  1000000 requests completed in 6.70 seconds  50 parallel clients  3 bytes payload  keep alive: 199.85% <= 1 milliseconds99.96% <= 2 milliseconds99.98% <= 3 milliseconds99.99% <= 4 milliseconds99.99% <= 5 milliseconds99.99% <= 6 milliseconds100.00% <= 7 milliseconds100.00% <= 8 milliseconds100.00% <= 8 milliseconds149342.89 requests per second====== REDE.PUSH - 15 second elements (needed for POLL) ======  1000000 requests completed in 6.59 seconds  50 parallel clients  3 bytes payload  keep alive: 199.65% <= 1 milliseconds99.93% <= 2 milliseconds99.94% <= 3 milliseconds99.96% <= 4 milliseconds99.98% <= 5 milliseconds99.99% <= 6 milliseconds99.99% <= 7 milliseconds100.00% <= 8 milliseconds100.00% <= 9 milliseconds100.00% <= 10 milliseconds100.00% <= 10 milliseconds151653.02 requests per second====== REDE.PUSH - 10 second elements (needed for POLL) ======  1000000 requests completed in 7.09 seconds  50 parallel clients  3 bytes payload  keep alive: 199.90% <= 1 milliseconds99.95% <= 2 milliseconds99.96% <= 3 milliseconds99.97% <= 4 milliseconds99.99% <= 5 milliseconds99.99% <= 6 milliseconds99.99% <= 7 milliseconds99.99% <= 8 milliseconds100.00% <= 9 milliseconds100.00% <= 10 milliseconds100.00% <= 10 milliseconds141063.61 requests per second====== REDE.PUSH - 5 second elements (needed for POLL) ======  1000000 requests completed in 7.33 seconds  50 parallel clients  3 bytes payload  keep alive: 199.74% <= 1 milliseconds99.96% <= 2 milliseconds99.98% <= 3 milliseconds99.98% <= 4 milliseconds99.99% <= 5 milliseconds99.99% <= 6 milliseconds99.99% <= 8 milliseconds100.00% <= 9 milliseconds100.00% <= 9 milliseconds136425.66 requests per second====== REDE.POLL ======  1000000 requests completed in 6.38 seconds  50 parallel clients  3 bytes payload  keep alive: 199.94% <= 1 milliseconds99.97% <= 2 milliseconds99.98% <= 3 milliseconds99.98% <= 5 milliseconds99.98% <= 6 milliseconds99.99% <= 7 milliseconds99.99% <= 8 milliseconds100.00% <= 8 milliseconds156666.14 requests per second```
=======
# Redis-Benchmark Results
the code used to generate these results is available in [`src/redis-benchmark.c`](../src/redis-benchmark.c)


```
$ ./redis-benchmark -t rede -n 1000000


====== REDE.PUSH ======
  1000000 requests completed in 5.88 seconds
  50 parallel clients
  3 bytes payload
  keep alive: 1

99.94% <= 1 milliseconds
99.97% <= 2 milliseconds
99.98% <= 3 milliseconds
99.98% <= 4 milliseconds
99.98% <= 5 milliseconds
99.99% <= 6 milliseconds
99.99% <= 7 milliseconds
100.00% <= 8 milliseconds
100.00% <= 8 milliseconds
170183.80 requests per second

====== REDE.PUSH ======
  1000000 requests completed in 5.97 seconds
  50 parallel clients
  3 bytes payload
  keep alive: 1

99.91% <= 1 milliseconds
99.97% <= 2 milliseconds
99.97% <= 3 milliseconds
99.99% <= 4 milliseconds
99.99% <= 5 milliseconds
100.00% <= 6 milliseconds
100.00% <= 7 milliseconds
100.00% <= 8 milliseconds
100.00% <= 8 milliseconds
167532.25 requests per second

====== REDE.PUSH - 10000 second elements (needed for POLL) ======
  1000000 requests completed in 6.70 seconds
  50 parallel clients
  3 bytes payload
  keep alive: 1

99.85% <= 1 milliseconds
99.96% <= 2 milliseconds
99.98% <= 3 milliseconds
99.99% <= 4 milliseconds
99.99% <= 5 milliseconds
99.99% <= 6 milliseconds
100.00% <= 7 milliseconds
100.00% <= 8 milliseconds
100.00% <= 8 milliseconds
149342.89 requests per second

====== REDE.PUSH - 15 second elements (needed for POLL) ======
  1000000 requests completed in 6.59 seconds
  50 parallel clients
  3 bytes payload
  keep alive: 1

99.65% <= 1 milliseconds
99.93% <= 2 milliseconds
99.94% <= 3 milliseconds
99.96% <= 4 milliseconds
99.98% <= 5 milliseconds
99.99% <= 6 milliseconds
99.99% <= 7 milliseconds
100.00% <= 8 milliseconds
100.00% <= 9 milliseconds
100.00% <= 10 milliseconds
100.00% <= 10 milliseconds
151653.02 requests per second

====== REDE.PUSH - 10 second elements (needed for POLL) ======
  1000000 requests completed in 7.09 seconds
  50 parallel clients
  3 bytes payload
  keep alive: 1

99.90% <= 1 milliseconds
99.95% <= 2 milliseconds
99.96% <= 3 milliseconds
99.97% <= 4 milliseconds
99.99% <= 5 milliseconds
99.99% <= 6 milliseconds
99.99% <= 7 milliseconds
99.99% <= 8 milliseconds
100.00% <= 9 milliseconds
100.00% <= 10 milliseconds
100.00% <= 10 milliseconds
141063.61 requests per second

====== REDE.PUSH - 5 second elements (needed for POLL) ======
  1000000 requests completed in 7.33 seconds
  50 parallel clients
  3 bytes payload
  keep alive: 1

99.74% <= 1 milliseconds
99.96% <= 2 milliseconds
99.98% <= 3 milliseconds
99.98% <= 4 milliseconds
99.99% <= 5 milliseconds
99.99% <= 6 milliseconds
99.99% <= 8 milliseconds
100.00% <= 9 milliseconds
100.00% <= 9 milliseconds
136425.66 requests per second

====== REDE.POLL ======
  1000000 requests completed in 6.38 seconds
  50 parallel clients
  3 bytes payload
  keep alive: 1

99.94% <= 1 milliseconds
99.97% <= 2 milliseconds
99.98% <= 3 milliseconds
99.98% <= 5 milliseconds
99.98% <= 6 milliseconds
99.99% <= 7 milliseconds
99.99% <= 8 milliseconds
100.00% <= 8 milliseconds
156666.14 requests per second


```
>>>>>>> b9db41d4b2fa25a10ef13ae197533a9748c31b8c
