use crate::cli::error_handler::NurlError;

/// Parses and validates custom header strings.
/// Returns an error if a header is malformed (i.e. lacks a colon separator).
pub fn parse_headers(raw_headers: &[String]) -> Result<Vec<(String, String)>, NurlError> {
    let mut headers = Vec::new();
    for h in raw_headers {
        if let Some((k, v)) = h.split_once(':') {
            headers.push((k.trim().to_string(), v.trim().to_string()));
        } else {
            return Err(NurlError::BadArgs(format!(
                "invalid header format: \"{}\"", h
            )));
        }
    }
    Ok(headers)
}

/// Redacts sensitive headers like "Authorization" and "Cookie".
pub fn redact_header_value(key: &str, value: &str) -> String {
    let key_lower = key.to_ascii_lowercase();
    if key_lower == "authorization" || key_lower == "cookie" {
        "[hidden]".to_string()
    } else {
        value.to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_headers() {
        let raw = vec!["Content-Type: application/json".to_string(), "Authorization: Bearer test".to_string()];
        let parsed = parse_headers(&raw).unwrap();
        assert_eq!(parsed[0], ("Content-Type".to_string(), "application/json".to_string()));
        assert_eq!(parsed[1], ("Authorization".to_string(), "Bearer test".to_string()));

        let bad = vec!["MalformedHeader".to_string()];
        assert!(parse_headers(&bad).is_err());
    }

    #[test]
    fn test_redact_header_value() {
        assert_eq!(redact_header_value("Authorization", "Bearer 123"), "[hidden]");
        assert_eq!(redact_header_value("cookie", "session=xyz"), "[hidden]");
        assert_eq!(redact_header_value("Content-Type", "application/json"), "application/json");
    }
}
