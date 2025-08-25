import os
from pathlib import Path
import textwrap
from typing import Any, Generator

from . import data, diff


def is_ignored(path):
    """Check if path should be ignored"""
    path_obj = Path(path)
    return ".ugit" in path_obj.parts


def _iter_tree_entries(oid) -> Generator[tuple[str, str, str], Any, None]:
    if not oid:
        return
    tree = data.get_object(oid, "tree")
    for entry in tree.decode().splitlines():
        type_, oid, name = entry.split(" ", 2)
        yield type_, oid, name


def get_tree(oid, base_path="") -> dict:
    result = {}
    for type_, oid, name in _iter_tree_entries(oid):
        assert "/" not in name
        assert name not in ("..", ".")
        path = base_path + name
        if type_ == "blob":
            result[path] = oid
        elif type_ == "tree":
            result.update(get_tree(oid, f"{path}/"))
        else:
            assert False, f"Unknown tree entry {type_}"
    return result


def read_tree(tree_oid, update_working=False):
    with data.get_index() as index:
        index.clear()
        index.update(get_tree(tree_oid))

        if update_working:
            _checkout_index(index)


def read_tree_merged(t_base, t_HEAD, t_other, update_working=False):
    with data.get_index() as index:
        index.clear()
        index.update(
            diff.merge_trees(get_tree(t_base), get_tree(t_HEAD), get_tree(t_other))
        )

        if update_working:
            _checkout_index(index)

def _checkout_index(index):
    _empty_current_directory()
    for path, oid in index.items():
        os.makedirs(os.path.dirname(f"./{path}"), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data.get_object(oid, "blob"))

def _empty_current_directory():
    for path in sorted(Path(".").rglob("*"), key=lambda p: len(p.parts), reverse=True):
        if is_ignored(path):
            continue

        if path.is_file(follow_symlinks=False):
            if not is_ignored(path):
                path.unlink()
        elif path.is_dir(follow_symlinks=False):
            try:
                path.rmdir()
            except (OSError, FileNotFoundError):
                pass


def _print_commit(oid: str, commit: str, refs: str = None):
    refs_str = f" ({', '.join(refs)})" if refs else ""
    print(f"commit {oid}{refs_str}\n")
    print(textwrap.indent(commit.message, "    "))
    print("")
