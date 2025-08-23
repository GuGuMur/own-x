from collections import defaultdict
import subprocess
import click
import sys
from pathlib import Path
from loguru import logger

from . import base, data, utils, diff


@click.group()
def cli():
    pass


@cli.command("init")
def init():
    """Initialize command"""
    git_path = Path.cwd() / data.GIT_DIR
    base.init(git_path)
    logger.success(f"Initialized empty ugit repository in {git_path.resolve()}")


@cli.command("hash-object")
@click.argument("file")
def hash_object(file: str):
    """Echo the sha1 hash of a file"""
    click.echo(data.hash_object(Path(file).read_bytes()))


@cli.command("cat-file")
@click.argument("object_id")
def cat_file(object_id: str):
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
@click.option("-m", "--message", required=True, help="Commit message")
def commit(message: str = "default commit message"):
    """Create a commit"""
    click.echo(base.commit(message))


@cli.command("log")
@click.argument("oid", required=False, default="@")
def log(oid: str = "@"):
    """Display history of commits"""
    refs: defaultdict[str, list[str]] = defaultdict(list)
    for refname, ref in data.iter_refs():
        refs[ref.value].append(refname)
    oid = oid or data.get_ref(ref="HEAD")
    for oid in base.iter_commits_and_parents({oid}):
        commit = base.get_commit(oid)
        utils._print_commit(oid, commit, refs.get(oid))
        # oid = commit.parent


@cli.command("checkout")
@click.argument("identifier")
def checkout(identifier: str):
    """Checkout to a commit or a branch"""
    oid = base.get_oid(identifier)
    commit = base.get_commit(oid)
    utils.read_tree(commit.tree)

    if base.is_branch(identifier):
        HEAD = data.RefValue(symbolic=True, value=f"refs/heads/{identifier}")
    else:
        HEAD = data.RefValue(symbolic=False, value=oid)

    data.update_ref("HEAD", HEAD, deref=False)


@cli.command("tag")
@click.argument("name")
@click.argument("oid", required=False, default="@")
def tag(name: str, oid: str = "@"):
    """Create a tag"""
    # oid = oid or data.get_ref(ref="HEAD")
    # if not oid:
    # logger.error("No commit found. Cannot create tag.")
    # sys.exit(1)
    base.create_tag(name, oid)
    logger.success(f"Tag {name} created for commit {oid}")


@cli.command("k")
def k():
    """Display commit history as a graph"""
    dot = "digraph commits {\n"

    oids = set()
    for refname, ref in data.iter_refs(deref=False):
        dot += f'"{refname}" [shape=note]\n'
        dot += f'"{refname}" -> "{ref.value}"\n'
        if not ref.symbolic:
            oids.add(ref.value)

    for oid in base.iter_commits_and_parents(oids):
        commit = base.get_commit(oid)
        dot += f'"{oid}" [shape=box style=filled label="{oid[:10]}"]\n'
        if commit.parent:
            dot += f'"{oid}" -> "{commit.parent}"\n'

    dot += "}"
    print(dot)

    with subprocess.Popen(
        ["dot", "-Tgtk", "/dev/stdin"], stdin=subprocess.PIPE
    ) as proc:
        proc.communicate(dot.encode())


@cli.command("branch")
@click.argument("name", required=False)
@click.argument("start_point", default="HEAD")
def branch(name: str, start_point: str = "@"):
    """List or create branch"""
    if not name:
        current = base.get_branch_name()
        for branch_name in base.iter_branch_names():
            prefix = "*" if branch_name == current else " "
            click.echo(f"{prefix} {branch_name}")
    else:
        base.create_branch(name, start_point)
        click.echo(f"Branch '{name}' created at {start_point[:10]}")


@cli.command("status")
def status():
    """Show the working tree status"""
    HEAD = base.get_oid("@")
    branch = base.get_branch_name()
    if branch:
        print(f"On branch {branch}")
    else:
        print(f"HEAD detached at {HEAD[:10]}")

    print("\nChanges to be committed:\n")
    HEAD_tree = HEAD and base.get_commit(HEAD).tree
    for path, action in diff.iter_changed_files(
        utils.get_tree(HEAD_tree), base.get_working_tree()
    ):
        print(f"{action:>12}: {path}")

@cli.command("reset")
@click.argument("commmit")
def reset(commmit: str):
    """Reset current branch to a specific commit"""
    base.reset(commmit)
    click.echo(f"HEAD reset to {commmit}")


@cli.command("show")
@click.argument("oid", required=False, default="@")
def show(oid: str = "@"):
    """Show commit details"""
    commit = base.get_commit(oid)
    parent_tree = None
    if commit.parent:
        parent_tree = base.get_commit(commit.parent).tree
    utils._print_commit(oid, commit)
    result = diff.diff_trees(utils.get_tree(parent_tree), utils.get_tree(commit.tree))
    # print(result)
    sys.stdout.flush()
    sys.stdout.buffer.write(result)


@cli.command("diff")
@click.argument("commit", required=False)
def diff_cmd(commit: str = None):
    """Show changes between commit and working directory"""
    tree = commit and base.get_commit(commit).tree

    result = diff.diff_trees(utils.get_tree(tree), base.get_working_tree())
    sys.stdout.flush()
    sys.stdout.buffer.write(result)

@cli.command("merge")
@click.argument("commit", required=True)
def merge_cmd(commit: str):
    """Merge a branch into the current branch"""
    base.merge(commit)
    click.echo(f"Merged branch '{commit}' into current branch")