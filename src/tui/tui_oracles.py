#!/usr/bin/env python3

from lib import rpclib, tuilib
import os
import time

header = "\
 _____                     _             _____  _____ \n\
|  _  |                   | |           /  __ \/  __ \\\n\
| | | | _ __   __ _   ___ | |  ___  ___ | /  \/| /  \/\n\
| | | || '__| / _` | / __|| | / _ \/ __|| |    | |\n\
\ \_/ /| |   | (_| || (__ | ||  __/\__ \| \__/\| \__/\\\n\
 \___/ |_|    \__,_| \___||_| \___||___/ \____/ \____/\n"

menuItems = [
    # TODO: Have to implement here native oracle file uploader / reader, should be dope
    # TODO: data publisher / converter for different types
    {"Check current connection": tuilib.getinfo_tui},
    {"Check mempool": tuilib.print_mempool},
    {"Create oracle": tuilib.oracle_create_tui},
    {"Register as publisher for oracle": tuilib.oracle_register_tui},
    {"Subscribe on oracle (+UTXO generator)": tuilib.oracle_subscription_utxogen},
    {"Upload file to oracle": tuilib.convert_file_oracle_D},
    {"Display list of files uploaded to this AC": tuilib.display_files_list},
    {"Download files from oracle": tuilib.files_downloader},
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
