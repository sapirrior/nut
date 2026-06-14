use serde::Deserialize;
use std::collections::HashMap;
use crate::cli::parser::CommonArgs;

#[derive(Deserialize, Debug, Default, Clone)]
pub struct ConfigFile {
    pub defaults: Option<DefaultsConfig>,
    pub headers: Option<HashMap<String, String>>,
    pub tls: Option<TlsConfigFile>,
    pub cookies: Option<CookiesConfigFile>,
}

#[derive(Deserialize, Debug, Default, Clone)]
pub struct DefaultsConfig {
    pub timeout: Option<u64>,
    pub connect_timeout: Option<u64>,
    pub follow_redirects: Option<bool>,
    pub max_redirects: Option<u32>,
    pub user_agent: Option<String>,
    pub compressed: Option<bool>,
    pub retry: Option<u32>,
}

#[derive(Deserialize, Debug, Default, Clone)]
pub struct TlsConfigFile {
    pub cacert: Option<String>,
}

#[derive(Deserialize, Debug, Default, Clone)]
pub struct CookiesConfigFile {
    pub jar: Option<String>,
}

pub fn load_config() -> ConfigFile {
    let path = if let Ok(override_path) = std::env::var("NURL_CONFIG") {
        std::path::PathBuf::from(override_path)
    } else if let Ok(home) = std::env::var("HOME") {
        let mut p = std::path::PathBuf::from(home);
        p.push(".config");
        p.push("nurl");
        p.push("config.toml");
        p
    } else {
        return ConfigFile::default();
    };

    if path.exists() {
        if let Ok(content) = std::fs::read_to_string(path) {
            if let Ok(config) = toml::from_str::<ConfigFile>(&content) {
                return config;
            }
        }
    }
    ConfigFile::default()
}

pub fn merge_config_into_args(args: &mut CommonArgs, config: &ConfigFile) {
    if let Some(ref defaults) = config.defaults {
        if args.timeout == 30 {
            if let Some(val) = defaults.timeout {
                args.timeout = val;
            }
        }
        if args.connect_timeout == 10 {
            if let Some(val) = defaults.connect_timeout {
                args.connect_timeout = val;
            }
        }
        if !args.location {
            if let Some(val) = defaults.follow_redirects {
                args.location = val;
            }
        }
        if args.max_redirects == 5 {
            if let Some(val) = defaults.max_redirects {
                args.max_redirects = val;
            }
        }
        if args.user_agent.is_none() {
            if let Some(ref val) = defaults.user_agent {
                args.user_agent = Some(val.clone());
            }
        }
        if !args.compressed {
            if let Some(val) = defaults.compressed {
                args.compressed = val;
            }
        }
        if args.retry == 0 {
            if let Some(val) = defaults.retry {
                args.retry = val;
            }
        }
    }

    if let Some(ref headers) = config.headers {
        for (k, v) in headers {
            let exists = args.header.iter().any(|h| {
                h.split_once(':')
                    .map(|(ck, _)| ck.trim().to_lowercase() == k.to_lowercase())
                    .unwrap_or(false)
            });
            if !exists {
                args.header.push(format!("{}: {}", k, v));
            }
        }
    }

    if let Some(ref tls) = config.tls {
        if args.cacert.is_none() {
            if let Some(ref val) = tls.cacert {
                args.cacert = Some(val.clone());
            }
        }
    }

    if let Some(ref cookies) = config.cookies {
        if args.cookie_jar.is_none() && args.session.is_none() {
            if let Some(ref val) = cookies.jar {
                args.cookie_jar = Some(val.clone());
            }
        }
    }
}
