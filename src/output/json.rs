pub fn print_pretty(val: &serde_json::Value) {
    if let Ok(pretty) = serde_json::to_string_pretty(val) {
        println!("{}", pretty);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_print_pretty_no_color() {
        let val: serde_json::Value = serde_json::from_str(r#"{"a": 1}"#).unwrap();
        print_pretty(&val);
    }
}
