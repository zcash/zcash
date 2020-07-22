# /************************************************************************
#  File: tnbox.py
#  Author: mdr0id
#  Description: tnbox.py
#
#  Usage: 
#
#  Known Bugs:
#
# ************************************************************************/
import os
import secrets

def change_net_magic(filename):
    class_net_name = "class CTestNetParams : public CChainParams"
    net_magic_num0 = "pchMessageStart[0]"
    net_magic_num1 = "pchMessageStart[1]"
    net_magic_num2 = "pchMessageStart[2]"
    net_magic_num3 = "pchMessageStart[3]"
    has_class_scope = False
    line_num = 0
    
    try:
        fin = open(filename, "r")
    except OSError:
        print("Could not open file:", filename)
        os.sys.exit(1)

    data = fin.readlines()

    for line in data:
        if has_class_scope:
            if net_magic_num0 in line:
                data[line_num] = "\t\t" + net_magic_num0 + str(" = %#x;\n" % ord(os.urandom(1)))
            if net_magic_num1 in line:
                data[line_num] = "\t\t" + net_magic_num1 + str(" = %#x;\n" % ord(os.urandom(1)))
            if net_magic_num2 in line:
                data[line_num] = "\t\t" + net_magic_num2 + str(" = %#x;\n" % ord(os.urandom(1)))
            if net_magic_num3 in line:
                data[line_num] = "\t\t" + net_magic_num3 + str(" = %#x;\n" % ord(os.urandom(1)))
                has_class_scope = False
        if class_net_name in line:
            has_class_scope = True
        line_num += 1
    fin.close()

    try:
        fout = open(filename, "w")
    except OSError:
        print("Could not open file for write:", filename)
        os.sys.exit(1)
    fout.writelines(data)
    fout.close()

def change_branchid(filename):
    #uphold ZIP 200 for heartwood branchid = 0xf5b9230b
    #Canopy 0xe9ff75a6
    net_info_struct = "const struct NUInfo NetworkUpgradeInfo"
    upcoming_nu ="\"Canopy\""
    has_class_scope = False
    line_num=0

    try:
        fin = open(filename, "r")
    except OSError:
        print("Could not open file:", filename)
        os.sys.exit(1)

    data = fin.readlines()

    for line in data:
        if has_class_scope:
            if upcoming_nu in line:
                branchID = secrets.token_hex(4)
                while branchID == 'e9ff75a6':
                    branchID = secrets.token_hex(4)
                data[line_num-1] = "\t\t" + "/*.nBranchId =*/ 0x" + branchID + ",\n"
        if net_info_struct in line:
            has_class_scope = True
        line_num += 1    
    fin.close()

    try:
        fout = open(filename, "w")
    except OSError:
        print("Could not open file for write:", filename)
        os.sys.exit(1)
    fout.writelines(data)
    fout.close()

def main():
    net_magic_file = "./../../src/chainparams.cpp"
    net_branchid_file = "./../../src/consensus/upgrades.cpp"

    change_net_magic(net_magic_file)
    change_branchid(net_branchid_file)
    print("Zcash testnet-in-a-box staging complete.")

if __name__ == "__main__":
    main()