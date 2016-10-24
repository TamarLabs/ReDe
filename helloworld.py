#! /usr/bin/python
import redis
import time

def helloworld(redis_service):
    redis_service.execute_command("dehydrator.clear")
    # push some data into the dehydrator
    redis_service.execute_command("dehydrator.push", "x", "world", 1)
    redis_service.execute_command("dehydrator.push", "y", "goodbye",2)
    redis_service.execute_command("dehydrator.push", "z", "hello", 3)

    # pull unneeded data before it expires
    redis_service.execute_command("dehydrator.pull", "y")

    # and make sure that it's gone
    assert(redis_service.execute_command("dehydrator.look", "y") == False)
    #or not
    assert(redis_service.execute_command("dehydrator.look", "x") == True)

    # poll at different times to get only the data that is done dehydrating
    time.sleep(1)
    t1_poll_result = redis_service.execute_command("dehydrator.poll")

    time.sleep(2)
    t3_poll_result = redis_service.execute_command("dehydrator.poll")

    print t3_poll_result[0], t1_poll_result[0]


if __name__ == "__main__":
    r = redis.StrictRedis(host='localhost', port=6379, db=0)
    helloworld(r)
