mod cli;
mod client;
mod output;
mod utils;

fn main() {
    cli::parser::parse_and_run();
}
