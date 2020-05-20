import os
import json
import time
import subprocess
import wget
import tarfile
from basic.pytest_util import create_proxy, validate_proxy, enable_mining


def load_env_config():
    tp = {}  # test env parameters
    if os.name == 'posix':
        envconfig = './envconfig.json'
    else:
        envconfig = 'envconfig.json'
    if os.environ['CHAIN']:
        tp.update({'clients_to_start': int(os.environ['CLIENTS'])})
        tp.update({'is_bootstrap_needed': os.environ['IS_BOOTSTRAP_NEEDED']})
        tp.update({'bootstrap_url': os.environ['BOOTSTRAP_URL']})
        tp.update({'chain_start_mode': os.environ['CHAIN_MODE']})
        tp.update({'ac_name': os.environ['CHAIN']})
        test_wif_list = []  # preset empty params lists
        test_addr_list = []
        test_pubkey_list = []
        for i in range(tp.get('clients_to_start')):
            test_wif_list.append(os.environ["TEST_WIF" + str(i)])
            test_addr_list.append(os.environ["TEST_ADDY" + str(i)])
            if os.environ['CHAIN_MODE'] not in ['DEX1', 'DEX2']:
                test_pubkey_list.append(os.environ["TEST_PUBKEY" + str(i)])
        tp.update({'test_wif': test_wif_list})
        tp.update({'test_address': test_addr_list})
        tp.update({'test_pubkey': test_pubkey_list})
    elif os.path.isfile(envconfig) and not os.environ['CHAIN']:
        with open(envconfig, 'r') as f:
            tp = json.load(f)
    else:
        raise EnvironmentError("\nNo test env configuration provided")
    return tp


def load_ac_params(asset, chain_mode='default'):
    if os.name == 'posix':
        chainconfig = './chainconfig.json'
        binary_path = '../../src/komodod'
    else:
        chainconfig = (os.getcwd() + '\\chainconfig.json')
        binary_path = (os.getcwd() + '\\..\\..\\src\\komodod.exe')
    if os.path.isfile(chainconfig):
        with open(chainconfig, 'r') as f:
            jsonparams = json.load(f)
        ac = jsonparams.get(asset)  # asset chain parameters
        ac.update({'binary_path': binary_path})
        if chain_mode == 'REGTEST':
            ac.update({'daemon_params': ['-daemon', '-whitelist=127.0.0.1', '-regtest']})
        elif chain_mode == 'DEX1':
            ac.update({'daemon_params': ['-daemon', '-whitelist=127.0.0.1', '-dexp2p=1']})
        elif chain_mode == 'DEX2':
            ac.update({'daemon_params': ['-daemon', '-whitelist=127.0.0.1', '-dexp2p=2']})
        else:
            ac.update({'daemon_params': ['-daemon', '-whitelist=127.0.0.1']})
    else:
        raise EnvironmentError("\nNo asset chains configuration provided")
    return ac


def create_configs(asset, node=0):
    if os.name == 'posix':
        confpath = ('./node_' + str(node) + '/' + asset + '.conf')
    else:
        confpath = (os.getcwd() + '\\node_' + str(node) + '\\' + asset + '.conf')
    if os.path.isfile(confpath) or os.path.isdir(os.getcwd() + '/node_' + str(node)):
        for root, dirs, files in os.walk('node_' + str(node), topdown=False):
            for name in files:
                os.remove(os.path.join(root, name))
            for name in dirs:
                os.rmdir(os.path.join(root, name))
        os.rmdir('node_' + str(node))
    print("Clean up done")
    os.mkdir('node_' + str(node))
    open(confpath, 'a').close()
    with open(confpath, 'a') as conf:
        conf.write("rpcuser=test\n")
        conf.write("rpcpassword=test\n")
        conf.write('rpcport=' + str(7000 + node) + '\n')
        conf.write("rpcbind=0.0.0.0\n")
        conf.write("rpcallowip=0.0.0.0/0\n")


def main():
    env_params = load_env_config()
    clients_to_start = env_params.get('clients_to_start')
    aschain = env_params.get('ac_name')
    if os.path.isfile('bootstrap.tar.gz'):
        os.remove('bootstrap.tar.gz')
        pass
    if env_params.get('is_bootstrap_needed'):  # bootstrap chains
        print("Downloading bootstrap")
        wget.download(env_params.get('bootstrap_url'), "bootstrap.tar.gz")
    try:
        tf = tarfile.open("bootstrap.tar.gz")
        btrp = True
    except FileNotFoundError:
        tf = ""
        btrp = False
    for i in range(clients_to_start):
        create_configs(aschain, i)
        if btrp:
            tf.extractall("node_" + str(i))
    mode = env_params.get('chain_start_mode')
    ac_params = load_ac_params(aschain, mode)
    for i in range(clients_to_start):  # start daemons
        if os.name == 'posix':
            confpath = (os.getcwd() + '/node_' + str(i) + '/' + aschain + '.conf')
            datapath = (os.getcwd() + '/node_' + str(i))
        else:
            confpath = (os.getcwd() + '\\node_' + str(i) + '\\' + aschain + '.conf')
            datapath = (os.getcwd() + '\\node_' + str(i))
        cl_args = [ac_params.get('binary_path'),
                   '-conf=' + confpath,
                   '-datadir=' + datapath
                   ]
        try:
            pubkey = env_params.get('test_pubkey')[i]
            cl_args.append('-pubkey=' + pubkey)
        except IndexError:
            pass
        if i == 0:
            for key in ac_params.keys():
                if key not in ['binary_path', 'daemon_params', 'rpc_user', 'rpcpassword'] and ac_params.get(key):
                    cl_args.append('-' + key + '=' + str(ac_params.get(key)))
        else:
            cl_args.append('-addnode=127.0.0.1:' + str(ac_params.get('port')))
            for key in ac_params.keys():
                if key not in ['binary_path', 'daemon_params', 'rpc_user', 'rpcpassword'] and ac_params.get(key):
                    if isinstance(ac_params.get(key), int):
                        data = ac_params.get(key) + i
                        cl_args.append('-' + key + '=' + str(data))
                    else:
                        cl_args.append('-' + key + '=' + str(ac_params.get(key)))
        cl_args.extend(ac_params.get('daemon_params'))
        print(cl_args)
        if os.name == "posix":
            subprocess.call(cl_args)
        else:
            subprocess.Popen(cl_args, shell=False, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
        time.sleep(5)
    for i in range(clients_to_start):
        node_params = {
            'rpc_user': 'test',
            'rpc_password': 'test',
            'rpc_ip': '127.0.0.1',
            'rpc_port': 7000 + i
        }
        rpc_p = create_proxy(node_params)
        validate_proxy(env_params, rpc_p, i)
        enable_mining(rpc_p)


if __name__ == '__main__':
    main()
