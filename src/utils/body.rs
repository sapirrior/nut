use crate::cli::error_handler::NurlError;
use std::io::Read;
use std::path::PathBuf;

#[derive(Debug, Clone, PartialEq, Eq)]
#[allow(dead_code)]
pub enum BodySource {
    Raw(String),
    File(PathBuf),
    Stdin,
    Form(Vec<(String, String)>),
    Binary(PathBuf),
    UrlEncoded(String),
    None,
}

impl BodySource {
    pub fn resolve(self) -> Result<Option<Vec<u8>>, NurlError> {
        match self {
            BodySource::None => Ok(None),
            BodySource::Raw(s) => Ok(Some(s.into_bytes())),
            BodySource::File(path) => {
                let content = std::fs::read_to_string(&path)
                    .map_err(|e| NurlError::WriteError(format!("could not read body file '{}': {}", path.display(), e)))?;
                Ok(Some(content.trim_end_matches(|c| c == '\r' || c == '\n').to_string().into_bytes()))
            }
            BodySource::Stdin => {
                let mut buffer = Vec::new();
                std::io::stdin().read_to_end(&mut buffer)
                    .map_err(|e| NurlError::Generic(format!("failed to read from stdin: {}", e)))?;
                Ok(Some(buffer))
            }
            BodySource::Binary(path) => {
                let buffer = std::fs::read(&path)
                    .map_err(|e| NurlError::WriteError(format!("could not read binary file '{}': {}", path.display(), e)))?;
                Ok(Some(buffer))
            }
            BodySource::UrlEncoded(s) => {
                let encoded = url::form_urlencoded::byte_serialize(s.as_bytes()).collect::<String>();
                Ok(Some(encoded.into_bytes()))
            }
            BodySource::Form(fields) => {
                let mut serializer = url::form_urlencoded::Serializer::new(String::new());
                for (k, v) in fields {
                    serializer.append_pair(&k, &v);
                }
                Ok(Some(serializer.finish().into_bytes()))
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::NamedTempFile;
    use std::io::Write;

    #[test]
    fn test_body_source_raw() {
        let source = BodySource::Raw("hello world".to_string());
        assert_eq!(source.resolve().unwrap(), Some("hello world".to_string().into_bytes()));
    }

    #[test]
    fn test_body_source_file() {
        let mut file = NamedTempFile::new().unwrap();
        writeln!(file, "hello file\n").unwrap();
        let path = file.path().to_path_buf();
        let source = BodySource::File(path);
        assert_eq!(source.resolve().unwrap(), Some("hello file".to_string().into_bytes()));
    }

    #[test]
    fn test_body_source_binary() {
        let mut file = NamedTempFile::new().unwrap();
        write!(file, "hello binary\n").unwrap();
        let path = file.path().to_path_buf();
        let source = BodySource::Binary(path);
        assert_eq!(source.resolve().unwrap(), Some("hello binary\n".to_string().into_bytes()));
    }

    #[test]
    fn test_body_source_url_encoded() {
        let source = BodySource::UrlEncoded("hello world!".to_string());
        assert_eq!(source.resolve().unwrap(), Some("hello+world%21".to_string().into_bytes()));
    }

    #[test]
    fn test_body_source_form() {
        let source = BodySource::Form(vec![("name".to_string(), "bob".to_string()), ("age".to_string(), "42".to_string())]);
        assert_eq!(source.resolve().unwrap(), Some("name=bob&age=42".to_string().into_bytes()));
    }
}
