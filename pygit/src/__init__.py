import textwrap
import click
import sys
from pathlib import Path
from loguru import logger
# from .utils import 
from . import base, data, utils


@click.group()
def cli():
    pass


@cli.command("init")
def init():
    """Initialize command"""
    git_path = Path.cwd() / data.GIT_DIR
    base.init(git_path)
    logger.success(f"Initialized empty ugit repository in {git_path.resolve()}")


@cli.command("hash")
@click.argument("file")
def do_hash(file):
    """Echo the sha1 hash of a file"""
    # with open(file, "rb") as f:
    #     click.echo(data.hash_object(f.read()))
    click.echo(data.hash_object(Path(file).read_bytes()))


@cli.command("cat-file")
@click.argument("object_id")
def cat_file(object_id):
    """Show contents of a git object"""
    content = data.get_object(object_id, expected=None)
    if isinstance(content, bytes):
        sys.stdout.buffer.write(content)
        sys.stdout.buffer.flush()
    else:
        click.echo(content, nl=False)


@cli.command("write-tree")
@click.argument("directory", default=".")
def write_tree(directory="."):
    """Write a tree object from the current directory"""
    result = base.write_tree(directory)
    click.echo(result)

@cli.command("commit")
@click.argument("message", default="default commit message")
def commit(message: str = "default commit message"):
    """Create a commit"""
    click.echo(base.commit(message))

@cli.command("log")
@click.argument("oid", required=False)
def log(oid: str = None):
    """Display history of commits"""
    oid = oid or data.get_HEAD()
    while oid:
        commit = base.get_commit(oid)
        click.echo(f"commit {oid}\n")
        click.echo(textwrap.indent(commit.message, "    "))
        click.echo("")

        oid = commit.parent

@cli.command("checkout")
@click.argument("oid")
def checkout(oid: str):
    """Checkout a commit"""
    base.checkout(oid)
    logger.success(f"HEAD is now at {oid}")

@cli.command("tag")
@click.argument("name")
@click.argument("oid", required=False)
def tag(name: str, oid: str = None):
    """Create a tag"""
    oid = oid or data.get_HEAD()
    if not oid:
        logger.error("No commit found. Cannot create tag.")
        sys.exit(1)
    base.create_tag(name, oid)
    logger.success(f"Tag {name} created for commit {oid}")

if __name__ == "__main__":
    cli()
