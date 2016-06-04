#! /usr/bin/env python
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright © 2016 Martine Lenders <mail@martine-lenders.eu>
#
# Distributed under terms of the MIT license.

import os
import pexpect
import sys
import time

MAX_EXP_MINUTES=12

EMPTY_APP_PATH = "../empty/"
APPS = ['time_tx', 'time_tx_rpl', 'time_rx', 'time_rx_rpl']
STACKS = ['emb6', 'gnrc', 'lwip']
NON_RPL_STACKS = ['lwip']
IOTLAB_USER = 'lenders'
IOTLAB_SITE = 'paris'
IOTLAB_NODE = 43
IOTLAB_EXP_NAME = 'masterthesis_run'
IOTLAB_DURATION = 2 * MAX_EXP_MINUTES * len(APPS) * len(STACKS)

MINUTE=60
MAX_BUILD_TIME=5 * MINUTE
MAX_EXP_TIME=MAX_EXP_MINUTES * MINUTE

def build(stacktest=False):
    env = os.environ
    env.update({'STACKTEST': str(int(bool(stacktest)))})
    make = pexpect.spawn("make -B clean all", env=env, timeout=MAX_BUILD_TIME)
    start = time.time()
    for app in APPS:
        for stack in STACKS:
            if (app[-3:] == 'rpl') and (stack in NON_RPL_STACKS):
                continue
            sys.stdout.write("Building %s_%s%s" %
                             (stack, app, " (stacktest)" if stacktest else ""))
            sys.stdout.flush()
            make.expect(r"/%s_%s\.elf\s*$" % (stack, app))
            print("... done")
    duration = time.time() - start
    make.wait()
    print("Building took %.2f minutes" % (duration / MINUTE))

def start_exp(site, node, duration, iotlab_exp_name):
    env = os.environ
    env.update({ 'IOTLAB_SITE': str(site),
                 'IOTLAB_PHY_NODES': str(node),
                 'IOTLAB_DURATION': str(duration),
                 'IOTLAB_EXP_NAME': str(iotlab_exp_name)})
    make = pexpect.spawn("make -C %s iotlab-exp" % EMPTY_APP_PATH, env=env,
                         timeout=MAX_BUILD_TIME)
    print("Starting experiment")
    make.expect(r"Waiting that experiment (\d+) gets in state Running")
    iotlab_exp_id = int(make.match.group(1))
    make.expect("\"Running\"")
    make.wait()
    print("Started experiment %d" % iotlab_exp_id)
    return iotlab_exp_id

def stop_exp(iotlab_exp_id=None):
    env = os.environ
    if iotlab_exp_id != None:
        env.update({'IOTLAB_EXP_ID': str(iotlab_exp_id)})
    make = pexpect.spawn("make -C %s iotlab-stop" % EMPTY_APP_PATH, env=env,
                         timeout=MINUTE)
    make.expect("Deleting the job = %d ...REGISTERED." % iotlab_exp_id)
    make.wait()
    IOTLAB_EXP_ID = 0
    print("Stopped experiment %d" % iotlab_exp_id)

def flash(path, iotlab_exp_id):
    print("Flashing %s" % path)
    env = os.environ
    env.update({'IOTLAB_EXP_ID': str(iotlab_exp_id)})
    make = pexpect.spawn("make -C %s iotlab-flash" % path, env=env,
                         timeout=MINUTE)
    make.expect("\"0\": \\[")
    make.expect("\"m3-\\d+.\\w+.iot-lab.info\"")
    make.wait()
    print("Flashed %s" % path)

def run_experiments(site, node, iotlab_exp_id, stacktest=False):
    env = os.environ
    env.update({'STACKTEST': str(int(bool(stacktest)))})
    if stacktest:
        log_name = "stackusage_%s-%d-%d-%d.txt" % (site, node, iotlab_exp_id, time.time())
    else:
        log_name = "times_%s-%d-%d-%d.txt" % (site, node, iotlab_exp_id, time.time())
    logger = pexpect.spawn("/bin/bash", ['-c',
                           "ssh %s@%s.iot-lab.info " % (IOTLAB_USER, site) +
                            "\"tmux -c 'serial_aggregator -i %d &> %s'\"" %
                            (iotlab_exp_id, log_name)])
    time.sleep(5)
    watcher = pexpect.spawn("ssh %s@%s.iot-lab.info 'tail -F %s'" %
                            (IOTLAB_USER, site, log_name), timeout=MAX_EXP_TIME)

    for app in APPS:
        for stack in STACKS:
            if (app[-3:] == 'rpl') and (stack in NON_RPL_STACKS):
                continue
            flash("%s/%s" % (app, stack), iotlab_exp_id)
            start = time.time()
            watcher.expect("%s_%s stopped" % (stack, app))
            duration = time.time() - start
            print("%s_%s%s ran for %.2f minutes" %
                  (stack, app, " (stacktest)" if stacktest else "",
                  duration / MINUTE))
    watcher.terminate()
    watcher.wait()
    logger.terminate()
    logger.wait()

if __name__ == "__main__":
    os.chdir(os.path.dirname(sys.argv[0]))
    build(True)
    IOTLAB_EXP_ID = start_exp(IOTLAB_SITE, IOTLAB_NODE, IOTLAB_DURATION,
                              IOTLAB_EXP_NAME)
    try:
        run_experiments(IOTLAB_SITE, IOTLAB_NODE, IOTLAB_EXP_ID, True)
        # build(True)
        # run_experiments(IOTLAB_SITE, IOTLAB_NODE, IOTLAB_EXP_ID, True)
    finally:
        stop_exp(IOTLAB_EXP_ID)
