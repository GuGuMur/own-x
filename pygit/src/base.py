from pathlib import Path
from . import data
from . import utils
import os
import itertools
import operator
from typing import NoReturn


def init(dir: str = data.GIT_DIR) -> NoReturn:
    os.makedirs(dir, exist_ok=True)
    os.makedirs("dir/objects")


def write_tree(dir_path: str = data.GIT_DIR):
    """Internal recursive function for writing trees"""
    dir_path = Path(dir_path)
    entries = []

    for entry in dir_path.iterdir():
        if utils.is_ignored(entry):
            continue

        if entry.is_file(follow_symlinks=False):
            type_ = "blob"
            oid = data.hash_object(entry.read_bytes())
        elif entry.is_dir(follow_symlinks=False):
            type_ = "tree"
            oid = write_tree(entry)
        else:
            continue

        entries.append((entry.name, oid, type_))

    tree = "".join(f"{type_} {oid} {name}\n" for name, oid, type_ in sorted(entries))
    return data.hash_object(tree.encode(), "tree")


def commit(message):
    commit = f"tree {write_tree()}\n"

    HEAD = data.get_HEAD()
    if HEAD:
        commit += f"parent {HEAD}\n"

    commit += "\n"
    commit += f"{message}\n"

    oid = data.hash_object(commit.encode(), "commit")

    data.set_HEAD(oid)

    return oid


def get_commit(oid):
    parent = None

    commit = data.get_object(oid, "commit").decode()
    lines = iter(commit.splitlines())
    for line in itertools.takewhile(operator.truth, lines):
        key, value = line.split(" ", 1)
        if key == "tree":
            tree = value
        elif key == "parent":
            parent = value
        else:
            assert False, f"Unknown field {key}"

    message = "\n".join(lines)
    return data.COMMIT(tree=tree, parent=parent, message=message)


def checkout(oid):
    commit = get_commit(oid)
    utils.read_tree(commit.tree)
    data.set_HEAD(oid)

def create_tag(name, oid):
    # TODO Actually create the tag
    pass
