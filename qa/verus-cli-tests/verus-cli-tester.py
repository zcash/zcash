from subprocess import Popen, check_output, call
from time import sleep
from os import environ, path

daemon_wrapper = "verusd"
cli_wrapper = "verus"
daemon_runtime_seconds = 600
cli_commands = ["getblockchaininfo", "getmininginfo", "getwalletinfo", "stop"]


def start_daemon(daemon_wrapper):
    try:
        Popen(daemon_wrapper, shell=True, close_fds=True)
    except:
        exit(1)


def fetch_zcash_params():
    try:
        call("fetch-params", shell=True)
    except:
        exit(1)


def run_cli_commands(cli_wrapper, commands):
    for command in commands:
        command = "%(cli_wrapper)s %(command)s" % locals()
        try:
            with open(path.join(environ["CI_PROJECT_DIR"], "log.txt"), "a") as log:
                command_output = check_output(command, shell=True)
                log.write("%(command_output)s\n" % locals())
        except:
            exit(1)


fetch_zcash_params()
start_daemon(daemon_wrapper)
sleep(daemon_runtime_seconds)
run_cli_commands(cli_wrapper, cli_commands)
