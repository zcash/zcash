# /************************************************************************
#  File: fetch-params.py
#  Author: mdr0id
#  Date: 1/18/2019
#  Description:  Used to distribute ceremony parameters, per platform, 
#                for zcashd
#
#  Usage: python3 fetch-params.py
#
#         OR USING IPFS:
#
#         python3 fetch-params.py --ipfs
#         
#         To generate a binary for a given platform using pyinstaller module :
#
#         pip3 install pyinstaller
#
#         pyinstaller --onefile fetch-params.py
#
#         <binary will be located in /dist of current working directory>
#
#
# Known Bugs:
#
# - IPFS Daemon: ERROR core: failure on stop:  context canceled; TODO: mock repo builder.go:47
#   see https://github.com/ipfs/go-ipfs/issues/6356 for details
#
# ************************************************************************/
import sys
import platform
import hashlib
import os
import argparse
from subprocess import Popen, PIPE

try:
    from logzero import logger
except ImportError as error:
    print(error.__class__.__name__, " : ", error)
    print(sys.version)

try:
    import requests
except ImportError as error:
    logger.exception(error.__class__.__name__, " : ", error)
    logger.debug(sys.version)

try:
    from tqdm import tqdm
except ImportError as error:
    logger.exception(error.__class__.__name__, " : ", error)
    logger.debug(sys.version)

# Set after all none standard Python module are imported successfully
# Comment out below line for ALL log messages 
logger.setLevel(level=20)

PARAMS = {
    "sapling-spend.params" : {
        "sha256" : "8e48ffd23abb3a5fd9c5589204f32d9c31285a04b78096ba40a79b75677efc13",
        "ipfs_bhash" : "QmaaA4e7C4QkxrooomzmrjdFgv6WXPGpizj6gcKxdRUnkW"
    },
    "sapling-output.params" : {
        "sha256" : "2f0ebbcbb9bb0bcffe95a397e7eba89c29eb4dde6191c339db88570e3f3fb0e4",
        "ipfs_bhash" : "QmQ8E53Fpp1q1zXvsqscskaQXrgyqfac9b3AqLxFxCubAz"
    },
    "sprout-groth16.params" : {
        "sha256" : "b685d700c60328498fbde589c8c7c484c722b788b265b72af448a5bf0ee55b50",
        "ipfs_bhash" : "QmWFuKQ1JgwJBrqYgGHDEmGSXR9pbkv51NYC1yNUaWwpMU"
    }
}

PARAMS_URL = "https://z.cash/downloads/"

DOWNLOADING = 1
DOWNLOADED = 0

#get system information to setup according platform lock and unlock methods
HOST_OS = platform.system()

#get host platform path for current user
PARAMS_DIR = os.path.expanduser('~')

if HOST_OS == 'Windows':
    import win32con, win32file, pywintypes

    PARAMS_DIR += "\\AppData\\Roaming\\ZcashParams\\"
    LOCK_SH = 0 # default flag
    LOCK_NB = win32con.LOCKFILE_FAIL_IMMEDIATELY
    LOCK_EX = win32con.LOCKFILE_EXCLUSIVE_LOCK
    __overlapped = pywintypes.OVERLAPPED()
    logger.debug("Host: %s", HOST_OS)
    logger.debug("Parameter path: %s", PARAMS_DIR)

    def lock(file, flags):
        try:
            hfile = win32file._get_osfhandle(file.fileno())
            win32file.LockFileEx(hfile, flags, 0, 0xffff0000, __overlapped)
        except:
            logger.exception("Unable to lock : %s", file)

    def unlock(file):
        try:
            hfile = win32file._get_osfhandle(file.fileno())
            win32file.UnlockFileEx(hfile, 0, 0xffff0000, __overlapped)
        except:
            logger.exception("Unable to unlock : %s", file)

elif HOST_OS == 'Linux':
    import fcntl

    PARAMS_DIR += r"/.zcash-params/"
    LOCK_SH = fcntl.LOCK_SH
    LOCK_NB = fcntl.LOCK_NB
    LOCK_EX = fcntl.LOCK_EX
    logger.debug("Host: %s", HOST_OS)
    logger.debug("Parameter path: %s", PARAMS_DIR)

    def lock(file, flags):
        try:
            fcntl.flock(file.fileno(), flags)
        except:
            logger.exception("Unable to lock : %s", file)

    def unlock(file):
        try:
            fcntl.flock(file.fileno(), fcntl.LOCK_UN)
        except:
            logger.exception("Unable to unlock : %s", file)

elif HOST_OS == 'Darwin':
    import fcntl

    PARAMS_DIR += r"/Library/Application Support/ZcashParams/"
    LOCK_SH = fcntl.LOCK_SH
    LOCK_NB = fcntl.LOCK_NB
    LOCK_EX = fcntl.LOCK_EX
    logger.debug("Host: %s", HOST_OS)
    logger.debug("Parameter path: %s", PARAMS_DIR)
    
    def lock(file, flags):
        try:
            fcntl.flock(file.fileno(), flags)
        except:
            logger.exception("Unable to lock : %s", file)

    def unlock(file):
        try:
            fcntl.flock(file.fileno(), fcntl.LOCK_UN)
        except:
            logger.exception("Unable to unlock : %s", file)
else:
    logger.error(" %s is not currently supported.", HOST_OS)

def is_ipfs_daemon_running():
    #Due to corner cases of graceful ipfs daemon shutdown via CLI,
    #we check the ipfs swarm to see if it has a valid network state.
    #Future IPFS CLI updates should address this with `ipfs status`

    if HOST_OS == 'Windows':
        p1 = Popen(['ipfs.exe', "swarm", "addrs"], stderr=PIPE, stdout=PIPE)
    else:
        p1 = Popen(['ipfs', "swarm","addrs"], stderr=PIPE, stdout=PIPE)
    stdout, stderr =  p1.communicate()

    if p1.returncode == 1:
        logger.error("IPFS Daemon is not running. Please start it and try again.")
        return False
    else:
        return True

def use_ipfs(filename):
    logger.debug("Writing to: %s" % PARAMS_DIR + filename)
    logger.info("Requesting %s from: /ipfs/%s" % (filename,PARAMS[filename]['ipfs_bhash']))
    if HOST_OS == 'Windows':
        p1 = Popen(['ipfs.exe', "get", "--output", PARAMS_DIR + filename, "/ipfs/" + PARAMS[filename]['ipfs_bhash']], stdout=PIPE)
    else:
        p1 = Popen(['ipfs', "get", "--output", PARAMS_DIR + filename, "/ipfs/" + PARAMS[filename]['ipfs_bhash']] , stdout=PIPE)
    buffer = p1.communicate()[0]

    if p1.returncode == 1:
        logger.error(buffer)
    
    verify_ipfs(filename, DOWNLOADING)

def use_https(filename):
    path = PARAMS_DIR + filename + ".dl"
    response = requests.get(PARAMS_URL + filename , stream=True)
    size = 0
    total_size = int(response.headers['Content-Length'])
    block_size = 1024

    logger.info("Retrieving : %s", PARAMS_URL + filename)
    logger.info("Length : ~ %s KB [ %s ]", str(total_size /  block_size), response.headers['Content-Type'] )
    logger.info("Saving to : %s", path)
    logger.debug(response.headers)
   
    with open(path, "wb") as handle:
        lock(handle, LOCK_EX )
        for chunk in tqdm(response.iter_content(block_size), total= int(total_size // block_size), unit="KB", desc=filename + ".dl"):
            if chunk:
                handle.write(chunk)
                size += len(chunk)
            else:
                logger.warning("Received response that was not content data.")
        unlock(handle)
    
    logger.info(" '%s' saved [%d/%d]", filename, size, total_size)
    
    if size == total_size :
        verify_file(path, PARAMS[filename]["sha256"], DOWNLOADING)

def download_file(filename, protocol):
    if protocol == "HTTPS":
        use_https(filename)
    elif protocol == "IPFS":
        use_ipfs(filename)
    else:
        logger.error("%s is not currently supported for retrieving Zcash params.\n", protocol)

def verify_ipfs(filename, download_state):
    #Generate the IPFS to verify, but don't actually add the file
    if HOST_OS == 'Windows':
        p2 = Popen(['ipfs.exe', "add", "--n", "--Q", PARAMS_DIR + filename], stdout=PIPE)
    else:
        p2 = Popen(['ipfs', "add", "--n", "--Q", PARAMS_DIR + filename], stdout=PIPE)
    buffer = p2.communicate()[0]

    if buffer.decode("utf-8").replace('\n','') != PARAMS[filename]['ipfs_bhash']:
        logger.error("Download failed: Multihash on %s does NOT match.", filename)
        return
    
    if download_state == DOWNLOADING :
        logger.info("Download successful!")
        logger.info("%s: OK", filename)

    if download_state == DOWNLOADED :
        logger.info("%s: OK", filename)


def verify_file(filename, sha256, download_state):
    logger.info("Checking SHA256 for: %s", filename) 
    with open(filename, 'rb') as f:
        try:
    	    contents = f.read()
        except:
            logger.exception("Unable to read in data blocks to verify SHA256 for: %s", filename)
        
        local_sha256 =  hashlib.sha256(contents).hexdigest()
        if local_sha256 != sha256 :
            logger.error("Download failed: SHA256 on %s does NOT match.", filename)
            return
            
    if download_state == DOWNLOADING :
        logger.info("Download successful!")
        
        try:
            os.rename(filename, filename[:-3])
        except:
            logger.exception("Unable to rename file:", filename)

        logger.debug("Renamed '%s' -> '%s' \n", filename, filename[:-3])
    
    if download_state == DOWNLOADED :
        logger.info("%s: OK", filename)

def get_params(protocol):
    for filename in PARAMS:
        download_file(filename, protocol)

def check_params( protocol):
    for key in PARAMS:
        if os.path.exists(PARAMS_DIR + key) == True and protocol == "HTTPS":
            verify_file(PARAMS_DIR + key , PARAMS[key]["sha256"], DOWNLOADED )
        elif os.path.exists(PARAMS_DIR + key) == True and protocol == "IPFS":
            verify_ipfs(key , DOWNLOADED )
        else :
            logger.warning("%s does not exist and will now be downloaded...", PARAMS_DIR + key )
            download_file( key, protocol)
            
def create_readme():
    try:
        os.mkdir(PARAMS_DIR)
        f = open(PARAMS_DIR + "README", "w+")
        f.write('''This directory stores common Zcash zkSNARK parameters. Note that it is \n'''
                '''distinct from the daemon's -datadir argument because the parameters are \n'''
                '''large and may be shared across multiple distinct -datadir's such as when \n'''
                '''setting up test networks.''')
        f.close()
    except OSError as e:
        logger.exception("Exception occured:", e)

def print_intro():
    print('''Zcash - fetch-params.py \n\n''' 
          '''This script will fetch the Zcash zkSNARK parameters and verify their  \n'''
          '''integrity with sha256sum. \n\n'''
          '''If they already exist locally, it will verify SHAs and exit. \n''')

def print_intro_info():
    print('''The complete parameters are currently just under 800MB in size, so plan \n'''
          '''accordingly for your bandwidth constraints. If the files are already \n'''
          '''present and have the correct sha256sum, no networking is used. \n\n'''
          '''Creating params directory. For details about this directory, see: \n''',
          PARAMS_DIR + '''README \n''')      

def main():
    print_intro()
    parser = argparse.ArgumentParser(description="Zcash fetch-params")
    parser.add_argument('args', nargs=argparse.REMAINDER)
    parser.add_argument('--ipfs', '-ipfs', action='store_true', help='Use IPFS to download Zcash param files.')
    args = parser.parse_args()

    protocol_type = ""
    if args.ipfs:
        protocol_type = "IPFS"
        if is_ipfs_daemon_running() == False:
            return
    else:
        protocol_type = "HTTPS"

    if os.path.exists(PARAMS_DIR) == False:
        create_readme()
        print_intro_info()
        get_params(protocol_type)
    else:
        check_params(protocol_type)
    
if __name__ == "__main__":
    main()
