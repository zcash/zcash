#!/usr/bin/env python3

from lib import rpclib, tuilib
import os
import time
import sys
import platform

header = "\
______                       _____  _____ \n\
| ___ \                     /  __ \/  __ \\\n\
| |_/ /___   __ _ _   _  ___| /  \/| /  \/\n\
|    // _ \ / _` | | | |/ _ \ |    | |\n\
| |\ \ (_) | (_| | |_| |  __/ \__/\| \__/\\\n\
\_| \_\___/ \__, |\__,_|\___|\____/ \____/\n\
             __/ |\n\
            |___/\n"


menuItems = [
    {"Check current connection": tuilib.getinfo_tui},
    {"Check mempool": tuilib.print_mempool},
    {"Check my warriors list": tuilib.print_players_list},
    {"Transfer warrior to other pubkey": tuilib.warrior_trasnfer},
    {"TOP-20 ROGUE Warriors": tuilib.top_warriors_rating},
    {"Set warriors name": tuilib.set_warriors_name},
    {"Start singleplayer training game (creating, registering and starting game)": tuilib.rogue_newgame_singleplayer},
    {"Create multiplayer game": tuilib.rogue_newgame_multiplayer},
    {"Join (register) multiplayer game": tuilib.rogue_join_multiplayer_game},
    {"Check my multiplayer games status / start": tuilib.play_multiplayer_game},
    {"Check if somebody wants to buy your warrior (incoming bids)": tuilib.print_icoming_bids},
    {"Place order to sell warrior": tuilib.sell_warrior},
    {"Place order to buy someones warrior": tuilib.place_bid_on_warriror},
    {"Check if somebody selling warrior": tuilib.find_warriors_asks},
    {"Check / cancel my warriors trade orders": tuilib.warriors_orders_check},
    # {"Manually exit the game (bailout)": "test"},
    # {"Manually claim ROGUE coins for game (highlander)": "test"},
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
            if menu_items_counter == 0:
                print("\nUtility:\n")
            menu_items_counter = menu_items_counter + 1
            print(tuilib.colorize("[" + str(menuItems.index(item)) + "] ", 'blue') + list(item.keys())[0])
            if menu_items_counter == 6:
                print("\nNew singleplayer game:\n")
            if menu_items_counter == 7:
                print("\nMultiplayer games:\n")
            if menu_items_counter == 10:
                print("\nDEX features:\n")
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
        chain = "ROGUE"
        try:
            print(tuilib.colorize("Welcome to the RogueCC TUI!\n"
                                  "Please provide asset chain RPC connection details for initialization", "blue"))
            rpc_connection = tuilib.def_credentials(chain)
            rpclib.getinfo(rpc_connection)
            # waiting until chain is in sync
            while True:
                have_blocks = rpclib.getinfo(rpc_connection)["blocks"]
                longest_chain = rpclib.getinfo(rpc_connection)["longestchain"]
                if have_blocks != longest_chain:
                    print(tuilib.colorize("ROGUE not synced yet.", "red"))
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
            tuilib.check_if_config_is_here(rpc_connection, "ROGUE")
        except Exception:
            print(tuilib.colorize("Cant connect to ROGUE daemon RPC! Please check if daemon is up.", "pink"))
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
