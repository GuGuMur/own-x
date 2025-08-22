import hashlib
from pathlib import Path
from collections import namedtuple

GIT_DIR = ".ugit"

COMMIT = namedtuple("Commit", ["tree", "parent", "message"])



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


def set_HEAD(oid) -> None:
    head_path = Path(GIT_DIR) / "HEAD"
    head_path.write_text(oid)


def get_HEAD() -> str | None:
    head_path = Path(GIT_DIR) / "HEAD"
    if head_path.is_file():
        return head_path.read_text().strip()
    return None
