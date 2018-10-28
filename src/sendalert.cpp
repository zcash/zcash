// Copyright (c) 2016-2018 The Zcash developers
// Original code from: https://gist.github.com/laanwj/0e689cfa37b52bcbbb44

/*

To set up a new alert system
----------------------------

Create a new alert key pair:
openssl ecparam -name secp256k1 -genkey -param_enc explicit -outform PEM -out data.pem

Get the private key in hex:
openssl ec -in data.pem -outform DER | tail -c 279 | xxd -p -c 279

Get the public key in hex:
openssl ec -in data.pem -pubout -outform DER | tail -c 65 | xxd -p -c 65

Update the public keys found in chainparams.cpp.


To send an alert message
------------------------

Copy the private keys into alertkeys.h.

Modify the alert parameters, id and message found in this file.

Build and run with -sendalert or -printalert.

./zcashd -printtoconsole -sendalert

One minute after starting up, the alert will be broadcast. It is then
flooded through the network until the nRelayUntil time, and will be
active until nExpiration OR the alert is cancelled.

If you make a mistake, send another alert with nCancel set to cancel
the bad alert.

*/

#include "main.h"
#include "net.h"
#include "alert.h"
#include "init.h"

#include "util.h"
#include "utiltime.h"
#include "key.h"
#include "clientversion.h"
#include "chainparams.h"

#include "alertkeys.h"


static const int64_t DAYS = 24 * 60 * 60;

void ThreadSendAlert()
{
    if (!mapArgs.count("-sendalert") && !mapArgs.count("-printalert"))
        return;

    MilliSleep(60*1000); // Wait a minute so we get connected

    //
    // Alerts are relayed around the network until nRelayUntil, flood
    // filling to every node.
    // After the relay time is past, new nodes are told about alerts
    // when they connect to peers, until either nExpiration or
    // the alert is cancelled by a newer alert.
    // Nodes never save alerts to disk, they are in-memory-only.
    //
    CAlert alert;
    alert.nRelayUntil   = GetTime() + 15 * 60;
    alert.nExpiration   = GetTime() + 10 * 365 * 24 * 60 * 60;
    alert.nID           = 1006;  // use https://github.com/zcash/zcash/wiki/specification#assigned-numbers to keep track of alert IDs
    alert.nCancel       = 1005;  // cancels previous messages up to this ID number

    // These versions are protocol versions
    // 170002 : 1.0.0
    // 170006 : 1.1.2
    // 170007 : 2.0.0
    alert.nMinVer       = 170002;
    alert.nMaxVer       = 170006;

    //
    // main.cpp:
    //  1000 for Misc warnings like out of disk space and clock is wrong
    //  2000 for longer invalid proof-of-work chain
    //  Higher numbers mean higher priority
    //  4000 or higher will put the RPC into safe mode
    alert.nPriority     = 4000;
    alert.strComment    = "";
    alert.strStatusBar  = "Your client is out of date and incompatible with the Sapling network upgrade. Please update to a recent version of Zcash (2.0.1 or later).";
    alert.strRPCError   = alert.strStatusBar;

    // Set specific client version/versions here. If setSubVer is empty, no filtering on subver is done:
    // alert.setSubVer.insert(std::string("/MagicBean:0.7.2/"));
    const std::vector<std::string> useragents = {}; //{"MagicBean", "BeanStalk", "AppleSeed", "EleosZcash"};

    BOOST_FOREACH(const std::string& useragent, useragents) {
    }

    // Sanity check
    assert(alert.strComment.length() <= 65536); // max length in alert.h
    assert(alert.strStatusBar.length() <= 256);
    assert(alert.strRPCError.length() <= 256);

    // Sign
    const CChainParams& chainparams = Params();
    std::string networkID = chainparams.NetworkIDString();
    bool fIsTestNet = networkID.compare("test") == 0;
    std::vector<unsigned char> vchTmp(ParseHex(fIsTestNet ? pszTestNetPrivKey : pszPrivKey));
    CPrivKey vchPrivKey(vchTmp.begin(), vchTmp.end());

    CDataStream sMsg(SER_NETWORK, CLIENT_VERSION);
    sMsg << *(CUnsignedAlert*)&alert;
    alert.vchMsg = std::vector<unsigned char>(sMsg.begin(), sMsg.end());
    CKey key;
    if (!key.SetPrivKey(vchPrivKey, false))
    {
        printf("ThreadSendAlert() : key.SetPrivKey failed\n");
        return;
    }
    if (!key.Sign(Hash(alert.vchMsg.begin(), alert.vchMsg.end()), alert.vchSig))
    {
        printf("ThreadSendAlert() : key.Sign failed\n");
        return;
    }

    // Test
    CDataStream sBuffer(SER_NETWORK, CLIENT_VERSION);
    sBuffer << alert;
    CAlert alert2;
    sBuffer >> alert2;
    if (!alert2.CheckSignature(chainparams.AlertKey()))
    {
        printf("ThreadSendAlert() : CheckSignature failed\n");
        return;
    }
    assert(alert2.vchMsg == alert.vchMsg);
    assert(alert2.vchSig == alert.vchSig);
    alert.SetNull();
    printf("\nThreadSendAlert:\n");
    printf("hash=%s\n", alert2.GetHash().ToString().c_str());
    printf("%s\n", alert2.ToString().c_str());
    printf("vchMsg=%s\n", HexStr(alert2.vchMsg).c_str());
    printf("vchSig=%s\n", HexStr(alert2.vchSig).c_str());

    // Confirm
    if (!mapArgs.count("-sendalert"))
        return;
    while (vNodes.size() < 1 && !ShutdownRequested())
        MilliSleep(500);
    if (ShutdownRequested())
        return;

    // Send
    printf("ThreadSendAlert() : Sending alert\n");
    int nSent = 0;
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if (alert2.RelayTo(pnode))
            {
                printf("ThreadSendAlert() : Sent alert to %s\n", pnode->addr.ToString().c_str());
                nSent++;
            }
        }
    }
    printf("ThreadSendAlert() : Alert sent to %d nodes\n", nSent);
}
