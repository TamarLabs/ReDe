#! /usr/bin/python
import redis
import time
import random
import sys

if __name__ == "__main__":
    r = redis.StrictRedis(host='localhost', port=6379, db=0)

    if (not (3 <= len(sys.argv) <= 4)) or (sys.argv[1] in ["-?", "-h", "--help"]):
        print "usage: %s <dehydrator_name> <channel> [Hz]" % sys.argv[0]
        exit()

    dehydrator_name = sys.argv[1]
    channel = sys.argv[2]
    if len(sys.argv) > 3:
        Hz = sys.argv[3]
        sleep_time = 1.0/Hz
    else:
        sleep_time = 1.0

    print "Polling and Publishing (press Ctrl-C to quit)"
    user_notified = False
    while True:
        try:
            poll_result = r.execute_command("REDE.POLL", dehydrator_name)
        except:
            if not user_notified:
                print "an error occured in POLL, probably bad dehydrator name (this message will not be repeted)"
                user_notified = True
        else:
            for element in poll_result:
                poll_result = r.execute_command("PUBLISH", channel, element)
        time.sleep(sleep_time)
