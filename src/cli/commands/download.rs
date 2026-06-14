use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::cli::error_handler::print_and_exit;
use std::fs::OpenOptions;
use std::io::{Read, Write, IsTerminal};
use std::time::{Duration, Instant};

#[derive(Args, Debug)]
pub struct DownloadArgs {
    /// Target URL
    pub url: String,

    /// Output file path (if omitted, inferred from URL)
    #[arg(short = 'o', long)]
    pub output: Option<String>,

    /// Resume partial download if file exists
    #[arg(long)]
    pub resume: bool,

    /// Force progress output even if stderr is not a TTY
    #[arg(long)]
    pub progress: bool,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: DownloadArgs) {
    let filename = args.output.clone().unwrap_or_else(|| {
        if let Ok(parsed) = url::Url::parse(&args.url) {
            if let Some(segments) = parsed.path_segments() {
                if let Some(last) = segments.last() {
                    if !last.is_empty() {
                        return last.to_string();
                    }
                }
            }
        }
        "download".to_string()
    });

    let clean_url = crate::utils::url::clean_url(&args.url);
    let mut headers = match crate::utils::headers::parse_headers(&args.common.header) {
        Ok(h) => h,
        Err(e) => {
            if !args.common.silent {
                print_and_exit(&e);
            } else {
                std::process::exit(e.exit_code());
            }
        }
    };

    if let Some(ref user) = args.common.user {
        use base64::Engine;
        let encoded = base64::engine::general_purpose::STANDARD.encode(user.as_bytes());
        headers.push(("Authorization".to_string(), format!("Basic {}", encoded)));
    }

    let mut start_pos = 0u64;
    let mut file_exists = false;
    if args.resume {
        if let Ok(metadata) = std::fs::metadata(&filename) {
            start_pos = metadata.len();
            file_exists = true;
            headers.push(("Range".to_string(), format!("bytes={}-", start_pos)));
        }
    }

    let agent = ureq::AgentBuilder::new()
        .timeout_connect(Duration::from_secs(10))
        .timeout(Duration::from_secs(args.common.timeout))
        .redirects(if args.common.location { 5 } else { 0 })
        .build();

    let mut request = agent.get(&clean_url);
    for (k, v) in &headers {
        request = request.set(k, v);
    }

    if args.common.verbose && !args.common.silent {
        eprintln!("* Connecting to {}", clean_url);
        eprintln!("> GET {}", clean_url);
        for (k, v) in &headers {
            let redacted_val = crate::utils::headers::redact_header_value(k, v);
            eprintln!("> {}: {}", k, redacted_val);
        }
        eprintln!(">");
    }

    let response = match request.call() {
        Ok(res) => res,
        Err(e) => {
            if !args.common.silent {
                eprintln!("error: could not connect to host");
                eprintln!("  details: {}", e);
                std::process::exit(2);
            } else {
                std::process::exit(2);
            }
        }
    };

    let status = response.status();
    let is_resume = status == 206 && file_exists;

    let mut file = match OpenOptions::new()
        .create(true)
        .write(true)
        .append(is_resume)
        .truncate(!is_resume)
        .open(&filename)
    {
        Ok(f) => f,
        Err(e) => {
            if !args.common.silent {
                eprintln!("error: could not open file for writing");
                eprintln!("  details: {}", e);
                std::process::exit(6);
            } else {
                std::process::exit(6);
            }
        }
    };

    let content_len = response.header("Content-Length")
        .and_then(|s| s.parse::<u64>().ok())
        .unwrap_or(0);

    let total_len = if is_resume {
        // Look for Content-Range to find total size, e.g. "bytes 100-200/500"
        let total_from_range = response.header("Content-Range")
            .and_then(|r| r.split('/').last())
            .and_then(|s| s.parse::<u64>().ok());
        total_from_range.unwrap_or(content_len + start_pos)
    } else {
        content_len
    };

    let show_progress = !args.common.silent && (args.progress || std::io::stderr().is_terminal());

    if show_progress {
        eprintln!("downloading {}", filename);
        eprintln!("  url: {}", clean_url);
        if total_len > 0 {
            eprintln!("  size: {:.1} MB", total_len as f64 / 1024.0 / 1024.0);
        } else {
            eprintln!("  size: unknown");
        }
        eprintln!();
    }

    let mut reader = response.into_reader();
    let mut buffer = [0u8; 8192];
    let mut downloaded = if is_resume { start_pos } else { 0 };
    let start_time = Instant::now();
    let mut last_update = Instant::now();

    loop {
        let n = match reader.read(&mut buffer) {
            Ok(0) => break,
            Ok(n) => n,
            Err(e) => {
                if !args.common.silent {
                    eprintln!("error: connection closed during download");
                    eprintln!("  details: {}", e);
                    std::process::exit(2);
                } else {
                    std::process::exit(2);
                }
            }
        };

        if let Err(e) = file.write_all(&buffer[..n]) {
            if !args.common.silent {
                eprintln!("error: could not write response to file");
                eprintln!("  details: {}", e);
                std::process::exit(6);
            } else {
                std::process::exit(6);
            }
        }

        downloaded += n as u64;

        if show_progress && (last_update.elapsed() >= Duration::from_millis(200) || downloaded == total_len) {
            last_update = Instant::now();
            let elapsed = start_time.elapsed().as_secs_f64().max(0.001);
            let speed = (downloaded - if is_resume { start_pos } else { 0 }) as f64 / elapsed;
            let speed_mb = speed / 1024.0 / 1024.0;
            
            let progress_pct = if total_len > 0 {
                (downloaded as f64 / total_len as f64 * 100.0) as u32
            } else {
                0
            };

            let remaining_time = if total_len > downloaded && speed > 0.0 {
                format!("{:.0}s remaining", (total_len - downloaded) as f64 / speed)
            } else {
                "unknown".to_string()
            };

            // Carriage return to overwrite progress line
            eprint!(
                "\r  {:.1} MB / {:.1} MB  {}%  {:.1} MB/s  {}",
                downloaded as f64 / 1024.0 / 1024.0,
                total_len as f64 / 1024.0 / 1024.0,
                progress_pct,
                speed_mb,
                remaining_time
            );
            let _ = std::io::stderr().flush();
        }
    }

    if show_progress {
        eprintln!("\n\nDownload completed successfully.");
    }
}
