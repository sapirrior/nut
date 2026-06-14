use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::cli::error_handler::print_and_exit;
use crate::client::request::{execute_request, RequestConfig};

#[derive(Args, Debug)]
pub struct UploadArgs {
    /// Target URL
    pub url: String,

    /// File to upload
    pub file: String,

    /// Additional form fields, repeatable
    #[arg(long, value_name = "key=val")]
    pub field: Vec<String>,

    /// Field name for the file part
    #[arg(long, default_value = "file")]
    pub name: String,

    /// Override detected MIME type
    #[arg(long)]
    pub mime: Option<String>,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: UploadArgs) {
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

    // Read the file to upload
    let file_bytes = match std::fs::read(&args.file) {
        Ok(b) => b,
        Err(e) => {
            let err = crate::cli::error_handler::NurlError::WriteError(format!(
                "could not read upload file '{}': {}", args.file, e
            ));
            if !args.common.silent {
                print_and_exit(&err);
            } else {
                std::process::exit(err.exit_code());
            }
        }
    };

    let file_name = std::path::Path::new(&args.file)
        .file_name()
        .and_then(|s| s.to_str())
        .unwrap_or("file");

    let mime_type = args.mime.clone().unwrap_or_else(|| {
        let ext = std::path::Path::new(&args.file)
            .extension()
            .and_then(|s| s.to_str())
            .unwrap_or("")
            .to_lowercase();
        match ext.as_str() {
            "jpg" | "jpeg" => "image/jpeg".to_string(),
            "png" => "image/png".to_string(),
            "gif" => "image/gif".to_string(),
            "pdf" => "application/pdf".to_string(),
            "txt" => "text/plain".to_string(),
            "json" => "application/json".to_string(),
            "html" | "htm" => "text/html".to_string(),
            _ => "application/octet-stream".to_string(),
        }
    });

    let boundary = "------------------------nurlboundary1234567890";
    let mut body = Vec::new();

    // 1. Add form fields
    for field in &args.field {
        if let Some((k, v)) = field.split_once('=') {
            body.extend_from_slice(format!("--{}\r\n", boundary).as_bytes());
            body.extend_from_slice(format!("Content-Disposition: form-data; name=\"{}\"\r\n\r\n", k.trim()).as_bytes());
            body.extend_from_slice(format!("{}\r\n", v.trim()).as_bytes());
        }
    }

    // 2. Add file part
    body.extend_from_slice(format!("--{}\r\n", boundary).as_bytes());
    body.extend_from_slice(format!(
        "Content-Disposition: form-data; name=\"{}\"; filename=\"{}\"\r\n",
        args.name, file_name
    ).as_bytes());
    body.extend_from_slice(format!("Content-Type: {}\r\n\r\n", mime_type).as_bytes());
    body.extend_from_slice(&file_bytes);
    body.extend_from_slice(b"\r\n");

    // 3. Close boundary
    body.extend_from_slice(format!("--{}--\r\n", boundary).as_bytes());

    // Set Content-Type header
    headers.push((
        "Content-Type".to_string(),
        format!("multipart/form-data; boundary={}", boundary),
    ));

    // Handle authentication flags
    if !args.common.no_auth {
        if let Some(ref tok) = args.common.bearer.as_ref().or(args.common.token.as_ref()) {
            headers.push(("Authorization".to_string(), format!("Bearer {}", tok)));
        } else if let Some(ref user) = args.common.user {
            use base64::Engine;
            let encoded = base64::engine::general_purpose::STANDARD.encode(user.as_bytes());
            headers.push(("Authorization".to_string(), format!("Basic {}", encoded)));
        }
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
        url: clean_url.clone(),
        method: "POST".to_string(),
        headers,
        body: Some(body),
        timeout: args.common.timeout,
        follow_redirects: args.common.location,
        output: args.common.output.clone(),
        verbose: args.common.verbose,
        silent: args.common.silent,
        tls,
    };

    match execute_request(config) {
        Ok(res_info) => {
            if !args.common.silent {
                crate::output::printer::print_response(
                    res_info.clone(),
                    args.common.include,
                    args.common.output,
                    args.common.verbose,
                    args.common.raw,
                );
            }
            if res_info.status >= 500 {
                std::process::exit(43);
            } else if res_info.status >= 400 {
                std::process::exit(22);
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
