#[cfg(feature = "color")]
use owo_colors::{AnsiColors, OwoColorize};

pub fn print_pretty(val: &serde_json::Value, color: bool) {
    if color {
        #[cfg(feature = "color")]
        {
            print_colored_json(val, 0);
            println!();
            return;
        }
    }
    if let Ok(pretty) = serde_json::to_string_pretty(val) {
        println!("{}", pretty);
    }
}

#[cfg(feature = "color")]
fn print_colored_json(val: &serde_json::Value, indent: usize) {
    let indent_str = " ".repeat(indent);
    match val {
        serde_json::Value::Null => {
            print!("{}", "null".color(AnsiColors::Magenta));
        }
        serde_json::Value::Bool(b) => {
            print!("{}", b.color(AnsiColors::Magenta));
        }
        serde_json::Value::Number(n) => {
            print!("{}", n.color(AnsiColors::Yellow));
        }
        serde_json::Value::String(s) => {
            print!("\"{}\"", s.color(AnsiColors::Green));
        }
        serde_json::Value::Array(arr) => {
            if arr.is_empty() {
                print!("[]");
                return;
            }
            println!("[");
            for (i, item) in arr.iter().enumerate() {
                print!("{}", " ".repeat(indent + 2));
                print_colored_json(item, indent + 2);
                if i + 1 < arr.len() {
                    println!(",");
                } else {
                    println!();
                }
            }
            print!("{}]", indent_str);
        }
        serde_json::Value::Object(obj) => {
            if obj.is_empty() {
                print!("{{}}");
                return;
            }
            println!("{{");
            for (i, (k, v)) in obj.iter().enumerate() {
                print!("{}\"{}\": ", " ".repeat(indent + 2), k.color(AnsiColors::Cyan));
                print_colored_json(v, indent + 2);
                if i + 1 < obj.len() {
                    println!(",");
                } else {
                    println!();
                }
            }
            print!("{}}}", indent_str);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_print_pretty_no_color() {
        let val: serde_json::Value = serde_json::from_str(r#"{"a": 1}"#).unwrap();
        print_pretty(&val, false);
    }
}
