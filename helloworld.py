#! /usr/bin/python
import redis
import time

def helloworld(redis_service):
    redis_service.execute_command("DEL", "helloworld_dehydrator")
    # push some data into the dehydrator
    redis_service.execute_command("dehydrator.push", "helloworld_dehydrator", "x", "world", 1)
    redis_service.execute_command("dehydrator.push", "helloworld_dehydrator", "y", "goodbye",2)
    redis_service.execute_command("dehydrator.push", "helloworld_dehydrator", "z", "hello", 3)

    # pull unneeded data before it expires
    redis_service.execute_command("dehydrator.pull", "helloworld_dehydrator", "y")

    # and make sure that it's gone
    try:
        redis_service.execute_command("dehydrator.look", "helloworld_dehydrator", "y")
    except redis.exceptions.ResponseError:
        pass # caught "ERROR: No Such Element", as expected


    # or not
    assert(redis_service.execute_command("dehydrator.look", "helloworld_dehydrator", "x"))

    # poll at different times to get only the data that is done dehydrating
    time.sleep(1)
    t1_poll_result = redis_service.execute_command("dehydrator.poll", "helloworld_dehydrator")

    time.sleep(2)
    t3_poll_result = redis_service.execute_command("dehydrator.poll", "helloworld_dehydrator")

    print t3_poll_result[0], t1_poll_result[0]
    redis_service.execute_command("DEL", "helloworld_dehydrator")


if __name__ == "__main__":
    r = redis.StrictRedis(host='localhost', port=6379, db=0)
    helloworld(r)
