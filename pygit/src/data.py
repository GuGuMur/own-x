import hashlib
from pathlib import Path
from collections import namedtuple
from typing import Iterator, Tuple, TypeAlias

GIT_DIR = ".ugit"

COMMIT = namedtuple("Commit", ["tree", "parent", "message"])
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
        value = f'ref: {value.value}'
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
    if value and value.startswith("ref:"):
        # ref: <refname> -> refname
        return get_ref(value.split(":", 1)[1].strip())

    symbolic = bool(value) and value.startswith("ref:")
    if symbolic:
        value = value.split(":", 1)[1].strip()
        if deref:
            return _get_ref_internal(value, deref=deref)

    return ref, RefValue(symbolic=symbolic, value=value)


def iter_refs(deref: bool = True) -> Iterator[Tuple[str, RefValueType]]:
    # Fisrt yield HEAD
    yield "HEAD", get_ref("HEAD")
    # Then yield all refs in refs/
    refs_dir = Path(GIT_DIR) / "refs"
    if refs_dir.exists():
        for ref_file in refs_dir.rglob("*"):
            if ref_file.is_file():
                refname = str(ref_file.relative_to(Path(GIT_DIR)))
                yield refname, get_ref(refname, deref=deref)
