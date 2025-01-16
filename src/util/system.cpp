// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2015-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "util/system.h"

#include "chainparamsbase.h"
#include "fs.h"
#include "random.h"
#include "serialize.h"
#include "sync.h"
#include "util/strencodings.h"
#include "util/time.h"

#include <stdarg.h>
#include <stdio.h>

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#include <pthread.h>
#include <pthread_np.h>
#endif

#ifndef WIN32
// for posix_fallocate
#ifdef __linux__

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L

#endif // __linux__

#include <algorithm>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

#else

#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <io.h> /* for _commit */
#include <shlobj.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include <boost/algorithm/string/case_conv.hpp> // for to_lower()
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp> // for startswith() and endswith()
#include <boost/program_options/detail/config_file.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/thread.hpp>

using namespace std;

const char * const BITCOIN_CONF_FILENAME = "zcash.conf";
const char * const BITCOIN_PID_FILENAME = "zcashd.pid";

CCriticalSection cs_args;
map<string, string> mapArgs;
map<string, vector<string>> mapMultiArgs;
bool fDebug = false;
bool fDaemon = false;
bool fServer = false;

/** Interpret string as boolean, for argument parsing */
static bool InterpretBool(const std::string& strValue)
{
    if (strValue.empty())
        return true;
    return (atoi(strValue) != 0);
}

/** Turn -noX into -X=0 (and -noX=0 into -X=1) */
static void InterpretNegativeSetting(std::string& strKey, std::string& strValue)
{
    if (strKey.length()>3 && strKey[0]=='-' && strKey[1]=='n' && strKey[2]=='o')
    {
        strKey = "-" + strKey.substr(3);
        strValue = InterpretBool(strValue) ? "0" : "1";
    }
}

void ParseParameters(int argc, const char* const argv[])
{
    LOCK(cs_args);
    mapArgs.clear();
    mapMultiArgs.clear();

    for (int i = 1; i < argc; i++)
    {
        std::string str(argv[i]);
        std::string strValue;
        size_t is_index = str.find('=');
        if (is_index != std::string::npos)
        {
            strValue = str.substr(is_index+1);
            str = str.substr(0, is_index);
        }
#ifdef WIN32
        boost::to_lower(str);
        if (boost::algorithm::starts_with(str, "/"))
            str = "-" + str.substr(1);
#endif

        if (str[0] != '-')
            break;

        // Interpret --foo as -foo.
        // If both --foo and -foo are set, the last takes effect.
        if (str.length() > 1 && str[1] == '-')
            str = str.substr(1);
        InterpretNegativeSetting(str, strValue);

        mapArgs[str] = strValue;
        mapMultiArgs[str].push_back(strValue);
    }
}

std::string GetArg(const std::string& strArg, const std::string& strDefault)
{
    LOCK(cs_args);
    if (mapArgs.count(strArg))
        return mapArgs[strArg];
    return strDefault;
}

int64_t GetArg(const std::string& strArg, int64_t nDefault)
{
    LOCK(cs_args);
    if (mapArgs.count(strArg))
        return atoi64(mapArgs[strArg]);
    return nDefault;
}

bool GetBoolArg(const std::string& strArg, bool fDefault)
{
    LOCK(cs_args);
    if (mapArgs.count(strArg))
        return InterpretBool(mapArgs[strArg]);
    return fDefault;
}

std::vector<std::string> GetMultiArg(const std::string& strArg)
{
    if (mapMultiArgs.count(strArg)) {
        return mapMultiArgs[strArg];
    } else {
        return {};
    }
}

bool SoftSetArg(const std::string& strArg, const std::string& strValue)
{
    LOCK(cs_args);
    if (mapArgs.count(strArg))
        return false;
    mapArgs[strArg] = strValue;
    return true;
}

bool SoftSetBoolArg(const std::string& strArg, bool fValue)
{
    if (fValue)
        return SoftSetArg(strArg, std::string("1"));
    else
        return SoftSetArg(strArg, std::string("0"));
}

static const int screenWidth = 79;
static const int optIndent = 2;
static const int msgIndent = 7;

std::string HelpMessageGroup(const std::string &message) {
    return std::string(message) + std::string("\n\n");
}

std::string HelpMessageOpt(const std::string &option, const std::string &message) {
    return std::string(optIndent,' ') + std::string(option) +
           std::string("\n") + std::string(msgIndent,' ') +
           FormatParagraph(message, screenWidth - msgIndent, msgIndent) +
           std::string("\n\n");
}

static std::string FormatException(const std::exception* pex, const char* pszThread)
{
#ifdef WIN32
    char pszModule[MAX_PATH] = "";
    GetModuleFileNameA(NULL, pszModule, sizeof(pszModule));
#else
    const char* pszModule = "Zcash";
#endif
    if (pex)
        return strprintf(
            "EXCEPTION: %s       \n%s       \n%s in %s       \n", typeid(*pex).name(), pex->what(), pszModule, pszThread);
    else
        return strprintf(
            "UNKNOWN EXCEPTION       \n%s in %s       \n", pszModule, pszThread);
}

void PrintExceptionContinue(const std::exception* pex, const char* pszThread)
{
    std::string message = FormatException(pex, pszThread);
    LogPrintf("\n\n************************\n%s\n", message);
}

fs::path GetDefaultDataDir()
{
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\Zcash
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\Zcash
    // Mac: ~/Library/Application Support/Zcash
    // Unix: ~/.zcash
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "Zcash";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    pathRet /= "Library/Application Support";
    TryCreateDirectory(pathRet);
    return pathRet / "Zcash";
#else
    // Unix
    return pathRet / ".zcash";
#endif
#endif
}

static fs::path pathCached;
static fs::path pathCachedNetSpecific;
static fs::path zc_paramsPathCached;
static CCriticalSection csPathCached;

static fs::path ZC_GetDefaultBaseParamsDir()
{
    // Copied from GetDefaultDataDir and adapter for zcash params.

    // Windows < Vista: C:\Documents and Settings\Username\Application Data\ZcashParams
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\ZcashParams
    // Mac: ~/Library/Application Support/ZcashParams
    // Unix: ~/.zcash-params
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "ZcashParams";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    pathRet /= "Library/Application Support";
    TryCreateDirectory(pathRet);
    return pathRet / "ZcashParams";
#else
    // Unix
    return pathRet / ".zcash-params";
#endif
#endif
}

const fs::path &ZC_GetParamsDir()
{
    LOCK2(cs_args, csPathCached);

    fs::path &path = zc_paramsPathCached;

    // This can be called during exceptions by LogPrintf(), so we cache the
    // value so we don't have to do memory allocations after that.
    if (!path.empty())
        return path;

    if (mapArgs.count("-paramsdir")) {
        path = fs::weakly_canonical(fs::system_complete(mapArgs["-paramsdir"]));
        if (!fs::is_directory(path)) {
            throw std::runtime_error(strprintf("The -paramsdir '%s' does not exist or is not a directory", path.string()));
        }
    } else {
        path = ZC_GetDefaultBaseParamsDir();
    }

    return path;
}

// Return the user specified export directory.  Create directory if it doesn't exist.
// If user did not set option, return an empty path.
// If there is a filesystem problem, throw an exception.
const fs::path GetExportDir()
{
    fs::path path;
    LOCK(cs_args);
    if (mapArgs.count("-exportdir")) {
        path = fs::weakly_canonical(fs::system_complete(mapArgs["-exportdir"]));
        if (fs::exists(path) && !fs::is_directory(path)) {
            throw std::runtime_error(strprintf("The -exportdir '%s' already exists and is not a directory", path.string()));
        }
        if (!fs::exists(path) && !fs::create_directories(path)) {
            throw std::runtime_error(strprintf("Failed to create directory at -exportdir '%s'", path.string()));
        }
    }
    return path;
}


const fs::path &GetDataDir(bool fNetSpecific)
{
    LOCK2(cs_args, csPathCached);

    fs::path &path = fNetSpecific ? pathCachedNetSpecific : pathCached;

    // This can be called during exceptions by LogPrintf(), so we cache the
    // value so we don't have to do memory allocations after that.
    if (!path.empty())
        return path;

    if (mapArgs.count("-datadir")) {
        path = fs::weakly_canonical(fs::system_complete(mapArgs["-datadir"]));
        if (!fs::is_directory(path)) {
            path = "";
            return path;
        }
    } else {
        path = GetDefaultDataDir();
    }
    if (fNetSpecific)
        path /= BaseParams().DataDir();

    fs::create_directories(path);

    return path;
}

void ClearDatadirCache()
{
    LOCK(csPathCached);
    pathCached = fs::path();
    pathCachedNetSpecific = fs::path();
}

fs::path GetConfigFile(const std::string& confPath)
{
    return AbsPathForConfigVal(fs::path(confPath), false);
}

void ReadConfigFile(const std::string& confPath,
                    map<string, string>& mapSettingsRet,
                    map<string, vector<string> >& mapMultiSettingsRet)
{
    fs::ifstream streamConfig(GetConfigFile(confPath));
    if (!streamConfig.good())
        throw missing_zcash_conf();

    set<string> setOptions;
    setOptions.insert("*");

    const vector<string> allowed_duplicates = {
        "addnode",
        "allowdeprecated",
        "bind",
        "connect",
        "debug",
        "externalip",
        "fundingstream",
        "loadblock",
        "metricsallowip",
        "nuparams",
        "onlynet",
        "rpcallowip",
        "rpcauth",
        "rpcbind",
        "seednode",
        "uacomment",
        "whitebind",
        "whitelist"
    };
    set<string> unique_options;

    {
        LOCK(cs_args);
        for (boost::program_options::detail::config_file_iterator it(streamConfig, setOptions), end; it != end; ++it)
        {
            string strKey = string("-") + it->string_key;
            string strValue = it->value[0];

            if (find(allowed_duplicates.begin(), allowed_duplicates.end(), it->string_key) == allowed_duplicates.end())
            {
                if (!unique_options.insert(strKey).second) {
                    throw std::runtime_error(strprintf("Option '%s' is duplicated, which is not allowed.", strKey));
                }
            }

            InterpretNegativeSetting(strKey, strValue);
            // Don't overwrite existing settings so command line settings override zcash.conf
            if (mapSettingsRet.count(strKey) == 0)
                mapSettingsRet[strKey] = strValue;
            mapMultiSettingsRet[strKey].push_back(strValue);
        }
    }
    // If datadir is changed in .conf file:
    ClearDatadirCache();
}

#ifndef WIN32
fs::path GetPidFile()
{
    return AbsPathForConfigVal(fs::path(GetArg("-pid", BITCOIN_PID_FILENAME)));
}

void CreatePidFile(const fs::path &path, pid_t pid)
{
    FILE* file = fsbridge::fopen(path, "w");
    if (file)
    {
        fprintf(file, "%d\n", pid);
        fclose(file);
    }
}
#endif

bool RenameOver(fs::path src, fs::path dest)
{
#ifdef WIN32
    return MoveFileExA(src.string().c_str(), dest.string().c_str(),
                       MOVEFILE_REPLACE_EXISTING) != 0;
#else
    int rc = std::rename(src.string().c_str(), dest.string().c_str());
    return (rc == 0);
#endif /* WIN32 */
}

/**
 * Ignores exceptions thrown by Boost's create_directory if the requested directory exists.
 * Specifically handles case where path p exists, but it wasn't possible for the user to
 * write to the parent directory.
 */
bool TryCreateDirectory(const fs::path& p)
{
    try
    {
        return fs::create_directory(p);
    } catch (const fs::filesystem_error&) {
        if (!fs::exists(p) || !fs::is_directory(p))
            throw;
    }

    // create_directory didn't create the directory, it had to have existed already
    return false;
}

void FileCommit(FILE *fileout)
{
    fflush(fileout); // harmless if redundantly called
#ifdef WIN32
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(fileout));
    FlushFileBuffers(hFile);
#else
    #if defined(__linux__) || defined(__NetBSD__)
    fdatasync(fileno(fileout));
    #elif defined(MAC_OSX) && defined(F_FULLFSYNC)
    fcntl(fileno(fileout), F_FULLFSYNC, 0);
    #else
    fsync(fileno(fileout));
    #endif
#endif
}

bool TruncateFile(FILE *file, unsigned int length) {
#if defined(WIN32)
    return _chsize(_fileno(file), length) == 0;
#else
    return ftruncate(fileno(file), length) == 0;
#endif
}

/**
 * this function tries to raise the file descriptor limit to the requested number.
 * It returns the actual file descriptor limit (which may be more or less than nMinFD)
 */
int RaiseFileDescriptorLimit(int nMinFD) {
#if defined(WIN32)
    return 2048;
#else
    struct rlimit limitFD;
    if (getrlimit(RLIMIT_NOFILE, &limitFD) != -1) {
        if (limitFD.rlim_cur < (rlim_t)nMinFD) {
            limitFD.rlim_cur = nMinFD;
            if (limitFD.rlim_cur > limitFD.rlim_max)
                limitFD.rlim_cur = limitFD.rlim_max;
            setrlimit(RLIMIT_NOFILE, &limitFD);
            getrlimit(RLIMIT_NOFILE, &limitFD);
        }
        return limitFD.rlim_cur;
    }
    return nMinFD; // getrlimit failed, assume it's fine
#endif
}

/**
 * this function tries to make a particular range of a file allocated (corresponding to disk space)
 * it is advisory, and the range specified in the arguments will never contain live data
 */
void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length) {
#if defined(WIN32)
    // Windows-specific version
    HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
    LARGE_INTEGER nFileSize;
    int64_t nEndPos = (int64_t)offset + length;
    nFileSize.u.LowPart = nEndPos & 0xFFFFFFFF;
    nFileSize.u.HighPart = nEndPos >> 32;
    SetFilePointerEx(hFile, nFileSize, 0, FILE_BEGIN);
    SetEndOfFile(hFile);
#elif defined(MAC_OSX)
    // OSX specific version
    fstore_t fst;
    fst.fst_flags = F_ALLOCATECONTIG;
    fst.fst_posmode = F_PEOFPOSMODE;
    fst.fst_offset = 0;
    fst.fst_length = (off_t)offset + length;
    fst.fst_bytesalloc = 0;
    if (fcntl(fileno(file), F_PREALLOCATE, &fst) == -1) {
        fst.fst_flags = F_ALLOCATEALL;
        fcntl(fileno(file), F_PREALLOCATE, &fst);
    }
    ftruncate(fileno(file), fst.fst_length);
#elif defined(__linux__)
    // Version using posix_fallocate
    off_t nEndPos = (off_t)offset + length;
    posix_fallocate(fileno(file), 0, nEndPos);
#else
    // Fallback version
    // TODO: just write one byte per block
    static const char buf[65536] = {};
    fseek(file, offset, SEEK_SET);
    while (length > 0) {
        unsigned int now = 65536;
        if (length < now)
            now = length;
        fwrite(buf, 1, now, file); // allowed to fail; this function is advisory anyway
        length -= now;
    }
#endif
}

#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate)
{
    WCHAR pwszPath[MAX_PATH] = L"";

    if(SHGetSpecialFolderPathW(nullptr, pwszPath, nFolder, fCreate))
    {
        return fs::path(pwszPath);
    }

    LogPrintf("SHGetSpecialFolderPathW() failed, could not obtain requested path.\n");
    return fs::path("");
}
#endif

void runCommand(const std::string& strCommand)
{
    int nErr = ::system(strCommand.c_str());
    if (nErr)
        LogPrintf("runCommand error: system(%s) returned %d\n", strCommand, nErr);
}

void RenameThread(const char* name)
{
#if defined(PR_SET_NAME)
    // Only the first 15 characters are used (16 - NUL terminator)
    ::prctl(PR_SET_NAME, name, 0, 0, 0);
#elif (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
    pthread_set_name_np(pthread_self(), name);

#elif defined(MAC_OSX)
    pthread_setname_np(name);
#else
    // Prevent warnings for unused parameters...
    (void)name;
#endif
}

void SetupEnvironment()
{
    // On most POSIX systems (e.g. Linux, but not BSD) the environment's locale
    // may be invalid, in which case the "C" locale is used as fallback.
#if !defined(WIN32) && !defined(MAC_OSX) && !defined(__FreeBSD__) && !defined(__OpenBSD__)
    try {
        std::locale(""); // Raises a runtime error if current locale is invalid
    } catch (const std::runtime_error&) {
        setenv("LC_ALL", "C", 1);
    }
#endif
    // The path locale is lazy initialized and to avoid deinitialization errors
    // in multithreading environments, it is set explicitly by the main thread.
    // A dummy locale is used to extract the internal default locale, used by
    // fs::path, which is then used to explicitly imbue the path.
    std::locale loc = fs::path::imbue(std::locale::classic());
    fs::path::imbue(loc);
}

bool SetupNetworking()
{
#ifdef WIN32
    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2,2), &wsadata);
    if (ret != NO_ERROR || LOBYTE(wsadata.wVersion ) != 2 || HIBYTE(wsadata.wVersion) != 2)
        return false;
#endif
    return true;
}

void SetThreadPriority(int nPriority)
{
#ifdef WIN32
    SetThreadPriority(GetCurrentThread(), nPriority);
#else // WIN32
#ifdef PRIO_THREAD
    setpriority(PRIO_THREAD, 0, nPriority);
#else // PRIO_THREAD
    setpriority(PRIO_PROCESS, 0, nPriority);
#endif // PRIO_THREAD
#endif // WIN32
}

std::string PrivacyInfo()
{
    return "\n" +
           FormatParagraph(strprintf(_("In order to ensure you are adequately protecting your privacy when using Zcash, please see <%s>."),
                                     "https://z.cash/support/security/")) + "\n";
}

std::string LicenseInfo()
{
    return CopyrightHolders(strprintf(_("Copyright (C) %i-%i"), 2009, COPYRIGHT_YEAR) + " ") + "\n" +
           "\n" +
           _("This is experimental software.") + "\n" +
           "\n" +
           _("Distributed under the MIT software license, see the accompanying file COPYING or <https://www.opensource.org/licenses/mit-license.php>.") +
           "\n";
}

int GetNumCores()
{
    return boost::thread::physical_concurrency();
}

std::string CopyrightHolders(const std::string& strPrefix)
{
    std::string strCopyrightHolders = strPrefix + _(COPYRIGHT_HOLDERS);
    if (strCopyrightHolders.find("%s") != strCopyrightHolders.npos) {
        strCopyrightHolders = strprintf(strCopyrightHolders, _(COPYRIGHT_HOLDERS_SUBSTITUTION));
    }
    if (strCopyrightHolders.find("Bitcoin Core developers") == strCopyrightHolders.npos) {
        strCopyrightHolders += "\n" + strPrefix + "The Bitcoin Core developers";
    }
    return strCopyrightHolders;
}

fs::path AbsPathForConfigVal(const fs::path& path, bool net_specific)
{
    return fs::absolute(path, GetDataDir(net_specific));
}
