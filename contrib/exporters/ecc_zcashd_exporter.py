#!/usr/bin/env python3

# /************************************************************************
#  File: ecc_zcashd_exporter.py
#  Author: mdr0id
#  Date: 6/1/2022
#  Description:  exporter for zcashd
#  Docs: RPC docs - https://zcash.github.io/rpc
#
#  Usage: 
#
#  Known Bugs:
#
# ************************************************************************/

from cmath import log
import inspect
import json
import logging
import time
import subprocess
import os
import sys
import pycurl
import geocoder
import pygeohash as pgh
from prometheus_client import start_http_server, Summary, Gauge, Enum, Info, Counter
from shutil import which
from dotenv import load_dotenv
from slickrpc import Proxy
from slickrpc.exc import RpcException
from slickrpc.exc import RpcInWarmUp

class ZcashExporter:

    def __init__(self, app_port=9200, polling_interval_seconds=5, polling_interval_startup_seconds=5,
                 rpc_user="user", rpc_password="passw", rpc_host="127.0.0.1", rpc_port=8232, rpc_network="mainnet"
                ):
        self.app_port = app_port
        self.polling_interval_startup_seconds = polling_interval_startup_seconds
        self.polling_interval_seconds = polling_interval_seconds
        self.rpc_user = rpc_user
        self.rpc_password = rpc_password
        self.rpc_host = rpc_host
        self.rpc_port = rpc_port
        self.rpc_network = rpc_network

        self.node_network= None #used to sanity check cacheing state issues across env

        """
         RPC Blockchain
        """
        # getblockchaininfo
        self.ZCASH_CHAIN = Info('zcash_chain', 'current network name as defined in BIP70 (main, test, regtest)')
        self.ZCASH_BLOCKS = Gauge('zcash_blocks', 'the current number of blocks processed in the server')
        self.ZCASH_IBD = Gauge('zcash_initial_block_download_complete', 'true if the initial download of the blockchain is complete')
        self.ZCASH_HEADERS = Gauge('zcash_headers', 'Zcashd current number of headers validated')
        self.ZCASH_BEST_BLOCK_HASH = Info('zcash_bestblockhash','the hash of the currently best block') #might need to change type
        self.ZCASH_DIFFICULTY= Gauge('zcash_difficulty', 'the current difficulty')
        self.ZCASH_VERIFICATION_PROG = Gauge('zcash_verification_progress', 'Zcashd estimate of chain verification progress' )
        self.ZCASH_ESTIMATED_HEIGHT = Gauge('zcash_estimated_height', 'Zcashd if syncing, the estimated height of the chain, else the current best height4 ')
        self.ZCASH_CHAINWORK = Info('zcash_chainwork', 'total amount of work in active chain, in hexadecimal')
        self.ZCASH_CHAIN_DISK_SIZE = Gauge('zcash_chain_size_on_disk', 'the estimated size of the block and undo files on disk')
        self.ZCASH_COMMITMENTS = Gauge('zcash_commitments', 'the current number of note commitments in the commitment tree')
        self.ZCASH_SPROUT_BALANCE = Gauge('zcash_sprout_bal', 'the current sprout pool balance')
        self.ZCASH_SAPLING_BALANCE = Gauge('zcash_sapling_bal', 'the current sapling pool balance')
        self.ZCASH_ORCHARD_BALANCE = Gauge('zcash_orchard_bal', 'the current orchard pool balance')

        
        # getmempoolinfo
        self.ZCASH_MEMPOOL_SIZE = Gauge('zcash_mempool_size', 'Zcash current mempool tx count')
        self.ZCASH_MEMPOOL_BYTES = Gauge('zcash_mempool_bytes', 'Zcash sum of tx sizes')
        self.ZCASH_MEMPOOL_USAGE = Gauge('zcash_mempool_usage', 'Zcash total memory usage for the mempool') 

        """
         RPC Control
        """ 
        # getinfo
        self.ZCASH_VERSION = Info('zcashd_server_version', 'Zcash server version')
        self.ZCASH_BUILD = Info('zcashd_build_version', 'Zcash build number')
        self.ZCASH_SUBVERSION = Info('zcashd_subversion', 'Zcash server sub-version')
        self.ZCASH_PROTOCOL_VERSION = Info('zcashd_protocol_version', 'Zcash protocol version')
        self.ZCASH_WALLET_VERSION = Info('zcashd_wallet_version', 'Zcash wallet version')

        # getmemoryinfo
        self.ZCASH_MEM_USED = Gauge("zcash_mem_used", 'Number of bytes used')
        self.ZCASH_MEM_FREE = Gauge("zcash_mem_free", 'Number of bytes available in current arenas')
        self.ZCASH_MEM_TOTAL = Gauge("zcash_mem_total", 'Total number of bytes managed')
        self.ZCASH_MEM_LOCKED = Gauge("zcash_mem_locked", 'Amount of bytes that succeeded locking. If this number is smaller than total, locking pages failed at some point and key data could be swapped to disk.')
        self.ZCASH_MEM_CHUNKS_USED = Gauge("zcash_mem_chunks_used", 'Number allocated chunks')
        self.ZCASH_MEM_CHUNKS_FREE = Gauge("zcash_mem_chunks_free", 'Number unused chunks')

        #gettxoutsetinfo
        self.ZCASH_TXOUT_HEIGHT = Gauge("zcash_txout_height", "The current block height(index)")
        self.ZCASH_TXOUT_BESTBLOCK = Info("zcash_txout_bestblock", "the best block hash hex")
        self.ZCASH_TXOUT_TRANSACTIONS = Gauge("zcash_txout_transactions", "The number of transactions")
        self.ZCASH_TXOUT_TXOUTS = Gauge("zcash_txout_txouts", "The number of output transactions")
        self.ZCASH_TXOUT_BYTES_SERIALIZED = Gauge("zcash_txout_bytes_serialized", "The serialized size") 
        self.ZCASH_TXOUT_HASH_SERIALIZED = Info("zcash_txout_hash_serialized", "The serialized hash")
        self.ZCASH_TXOUT_TOTAL_AMOUNT = Gauge("zcash_txout_total_amount", "The total amount")

        """
        RPC Mining
        """
        self.ZCASH_MINING_BLOCKS = Gauge("zcash_mining_blocks", "The current block")
        self.ZCASH_MINING_DIFFICULTY = Gauge("zcash_mining_difficulty", "The current difficulty")
        self.ZCASH_MINING_ERRORS = Info("zcash_mining_errors", "The current errors")
        self.ZCASH_MINING_ERROR_TIMESTAMP = Gauge("zcash_mining_error_timestamp", "The current error timestamp")
        self.ZCASH_MINING_GEN_PROC_LIMIT = Gauge("zcash_mining_gen_proc_limit", "The processor limit for generation. -1 if no generation. (see getgenerate or setgenerate calls)")
        self.ZCASH_MINING_LOCAL_SOLPS = Gauge("zcash_mining_local_solps", "The average local solution rate in Sol/s since this node was started")
        self.ZCASH_MINING_NETWORK_SOLPS = Gauge("zcash_mining_network_solps", "The estimated network solution rate in Sol/s")
        self.ZCASH_MINING_NETWORK_HASHPS = Gauge("zcash_mining_network_hashps", "The estimated netework hash ps")    
        self.ZCASH_MINING_POOLEDTX = Gauge("zcash_mining_pooledtx",  "The size of the mem pool")
        self.ZCASH_MINING_TESTNET = Gauge("zcash_mining_testnet", "If using testnet or not")
        self.ZCASH_MINING_CHAIN = Info("zcash_mining_chain", "current network name as defined in BIP70 (main, test, regtest)" )
        self.ZCASH_MINING_GENERATE = Gauge("zcash_mining_generate", "If the generation is on or off (see getgenerate or setgenerate calls)")

        """
         RPC Network
        """
        # getdeprecationinfo
        self.ZCASH_DEPRECATION_HEIGHT= Gauge('zcash_deprecation', 'Zcash mainnet block height at which this version will deprecate and shut down')
        self.ZCASH_DEPRECATION_TIME= Gauge('zcash_deprecation_time', 'the approximate time at which this version is expected to reach the end-of-service height, \
                                             in seconds since epoch (midnight Jan 1 1970 GMT). Please note that given the variability of block times, \
                                             the actual end-of-service halt may vary from this time by hours or even days; this value is provided \
                                             solely for informational purposes and should not be relied upon to remain accurate. If the end-of-service \
                                             height for this node has already been reached, this timestamp may be in the past.')
        # getnettotals
        self.ZCASH_TOTAL_BYTES_RECV = Gauge("zcash_total_bytes_recv", "Total bytes received")
        self.ZCASH_TOTAL_BYTES_SENT = Gauge("zcash_total_bytes_sent", "Total bytes sent")

        # getconnectioncount
        self.ZCASH_CONNECTION_COUNT = Gauge("zcash_connection_count", "Current number of peers connected")


        # getpeerinfo
        #self.ZCASH_PEER_CONNECTION_TIME = Gauge("zcash_peer_conn_time", "zcash peer connection time", ["id", "addr", "addrlocal"])
        self.ZCASH_PEER_ID = Gauge("zcash_peer_id", "Peer index", ["addr", "geohash"])
        self.ZCASH_PEER_SERVICES = Gauge("zcash_peer_services", "(string) The services offered", ["id", "addr"])
        self.ZCASH_PEER_RELAYTXES = Gauge("zcash_peer_relaytxes","(boolean) Whether peer has asked us to relay transactions to it", ["id", "addr"])
        self.ZCASH_PEER_LAST_SEND =  Gauge("zcash_peer_last_send", "he time in seconds since epoch (Jan 1 1970 GMT) of the last send", ["id", "addr"])
        self.ZCASH_PEER_LAST_RECV = Gauge("zcash_peer_last_recv", "he time in seconds since epoch (Jan 1 1970 GMT) of the last receive", ["id", "addr"])
        self.ZCASH_PEER_BYTES_SENT = Gauge("zcash_peer_bytes_sent", "The total bytes sent", ["id", "addr"])
        self.ZCASH_PEER_BYTES_RECV = Gauge("zcash_peer_bytes_recv", "The total bytes received", ["id", "addr"])
        self.ZCASH_PEER_CONNECTION_TIME = Gauge("zcash_peer_conn_time", "zcash peer connection time", ["id", "addr"])
        self.ZCASH_PEER_TIME_OFFSET = Gauge("zcash_peer_time_offset", "The time offset in seconds", ["id", "addr"])
        self.ZCASH_PEER_PING_TIME = Gauge("zcash_peer_ping_time", "ping time", ["id", "addr"])
        self.ZCASH_PEER_PING_WAIT = Gauge("zcash_peer_ping_wait", "ping wait", ["id", "addr"])
        self.ZCASH_PEER_VERSION = Gauge("zcash_peer_version", "Peer version", ["id", "addr"])
        self.ZCASH_PEER_SUBVERSION = Gauge("zcash_peer_subversion", "(string) The string version", ["id", "addr"])
        self.ZCASH_PEER_INBOUND = Gauge("zcash_peer_inbound", "Inbound (true) or Outbound (false)", ["id", "addr"])
        self.ZCASH_PEER_STARTING_HEIGHT = Gauge("zcash_peer_starting_height", "The starting height (block) of the peer", ["id", "addr"])
        self.ZCASH_PEER_BANSCORE = Gauge("zcash_peer_banscore", "The ban score", ["id", "addr"])
        self.ZCASH_SYNCED_HEADERS = Gauge("zcash_peer_synced_headers", "The last header we have in common with this peer", ["id", "addr"])
        self.ZCASH_SYNCED_BLOCKS = Gauge("zcash_peer_synced_blocks", "The last block we have in common with this peer", ["id", "addr"])
        self.ZCASH_ADDR_PROCESSED = Gauge("zcash_peer_addr_processed", "zcash current addreses processed", ["id", "addr"])
        self.ZCASH_ADDR_RATE_LIMITED = Gauge("zcash_peer_addr_rate_limited", "current zcash address rate limit", ["id", "addr"])
        self.ZCASH_WHITELISTED = Gauge("zcash_peer_whitelisted", "zcash whitelisted", ["id", "addr"]) #TRUE/FA:S

        #getchaintips
        self.ZCASH_CHAINTIP_HEIGHT = Gauge("zcash_chaintip_height", "height of the chain tip", ["hash", "status"])
        self.ZCASH_CHAINTIP_BRANCH_LEN = Gauge("zcash_chaintip_branchlen", "zero for main chain", ["hash", "status"])

        #self.ZCASH_PEER_ID = Gauge("zcash_peer_id", "zcash peer id", ["ip", "id"])

    def run_startup_loop(self):
        """startup polling loop to ensure valid zcashd state before fetching metrics"""
        logging.info("Startup loop")
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        
        #Get startup state of zcashd node
        logging.info("Checking if zcashd is in valid state...")
        zcash_info = None
        while True:
            try:
                zcash_info = api.getblockchaininfo()
            except pycurl.error:
                logging.info("Zcashd has not been started yet. Retrying...")
                time.sleep(self.polling_interval_startup_seconds)
            except RpcInWarmUp:
                logging.info("Zcashd is in RpcInWarmUp state. Retrying...")
                time.sleep(self.polling_interval_startup_seconds)
            except ValueError:
                logging.info("Value error likely indicating bad env load")
                time.sleep(self.polling_interval_startup_seconds)
            else:
                break
        
        if(zcash_info['chain'] == 'main'):
            logging.info("Zcashd node is on mainnet")
            self.ZCASH_CHAIN.info({'zcash_chain': "mainnet"})
            self.node_network = "mainnet"
        elif(zcash_info['chain'] == 'test'):
            logging.info("Zcashd node is on testnet")
            self.ZCASH_CHAIN.info({'zcash_chain': "testnet"})
            self.node_network = "testnet"
        elif(zcash_info['chain'] == 'regtest'):
            logging.info("Zcashd node is on regtest")
            self.ZCASH_CHAIN.info({'zcash_chain': "regtest"})
            self.node_network = "regtest"
        else:
            logging.warning("Zcashd node is not mainnet, testnet, or regtest")
            self.ZCASH_CHAIN.info({'zcash_chain': "other"})
            self.node_network = "other"

    def run_loop(self):
        while True:
            self.fetch()
            time.sleep(self.polling_interval_startup_seconds)
    
    def rpc_getinfo(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")
        try:
            zcash_info = api.getinfo()
            self.ZCASH_VERSION.info({'serverversion': str(zcash_info['version'])})
            self.ZCASH_PROTOCOL_VERSION.info({'protocolversion': str(zcash_info['protocolversion'])})
            self.ZCASH_WALLET_VERSION.info({'walletversion': str(zcash_info['walletversion'])})
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)
    
    def rpc_getblockchaininfo(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.getblockchaininfo()
            logging.info("return from rpc")
            self.ZCASH_CHAIN.info({'zcash_chain': str(zcash_info['chain'])})
            self.ZCASH_BLOCKS.set(zcash_info['blocks'])
            self.ZCASH_IBD.set(zcash_info['initial_block_download_complete'])
            self.ZCASH_HEADERS.set(zcash_info['headers'])
            self.ZCASH_BEST_BLOCK_HASH.info({'zcash_bestblockhash' : str(zcash_info['bestblockhash'])})
            self.ZCASH_DIFFICULTY.set(zcash_info['difficulty'])
            self.ZCASH_VERIFICATION_PROG.set(zcash_info['verificationprogress'])
            self.ZCASH_ESTIMATED_HEIGHT.set(zcash_info['estimatedheight'])
            self.ZCASH_CHAINWORK.info({'zcash_chainwork': str(zcash_info['chainwork'])})
            self.ZCASH_CHAIN_DISK_SIZE.set(zcash_info['size_on_disk'])
            self.ZCASH_COMMITMENTS.set(zcash_info['commitments'])
            #@todo
            # softfork
            # upgrades
            # consensues
            for i in zcash_info['valuePools']:
                if i["id"] == "orchard":
                    self.ZCASH_ORCHARD_BALANCE.set(i["chainValue"])
                if i["id"] == "sapling":
                    self.ZCASH_SAPLING_BALANCE.set(i["chainValue"])
                if i["id"] == "sprout":
                    self.ZCASH_SPROUT_BALANCE.set(i["chainValue"])           
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)

    def rpc_getmempoolinfo(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.getmempoolinfo()
            logging.info("return from rpc")
            self.ZCASH_MEMPOOL_SIZE.set(zcash_info["size"])
            self.ZCASH_MEMPOOL_BYTES.set(zcash_info["bytes"])
            self.ZCASH_MEMPOOL_USAGE.set(zcash_info["usage"])
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: ", frame.f_code.co_name)

    def rpc_gettxoutsetinfo(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.gettxoutsetinfo()
            logging.info("return from rpc")
            self.ZCASH_TXOUT_HEIGHT.set(zcash_info["height"])
            self.ZCASH_TXOUT_BESTBLOCK.info({"zcash_txout_bestblock" : str(zcash_info['bestblock'])})
            self.ZCASH_TXOUT_TRANSACTIONS.set(zcash_info["transactions"])
            self.ZCASH_TXOUT_TXOUTS.set(zcash_info["txouts"])
            self.ZCASH_TXOUT_BYTES_SERIALIZED.set(zcash_info["bytes_serialized"])
            self.ZCASH_TXOUT_HASH_SERIALIZED.info({"zcash_txout_hash_serialized" : str(zcash_info['hash_serialized'])})
            self.ZCASH_TXOUT_TOTAL_AMOUNT.set(zcash_info["total_amount"])
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)

    def rpc_getmemoryinfo(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.getmemoryinfo()
            logging.info("return from rpc")
            self.ZCASH_MEM_USED.set(zcash_info["locked"]["used"])
            self.ZCASH_MEM_FREE.set(zcash_info["locked"]["free"])
            self.ZCASH_MEM_TOTAL.set(zcash_info["locked"]["total"])
            self.ZCASH_MEM_LOCKED.set(zcash_info["locked"]["locked"])
            self.ZCASH_MEM_CHUNKS_USED.set(zcash_info["locked"]["chunks_used"])
            self.ZCASH_MEM_CHUNKS_FREE.set(zcash_info["locked"]["chunks_free"])
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)
    
    def rpc_getmininginfo(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.getmininginfo()
            logging.info("return from rpc")
            self.ZCASH_MINING_BLOCKS.set(zcash_info["blocks"])
            self.ZCASH_MINING_DIFFICULTY.set(zcash_info["difficulty"])
            #self.ZCASH_MINING_ERRORS.info()
            self.ZCASH_MINING_ERROR_TIMESTAMP.set(zcash_info["errorstimestamp"])
            self.ZCASH_MINING_GEN_PROC_LIMIT.set(zcash_info["genproclimit"])
            self.ZCASH_MINING_LOCAL_SOLPS.set(zcash_info["localsolps"])
            self.ZCASH_MINING_NETWORK_SOLPS.set(zcash_info["networksolps"])
            self.ZCASH_MINING_NETWORK_HASHPS.set(zcash_info["networkhashps"])
            self.ZCASH_MINING_POOLEDTX.set(zcash_info["pooledtx"])
            if zcash_info["testnet"] == "True":
                self.ZCASH_MINING_TESTNET.set(1)
                #SET EXPORTER VAR NOT TO GET DEPINFO
            else:
                self.ZCASH_MINING_TESTNET.set(0)
            self.ZCASH_MINING_CHAIN.info({"zcash_mining_chain" : str(zcash_info['chain'])})
            if zcash_info["generate"] == "True":
                self.ZCASH_MINING_GENERATE.set(1)
            else:
                self.ZCASH_MINING_GENERATE.set(0)
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)
    
    def rpc_getdeprecationinfo(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.getdeprecationinfo()
            logging.info("return from rpc")
            self.ZCASH_DEPRECATION_HEIGHT.set(zcash_info["end_of_service"]["block_height"])
            self.ZCASH_DEPRECATION_TIME.set(zcash_info["end_of_service"]["estimated_time"])
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)
    
    def rpc_getnettotals(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.getnettotals()
            logging.info("return from rpc")
            self.ZCASH_TOTAL_BYTES_RECV.set(zcash_info["totalbytesrecv"])
            self.ZCASH_TOTAL_BYTES_SENT.set(zcash_info["totalbytessent"])
        #todo need to check if meta is needed
        #   "timemillis": 1684792583787,
        #     "uploadtarget": {
        #         "timeframe": 86400,
        #         "target": 0,
        #         "target_reached": false,
        #         "serve_historical_blocks": true,
        #         "bytes_left_in_cycle": 0,
        #         "time_left_in_cycle": 0
        #     }
        #     }
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)

    def rpc_getconnectioncount(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.getconnectioncount()
            logging.info("return from rpc")
            self.ZCASH_CONNECTION_COUNT.set(zcash_info)
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)

    def rpc_getpeerinfo(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.getpeerinfo()
            logging.info("return from rpc")
            for i in zcash_info:
                #self.ZCASH_PEER_ID.labels(i["addr"], "geohash").set(i["id"])
                # node_ip_addr = i['addr']
                # ip = node_ip_addr.split(":")
                # g = geocoder.ip(ip[0])
                # lat_p = g.latlng[0]
                # long_p =  g.latlng[1]
                # geohash2 = pgh.encode(lat_p, long_p)
                # print(geohash2)
                # self.ZCASH_PEER_CONNECTION_TIME.labels(i["id"], geohash2, i["addrlocal"]).set(i["conntime"])
                #ZCASH_PEER_ID.labels(i["addr"]).set(i["id"])
                #self.ZCASH_PEER_SERVICES.labels(i["id"], i["addr"]).set(int(i["services"]))
                if i["relaytxes"] == True:
                    self.ZCASH_PEER_RELAYTXES.labels(i["id"], i["addr"]).set(1)
                else:
                    self.ZCASH_PEER_RELAYTXES.labels(i["id"], i["addr"]).set(0)
                self.ZCASH_PEER_LAST_SEND.labels(i["id"], i["addr"]).set(i["lastsend"])
                self.ZCASH_PEER_LAST_RECV.labels(i["id"], i["addr"]).set(i["lastrecv"])
                self.ZCASH_PEER_BYTES_SENT.labels(i["id"], i["addr"]).set(i["bytessent"])
                self.ZCASH_PEER_BYTES_RECV.labels(i["id"],i["addr"]).set(i["bytesrecv"])
                self.ZCASH_PEER_CONNECTION_TIME.labels(i["id"],i["addr"]).set(i["conntime"])
                self.ZCASH_PEER_TIME_OFFSET.labels(i["id"],i["addr"]).set(i["timeoffset"])
                self.ZCASH_PEER_PING_TIME.labels(i["id"],i["addr"]).set(i["pingtime"])
                # self.ZCASH_PEER_PING_WAIT.labels(i["id"],i["addr"]).set(i["pingwait"])
                self.ZCASH_PEER_VERSION.labels(i["id"],i["addr"]).set(i["version"])
                #self.ZCASH_PEER_SUBVERSION.labels(i["id"],i["addr"]).set(i['subversion']) #string
                if i["inbound"] == True:
                    self.ZCASH_PEER_INBOUND.labels(i["id"],i["addr"]).set(1)
                else:
                    self.ZCASH_PEER_INBOUND.labels(i["id"],i["addr"]).set(0)
                self.ZCASH_PEER_STARTING_HEIGHT.labels(i["id"],i["addr"]).set(i["startingheight"])
                self.ZCASH_PEER_BANSCORE.labels(i["id"],i["addr"]).set(i["banscore"])
                self.ZCASH_SYNCED_HEADERS.labels(i["id"],i["addr"]).set(i["synced_headers"])
                self.ZCASH_SYNCED_BLOCKS.labels(i["id"],i["addr"]).set(i["synced_blocks"])
                # #NEED TO CHECK INFLIGHT BLOCKS
                self.ZCASH_ADDR_PROCESSED.labels(i["id"],i["addr"]).set(i["addr_processed"])
                self.ZCASH_ADDR_RATE_LIMITED.labels(i["id"],i["addr"]).set(i["addr_rate_limited"])
                if i["whitelisted"] == True:
                    self.ZCASH_WHITELISTED.labels(i["id"],i["addr"]).set(1)
                else:
                    self.ZCASH_WHITELISTED.labels(i["id"],i["addr"]).set(0)
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)


    def rpc_getchaintips(self):
        api = Proxy(f"http://{self.rpc_user}:{self.rpc_password}@{self.rpc_host}:{self.rpc_port}")
        logging.info("calling zcash RPC API endpoint")

        try:
            zcash_info = api.getchaintips()
            for i in zcash_info:
                if i["branchlen"] >= 2:
                    self.ZCASH_CHAINTIP_HEIGHT.labels(i["hash"], i["status"]).set(i["height"])
                    self.ZCASH_CHAINTIP_BRANCH_LEN.labels(i["hash"], i["status"]).set(i["branchlen"])
        except Exception as e:
            frame = inspect.currentframe()
            logging.info("missed zcash RPC endpoint call: %s", frame.f_code.co_name)


    def fetch(self):
        """
        Get metrics from zcashd and refresh Prometheus metrics with new data.
        @TODO Change this to be a command set relative to env loaded 
              (e.g. don't call miner RPCs for none miner configs;
              regression testing for RPCs; combos/perm of RPCS on none wallet configs)
              - make these into functions later so we can version RPCs
        """
        self.rpc_getinfo()
        self.rpc_getblockchaininfo()
        self.rpc_getmempoolinfo() #todo might need to check if only for synced node
        #self.rpc_gettxoutsetinfo() #takes 4 seconds on synced node
        self.rpc_getmemoryinfo()
        self.rpc_getmininginfo()
        #check if on mainnet
        self.rpc_getdeprecationinfo()
        self.rpc_getpeerinfo() #can't poll on geohash or will get rate limited
        self.rpc_getchaintips()
        self.rpc_getnettotals()
        self.rpc_getconnectioncount()
        time.sleep(self.polling_interval_seconds)

        
def main():
    logging.basicConfig(format='%(asctime)s - %(message)s', level=logging.INFO)
    #Get configuration from .env
    logging.info("Loading .env file.")
    if(load_dotenv() == True):
        logging.info(".env loaded successfully")
    else:
        logging.error(".env did not load correctly.")
        return 1

    zcash_exporter = ZcashExporter(
        app_port=int(os.getenv("ZEXPORTER_PORT")),
        polling_interval_startup_seconds=int(os.getenv("ZEXPORTER_POLLING_INTERVAL_STARTUP")),
        polling_interval_seconds=int(os.getenv("ZEXPORTER_POLLING_INTERVAL")),
        rpc_user=os.getenv("ZCASHD_RPCUSER"),
        rpc_password=os.getenv("ZCASHD_RPCPASSWORD"),
        rpc_host=os.getenv("ZCASHD_RPCHOST"),
        rpc_port=int(os.getenv("ZCASHD_RPCPORT")),
        rpc_network=os.getenv("ZCASHD_NETWORK")
    )

    start_http_server(int(os.getenv("ZEXPORTER_PORT")))
    zcash_exporter.run_startup_loop()
    zcash_exporter.run_loop()

if __name__ == "__main__":
    main()