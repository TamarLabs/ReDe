[![GitHub version](https://img.shields.io/github/release/tamarlabs/rede.svg?style=flat-square)](https://github.com/TamarLabs/ReDe/releases/latest)
[![issues count](https://img.shields.io/github/issues/tamarlabs/rede.svg?style=flat-square)](https://github.com/TamarLabs/ReDe/issues)
[![Build Status](https://img.shields.io/travis/TamarLabs/ReDe/master.svg?style=flat-square)](https://travis-ci.org/TamarLabs/ReDe)
<h1>  ReDe - The <img src="https://upload.wikimedia.org/wikipedia/en/6/6b/Redis_Logo.svg" alt="redis" height="47" align="top"/> Element Dehydration Module</h1>

:rocket:**TL;DR - A Dehydrator is a fancy delayed queue, see what commands this module provides [HERE](docs/Commands.md)**

**ReDe** /'redɪ/ *n.* a Redis Module for simple data dehydration. This is a pretty straightforward implementation of the dehydration system depicted in the article "[Fast Data](https://goo.gl/DDFFPO)". The Goal of this module is to solve the *Contextual Completeness* and *Emergent Relevancy* problems by adding the ability to postpone incoming elements to a later time in which we will have a complete information for these elements. Effectively acting as a snooze button to any element.

![a schematic view of the Filter-Split-Dehydrate architecture](img/FSD-scheme.png)

From the article:
> Dehydrators are simplistic time machines. They transport data elements that arrived prematurely in terms of their context right to the future where they might be needed, without loading the system while waiting. This concept is achieved by attaching a time-indexed data store to a clock, storing elements as they arrive to the dehydrator and re-introducing them as inputs to the system once a predetermined time period has passed.


Using this system it is also possible to craft a self cleaning "claims check", to minimize load on transportation and manipulation nodes of a pipeline architecture.

*You can read further on the algorithm behind this module [here](docs/Algorithm.md).*

The module works by adding a new type to Redis -`DehydratorType`. It will be ceated automatically when you call a push command on it, and it can be deleted using the `DEL` command like any other key.

![a gif that shows basic usage](img/redehy-basics.gif)

## Common Use Cases

* **Stream Coordination** -  Make data from one stream wait for the corresponding data from another (preferebly using sliding-window style timing).
* **Event Rate Limitation** - Delay any event beyond current max throughput to the next available time slot, while preserving order.
* **Self Cleaning Claims-Check** - Store data for a well known period, without the need to search for it when it is expired or clear it from the data-store yourself.
* **Task Timer** - Postpone actions and their respective payloads to a specific point in time.

## What this repo includes:

### 1. Dehydration module source code

[module.c](src/module.c) - Build it, read it, love it, extend it (PRs are welcome)!

### 2. usage example files and load tests

In this repository there are two python files that exemplify the usage of the module:
* [helloworld.py](tests/helloworld.py) - very simple usage example of all the functions exposed by the module
* [test.py](tests/test.py) - run internal as well as external functional tests, load test and print it all to stdout.

### 3. Redis PubSub utility script

[pubsub.py](src/pubsub.py) - A workaround for the lack of redis background tasks for providing PUB/SUB functionality


### 4. Redis [Benchmark](src/redis-benchmark.c)

The modified redis-benchmark code used to measure some of the module's performance.

### 5. klib [khash](src/khash.h)

A set of macros to create the hash maps used to implement the dehydrator type.

### 6. LibRMUtil

From [Redis Modules SDK](https://github.com/RedisLabs/RedisModulesSDK) README:

A small library of utility functions and macros for module developers, including:

* Easier argument parsing for your commands.
* Testing utilities that allow you to wrap your module's tests as a redis command.
* `RedisModuleString` utility functions (formatting, comparison, etc)
* The entire `sds` string library, lifted from Redis itself.
* A generic scalable Vector library. Not redis specific but we found it useful.
* A few other helpful macros and functions.
* `alloc.h`, an include file that allows modules implementing data types to implicitly replace the `malloc()` function family with the Redis special allocation wrappers.

It can be found under the `rmutil` folder, and compiles into a static library you link your module against.    

## Usage

The dehydrator is an effective 'snooze button' for events, you push an event into it along with an id (for future referance) and in how many seconds you want it back, and poll whenever you want the elements back. only expired elements would pop out.

**The module include 7 commands:**

* [`REDE.PUSH`](docs/Commands.md/#push) - Insert an element. The command takes an id for the element, the element itself and dehydration time in milliseconds.
* [`REDE.PULL`](docs/Commands.md/#pull) - Remove the element with the appropriate id before it expires.
* [`REDE.POLL`](docs/Commands.md/#poll) - Pull and return all the expired elements.
* [`REDE.GIDPUSH`](docs/Commands.md/#gidpush) - Insert an element. The command generates an id for the element, but still needs the element itself and dehydration time in milliseconds.
* [`REDE.LOOK`](docs/Commands.md/#look) - Search the dehydrator for an element with the given id and if found return it's payload (without pulling).
* [`REDE.TTN`](docs/Commands.md/#ttn) - Return the minimal time between now and the first expiration
* [`REDE.UPDATE`](docs/Commands.md/#update) - Set the element represented by a given id, the current element will be returned, and the new element will inherit the current expiration.

**it also includes a test command:**
* `REDE.TEST`  - a set of unit tests of the above commands. **NOTE!** This command is running in fixed time (~15 seconds) as it uses `sleep` (dios mio, No! &#x271e;&#x271e;&#x271e;).

*see more about the commands in [Commands.md](docs/Commands.md)*

### Quick Start Guide

Here's what you need to do to build this module:

0. Build Redis in a build supporting modules.
1. Build the module: `make` or download the `.so` file from [the latest release](https://github.com/TamarLabs/ReDe/releases/latest)
3. Run Redis loading the module: `/path/to/redis-server --loadmodule path/to/module.so`

Now run `redis-cli` and try the commands:

```
127.0.0.1:9979> REDE.PUSH some_dehy id1 world 15
OK
127.0.0.1:9979> REDE.PUSH some_dehy id2 hello 1
OK
127.0.0.1:9979> REDE.PUSH some_dehy id3 goodbye 2
OK
127.0.0.1:9979> REDE.PULL some_dehy id3
"goodbye"
127.0.0.1:9979> REDE.POLL some_dehy
1) "hello"
127.0.0.1:9979> REDE.POLL some_dehy
(empty list or set)
127.0.0.1:6379> REDE.LOOK some_dehy id2
(nil)
127.0.0.1:6379> REDE.LOOK some_dehy id1
"world"
127.0.0.1:6379> REDE.PULL some_dehy id2
(nil)
127.0.0.1:6379> REDE.TTN some_dehy
8
```

This `(empty list or set)` reply from `REDE.POLL` means that the there are no more items to pull right now, so we'll have to wait until enough time passes for our next element to expire. using `REDE.TTN` we can see this will be in 8 seconds (in this example we waited a bit between commands). Once 8 seconds will pass we can run:

```
127.0.0.1:9979> REDE.POLL some_dehy
1) "world"
127.0.0.1:9979> REDE.TEST
PASS
(15.00s)
127.0.0.1:9979> DEL some_dehy
OK
```

Enjoy!

## Knowen Issues

* TTN test is somewhat unstable on small or heavliy loaded machines.

## Future work

* add some sort of pub/sub mechanism to POLL - waiting for some sort of "reactor" pattern or background tasks in redis (maybe this should be a module).. right now this functionality can be achieved by using [this python script](src/pubsub.py), there is currently also a task to add a blocking command that duplicates the behavior of the script. 
* Additional / more thorough / automatic tests

## About This Module

This module is based off a python version of the same concepts designed by Adam Lev-Libfeld and developed in Tamar Labs by Adam Lev-Libfeld and Alexander Margolin in mid 2015.

The Redis module was created by Adam Lev-Libfeld during the RedisModulesHackathon in late 2016, and is maintained by him solely.
