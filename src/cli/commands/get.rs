use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::utils::body::BodySource;

#[derive(Args, Debug)]
pub struct GetArgs {
    /// Target URL
    pub url: String,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: GetArgs) {
    crate::cli::runner::run("GET", &args.url, BodySource::None, &args.common);
}
