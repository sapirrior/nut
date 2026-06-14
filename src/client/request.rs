use crate::cli::error_handler::NurlError;
use crate::client::response::ResponseInfo;
use crate::client::tls::TlsConfig;
use std::io::Read;
use std::time::{Duration, Instant};

#[allow(dead_code)]
pub struct RequestConfig {
    pub url: String,
    pub method: String,
    pub headers: Vec<(String, String)>,
    pub body: Option<Vec<u8>>,
    pub timeout: u64,
    pub follow_redirects: bool,
    pub output: Option<String>,
    pub verbose: bool,
    pub silent: bool,
    pub tls: TlsConfig,
}

pub fn execute_request(config: RequestConfig) -> Result<ResponseInfo, NurlError> {
    let tls_config = crate::client::tls::build_tls_config(&config.tls)
        .map_err(|e| NurlError::TlsError(e))?;

    let mut agent_builder = ureq::AgentBuilder::new()
        .timeout_connect(Duration::from_secs(10))
        .timeout(Duration::from_secs(config.timeout))
        .tls_config(tls_config);

    if config.follow_redirects {
        agent_builder = agent_builder.redirects(5);
    } else {
        agent_builder = agent_builder.redirects(0);
    }

    let agent = agent_builder.build();
    let clean_url = &config.url;

    if config.verbose && !config.silent {
        let parsed_url = url::Url::parse(clean_url).ok();
        let host = parsed_url.as_ref().and_then(|u| u.host_str()).unwrap_or(clean_url);
        let port = parsed_url.as_ref().and_then(|u| u.port()).unwrap_or(if clean_url.starts_with("https") { 443 } else { 80 });
        crate::output::theme::print_notice(&format!("connecting to {} port {}", host, port));
        if clean_url.starts_with("https") {
            crate::output::theme::print_notice("TLS handshake complete");
        }
        eprintln!();

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
        crate::output::theme::print_request_line(&format!("{} {} HTTP/1.1", config.method, path_and_query));
        if let Some(ref u) = parsed_url {
            crate::output::theme::print_request_line(&format!("Host: {}", u.host_str().unwrap_or("")));
        }
        for (k, v) in &config.headers {
            let redacted_val = crate::utils::headers::redact_header_value(k, v);
            crate::output::theme::print_request_line(&format!("{}: {}", k, redacted_val));
        }
        if let Some(ref body) = config.body {
            crate::output::theme::print_request_line(&format!("[{} bytes payload]", body.len()));
        }
        eprintln!();
    }

    let mut request = match config.method.as_str() {
        "GET" => agent.get(clean_url),
        "POST" => agent.post(clean_url),
        "PUT" => agent.put(clean_url),
        "DELETE" => agent.delete(clean_url),
        "HEAD" => agent.head(clean_url),
        "PATCH" => agent.patch(clean_url),
        "OPTIONS" => agent.request("OPTIONS", clean_url),
        _ => return Err(NurlError::Generic(format!("Unsupported method: {}", config.method))),
    };

    // Add custom headers
    for (k, v) in &config.headers {
        request = request.set(k, v);
    }

    let start = Instant::now();

    // Perform request and capture response
    let response_result = if let Some(ref body) = config.body {
        request.send_bytes(body)
    } else {
        request.call()
    };

    let duration = start.elapsed();

    let response = match response_result {
        Ok(res) => res,
        Err(ureq::Error::Status(_status, res)) => res, // Still print response details for 4xx/5xx responses
        Err(ureq::Error::Transport(transport)) => {
            let detail = transport.to_string();
            if detail.contains("timeout") || detail.contains("Timed out") {
                return Err(NurlError::Timeout(detail));
            } else if detail.contains("Bad URL") || detail.contains("parse") {
                return Err(NurlError::InvalidUrl(clean_url.clone()));
            } else if detail.contains("tls") || detail.contains("certificate") || detail.contains("handshake") {
                return Err(NurlError::TlsError(detail));
            } else {
                return Err(NurlError::NetworkError(detail));
            }
        }
    };

    let status = response.status();
    let status_text = response.status_text().to_string();

    // Map headers
    let mut headers = Vec::new();
    for name in response.headers_names() {
        if let Some(val) = response.header(&name) {
            headers.push((name, val.to_string()));
        }
    }

    // Read body as bytes
    let mut body = Vec::new();
    if let Err(e) = response.into_reader().read_to_end(&mut body) {
        return Err(NurlError::NetworkError(format!("Failed to read response body: {}", e)));
    }

    let bytes_size = body.len();

    Ok(ResponseInfo {
        status,
        status_text,
        headers,
        body,
        duration,
        bytes_size,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use httpmock::prelude::*;

    #[test]
    fn test_execute_request_success() {
        let server = MockServer::start();
        let mock = server.mock(|when, then| {
            when.method(GET)
                .path("/test");
            then.status(200)
                .header("Content-Type", "application/json")
                .body(r#"{"success": true}"#);
        });

        let config = RequestConfig {
            url: server.url("/test"),
            method: "GET".to_string(),
            headers: vec![],
            body: None,
            timeout: 10,
            follow_redirects: false,
            output: None,
            verbose: false,
            silent: true,
            tls: TlsConfig::default(),
        };

        let res = execute_request(config).unwrap();
        assert_eq!(res.status, 200);
        assert_eq!(res.body, r#"{"success": true}"#.as_bytes());
        mock.assert();
    }
}
