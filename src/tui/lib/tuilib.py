from lib import rpclib
import json
import time
import re
import sys
import pickle
import platform
import os
import subprocess
import random
import signal
from slickrpc import Proxy
from binascii import hexlify
from binascii import unhexlify
from functools import partial
from shutil import copy


operating_system = platform.system()
if operating_system != 'Win64' and operating_system != 'Windows':
    import readline


def colorize(string, color):

    colors = {
        'blue': '\033[94m',
        'magenta': '\033[95m',
        'green': '\033[92m',
        'red': '\033[91m'
    }
    if color not in colors:
        return string
    else:
        return colors[color] + string + '\033[0m'


def rpc_connection_tui():
    # TODO: possible to save multiply entries from successfull sessions and ask user to choose then
    while True:
        restore_choice = input("Do you want to use connection details from previous session? [y/n]: ")
        if restore_choice == "y":
            try:
                with open("connection.json", "r") as file:
                    connection_json = json.load(file)
                    rpc_user = connection_json["rpc_user"]
                    rpc_password = connection_json["rpc_password"]
                    rpc_port = connection_json["rpc_port"]
                    rpc_connection = rpclib.rpc_connect(rpc_user, rpc_password, int(rpc_port))
            except FileNotFoundError:
                print(colorize("You do not have cached connection details. Please select n for connection setup", "red"))
            break
        elif restore_choice == "n":
            rpc_user = input("Input your rpc user: ")
            rpc_password = input("Input your rpc password: ")
            rpc_port = input("Input your rpc port: ")
            connection_details = {"rpc_user": rpc_user,
                                  "rpc_password": rpc_password,
                                  "rpc_port": rpc_port}
            connection_json = json.dumps(connection_details)
            with open("connection.json", "w+") as file:
                file.write(connection_json)
            rpc_connection = rpclib.rpc_connect(rpc_user, rpc_password, int(rpc_port))
            break
        else:
            print(colorize("Please input y or n", "red"))
    return rpc_connection


def def_credentials(chain):
    rpcport ='';
    operating_system = platform.system()
    if operating_system == 'Darwin':
        ac_dir = os.environ['HOME'] + '/Library/Application Support/Komodo'
    elif operating_system == 'Linux':
        ac_dir = os.environ['HOME'] + '/.komodo'
    elif operating_system == 'Win64' or operating_system == 'Windows':
        ac_dir = '%s/komodo/' % os.environ['APPDATA']
    if chain == 'KMD':
        coin_config_file = str(ac_dir + '/komodo.conf')
    else:
        coin_config_file = str(ac_dir + '/' + chain + '/' + chain + '.conf')
    with open(coin_config_file, 'r') as f:
        for line in f:
            l = line.rstrip()
            if re.search('rpcuser', l):
                rpcuser = l.replace('rpcuser=', '')
            elif re.search('rpcpassword', l):
                rpcpassword = l.replace('rpcpassword=', '')
            elif re.search('rpcport', l):
                rpcport = l.replace('rpcport=', '')
    if len(rpcport) == 0:
        if chain == 'KMD':
            rpcport = 7771
        else:
            print("rpcport not in conf file, exiting")
            print("check "+coin_config_file)
            exit(1)

    return(Proxy("http://%s:%s@127.0.0.1:%d"%(rpcuser, rpcpassword, int(rpcport))))


def getinfo_tui(rpc_connection):

    info_raw = rpclib.getinfo(rpc_connection)
    if isinstance(info_raw, dict):
        for key in info_raw:
            print("{}: {}".format(key, info_raw[key]))
        input("Press [Enter] to continue...")
    else:
        print("Error!\n")
        print(info_raw)
        input("\nPress [Enter] to continue...")


def token_create_tui(rpc_connection):

    while True:
        try:
            name = input("Set your token name: ")
            supply = input("Set your token supply: ")
            description = input("Set your token description: ")
        except KeyboardInterrupt:
            break
        else:
            token_hex = rpclib.token_create(rpc_connection, name, supply, description)
        if token_hex['result'] == "error":
            print(colorize("\nSomething went wrong!\n", "pink"))
            print(token_hex)
            print("\n")
            input("Press [Enter] to continue...")
            break
        else:
            try:
                token_txid = rpclib.sendrawtransaction(rpc_connection,
                                                       token_hex['hex'])
            except KeyError:
                print(token_txid)
                print("Error")
                input("Press [Enter] to continue...")
                break
            finally:
                print(colorize("Token creation transaction broadcasted: " + token_txid, "green"))
                file = open("tokens_list", "a")
                file.writelines(token_txid + "\n")
                file.close()
                print(colorize("Entry added to tokens_list file!\n", "green"))
                input("Press [Enter] to continue...")
                break


def oracle_create_tui(rpc_connection):

    print(colorize("\nAvailiable data types:\n", "blue"))
    oracles_data_types = ["Ihh -> height, blockhash, merkleroot\ns -> <256 char string\nS -> <65536 char string\nd -> <256 binary data\nD -> <65536 binary data",
                "c -> 1 byte signed little endian number, C unsigned\nt -> 2 byte signed little endian number, T unsigned",
                "i -> 4 byte signed little endian number, I unsigned\nl -> 8 byte signed little endian number, L unsigned",
                "h -> 32 byte hash\n"]
    for oracles_type in oracles_data_types:
        print(str(oracles_type))
    while True:
        try:
            name = input("Set your oracle name: ")
            description = input("Set your oracle description: ")
            oracle_data_type = input("Set your oracle type (e.g. Ihh): ")
        except KeyboardInterrupt:
            break
        else:
            oracle_hex = rpclib.oracles_create(rpc_connection, name, description, oracle_data_type)
        if oracle_hex['result'] == "error":
            print(colorize("\nSomething went wrong!\n", "pink"))
            print(oracle_hex)
            print("\n")
            input("Press [Enter] to continue...")
            break
        else:
            try:
                oracle_txid = rpclib.sendrawtransaction(rpc_connection, oracle_hex['hex'])
            except KeyError:
                print(oracle_txid)
                print("Error")
                input("Press [Enter] to continue...")
                break
            finally:
                print(colorize("Oracle creation transaction broadcasted: " + oracle_txid, "green"))
                file = open("oracles_list", "a")
                file.writelines(oracle_txid + "\n")
                file.close()
                print(colorize("Entry added to oracles_list file!\n", "green"))
                input("Press [Enter] to continue...")
                break


def oracle_register_tui(rpc_connection):
    #TODO: have an idea since blackjoker new RPC call
    #grab all list and printout only or which owner match with node pubkey
    try:
        print(colorize("Oracles created from this instance by TUI: \n", "blue"))
        with open("oracles_list", "r") as file:
            for oracle in file:
                print(oracle)
        print(colorize('_' * 65, "blue"))
        print("\n")
    except FileNotFoundError:
        print("Seems like a no oracles created from this instance yet\n")
        pass
    while True:
        try:
            oracle_id = input("Input txid of oracle you want to register to: ")
            data_fee = input("Set publisher datafee (in satoshis): ")
        except KeyboardInterrupt:
            break
        oracle_register_hex = rpclib.oracles_register(rpc_connection, oracle_id, data_fee)
        if oracle_register_hex['result'] == "error":
            print(colorize("\nSomething went wrong!\n", "pink"))
            print(oracle_register_hex)
            print("\n")
            input("Press [Enter] to continue...")
            break
        else:
            try:
                oracle_register_txid = rpclib.sendrawtransaction(rpc_connection, oracle_register_hex['hex'])
            except KeyError:
                print(oracle_register_hex)
                print("Error")
                input("Press [Enter] to continue...")
                break
            else:
                print(colorize("Oracle registration transaction broadcasted: " + oracle_register_txid, "green"))
                input("Press [Enter] to continue...")
                break


def oracle_subscription_utxogen(rpc_connection):
    # TODO: have an idea since blackjoker new RPC call
    # grab all list and printout only or which owner match with node pubkey
    try:
        print(colorize("Oracles created from this instance by TUI: \n", "blue"))
        with open("oracles_list", "r") as file:
            for oracle in file:
                print(oracle)
        print(colorize('_' * 65, "blue"))
        print("\n")
    except FileNotFoundError:
        print("Seems like a no oracles created from this instance yet\n")
        pass
    while True:
        try:
            oracle_id = input("Input oracle ID you want to subscribe to: ")
            #printout to fast copypaste publisher id
            oracle_info = rpclib.oracles_info(rpc_connection, oracle_id)
            publishers = 0
            print(colorize("\nPublishers registered for a selected oracle: \n", "blue"))
            try:
                for entry in oracle_info["registered"]:
                    publisher = entry["publisher"]
                    print(publisher + "\n")
                    publishers = publishers + 1
                print("Total publishers:{}".format(publishers))
            except (KeyError, ConnectionResetError):
                print(colorize("Please re-check your input. Oracle txid seems not valid.", "red"))
                pass
            print(colorize('_' * 65, "blue"))
            print("\n")
            if publishers == 0:
                print(colorize("This oracle have no publishers to subscribe.\n"
                               "Please register as an oracle publisher first and/or wait since registration transaciton mined!", "red"))
                input("Press [Enter] to continue...")
                break
            publisher_id = input("Input oracle publisher id you want to subscribe to: ")
            data_fee = input("Input subscription fee (in COINS!): ")
            utxo_num = int(input("Input how many transactions you want to broadcast: "))
        except KeyboardInterrupt:
            break
        while utxo_num > 0:
            while True:
                oracle_subscription_hex = rpclib.oracles_subscribe(rpc_connection, oracle_id, publisher_id, data_fee)
                oracle_subscription_txid = rpclib.sendrawtransaction(rpc_connection, oracle_subscription_hex['hex'])
                mempool = rpclib.get_rawmempool(rpc_connection)
                if oracle_subscription_txid in mempool:
                    break
                else:
                    pass
            print(colorize("Oracle subscription transaction broadcasted: " + oracle_subscription_txid, "green"))
            utxo_num = utxo_num - 1
        input("Press [Enter] to continue...")
        break

def gateways_bind_tui(rpc_connection):
    # main loop with keyboard interrupt handling
    while True:
        try:
            while True:
                try:
                    print(colorize("Tokens created from this instance by TUI: \n", "blue"))
                    with open("tokens_list", "r") as file:
                        for oracle in file:
                            print(oracle)
                    print(colorize('_' * 65, "blue"))
                    print("\n")
                except FileNotFoundError:
                    print("Seems like a no oracles created from this instance yet\n")
                    pass
                token_id = input("Input id of token you want to use in gw bind: ")
                try:
                    token_name = rpclib.token_info(rpc_connection, token_id)["name"]
                except KeyError:
                    print(colorize("Not valid tokenid. Please try again.", "red"))
                    input("Press [Enter] to continue...")
                token_info = rpclib.token_info(rpc_connection, token_id)
                print(colorize("\n{} token total supply: {}\n".format(token_id, token_info["supply"]), "blue"))
                token_supply = input("Input supply for token binding: ")
                try:
                    print(colorize("\nOracles created from this instance by TUI: \n", "blue"))
                    with open("oracles_list", "r") as file:
                        for oracle in file:
                            print(oracle)
                    print(colorize('_' * 65, "blue"))
                    print("\n")
                except FileNotFoundError:
                    print("Seems like a no oracles created from this instance yet\n")
                    pass
                oracle_id = input("Input id of oracle you want to use in gw bind: ")
                try:
                    oracle_name = rpclib.oracles_info(rpc_connection, oracle_id)["name"]
                except KeyError:
                    print(colorize("Not valid oracleid. Please try again.", "red"))
                    input("Press [Enter] to continue...")
                while True:
                    coin_name = input("Input external coin ticker (binded oracle and token need to have same name!): ")
                    if token_name == oracle_name and token_name == coin_name:
                        break
                    else:
                        print(colorize("Token name, oracle name and external coin ticker should match!", "red"))
                while True:
                    M = input("Input minimal amount of pubkeys needed for transaction confirmation (1 for non-multisig gw): ")
                    N = input("Input maximal amount of pubkeys needed for transaction confirmation (1 for non-multisig gw): ")
                    if (int(N) >= int(M)):
                        break
                    else:
                        print("Maximal amount of pubkeys should be more or equal than minimal. Please try again.")
                pubkeys = []
                for i in range(int(N)):
                    pubkeys.append(input("Input pubkey {}: ".format(i+1)))
                pubtype = input("Input pubtype of external coin: ")
                p2shtype = input("Input p2shtype of external coin: ")
                wiftype = input("Input wiftype of external coin: ")
                args = [rpc_connection, token_id, oracle_id, coin_name, token_supply, M, N]
                new_args = [str(pubtype), str(p2shtype), wiftype]
                args = args + pubkeys + new_args
                # broadcasting block
                try:
                    gateways_bind_hex = rpclib.gateways_bind(*args)
                except Exception as e:
                    print(e)
                    input("Press [Enter] to continue...")
                    break
                try:
                    gateways_bind_txid = rpclib.sendrawtransaction(rpc_connection, gateways_bind_hex["hex"])
                except Exception as e:
                    print(e)
                    print(gateways_bind_hex)
                    input("Press [Enter] to continue...")
                    break
                else:
                    print(colorize("Gateway bind transaction broadcasted: " + gateways_bind_txid, "green"))
                    file = open("gateways_list", "a")
                    file.writelines(gateways_bind_txid + "\n")
                    file.close()
                    print(colorize("Entry added to gateways_list file!\n", "green"))
                    input("Press [Enter] to continue...")
                    break
            break
        except KeyboardInterrupt:
            break

# temporary :trollface: custom connection function solution
# to have connection to KMD daemon and cache it in separate file


def rpc_kmd_connection_tui():
    while True:
        restore_choice = input("Do you want to use KMD daemon connection details from previous session? [y/n]: ")
        if restore_choice == "y":
            try:
                with open("connection_kmd.json", "r") as file:
                    connection_json = json.load(file)
                    rpc_user = connection_json["rpc_user"]
                    rpc_password = connection_json["rpc_password"]
                    rpc_port = connection_json["rpc_port"]
                    rpc_connection_kmd = rpclib.rpc_connect(rpc_user, rpc_password, int(rpc_port))
                    try:
                        print(rpc_connection_kmd.getinfo())
                        print(colorize("Successfully connected!\n", "green"))
                        input("Press [Enter] to continue...")
                        break
                    except Exception as e:
                        print(e)
                        print(colorize("NOT CONNECTED!\n", "red"))
                        input("Press [Enter] to continue...")
                        break
            except FileNotFoundError:
                print(colorize("You do not have cached KMD daemon connection details."
                               " Please select n for connection setup", "red"))
                input("Press [Enter] to continue...")
        elif restore_choice == "n":
            rpc_user = input("Input your rpc user: ")
            rpc_password = input("Input your rpc password: ")
            rpc_port = input("Input your rpc port: ")
            connection_details = {"rpc_user": rpc_user,
                                  "rpc_password": rpc_password,
                                  "rpc_port": rpc_port}
            connection_json = json.dumps(connection_details)
            with open("connection_kmd.json", "w+") as file:
                file.write(connection_json)
            rpc_connection_kmd = rpclib.rpc_connect(rpc_user, rpc_password, int(rpc_port))
            try:
                print(rpc_connection_kmd.getinfo())
                print(colorize("Successfully connected!\n", "green"))
                input("Press [Enter] to continue...")
                break
            except Exception as e:
                print(e)
                print(colorize("NOT CONNECTED!\n", "red"))
                input("Press [Enter] to continue...")
                break
        else:
            print(colorize("Please input y or n", "red"))
    return rpc_connection_kmd


def z_sendmany_twoaddresses(rpc_connection, sendaddress, recepient1, amount1, recepient2, amount2):
    str_sending_block = "[{{\"address\":\"{}\",\"amount\":{}}},{{\"address\":\"{}\",\"amount\":{}}}]".format(recepient1, amount1, recepient2, amount2)
    sending_block = json.loads(str_sending_block)
    operation_id = rpc_connection.z_sendmany(sendaddress,sending_block)
    return operation_id


def operationstatus_to_txid(rpc_connection, zstatus):
    str_sending_block = "[\"{}\"]".format(zstatus)
    sending_block = json.loads(str_sending_block)
    operation_json = rpc_connection.z_getoperationstatus(sending_block)
    operation_dump = json.dumps(operation_json)
    operation_dict = json.loads(operation_dump)[0]
    txid = operation_dict['result']['txid']
    return txid


def gateways_send_kmd(rpc_connection):
     # TODO: have to handle CTRL+C on text input
     print(colorize("Please be carefull when input wallet addresses and amounts since all transactions doing in real KMD!", "pink"))
     print("Your addresses with balances: ")
     list_address_groupings = rpc_connection.listaddressgroupings()
     for address in list_address_groupings:
         print(str(address) + "\n")
     sendaddress = input("Input address from which you transfer KMD: ")
     recepient1 = input("Input address which belongs to pubkey which will receive tokens: ")
     amount1 = 0.0001
     recepient2 = input("Input gateway deposit address: ")
     file = open("deposits_list", "a")
     #have to show here deposit addresses for gateways created by user
     amount2 = input("Input how many KMD you want to deposit on this gateway: ")
     operation = z_sendmany_twoaddresses(rpc_connection, sendaddress, recepient1, amount1, recepient2, amount2)
     print("Operation proceed! " + str(operation) + " Let's wait 2 seconds to get txid")
     # trying to avoid pending status of operation
     time.sleep(2)
     txid = operationstatus_to_txid(rpc_connection, operation)
     file.writelines(txid + "\n")
     file.close()
     print(colorize("KMD Transaction ID: " + str(txid) + " Entry added to deposits_list file", "green"))
     input("Press [Enter] to continue...")


def gateways_deposit_tui(rpc_connection_assetchain, rpc_connection_komodo):
    while True:
        bind_txid = input("Input your gateway bind txid: ")
        coin_name = input("Input your external coin ticker (e.g. KMD): ")
        coin_txid = input("Input your deposit txid: ")
        dest_pub = input("Input pubkey which claim deposit: ")
        amount = input("Input amount of your deposit: ")
        height = rpc_connection_komodo.getrawtransaction(coin_txid, 1)["height"]
        deposit_hex = rpc_connection_komodo.getrawtransaction(coin_txid, 1)["hex"]
        claim_vout = "0"
        proof_sending_block = "[\"{}\"]".format(coin_txid)
        proof = rpc_connection_komodo.gettxoutproof(json.loads(proof_sending_block))
        deposit_hex = rpclib.gateways_deposit(rpc_connection_assetchain, bind_txid, str(height), coin_name, \
                         coin_txid, claim_vout, deposit_hex, proof, dest_pub, amount)
        print(deposit_hex)
        deposit_txid = rpclib.sendrawtransaction(rpc_connection_assetchain, deposit_hex["hex"])
        print("Done! Gateways deposit txid is: " + deposit_txid + " Please not forget to claim your deposit!")
        input("Press [Enter] to continue...")
        break


def gateways_claim_tui(rpc_connection):
    while True:
        bind_txid = input("Input your gateway bind txid: ")
        coin_name = input("Input your external coin ticker (e.g. KMD): ")
        deposit_txid = input("Input your gatewaysdeposit txid: ")
        dest_pub = input("Input pubkey which claim deposit: ")
        amount = input("Input amount of your deposit: ")
        claim_hex = rpclib.gateways_claim(rpc_connection, bind_txid, coin_name, deposit_txid, dest_pub, amount)
        try:
            claim_txid = rpclib.sendrawtransaction(rpc_connection, claim_hex["hex"])
        except Exception as e:
            print(e)
            print(claim_hex)
            input("Press [Enter] to continue...")
            break
        else:
            print("Succesfully claimed! Claim transaction id: " + claim_txid)
            input("Press [Enter] to continue...")
            break


def gateways_withdrawal_tui(rpc_connection):
    while True:
        bind_txid = input("Input your gateway bind txid: ")
        coin_name = input("Input your external coin ticker (e.g. KMD): ")
        withdraw_pub = input("Input pubkey to which you want to withdraw: ")
        amount = input("Input amount of withdrawal: ")
        withdraw_hex = rpclib.gateways_withdraw(rpc_connection, bind_txid, coin_name, withdraw_pub, amount)
        withdraw_txid = rpclib.sendrawtransaction(rpc_connection, withdraw_hex["hex"])
        print(withdraw_txid)
        input("Press [Enter] to continue...")
        break


def print_mempool(rpc_connection):
    while True:
        mempool = rpclib.get_rawmempool(rpc_connection)
        tx_counter = 0
        print(colorize("Transactions in mempool: \n", "magenta"))
        for transaction in mempool:
            print(transaction + "\n")
            tx_counter = tx_counter + 1
        print("Total: " + str(tx_counter) + " transactions\n")
        print("R + Enter to refresh list. E + Enter to exit menu." + "\n")
        is_refresh = input("Choose your destiny: ")
        if is_refresh == "R":
            print("\n")
            pass
        elif is_refresh == "E":
            print("\n")
            break
        else:
            print("\nPlease choose R or E\n")


def print_tokens_list(rpc_connection):
    # TODO: have to print it with tokeninfo to have sense
    pass


def print_tokens_balances(rpc_connection):
    # TODO: checking tokenbalance for each token from tokenlist and reflect non zero ones
    pass


def hexdump(filename, chunk_size=1<<15):
    data = ""
    #add_spaces = partial(re.compile(b'(..)').sub, br'\1 ')
    #write = getattr(sys.stdout, 'buffer', sys.stdout).write
    with open(filename, 'rb') as file:
        for chunk in iter(partial(file.read, chunk_size), b''):
            data += str(hexlify(chunk).decode())
    return data


def convert_file_oracle_d(rpc_connection):
    while True:
        path = input("Input path to file you want to upload to oracle: ")
        try:
            hex_data = (hexdump(path, 1))[2:]
        except Exception as e:
            print(e)
            print("Seems something goes wrong (I guess you've specified wrong path)!")
            input("Press [Enter] to continue...")
            break
        else:
            length = round(len(hex_data) / 2)
            if length > 256:
                print("Length: " + str(length) + " bytes")
                print("File is too big for this app")
                input("Press [Enter] to continue...")
                break
            else:
                hex_length = format(length, '#04x')[2:]
                data_for_oracle = str(hex_length) + hex_data
                print("File hex representation: \n")
                print(data_for_oracle + "\n")
                print("Length: " + str(length) + " bytes")
                print("File converted!")
                new_oracle_hex = rpclib.oracles_create(rpc_connection, "tonyconvert", path, "d")
                new_oracle_txid = rpclib.sendrawtransaction(rpc_connection, new_oracle_hex["hex"])
                time.sleep(0.5)
                oracle_register_hex = rpclib.oracles_register(rpc_connection, new_oracle_txid, "10000")
                oracle_register_txid = rpclib.sendrawtransaction(rpc_connection, oracle_register_hex["hex"])
                time.sleep(0.5)
                oracle_subscribe_hex = rpclib.oracles_subscribe(rpc_connection, new_oracle_txid, rpclib.getinfo(rpc_connection)["pubkey"], "0.001")
                oracle_subscribe_txid = rpclib.sendrawtransaction(rpc_connection, oracle_subscribe_hex["hex"])
                time.sleep(0.5)
                while True:
                    mempool = rpclib.get_rawmempool(rpc_connection)
                    if oracle_subscribe_txid in mempool:
                        print("Waiting for oracle subscribtion tx to be mined" + "\n")
                        time.sleep(6)
                        pass
                    else:
                        break
                oracles_data_hex = rpclib.oracles_data(rpc_connection, new_oracle_txid, data_for_oracle)
                try:
                    oracle_data_txid = rpclib.sendrawtransaction(rpc_connection, oracles_data_hex["hex"])
                except Exception as e:
                    print(oracles_data_hex)
                    print(e)
                print("Oracle created: " + str(new_oracle_txid))
                print("Data published: " + str(oracle_data_txid))
                input("Press [Enter] to continue...")
                break


def convert_file_oracle_D(rpc_connection):
    while True:
        path = input("Input path to file you want to upload to oracle: ")
        try:
            hex_data = (hexdump(path, 1))
        except Exception as e:
            print(e)
            print("Seems something goes wrong (I guess you've specified wrong path)!")
            input("Press [Enter] to continue...")
            break
        else:
            length = round(len(hex_data) / 2)
            # if length > 800000:
            #     print("Too big file size to upload for this version of program. Maximum size is 800KB.")
            #     input("Press [Enter] to continue...")
            #     break
            if length > 8000:
                # if file is more than 8000 bytes - slicing it to <= 8000 bytes chunks (16000 symbols = 8000 bytes)
                data = [hex_data[i:i + 16000] for i in range(0, len(hex_data), 16000)]
                chunks_amount = len(data)
                # TODO: have to create oracle but subscribe this time chunks amount times to send whole file in same block
                # TODO: 2 - on some point file will not fit block - have to find this point
                # TODO: 3 way how I want to implement it first will keep whole file in RAM - have to implement some way to stream chunks to oracle before whole file readed
                # TODO: have to "optimise" registration fee
                # Maybe just check size first by something like a du ?
                print("Length: " + str(length) + " bytes.\n Chunks amount: " + str(chunks_amount))
                new_oracle_hex = rpclib.oracles_create(rpc_connection, "tonyconvert_" + str(chunks_amount), path, "D")
                new_oracle_txid = rpclib.sendrawtransaction(rpc_connection, new_oracle_hex["hex"])
                time.sleep(0.5)
                oracle_register_hex = rpclib.oracles_register(rpc_connection, new_oracle_txid, "10000")
                oracle_register_txid = rpclib.sendrawtransaction(rpc_connection, oracle_register_hex["hex"])
                # subscribe chunks_amount + 1 times, but lets limit our broadcasting 100 tx per block (800KB/block)
                if chunks_amount > 100:
                    utxo_num = 101
                else:
                    utxo_num = chunks_amount
                while utxo_num > 0:
                    while True:
                        oracle_subscription_hex = rpclib.oracles_subscribe(rpc_connection, new_oracle_txid, rpclib.getinfo(rpc_connection)["pubkey"], "0.001")
                        oracle_subscription_txid = rpclib.sendrawtransaction(rpc_connection,
                                                                             oracle_subscription_hex['hex'])
                        mempool = rpclib.get_rawmempool(rpc_connection)
                        if oracle_subscription_txid in mempool:
                            break
                        else:
                            pass
                    print(colorize("Oracle subscription transaction broadcasted: " + oracle_subscription_txid, "green"))
                    utxo_num = utxo_num - 1
                # waiting for last broadcasted subscribtion transaction to be mined to be sure that money are on oracle balance
                while True:
                    mempool = rpclib.get_rawmempool(rpc_connection)
                    if oracle_subscription_txid in mempool:
                        print("Waiting for oracle subscribtion tx to be mined" + "\n")
                        time.sleep(6)
                        pass
                    else:
                        break
                print("Oracle preparation is finished. Oracle txid: " + new_oracle_txid)
                # can publish data now
                counter = 0
                for chunk in data:
                    hex_length_bigendian = format(round(len(chunk) / 2), '#06x')[2:]
                    # swap to get little endian length
                    a = hex_length_bigendian[2:]
                    b = hex_length_bigendian[:2]
                    hex_length = a + b
                    data_for_oracle = str(hex_length) + chunk
                    counter = counter + 1
                    # print("Chunk number: " + str(counter) + "\n")
                    # print(data_for_oracle)
                    try:
                        oracles_data_hex = rpclib.oracles_data(rpc_connection, new_oracle_txid, data_for_oracle)
                    except Exception as e:
                        print(data_for_oracle)
                        print(e)
                        input("Press [Enter] to continue...")
                        break
                    # on broadcasting ensuring that previous one reached mempool before blast next one
                    while True:
                        mempool = rpclib.get_rawmempool(rpc_connection)
                        oracle_data_txid = rpclib.sendrawtransaction(rpc_connection, oracles_data_hex["hex"])
                        #time.sleep(0.1)
                        if oracle_data_txid in mempool:
                            break
                        else:
                            pass
                    # blasting not more than 100 at once (so maximum capacity per block can be changed here)
                    # but keep in mind that registration UTXOs amount needs to be changed too !
                    if counter % 100 == 0 and chunks_amount > 100:
                        while True:
                            mempool = rpclib.get_rawmempool(rpc_connection)
                            if oracle_data_txid in mempool:
                                print("Waiting for previous data chunks to be mined before send new ones" + "\n")
                                print("Sent " + str(counter) + " chunks from " + str(chunks_amount))
                                time.sleep(6)
                                pass
                            else:
                                break

                print("Last baton: " + oracle_data_txid)
                input("Press [Enter] to continue...")
                break
            # if file suits single oraclesdata just broadcasting it straight without any slicing
            else:
                hex_length_bigendian = format(length, '#06x')[2:]
                # swap to get little endian length
                a = hex_length_bigendian[2:]
                b = hex_length_bigendian[:2]
                hex_length = a + b
                data_for_oracle = str(hex_length) + hex_data
                print("File hex representation: \n")
                print(data_for_oracle + "\n")
                print("Length: " + str(length) + " bytes")
                print("File converted!")
                new_oracle_hex = rpclib.oracles_create(rpc_connection, "tonyconvert_" + "1", path, "D")
                new_oracle_txid = rpclib.sendrawtransaction(rpc_connection, new_oracle_hex["hex"])
                time.sleep(0.5)
                oracle_register_hex = rpclib.oracles_register(rpc_connection, new_oracle_txid, "10000")
                oracle_register_txid = rpclib.sendrawtransaction(rpc_connection, oracle_register_hex["hex"])
                time.sleep(0.5)
                oracle_subscribe_hex = rpclib.oracles_subscribe(rpc_connection, new_oracle_txid, rpclib.getinfo(rpc_connection)["pubkey"], "0.001")
                oracle_subscribe_txid = rpclib.sendrawtransaction(rpc_connection, oracle_subscribe_hex["hex"])
                time.sleep(0.5)
                while True:
                    mempool = rpclib.get_rawmempool(rpc_connection)
                    if oracle_subscribe_txid in mempool:
                        print("Waiting for oracle subscribtion tx to be mined" + "\n")
                        time.sleep(6)
                        pass
                    else:
                        break
                oracles_data_hex = rpclib.oracles_data(rpc_connection, new_oracle_txid, data_for_oracle)
                try:
                    oracle_data_txid = rpclib.sendrawtransaction(rpc_connection, oracles_data_hex["hex"])
                except Exception as e:
                    print(oracles_data_hex)
                    print(e)
                    input("Press [Enter] to continue...")
                    break
                else:
                    print("Oracle created: " + str(new_oracle_txid))
                    print("Data published: " + str(oracle_data_txid))
                    input("Press [Enter] to continue...")
                    break


def get_files_list(rpc_connection):

    start_time = time.time()
    oracles_list = rpclib.oracles_list(rpc_connection)
    files_list = []
    for oracle_txid in oracles_list:
        oraclesinfo_result = rpclib.oracles_info(rpc_connection, oracle_txid)
        description = oraclesinfo_result['description']
        name = oraclesinfo_result['name']
        if name[0:12] == 'tonyconvert_':
            new_file = '[' + name + ': ' + description + ']: ' + oracle_txid
            files_list.append(new_file)
    print("--- %s seconds ---" % (time.time() - start_time))
    return files_list


def display_files_list(rpc_connection):
    print("Scanning oracles. Please wait...")
    list_to_display = get_files_list(rpc_connection)
    while True:
        for file in list_to_display:
            print(file + "\n")
        input("Press [Enter] to continue...")
        break


def files_downloader(rpc_connection):
    while True:
        display_files_list(rpc_connection)
        print("\n")
        oracle_id = input("Input oracle ID you want to download file from: ")
        output_path = input("Input output path for downloaded file (name included) e.g. /home/test.txt: ")
        oracle_info = rpclib.oracles_info(rpc_connection, oracle_id)
        name = oracle_info['name']
        latest_baton_txid = oracle_info['registered'][0]['batontxid']
        if name[0:12] == 'tonyconvert_':
            # downloading process here
            chunks_amount = int(name[12:])
            data = rpclib.oracles_samples(rpc_connection, oracle_id, latest_baton_txid, str(chunks_amount))["samples"]
            for chunk in reversed(data):
                with open(output_path, 'ab+') as file:
                    file.write(unhexlify(chunk[0]))
            print("I hope that file saved to " + output_path + "\n")
            input("Press [Enter] to continue...")
            break

        else:
            print("I cant recognize file inside this oracle. I'm very sorry, boss.")
            input("Press [Enter] to continue...")
            break


def marmara_receive_tui(rpc_connection):
    while True:
        issuer_pubkey = input("Input pubkey of person who do you want to receive MARMARA from: ")
        issuance_sum = input("Input amount of MARMARA you want to receive: ")
        blocks_valid = input("Input amount of blocks for cheque matures: ")
        try:
            marmara_receive_txinfo = rpc_connection.marmarareceive(issuer_pubkey, issuance_sum, "MARMARA", blocks_valid)
            marmara_receive_txid = rpc_connection.sendrawtransaction(marmara_receive_txinfo["hex"])
            print("Marmara receive txid broadcasted: " + marmara_receive_txid + "\n")
            print(json.dumps(marmara_receive_txinfo, indent=4, sort_keys=True) + "\n")
            with open("receive_txids.txt", 'a+') as file:
                file.write(marmara_receive_txid + "\n")
                file.write(json.dumps(marmara_receive_txinfo, indent=4, sort_keys=True) + "\n")
            print("Transaction id is saved to receive_txids.txt file.")
            input("Press [Enter] to continue...")
            break
        except Exception as e:
            print(marmara_receive_txinfo)
            print(e)
            print("Something went wrong. Please check your input")


def marmara_issue_tui(rpc_connection):
    while True:
        receiver_pubkey = input("Input pubkey of person who do you want to issue MARMARA: ")
        issuance_sum = input("Input amount of MARMARA you want to issue: ")
        maturing_block = input("Input number of block on which issuance mature: ")
        approval_txid = input("Input receiving request transaction id: ")
        try:
            marmara_issue_txinfo = rpc_connection.marmaraissue(receiver_pubkey, issuance_sum, "MARMARA", maturing_block, approval_txid)
            marmara_issue_txid = rpc_connection.sendrawtransaction(marmara_issue_txinfo["hex"])
            print("Marmara issuance txid broadcasted: " + marmara_issue_txid + "\n")
            print(json.dumps(marmara_issue_txinfo, indent=4, sort_keys=True) + "\n")
            with open("issue_txids.txt", "a+") as file:
                file.write(marmara_issue_txid + "\n")
                file.write(json.dumps(marmara_issue_txinfo, indent=4, sort_keys=True) + "\n")
            print("Transaction id is saved to issue_txids.txt file.")
            input("Press [Enter] to continue...")
            break
        except Exception as e:
            print(marmara_issue_txinfo)
            print(e)
            print("Something went wrong. Please check your input")


def marmara_creditloop_tui(rpc_connection):
    while True:
        loop_txid = input("Input transaction ID of credit loop you want to get info about: ")
        try:
            marmara_creditloop_info = rpc_connection.marmaracreditloop(loop_txid)
            print(json.dumps(marmara_creditloop_info, indent=4, sort_keys=True) + "\n")
            input("Press [Enter] to continue...")
            break
        except Exception as e:
            print(marmara_creditloop_info)
            print(e)
            print("Something went wrong. Please check your input")


def marmara_settlement_tui(rpc_connection):
    while True:
        loop_txid = input("Input transaction ID of credit loop to make settlement: ")
        try:
            marmara_settlement_info = rpc_connection.marmarasettlement(loop_txid)
            marmara_settlement_txid = rpc_connection.sendrawtransaction(marmara_settlement_info["hex"])
            print("Loop " + loop_txid + " succesfully settled!\nSettlement txid: " + marmara_settlement_txid)
            with open("settlement_txids.txt", "a+") as file:
                file.write(marmara_settlement_txid + "\n")
                file.write(json.dumps(marmara_settlement_info, indent=4, sort_keys=True) + "\n")
            print("Transaction id is saved to settlement_txids.txt file.")
            input("Press [Enter] to continue...")
            break
        except Exception as e:
            print(marmara_settlement_info)
            print(e)
            print("Something went wrong. Please check your input")
            input("Press [Enter] to continue...")
            break


def marmara_lock_tui(rpc_connection):
    while True:
        amount = input("Input amount of coins you want to lock for settlement and staking: ")
        unlock_height = input("Input height on which coins should be unlocked: ")
        try:
            marmara_lock_info = rpc_connection.marmaralock(amount, unlock_height)
            marmara_lock_txid = rpc_connection.sendrawtransaction(marmara_lock_info["hex"])
            with open("lock_txids.txt", "a+") as file:
                file.write(marmara_lock_txid + "\n")
                file.write(json.dumps(marmara_lock_info, indent=4, sort_keys=True) + "\n")
            print("Transaction id is saved to lock_txids.txt file.")
            input("Press [Enter] to continue...")
            break
        except Exception as e:
            print(e)
            print("Something went wrong. Please check your input")
            input("Press [Enter] to continue...")
            break


def marmara_info_tui(rpc_connection):
    while True:
        firstheight = input("Input first height (default 0): ")
        if not firstheight:
            firstheight = "0"
        lastheight = input("Input last height (default current (0) ): ")
        if not lastheight:
            lastheight = "0"
        minamount = input("Input min amount (default 0): ")
        if not minamount:
            minamount = "0"
        maxamount = input("Input max amount (default 0): ")
        if not maxamount:
            maxamount = "0"
        issuerpk = input("Optional. Input issuer public key: ")
        try:
            if issuerpk:
                marmara_info = rpc_connection.marmarainfo(firstheight, lastheight, minamount, maxamount, "MARMARA", issuerpk)
            else:
                marmara_info = rpc_connection.marmarainfo(firstheight, lastheight, minamount, maxamount)
            print(json.dumps(marmara_info, indent=4, sort_keys=True) + "\n")
            input("Press [Enter] to continue...")
            break
        except Exception as e:
            print(marmara_info)
            print(e)
            print("Something went wrong. Please check your input")
            input("Press [Enter] to continue...")
            break


def rogue_game_info(rpc_connection, game_txid):
    game_info_arg = '"' + "[%22" + game_txid + "%22]" + '"'
    game_info = rpc_connection.cclib("gameinfo", "17", game_info_arg)
    return game_info


def rogue_game_register(rpc_connection, game_txid, player_txid = False):
    if player_txid:
        registration_info_arg = '"' + "[%22" + game_txid + "%22,%22" + player_txid + "%22]" + '"'
    else:
        registration_info_arg = '"' + "[%22" + game_txid + "%22]" + '"'
    registration_info = rpc_connection.cclib("register", "17", registration_info_arg)
    return registration_info


def rogue_pending(rpc_connection):
    rogue_pending_list = rpc_connection.cclib("pending", "17")
    return rogue_pending_list


def rogue_bailout(rpc_connection, game_txid):
    bailout_info_arg = '"' + "[%22" + game_txid + "%22]" + '"'
    bailout_info = rpc_connection.cclib("bailout", "17", bailout_info_arg)
    return bailout_info


def rogue_highlander(rpc_connection, game_txid):
    highlander_info_arg = '"' + "[%22" + game_txid + "%22]" + '"'
    highlander_info = rpc_connection.cclib("highlander", "17", highlander_info_arg)
    return highlander_info


def rogue_players_list(rpc_connection):
    rogue_players_list = rpc_connection.cclib("players", "17")
    return rogue_players_list


def rogue_player_info(rpc_connection, playertxid):
    player_info_arg = '"' + "[%22" + playertxid + "%22]" + '"'
    player_info = rpc_connection.cclib("playerinfo", "17", player_info_arg)
    return player_info


def rogue_extract(rpc_connection, game_txid, pubkey):
    extract_info_arg = '"' + "[%22" + game_txid + "%22,%22" + pubkey + "%22]" + '"'
    extract_info = rpc_connection.cclib("extract", "17", extract_info_arg)
    return extract_info


def rogue_keystrokes(rpc_connection, game_txid, keystroke):
    rogue_keystrokes_arg = '"' + "[%22" + game_txid + "%22,%22" + keystroke + "%22]" + '"'
    keystroke_info = rpc_connection.cclib("keystrokes", "17", rogue_keystrokes_arg)
    return keystroke_info


def print_multiplayer_games_list(rpc_connection):
    while True:
        pending_list = rogue_pending(rpc_connection)
        multiplayer_pending_list = []
        for game in pending_list["pending"]:
            if rogue_game_info(rpc_connection, game)["maxplayers"] > 1:
                multiplayer_pending_list.append(game)
        print("Multiplayer games availiable to join: \n")
        for active_multiplayer_game in multiplayer_pending_list:
            game_info = rogue_game_info(rpc_connection, active_multiplayer_game)
            print(colorize("\n================================\n", "green"))
            print("Game txid: " + game_info["gametxid"])
            print("Game buyin: " + str(game_info["buyin"]))
            print("Game height: " + str(game_info["gameheight"]))
            print("Start height: " + str(game_info["start"]))
            print("Alive players: " + str(game_info["alive"]))
            print("Registered players: " + str(game_info["numplayers"]))
            print("Max players: " + str(game_info["maxplayers"]))
            print(colorize("\n***\n", "blue"))
            print("Players in game:")
            for player in game_info["players"]:
                print("Slot: " + str(player["slot"]))
                if "baton" in player.keys():
                    print("Baton: " + str(player["baton"]))
                if "tokenid" in player.keys():
                    print("Tokenid: " + str(player["tokenid"]))
                print("Is mine?: " + str(player["ismine"]))
        print(colorize("\nR + Enter - refresh list.\nE + Enter - to the game choice.\nCTRL + C - back to main menu", "blue"))
        is_refresh = input("Choose your destiny: ")
        if is_refresh == "R":
            print("\n")
            pass
        elif is_refresh == "E":
            print("\n")
            break
        else:
            print("\nPlease choose R or E\n")


def rogue_newgame_singleplayer(rpc_connection, is_game_a_rogue=True):
    try:
        if is_game_a_rogue:
            new_game_txid = rpc_connection.cclib("newgame", "17", "[1]")["txid"]
            print("New singleplayer training game succesfully created. txid: " + new_game_txid)
            while True:
                mempool = rpc_connection.getrawmempool()
                if new_game_txid in mempool:
                    print(colorize("Waiting for game transaction to be mined", "blue"))
                    time.sleep(5)
                else:
                    print(colorize("Game transaction is mined", "green"))
                    break
        else:
            pending_games = rpc_connection.cclib("pending", "17")["pending"]
            new_game_txid = random.choice(pending_games)
        if is_game_a_rogue:
            players_list = rogue_players_list(rpc_connection)
            if len(players_list["playerdata"]) > 0:
                print_players_list(rpc_connection)
                while True:
                    is_choice_needed = input("Do you want to choose a player for this game? [y/n] ")
                    if is_choice_needed == "y":
                        player_txid = input("Please input player txid: ")
                        newgame_regisration_txid = rogue_game_register(rpc_connection, new_game_txid, player_txid)["txid"]
                        break
                    elif is_choice_needed == "n":
                        set_warriors_name(rpc_connection)
                        newgame_regisration_txid = rogue_game_register(rpc_connection, new_game_txid)["txid"]
                        break
                    else:
                        print("Please choose y or n !")
            else:
                print("No players available to select")
                input("Press [Enter] to continue...")
                newgame_regisration_txid = rogue_game_register(rpc_connection, new_game_txid)["txid"]
        else:
            newgame_regisration_txid = rogue_game_register(rpc_connection, new_game_txid)["txid"]
        while True:
            mempool = rpc_connection.getrawmempool()
            if newgame_regisration_txid in mempool:
                print(colorize("Waiting for registration transaction to be mined", "blue"))
                time.sleep(5)
            else:
                print(colorize("Registration transaction is mined", "green"))
                break
        game_info = rogue_game_info(rpc_connection, new_game_txid)
        start_time = time.time()
        while True:
            if is_game_a_rogue:
                subprocess.call(["../cc/rogue/rogue", str(game_info["seed"]), str(game_info["gametxid"])])
            else:
                subprocess.call(["../cc/games/tetris", str(game_info["seed"]), str(game_info["gametxid"])])
            time_elapsed = time.time() - start_time
            if time_elapsed > 1:
                break
            else:
                print("Game less than 1 second. Trying to start again")
                time.sleep(1)
        game_end_height = int(rpc_connection.getinfo()["blocks"])
        while True:
            current_height = int(rpc_connection.getinfo()["blocks"])
            height_difference = current_height - game_end_height
            if height_difference == 0:
                print(current_height)
                print(game_end_height)
                print(colorize("Waiting for next block before bailout", "blue"))
                time.sleep(5)
            else:
                break
        #print("\nKeystrokes of this game:\n")
        #time.sleep(0.5)
        while True:
            keystrokes_rpc_responses = find_game_keystrokes_in_log(new_game_txid)[1::2]
            if len(keystrokes_rpc_responses) < 1:
                print("No keystrokes broadcasted yet. Let's wait 5 seconds")
                time.sleep(5)
            else:
                break
        #print(keystrokes_rpc_responses)
        for keystroke in keystrokes_rpc_responses:
            json_keystroke = json.loads(keystroke)["result"]
            if "status" in json_keystroke.keys() and json_keystroke["status"] == "error":
                while True:
                    print("Trying to re-brodcast keystroke")
                    keystroke_rebroadcast = rogue_keystrokes(rpc_connection, json_keystroke["gametxid"], json_keystroke["keystrokes"])
                    if "txid" in keystroke_rebroadcast.keys():
                        print("Keystroke broadcasted! txid: " + keystroke_rebroadcast["txid"])
                        break
                    else:
                        print("Let's try again in 5 seconds")
                        time.sleep(5)
        # waiting for last keystroke confirmation here
        last_keystroke_json = json.loads(keystrokes_rpc_responses[-1])
        while True:
            while True:
                try:
                    rpc_connection.sendrawtransaction(last_keystroke_json["result"]["hex"])
                except Exception as e:
                    pass
                try:
                    confirmations_amount = rpc_connection.getrawtransaction(last_keystroke_json["result"]["txid"], 1)["confirmations"]
                    break
                except Exception as e:
                    print(e)
                    print("Let's wait a little bit more")
                    time.sleep(5)
                    pass
            if confirmations_amount < 1:
                print("Last keystroke not confirmed yet! Let's wait a little")
                time.sleep(10)
            else:
                print("Last keystroke confirmed!")
                break
        while True:
            print("\nExtraction info:\n")
            extraction_info = rogue_extract(rpc_connection, new_game_txid, rpc_connection.getinfo()["pubkey"])
            if extraction_info["status"] == "error":
                print(colorize("Your warrior died or no any information about game was saved on blockchain", "red"))
                print("If warrior was alive - try to wait a little (choose n to wait for a next block). If he is dead - you can bailout now (choose y).")
            else:
                print("Current game state:")
                print("Game txid: " + extraction_info["gametxid"])
                print("Information about game saved on chain: " + extraction_info["extracted"])
            print("\n")
            is_bailout_needed = input("Do you want to make bailout now [y] or wait for one more block [n]? [y/n]: ")
            if is_bailout_needed == "y":
                bailout_info = rogue_bailout(rpc_connection, new_game_txid)
                if is_game_a_rogue:
                    while True:
                        try:
                            confirmations_amount = rpc_connection.getrawtransaction(bailout_info["txid"], 1)["confirmations"]
                            break
                        except Exception as e:
                            print(e)
                            print("Bailout not on blockchain yet. Let's wait a little bit more")
                            time.sleep(20)
                            pass
                break
            elif is_bailout_needed == "n":
                game_end_height = int(rpc_connection.getinfo()["blocks"])
                while True:
                    current_height = int(rpc_connection.getinfo()["blocks"])
                    height_difference = current_height - game_end_height
                    if height_difference == 0:
                        print(current_height)
                        print(game_end_height)
                        print(colorize("Waiting for next block before bailout", "blue"))
                        time.sleep(5)
                    else:
                        break
            else:
                print("Please choose y or n !")
        print(bailout_info)
        print("\nGame is finished!\n")
        bailout_txid = bailout_info["txid"]
        input("Press [Enter] to continue...")
    except Exception as e:
        print("Something went wrong.")
        print(e)
        input("Press [Enter] to continue...")


def play_multiplayer_game(rpc_connection):
    # printing list of user active multiplayer games
    active_games_list = rpc_connection.cclib("games", "17")["games"]
    active_multiplayer_games_list = []
    for game in active_games_list:
        gameinfo = rogue_game_info(rpc_connection, game)
        if gameinfo["maxplayers"] > 1:
            active_multiplayer_games_list.append(gameinfo)
    games_counter = 0
    for active_multiplayer_game in active_multiplayer_games_list:
        games_counter = games_counter + 1
        is_ready_to_start = False
        try:
            active_multiplayer_game["seed"]
            is_ready_to_start = True
        except Exception as e:
            pass
        print(colorize("\n================================\n", "green"))
        print("Game txid: " + active_multiplayer_game["gametxid"])
        print("Game buyin: " + str(active_multiplayer_game["buyin"]))
        if is_ready_to_start:
            print(colorize("Ready for start!", "green"))
        else:
            print(colorize("Not ready for start yet, wait until start height!", "red"))
        print("Game height: " + str(active_multiplayer_game["gameheight"]))
        print("Start height: " + str(active_multiplayer_game["start"]))
        print("Alive players: " + str(active_multiplayer_game["alive"]))
        print("Registered players: " + str(active_multiplayer_game["numplayers"]))
        print("Max players: " + str(active_multiplayer_game["maxplayers"]))
        print(colorize("\n***\n", "blue"))
        print("Players in game:")
        for player in active_multiplayer_game["players"]:
            print("Slot: " + str(player["slot"]))
            print("Baton: " + str(player["baton"]))
            print("Tokenid: " +  str(player["tokenid"]))
            print("Is mine?: " + str(player["ismine"]))
    # asking user if he want to start any of them
    while True:
        start_game = input("\nDo you want to start any of your pendning multiplayer games?[y/n]: ")
        if start_game == "y":
            new_game_txid = input("Input txid of game which you want to start: ")
            game_info = rogue_game_info(rpc_connection, new_game_txid)
            try:
                start_time = time.time()
                while True:
                    subprocess.call(["cc/rogue/rogue", str(game_info["seed"]), str(game_info["gametxid"])])
                    time_elapsed = time.time() - start_time
                    if time_elapsed > 1:
                        break
                    else:
                        print("Game less than 1 second. Trying to start again")
                        time.sleep(1)
            except Exception as e:
                print("Maybe game isn't ready for start yet or your input was not correct, sorry.")
                input("Press [Enter] to continue...")
                break
            game_end_height = int(rpc_connection.getinfo()["blocks"])
            while True:
                current_height = int(rpc_connection.getinfo()["blocks"])
                height_difference = current_height - game_end_height
                if height_difference == 0:
                    print(current_height)
                    print(game_end_height)
                    print(colorize("Waiting for next block before bailout or highlander", "blue"))
                    time.sleep(5)
                else:
                    break
            while True:
                keystrokes_rpc_responses = find_game_keystrokes_in_log(new_game_txid)[1::2]
                if len(keystrokes_rpc_responses) < 1:
                    print("No keystrokes broadcasted yet. Let's wait 5 seconds")
                    time.sleep(5)
                else:
                    break
            for keystroke in keystrokes_rpc_responses:
                json_keystroke = json.loads(keystroke)["result"]
                if "status" in json_keystroke.keys() and json_keystroke["status"] == "error":
                    while True:
                        print("Trying to re-brodcast keystroke")
                        keystroke_rebroadcast = rogue_keystrokes(rpc_connection, json_keystroke["gametxid"],
                                                                 json_keystroke["keystrokes"])
                        if "txid" in keystroke_rebroadcast.keys():
                            print("Keystroke broadcasted! txid: " + keystroke_rebroadcast["txid"])
                            break
                        else:
                            print("Let's try again in 5 seconds")
                            time.sleep(5)
            last_keystroke_json = json.loads(keystrokes_rpc_responses[-1])
            while True:
                while True:
                    try:
                        confirmations_amount = rpc_connection.getrawtransaction(last_keystroke_json["result"]["txid"], 1)["confirmations"]
                        break
                    except Exception as e:
                        print(e)
                        print("Let's wait a little bit more")
                        rpc_connection.sendrawtransaction(last_keystroke_json["result"]["hex"])
                        time.sleep(5)
                        pass
                if confirmations_amount < 2:
                    print("Last keystroke not confirmed yet! Let's wait a little")
                    time.sleep(10)
                else:
                    print("Last keystroke confirmed!")
                    break
                while True:
                    print("\nExtraction info:\n")
                    extraction_info = rogue_extract(rpc_connection, new_game_txid, rpc_connection.getinfo()["pubkey"])
                    if extraction_info["status"] == "error":
                        print(colorize("Your warrior died or no any information about game was saved on blockchain", "red"))
                        print("If warrior was alive - try to wait a little (choose n to wait for a next block). If he is dead - you can bailout now (choose y).")
                    else:
                        print("Current game state:")
                        print("Game txid: " + extraction_info["gametxid"])
                        print("Information about game saved on chain: " + extraction_info["extracted"])
                    print("\n")
                    is_bailout_needed = input(
                        "Do you want to make bailout now [y] or wait for one more block [n]? [y/n]: ")
                    if is_bailout_needed == "y":
                        if game_info["alive"] > 1:
                            bailout_info = rogue_bailout(rpc_connection, new_game_txid)
                            try:
                                bailout_txid = bailout_info["txid"]
                                print(bailout_info)
                                print("\nGame is finished!\n")
                                input("Press [Enter] to continue...")
                                break
                            except Exception:
                                highlander_info = rogue_highlander(rpc_connection, new_game_txid)
                                highlander_info = highlander_info["txid"]
                                print(highlander_info)
                                print("\nGame is finished!\n")
                                input("Press [Enter] to continue...")
                                break
                        else:
                            highlander_info = rogue_highlander(rpc_connection, new_game_txid)
                            if 'error' in highlander_info.keys() and highlander_info["error"] == 'numplayers != maxplayers':
                                bailout_info = rogue_bailout(rpc_connection, new_game_txid)
                                print(bailout_info)
                                print("\nGame is finished!\n")
                                input("Press [Enter] to continue...")
                                break
                            else:
                                print(highlander_info)
                                print("\nGame is finished!\n")
                                input("Press [Enter] to continue...")
                                break
                    elif is_bailout_needed == "n":
                        game_end_height = int(rpc_connection.getinfo()["blocks"])
                        while True:
                            current_height = int(rpc_connection.getinfo()["blocks"])
                            height_difference = current_height - game_end_height
                            if height_difference == 0:
                                print(current_height)
                                print(game_end_height)
                                print(colorize("Waiting for next block before bailout", "blue"))
                                time.sleep(5)
                            else:
                                break
                break
            break
        if start_game == "n":
            print("As you wish!")
            input("Press [Enter] to continue...")
            break
        else:
            print(colorize("Choose y or n!", "red"))


def rogue_newgame_multiplayer(rpc_connection):
    while True:
        max_players = input("Input game max. players (>1): ")
        if int(max_players) > 1:
            break
        else:
            print("Please re-check your input")
            input("Press [Enter] to continue...")
    while True:
        buyin = input("Input game buyin (>0.001): ")
        if float(buyin) > 0.001:
            break
        else:
            print("Please re-check your input")
            input("Press [Enter] to continue...")
    try:
        new_game_txid = rpc_connection.cclib("newgame", "17", '"[' + max_players + "," + buyin + ']"')["txid"]
        print(colorize("New multiplayer game succesfully created. txid: " + new_game_txid, "green"))
        input("Press [Enter] to continue...")
    except Exception as e:
        print("Something went wrong.")
        print(e)
        input("Press [Enter] to continue...")


def rogue_join_multiplayer_game(rpc_connection):
    while True:
        try:
            print_multiplayer_games_list(rpc_connection)
            # TODO: optional player data txid (print players you have and ask if you want to choose one)
            game_txid = input("Input txid of game you want to join: ")
            try:
                while True:
                    print_players_list(rpc_connection)
                    is_choice_needed = input("Do you want to choose a player for this game? [y/n] ")
                    if is_choice_needed == "y":
                        player_txid = input("Please input player txid: ")
                        newgame_regisration_txid = rogue_game_register(rpc_connection, game_txid, player_txid)["txid"]
                        break
                    elif is_choice_needed == "n":
                        set_warriors_name(rpc_connection)
                        newgame_regisration_txid = rogue_game_register(rpc_connection, game_txid)["txid"]
                        break
                    else:
                        print("Please choose y or n !")
            except Exception as e:
                print("Something went wrong. Maybe you're trying to register on game twice or don't have enough funds to pay buyin.")
                print(e)
                input("Press [Enter] to continue...")
                break
            print(colorize("Succesfully registered.", "green"))
            while True:
                mempool = rpc_connection.getrawmempool()
                if newgame_regisration_txid in mempool:
                    print(colorize("Waiting for registration transaction to be mined", "blue"))
                    time.sleep(5)
                else:
                    print(colorize("Registration transaction is mined", "green"))
                    break
            print(newgame_regisration_txid)
            input("Press [Enter] to continue...")
            break
        except KeyboardInterrupt:
            break


def print_players_list(rpc_connection):
    players_list = rogue_players_list(rpc_connection)
    print(colorize("\nYou own " + str(players_list["numplayerdata"]) + " warriors\n", "blue"))
    warrior_counter = 0
    for player in players_list["playerdata"]:
        warrior_counter = warrior_counter + 1
        player_data = rogue_player_info(rpc_connection, player)["player"]
        print(colorize("\n================================\n","green"))
        print("Warrior " + str(warrior_counter))
        print("Name: " + player_data["pname"] + "\n")
        print("Player txid: " + player_data["playertxid"])
        print("Token txid: " + player_data["tokenid"])
        print("Hitpoints: " + str(player_data["hitpoints"]))
        print("Strength: " + str(player_data["strength"]))
        print("Level: " + str(player_data["level"]))
        print("Experience: " + str(player_data["experience"]))
        print("Dungeon Level: " + str(player_data["dungeonlevel"]))
        print("Chain: " + player_data["chain"])
        print(colorize("\nInventory:\n","blue"))
        for item in player_data["pack"]:
            print(item)
        print("\nTotal packsize: " + str(player_data["packsize"]) + "\n")
    input("Press [Enter] to continue...")


def sell_warrior(rpc_connection):
    print(colorize("Your brave warriors: \n", "blue"))
    print_players_list(rpc_connection)
    print("\n")
    while True:
        need_sell = input("Do you want to place order to sell any? [y/n]: ")
        if need_sell == "y":
            playertxid = input("Input playertxid of warrior you want to sell: ")
            price = input("Input price (in ROGUE coins) you want to sell warrior for: ")
            try:
                tokenid = rogue_player_info(rpc_connection, playertxid)["player"]["tokenid"]
            except Exception as e:
                print(e)
                print("Something went wrong. Be careful with input next time.")
                input("Press [Enter] to continue...")
                break
            token_ask_raw = rpc_connection.tokenask("1", tokenid, price)
            try:
                token_ask_txid = rpc_connection.sendrawtransaction(token_ask_raw["hex"])
            except Exception as e:
                print(e)
                print(token_ask_raw)
                print("Something went wrong. Be careful with input next time.")
                input("Press [Enter] to continue...")
                break
            print(colorize("Ask succesfully placed. Ask txid is: " + token_ask_txid, "green"))
            input("Press [Enter] to continue...")
            break
        if need_sell == "n":
            print("As you wish!")
            input("Press [Enter] to continue...")
            break
        else:
            print(colorize("Choose y or n!", "red"))


#TODO: have to combine into single scanner with different cases
def is_warrior_alive(rpc_connection, warrior_txid):
    warrior_alive = False
    raw_transaction = rpc_connection.getrawtransaction(warrior_txid, 1)
    for vout in raw_transaction["vout"]:
        if vout["value"] == 0.00000001 and rpc_connection.gettxout(raw_transaction["txid"], vout["n"]):
            warrior_alive = True
    return warrior_alive


def warriors_scanner(rpc_connection):
    start_time = time.time()
    token_list = rpc_connection.tokenlist()
    my_warriors_list = rogue_players_list(rpc_connection)
    warriors_list = {}
    for token in token_list:
        player_info = rogue_player_info(rpc_connection, token)
        if "status" in player_info and player_info["status"] == "error":
            pass
        elif player_info["player"]["playertxid"] in my_warriors_list["playerdata"]:
            pass
        elif not is_warrior_alive(rpc_connection, player_info["player"]["playertxid"]):
            pass
        else:
            warriors_list[token] = player_info["player"]
    print("--- %s seconds ---" % (time.time() - start_time))
    return warriors_list


def warriors_scanner_for_rating(rpc_connection):
    print("It can take some time")
    token_list = rpc_connection.tokenlist()
    my_warriors_list = rogue_players_list(rpc_connection)
    actual_playerids = []
    warriors_list = {}
    for token in token_list:
        player_info = rogue_player_info(rpc_connection, token)
        if "status" in player_info and player_info["status"] == "error":
            pass
        else:
            while True:
                if "batontxid" in player_info["player"].keys():
                    player_info = rogue_player_info(rpc_connection, player_info["player"]["batontxid"])
                else:
                    actual_playerids.append(player_info["player"]["playertxid"])
                    break
    for player_id in actual_playerids:
        player_info = rogue_player_info(rpc_connection, player_id)
        if not is_warrior_alive(rpc_connection, player_info["player"]["playertxid"]):
            pass
        else:
            warriors_list[player_id] = player_info["player"]
    return warriors_list


def warriors_scanner_for_dex(rpc_connection):
    start_time = time.time()
    token_list = rpc_connection.tokenlist()
    my_warriors_list = rogue_players_list(rpc_connection)
    warriors_list = {}
    for token in token_list:
        player_info = rogue_player_info(rpc_connection, token)
        if "status" in player_info and player_info["status"] == "error":
            pass
        elif player_info["player"]["tokenid"] in my_warriors_list["playerdata"]:
            pass
        else:
            warriors_list[token] = player_info["player"]
    print("--- %s seconds ---" % (time.time() - start_time))
    return warriors_list


def print_warrior_list(rpc_connection):
    players_list = warriors_scanner(rpc_connection)
    print(colorize("All warriors on ROGUE chain: \n", "blue"))
    warrior_counter = 0
    for player in players_list:
        warrior_counter = warrior_counter + 1
        player_data = rogue_player_info(rpc_connection, player)["player"]
        print(colorize("\n================================\n","green"))
        print("Warrior " + str(warrior_counter))
        print("Name: " + player_data["pname"] + "\n")
        print("Player txid: " + player_data["playertxid"])
        print("Token txid: " + player_data["tokenid"])
        print("Hitpoints: " + str(player_data["hitpoints"]))
        print("Strength: " + str(player_data["strength"]))
        print("Level: " + str(player_data["level"]))
        print("Experience: " + str(player_data["experience"]))
        print("Dungeon Level: " + str(player_data["dungeonlevel"]))
        print("Chain: " + player_data["chain"])
        print(colorize("\nInventory:\n","blue"))
        for item in player_data["pack"]:
            print(item)
        print("\nTotal packsize: " + str(player_data["packsize"]) + "\n")
    input("Press [Enter] to continue...")


def place_bid_on_warriror(rpc_connection):
    warriors_list = print_warrior_list(rpc_connection)
    # TODO: have to drop my warriors or at least print my warriors ids
    while True:
        need_buy = input("Do you want to place order to buy some warrior? [y/n]: ")
        if need_buy == "y":
            playertxid = input("Input playertxid of warrior you want to place bid for: ")
            price = input("Input price (in ROGUE coins) you want to buy warrior for: ")
            tokenid = rogue_player_info(rpc_connection, playertxid)["player"]["tokenid"]
            token_bid_raw = rpc_connection.tokenbid("1", tokenid, price)
            try:
                token_bid_txid = rpc_connection.sendrawtransaction(token_bid_raw["hex"])
            except Exception as e:
                print(e)
                print(token_bid_raw)
                print("Something went wrong. Be careful with input next time.")
                input("Press [Enter] to continue...")
                break
            print(colorize("Bid succesfully placed. Bid txid is: " + token_bid_txid, "green"))
            input("Press [Enter] to continue...")
            break
        if need_buy == "n":
            print("As you wish!")
            input("Press [Enter] to continue...")
            break
        else:
            print(colorize("Choose y or n!", "red"))


def check_incoming_bids(rpc_connection):
    # TODO: have to scan for warriors which are in asks as well
    players_list = rogue_players_list(rpc_connection)
    incoming_orders = []
    for player in players_list["playerdata"]:
        token_id = rogue_player_info(rpc_connection, player)["player"]["tokenid"]
        orders = rpc_connection.tokenorders(token_id)
        if len(orders) > 0:
            for order in orders:
                if order["funcid"] == "b":
                    incoming_orders.append(order)
    return incoming_orders


def print_icoming_bids(rpc_connection):
    incoming_bids = check_incoming_bids(rpc_connection)
    for bid in incoming_bids:
        print("Recieved bid for warrior " + bid["tokenid"])
        player_data = rogue_player_info(rpc_connection, bid["tokenid"])["player"]
        print(colorize("\n================================\n", "green"))
        print("Name: " + player_data["pname"] + "\n")
        print("Player txid: " + player_data["playertxid"])
        print("Token txid: " + player_data["tokenid"])
        print("Hitpoints: " + str(player_data["hitpoints"]))
        print("Strength: " + str(player_data["strength"]))
        print("Level: " + str(player_data["level"]))
        print("Experience: " + str(player_data["experience"]))
        print("Dungeon Level: " + str(player_data["dungeonlevel"]))
        print("Chain: " + player_data["chain"])
        print(colorize("\nInventory:\n", "blue"))
        for item in player_data["pack"]:
            print(item)
        print("\nTotal packsize: " + str(player_data["packsize"]) + "\n")
        print(colorize("\n================================\n", "blue"))
        print("Order info: \n")
        print("Bid txid: " + bid["txid"])
        print("Price: " + str(bid["price"]) + "\n")
    if len(incoming_bids) == 0:
        print(colorize("There is no any incoming orders!", "blue"))
        input("Press [Enter] to continue...")
    else:
        while True:
            want_to_sell = input("Do you want to fill any incoming bid? [y/n]: ")
            if want_to_sell == "y":
                bid_txid = input("Input bid txid you want to fill: ")
                for bid in incoming_bids:
                    if bid_txid == bid["txid"]:
                        tokenid = bid["tokenid"]
                        fill_sum = bid["totalrequired"]
                fillbid_hex = rpc_connection.tokenfillbid(tokenid, bid_txid, str(fill_sum))
                try:
                    fillbid_txid = rpc_connection.sendrawtransaction(fillbid_hex["hex"])
                except Exception as e:
                    print(e)
                    print(fillbid_hex)
                    print("Something went wrong. Be careful with input next time.")
                    input("Press [Enter] to continue...")
                    break
                print(colorize("Warrior succesfully sold. Txid is: " + fillbid_txid, "green"))
                input("Press [Enter] to continue...")
                break
            if want_to_sell == "n":
                print("As you wish!")
                input("Press [Enter] to continue...")
                break
            else:
                print(colorize("Choose y or n!", "red"))


def find_warriors_asks(rpc_connection):
    warriors_list = warriors_scanner_for_dex(rpc_connection)
    warriors_asks = []
    for player in warriors_list:
        orders = rpc_connection.tokenorders(player)
        if len(orders) > 0:
            for order in orders:
                if order["funcid"] == "s":
                    warriors_asks.append(order)
    for ask in warriors_asks:
        print(colorize("\n================================\n", "green"))
        print("Warrior selling on marketplace: " + ask["tokenid"])
        player_data = rogue_player_info(rpc_connection, ask["tokenid"])["player"]
        print("Name: " + player_data["pname"] + "\n")
        print("Player txid: " + player_data["playertxid"])
        print("Token txid: " + player_data["tokenid"])
        print("Hitpoints: " + str(player_data["hitpoints"]))
        print("Strength: " + str(player_data["strength"]))
        print("Level: " + str(player_data["level"]))
        print("Experience: " + str(player_data["experience"]))
        print("Dungeon Level: " + str(player_data["dungeonlevel"]))
        print("Chain: " + player_data["chain"])
        print(colorize("\nInventory:\n", "blue"))
        for item in player_data["pack"]:
            print(item)
        print("\nTotal packsize: " + str(player_data["packsize"]) + "\n")
        print(colorize("Order info: \n", "red"))
        print("Ask txid: " + ask["txid"])
        print("Price: " + str(ask["price"]) + "\n")
    while True:
        want_to_buy = input("Do you want to buy any warrior? [y/n]: ")
        if want_to_buy == "y":
            ask_txid = input("Input asktxid which you want to fill: ")
            for ask in warriors_asks:
                if ask_txid == ask["txid"]:
                    tokenid = ask["tokenid"]
            try:
                fillask_raw = rpc_connection.tokenfillask(tokenid, ask_txid, "1")
            except Exception as e:
                print("Something went wrong. Be careful with input next time.")
                input("Press [Enter] to continue...")
                break
            try:
                fillask_txid = rpc_connection.sendrawtransaction(fillask_raw["hex"])
            except Exception as e:
                print(e)
                print(fillask_raw)
                print("Something went wrong. Be careful with input next time.")
                input("Press [Enter] to continue...")
                break
            print(colorize("Warrior succesfully bought. Txid is: " + fillask_txid, "green"))
            input("Press [Enter] to continue...")
            break
        if want_to_buy == "n":
            print("As you wish!")
            input("Press [Enter] to continue...")
            break
        else:
            print(colorize("Choose y or n!", "red"))


def warriors_orders_check(rpc_connection):
    my_orders_list = rpc_connection.mytokenorders("17")
    warriors_orders = {}
    for order in my_orders_list:
        player_info = rogue_player_info(rpc_connection, order["tokenid"])
        if "status" in player_info and player_info["status"] == "error":
            pass
        else:
            warriors_orders[order["tokenid"]] = order
    bids_list = []
    asks_list = []
    for order in warriors_orders:
        if warriors_orders[order]["funcid"] == "s":
            asks_list.append(warriors_orders[order])
        else:
            bids_list.append(order)
    print(colorize("\nYour asks:\n", "blue"))
    print(colorize("\n********************************\n", "red"))
    for ask in asks_list:
        print("txid: " + ask["txid"])
        print("Price: " + ask["price"])
        print("Warrior tokenid: " + ask["tokenid"])
        print(colorize("\n================================\n", "green"))
        print("Warrior selling on marketplace: " + ask["tokenid"])
        player_data = rogue_player_info(rpc_connection, ask["tokenid"])["player"]
        print("Name: " + player_data["pname"] + "\n")
        print("Player txid: " + player_data["playertxid"])
        print("Token txid: " + player_data["tokenid"])
        print("Hitpoints: " + str(player_data["hitpoints"]))
        print("Strength: " + str(player_data["strength"]))
        print("Level: " + str(player_data["level"]))
        print("Experience: " + str(player_data["experience"]))
        print("Dungeon Level: " + str(player_data["dungeonlevel"]))
        print("Chain: " + player_data["chain"])
        print(colorize("\nInventory:\n", "blue"))
        for item in player_data["pack"]:
            print(item)
        print("\nTotal packsize: " + str(player_data["packsize"]) + "\n")
        print(colorize("\n================================\n", "green"))
    print(colorize("\nYour bids:\n", "blue"))
    print(colorize("\n********************************\n", "red"))
    for bid in bids_list:
        print("txid: " + bid["txid"])
        print("Price: " + bid["price"])
        print("Warrior tokenid: " + bid["tokenid"])
        print(colorize("\n================================\n", "green"))
        print("Warrior selling on marketplace: " + bid["tokenid"])
        player_data = rogue_player_info(rpc_connection, bid["tokenid"])["player"]
        print("Name: " + player_data["pname"] + "\n")
        print("Player txid: " + player_data["playertxid"])
        print("Token txid: " + player_data["tokenid"])
        print("Hitpoints: " + str(player_data["hitpoints"]))
        print("Strength: " + str(player_data["strength"]))
        print("Level: " + str(player_data["level"]))
        print("Experience: " + str(player_data["experience"]))
        print("Dungeon Level: " + str(player_data["dungeonlevel"]))
        print("Chain: " + player_data["chain"])
        print(colorize("\nInventory:\n", "blue"))
        for item in player_data["pack"]:
            print(item)
        print("\nTotal packsize: " + str(player_data["packsize"]) + "\n")
        print(colorize("\n================================\n", "green"))
    while True:
        need_order_change = input("Do you want to cancel any of your orders? [y/n]: ")
        if need_order_change == "y":
            while True:
                ask_or_bid = input("Do you want cancel ask or bid? [a/b]: ")
                if ask_or_bid == "a":
                    ask_txid = input("Input txid of ask you want to cancel: ")
                    warrior_tokenid = input("Input warrior token id for this ask: ")
                    try:
                        ask_cancellation_hex = rpc_connection.tokencancelask(warrior_tokenid, ask_txid)
                        ask_cancellation_txid = rpc_connection.sendrawtransaction(ask_cancellation_hex["hex"])
                    except Exception as e:
                        print(colorize("Please re-check your input!", "red"))
                    print(colorize("Ask succefully cancelled. Cancellation txid: " + ask_cancellation_txid, "green"))
                    break
                if ask_or_bid == "b":
                    bid_txid = input("Input txid of bid you want to cancel: ")
                    warrior_tokenid = input("Input warrior token id for this bid: ")
                    try:
                        bid_cancellation_hex = rpc_connection.tokencancelbid(warrior_tokenid, bid_txid)
                        bid_cancellation_txid = rpc_connection.sendrawtransaction(bid_cancellation_hex["hex"])
                    except Exception as e:
                        print(colorize("Please re-check your input!", "red"))
                    print(colorize("Bid succefully cancelled. Cancellation txid: " + bid_cancellation_txid, "green"))
                    break
                else:
                    print(colorize("Choose a or b!", "red"))
            input("Press [Enter] to continue...")
            break
        if need_order_change == "n":
            print("As you wish!")
            input("Press [Enter] to continue...")
            break
        else:
            print(colorize("Choose y or n!", "red"))


def set_warriors_name(rpc_connection):
    warriors_name = input("What warrior name do you want for legends and tales about your brave adventures?: ")
    warrior_name_arg = '"' + "[%22" + warriors_name + "%22]" + '"'
    set_name_status = rpc_connection.cclib("setname", "17", warrior_name_arg)
    print(colorize("Warrior name succesfully set", "green"))
    print("Result: " + set_name_status["result"])
    print("Name: " + set_name_status["pname"])
    input("Press [Enter] to continue...")


def top_warriors_rating(rpc_connection):
    start_time = time.time()
    warriors_list = warriors_scanner_for_rating(rpc_connection)
    warriors_exp = {}
    for warrior in warriors_list:
        warriors_exp[warrior] = warriors_list[warrior]["experience"]
    warriors_exp_sorted = {}
    temp = [(k, warriors_exp[k]) for k in sorted(warriors_exp, key=warriors_exp.get, reverse=True)]
    for k,v in temp:
        warriors_exp_sorted[k] = v
    counter = 0
    for experienced_warrior in warriors_exp_sorted:
        if counter < 20:
            counter = counter + 1
            print("\n" + str(counter) + " place.")
            print(colorize("\n================================\n", "blue"))
            player_data = rogue_player_info(rpc_connection, experienced_warrior)["player"]
            print("Name: " + player_data["pname"] + "\n")
            print("Player txid: " + player_data["playertxid"])
            print("Token txid: " + player_data["tokenid"])
            print("Hitpoints: " + str(player_data["hitpoints"]))
            print("Strength: " + str(player_data["strength"]))
            print("Level: " + str(player_data["level"]))
            print("Experience: " + str(player_data["experience"]))
            print("Dungeon Level: " + str(player_data["dungeonlevel"]))
            print("Chain: " + player_data["chain"])
    print("--- %s seconds ---" % (time.time() - start_time))
    input("Press [Enter] to continue...")


def exit():
    sys.exit()


def warrior_trasnfer(rpc_connection):
    print(colorize("Your brave warriors: \n", "blue"))
    print_players_list(rpc_connection)
    print("\n")
    while True:
        need_transfer = input("Do you want to transfer any warrior? [y/n]: ")
        if need_transfer == "y":
            warrior_tokenid = input("Input warrior tokenid: ")
            recepient_pubkey = input("Input recepient pubkey: ")
            try:
                token_transfer_hex = rpc_connection.tokentransfer(warrior_tokenid, recepient_pubkey, "1")
                token_transfer_txid = rpc_connection.sendrawtransaction(token_transfer_hex["hex"])
            except Exception as e:
                print(e)
                print("Something went wrong. Please be careful with your input next time!")
                input("Press [Enter] to continue...")
                break
            print(colorize("Warrior succesfully transferred! Transfer txid: " + token_transfer_txid, "green"))
            input("Press [Enter] to continue...")
            break
        if need_transfer == "n":
            print("As you wish!")
            input("Press [Enter] to continue...")
            break
        else:
            print(colorize("Choose y or n!", "red"))


def check_if_config_is_here(rpc_connection, assetchain_name):
    config_name = assetchain_name + ".conf"
    if os.path.exists(config_name):
        print(colorize("Config is already in daemon folder", "green"))
    else:
        if operating_system == 'Darwin':
            path_to_config = os.environ['HOME'] + '/Library/Application Support/Komodo/' + assetchain_name + '/' + config_name
        elif operating_system == 'Linux':
            path_to_config = os.environ['HOME'] + '/.komodo/' + assetchain_name + '/' + config_name
        elif operating_system == 'Win64' or operating_system == 'Windows':
            path_to_config = '%s/komodo/' + assetchain_name + '/' + config_name % os.environ['APPDATA']
        try:
            copy(path_to_config, os.getcwd())
        except Exception as e:
            print(e)
            print("Can't copy config to current daemon directory automatically by some reason.")
            print("Please copy it manually. It's locating here: " + path_to_config)


def find_game_keystrokes_in_log(gametxid):

    operating_system = platform.system()
    if operating_system == 'Win64' or operating_system == 'Windows':
        p1 = subprocess.Popen(["type", "keystrokes.log"], stdout=subprocess.PIPE, shell=True)
        p2 = subprocess.Popen(["findstr", gametxid], stdin=p1.stdout, stdout=subprocess.PIPE, shell=True)
    else:
        p1 = subprocess.Popen(["cat", "keystrokes.log"], stdout=subprocess.PIPE)
        p2 = subprocess.Popen(["grep", gametxid], stdin=p1.stdout, stdout=subprocess.PIPE)
    p1.stdout.close()
    output = p2.communicate()[0]
    keystrokes_log_for_game = bytes.decode(output).split("\n")
    return keystrokes_log_for_game


def check_if_tx_in_mempool(rpc_connection, txid):
    while True:
        mempool = rpc_connection.getrawmempool()
        if txid in mempool:
            print(colorize("Waiting for " + txid + " transaction to be mined", "blue"))
            time.sleep(5)
        else:
            print(colorize("Transaction is mined", "green"))
            break
