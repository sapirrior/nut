use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::utils::body::BodySource;

#[derive(Args, Debug)]
pub struct PatchArgs {
    /// Target URL
    pub url: String,

    /// Request body data
    #[arg(short = 'd', long)]
    pub body: Option<String>,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: PatchArgs) {
    let body = match args.body {
        Some(s) => BodySource::Raw(s),
        None => BodySource::None,
    };
    crate::cli::runner::run("PATCH", &args.url, body, &args.common);
}
