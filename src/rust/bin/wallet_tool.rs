use std::cmp::min;
use std::env::{self, consts::EXE_EXTENSION};
use std::ffi::OsStr;
use std::fs::File;
use std::io::{self, BufRead, ErrorKind, Stdin};
use std::iter;
use std::panic;
use std::path::{Path, PathBuf};
use std::process::{self, Command, Output};
use std::str::from_utf8;
use std::time::SystemTime;

use backtrace::Backtrace;
use gumdrop::{Options, ParsingStyle};
use rand::{thread_rng, Rng};
use time::macros::format_description;
use time::OffsetDateTime;

#[derive(Debug, Options)]
struct CliOptions {
    #[options(no_short, help = "Print this help output")]
    help: bool,

    #[options(no_short, help = "Display debugging output")]
    debug: bool,

    #[options(
        no_short,
        help = "Specify configuration file (default: zcash.conf)",
        meta = "PATH"
    )]
    conf: Option<String>,

    #[options(no_short, help = "Specify data directory")]
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

pub fn main() {
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
        "or in Bitcoin style with a single hyphen (`-conf=CONF`).\n"
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

fn run(opts: &CliOptions) -> Result<(), io::Error> {
    let cli_options: Vec<String> = opts.to_zcash_cli_options();

    println!(concat!(
        "To reduce the risk of loss of funds, we're going to confirm that the\n",
        "zcashd wallet is backed up reliably.\n\n",
        "  👛 ➜ 🗃️ \n"
    ));

    println!("Checking that we can connect to zcashd...");
    let zcash_cli = zcash_cli_path(opts.debug)?;

    // Pass an invalid filename, "\x01", and use the error message to distinguish
    // whether zcashd is running with the -exportdir option, running without that
    // option, or not running / cannot connect.
    let mut cli_args = cli_options.clone();
    cli_args.extend_from_slice(&["z_exportwallet".to_string(), "\x01".to_string()]);
    let out = exec(&zcash_cli, &cli_args, opts.debug)?;
    let cli_err: Vec<_> = from_utf8(&out.stderr)
        .map_err(|_| io::Error::from(ErrorKind::InvalidData))?
        .lines()
        .map(|s| s.trim_end_matches('\r'))
        .collect();

    if cli_err.len() < 3 || cli_err[0] != "error code: -4" || cli_err[1] != "error message:" {
        // TODO: distinguish not running case more precisely
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
        return Err(io::Error::from(ErrorKind::Other));
    }
    if cli_err[2].contains("zcashd -exportdir") {
        println!(concat!(
            "\nIt looks like zcashd is running without the '-exportdir' option.\n\n",
            "Please start or restart zcashd with '-exportdir' set to the absolute\n",
            "path of the directory you want to save the wallet export file to.\n",
            "(Don't forget to restart zcashd without '-exportdir' after finishing\n",
            "the backup, if running it long-term with that option is not desired\n",
            "or would be a security hazard in your environment.)"
        ));
        return Err(io::Error::from(ErrorKind::Other));
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
        let mut buf = String::new();
        let response = prompt(&mut stdin, &mut buf)?;
        let filename = if response.is_empty() {
            r = r.saturating_add(1);
            &default_filename
        } else {
            response
        };
        if opts.debug {
            eprintln!("DEBUG: Using filename {:?}", filename);
        }

        let mut cli_args = cli_options.clone();
        cli_args.extend_from_slice(&["z_exportwallet".to_string(), filename.to_string()]);
        let out = exec(&zcash_cli, &cli_args, opts.debug)?;
        let cli_err: Vec<_> = from_utf8(&out.stderr)
            .map_err(|_| io::Error::from(ErrorKind::InvalidData))?
            .lines()
            .map(|s| s.trim_end_matches('\r'))
            .collect();
        if opts.debug {
            eprintln!("DEBUG: stderr {:?}", cli_err);
        }
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
        .map_err(|_| io::Error::from(ErrorKind::InvalidData))?
        .lines()
        .map(|s| s.trim_end_matches('\r'))
        .collect();
    if opts.debug {
        eprintln!("DEBUG: stdout {:?}", cli_out);
    }
    if cli_out.is_empty() {
        return Err(io::Error::from(ErrorKind::InvalidData));
    }
    let export_path = cli_out[0];

    println!("\nSaved the export file to '{}'.", export_path);
    println!("IMPORTANT: This file contains secrets that allow spending all wallet funds.\n");

    // TODO: better handling of file not found and permission errors.
    let export_file = File::open(export_path)?;
    let phrase_line: Vec<_> = io::BufReader::new(export_file)
        .lines()
        .filter(|s| {
            s.as_ref()
                .map(|t| t.starts_with("# - recovery_phrase=\""))
                .unwrap_or(false)
        })
        .collect();
    if phrase_line.len() != 1 || phrase_line[0].is_err() {
        return Err(io::Error::from(ErrorKind::InvalidData));
    }
    let phrase = phrase_line[0]
        .as_ref()
        .unwrap()
        .trim_start_matches("# - recovery_phrase=\"")
        .trim_end_matches('"');

    // This panic hook allows us to make a best effort to clear the screen (and then print
    // another reminder about secrets in the export file) even if a panic occurs.

    let moved_export_path = export_path.to_string(); // borrow checker workaround
    let old_hook = panic::take_hook();
    panic::set_hook(Box::new(move |panic_info| {
        clear_and_show_cautions(&moved_export_path);

        let s = panic_info.payload().downcast_ref::<&str>().unwrap_or(&"");
        eprintln!("\nPanic: {}\n{:?}", s, Backtrace::new());
    }));

    let res = (|| -> io::Result<()> {
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
            "\nPlease write down this phrase on something durable that you will keep\n",
            "in a secure location.\n",
            "Press Enter when finished; then the phrase will disappear and you'll be\n",
            "asked to re-enter a selection of words from it."
        ));

        let mut stdin = io::stdin();
        let mut buf = String::new();
        prompt(&mut stdin, &mut buf)?;

        // The only reliable and portable way to make sure the recovery phrase
        // is no longer displayed is to clear the whole terminal (including
        // scrollback, if possible). The text is only printed if clearing fails.
        try_to_clear(concat!(
            "\n\n\n\n\n\n\n\n\n\n\n\n",
            "Please adjust the terminal window so that you can't see the\n",
            "recovery phrase above. After finishing the backup, clear the\n",
            "terminal window"
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
                let mut buf = String::new();
                println!("\nPlease enter the {} word:", ordinal(n + 1));
                let line = prompt(&mut stdin, &mut buf)?;
                if words[n] == line {
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
    cli_args.extend_from_slice(&["walletconfirmbackup".to_string(), phrase.to_string()]);

    // Always pass false for debug to avoid printing the recovery phrase.
    exec(&zcash_cli, &cli_args, false)
        .and_then(|out| {
            let cli_err: Vec<_> = from_utf8(&out.stderr)
                .map_err(|_| io::Error::from(ErrorKind::InvalidData))?
                .lines()
                .map(|s| s.trim_end_matches('\r'))
                .collect();
            if opts.debug {
                eprintln!("DEBUG: stderr {:?}", cli_err);
            }
            if !cli_err.is_empty() {
                // TODO: distinguish errors: cannot connect, vs incorrect passphrase
                // (RPC_WALLET_PASSPHRASE_INCORRECT), vs other errors.
                Err(io::Error::from(ErrorKind::Other))
            } else {
                println!(concat!(
                    "\nThe backup of the emergency recovery phrase for the zcashd\n",
                    "wallet has been successfully confirmed 🙂. You can now use the\n",
                    "zcashd RPC methods that create keys and addresses in that wallet.\n\n",
                    "If you use other wallets, their recovery information will need\n",
                    "to be backed up separately.\n"
                ));
                Ok(())
            }
        })
        .map_err(|e| {
            println!(concat!(
                "\nzcash-wallet-tool was unable to communicate to zcashd that the\n",
                "backup was confirmed. This can happen if zcashd stopped, in which\n",
                "case you should try again. If zcashd is still running, please seek\n",
                "help or try to use 'zcash-cli -stdin walletconfirmbackup' manually.\n"
            ));
            e
        })
}

fn prompt<'a>(input: &mut Stdin, buf: &'a mut String) -> io::Result<&'a str> {
    let n = input.read_line(buf)?;
    if n == 0 {
        return Err(io::Error::from(ErrorKind::UnexpectedEof));
    }
    Ok(buf.trim_end_matches(|c| c == '\r' || c == '\n').trim())
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

fn zcash_cli_path(debug: bool) -> io::Result<PathBuf> {
    // First look for `zcash_cli[.exe]` as a sibling of the executable.
    let mut exe = env::current_exe()?;
    exe.set_file_name("zcash-cli");
    exe.set_extension(EXE_EXTENSION);
    if debug {
        eprintln!("DEBUG: Testing for zcash-cli at {:?}", exe);
    }
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
            return Err(io::Error::from(ErrorKind::NotFound));
        }
    }
    // Replace 'target/' with 'src/'.
    exe.set_file_name("src");
    exe.push("zcash-cli");
    exe.set_extension(EXE_EXTENSION);
    if debug {
        eprintln!("DEBUG: Testing for zcash-cli at {:?}", exe);
    }
    if exe.exists() {
        Ok(exe)
    } else {
        Err(io::Error::from(ErrorKind::NotFound))
    }
}

fn exec(exe_path: &Path, args: &[String], debug: bool) -> Result<Output, io::Error> {
    if debug {
        eprintln!("DEBUG: Running {:?} {:?}", exe_path, args);
    }
    Command::new(exe_path).args(args).output()
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
        "then clear it"
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
        const CLEAR: &str = "cls";
        #[cfg(not(target_os = "windows"))]
        const CLEAR: &str = "clear";

        println!("{} using '{}'.", error_blurb, CLEAR);
    }
}
