"""Package the canonical Release build; never publish or run the product (#413)."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent
ENTRIES = ("NeNeNib.exe", "README.txt", "LICENSE", "THIRD_PARTY_NOTICES.txt")
REMOTE = "https://github.com/hideyukiMORI/nene-nib.git"
OUTPUT_ROOTS = (Path("D:/NeNeNib/outputs"), Path("D:/NeNeNib/releases"))


def run(arguments: list[str], root: Path = ROOT) -> str:
    result = subprocess.run(arguments, cwd=root, check=True, capture_output=True,
                            text=True, encoding="utf-8", errors="strict")
    return result.stdout.strip()


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def version_of(cmake: str) -> str:
    matches = re.findall(r"^project\(NeNeNib VERSION (\d+\.\d+\.\d+) LANGUAGES CXX\)$",
                         cmake, re.MULTILINE)
    if len(matches) != 1:
        raise ValueError("CMake must contain one canonical product VERSION")
    return matches[0]


def validate_output(path: Path, roots: tuple[Path, ...] = OUTPUT_ROOTS) -> Path:
    absolute = Path(os.path.abspath(path))
    if not any(absolute.is_relative_to(root.absolute()) and absolute != root.absolute()
               for root in roots):
        raise ValueError("Output must be a new directory below D:/NeNeNib/outputs or releases")
    for ancestor in (absolute, *absolute.parents):
        if ancestor.is_symlink():
            raise ValueError(f"Output ancestry contains a link: {ancestor}")
        if ancestor.exists():
            attributes = getattr(ancestor.lstat(), "st_file_attributes", 0)
            if attributes & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0):
                raise ValueError(f"Output ancestry contains a reparse point: {ancestor}")
    if absolute.exists():
        raise ValueError(f"Output already exists: {absolute}")
    return absolute


def platform_dlls(architecture: dict) -> set[str]:
    if architecture["runtimeDependencies"]:
        raise ValueError("This package path requires no non-OS runtime dependencies")
    pending = ["app"]
    seen: set[str] = set()
    libraries: set[str] = set()
    while pending:
        module = pending.pop()
        if module in seen:
            continue
        seen.add(module)
        pending.extend(architecture["modules"][module]["dependencies"])
        libraries.update(architecture["platformLibraries"].get(module, []))
    return {name.lower() + ".dll" for name in libraries}


def validate_pe(report: str, metadata: dict, version: str, allowed: set[str]) -> list[str]:
    if "Machine: IMAGE_FILE_MACHINE_AMD64 (0x8664)" not in report:
        raise ValueError("The executable is not Windows x64")
    imports = sorted({name.lower() for name in re.findall(r"^  Name: (\S+\.dll)$", report,
                                                         re.MULTILINE | re.IGNORECASE)})
    if not imports or set(imports) - allowed:
        raise ValueError(f"Missing or undeclared dynamic imports: {set(imports) - allowed}")
    if metadata["fileVersion"] != version + ".0" or metadata["productVersion"] != version:
        raise ValueError("Embedded product/file VERSION differs from CMake")
    return imports


def verify_zip(archive: Path, contents: dict[str, bytes], extracted: Path) -> None:
    with zipfile.ZipFile(archive) as bundle:
        if sorted(bundle.namelist()) != sorted(ENTRIES):
            raise ValueError("ZIP must contain exactly the three declared root files")
        extracted.mkdir()
        for name in ENTRIES:
            data = bundle.read(name)
            if data != contents[name]:
                raise ValueError(f"ZIP bytes differ for {name}")
            (extracted / name).write_bytes(data)
    if any((extracted / name).read_bytes() != contents[name] for name in ENTRIES):
        raise ValueError("Extracted bytes differ")


def source_state(root: Path, preview: bool) -> tuple[str, str]:
    if run(["git", "status", "--porcelain", "--untracked-files=all"], root):
        raise ValueError("Commit all source changes before packaging")
    commit = run(["git", "rev-parse", "HEAD"], root)
    branch = run(["git", "branch", "--show-current"], root)
    if not preview:
        remote = run(["git", "ls-remote", "--exit-code", REMOTE, "refs/heads/main"], root)
        if branch not in ("main", "") or remote.split()[0] != commit:
            raise ValueError("Publication package requires clean main at current origin/main")
    return commit, branch


def verification_record(root: Path, name: str) -> dict:
    relative = Path(name)
    if relative.is_absolute() or ".." in relative.parts:
        raise ValueError("Verification must name a tracked, repo-relative record")
    run(["git", "ls-files", "--error-unmatch", "--", relative.as_posix()], root)
    path = root / relative
    if path.is_symlink() or not path.resolve().is_relative_to(root.resolve()):
        raise ValueError("Verification record must reside in the source tree")
    return {"path": relative.as_posix(), "sha256": digest(path.read_bytes())}


def package(args: argparse.Namespace) -> Path:
    output = validate_output(args.output)
    commit, branch = source_state(ROOT, args.preview)
    verification = verification_record(ROOT, args.verification)
    version = version_of((ROOT / "CMakeLists.txt").read_text(encoding="utf-8"))
    release_version = f"{version}-beta.{args.beta}"
    short = run(["git", "rev-parse", "--short=7", commit])
    build = ROOT / "build" / f"release-{short}"
    if build.exists():
        raise ValueError(f"Clean build required; keep existing evidence and use a clean tree: {build}")
    architecture = json.loads((ROOT / "eng/architecture.json").read_text(encoding="utf-8"))
    allowed = platform_dlls(architecture)
    output.mkdir(parents=True)
    with (output / "build.log").open("w", encoding="utf-8") as log:
        subprocess.run(["pwsh", "-NoProfile", "-File", str(ROOT / "eng/build-release.ps1"),
                        "-Ref", "HEAD"], cwd=ROOT, check=True, stdout=log,
                       stderr=subprocess.STDOUT)
    if source_state(ROOT, args.preview) != (commit, branch):
        raise ValueError("Source changed during the build")
    release = json.loads((ROOT / "out/release" / f"{short}.json").read_text(encoding="utf-8-sig"))
    executable = build / "NeNeNib.exe"
    binary = executable.read_bytes()
    if release["commit"] != commit or release["head"] != commit or release["sha256"].lower() != digest(binary):
        raise ValueError("Release build record does not identify this commit and executable")
    report = run(["llvm-readobj", "--file-headers", "--coff-imports", str(executable)])
    (output / "pe-inspection.txt").write_text(report + "\n", encoding="utf-8")
    metadata = json.loads(run(["pwsh", "-NoProfile", "-File",
                              str(ROOT / "eng/release/inspect-executable.ps1"), str(executable)]))
    imports = validate_pe(report, metadata, version, allowed)
    readme = (ROOT / "eng/release/README.txt.in").read_text(encoding="utf-8")
    contents = {"NeNeNib.exe": binary,
                "README.txt": readme.replace("@VERSION@", release_version).encode("utf-8"),
                "LICENSE": (ROOT / "LICENSE").read_bytes(),
                "THIRD_PARTY_NOTICES.txt": (ROOT / "THIRD_PARTY_NOTICES.txt").read_bytes()}
    archive = output / f"NeNeNib-{release_version}-windows-x64.zip"
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for name in ENTRIES:
            bundle.writestr(name, contents[name])
    verify_zip(archive, contents, output / "extracted")
    archive_hash = digest(archive.read_bytes())
    (output / "SHA256SUMS").write_text(f"{archive_hash}  {archive.name}\n", encoding="ascii")
    record = {"schema": 1, "commit": commit, "branch": branch, "preview": args.preview,
              "version": release_version, "productVersion": version,
              "verification": verification, "release": release, "metadata": metadata,
              "imports": imports, "declaredImports": sorted(allowed),
              "archive": {"name": archive.name, "sha256": archive_hash,
                          "bytes": archive.stat().st_size},
              "contents": {name: {"sha256": digest(data), "bytes": len(data)}
                           for name, data in contents.items()},
              "extractedBytesMatch": True, "launched": False, "published": False}
    (output / "package.json").write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n",
                                          encoding="utf-8")
    return archive


def positive(value: str) -> int:
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("beta number must be positive")
    return number


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--beta", type=positive, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--verification", required=True)
    parser.add_argument("--preview", action="store_true")
    args = parser.parse_args()
    print(package(args))


if __name__ == "__main__":
    main()
