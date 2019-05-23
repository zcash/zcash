#!/usr/bin/env python3

from lib import rpclib, tuilib
import os
import time


header = "\
  ___                   _         _____ \n\
 / _ \                 | |       /  __ \\\n\
/ /_\ \ ___  ___   ___ | |_  ___ | /  \/\n\
|  _  |/ __|/ __| / _ \| __|/ __|| |    \n\
| | | |\__ \\\__ \|  __/| |_ \__ \| \__/\\\n\
\_| |_/|___/|___/ \___| \__||___/ \____/\n"


menuItems = [
    {"Check current connection": tuilib.getinfo_tui},
    {"Check mempool": tuilib.print_mempool},
    {"Print tokens list": tuilib.print_tokens_list},
    {"Check my tokens balances" : tuilib.print_tokens_balances},
    # transfer tokens (pre-print tokens balances)
    {"Create token": tuilib.token_create_tui},
    # trading zone - pre-print token orders - possible to open order or fill existing one
    {"Exit": exit}
]


def main():
    while True:
        os.system('clear')
        print(tuilib.colorize(header, 'pink'))
        print(tuilib.colorize('CLI version 0.2 by Anton Lysakov\n', 'green'))
        for item in menuItems:
            print(tuilib.colorize("[" + str(menuItems.index(item)) + "] ", 'blue') + list(item.keys())[0])
        choice = input(">> ")
        try:
            if int(choice) < 0:
                raise ValueError
            # Call the matching function
            if list(menuItems[int(choice)].keys())[0] == "Exit":
                list(menuItems[int(choice)].values())[0]()
            else:
                list(menuItems[int(choice)].values())[0](rpc_connection)
        except (ValueError, IndexError):
            pass


if __name__ == "__main__":
    while True:
        try:
            print(tuilib.colorize("Welcome to the GatewaysCC TUI!\n"
                                  "Please provide asset chain RPC connection details for initialization", "blue"))
            rpc_connection = tuilib.rpc_connection_tui()
            rpclib.getinfo(rpc_connection)
        except Exception:
            print(tuilib.colorize("Cant connect to RPC! Please re-check credentials.", "pink"))
        else:
            print(tuilib.colorize("Succesfully connected!\n", "green"))
            with (open("lib/logo.txt", "r")) as logo:
                for line in logo:
                    print(line, end='')
                    time.sleep(0.04)
                print("\n")
            break
    main()

