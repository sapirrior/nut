use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::cli::error_handler::NurlError;
use std::time::{Duration, Instant};

#[derive(Args, Debug)]
pub struct PingArgs {
    /// Target URL
    pub url: String,

    /// Number of pings
    #[arg(long, default_value_t = 1)]
    pub count: u32,

    /// Delay between pings in milliseconds
    #[arg(long, default_value_t = 1000)]
    pub interval: u64,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: PingArgs) {
    let clean_url = crate::utils::url::clean_url(&args.url);
    let host = url::Url::parse(&clean_url)
        .map(|u| u.host_str().unwrap_or("").to_string())
        .unwrap_or_else(|_| "".to_string());

    let mut latencies = Vec::new();
    let count = args.count;

    for i in 0..count {
        if i > 0 {
            std::thread::sleep(Duration::from_millis(args.interval));
        }

        let start = Instant::now();
        let mut res = ping_once("HEAD", &args);
        let mut duration = start.elapsed().as_millis() as u64;

        if let Err(NurlError::Status4xx(405, _)) = res {
            let start_get = Instant::now();
            res = ping_once("GET", &args);
            duration = start_get.elapsed().as_millis() as u64;
        }

        match res {
            Ok((status, status_text)) => {
                latencies.push(duration);
                println!("{}  {}  {}  {}ms", status, status_text, host, duration);
            }
            Err(e) => {
                if !args.common.silent {
                    crate::cli::error_handler::print_custom_error(
                        "Ping failed",
                        &[&e.to_string()],
                    );
                }
                std::process::exit(e.exit_code());
            }
        }
    }

    if count > 1 && !latencies.is_empty() {
        let min = latencies.iter().min().unwrap();
        let max = latencies.iter().max().unwrap();
        let sum: u64 = latencies.iter().sum();
        let avg = sum / latencies.len() as u64;
        println!("\nmin {}ms  avg {}ms  max {}ms", min, avg, max);
    }
}

fn ping_once(method: &str, args: &PingArgs) -> Result<(u16, String), NurlError> {
    let clean_url = crate::utils::url::clean_url(&args.url);
    let agent = ureq::AgentBuilder::new()
        .timeout_connect(Duration::from_secs(10))
        .timeout(Duration::from_secs(args.common.timeout))
        .redirects(0)
        .build();

    let request = match method {
        "HEAD" => agent.head(&clean_url),
        "GET" => agent.get(&clean_url),
        _ => agent.get(&clean_url),
    };

    let response = match request.call() {
        Ok(res) => res,
        Err(ureq::Error::Status(status, res)) => {
            if status == 405 && method == "HEAD" {
                return Err(NurlError::Status4xx(405, clean_url));
            }
            res
        }
        Err(ureq::Error::Transport(transport)) => {
            let detail = transport.to_string();
            if detail.contains("timeout") || detail.contains("Timed out") {
                return Err(NurlError::Timeout(detail));
            } else {
                return Err(NurlError::NetworkError(detail));
            }
        }
    };

    let status = response.status();
    if status >= 500 {
        return Err(NurlError::Status5xx(status, clean_url));
    }
    
    Ok((status, response.status_text().to_string()))
}
