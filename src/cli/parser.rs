use clap::{Parser, Subcommand};
use crate::cli::commands::{get, post, put, delete, head, patch, options, download, upload, inspect, ping, resolve};

#[derive(Parser, Debug)]
#[command(name = "nurl")]
#[command(version)]
#[command(about = "NetworkURL - A clean, fast, and structured HTTP client CLI", long_about = None)]
#[command(after_help = "For detailed info and commands/flags usage, visit: https://github.com/sapirrior/nurl")]
pub struct Cli {
    #[command(subcommand)]
    pub command: Commands,
}

#[derive(Subcommand, Debug)]
pub enum Commands {
    /// Send a GET request to a URL
    Get(get::GetArgs),

    /// Send a POST request to a URL
    Post(post::PostArgs),

    /// Send a PUT request to a URL
    Put(put::PutArgs),

    /// Send a DELETE request to a URL
    Delete(delete::DeleteArgs),

    /// Fetch HTTP headers from a URL
    Head(head::HeadArgs),

    /// Send a PATCH request to a URL
    Patch(patch::PatchArgs),

    /// Show Allow and Access-Control headers
    Options(options::OptionsArgs),

    /// Stream a file download to disk
    Download(download::DownloadArgs),

    /// Upload a file as multipart/form-data
    Upload(upload::UploadArgs),

    /// Inspect the request headers and body without sending
    Inspect(inspect::InspectArgs),

    /// Ping a URL to measure latency
    Ping(ping::PingArgs),

    /// Resolve host DNS records
    Resolve(resolve::ResolveArgs),
}

pub fn parse_and_run() {
    let mut args: Vec<String> = std::env::args().collect();
    if args.len() > 1 {
        let first_arg = &args[1];
        let subcommands = vec![
            "get", "post", "put", "delete", "head", "patch", "options",
            "download", "upload", "inspect", "ping", "resolve",
            "help", "--help", "-h", "-V", "--version"
        ];
        if !subcommands.contains(&first_arg.as_str()) && !first_arg.starts_with('-') {
            args.insert(1, "get".to_string());
        }
    }

    let mut cli = Cli::parse_from(args);
    let config = crate::utils::config::load_config();
    match &mut cli.command {
        Commands::Get(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Post(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Put(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Delete(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Head(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Patch(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Options(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Download(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Upload(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Inspect(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Ping(ref mut args) => crate::utils::config::merge_config_into_args(&mut args.common, &config),
        Commands::Resolve(_) => {}
    }

    match cli.command {
        Commands::Get(args) => get::run(args),
        Commands::Post(args) => post::run(args),
        Commands::Put(args) => put::run(args),
        Commands::Delete(args) => delete::run(args),
        Commands::Head(args) => head::run(args),
        Commands::Patch(args) => patch::run(args),
        Commands::Options(args) => options::run(args),
        Commands::Download(args) => download::run(args),
        Commands::Upload(args) => upload::run(args),
        Commands::Inspect(args) => inspect::run(args),
        Commands::Ping(args) => ping::run(args),
        Commands::Resolve(args) => resolve::run(args),
    }
}

#[derive(clap::Args, Debug, Clone)]
pub struct CommonArgs {
    // Auth
    #[arg(short = 'u', long, env = "NURL_USER")]
    pub user: Option<String>,
    #[arg(long, env = "NURL_BEARER")]
    pub bearer: Option<String>,
    #[arg(long, env = "NURL_TOKEN")]
    pub token: Option<String>,
    #[arg(long)]
    pub digest: bool,
    #[arg(long, env = "NURL_NETRC")]
    pub netrc: bool,
    #[arg(long, env = "NURL_NETRC_FILE")]
    pub netrc_file: Option<String>,
    #[arg(long)]
    pub no_auth: bool,

    // Body
    #[arg(short = 'd', long)]
    pub data: Option<String>,
    #[arg(short = 'F', long)]
    pub form: Vec<String>,
    #[arg(long)]
    pub data_binary: Option<String>,
    #[arg(long)]
    pub data_urlencode: Option<String>,
    #[arg(short = 'j', long)]
    pub json: bool,

    // TLS
    #[arg(short = 'k', long)]
    pub no_verify: bool,
    #[arg(long, env = "NURL_CACERT")]
    pub cacert: Option<String>,
    #[arg(long)]
    pub cert: Option<String>,
    #[arg(long)]
    pub key: Option<String>,
    #[arg(long)]
    pub tls12: bool,
    #[arg(long)]
    pub tls13: bool,

    // Cookies
    #[arg(short = 'b', long)]
    pub cookie: Option<String>,
    #[arg(short = 'c', long)]
    pub cookie_jar: Option<String>,
    #[arg(long)]
    pub session: Option<String>,

    // Request Control
    #[arg(short = 'q', long)]
    pub query: Vec<String>,
    #[arg(short = 'X', long)]
    pub method: Option<String>,
    #[arg(short = 't', long, default_value_t = 30, env = "NURL_TIMEOUT")]
    pub timeout: u64,
    #[arg(long, default_value_t = 10, env = "NURL_CONNECT_TIMEOUT")]
    pub connect_timeout: u64,
    #[arg(short = 'L', long)]
    pub location: bool,
    #[arg(long, default_value_t = 5)]
    pub max_redirects: u32,
    #[arg(short = 'A', long, env = "NURL_USER_AGENT")]
    pub user_agent: Option<String>,
    #[arg(short = 'e', long)]
    pub referer: Option<String>,
    #[arg(short = 'H', long)]
    pub header: Vec<String>,
    #[arg(long)]
    pub compressed: bool,
    #[arg(long)]
    pub http10: bool,
    #[arg(long)]
    pub http11: bool,
    #[arg(long)]
    pub http2: bool,
    #[arg(long, env = "NURL_PROXY")]
    pub proxy: Option<String>,
    #[arg(long)]
    pub proxy_user: Option<String>,
    #[arg(long, env = "NURL_NO_PROXY")]
    pub no_proxy: Option<String>,
    #[arg(long)]
    pub interface: Option<String>,
    #[arg(long)]
    pub limit_rate: Option<String>,
    #[arg(long)]
    pub max_filesize: Option<String>,
    #[arg(long)]
    pub keepalive: bool,
    #[arg(long, default_value_t = 0)]
    pub retry: u32,
    #[arg(long)]
    pub retry_delay: Option<u64>,
    #[arg(long)]
    pub retry_on: Option<String>,

    // Output
    #[arg(short = 'o', long)]
    pub output: Option<String>,
    #[arg(short = 'O')]
    pub output_name: bool,
    #[arg(short = 'i', long)]
    pub include: bool,
    #[arg(short = 'v', long)]
    pub verbose: bool,
    #[arg(short = 's', long)]
    pub silent: bool,
    #[arg(short = 'S', long)]
    pub show_error: bool,
    #[arg(short = 'w', long)]
    pub write_out: Option<String>,
    #[arg(long)]
    pub fail: bool,
    #[arg(long)]
    pub fail_with_body: bool,
    #[arg(long)]
    pub raw: bool,
    #[arg(long)]
    pub head_only: bool,
    #[arg(long)]
    pub body_only: bool,
    #[arg(long)]
    pub dump_header: Option<String>,
    #[arg(long)]
    pub trace: Option<String>,
    #[arg(long)]
    pub format: Option<String>,
}
