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



pub fn print_custom_error(message: &str, sub_items: &[&str]) {
    crate::output::theme::print_error(message, sub_items);
}

pub fn print_and_exit(err: &NurlError) -> ! {
    match err {
        NurlError::Generic(msg) => {
            print_custom_error(msg, &[]);
        }
        NurlError::NetworkError(detail) => {
            print_custom_error(
                "Could not connect to host",
                &[
                    detail,
                    "Check DNS settings or try --no-verify",
                ],
            );
        }
        NurlError::Timeout(detail) => {
            print_custom_error(
                "Request timed out",
                &[
                    detail,
                    "Increase the limit with -t <seconds>",
                ],
            );
        }
        NurlError::InvalidUrl(url) => {
            print_custom_error(
                "Invalid URL",
                &[
                    url,
                    "URLs must start with http:// or https://",
                ],
            );
        }
        NurlError::BadArgs(msg) => {
            print_custom_error(
                "Invalid arguments",
                &[
                    msg,
                    "Check CLI options",
                ],
            );
        }
        NurlError::TlsError(detail) => {
            print_custom_error(
                "TLS verification failed",
                &[
                    detail,
                    "Use --no-verify to skip, or --cacert <file> for a custom CA",
                ],
            );
        }
        NurlError::WriteError(detail) => {
            print_custom_error(
                "Could not save response",
                &[
                    detail,
                    "Permission denied — check path or run with write access",
                ],
            );
        }
        NurlError::Status4xx(code, url) => {
            let desc = format!("HTTP {}  {}", code, get_status_text(*code));
            print_custom_error(
                &desc,
                &[
                    url,
                    "The resource does not exist on the server or requires authentication",
                ],
            );
        }
        NurlError::Status5xx(code, url) => {
            let desc = format!("HTTP {}  {}", code, get_status_text(*code));
            print_custom_error(
                &desc,
                &[
                    url,
                    "Server encountered an error — try again or contact the API owner",
                ],
            );
        }
    }
    std::process::exit(err.exit_code());
}

fn get_status_text(code: u16) -> &'static str {
    match code {
        400 => "Bad Request",
        401 => "Unauthorized",
        403 => "Forbidden",
        404 => "Not Found",
        405 => "Method Not Allowed",
        500 => "Internal Server Error",
        502 => "Bad Gateway",
        503 => "Service Unavailable",
        504 => "Gateway Timeout",
        _ => "Error",
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_print_custom_error_layout() {
        // Just verify that printing custom error doesn't panic with empty/populated inputs
        print_custom_error("Test Error Message", &[]);
        print_custom_error(
            "Test Error Message With Sub Items",
            &[
                "Some error description",
                "Try another value",
            ],
        );
    }
}
