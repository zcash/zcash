use std::cmp::min;
use std::env::{self, consts::EXE_EXTENSION};
use std::ffi::OsStr;
use std::fs::File;
use std::io::{self, BufRead, Stdin, Write};
use std::iter;
use std::panic;
use std::path::{Path, PathBuf};
use std::process::{self, ChildStdin, Command, Output, Stdio};
use std::str::from_utf8;
use std::time::SystemTime;

use anyhow::{self, Context};
use backtrace::Backtrace;
use gumdrop::{Options, ParsingStyle};
use rand::{thread_rng, Rng};
use secrecy::{ExposeSecret, SecretString};
use thiserror::Error;
use time::macros::format_description;
use time::OffsetDateTime;
use tracing::debug;
use tracing_subscriber::{fmt, EnvFilter};

#[derive(Debug, Options)]
struct CliOptions {
    #[options(no_short, help = "Print this help output")]
    help: bool,

    #[options(
        no_short,
        help = "Specify configuration filename, relative to the data directory (default: zcash.conf)",
        meta = "FILENAME"
    )]
    conf: Option<String>,

    #[options(
        no_short,
        help = "Specify data directory (this path cannot use '~')",
        meta = "PATH"
    )]
    datadir: Option<String>,

    #[options(no_short, help = "Use the test chain")]
    testnet: bool,

    #[options(
        no_short,
        help = "Send commands to node running on IPADDR (default: 127.0.0.1)",
        meta = "IPADDR"
    )]
    rpcconnect: Option<String>,

    #[options(
        no_short,
        help = "Connect to JSON-RPC on PORT (default: 8232 or testnet: 18232)",
        meta = "PORT"
    )]
    rpcport: Option<u16>,

    #[options(
        no_short,
        help = "Username for JSON-RPC connections",
        meta = "USERNAME"
    )]
    rpcuser: Option<String>,

    #[options(
        no_short,
        help = "Password for JSON-RPC connections",
        meta = "PASSWORD"
    )]
    rpcpassword: Option<String>,

    #[options(
        no_short,
        help = "Timeout in seconds during HTTP requests, or 0 for no timeout. (default: 900)",
        meta = "SECONDS"
    )]
    rpcclienttimeout: Option<u32>,
}

impl CliOptions {
    fn to_zcash_cli_options(&self) -> Vec<String> {
        iter::empty::<String>()
            .chain(self.conf.as_ref().map(|o| format!("-conf={}", o)))
            .chain(self.datadir.as_ref().map(|o| format!("-datadir={}", o)))
            .chain(if self.testnet {
                Some("-testnet".into())
            } else {
                None
            })
            .chain(
                self.rpcconnect
                    .as_ref()
                    .map(|o| format!("-rpcconnect={}", o)),
            )
            .chain(self.rpcport.as_ref().map(|o| format!("-rpcport={}", o)))
            .chain(self.rpcuser.as_ref().map(|o| format!("-rpcuser={}", o)))
            .chain(
                self.rpcpassword
                    .as_ref()
                    .map(|o| format!("-rpcpassword={}", o)),
            )
            .chain(
                self.rpcclienttimeout
                    .as_ref()
                    .map(|o| format!("-rpcclienttimeout={}", o)),
            )
            .collect()
    }
}

#[derive(Debug, Error)]
enum WalletToolError {
    #[error("zcash-cli executable not found")]
    ZcashCliNotFound,

    #[error("Unexpected response from zcash-cli or zcashd")]
    UnexpectedResponse,

    #[error("Could not connect to zcashd")]
    ZcashdConnection,

    #[error("zcashd -exportdir option not set")]
    ExportDirNotSet,

    #[error("Could not parse a unique recovery phrase from the export file")]
    RecoveryPhraseNotFound,

    #[error("Unexpected EOF in input")]
    UnexpectedEof,
}

pub fn main() {
    // Log to stderr, configured by the RUST_LOG environment variable.
    fmt()
        .with_writer(io::stderr)
        .with_env_filter(EnvFilter::from_default_env())
        .init();

    // Allow either Bitcoin-style or GNU-style arguments.
    let mut args = env::args();
    let command = args.next().expect("argv[0] should exist");
    let args: Vec<_> = args
        .map(|s| {
            if s.starts_with('-') && !s.starts_with("--") {
                format!("-{}", s)
            } else {
                s
            }
        })
        .collect();

    const USAGE_NOTE: &str = concat!(
        "Options can be given in GNU style (`--conf=CONF` or `--conf CONF`),\n",
        "or in Bitcoin style with a single hyphen (`-conf=CONF`).\n\n",
        "The environment variable RUST_LOG controls debug output, e.g.\n",
        "`RUST_LOG=debug`.\n",
    );

    let opts = CliOptions::parse_args(&args, ParsingStyle::default()).unwrap_or_else(|e| {
        eprintln!(
            "{}: {}\n\nUsage: {} [OPTIONS]\n\n{}\n\n{}",
            command,
            e,
            command,
            CliOptions::usage(),
            USAGE_NOTE,
        );
        process::exit(2);
    });

    if opts.help_requested() {
        println!(
            "Usage: {} [OPTIONS]\n\n{}\n\n{}",
            command,
            opts.self_usage(),
            USAGE_NOTE
        );
        process::exit(0);
    }

    if let Err(e) = run(&opts) {
        eprintln!("{}: {}", command, e);
        process::exit(1);
    }
}

fn run(opts: &CliOptions) -> anyhow::Result<()> {
    let cli_options: Vec<String> = opts.to_zcash_cli_options();

    println!(concat!(
        "To reduce the risk of loss of funds, we're going to confirm that the\n",
        "zcashd wallet is backed up reliably.\n\n",
        "  👛 ➜ 🗃️ \n"
    ));

    println!("Checking that we can connect to zcashd...");
    let zcash_cli = zcash_cli_path()?;

    // Pass an invalid filename, "\x01", and use the error message to distinguish
    // whether zcashd is running with the -exportdir option, running without that
    // option, or not running / cannot connect.
    let mut cli_args = cli_options.clone();
    cli_args.extend_from_slice(&["z_exportwallet".to_string(), "\x01".to_string()]);
    let out = exec(&zcash_cli, &cli_args, None)?;
    let cli_err: Vec<_> = from_utf8(&out.stderr)
        .with_context(|| "Output from zcash-cli was not UTF-8")?
        .lines()
        .map(|s| s.trim_end_matches('\r'))
        .collect();
    debug!("stderr {:?}", cli_err);

    if !cli_err.is_empty() {
        if cli_err[0].starts_with("Error reading configuration file") {
            println!(
                "\nNo, we could not read the zcashd configuration file, expected to be at\n{:?}.",
                Path::new(opts.datadir.as_ref().map_or("~/.zcash", |s| &s[..])).join(Path::new(
                    opts.conf.as_ref().map_or("zcash.conf", |s| &s[..])
                )),
            );
            println!(concat!(
                "If it is not at that path, please try again with the '-datadir' and/or\n",
                "'-conf' options set correctly (see '--help' for details). Also make sure\n",
                "that the current user has permission to read the configuration file.\n",
            ));
            return Err(WalletToolError::ZcashdConnection.into());
        }
        if cli_err[0].starts_with("error: couldn't connect") {
            println!(concat!(
                "\nNo, we could not connect. zcashd might not be running; in that case\n",
                "please start it. The '-exportdir' option should be set to the absolute\n",
                "path of the directory you want to save the wallet export file to.\n\n",
                "(Don't forget to restart zcashd without '-exportdir' after finishing\n",
                "the backup, if running it long-term with that option is not desired\n",
                "or would be a security hazard in your environment.)\n\n",
                "If you believe zcashd is running, it might be using an unexpected port,\n",
                "address, or authentication options for the RPC interface, for example.\n",
                "In that case try to connect to it using zcash-cli, and if successful,\n",
                "use the same connection options for zcashd-wallet-tool (see '--help' for\n",
                "accepted options) as for zcash-cli.\n"
            ));
            return Err(WalletToolError::ZcashdConnection.into());
        }
        if cli_err[0] == "error code: -28" {
            println!(concat!(
                "\nNo, we could not connect. zcashd seems to be initializing; please try\n",
                "again once it has finished.\n",
            ));
            return Err(WalletToolError::ZcashdConnection.into());
        }
    }

    const REMINDER_MSG: &str = concat!(
        "\n\nPlease start or restart zcashd with '-exportdir' set to the absolute\n",
        "path of the directory you want to save the wallet export file to.\n",
        "(Don't forget to restart zcashd without '-exportdir' after finishing\n",
        "the backup, if running it long-term with that option is not desired\n",
        "or would be a security hazard in your environment.)\n",
    );

    if cli_err.len() >= 3
        && cli_err[0] == "error code: -4"
        && cli_err[2].contains("zcashd -exportdir")
    {
        println!(
            "\nIt looks like zcashd is running without the '-exportdir' option.{}",
            REMINDER_MSG
        );
        return Err(WalletToolError::ExportDirNotSet.into());
    }
    if !(cli_err.len() >= 3
        && cli_err[0] == "error code: -4"
        && cli_err[2].starts_with("Filename is invalid"))
    {
        println!(
            "\nThere was an unexpected response from zcash-cli or zcashd:\n> {}{}",
            cli_err.join("\n> "),
            REMINDER_MSG,
        );
        return Err(WalletToolError::UnexpectedResponse.into());
    }
    println!("Yes, and it is running with the '-exportdir' option as required.");

    let mut stdin = io::stdin();

    let base = default_filename_base();
    let mut r = 0u32;
    let out = loop {
        let default_filename = if r != 0 {
            format!("{}r{}", base, r)
        } else {
            base.to_string()
        };
        println!(
            concat!(
                "\nEnter the filename for the wallet export file, using only characters\n",
                "a-z, A-Z and 0-9 (default '{}')."
            ),
            default_filename
        );
        let response = prompt(&mut stdin)?;
        let response = strip(&response);
        let filename = if response.is_empty() {
            r = r.saturating_add(1);
            &default_filename
        } else {
            response
        };
        debug!("Using filename {:?}", filename);

        let mut cli_args = cli_options.clone();
        cli_args.extend_from_slice(&["z_exportwallet".to_string(), filename.to_string()]);
        let out = exec(&zcash_cli, &cli_args, None)?;
        let cli_err: Vec<_> = from_utf8(&out.stderr)
            .with_context(|| "Output from zcash-cli was not UTF-8")?
            .lines()
            .map(|s| s.trim_end_matches('\r'))
            .collect();
        debug!("stderr {:?}", cli_err);

        if cli_err.len() >= 3
            && cli_err[0] == "error code: -8"
            && cli_err[1] == "error message:"
            && cli_err[2].contains("overwrite existing")
        {
            println!(concat!(
                "That file already exists. Please pick a unique filename in the\n",
                "directory specified by the '-exportdir' option to zcashd."
            ));
            continue;
        } else {
            break out;
        }
    };

    let cli_out: Vec<_> = from_utf8(&out.stdout)
        .with_context(|| "Output from zcash-cli was not UTF-8")?
        .lines()
        .map(|s| s.trim_end_matches('\r'))
        .collect();
    debug!("stdout {:?}", cli_out);

    if cli_out.is_empty() {
        return Err(WalletToolError::UnexpectedResponse.into());
    }
    let export_path = cli_out[0];

    println!("\nSaved the export file to '{}'.", export_path);
    println!("IMPORTANT: This file contains secrets that allow spending all wallet funds.\n");

    let export_file = File::open(export_path)
        .with_context(|| format!("Could not open {:?} for reading", export_path))?;

    // TODO: ensure the buffer will be zeroized (#5650)
    let phrase_line: Vec<_> = io::BufReader::new(export_file)
        .lines()
        .map(|line| line.map(SecretString::new))
        .filter(|s| {
            s.as_ref()
                .map(|t| t.expose_secret().starts_with("# - recovery_phrase=\""))
                .unwrap_or(false)
        })
        .collect();

    let phrase = match &phrase_line[..] {
        [Ok(line)] => line
            .expose_secret()
            .trim_start_matches("# - recovery_phrase=\"")
            .trim_end_matches('"'),
        _ => return Err(WalletToolError::RecoveryPhraseNotFound.into()),
    };

    // This panic hook allows us to make a best effort to clear the screen (and then print
    // another reminder about secrets in the export file) even if a panic occurs.

    let old_hook = panic::take_hook();
    {
        let export_path = export_path.to_string();
        panic::set_hook(Box::new(move |panic_info| {
            clear_and_show_cautions(&export_path);

            let s = panic_info.payload().downcast_ref::<&str>().unwrap_or(&"");
            eprintln!("\nPanic: {}\n{:?}", s, Backtrace::new());
        }));
    }

    let res = (|| -> anyhow::Result<()> {
        println!("The recovery phrase is:\n");

        const WORDS_PER_LINE: usize = 3;

        let words: Vec<_> = phrase.split(' ').collect();
        let max_len = words.iter().map(|w| w.len()).max().unwrap_or(0);

        for (i, word) in words.iter().enumerate() {
            print!("{0:2}: {1:2$}", i + 1, word, max_len + 2);
            if (i + 1) % WORDS_PER_LINE == 0 {
                println!();
            }
        }
        if words.len() % WORDS_PER_LINE != 0 {
            println!();
        }

        println!(concat!(
            "\nPlease write down this phrase (including the numbering of words) on\n",
            "something durable that you will keep in a secure location.\n",
            "Press Enter when finished; then the phrase will disappear and you'll be\n",
            "asked to re-enter a selection of words from it."
        ));

        let mut stdin = io::stdin();
        prompt(&mut stdin)?;

        // The only reliable and portable way to make sure the recovery phrase
        // is no longer displayed is to clear the whole terminal (including
        // scrollback, if possible). The text is only printed if clearing fails.
        try_to_clear(concat!(
            "\n\n\n\n\n\n\n\n\n\n\n\n",
            "Please adjust the terminal window so that you can't see the\n",
            "recovery phrase above. After finishing the backup, close the\n",
            "terminal window or clear it"
        ));

        println!("\nNow we're going to confirm that you backed up the recovery phrase.");

        let mut rng = thread_rng();
        let mut unconfirmed: Vec<usize> = (0..words.len()).collect();
        for _ in 0..min(3, words.len()) {
            let index: usize = rng.gen_range(0..unconfirmed.len());
            let n = unconfirmed[index];
            unconfirmed[index] = unconfirmed[unconfirmed.len() - 1];
            unconfirmed.pop().expect("should be nonempty");

            loop {
                println!("\nPlease enter the {} word:", ordinal(n + 1));
                let line = prompt(&mut stdin)?;
                if words[n] == strip(&line) {
                    break;
                }
                println!("That's not correct, please try again.");
            }
        }
        Ok(())
    })();

    panic::set_hook(old_hook);
    clear_and_show_cautions(export_path);
    res?;

    let mut cli_args = cli_options;
    cli_args.extend_from_slice(&["-stdin".to_string(), "walletconfirmbackup".to_string()]);
    exec(&zcash_cli, &cli_args, Some(phrase))
        .and_then(|out| {
            let cli_err: Vec<_> = from_utf8(&out.stderr)
                .with_context(|| "Output from zcash-cli was not UTF-8")?
                .lines()
                .map(|s| s.trim_end_matches('\r'))
                .collect();
            debug!("stderr {:?}", cli_err);

            if !cli_err.is_empty() {
                if cli_err[0].starts_with("error: couldn't connect") {
                    println!("\nWe could not connect to zcashd; it may have exited.");
                    return Err(WalletToolError::ZcashdConnection.into());
                } else {
                    println!(
                        "\nThere was an unexpected response from zcash-cli or zcashd:\n> {}",
                        cli_err.join("\n> "),
                    );
                    return Err(WalletToolError::UnexpectedResponse.into());
                }
            }
            println!(concat!(
                "\nThe backup of the emergency recovery phrase for the zcashd\n",
                "wallet has been successfully confirmed 🙂. You can now use the\n",
                "zcashd RPC methods that create keys and addresses in that wallet.\n",
                "\n",
                "The zcashd wallet might also contain keys that are *not* derived\n",
                "from the emergency recovery phrase. If you lose the 'wallet.dat'\n",
                "file then any funds associated with these keys would be lost. To\n",
                "minimize this risk, it is recommended to send all funds from this\n",
                "wallet into new addresses and discontinue the use of legacy addresses.\n",
                "Note that even after doing so, there is the possibility that\n",
                "additional funds could be sent to legacy addresses (if any exist)\n",
                "and so it is necessary to keep backing up the 'wallet.dat' file\n",
                "in any case.\n",
                "\n",
                "If you use other wallets, their recovery information will need to\n",
                "be backed up separately.\n"
            ));
            Ok(())
        })
        .map_err(|e| {
            println!(concat!(
                "\nzcash-wallet-tool was unable to communicate to zcashd that the\n",
                "backup was confirmed. This can happen if zcashd stopped, in which\n",
                "case you should try again. If zcashd is still running, please seek\n",
                "help or try to use 'zcash-cli -stdin walletconfirmbackup' manually.\n"
            ));
            e
        })?;
    Ok(())
}

const MAX_USER_INPUT_LEN: usize = 100;

fn prompt(input: &mut Stdin) -> anyhow::Result<SecretString> {
    let mut buf = String::with_capacity(MAX_USER_INPUT_LEN);
    let res = input
        .read_line(&mut buf)
        .with_context(|| "Error reading from stdin");

    // Ensure the buffer is zeroized even on error.
    let line = SecretString::new(buf);
    res.and_then(|_| {
        if line.expose_secret().ends_with('\n') {
            Ok(line)
        } else {
            Err(WalletToolError::UnexpectedEof.into())
        }
    })
}

fn strip(input: &SecretString) -> &str {
    input
        .expose_secret()
        .trim_end_matches(|c| c == '\r' || c == '\n')
        .trim()
}

fn ordinal(num: usize) -> String {
    let suffix = if (11..=13).contains(&(num % 100)) {
        "th"
    } else {
        match num % 10 {
            1 => "st",
            2 => "nd",
            3 => "rd",
            _ => "th",
        }
    };
    format!("{}{}", num, suffix)
}

fn zcash_cli_path() -> anyhow::Result<PathBuf> {
    // First look for `zcash_cli[.exe]` as a sibling of the executable.
    let mut exe = env::current_exe()
        .with_context(|| "Cannot determine the path of the running executable")?;
    exe.set_file_name("zcash-cli");
    exe.set_extension(EXE_EXTENSION);

    debug!("Testing for zcash-cli at {:?}", exe);
    if exe.exists() {
        return Ok(exe);
    }
    // If not found there, look in `../src/zcash_cli[.exe]` provided
    // that `src` is a sibling of `target`.
    exe.pop(); // strip filename
    exe.pop(); // ..
    if exe.file_name() != Some(OsStr::new("target")) {
        // or in `../../src/zcash_cli[.exe]` under the same proviso
        exe.pop(); // ../..
        if exe.file_name() != Some(OsStr::new("target")) {
            return Err(WalletToolError::ZcashCliNotFound.into());
        }
    }
    // Replace 'target/' with 'src/'.
    exe.set_file_name("src");
    exe.push("zcash-cli");
    exe.set_extension(EXE_EXTENSION);

    debug!("Testing for zcash-cli at {:?}", exe);
    if !exe.exists() {
        return Err(WalletToolError::ZcashCliNotFound.into());
    }
    Ok(exe)
}

fn exec(exe_path: &Path, args: &[String], stdin: Option<&str>) -> anyhow::Result<Output> {
    debug!("Running {:?} {:?}", exe_path, args);
    let mut cmd = Command::new(exe_path);
    let cli = cmd.args(args);
    match stdin {
        None => Ok(cli.output()?),
        Some(data) => {
            let mut cli_process = cli
                .stdin(Stdio::piped())
                .stdout(Stdio::piped())
                .stderr(Stdio::piped())
                .spawn()?;
            cli_process
                .stdin
                .take()
                .with_context(|| "Could not open pipe to zcash-cli's stdin")
                .and_then(|mut pipe: ChildStdin| -> anyhow::Result<()> {
                    pipe.write_all(data.as_bytes())?;
                    pipe.write_all("\n".as_bytes())?;
                    Ok(())
                })
                .with_context(|| "Could not write to zcash-cli's stdin")?;
            Ok(cli_process.wait_with_output()?)
        }
    }
}

fn default_filename_base() -> String {
    let format = format_description!("export[year][month][day]");

    // We use the UTC date because there is a security issue in obtaining the local date
    // from either `chrono` or `time`: <https://github.com/chronotope/chrono/issues/602>.
    // We could use the approach in
    // <https://github.com/ArekPiekarz/rusty-tax-break/commit/3aac8f0c26fd96b7365619509a544f78b59627fe>
    // if it were important, but it isn't worth the dependency on `tz-rs`.

    OffsetDateTime::from(SystemTime::now())
        .format(&format)
        .unwrap_or_else(|_| "export".to_string())
}

fn clear_and_show_cautions(export_path: &str) {
    try_to_clear(concat!(
        "\nCAUTION: This terminal window might be showing secrets (or have\n",
        "them in the scrollback). Please copy any useful information and\n",
        "then close it, or clear it"
    ));

    println!(
        concat!(
            "\nIMPORTANT: Secrets that allow spending all zcashd wallet funds\n",
            "have been left in the file '{}'.\n\n",
            "Don't forget to restart zcashd without '-exportdir', if running it\n",
            "long-term with that option is not desired or would be a security\n",
            "hazard in your environment.\n\n",
            "When choosing a location for the physical backup of your emergency\n",
            "recovery phrase, please make sure to consider both risk of theft,\n",
            "and your long-term ability to remember where it is kept."
        ),
        export_path,
    );
}

fn try_to_clear(error_blurb: &str) {
    if let Err(e) = clearscreen::clear() {
        eprintln!("Unable to clear screen: {}.", e);

        #[cfg(target_os = "windows")]
        const HOW_TO_CLEAR: &str = "using 'cls'";
        #[cfg(target_os = "macos")]
        const HOW_TO_CLEAR: &str = "by pressing Command + K";
        #[cfg(not(any(target_os = "windows", target_os = "macos")))]
        const HOW_TO_CLEAR: &str = "using 'clear'";

        println!("{} {}.", error_blurb, HOW_TO_CLEAR);
    }
}
