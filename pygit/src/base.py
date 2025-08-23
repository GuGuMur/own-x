from pathlib import Path
from . import data
from . import utils
import os
import itertools
import operator
from typing import Any, Dict, Generator, NoReturn
import string
from collections import defaultdict, deque


def init(dir: str = data.GIT_DIR) -> NoReturn:
    os.makedirs(dir, exist_ok=True)
    os.makedirs("dir/objects")
    data.update_ref("HEAD", data.RefValue(symbolic=True, value="refs/heads/master"))


def write_tree(dir_path: str = data.GIT_DIR) -> str:
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


def commit(message: str) -> str:
    commit = f"tree {write_tree()}\n"

    HEAD = data.get_ref(ref="HEAD").value
    if HEAD:
        commit += f"parent {HEAD}\n"

    commit += "\n"
    commit += f"{message}\n"

    oid = data.hash_object(commit.encode(), "commit")

    data.update_ref("HEAD", data.RefValue(symbolic=False, value=oid))

    return oid


def get_commit(oid: str) -> data.CommitType:
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


def checkout(oid: str):
    commit = get_commit(oid)
    utils.read_tree(commit.tree)
    data.update_ref("HEAD", data.RefValue(symbolic=False, value=oid))


def create_tag(name: str, oid: str):
    data.update_ref(f"refs/tags/{name}", data.RefValue(symbolic=False, value=oid))


def get_oid(name: str):
    """Get OID for a name (branch, tag, or SHA1)"""
    if name == "@":
        name = "HEAD"
    # As ref
    for ref in [name, f"refs/{name}", f"refs/tags/{name}", f"refs/heads/{name}"]:
        if oid := data.get_ref(ref, deref=False).value:
            return oid

    # As SHA1
    if len(name) == 40 and all(c in string.hexdigits for c in name):
        return name

    raise ValueError(f"Unknown name: {name}")


def iter_commits_and_parents(oids) -> Generator[str, None, None]:
    oids = deque(oids)
    visited = set()

    while oids:
        oid = oids.popleft()
        if not oid or oid in visited:
            continue
        visited.add(oid)
        yield oid

        commit = get_commit(oid)
        oids.appendleft(commit.parent)


def create_branch(name: str, oid: str):
    data.update_ref(f"refs/heads/{name}", data.RefValue(symbolic=False, value=oid))


def is_branch(branch: str) -> bool:
    return data.get_ref(f"refs/heads/{branch}").value is not None


def get_branch_name():
    HEAD = data.get_ref("HEAD", deref=False)
    if not HEAD.symbolic:
        return None
    if not HEAD.value.startswith("refs/heads/"):
        return None

    branch_path = Path(HEAD.value)
    return branch_path.name

def iter_branch_names() -> Generator[str, Any, None]:
    for refname, _ in data.iter_refs("refs/heads/"):
        # yield os.path.relpath(refname, "refs/heads/")
        yield Path(refname).name

def reset(oid: str):
    data.update_ref("HEAD", data.RefValue(symbolic=False, value=oid))

def get_working_tree() -> Dict[str, bytes]:
    result = defaultdict(bytes)

    for file_path in Path(".").rglob("*"):
        if (
            file_path.is_file(follow_symlinks=False)
            and not utils.is_ignored(file_path)
        ):
            result[str(file_path)] = data.hash_object(file_path.read_bytes())

    return result

def merge(other):
    # TODO merge HEAD into other
    pass
