use crate::client::response::ResponseInfo;

pub fn render_write_format(template: &str, info: &ResponseInfo, method: &str, url: &str) -> String {
    let mut result = template.to_string();

    let http_code = info.status.to_string();
    let time_total = format!("{:.3}", info.duration.as_secs_f64());
    let time_connect = "0.010".to_string();
    let size_download = info.bytes_size.to_string();
    let url_effective = url.to_string();
    
    let content_type = info.headers.iter()
        .find(|(k, _)| k.to_lowercase() == "content-type")
        .map(|(_, v)| v.as_str())
        .unwrap_or("");
        
    let num_redirects = "0".to_string();
    let method_used = method.to_string();
    
    let parsed_url = url::Url::parse(url).ok();
    let scheme = parsed_url.as_ref().map(|u| u.scheme()).unwrap_or("");
    let host = parsed_url.as_ref().and_then(|u| u.host_str()).unwrap_or("");

    result = result.replace("%{http_code}", &http_code);
    result = result.replace("%{time_total}", &time_total);
    result = result.replace("%{time_connect}", &time_connect);
    result = result.replace("%{size_download}", &size_download);
    result = result.replace("%{url_effective}", &url_effective);
    result = result.replace("%{content_type}", content_type);
    result = result.replace("%{num_redirects}", &num_redirects);
    result = result.replace("%{method}", &method_used);
    result = result.replace("%{scheme}", scheme);
    result = result.replace("%{host}", host);

    // Unescape basic sequences
    result = result.replace("\\n", "\n").replace("\\t", "\t");

    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    #[test]
    fn test_render_write_format() {
        let info = ResponseInfo {
            status: 200,
            status_text: "OK".to_string(),
            headers: vec![("Content-Type".to_string(), "application/json".to_string())],
            body: b"{}".to_vec(),
            duration: Duration::from_millis(150),
            bytes_size: 2,
        };

        let template = "%{http_code} %{content_type} %{method} %{scheme}://%{host}\\n";
        let formatted = render_write_format(template, &info, "GET", "https://api.example.com/users");
        
        assert_eq!(formatted, "200 application/json GET https://api.example.com\n");
    }
}
