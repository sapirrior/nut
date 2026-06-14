use std::sync::Arc;
use rustls::client::danger::{ServerCertVerifier, ServerCertVerified};
use rustls::pki_types::{CertificateDer, ServerName, UnixTime};

#[derive(Debug, Clone, Default)]
#[allow(dead_code)]
pub struct TlsConfig {
    pub no_verify: bool,
    pub cacert: Option<std::path::PathBuf>,
    pub cert: Option<std::path::PathBuf>,
    pub key: Option<std::path::PathBuf>,
    pub force_tls12: bool,
    pub force_tls13: bool,
}

#[derive(Debug)]
struct NoVerifier;

impl ServerCertVerifier for NoVerifier {
    fn verify_server_cert(
        &self,
        _end_entity: &CertificateDer<'_>,
        _intermediates: &[CertificateDer<'_>],
        _server_name: &ServerName<'_>,
        _ocsp_response: &[u8],
        _now: UnixTime,
    ) -> Result<ServerCertVerified, rustls::Error> {
        Ok(ServerCertVerified::assertion())
    }

    fn verify_tls12_signature(
        &self,
        _message: &[u8],
        _cert: &CertificateDer<'_>,
        _dss: &rustls::DigitallySignedStruct,
    ) -> Result<rustls::client::danger::HandshakeSignatureValid, rustls::Error> {
        Ok(rustls::client::danger::HandshakeSignatureValid::assertion())
    }

    fn verify_tls13_signature(
        &self,
        _message: &[u8],
        _cert: &CertificateDer<'_>,
        _dss: &rustls::DigitallySignedStruct,
    ) -> Result<rustls::client::danger::HandshakeSignatureValid, rustls::Error> {
        Ok(rustls::client::danger::HandshakeSignatureValid::assertion())
    }

    fn supported_verify_schemes(&self) -> Vec<rustls::SignatureScheme> {
        rustls::crypto::ring::default_provider()
            .signature_verification_algorithms
            .supported_schemes()
    }
}

fn load_pem_certs(path: &std::path::Path) -> Result<Vec<CertificateDer<'static>>, String> {
    let content = std::fs::read_to_string(path)
        .map_err(|e| format!("failed to read cert file: {}", e))?;
    let mut certs = Vec::new();
    let mut current_cert = String::new();
    let mut inside = false;
    for line in content.lines() {
        let trimmed = line.trim();
        if trimmed == "-----BEGIN CERTIFICATE-----" {
            inside = true;
            current_cert.clear();
        } else if trimmed == "-----END CERTIFICATE-----" {
            inside = false;
            use base64::Engine;
            let decoded = base64::engine::general_purpose::STANDARD
                .decode(current_cert.replace('\n', "").replace('\r', ""))
                .map_err(|e| format!("failed to decode base64 certificate: {}", e))?;
            certs.push(CertificateDer::from(decoded));
        } else if inside {
            current_cert.push_str(trimmed);
        }
    }
    Ok(certs)
}

pub fn build_tls_config(config: &TlsConfig) -> Result<Arc<rustls::ClientConfig>, String> {
    let mut root_store = rustls::RootCertStore::empty();
    
    if let Some(ref cacert) = config.cacert {
        let certs = load_pem_certs(cacert)?;
        for cert in certs {
            root_store.add(cert)
                .map_err(|e| format!("failed to add CA certificate to store: {}", e))?;
        }
    } else {
        root_store.extend(webpki_roots::TLS_SERVER_ROOTS.iter().cloned());
    }

    let provider = rustls::crypto::ring::default_provider();
    
    let mut client_config = if config.no_verify {
        let mut cfg = rustls::ClientConfig::builder_with_provider(Arc::new(provider))
            .with_safe_default_protocol_versions()
            .map_err(|e| e.to_string())?
            .with_root_certificates(root_store)
            .with_no_client_auth();
        cfg.dangerous().set_certificate_verifier(Arc::new(NoVerifier));
        cfg
    } else {
        rustls::ClientConfig::builder_with_provider(Arc::new(provider))
            .with_safe_default_protocol_versions()
            .map_err(|e| e.to_string())?
            .with_root_certificates(root_store)
            .with_no_client_auth()
    };

    if config.force_tls12 {
        client_config.alpn_protocols = vec![b"http/1.1".to_vec()];
    }

    Ok(Arc::new(client_config))
}
