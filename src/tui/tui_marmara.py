#!/usr/bin/env python3

from lib import rpclib, tuilib
import os
import time


header = "\
___  ___                                       _____ _   _ _____ \n\
|  \/  |                                      |_   _| | | |_   _|\n\
| .  . | __ _ _ __ _ __ ___   __ _ _ __ __ _    | | | | | | | |\n\
| |\/| |/ _` | '__| '_ ` _ \ / _` | '__/ _` |   | | | | | | | |\n\
| |  | | (_| | |  | | | | | | (_| | | | (_| |   | | | |_| |_| |_\n\
\_|  |_/\__,_|_|  |_| |_| |_|\__,_|_|  \__,_|   \_/  \___/ \___/\n"


menuItems = [
    {"Check current connection": tuilib.getinfo_tui},
    {"Check mempool": tuilib.print_mempool},
    {"Check MARMARA info": tuilib.marmara_info_tui},
    {"Lock funds for MARMARA": tuilib.marmara_lock_tui},
    {"Request MARMARA cheque": tuilib.marmara_receive_tui},
    {"Issue MARMARA cheque": tuilib.marmara_issue_tui},
    {"Check credit loop status": tuilib.marmara_creditloop_tui},
    {"Settle MARMARA loop": tuilib.marmara_settlement_tui},
    {"Exit": exit}
]


def main():
    while True:
        os.system('clear')
        print(tuilib.colorize(header, 'pink'))
        print(tuilib.colorize('CLI version 0.1\n', 'green'))
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
        chain = input("Input assetchain name (-ac_name= value) you want to work with: ")
        try:
            print(tuilib.colorize("Welcome to the MarmaraCC TUI!\n"
                                  "Please provide asset chain RPC connection details for initialization", "blue"))
            rpc_connection = tuilib.def_credentials(chain)
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
