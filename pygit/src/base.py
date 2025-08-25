from pathlib import Path
from . import data, utils
import os
import itertools
import operator
from typing import Any, Dict, Generator, NoReturn
import string
from collections import defaultdict, deque


def init(dir: str = data.GIT_DIR) -> NoReturn:
    os.makedirs(dir, exist_ok=True)
    os.makedirs(f"{dir}/objects")
    data.update_ref("HEAD", data.RefValue(symbolic=True, value="refs/heads/master"))


def write_tree():
    # Index is flat, we need it as a tree of dicts
    index_as_tree = {}
    with data.get_index() as index:
        for path, oid in index.items():
            path = path.split("/")
            dirpath, filename = path[:-1], path[-1]

            current = index_as_tree
            # Find the dict for the directory of this file
            for dirname in dirpath:
                current = current.setdefault(dirname, {})
            current[filename] = oid

    def write_tree_recursive(tree_dict):
        entries = []
        for name, value in tree_dict.items():
            if type(value) is dict:
                type_ = "tree"
                oid = write_tree_recursive(value)
            else:
                type_ = "blob"
                oid = value
            entries.append((name, oid, type_))

        tree = "".join(
            f"{type_} {oid} {name}\n" for name, oid, type_ in sorted(entries)
        )
        return data.hash_object(tree.encode(), "tree")

    return write_tree_recursive(index_as_tree)


def commit(message: str) -> str:
    commit = f"tree {write_tree()}\n"

    HEAD = data.get_ref(ref="HEAD").value
    if HEAD:
        commit += f"parent {HEAD}\n"
    MERGE_HEAD = data.get_ref("MERGE_HEAD").value
    if MERGE_HEAD:
        commit += f"parent {MERGE_HEAD}\n"
        data.delete_ref("MERGE_HEAD", deref=False)
    commit += "\n"
    commit += f"{message}\n"

    oid = data.hash_object(commit.encode(), "commit")

    data.update_ref("HEAD", data.RefValue(symbolic=False, value=oid))

    return oid


def get_commit(oid: str) -> data.CommitType:
    parents = []
    commit = data.get_object(oid, "commit").decode()
    lines = iter(commit.splitlines())
    for line in itertools.takewhile(operator.truth, lines):
        key, value = line.split(" ", 1)
        if key == "tree":
            tree = value
        elif key == "parent":
            parents.append(value)
        else:
            assert False, f"Unknown field {key}"

    message = "\n".join(lines)
    return data.COMMIT(tree=tree, parents=parents, message=message)


def checkout(oid: str):
    commit = get_commit(oid)
    utils.read_tree(commit.tree, True)
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
        oids.extendleft(commit.parents[:1])
        oids.extend(commit.parents[1:])

def iter_objects_in_commits(oids) -> Generator[str | Any, Any, None]:
    # N.B. Must yield the oid before acccessing it (to allow caller to fetch it
    # if needed)

    visited = set()

    def iter_objects_in_tree(oid) -> Generator[Any | str, Any, None]:
        visited.add(oid)
        yield oid
        for type_, oid, _ in utils._iter_tree_entries(oid):
            if oid not in visited:
                if type_ == "tree":
                    yield from iter_objects_in_tree(oid)
                else:
                    visited.add(oid)
                    yield oid

    for oid in iter_commits_and_parents(oids):
        yield oid
        commit = get_commit(oid)
        if commit.tree not in visited:
            yield from iter_objects_in_tree(commit.tree)

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
    for refname, _ in data.iter_refs(prefix="refs/heads/"):
        # yield os.path.relpath(refname, "refs/heads/")
        yield Path(refname).name


def reset(oid: str):
    data.update_ref("HEAD", data.RefValue(symbolic=False, value=oid))


def get_working_tree() -> Dict[str, bytes]:
    result = defaultdict(bytes)

    for file_path in Path(".").rglob("*"):
        if file_path.is_file(follow_symlinks=False) and not utils.is_ignored(file_path):
            result[str(file_path)] = data.hash_object(file_path.read_bytes())

    return result


def merge(other: str):
    HEAD = data.get_ref("HEAD").value
    assert HEAD
    merge_base = get_merge_base(other, HEAD)
    c_other = get_commit(other)

    if merge_base == HEAD:
        utils.read_tree(c_other.tree, True)
        data.update_ref("HEAD", data.RefValue(symbolic=False, value=other))
        print("Fast-forward merge, no need to commit")
        return

    data.update_ref("MERGE_HEAD", data.RefValue(symbolic=False, value=other))

    c_base = get_commit(merge_base)
    c_HEAD = get_commit(HEAD)
    utils.read_tree_merged(c_base.tree, c_HEAD.tree, c_other.tree, True)
    print("Merged in working tree\nPlease commit")



def delete_ref(ref: str, deref: bool = True) -> NoReturn:
    ref_path_str = data._get_ref_internal(ref, deref)[0]
    ref_path = Path(data.GIT_DIR) / ref_path_str
    ref_path.unlink()

def get_merge_base(oid1: str, oid2: str) -> str:
    parents1 = set(iter_commits_and_parents({oid1}))

    for oid in iter_commits_and_parents({oid2}):
        if oid in parents1:
            return oid


def is_ancestor_of(commit, maybe_ancestor):
    return maybe_ancestor in iter_commits_and_parents({commit})

def add(filenames):
    def add_file(filename):
        # Normalize path
        filename = os.path.relpath(filename)
        with open(filename, "rb") as f:
            oid = data.hash_object(f.read())
        index[filename] = oid

    def add_directory(dirname):
        for root, _, filenames in os.walk(dirname):
            for filename in filenames:
                # Normalize path
                path = os.path.relpath(f"{root}/{filename}")
                if utils.is_ignored(path) or not os.path.isfile(path):
                    continue
                add_file(path)

    with data.get_index() as index:
        for name in filenames:
            if os.path.isfile(name):
                add_file(name)
            elif os.path.isdir(name):
                add_directory(name)

def get_index_tree():
    with data.get_index() as index:
        return index
