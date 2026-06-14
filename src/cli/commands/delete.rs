use clap::Args;
use crate::cli::parser::CommonArgs;
use crate::utils::body::BodySource;

#[derive(Args, Debug)]
pub struct DeleteArgs {
    /// Target URL
    pub url: String,

    #[clap(flatten)]
    pub common: CommonArgs,
}

pub fn run(args: DeleteArgs) {
    crate::cli::runner::run("DELETE", &args.url, BodySource::None, &args.common);
}
