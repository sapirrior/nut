use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::utils::body::BodySource;

#[derive(Args, Debug)]
pub struct HeadArgs {
    /// Target URL
    pub url: String,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: HeadArgs) {
    let mut common = args.common.clone();
    common.include = true;
    crate::cli::runner::run("HEAD", &args.url, BodySource::None, &common);
}
