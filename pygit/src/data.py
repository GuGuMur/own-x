import hashlib
from pathlib import Path
import ujson as json
import shutil
from collections import namedtuple
from typing import Any, Generator, Iterator, Tuple, TypeAlias
from contextlib import contextmanager

# GIT_DIR = ".ugit"
GIT_DIR = None

@contextmanager
def change_git_dir(new_dir: str = ".") -> Generator[None, Any, None]:
    global GIT_DIR
    old_dir = GIT_DIR
    GIT_DIR = f"{new_dir}/.ugit"
    yield
    GIT_DIR = old_dir

COMMIT = namedtuple("Commit", ["tree", "parents", "message"])
CommitType: TypeAlias = COMMIT

RefValue = namedtuple("RefValue", ["symbolic", "value"])
RefValueType: TypeAlias = RefValue


def hash_object(data, type_="blob"):
    obj: bytes = type_.encode() + b"\x00" + data
    oid: str = hashlib.sha1(obj).hexdigest()
    object_path = Path(GIT_DIR) / "objects" / oid
    object_path.parent.mkdir(parents=True, exist_ok=True)
    object_path.write_bytes(obj)
    return oid


def get_object(oid, expected="blob"):
    obj = Path(f"{GIT_DIR}/objects/{oid}").read_bytes()
    type_, _, content = obj.partition(b"\x00")
    type_ = type_.decode()

    if expected is not None:
        assert type_ == expected, f"Expected {expected}, got {type_}"
    return content


def update_ref(
    ref: str = "HEAD", value: RefValueType = RefValue(False, None), deref: bool = True
) -> None:
    ref = _get_ref_internal(ref, deref=deref)[0]
    assert value.value
    if value.symbolic:
        value = f"ref: {value.value}"
    else:
        value = value.value
    head_path = Path(GIT_DIR) / ref
    head_path.parent.mkdir(parents=True, exist_ok=True)
    head_path.write_text(value)


def get_ref(ref: str = "HEAD", deref: bool = True) -> RefValueType:
    return _get_ref_internal(ref, deref=deref)[1]


def _get_ref_internal(ref: str, deref: bool = True) -> Tuple[str, RefValueType]:
    ref_path = Path(GIT_DIR) / ref
    value = None
    if ref_path.is_file():
        value = ref_path.read_text().strip()

    symbolic = bool(value) and value.startswith("ref:")
    if symbolic:
        value = value.split(":", 1)[1].strip()
        if deref:
            return _get_ref_internal(value, deref=True)

    return ref, RefValue(symbolic=symbolic, value=value)


def iter_refs(
    prefix: str = "", deref: bool = True
) -> Iterator[Tuple[str, RefValueType]]:
    refs = ["HEAD", "MERGE_HEAD"]
    git_dir_path = Path(GIT_DIR)

    refs_dir = git_dir_path / "refs"
    if refs_dir.exists():
        for ref_file in refs_dir.rglob("*"):
            if ref_file.is_file():
                # 获取相对于 GIT_DIR 的路径
                refname = str(ref_file.relative_to(git_dir_path))
                refs.append(refname)

    for refname in refs:
        if not refname.startswith(prefix):
            continue

        ref = get_ref(refname, deref=deref)
        if ref.value:
            yield refname, ref


def object_exists(oid):
    return Path(f"{GIT_DIR}/objects/{oid}").is_file()


def fetch_object_if_missing(oid, remote_git_dir):
    if object_exists(oid):
        return
    remote_git_dir += "/.ugit"
    shutil.copy(f"{remote_git_dir}/objects/{oid}", f"{GIT_DIR}/objects/{oid}")


def push_object(oid, remote_git_dir):
    remote_git_dir += "/.ugit"
    shutil.copy(f"{GIT_DIR}/objects/{oid}", f"{remote_git_dir}/objects/{oid}")

@contextmanager
def get_index():
    index = {}
    entry = Path(f"{GIT_DIR}/index")
    if entry.is_file():
        index = json.laod(entry.read_text())
    yield index

    entry.write_text(json.dumps(index))
