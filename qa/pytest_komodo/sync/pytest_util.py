import os
import requests
import json


def env_get(var, default):
    try:
        res = os.environ[var]
    except KeyError:
        res = default
    return res


def get_chainstate(proxy):
    vals = {}
    res = proxy.getinfo()
    vals.update({'synced': res.get('synced')})
    vals.update({'notarized': res.get('notarized')})
    vals.update({'blocks': res.get('blocks')})
    vals.update({'longestchain': res.get('longestchain')})
    return vals


def get_notary_stats():
    api = "https://komodostats.com/api/notary/summary.json"
    local = "notary.json"
    data = requests.get(api).json()
    with open(local, 'w') as lf:
        lf.write(str(data))
    return data


def check_notarized(proxy, api_stats, coin, blocktime=60):
    maxblocksdiff = round(1500 / blocktime)
    daemon_stats = proxy.getinfo()
    notarizations = {}
    for item in api_stats:
        if item.get('ac_name') == coin:
            notarizations = item
    if not notarizations:
        raise BaseException("Chain notary data not found")
    if daemon_stats['notarized'] == notarizations['notarized']:
        assert daemon_stats['notarizedhash'] == notarizations['notarizedhash']
        assert daemon_stats['notarizedtxid'] == notarizations['notarizedtxid']
        return True
    elif abs(daemon_stats['notarazied'] - notarizations['notarized']) >= maxblocksdiff:
        return False
    else:
        assert daemon_stats['notarized']
        assert daemon_stats['notarizedhash'] != '0000000000000000000000000000000000000000000000000000000000000000'
        assert daemon_stats['notarizedtxid'] != '0000000000000000000000000000000000000000000000000000000000000000'
        return True
