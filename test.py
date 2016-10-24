#! /usr/bin/python
import redis
import time
import random
import sys

def run_internal_test(redis_service):
    sys.stdout.write("module functional test (internal) - ")
    sys.stdout.flush()
    print(redis_service.execute_command("dehydrator.test"))

def function_test_dehydrator(redis_service):
    redis_service.execute_command("dehydrator.clear")
    sys.stdout.write("module functional test (external) - ")
    sys.stdout.flush()
    #  "push elements a,b & c (for 1,3 & 7 seconds)"
    redis_service.execute_command("dehydrator.push", "a", "test_element a", 1)
    redis_service.execute_command("dehydrator.push", "b", "test_element b",3)
    redis_service.execute_command("dehydrator.push", "c", "test_element c", 7)
    #  "pull element b"
    redis_service.execute_command("dehydrator.pull", "b")
    #  "poll (t=0) - no element should pop out right away"
    assert(len(redis_service.execute_command("dehydrator.poll")) == 0)
    #  "sleep 1"
    time.sleep(1)
    #  "poll (t=1) - we expect only element a to pop out"
    t1_poll_result = redis_service.execute_command("dehydrator.poll")
    # (t1_poll_result)
    assert(len(t1_poll_result) == 1 and t1_poll_result[0] == "test_element a")
    #  "sleep 1"
    time.sleep(1)
    #  "poll (t=2) - no element should pop out right now"
    assert(len(redis_service.execute_command("dehydrator.poll")) == 0)
    #  "sleep 8"
    time.sleep(8)
    # "poll (t=1) - we expect only element c to pop out (3 was already pulled)"
    t1_poll_result = redis_service.execute_command("dehydrator.poll")
    # (t1_poll_result)
    assert(len(t1_poll_result) == 1 and t1_poll_result[0] == "test_element c")
    print("PASS")
    redis_service.execute_command("dehydrator.clear")

def load_test_dehydrator(redis_service, cycles=100000, timeouts=[1,2,4,16,32,100,200,1000]):
    redis_service.execute_command("dehydrator.clear")
    print "starting load tests"

    print "measuring PUSH"
    start = time.time()
    # test push
    for i in range(cycles):
        redis_service.execute_command("dehydrator.push", "%d" % i, "payload", random.choice(timeouts))
    push_end = time.time()

    print "measuring PULL"
    for i in range(cycles):
        redis_service.execute_command("dehydrator.pull", "%d" % i)
    pull_end = time.time()

    print "preparing POLL"
    start_i = 0
    end_i = cycles/3
    for j in range(3):
        for i in range(start_i,end_i):
            redis_service.execute_command("dehydrator.push", "%d" % i, "payload", (3-j+random.choice([1,2,3])))
        start_i += cycles/3
        end_i += cycles/3
        time.sleep(1)

    print "measuring POLL"
    poll_sum = 0
    for j in range(10):
        time.sleep(1)
        poll_start = time.time()
        redis_service.execute_command("dehydrator.poll")
        poll_end = time.time()
        poll_sum += poll_end-poll_start

    print "mean push velocity =", cycles/(push_end-start), "per second"
    print "mean pull velocity =", cycles/(pull_end-push_end), "per second"
    print "mean poll velocity = ", cycles/poll_sum, "per second"


if __name__ == "__main__":
    r = redis.StrictRedis(host='localhost', port=6379, db=0)
    run_internal_test(r)
    function_test_dehydrator(r)
    load_test_dehydrator(r)
