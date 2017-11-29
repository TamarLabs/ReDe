#! /usr/bin/python
import redis
import time

def helloworld(redis_service):
    redis_service.execute_command("DEL", "helloworld_dehydrator")
    # push some data into the dehydrator
    redis_service.execute_command("rede.push", "helloworld_dehydrator", 2000, "goodbye", "x")
    redis_service.execute_command("rede.gidpush", "helloworld_dehydrator", 1000, "world")
    redis_service.execute_command("rede.push", "helloworld_dehydrator", 3000, "derp", "y")

    # pull unneeded data before it expires
    redis_service.execute_command("rede.pull", "helloworld_dehydrator", "x")

    # and make sure that it's gone
    assert( not redis_service.execute_command("rede.look", "helloworld_dehydrator", "x"))

    # or still there
    if redis_service.execute_command("rede.look", "helloworld_dehydrator", "y") != "hello":
        redis_service.execute_command("rede.update", "helloworld_dehydrator", "y", "hello")

    # poll at different times to get only the data that is done dehydrating
    time.sleep(1)
    t1_poll_result = redis_service.execute_command("rede.poll", "helloworld_dehydrator")

    time.sleep(redis_service.execute_command("rede.ttn", "helloworld_dehydrator")/1000.0)
    t3_poll_result = redis_service.execute_command("rede.poll", "helloworld_dehydrator")

    print t3_poll_result[0], t1_poll_result[0]
    redis_service.execute_command("DEL", "helloworld_dehydrator")


if __name__ == "__main__":
    r = redis.StrictRedis(host='localhost', port=6379, db=0)
    helloworld(r)
