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
        else:
            ac.update({'daemon_params': ['-daemon', '-whitelist=127.0.0.1']})
    else:
        raise EnvironmentError("\nNo asset chains configuration provided")
    return ac


# TODO: add coins file compatibility with create_configs func
def create_configs(asset, node=0):
    if os.name == 'posix':
        confpath = ('./node_' + str(node) + '/' + asset + '.conf')
    else:
        confpath = (os.getcwd() + '\\node_' + str(node) + '\\' + asset + '.conf')
    if not os.path.isfile(confpath):
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
    for node in range(clients_to_start):  # prepare config folders
        create_configs(aschain, node)
    if env_params.get('is_bootstrap_needed'):  # bootstrap chains
        if not os.path.isfile('bootstrap.tar.gz'):
            wget.download(env_params.get('bootstrap_url'), "bootstrap.tar.gz")
        tf = tarfile.open("bootstrap.tar.gz")
        for i in range(clients_to_start):
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
                   '-ac_name=' + aschain,
                   '-conf=' + confpath,
                   '-datadir=' + datapath,
                   '-pubkey=' + env_params.get('test_pubkey')[i],
                   ]
        if i == 0:
            for key in ac_params.keys():
                cl_args.append('-' + key + '=' + str(ac_params.get(key)))
        else:
            cl_args.append('-addnode=127.0.0.1:' + str(ac_params.get('port')))
            for key in ac_params.keys():
                if isinstance(ac_params.get(key), int):
                    data = ac_params.get(key) + 1
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
        enable_mining(rpc_p)
        validate_proxy(env_params, rpc_p, i)


if __name__ == '__main__':
    main()
