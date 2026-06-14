use std::time::Duration;

#[derive(Debug, Clone)]
pub struct ResponseInfo {
    pub status: u16,
    pub status_text: String,
    pub headers: Vec<(String, String)>,
    pub body: Vec<u8>,
    pub duration: Duration,
    pub bytes_size: usize,
}
