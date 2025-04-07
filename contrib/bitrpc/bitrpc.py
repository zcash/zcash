from jsonrpc import ServiceProxy
import sys
import getpass

# ===== BEGIN USER SETTINGS =====
# if you do not set these you will be prompted for a password for every command
rpcuser = ""
rpcpass = ""
# ====== END USER SETTINGS ======


if rpcpass == "":
    access = ServiceProxy("http://127.0.0.1:8232")
else:
    access = ServiceProxy("http://"+rpcuser+":"+rpcpass+"@127.0.0.1:8232")
cmd = sys.argv[1].lower()

if cmd == "backupwallet":
    try:
        path = raw_input("Enter destination path/filename: ")
        print access.backupwallet(path)
    except Exception as inst:
        print inst

elif cmd == "encryptwallet":
    try:
        pwd = getpass.getpass(prompt="Enter passphrase: ")
        pwd2 = getpass.getpass(prompt="Repeat passphrase: ")
        if pwd == pwd2:
            access.encryptwallet(pwd)
            print "\n---Wallet encrypted. Server stopping, restart to run with encrypted wallet---\n"
        else:
            print "\n---Passphrases do not match---\n"
    except Exception as inst:
        print inst

elif cmd == "getaccount":
    try:
        addr = raw_input("Enter a Bitcoin address: ")
        print access.getaccount(addr)
    except Exception as inst:
        print inst

elif cmd == "getaccountaddress":
    try:
        acct = raw_input("Enter an account name: ")
        print access.getaccountaddress(acct)
    except Exception as inst:
        print inst

elif cmd == "getaddressesbyaccount":
    try:
        acct = raw_input("Enter an account name: ")
        print access.getaddressesbyaccount(acct)
    except Exception as inst:
        print inst

elif cmd == "getbalance":
    try:
        acct = raw_input("Enter an account (optional): ")
        mc = raw_input("Minimum confirmations (optional): ")
        try:
            print access.getbalance(acct, mc)
        except:
            print access.getbalance()
    except Exception as inst:
        print inst

elif cmd == "getblockbycount":
    try:
        height = raw_input("Height: ")
        print access.getblockbycount(height)
    except Exception as inst:
        print inst

elif cmd == "getblockcount":
    try:
        print access.getblockcount()
    except Exception as inst:
        print inst

elif cmd == "getblocknumber":
    try:
        print access.getblocknumber()
    except Exception as inst:
        print inst

elif cmd == "getconnectioncount":
    try:
        print access.getconnectioncount()
    except Exception as inst:
        print inst

elif cmd == "getdifficulty":
    try:
        print access.getdifficulty()
    except Exception as inst:
        print inst

elif cmd == "getgenerate":
    try:
        print access.getgenerate()
    except Exception as inst:
        print inst

elif cmd == "gethashespersec":
    try:
        print access.gethashespersec()
    except Exception as inst:
        print inst

elif cmd == "getinfo":
    try:
        print access.getinfo()
    except Exception as inst:
        print inst

elif cmd == "getnewaddress":
    try:
        acct = raw_input("Enter an account name: ")
        try:
            print access.getnewaddress(acct)
        except:
            print access.getnewaddress()
    except Exception as inst:
        print inst

elif cmd == "getreceivedbyaccount":
    try:
        acct = raw_input("Enter an account (optional): ")
        mc = raw_input("Minimum confirmations (optional): ")
        try:
            print access.getreceivedbyaccount(acct, mc)
        except:
            print access.getreceivedbyaccount()
    except Exception as inst:
        print inst

elif cmd == "getreceivedbyaddress":
    try:
        addr = raw_input("Enter a Bitcoin address (optional): ")
        mc = raw_input("Minimum confirmations (optional): ")
        try:
            print access.getreceivedbyaddress(addr, mc)
        except:
            print access.getreceivedbyaddress()
    except Exception as inst:
        print inst

elif cmd == "gettransaction":
    try:
        txid = raw_input("Enter a transaction ID: ")
        print access.gettransaction(txid)
    except Exception as inst:
        print inst

elif cmd == "getwork":
    try:
        data = raw_input("Data (optional): ")
        try:
            print access.gettransaction(data)
        except:
            print access.gettransaction()
    except Exception as inst:
        print inst

elif cmd == "help":
    try:
        cmd = raw_input("Command (optional): ")
        try:
            print access.help(cmd)
        except:
            print access.help()
    except Exception as inst:
        print inst

elif cmd == "listaccounts":
    try:
        mc = raw_input("Minimum confirmations (optional): ")
        try:
            print access.listaccounts(mc)
        except:
            print access.listaccounts()
    except Exception as inst:
        print inst

elif cmd == "listreceivedbyaccount":
    try:
        mc = raw_input("Minimum confirmations (optional): ")
        incemp = raw_input("Include empty? (true/false, optional): ")
        try:
            print access.listreceivedbyaccount(mc, incemp)
        except:
            print access.listreceivedbyaccount()
    except Exception as inst:
        print inst

elif cmd == "listreceivedbyaddress":
    try:
        mc = raw_input("Minimum confirmations (optional): ")
        incemp = raw_input("Include empty? (true/false, optional): ")
        try:
            print access.listreceivedbyaddress(mc, incemp)
        except:
            print access.listreceivedbyaddress()
    except Exception as inst:
        print inst

elif cmd == "listtransactions":
    try:
        acct = raw_input("Account (optional): ")
        count = raw_input("Number of transactions (optional): ")
        frm = raw_input("Skip (optional):")
        try:
            print access.listtransactions(acct, count, frm)
        except:
            print access.listtransactions()
    except Exception as inst:
        print inst

elif cmd == "move":
    try:
        frm = raw_input("From: ")
        to = raw_input("To: ")
        amt = raw_input("Amount:")
        mc = raw_input("Minimum confirmations (optional): ")
        comment = raw_input("Comment (optional): ")
        try:
            print access.move(frm, to, amt, mc, comment)
        except:
            print access.move(frm, to, amt)
    except Exception as inst:
        print inst

elif cmd == "sendfrom":
    try:
        frm = raw_input("From: ")
        to = raw_input("To: ")
        amt = raw_input("Amount:")
        mc = raw_input("Minimum confirmations (optional): ")
        comment = raw_input("Comment (optional): ")
        commentto = raw_input("Comment-to (optional): ")
        try:
            print access.sendfrom(frm, to, amt, mc, comment, commentto)
        except:
            print access.sendfrom(frm, to, amt)
    except Exception as inst:
        print inst

elif cmd == "sendmany":
    try:
        frm = raw_input("From: ")
        to = raw_input("To (in format address1:amount1,address2:amount2,...): ")
        mc = raw_input("Minimum confirmations (optional): ")
        comment = raw_input("Comment (optional): ")
        try:
            print access.sendmany(frm,to,mc,comment)
        except:
            print access.sendmany(frm,to)
    except Exception as inst:
        print inst

elif cmd == "sendtoaddress":
    try:
        to = raw_input("To (in format address1:amount1,address2:amount2,...): ")
        amt = raw_input("Amount:")
        comment = raw_input("Comment (optional): ")
        commentto = raw_input("Comment-to (optional): ")
        try:
            print access.sendtoaddress(to,amt,comment,commentto)
        except:
            print access.sendtoaddress(to,amt)
    except Exception as inst:
        print inst

elif cmd == "setaccount":
    try:
        addr = raw_input("Address: ")
        acct = raw_input("Account:")
        print access.setaccount(addr,acct)
    except Exception as inst:
        print inst

elif cmd == "setgenerate":
    try:
        gen= raw_input("Generate? (true/false): ")
        cpus = raw_input("Max processors/cores (-1 for unlimited, optional):")
        try:
            print access.setgenerate(gen, cpus)
        except:
            print access.setgenerate(gen)
    except Exception as inst:
        print inst

elif cmd == "settxfee":
    try:
        amt = raw_input("Amount:")
        print access.settxfee(amt)
    except Exception as inst:
        print inst

elif cmd == "stop":
    try:
        print access.stop()
    except Exception as inst:
        print inst

elif cmd == "validateaddress":
    try:
        addr = raw_input("Address: ")
        print access.validateaddress(addr)
    except Exception as inst:
        print inst

elif cmd == "walletpassphrase":
    try:
        pwd = getpass.getpass(prompt="Enter wallet passphrase: ")
        access.walletpassphrase(pwd, 60)
        print "\n---Wallet unlocked---\n"
    except Exception as inst:
        print inst

elif cmd == "walletpassphrasechange":
    try:
        pwd = getpass.getpass(prompt="Enter old wallet passphrase: ")
        pwd2 = getpass.getpass(prompt="Enter new wallet passphrase: ")
        access.walletpassphrasechange(pwd, pwd2)
        print
        print "\n---Passphrase changed---\n"
    except Exception as inst:
        print inst

else:
    print "Command not found or not supported"
