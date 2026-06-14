use std::fmt;
use std::error::Error;

#[derive(Debug)]
pub enum NurlError {
    Generic(String),
    NetworkError(String),
    Timeout(String),
    InvalidUrl(String),
    BadArgs(String),
    TlsError(String),
    WriteError(String),
    Status4xx(u16, String), // (code, url)
    Status5xx(u16, String), // (code, url)
}

impl NurlError {
    pub fn exit_code(&self) -> i32 {
        match self {
            NurlError::Generic(_) => 1,
            NurlError::NetworkError(_) => 2,
            NurlError::Timeout(_) => 3,
            NurlError::InvalidUrl(_) | NurlError::BadArgs(_) => 4,
            NurlError::TlsError(_) => 5,
            NurlError::WriteError(_) => 6,
            NurlError::Status4xx(_, _) => 22,
            NurlError::Status5xx(_, _) => 43,
        }
    }
}

impl fmt::Display for NurlError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            NurlError::Generic(msg) => write!(f, "{}", msg),
            NurlError::NetworkError(_) => write!(f, "could not connect to host"),
            NurlError::Timeout(_) => write!(f, "request timed out"),
            NurlError::InvalidUrl(_) => write!(f, "invalid URL format"),
            NurlError::BadArgs(msg) => write!(f, "{}", msg),
            NurlError::TlsError(_) => write!(f, "tls certificate verification failed"),
            NurlError::WriteError(_) => write!(f, "could not write response to file"),
            NurlError::Status4xx(code, _) => write!(f, "http {} client error", code),
            NurlError::Status5xx(code, _) => write!(f, "http {} server error", code),
        }
    }
}

impl Error for NurlError {}

pub fn print_and_exit(err: &NurlError) -> ! {
    eprintln!("error: {}", err);
    match err {
        NurlError::Generic(msg) => {
            eprintln!("  details: {}", msg);
        }
        NurlError::NetworkError(detail) => {
            eprintln!("  details: {}", detail);
            eprintln!("  hint: check DNS or try --no-verify");
        }
        NurlError::Timeout(detail) => {
            eprintln!("  details: {}", detail);
            eprintln!("  hint: increase timeout with -t <seconds>");
        }
        NurlError::InvalidUrl(url) => {
            eprintln!("  url: {}", url);
            eprintln!("  hint: check URL format");
        }
        NurlError::BadArgs(msg) => {
            eprintln!("  details: {}", msg);
            eprintln!("  hint: check CLI options");
        }
        NurlError::TlsError(detail) => {
            eprintln!("  details: {}", detail);
            eprintln!("  hint: use --no-verify to skip (insecure) or --cacert to provide a custom CA");
        }
        NurlError::WriteError(detail) => {
            eprintln!("  details: {}", detail);
            eprintln!("  hint: check file permissions or path");
        }
        NurlError::Status4xx(code, url) => {
            eprintln!("  url: {}", url);
            eprintln!("  hint: http {} status code indicates client error", code);
        }
        NurlError::Status5xx(code, url) => {
            eprintln!("  url: {}", url);
            eprintln!("  hint: http {} status code indicates server error", code);
        }
    }
    std::process::exit(err.exit_code());
}
