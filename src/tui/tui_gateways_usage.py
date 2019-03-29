#!/usr/bin/env python3

from lib import rpclib, tuilib
import os, time

header = "\
 _____       _                               _____  _____ \n\
|  __ \     | |                             /  __ \/  __ \\\n\
| |  \/ __ _| |_ _____      ____ _ _   _ ___| /  \/| /  \/\n\
| | __ / _` | __/ _ \ \ /\ / / _` | | | / __| |    | |    \n\
| |_\ \ (_| | ||  __/\ V  V / (_| | |_| \__ \ \__/\| \__/\\\n\
 \____/\__,_|\__\___| \_/\_/ \__,_|\__, |___/\____/ \____/\n\
                                    __/ |                 \n\
                                   |___/                  \n"

menuItems = [
    {"Check connection to assetchain": tuilib.getinfo_tui},
    {"Check assetchain mempool": tuilib.print_mempool},
    {"Check connection to KMD": tuilib.getinfo_tui},
    {"Connect to KMD daemon": tuilib.rpc_kmd_connection_tui},
    {"Send KMD gateway deposit transaction": tuilib.gateways_send_kmd},
    {"Execute gateways deposit": tuilib.gateways_deposit_tui},
    {"Execute gateways claim": tuilib.gateways_claim_tui},
    {"Execute gateways withdrawal": tuilib.gateways_withdrawal_tui},
    {"Exit": exit}
]

def main():
    while True:
        os.system('clear')
        print(tuilib.colorize(header, 'pink'))
        print(tuilib.colorize('CLI version 0.2\n', 'green'))
        for item in menuItems:
            print(tuilib.colorize("[" + str(menuItems.index(item)) + "] ", 'blue') + list(item.keys())[0])
        choice = input(">> ")
        try:
            if int(choice) < 0:
                raise ValueError
            # Call the matching function
            if list(menuItems[int(choice)].keys())[0] == "Exit":
                list(menuItems[int(choice)].values())[0]()
            # We have to call KMD specific functions with connection to KMD daemon
            elif list(menuItems[int(choice)].keys())[0] == "Connect to KMD daemon":
                rpc_connection_kmd = list(menuItems[int(choice)].values())[0]()
            elif list(menuItems[int(choice)].keys())[0] == "Check connection to KMD":
                while True:
                    try:
                        list(menuItems[int(choice)].values())[0](rpc_connection_kmd)
                        break
                    except Exception as e:
                        print("Please connect to KMD daemon first!")
                        input("Press [Enter] to continue...")
                        break
            elif list(menuItems[int(choice)].keys())[0] == "Send KMD gateway deposit transaction":
                while True:
                    try:
                        list(menuItems[int(choice)].values())[0](rpc_connection_kmd)
                        break
                    except Exception as e:
                        print(e)
                        print("Please connect to KMD daemon first!")
                        input("Press [Enter] to continue...")
                        break
            elif list(menuItems[int(choice)].keys())[0] == "Execute gateways deposit":
                while True:
                    try:
                        list(menuItems[int(choice)].values())[0](rpc_connection, rpc_connection_kmd)
                        break
                    except Exception as e:
                        print(e)
                        print("Please connect to KMD daemon first!")
                        input("Press [Enter] to continue...")
                        break
            else:
                list(menuItems[int(choice)].values())[0](rpc_connection)
        except (ValueError, IndexError):
            pass


if __name__ == "__main__":
    while True:
        try:
            print(tuilib.colorize("Welcome to the GatewaysCC TUI!\nPlease provide RPC connection details for initialization", "blue"))
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
