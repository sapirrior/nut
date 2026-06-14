use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::utils::body::BodySource;

#[derive(Args, Debug)]
pub struct PutArgs {
    /// Target URL
    pub url: String,

    /// Request body data
    #[arg(short = 'd', long)]
    pub body: Option<String>,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: PutArgs) {
    let body = match args.body {
        Some(s) => BodySource::Raw(s),
        None => BodySource::None,
    };
    crate::cli::runner::run("PUT", &args.url, body, &args.common);
}
