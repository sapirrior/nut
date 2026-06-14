use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::client::request::{execute_request, RequestConfig};
use crate::cli::error_handler::print_and_exit;

#[derive(Args, Debug)]
pub struct OptionsArgs {
    /// Target URL
    pub url: String,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: OptionsArgs) {
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

    let tls = crate::client::tls::TlsConfig {
        no_verify: args.common.no_verify,
        cacert: args.common.cacert.clone().map(std::path::PathBuf::from),
        cert: args.common.cert.clone().map(std::path::PathBuf::from),
        key: args.common.key.clone().map(std::path::PathBuf::from),
        force_tls12: args.common.tls12,
        force_tls13: args.common.tls13,
    };

    let config = RequestConfig {
        url: clean_url,
        method: "OPTIONS".to_string(),
        headers,
        body: None,
        timeout: args.common.timeout,
        follow_redirects: args.common.location,
        output: None,
        verbose: args.common.verbose,
        silent: args.common.silent,
        tls,
    };

    match execute_request(config) {
        Ok(res) => {
            if !args.common.silent {
                for (k, v) in &res.headers {
                    let k_lower = k.to_lowercase();
                    if k_lower == "allow" || k_lower.starts_with("access-control-") {
                        println!("{}: {}", k, v);
                    }
                }
            }
        }
        Err(err) => {
            if !args.common.silent {
                print_and_exit(&err);
            } else {
                std::process::exit(err.exit_code());
            }
        }
    }
}
