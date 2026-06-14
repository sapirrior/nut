use crate::cli::error_handler::NurlError;
use crate::cli::parser::CommonArgs;
use crate::client::request::{execute_request, RequestConfig};
use crate::utils::body::BodySource;

pub fn run(method: &str, url: &str, body_source: BodySource, common: &CommonArgs) {
    if let Err(e) = execute_inner(method, url, body_source, common) {
        if !common.silent {
            crate::cli::error_handler::print_and_exit(&e);
        } else {
            std::process::exit(e.exit_code());
        }
    }
}

fn execute_inner(method: &str, url: &str, body_source: BodySource, common: &CommonArgs) -> Result<(), NurlError> {
    let clean_url = crate::utils::url::clean_url(url);
    let mut headers = crate::utils::headers::parse_headers(&common.header)?;

    // Append Content-Type for JSON if requested and not already defined
    let has_content_type = headers.iter().any(|(k, _)| k.to_lowercase() == "content-type");
    if common.json && !has_content_type {
        headers.push(("Content-Type".to_string(), "application/json".to_string()));
    }

    // Apply authentication flags
    if !common.no_auth {
        if let Some(ref tok) = common.bearer.as_ref().or(common.token.as_ref()) {
            headers.push(("Authorization".to_string(), format!("Bearer {}", tok)));
        } else if let Some(ref user) = common.user {
            if common.digest {
                return Err(NurlError::Generic("HTTP Digest auth is not supported".to_string()));
            }
            use base64::Engine;
            let encoded = base64::engine::general_purpose::STANDARD.encode(user.as_bytes());
            headers.push(("Authorization".to_string(), format!("Basic {}", encoded)));
        }
    } else {
        // Strip any Authorization header
        headers.retain(|(k, _)| k.to_lowercase() != "authorization");
    }

    // Load cookies if requested
    let mut cookie_list = Vec::new();
    let mut loaded_jar = crate::utils::cookies::CookieJar::default();
    let mut jar_loaded = false;

    if let Some(ref cookie_opt) = common.cookie {
        if cookie_opt.starts_with('@') {
            let path_str = &cookie_opt[1..];
            if let Ok(jar) = crate::utils::cookies::CookieJar::load_from_file(std::path::Path::new(path_str)) {
                loaded_jar = jar;
                jar_loaded = true;
            }
        } else {
            cookie_list.push(cookie_opt.clone());
        }
    }

    if let Some(ref session_opt) = common.session {
        let path = std::path::Path::new(session_opt);
        if path.exists() {
            if let Ok(jar) = crate::utils::cookies::CookieJar::load_from_file(path) {
                if jar_loaded {
                    loaded_jar.cookies.extend(jar.cookies);
                } else {
                    loaded_jar = jar;
                    jar_loaded = true;
                }
            }
        }
    }

    if jar_loaded {
        for cookie in &loaded_jar.cookies {
            cookie_list.push(format!("{}={}", cookie.name, cookie.value));
        }
    }

    if !cookie_list.is_empty() {
        headers.push(("Cookie".to_string(), cookie_list.join("; ")));
    }

    // Resolve body source (Phase 3 supports all resolved sources)
    let body = body_source.resolve()?;

    // Map TLS configuration
    let tls = crate::client::tls::TlsConfig {
        no_verify: common.no_verify,
        cacert: common.cacert.clone().map(std::path::PathBuf::from),
        cert: common.cert.clone().map(std::path::PathBuf::from),
        key: common.key.clone().map(std::path::PathBuf::from),
        force_tls12: common.tls12,
        force_tls13: common.tls13,
    };

    let config = RequestConfig {
        url: clean_url.clone(),
        method: method.to_string(),
        headers,
        body,
        timeout: common.timeout,
        follow_redirects: common.location,
        output: common.output.clone(),
        verbose: common.verbose,
        silent: common.silent,
        tls,
    };

    let res_info = execute_request(config)?;

    // Extract Set-Cookie headers and save to jar
    let save_path = common.cookie_jar.as_ref().or(common.session.as_ref());
    if let Some(ref path_str) = save_path {
        let path = std::path::Path::new(path_str);
        let mut save_jar = if path.exists() {
            crate::utils::cookies::CookieJar::load_from_file(path).unwrap_or_default()
        } else {
            crate::utils::cookies::CookieJar::default()
        };

        for (k, v) in &res_info.headers {
            if k.to_lowercase() == "set-cookie" {
                let parts: Vec<&str> = v.split(';').collect();
                if !parts.is_empty() {
                    if let Some((name, val)) = parts[0].split_once('=') {
                        let name = name.trim().to_string();
                        let value = val.trim().to_string();
                        let mut domain = "".to_string();
                        let mut cookie_path = "/".to_string();
                        let mut secure = false;
                        for attr in &parts[1..] {
                            let attr_trim = attr.trim();
                            if let Some((k_attr, v_attr)) = attr_trim.split_once('=') {
                                let k_attr_lower = k_attr.trim().to_lowercase();
                                let v_attr_trim = v_attr.trim().to_string();
                                if k_attr_lower == "domain" {
                                    domain = v_attr_trim;
                                } else if k_attr_lower == "path" {
                                    cookie_path = v_attr_trim;
                                }
                            } else if attr_trim.to_lowercase() == "secure" {
                                secure = true;
                            }
                        }
                        if domain.is_empty() {
                            if let Ok(parsed_url) = url::Url::parse(&clean_url) {
                                domain = parsed_url.host_str().unwrap_or("").to_string();
                            }
                        }
                        save_jar.cookies.retain(|c| !(c.name == name && c.domain == domain));
                        save_jar.cookies.push(crate::utils::cookies::Cookie {
                            domain,
                            include_subdomains: true,
                            path: cookie_path,
                            secure,
                            expiry: 0,
                            name,
                            value,
                        });
                    }
                }
            }
        }
        let _ = save_jar.save_to_file(path);
    }

    if !common.silent {
        crate::output::printer::print_response(
            res_info.clone(),
            common.include,
            common.output.clone(),
            common.verbose,
            common.raw,
        );

        if let Some(ref template) = common.write_out {
            let formatted = crate::output::format::render_write_format(template, &res_info, method, url);
            print!("{}", formatted);
        }
    }

    if res_info.status >= 500 {
        return Err(NurlError::Status5xx(res_info.status, clean_url));
    } else if res_info.status >= 400 {
        return Err(NurlError::Status4xx(res_info.status, clean_url));
    }

    Ok(())
}
