#![allow(unused)]
use pest::Parser;
use pest_derive::Parser;
use std::env;
use std::fs;

#[derive(Parser)]
#[grammar = "expression.pest"]
pub struct ExpressionParser;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        return;
    }
    let filename = &args[1];

    let content = fs::read_to_string(filename).expect("Cannot read file");

    // 开始解析
    match ExpressionParser::parse(Rule::CompUnit, &content) {
        Ok(pairs) => {
            for pair in pairs.clone().next().unwrap().into_inner() {
                let rule = pair.as_rule();
                let span = pair.as_span();
                let (line, _) = span.start_pos().line_col();
                let text = pair.as_str();

                match rule {
                    Rule::INTEGER_CONST => {
                        let val = if text.starts_with("0x") || text.starts_with("0X") {
                            i32::from_str_radix(&text[2..], 16).unwrap()
                        } else if text.starts_with('0') && text.len() > 1 {
                            i32::from_str_radix(text, 8).unwrap()
                        } else {
                            text.parse::<i32>().unwrap()
                        };
                        eprintln!("INTEGER_CONST {} at Line {}.", val, line);
                    }
                    Rule::FLOAT_CONST => {
                        let val = text.parse::<f64>().unwrap();
                        eprintln!("FLOAT_CONST {} at Line {}.", val, line);
                    }
                    Rule::EOI => break,
                    _ => {
                        eprintln!("{:?} {} at Line {}.", rule, text, line);
                    }
                }
            }
        }
        Err(e) => {
            let line = match e.line_col {
                pest::error::LineColLocation::Pos((l, _)) => l,
                pest::error::LineColLocation::Span((l, _), _) => l,
            };
            eprintln!("Error type A at Line {}: {}", line, "Invalid token");
        }
    }
}
