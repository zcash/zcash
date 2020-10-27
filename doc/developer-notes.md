Coding
====================

Various coding styles have been used during the history of the codebase,
and the result is not very consistent. However, we're now trying to converge to
a single style, so please use it in new code. Old code will be converted
gradually.
- Basic rules specified in src/.clang-format. Use a recent clang-format-3.5 to format automatically.
  - Braces on new lines for namespaces, classes, functions, methods.
  - Braces on the same line for everything else.
  - 4 space indentation (no tabs) for every block except namespaces.
  - No indentation for public/protected/private or for namespaces.
  - No extra spaces inside parenthesis; don't do ( this )
  - No space after function names; one space after if, for and while.

Block style example:
```c++
namespace foo
{
class Class
{
    bool Function(char* psz, int n)
    {
        // Comment summarising what this section of code does
        for (int i = 0; i < n; i++) {
            // When something fails, return early
            if (!Something())
                return false;
            ...
        }

        // Success return is usually at the end
        return true;
    }
}
}
```

Doxygen comments
-----------------

To facilitate the generation of documentation, use doxygen-compatible comment blocks for functions, methods and fields.

For example, to describe a function use:
```c++
/**
 * ... text ...
 * @param[in] arg1    A description
 * @param[in] arg2    Another argument description
 * @pre Precondition for function...
 */
bool function(int arg1, const char *arg2)
```
A complete list of `@xxx` commands can be found at http://www.stack.nl/~dimitri/doxygen/manual/commands.html.
As Doxygen recognizes the comments by the delimiters (`/**` and `*/` in this case), you don't
*need* to provide any commands for a comment to be valid; just a description text is fine.

To describe a class use the same construct above the class definition:
```c++
/**
 * Alerts are for notifying old versions if they become too obsolete and
 * need to upgrade. The message is displayed in the status bar.
 * @see GetWarnings()
 */
class CAlert
{
```

To describe a member or variable use:
```c++
int var; //!< Detailed description after the member
```

or
```cpp
//! Description before the member
int var;
```

Also OK:
```c++
///
/// ... text ...
///
bool function2(int arg1, const char *arg2)
```

Not OK (used plenty in the current source, but not picked up):
```c++
//
// ... text ...
//
```

A full list of comment syntaxes picked up by doxygen can be found at http://www.stack.nl/~dimitri/doxygen/manual/docblocks.html,
but if possible use one of the above styles.

Development tips and tricks
---------------------------

**compiling for debugging**

Run configure with the --enable-debug option, then make. Or run configure with
CXXFLAGS="-g -ggdb -O0" or whatever debug flags you need.

**compiling for gprof profiling**

Run configure with the --enable-gprof option, then make.

**debug.log**

If the code is behaving strangely, take a look in the debug.log file in the data directory;
error and debugging messages are written there.

The -debug=... command-line option controls debugging; running with just -debug or -debug=1 will turn
on all categories (and give you a very large debug.log file).

**testnet and regtest modes**

Run with the -testnet option to run with "play zcash" on the test network, if you
are testing multi-machine code that needs to operate across the internet.

If you are testing something that can run on one machine, run with the -regtest option.
In regression test mode, blocks can be created on-demand; see qa/rpc-tests/ for tests
that run in -regtest mode.

**DEBUG_LOCKORDER**

Zcash is a multithreaded application, and deadlocks or other multithreading bugs
can be very difficult to track down. Compiling with -DDEBUG_LOCKORDER (configure
CXXFLAGS="-DDEBUG_LOCKORDER -g") inserts run-time checks to keep track of which locks
are held, and adds warnings to the debug.log file if inconsistencies are detected.

**Sanitizers**

Bitcoin can be compiled with various "sanitizers" enabled, which add
instrumentation for issues regarding things like memory safety, thread race
conditions, or undefined behavior. This is controlled with the
`--with-sanitizers` configure flag, which should be a comma separated list of
sanitizers to enable. The sanitizer list should correspond to supported
`-fsanitize=` options in your compiler. These sanitizers have runtime overhead,
so they are most useful when testing changes or producing debugging builds.

Some examples:

```bash
# Enable both the address sanitizer and the undefined behavior sanitizer
./configure --with-sanitizers=address,undefined

# Enable the thread sanitizer
./configure --with-sanitizers=thread
```

If you are compiling with GCC you will typically need to install corresponding
"san" libraries to actually compile with these flags, e.g. libasan for the
address sanitizer, libtsan for the thread sanitizer, and libubsan for the
undefined sanitizer. If you are missing required libraries, the configure script
will fail with a linker error when testing the sanitizer flags.

The test suite should pass cleanly with the `thread` and `undefined` sanitizers,
but there are a number of known problems when using the `address` sanitizer. The
address sanitizer is known to fail in
[sha256_sse4::Transform](/src/crypto/sha256_sse4.cpp) which makes it unusable
unless you also use `--disable-asm` when running configure. We would like to fix
sanitizer issues, so please send pull requests if you can fix any errors found
by the address sanitizer (or any other sanitizer).

Not all sanitizer options can be enabled at the same time, e.g. trying to build
with `--with-sanitizers=address,thread` will fail in the configure script as
these sanitizers are mutually incompatible. Refer to your compiler manual to
learn more about these options and which sanitizers are supported by your
compiler.

Additional resources:

 * [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
 * [LeakSanitizer](https://clang.llvm.org/docs/LeakSanitizer.html)
 * [MemorySanitizer](https://clang.llvm.org/docs/MemorySanitizer.html)
 * [ThreadSanitizer](https://clang.llvm.org/docs/ThreadSanitizer.html)
 * [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
 * [GCC Instrumentation Options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html)
 * [Google Sanitizers Wiki](https://github.com/google/sanitizers/wiki)
 * [Issue #12691: Enable -fsanitize flags in Travis](https://github.com/bitcoin/bitcoin/issues/12691)

Locking/mutex usage notes
-------------------------

The code is multi-threaded, and uses mutexes and the
LOCK/TRY_LOCK macros to protect data structures.

Deadlocks due to inconsistent lock ordering (thread 1 locks cs_main
and then cs_wallet, while thread 2 locks them in the opposite order:
result, deadlock as each waits for the other to release its lock) are
a problem. Compile with -DDEBUG_LOCKORDER to get lock order
inconsistencies reported in the debug.log file.

Re-architecting the core code so there are better-defined interfaces
between the various components is a goal, with any necessary locking
done by the components (e.g. see the self-contained CKeyStore class
and its cs_KeyStore lock for example).

Threads
-------

- ThreadScriptCheck : Verifies block scripts.

- ThreadImport : Loads blocks from blk*.dat files or bootstrap.dat.

- StartNode : Starts other threads.

- ThreadDNSAddressSeed : Loads addresses of peers from the DNS.

- ThreadMapPort : Universal plug-and-play startup/shutdown

- ThreadSocketHandler : Sends/Receives data from peers on port 8233.

- ThreadOpenAddedConnections : Opens network connections to added nodes.

- ThreadOpenConnections : Initiates new connections to peers.

- ThreadMessageHandler : Higher-level message handling (sending and receiving).

- DumpAddresses : Dumps IP addresses of nodes to peers.dat.

- ThreadFlushWalletDB : Close the wallet.dat file if it hasn't been used in 500ms.

- ThreadRPCServer : Remote procedure call handler, listens on port 8232 for connections and services them.

- ZcashMiner : Generates zcash (if wallet is enabled).

- Shutdown : Does an orderly shutdown of everything.

Pull Request Terminology
------------------------

Concept ACK - Agree with the idea and overall direction, but have neither reviewed nor tested the code changes.

utACK (untested ACK) - Reviewed and agree with the code changes but haven't actually tested them.

Tested ACK - Reviewed the code changes and have verified the functionality or bug fix.

ACK -  A loose ACK can be confusing. It's best to avoid them unless it's a documentation/comment only change in which case there is nothing to test/verify; therefore the tested/untested distinction is not there.

NACK - Disagree with the code changes/concept. Should be accompanied by an explanation.

See the [Development Guidelines](https://zcash.readthedocs.io/en/latest/rtd_pages/development_guidelines.html) documentation for preferred workflows, information on continuous integration and release versioning.

Strings and formatting
------------------------

- Avoid using locale dependent functions if possible. You can use the provided
  [`lint-locale-dependence.sh`](/contrib/devtools/lint-locale-dependence.sh)
  to check for accidental use of locale dependent functions.

  - *Rationale*: Unnecessary locale dependence can cause bugs that are very tricky to isolate and fix.

  - These functions are known to be locale dependent:
    `alphasort`, `asctime`, `asprintf`, `atof`, `atoi`, `atol`, `atoll`, `atoq`,
    `btowc`, `ctime`, `dprintf`, `fgetwc`, `fgetws`, `fprintf`, `fputwc`,
    `fputws`, `fscanf`, `fwprintf`, `getdate`, `getwc`, `getwchar`, `isalnum`,
    `isalpha`, `isblank`, `iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`,
    `ispunct`, `isspace`, `isupper`, `iswalnum`, `iswalpha`, `iswblank`,
    `iswcntrl`, `iswctype`, `iswdigit`, `iswgraph`, `iswlower`, `iswprint`,
    `iswpunct`, `iswspace`, `iswupper`, `iswxdigit`, `isxdigit`, `mblen`,
    `mbrlen`, `mbrtowc`, `mbsinit`, `mbsnrtowcs`, `mbsrtowcs`, `mbstowcs`,
    `mbtowc`, `mktime`, `putwc`, `putwchar`, `scanf`, `snprintf`, `sprintf`,
    `sscanf`, `stoi`, `stol`, `stoll`, `strcasecmp`, `strcasestr`, `strcoll`,
    `strfmon`, `strftime`, `strncasecmp`, `strptime`, `strtod`, `strtof`,
    `strtoimax`, `strtol`, `strtold`, `strtoll`, `strtoq`, `strtoul`,
    `strtoull`, `strtoumax`, `strtouq`, `strxfrm`, `swprintf`, `tolower`,
    `toupper`, `towctrans`, `towlower`, `towupper`, `ungetwc`, `vasprintf`,
    `vdprintf`, `versionsort`, `vfprintf`, `vfscanf`, `vfwprintf`, `vprintf`,
    `vscanf`, `vsnprintf`, `vsprintf`, `vsscanf`, `vswprintf`, `vwprintf`,
    `wcrtomb`, `wcscasecmp`, `wcscoll`, `wcsftime`, `wcsncasecmp`, `wcsnrtombs`,
    `wcsrtombs`, `wcstod`, `wcstof`, `wcstoimax`, `wcstol`, `wcstold`,
    `wcstoll`, `wcstombs`, `wcstoul`, `wcstoull`, `wcstoumax`, `wcswidth`,
    `wcsxfrm`, `wctob`, `wctomb`, `wctrans`, `wctype`, `wcwidth`, `wprintf`

Scripts
--------------------------

### Shebang

- Use `#!/usr/bin/env bash` instead of obsolete `#!/bin/bash`.

  - [*Rationale*](https://github.com/dylanaraps/pure-bash-bible#shebang):

    `#!/bin/bash` assumes it is always installed to /bin/ which can cause issues;

    `#!/usr/bin/env bash` searches the user's PATH to find the bash binary.

  OK:

```bash
#!/usr/bin/env bash
```

  Wrong:

```bash
#!/bin/bash
```

Source code organization
--------------------------

- Use include guards to avoid the problem of double inclusion. The header file
  `foo/bar.h` should use the include guard identifier `ZCASH_FOO_BAR_H`, e.g.

```c++
#ifndef ZCASH_FOO_BAR_H
#define ZCASH_FOO_BAR_H
...
#endif // ZCASH_FOO_BAR_H
```

Scripted diffs
--------------

For reformatting and refactoring commits where the changes can be easily automated using a bash script, we use
scripted-diff commits. The bash script is included in the commit message and our Travis CI job checks that
the result of the script is identical to the commit. This aids reviewers since they can verify that the script
does exactly what it's supposed to do. It is also helpful for rebasing (since the same script can just be re-run
on the new master commit).

To create a scripted-diff:

- start the commit message with `scripted-diff:` (and then a description of the diff on the same line)
- in the commit message include the bash script between lines containing just the following text:

    - `-BEGIN VERIFY SCRIPT-`
    - `-END VERIFY SCRIPT-`

The scripted-diff is verified by the tool `test/lint/commit-script-check.sh`

Commit `ccd074a5` is an example of a scripted-diff.
