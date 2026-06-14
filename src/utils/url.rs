/// Normalizes a user-provided URL string.
/// If the URL does not start with "http://" or "https://", it prepends "https://" by default.
pub fn clean_url(url: &str) -> String {
    let trimmed = url.trim();
    if trimmed.starts_with("http://") || trimmed.starts_with("https://") {
        trimmed.to_string()
    } else {
        format!("https://{}", trimmed)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_clean_url() {
        assert_eq!(clean_url("google.com"), "https://google.com");
        assert_eq!(clean_url("http://localhost:8080"), "http://localhost:8080");
        assert_eq!(clean_url("https://example.com/api"), "https://example.com/api");
    }
}
