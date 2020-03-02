import pytest
import json
import os
import time
# Using different proxy to bypass libcurl issues on Windows
<<<<<<< HEAD
try:
    from slickrpc import Proxy
    import pycurl
except ImportError:
=======
if os.name == 'posix':
    from slickrpc import Proxy
else:
>>>>>>> dev
    from bitcoinrpc.authproxy import AuthServiceProxy as Proxy


@pytest.fixture(scope='session')
def proxy_connection():
    proxy_connected = []

    def _proxy_connection(node_params_dictionary):
        try:
            proxy = Proxy("http://%s:%s@%s:%d" % (node_params_dictionary["rpc_user"],
                                                  node_params_dictionary["rpc_password"],
                                                  node_params_dictionary["rpc_ip"],
                                                  node_params_dictionary["rpc_port"]), timeout=120)
            proxy_connected.append(proxy)
<<<<<<< HEAD
        except (ConnectionAbortedError, pycurl.error) as e:
=======
        except Exception as e:
>>>>>>> dev
            raise Exception("Connection error! Probably no daemon on selected port. Error: ", e)
        return proxy

    yield _proxy_connection

    for each in proxy_connected:
        print("\nStopping created proxies...")
        time.sleep(10)  # time wait for tests to finish correctly before stopping daemon
        try:  # while using AuthServiceProxy, stop method results in connection aborted error
            each.stop()
<<<<<<< HEAD
        except (ConnectionAbortedError, pycurl.error) as e:
=======
        except ConnectionAbortedError as e:
>>>>>>> dev
            print(e)


@pytest.fixture(scope='session')
def test_params(proxy_connection):
    with open('nodesconfig.json', 'r') as f:
        params_dict = json.load(f)
<<<<<<< HEAD
    nodelist_raw = list(params_dict.keys())
    nodelist = []
    if os.environ['CLIENTS']:
        numclients = int(os.environ['CLIENTS'])
        for i in range(numclients):
            nodelist.append(nodelist_raw[i])
    else:
        nodelist_raw.pop()  # escape extra param in dict -- is_fresh_chain
        nodelist = nodelist_raw
    test_params = {}
    for node in nodelist:
        node_params = params_dict[node]
        rpc = proxy_connection(node_params)
        test_params.update({node: node_params})
        test_params[node].update({'rpc': rpc})
=======
    node1_params = params_dict['node1']
    node2_params = params_dict['node2']
    rpc1 = proxy_connection(node1_params)
    rpc2 = proxy_connection(node2_params)
    test_params = {'node1': node1_params, 'node2': node2_params}
    test_params['node1'].update({'rpc': rpc1})
    test_params['node2'].update({'rpc': rpc2})
>>>>>>> dev
    return test_params
