#!/usr/bin/env python3

from lib import rpclib, tuilib
import os
import time
import sys
import platform

header = "\
 _____    _        _    ______\n\
|_   _|  | |      (_)   |  _  \n\
  | | ___| |_ _ __ _ ___| | | |__ _ _ __  _ __\n\
  | |/ _ \ __| '__| / __| | | / _` | '_ \| '_ \\\n\
  | |  __/ |_| |  | \__ \ |/ / (_| | |_) | |_) |\n\
  \_/\___|\__|_|  |_|___/___/ \__,_| .__/| .__/\n\
                                   | |   | |\n\
                                   |_|   |_|"


menuItems = [
    {"Check current connection": tuilib.getinfo_tui},
    {"Check mempool": tuilib.print_mempool},
    {"Start singleplayer tetris game (creating, registering and starting game)": tuilib.rogue_newgame_singleplayer},
    {"Exit": tuilib.exit}
]


def main():
    while True:
        operating_system = platform.system()
        if operating_system != 'Win64' and operating_system != 'Windows':
            os.system('clear')
        else:
            os.system('cls')
        print(tuilib.colorize(header, 'pink'))
        print(tuilib.colorize('TUI v0.0.3\n', 'green'))
        menu_items_counter = 0
        for item in menuItems:
            print(tuilib.colorize("[" + str(menuItems.index(item)) + "] ", 'blue') + list(item.keys())[0])
        choice = input(">> ")
        try:
            if int(choice) < 0:
                raise ValueError
            # Call the matching function
            if list(menuItems[int(choice)].keys())[0] == "Exit":
                list(menuItems[int(choice)].values())[0]()
            elif list(menuItems[int(choice)].keys())[0] == "Start singleplayer tetris game (creating, registering and starting game)":
                list(menuItems[int(choice)].values())[0](rpc_connection, False)
            else:
                list(menuItems[int(choice)].values())[0](rpc_connection)
        except (ValueError, IndexError):
            pass


if __name__ == "__main__":
    while True:
        chain = "GTEST"
        try:
            print(tuilib.colorize("Welcome to the Tetris TUI!\n"
                                  "Please provide asset chain RPC connection details for initialization", "blue"))
            rpc_connection = tuilib.def_credentials(chain)
            rpclib.getinfo(rpc_connection)
            # waiting until chain is in sync
            while True:
                have_blocks = rpclib.getinfo(rpc_connection)["blocks"]
                longest_chain = rpclib.getinfo(rpc_connection)["longestchain"]
                if have_blocks != longest_chain:
                    print(tuilib.colorize("GTEST not synced yet.", "red"))
                    print("Have " + str(have_blocks) + " from " + str(longest_chain) + " blocks")
                    time.sleep(5)
                else:
                    print(tuilib.colorize("Chain is synced!", "green"))
                    break
            # checking if pubkey is set and set valid if not
            info = rpclib.getinfo(rpc_connection)
            if "pubkey" in info.keys():
                print("Pubkey is already set")
            else:
                valid_address = rpc_connection.getaccountaddress("")
                valid_pubkey = rpc_connection.validateaddress(valid_address)["pubkey"]
                rpc_connection.setpubkey(valid_pubkey)
                print(tuilib.colorize("Pubkey is succesfully set!", "green"))
            # copy ROGUE config to current daemon directory if it's not here
            tuilib.check_if_config_is_here(rpc_connection, "GTEST")
        except Exception:
            print(tuilib.colorize("Cant connect to GTEST daemon RPC! Please check if daemon is up.", "pink"))
            tuilib.exit()
        else:
            print(tuilib.colorize("Succesfully connected!\n", "green"))
            with (open("lib/logo.txt", "r")) as logo:
                for line in logo:
                    print(line, end='')
                    time.sleep(0.04)
                print("\n")
            break
    main()
