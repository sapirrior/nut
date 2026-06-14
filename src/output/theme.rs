pub fn print_error(message: &str, sub_items: &[&str]) {
    eprintln!("! {}", message);
    for item in sub_items {
        eprintln!("  - {}", item);
    }
}

pub fn print_notice(msg: &str) {
    eprintln!("* {}", msg);
}

pub fn print_success(msg: &str, detail: Option<&str>) {
    if let Some(d) = detail {
        eprintln!("+ {}  {}", msg, d);
    } else {
        eprintln!("+ {}", msg);
    }
}

pub fn print_request_line(line: &str) {
    eprintln!("> {}", line);
}

pub fn print_response_line(line: &str) {
    eprintln!("< {}", line);
}
