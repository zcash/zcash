// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "clientversion.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/upgrades.h"
#include "core_io.h"
#include "key_io.h"
#include "keystore.h"
#include "policy/policy.h"
#include "primitives/transaction.h"
#include "script/script.h"
#include "script/sign.h"
#include <univalue.h>
#include "util/system.h"
#include "util/moneystr.h"
#include "util/strencodings.h"

#include <stdio.h>

#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>

static bool fCreateBlank;
static std::map<std::string,UniValue> registers;
static const int CONTINUE_EXECUTION=-1;

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

//
// This function returns either one of EXIT_ codes when it's expected to stop the process or
// CONTINUE_EXECUTION when it's expected to continue further.
//
static int AppInitRawTx(int argc, char* argv[])
{
    //
    // Parameters
    //
    ParseParameters(argc, argv);

    // Check for -testnet or -regtest parameter (Params() calls are only valid after this clause)
    try {
        SelectParams(ChainNameFromCommandLine());
    } catch(std::exception &e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return false;
    }

    fCreateBlank = GetBoolArg("-create", false);

    if (argc<2 || mapArgs.count("-?") || mapArgs.count("-h") || mapArgs.count("-help"))
    {
        // First part of help message is specific to this utility
        std::string strUsage = _("Zcash zcash-tx utility version") + " " + FormatFullVersion() + "\n\n" +
            _("Usage:") + "\n" +
              "  zcash-tx [options] <hex-tx> [commands]  " + _("Update hex-encoded zcash transaction") + "\n" +
              "  zcash-tx [options] -create [commands]   " + _("Create hex-encoded zcash transaction") + "\n" +
              "\n";

        fprintf(stdout, "%s", strUsage.c_str());

        strUsage = HelpMessageGroup(_("Options:"));
        strUsage += HelpMessageOpt("-?", _("This help message"));
        strUsage += HelpMessageOpt("-create", _("Create new, empty TX."));
        strUsage += HelpMessageOpt("-json", _("Select JSON output"));
        strUsage += HelpMessageOpt("-txid", _("Output only the hex-encoded transaction id of the resultant transaction."));
        AppendParamsHelpMessages(strUsage);

        fprintf(stdout, "%s", strUsage.c_str());

        strUsage = HelpMessageGroup(_("Commands:"));
        strUsage += HelpMessageOpt("delin=N", _("Delete input N from TX"));
        strUsage += HelpMessageOpt("delout=N", _("Delete output N from TX"));
        strUsage += HelpMessageOpt("in=TXID:VOUT(:SEQUENCE_NUMBER)", _("Add input to TX"));
        strUsage += HelpMessageOpt("locktime=N", _("Set TX lock time to N"));
        strUsage += HelpMessageOpt("nversion=N", _("Set TX version to N"));
        strUsage += HelpMessageOpt("outaddr=VALUE:ADDRESS", _("Add address-based output to TX"));
        strUsage += HelpMessageOpt("outscript=VALUE:SCRIPT", _("Add raw script output to TX"));
        strUsage += HelpMessageOpt("sign=HEIGHT:SIGHASH-FLAGS", _("Add zero or more signatures to transaction") + ". " +
            _("This command requires JSON registers:") +
            _("prevtxs=JSON object") + ", " +
            _("privatekeys=JSON object") + ". " +
            _("See signrawtransaction docs for format of sighash flags, JSON objects."));
        fprintf(stdout, "%s", strUsage.c_str());

        strUsage = HelpMessageGroup(_("Register Commands:"));
        strUsage += HelpMessageOpt("load=NAME:FILENAME", _("Load JSON file FILENAME into register NAME"));
        strUsage += HelpMessageOpt("set=NAME:JSON-STRING", _("Set register NAME to given JSON-STRING"));
        fprintf(stdout, "%s", strUsage.c_str());

        if (argc < 2) {
            fprintf(stderr, "Error: too few parameters\n");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    return CONTINUE_EXECUTION;
}

static void RegisterSetJson(const std::string& key, const std::string& rawJson)
{
    UniValue val;
    if (!val.read(rawJson)) {
        std::string strErr = "Cannot parse JSON for key " + key;
        throw std::runtime_error(strErr);
    }

    registers[key] = val;
}

static void RegisterSet(const std::string& strInput)
{
    // separate NAME:VALUE in string
    size_t pos = strInput.find(':');
    if ((pos == std::string::npos) ||
        (pos == 0) ||
        (pos == (strInput.size() - 1)))
        throw std::runtime_error("Register input requires NAME:VALUE");

    std::string key = strInput.substr(0, pos);
    std::string valStr = strInput.substr(pos + 1, std::string::npos);

    RegisterSetJson(key, valStr);
}

static void RegisterLoad(const std::string& strInput)
{
    // separate NAME:FILENAME in string
    size_t pos = strInput.find(':');
    if ((pos == std::string::npos) ||
        (pos == 0) ||
        (pos == (strInput.size() - 1)))
        throw std::runtime_error("Register load requires NAME:FILENAME");

    std::string key = strInput.substr(0, pos);
    std::string filename = strInput.substr(pos + 1, std::string::npos);

    FILE *f = fopen(filename.c_str(), "r");
    if (!f) {
        std::string strErr = "Cannot open file " + filename;
        throw std::runtime_error(strErr);
    }

    // load file chunks into one big buffer
    std::string valStr;
    while ((!feof(f)) && (!ferror(f))) {
        char buf[4096];
        int bread = fread(buf, 1, sizeof(buf), f);
        if (bread <= 0)
            break;

        valStr.insert(valStr.size(), buf, bread);
    }

    int error = ferror(f);
    fclose(f);

    if (error) {
        std::string strErr = "Error reading file " + filename;
        throw std::runtime_error(strErr);
    }

    // evaluate as JSON buffer register
    RegisterSetJson(key, valStr);
}

static void MutateTxVersion(CMutableTransaction& tx, const std::string& cmdVal)
{
    int64_t newVersion = atoi64(cmdVal);
    if (newVersion < CTransaction::SPROUT_MIN_CURRENT_VERSION || newVersion > CTransaction::SPROUT_MAX_CURRENT_VERSION)
        throw std::runtime_error("Invalid TX version requested");

    tx.nVersion = (int) newVersion;
}

static void MutateTxExpiry(CMutableTransaction& tx, const std::string& cmdVal)
{
    int64_t newExpiry = atoi64(cmdVal);
    if (newExpiry <= 0 || newExpiry >= TX_EXPIRY_HEIGHT_THRESHOLD) {
        throw std::runtime_error("Invalid TX expiry requested");
    }
    tx.nExpiryHeight = (int) newExpiry;
}

static void MutateTxLocktime(CMutableTransaction& tx, const std::string& cmdVal)
{
    int64_t newLocktime = atoi64(cmdVal);
    if (newLocktime < 0LL || newLocktime > 0xffffffffLL)
        throw std::runtime_error("Invalid TX locktime requested");

    tx.nLockTime = (unsigned int) newLocktime;
}

static void MutateTxAddInput(CMutableTransaction& tx, const std::string& strInput)
{
    std::vector<std::string> vStrInputParts;
    boost::split(vStrInputParts, strInput, boost::is_any_of(":"));

    // separate TXID:VOUT in string
    if (vStrInputParts.size()<2)
        throw std::runtime_error("TX input missing separator");

    // extract and validate TXID
    std::string strTxid = vStrInputParts[0];
    if ((strTxid.size() != 64) || !IsHex(strTxid))
        throw std::runtime_error("invalid TX input txid");
    uint256 txid(uint256S(strTxid));

    static const unsigned int minTxOutSz = 9;
    static const unsigned int maxVout = MAX_BLOCK_SIZE / minTxOutSz;

    // extract and validate vout
    std::string strVout = vStrInputParts[1];
    int vout = atoi(strVout);
    if ((vout < 0) || (vout > (int)maxVout))
        throw std::runtime_error("invalid TX input vout");

    // extract the optional sequence number
    uint32_t nSequenceIn=std::numeric_limits<unsigned int>::max();
    if (vStrInputParts.size() > 2)
        nSequenceIn = atoi(vStrInputParts[2]);

    // append to transaction input list
    CTxIn txin(txid, vout, CScript(), nSequenceIn);
    tx.vin.push_back(txin);
}

static void MutateTxAddOutAddr(CMutableTransaction& tx, const std::string& strInput)
{
    // separate VALUE:ADDRESS in string
    size_t pos = strInput.find(':');
    if ((pos == std::string::npos) ||
        (pos == 0) ||
        (pos == (strInput.size() - 1)))
        throw std::runtime_error("TX output missing separator");

    // extract and validate VALUE
    std::string strValue = strInput.substr(0, pos);
    CAmount value;
    if (!ParseMoney(strValue, value))
        throw std::runtime_error("invalid TX output value");

    KeyIO keyIO(Params());

    // extract and validate ADDRESS
    std::string strAddr = strInput.substr(pos + 1, std::string::npos);
    CTxDestination destination = keyIO.DecodeDestination(strAddr);
    if (!IsValidDestination(destination)) {
        throw std::runtime_error("invalid TX output address");
    }
    CScript scriptPubKey = GetScriptForDestination(destination);

    // construct TxOut, append to transaction output list
    CTxOut txout(value, scriptPubKey);
    tx.vout.push_back(txout);
}

static void MutateTxAddOutScript(CMutableTransaction& tx, const std::string& strInput)
{
    // separate VALUE:SCRIPT in string
    size_t pos = strInput.find(':');
    if ((pos == std::string::npos) ||
        (pos == 0))
        throw std::runtime_error("TX output missing separator");

    // extract and validate VALUE
    std::string strValue = strInput.substr(0, pos);
    CAmount value;
    if (!ParseMoney(strValue, value))
        throw std::runtime_error("invalid TX output value");

    // extract and validate script
    std::string strScript = strInput.substr(pos + 1, std::string::npos);
    CScript scriptPubKey = ParseScript(strScript); // throws on err

    // construct TxOut, append to transaction output list
    CTxOut txout(value, scriptPubKey);
    tx.vout.push_back(txout);
}

static void MutateTxDelInput(CMutableTransaction& tx, const std::string& strInIdx)
{
    // parse requested deletion index
    int inIdx = atoi(strInIdx);
    if (inIdx < 0 || inIdx >= (int)tx.vin.size()) {
        std::string strErr = "Invalid TX input index '" + strInIdx + "'";
        throw std::runtime_error(strErr.c_str());
    }

    // delete input from transaction
    tx.vin.erase(tx.vin.begin() + inIdx);
}

static void MutateTxDelOutput(CMutableTransaction& tx, const std::string& strOutIdx)
{
    // parse requested deletion index
    int outIdx = atoi(strOutIdx);
    if (outIdx < 0 || outIdx >= (int)tx.vout.size()) {
        std::string strErr = "Invalid TX output index '" + strOutIdx + "'";
        throw std::runtime_error(strErr.c_str());
    }

    // delete output from transaction
    tx.vout.erase(tx.vout.begin() + outIdx);
}

static const unsigned int N_SIGHASH_OPTS = 6;
static const struct {
    const char *flagStr;
    int flags;
} sighashOptions[N_SIGHASH_OPTS] = {
    {"ALL", SIGHASH_ALL},
    {"NONE", SIGHASH_NONE},
    {"SINGLE", SIGHASH_SINGLE},
    {"ALL|ANYONECANPAY", SIGHASH_ALL|SIGHASH_ANYONECANPAY},
    {"NONE|ANYONECANPAY", SIGHASH_NONE|SIGHASH_ANYONECANPAY},
    {"SINGLE|ANYONECANPAY", SIGHASH_SINGLE|SIGHASH_ANYONECANPAY},
};

static bool findSighashFlags(int& flags, const std::string& flagStr)
{
    flags = 0;

    for (unsigned int i = 0; i < N_SIGHASH_OPTS; i++) {
        if (flagStr == sighashOptions[i].flagStr) {
            flags = sighashOptions[i].flags;
            return true;
        }
    }

    return false;
}

static CAmount AmountFromValue(const UniValue& value)
{
    if (!value.isNum() && !value.isStr())
        throw std::runtime_error("Amount is not a number or string");
    CAmount amount;
    if (!ParseFixedPoint(value.getValStr(), 8, &amount))
        throw std::runtime_error("Invalid amount");
    if (!MoneyRange(amount))
        throw std::runtime_error("Amount out of range");
    return amount;
}

static void MutateTxSign(CMutableTransaction& tx, const std::string& strInput)
{
    // separate HEIGHT:SIGHASH-FLAGS in string
    size_t pos = strInput.find(':');
    if ((pos == 0) ||
        (pos == (strInput.size() - 1)))
        throw std::runtime_error("Invalid sighash flag separator");

    // extract and validate HEIGHT
    std::string strHeight = strInput.substr(0, pos);
    int nHeight = atoi(strHeight);
    if (nHeight <= 0) {
        throw std::runtime_error("invalid height");
    }

    // extract and validate SIGHASH-FLAGS
    int nHashType = SIGHASH_ALL;
    std::string flagStr;
    if (pos != std::string::npos) {
        flagStr = strInput.substr(pos + 1, std::string::npos);
    }
    if (flagStr.size() > 0)
        if (!findSighashFlags(nHashType, flagStr))
            throw std::runtime_error("unknown sighash flag/sign option");

    std::vector<CTransaction> txVariants;
    txVariants.emplace_back(tx);

    // mergedTx will end up with all the signatures; it
    // starts as a clone of the raw tx:
    CMutableTransaction mergedTx(txVariants[0]);
    bool fComplete = true;
    CCoinsViewDummy viewDummy;
    CCoinsViewCache view(&viewDummy);

    if (!registers.count("privatekeys"))
        throw std::runtime_error("privatekeys register variable must be set.");
    bool fGivenKeys = false;
    CBasicKeyStore tempKeystore;
    UniValue keysObj = registers["privatekeys"];
    fGivenKeys = true;

    KeyIO keyIO(Params());

    for (size_t kidx = 0; kidx < keysObj.size(); kidx++) {
        if (!keysObj[kidx].isStr())
            throw std::runtime_error("privatekey not a std::string");
        CKey key = keyIO.DecodeSecret(keysObj[kidx].getValStr());
        if (!key.IsValid()) {
            throw std::runtime_error("privatekey not valid");
        }
        tempKeystore.AddKey(key);
    }

    // Add previous txouts given in the RPC call:
    if (!registers.count("prevtxs"))
        throw std::runtime_error("prevtxs register variable must be set.");
    UniValue prevtxsObj = registers["prevtxs"];
    {
        for (size_t previdx = 0; previdx < prevtxsObj.size(); previdx++) {
            UniValue prevOut = prevtxsObj[previdx];
            if (!prevOut.isObject())
                throw std::runtime_error("expected prevtxs internal object");

            std::map<std::string,UniValue::VType> types = boost::assign::map_list_of("txid", UniValue::VSTR)("vout",UniValue::VNUM)("scriptPubKey",UniValue::VSTR);
            if (!prevOut.checkObject(types))
                throw std::runtime_error("prevtxs internal object typecheck fail");

            uint256 txid = ParseHashUV(prevOut["txid"], "txid");

            int nOut = atoi(prevOut["vout"].getValStr());
            if (nOut < 0)
                throw std::runtime_error("vout must be positive");

            std::vector<unsigned char> pkData(ParseHexUV(prevOut["scriptPubKey"], "scriptPubKey"));
            CScript scriptPubKey(pkData.begin(), pkData.end());

            {
                CCoinsModifier coins = view.ModifyCoins(txid);
                if (coins->IsAvailable(nOut) && coins->vout[nOut].scriptPubKey != scriptPubKey) {
                    std::string err("Previous output scriptPubKey mismatch:\n");
                    err = err + ScriptToAsmStr(coins->vout[nOut].scriptPubKey) + "\nvs:\n"+
                        ScriptToAsmStr(scriptPubKey);
                    throw std::runtime_error(err);
                }
                if ((unsigned int)nOut >= coins->vout.size())
                    coins->vout.resize(nOut+1);
                coins->vout[nOut].scriptPubKey = scriptPubKey;
                coins->vout[nOut].nValue = 0;
                if (prevOut.exists("amount")) {
                    coins->vout[nOut].nValue = AmountFromValue(prevOut["amount"]);
                }
            }

            // if redeemScript given and private keys given,
            // add redeemScript to the tempKeystore so it can be signed:
            if (fGivenKeys && scriptPubKey.IsPayToScriptHash() &&
                prevOut.exists("redeemScript")) {
                UniValue v = prevOut["redeemScript"];
                std::vector<unsigned char> rsData(ParseHexUV(v, "redeemScript"));
                CScript redeemScript(rsData.begin(), rsData.end());
                tempKeystore.AddCScript(redeemScript);
            }
        }
    }

    const CKeyStore& keystore = tempKeystore;
    // We don't support v5 transactions via this API yet.
    if (mergedTx.nVersion >= ZIP225_TX_VERSION) {
        throw std::runtime_error("v5+ transactions not supported yet");
    }
    const PrecomputedTransactionData txdata(CTransaction(mergedTx), {});

    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);

    // Grab the consensus branch ID for the given height
    auto consensusBranchId = CurrentEpochBranchId(nHeight, Params().GetConsensus());

    // Sign what we can:
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++) {
        CTxIn& txin = mergedTx.vin[i];
        const CCoins* coins = view.AccessCoins(txin.prevout.hash);
        if (!coins || !coins->IsAvailable(txin.prevout.n)) {
            fComplete = false;
            continue;
        }
        const CScript& prevPubKey = coins->vout[txin.prevout.n].scriptPubKey;
        const CAmount& amount = coins->vout[txin.prevout.n].nValue;

        SignatureData sigdata;
        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if (!fHashSingle || (i < mergedTx.vout.size()))
            ProduceSignature(MutableTransactionSignatureCreator(&keystore, &mergedTx, txdata, i, amount, nHashType), prevPubKey, sigdata, consensusBranchId);

        // ... and merge in other signatures:
        for (const CTransaction& txv : txVariants)
            sigdata = CombineSignatures(prevPubKey, MutableTransactionSignatureChecker(&mergedTx, txdata, i, amount), sigdata, DataFromTransaction(txv, i), consensusBranchId);
        UpdateTransaction(mergedTx, i, sigdata);

        if (!VerifyScript(txin.scriptSig, prevPubKey, STANDARD_SCRIPT_VERIFY_FLAGS, MutableTransactionSignatureChecker(&mergedTx, txdata, i, amount), consensusBranchId))
            fComplete = false;
    }

    if (fComplete) {
        // do nothing... for now
        // perhaps store this for later optional JSON output
    }

    tx = mergedTx;
}

class Secp256k1Init
{
public:
    Secp256k1Init() {
        ECC_Start();
    }
    ~Secp256k1Init() {
        ECC_Stop();
    }
};

static void MutateTx(CMutableTransaction& tx, const std::string& command,
                     const std::string& commandVal)
{
    boost::scoped_ptr<Secp256k1Init> ecc;

    if (command == "nversion")
        MutateTxVersion(tx, commandVal);
    else if (command == "locktime")
        MutateTxLocktime(tx, commandVal);
    else if (command == "expiry")
        MutateTxExpiry(tx, commandVal);

    else if (command == "delin")
        MutateTxDelInput(tx, commandVal);
    else if (command == "in")
        MutateTxAddInput(tx, commandVal);

    else if (command == "delout")
        MutateTxDelOutput(tx, commandVal);
    else if (command == "outaddr")
        MutateTxAddOutAddr(tx, commandVal);
    else if (command == "outscript")
        MutateTxAddOutScript(tx, commandVal);

    else if (command == "sign") {
        if (!ecc) { ecc.reset(new Secp256k1Init()); }
        MutateTxSign(tx, commandVal);
    }

    else if (command == "load")
        RegisterLoad(commandVal);

    else if (command == "set")
        RegisterSet(commandVal);

    else
        throw std::runtime_error("unknown command");
}

static void OutputTxJSON(const CTransaction& tx)
{
    UniValue entry(UniValue::VOBJ);
    TxToUniv(tx, uint256(), entry);

    std::string jsonOutput = entry.write(4);
    fprintf(stdout, "%s\n", jsonOutput.c_str());
}

static void OutputTxHash(const CTransaction& tx)
{
    std::string strHexHash = tx.GetHash().GetHex(); // the hex-encoded transaction hash (aka the transaction id)

    fprintf(stdout, "%s\n", strHexHash.c_str());
}

static void OutputTxHex(const CTransaction& tx)
{
    std::string strHex = EncodeHexTx(tx);

    fprintf(stdout, "%s\n", strHex.c_str());
}

static void OutputTx(const CTransaction& tx)
{
    if (GetBoolArg("-json", false))
        OutputTxJSON(tx);
    else if (GetBoolArg("-txid", false))
        OutputTxHash(tx);
    else
        OutputTxHex(tx);
}

static std::string readStdin()
{
    char buf[4096];
    std::string ret;

    while (!feof(stdin)) {
        size_t bread = fread(buf, 1, sizeof(buf), stdin);
        ret.append(buf, bread);
        if (bread < sizeof(buf))
            break;
    }

    if (ferror(stdin))
        throw std::runtime_error("error reading stdin");

    boost::algorithm::trim_right(ret);

    return ret;
}

static int CommandLineRawTx(int argc, char* argv[])
{
    std::string strPrint;
    int nRet = 0;
    try {
        // Skip switches; Permit common stdin convention "-"
        while (argc > 1 && IsSwitchChar(argv[1][0]) &&
               (argv[1][1] != 0)) {
            argc--;
            argv++;
        }

        CTransaction txDecodeTmp;
        int startArg;

        if (!fCreateBlank) {
            // require at least one param
            if (argc < 2)
                throw std::runtime_error("too few parameters");

            // param: hex-encoded bitcoin transaction
            std::string strHexTx(argv[1]);
            if (strHexTx == "-")                 // "-" implies standard input
                strHexTx = readStdin();

            if (!DecodeHexTx(txDecodeTmp, strHexTx))
                throw std::runtime_error("invalid transaction encoding");

            startArg = 2;
        } else
            startArg = 1;

        CMutableTransaction tx(txDecodeTmp);

        for (int i = startArg; i < argc; i++) {
            std::string arg = argv[i];
            std::string key, value;
            size_t eqpos = arg.find('=');
            if (eqpos == std::string::npos)
                key = arg;
            else {
                key = arg.substr(0, eqpos);
                value = arg.substr(eqpos + 1);
            }

            MutateTx(tx, key, value);
        }

        OutputTx(CTransaction(tx));
    }
    catch (const std::exception& e) {
        strPrint = std::string("error: ") + e.what();
        nRet = EXIT_FAILURE;
    }
    catch (...) {
        PrintExceptionContinue(NULL, "CommandLineRawTx()");
        throw;
    }

    if (strPrint != "") {
        fprintf((nRet == 0 ? stdout : stderr), "%s\n", strPrint.c_str());
    }
    return nRet;
}

int main(int argc, char* argv[])
{
    SetupEnvironment();

    try {
        int ret = AppInitRawTx(argc, argv);
        if (ret != CONTINUE_EXECUTION)
            return ret;
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "AppInitRawTx()");
        return EXIT_FAILURE;
    } catch (...) {
        PrintExceptionContinue(NULL, "AppInitRawTx()");
        return EXIT_FAILURE;
    }

    int ret = EXIT_FAILURE;
    try {
        ret = CommandLineRawTx(argc, argv);
    }
    catch (const std::exception& e) {
        PrintExceptionContinue(&e, "CommandLineRawTx()");
    } catch (...) {
        PrintExceptionContinue(NULL, "CommandLineRawTx()");
    }
    return ret;
}
