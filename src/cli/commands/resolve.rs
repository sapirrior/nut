use clap::Args;
use std::net::ToSocketAddrs;

#[derive(Args, Debug)]
pub struct ResolveArgs {
    /// Hostname to resolve (e.g. api.example.com)
    pub host: String,
}

pub fn run(args: ResolveArgs) {
    let host = args.host.trim();
    // Strip scheme if it is a full URL
    let target_host = if host.contains("://") {
        if let Ok(parsed) = url::Url::parse(host) {
            parsed.host_str().unwrap_or(host).to_string()
        } else {
            host.to_string()
        }
    } else {
        host.to_string()
    };

    match format!("{}:80", target_host).to_socket_addrs() {
        Ok(addrs) => {
            let mut found = false;
            for addr in addrs {
                found = true;
                let ip = addr.ip();
                let record_type = if ip.is_ipv4() { "A" } else { "AAAA" };
                println!("{} → {}  ({})", target_host, ip, record_type);
            }
            if !found {
                eprintln!("error: could not resolve host '{}'", target_host);
                std::process::exit(2);
            }
        }
        Err(e) => {
            eprintln!("error: DNS resolution failed for host '{}'", target_host);
            eprintln!("  details: {}", e);
            std::process::exit(2);
        }
    }
}
