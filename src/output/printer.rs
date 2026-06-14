use crate::client::response::ResponseInfo;
use std::io::IsTerminal;
use std::time::Duration;

fn print_meta_bar(status: u16, status_text: &str, bytes_size: usize, duration: Duration) {
    let size_str = if bytes_size >= 1024 * 1024 {
        format!("{:.1} MB", bytes_size as f64 / 1024.0 / 1024.0)
    } else if bytes_size >= 1024 {
        format!("{:.1} KB", bytes_size as f64 / 1024.0)
    } else {
        format!("{} B", bytes_size)
    };
    eprintln!(
        "{}  {}  {}  {}ms",
        status,
        status_text,
        size_str,
        duration.as_millis()
    );
}

pub fn print_response(info: ResponseInfo, include_headers: bool, output: Option<String>, verbose: bool, raw: bool) {
    if let Some(filepath) = output {
        if let Err(e) = std::fs::write(&filepath, &info.body) {
            crate::cli::error_handler::print_custom_error(
                "Could not save response",
                &[
                    &filepath,
                    &format!("Permission denied — {}", e),
                ],
            );
        } else {
            let show_meta = verbose || std::io::stdout().is_terminal();
            if show_meta {
                print_meta_bar(info.status, &info.status_text, info.bytes_size, info.duration);
                eprintln!();
            }
            if include_headers {
                println!("HTTP/1.1 {} {}", info.status, info.status_text);
                for (k, v) in &info.headers {
                    println!("{}: {}", k, v);
                }
                println!();
            }
            
            let size_str = if info.bytes_size >= 1024 * 1024 {
                format!("{:.1} MB", info.bytes_size as f64 / 1024.0 / 1024.0)
            } else if info.bytes_size >= 1024 {
                format!("{:.1} KB", info.bytes_size as f64 / 1024.0)
            } else {
                format!("{} B", info.bytes_size)
            };
            crate::output::theme::print_success(&format!("Saved to {}", filepath), Some(&size_str));
        }
        return;
    }

    let show_meta = verbose || std::io::stdout().is_terminal();
    if show_meta {
        print_meta_bar(info.status, &info.status_text, info.bytes_size, info.duration);
        eprintln!();
    }

    if verbose {
        crate::output::theme::print_response_line(&format!("HTTP/1.1 {} {}", info.status, info.status_text));
        for (k, v) in &info.headers {
            crate::output::theme::print_response_line(&format!("{}: {}", k, v));
        }
        eprintln!();
    } else if include_headers {
        println!("HTTP/1.1 {} {}", info.status, info.status_text);
        for (k, v) in &info.headers {
            println!("{}: {}", k, v);
        }
        println!();
    }

    // Response Body
    if !info.body.is_empty() {
        let body_str = String::from_utf8_lossy(&info.body);
        let is_json = info.headers.iter().any(|(k, v)| {
            k.to_ascii_lowercase() == "content-type" && v.to_ascii_lowercase().contains("application/json")
        });

        if is_json && !raw {
            if let Ok(json_val) = serde_json::from_str::<serde_json::Value>(&body_str) {
                crate::output::json::print_pretty(&json_val);
                return;
            }
        }

        // Fallback to raw body
        println!("{}", body_str);
    }
}
