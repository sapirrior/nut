use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::cli::error_handler::print_and_exit;
use crate::utils::body::BodySource;

#[derive(Args, Debug)]
pub struct InspectArgs {
    /// HTTP Method (GET, POST, PUT, etc.)
    pub method: String,

    /// Target URL
    pub url: String,

    /// Request body data
    #[arg(short = 'd', long)]
    pub body: Option<String>,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: InspectArgs) {
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

    let has_content_type = headers.iter().any(|(k, _)| k.to_lowercase() == "content-type");
    if args.common.json && !has_content_type {
        headers.push(("Content-Type".to_string(), "application/json".to_string()));
    }

    if !args.common.no_auth {
        if let Some(ref tok) = args.common.bearer.as_ref().or(args.common.token.as_ref()) {
            headers.push(("Authorization".to_string(), format!("Bearer {}", tok)));
        } else if let Some(ref user) = args.common.user {
            use base64::Engine;
            let encoded = base64::engine::general_purpose::STANDARD.encode(user.as_bytes());
            headers.push(("Authorization".to_string(), format!("Basic {}", encoded)));
        }
    } else {
        headers.retain(|(k, _)| k.to_lowercase() != "authorization");
    }

    let body_source = match args.body {
        Some(s) => BodySource::Raw(s),
        None => BodySource::None,
    };

    let body = match body_source.resolve() {
        Ok(b) => b,
        Err(e) => {
            if !args.common.silent {
                print_and_exit(&e);
            } else {
                std::process::exit(e.exit_code());
            }
        }
    };

    let has_content_length = headers.iter().any(|(k, _)| k.to_lowercase() == "content-length");
    if let Some(ref b) = body {
        if !has_content_length {
            headers.push(("Content-Length".to_string(), b.len().to_string()));
        }
    }

    let parsed_url = url::Url::parse(&clean_url).ok();
    let host = parsed_url.as_ref().and_then(|u| u.host_str()).unwrap_or(&clean_url);
    let path_and_query = parsed_url.as_ref()
        .map(|u| {
            let mut pq = u.path().to_string();
            if let Some(q) = u.query() {
                pq.push('?');
                pq.push_str(q);
            }
            pq
        })
        .unwrap_or_else(|| "/".to_string());

    println!("> {} {} HTTP/1.1", args.method.to_uppercase(), path_and_query);
    println!("> Host: {}", host);
    for (k, v) in &headers {
        let redacted = crate::utils::headers::redact_header_value(k, v);
        println!("> {}: {}", k, redacted);
    }
    println!(">");
    if let Some(ref b) = body {
        let body_str = String::from_utf8_lossy(b);
        for line in body_str.lines() {
            println!("> {}", line);
        }
    }
}
