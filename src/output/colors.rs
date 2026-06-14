#![allow(dead_code)]

#[cfg(feature = "color")]
use owo_colors::{AnsiColors, OwoColorize};

pub enum StatusType {
    Success,     // 2xx -> Soft Green
    Redirect,    // 3xx -> Soft Yellow
    ClientError, // 4xx -> Soft Red
    ServerError, // 5xx -> Soft Red/Magenta
    Unknown,
}

impl StatusType {
    pub fn from_code(code: u16) -> Self {
        match code {
            200..=299 => StatusType::Success,
            300..=399 => StatusType::Redirect,
            400..=499 => StatusType::ClientError,
            500..=599 => StatusType::ServerError,
            _ => StatusType::Unknown,
        }
    }
}

pub fn paint_header_key(key: &str) -> String {
    #[cfg(feature = "color")]
    {
        key.color(AnsiColors::Cyan).to_string()
    }
    #[cfg(not(feature = "color"))]
    {
        key.to_string()
    }
}

pub fn paint_header_val(val: &str) -> String {
    #[cfg(feature = "color")]
    {
        val.dimmed().to_string()
    }
    #[cfg(not(feature = "color"))]
    {
        val.to_string()
    }
}

pub fn paint_error(msg: &str) -> String {
    #[cfg(feature = "color")]
    {
        format!("{} {}", "●".color(AnsiColors::Red).bold(), msg.color(AnsiColors::Red))
    }
    #[cfg(not(feature = "color"))]
    {
        format!("● {}", msg)
    }
}

pub fn paint_success(msg: &str) -> String {
    #[cfg(feature = "color")]
    {
        format!("{} {}", "●".color(AnsiColors::Green).bold(), msg)
    }
    #[cfg(not(feature = "color"))]
    {
        format!("● {}", msg)
    }
}

pub fn paint_suggestion(msg: &str) -> String {
    #[cfg(feature = "color")]
    {
        format!("{} {}", "●".color(AnsiColors::Yellow).bold(), msg)
    }
    #[cfg(not(feature = "color"))]
    {
        format!("● {}", msg)
    }
}

pub fn paint_dim(msg: &str) -> String {
    #[cfg(feature = "color")]
    {
        msg.dimmed().to_string()
    }
    #[cfg(not(feature = "color"))]
    {
        msg.to_string()
    }
}

pub fn should_color(common_color: bool, common_no_color: bool) -> bool {
    #[cfg(not(feature = "color"))]
    {
        let _ = common_color;
        let _ = common_no_color;
        false
    }
    #[cfg(feature = "color")]
    {
        if common_no_color || std::env::var("NO_COLOR").is_ok() {
            return false;
        }
        if common_color || std::env::var("NURL_COLOR").map(|v| v == "1" || v.to_lowercase() == "true").unwrap_or(false) {
            return true;
        }
        false
    }
}

