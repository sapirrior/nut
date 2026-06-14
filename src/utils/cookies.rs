use std::fs::File;
use std::io::{BufRead, BufReader, Write};
use std::path::Path;

#[derive(Debug, Clone, Default)]
pub struct CookieJar {
    pub cookies: Vec<Cookie>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Cookie {
    pub domain: String,
    pub include_subdomains: bool,
    pub path: String,
    pub secure: bool,
    pub expiry: u64,
    pub name: String,
    pub value: String,
}

impl CookieJar {
    pub fn load_from_file(path: &Path) -> Result<Self, String> {
        let file = File::open(path).map_err(|e| e.to_string())?;
        let reader = BufReader::new(file);
        let mut jar = CookieJar::default();
        for line in reader.lines() {
            let line = line.map_err(|e| e.to_string())?;
            let trimmed = line.trim();
            if trimmed.is_empty() || trimmed.starts_with('#') {
                continue;
            }
            let parts: Vec<&str> = trimmed.split('\t').collect();
            if parts.len() >= 7 {
                jar.cookies.push(Cookie {
                    domain: parts[0].to_string(),
                    include_subdomains: parts[1].to_ascii_uppercase() == "TRUE",
                    path: parts[2].to_string(),
                    secure: parts[3].to_ascii_uppercase() == "TRUE",
                    expiry: parts[4].parse().unwrap_or(0),
                    name: parts[5].to_string(),
                    value: parts[6].to_string(),
                });
            }
        }
        Ok(jar)
    }

    pub fn save_to_file(&self, path: &Path) -> Result<(), String> {
        let mut file = File::create(path).map_err(|e| e.to_string())?;
        writeln!(file, "# Netscape HTTP Cookie File").map_err(|e| e.to_string())?;
        for c in &self.cookies {
            writeln!(
                file,
                "{}\t{}\t{}\t{}\t{}\t{}\t{}",
                c.domain,
                if c.include_subdomains { "TRUE" } else { "FALSE" },
                c.path,
                if c.secure { "TRUE" } else { "FALSE" },
                c.expiry,
                c.name,
                c.value
            ).map_err(|e| e.to_string())?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::NamedTempFile;

    #[test]
    fn test_cookie_jar_roundtrip() {
        let temp_file = NamedTempFile::new().unwrap();
        let path = temp_file.path();

        let mut jar = CookieJar::default();
        jar.cookies.push(Cookie {
            domain: "example.com".to_string(),
            include_subdomains: true,
            path: "/".to_string(),
            secure: true,
            expiry: 123456,
            name: "session".to_string(),
            value: "xyz".to_string(),
        });

        jar.save_to_file(path).unwrap();

        let loaded = CookieJar::load_from_file(path).unwrap();
        assert_eq!(loaded.cookies.len(), 1);
        assert_eq!(loaded.cookies[0], jar.cookies[0]);
    }
}
