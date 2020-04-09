# /************************************************************************
#  File: fetch-pyarams.py
#  Author: mdr0id
#  Date: 1/18/2019
#  Description:  Used to distribute ceremony parameters, per platform, 
#                for zcashd (Python3)
#
#  Usage: python3 fetch-pyarams.py
#
#          OR
#
#         python3 fetch-pyarams.py --ipfs
#         
#         Generate binary for a given platform using pyinstaller module :
#
#         pyinstaller --onefile fetch-pyarams.py
#
#         <binary will be located in /dist>
#
#  Notes:
#  Added support for IPFS
# 
#  Known bugs/missing features:
#
# ************************************************************************/

import platform
import requests
import hashlib
import os
import logging
import argparse
from tqdm import tqdm
from subprocess import Popen, PIPE

logging.basicConfig(format='%(asctime)s - PID:%(process)d - %(levelname)s: %(message)s', level=logging.INFO)

PARAM_FILES = {
    "sapling-spend.params" : "8e48ffd23abb3a5fd9c5589204f32d9c31285a04b78096ba40a79b75677efc13",
    "sapling-output.params" : "2f0ebbcbb9bb0bcffe95a397e7eba89c29eb4dde6191c339db88570e3f3fb0e4",
    "sprout-groth16.params" : "b685d700c60328498fbde589c8c7c484c722b788b265b72af448a5bf0ee55b50"
}


PARAMS_URL = "https://z.cash/downloads/"
PARAMS_IPFS = "/ipfs/QmUSFo5zgPPXXejidzFWZcxVyF3AJH6Pr9br6Xisdww1r1/"
PARAMS_IPFS_ALT1 = "/ipfs/QmXRHVGLQBiKwvNq7c2vPxAKz1zRVmMYbmt7G5TQss7tY7"

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
    logging.debug("Host: %s", HOST_OS)
    logging.debug("Parameter path: %s", PARAMS_DIR)

    def lock(file, flags):
        try:
            hfile = win32file._get_osfhandle(file.fileno())
            win32file.LockFileEx(hfile, flags, 0, 0xffff0000, __overlapped)
        except:
            logging.exception("Unable to lock : %s", file)

    def unlock(file):
        try:
            hfile = win32file._get_osfhandle(file.fileno())
            win32file.UnlockFileEx(hfile, 0, 0xffff0000, __overlapped)
        except:
            logging.exception("Unable to unlock : %s", file)

elif HOST_OS == 'Linux':
    import fcntl

    PARAMS_DIR += r"/.zcash-params/"
    LOCK_SH = fcntl.LOCK_SH
    LOCK_NB = fcntl.LOCK_NB
    LOCK_EX = fcntl.LOCK_EX
    logging.debug("Host: %s", HOST_OS)
    logging.debug("Parameter path: %s", PARAMS_DIR)

    def lock(file, flags):
        try:
            fcntl.flock(file.fileno(), flags)
        except:
            logging.exception("Unable to lock : %s", file)

    def unlock(file):
        try:
            fcntl.flock(file.fileno(), fcntl.LOCK_UN)
        except:
            logging.exception("Unable to unlock : %s", file)

elif HOST_OS == 'Darwin':
    import fcntl

    PARAMS_DIR += r"/Library/Application Support/ZcashParams/"
    LOCK_SH = fcntl.LOCK_SH
    LOCK_NB = fcntl.LOCK_NB
    LOCK_EX = fcntl.LOCK_EX
    logging.debug("Host: %s", HOST_OS)
    logging.debug("Parameter path: %s", PARAMS_DIR)
    
    def lock(file, flags):
        try:
            fcntl.flock(file.fileno(), flags)
        except:
            logging.exception("Unable to lock : %s", file)

    def unlock(file):
        try:
            fcntl.flock(file.fileno(), fcntl.LOCK_UN)
        except:
            logging.exception("Unable to unlock : %s", file)
else:
    logging.error(" %s is not currently supported.", HOST_OS)


def use_ipfs(filename):
    try:
        print("DESTINATION: ", PARAMS_DIR + filename)
        print("REQ: ", PARAMS_IPFS + filename )
        if HOST_OS == 'Windows':
            p1 = Popen(['ipfs.exe', "get", "--output", PARAMS_DIR + filename, PARAMS_IPFS + filename])
        else:
            p1 = Popen(['ipfs', "get", "--output", PARAMS_DIR + filename, PARAMS_IPFS + filename])
        buffer = p1.communicate()[0]
    except:
        print("ERROR in IPFS process call")

    if p1.returncode != 0:
        print("Likely got an error from subprocess")

def use_http(filename):
    use_https(filename)

def use_https(filename):
    path = PARAMS_DIR + filename + ".dl"
    response = requests.get(PARAMS_URL + filename , stream=True)
    size = 0
    total_size = int(response.headers['Content-Length'])
    block_size = 1024

    logging.info("Retrieving : %s", PARAMS_URL + filename)
    logging.info("Length : ~ %s KB [ %s ]", str(total_size /  block_size), response.headers['Content-Type'] )
    logging.info("Saving to : %s", path)
    logging.debug(response.headers)
   
    with open(path, "wb") as handle:
        lock(handle, LOCK_EX )
        for chunk in tqdm(response.iter_content(block_size), total= int(total_size // block_size), unit="KB", desc=filename + ".dl"):
            if chunk:
                handle.write(chunk)
                size += len(chunk)
            else:
                logging.warning("Received response that was not content data.")
        unlock(handle)
    
    logging.info(" '%s' saved [%d/%d]", filename, size, total_size)
    
    if size == total_size :
        verify_file(path, PARAM_FILES.get(filename), DOWNLOADING)

def download_file(filename, protocol):
    if protocol == "HTTPS":
        use_https(filename)
    elif protocol == "HTTP":
        use_http(filename)
    elif protocol == "IPFS":
        use_ipfs(filename)
    else:
        logging.error("%s is not currently supported for retrieving Zcash params.\n", protocol)

def verify_file(filename, sha256, download_state):
    logging.debug("Checking SHA256 for: %s", filename) 
    with open(filename, 'rb') as f:
        try:
    	    contents = f.read()
        except:
            logging.exception("Unable to read in data blocks to verify SHA256 for: %s", filename)
        
        local_sha256 =  hashlib.sha256(contents).hexdigest()
        if local_sha256 != sha256 :
            logging.error("Download failed: SHA256 on %s does NOT match.", filename)
            return
            
    if download_state == DOWNLOADING :
        logging.info("Download successful!")
        logging.info("%s: OK", filename)
        
        try:
            os.rename(filename, filename[:-3])
        except:
            logging.exception("Unable to rename file.")

        logging.info("renamed '%s' -> '%s' \n", filename, filename[:-3])
    
    if download_state == DOWNLOADED :
        logging.info("%s: OK", filename)

def get_params(param_file_list, protocol):
    for filename in param_file_list:
        download_file(filename, protocol)

def check_params(param_file_list, protocol):
    for key in param_file_list:
        if os.path.exists(PARAMS_DIR + key) == True :
            verify_file(PARAMS_DIR + key , param_file_list.get(key), DOWNLOADED )
        else :
            logging.warning("%s does not exist and will now be downloaded...", PARAMS_DIR + key )
            download_file( key, protocol)
            
def create_readme():
    try:
        os.mkdir(PARAMS_DIR)
        f = open(PARAMS_DIR + "README", "w+")
        f.write("This directory stores common Zcash zkSNARK parameters. Note that it is \n")
        f.write("distinct from the daemon's -datadir argument because the parameters are \n")
        f.write("large and may be shared across multiple distinct -datadir's such as when \n")
        f.write("setting up test networks.")
        f.close()
    except OSError as e:
        logging.exception("Exception occured")

def print_intro():
    print("Zcash - fetch-params.py \n")
    print("This script will fetch the Zcash zkSNARK parameters and verify their integrity with sha256sum. \n")
    print("If they already exist locally, it will verify SHAs and exit. \n ")

def print_intro_info():
    print("The complete parameters are currently just under 1.7GB in size, so plan") 
    print("accordingly for your bandwidth constraints. If the Sprout parameters are")
    print("already present the additional Sapling parameters required are just under")
    print("800MB in size. If the files are already present and have the correct ")
    print("sha256sum, no networking is used. \n\n")
    print("Creating params directory. For details about this directory, see: \n ")
    print(PARAMS_DIR + "README" + "\n")      

def main():
    print_intro()
    parser = argparse.ArgumentParser(description="Zcashd fetch-params")
    parser.add_argument('args', nargs=argparse.REMAINDER)
    parser.add_argument('--ipfs', '-ipfs', action='store_true', help='Use IPFS to download Zcash key and param files.')
    args = parser.parse_args()

    protocol_type = ""
    if args.ipfs:
        protocol_type = "IPFS"
    else:
        protocol_type = "HTTPS"

    if os.path.exists(PARAMS_DIR) == False:
        create_readme()
        print_intro_info()
        get_params(PARAM_FILES, protocol_type)
    else:
        check_params(PARAM_FILES, protocol_type)
    
if __name__ == "__main__":
    main()
