"""Wrap the canonical Release exe in an unsigned MSIX for the Microsoft Store (#1581 / ADR 0186).

The input is the exe that eng/build-release.ps1 built for HEAD (the same file eng/package-release.py
puts in the ZIP). This tool never builds, signs, installs or launches anything:

  python eng/package-msix.py --output <new dir> [--zip <release zip>] [--store]

The identity, the display name, the languages and the pinned Windows SDK come from one table,
eng/store-identity.json. The package version is project(NeNeNib VERSION x.y.z) as x.y.z.0
(CNF-007); --store refuses a version whose first number is 0. The 25 logos are the ones
eng/render-app-icon.py generated (eng/app-icon.json). After MakeAppx packs, the tool reads the
archive and MakeAppx unpacks it again: the file set must be exactly the expected one, the exe bytes
must be the input exe's (and the ZIP's when --zip is given), and the manifest must carry the table's
identity and the CMake version. The record is msix.json in the output directory.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parent.parent
IDENTITY = "eng/store-identity.json"
LOGO_COUNT = 25
EXECUTABLE = "NeNeNib.exe"
NOTICES = ("LICENSE", "THIRD_PARTY_NOTICES.txt")
GENERATED = ("AppxManifest.xml", "resources.pri")
FOOTPRINTS = ("AppxBlockMap.xml", "[Content_Types].xml")
# MakeAppx unpack writes the block map back but not [Content_Types].xml (measured with SDK 26100).
UNPACKED_FOOTPRINTS = ("AppxBlockMap.xml",)
OUTPUT_ROOTS = (Path("D:/NeNeNib/outputs"), Path("D:/NeNeNib/releases"), ROOT / "out")
IDENTITY_KEYS = {"schema", "identityName", "publisher", "publisherDisplayName", "packageFamilyName",
                 "storeId", "displayName", "description", "languages", "minVersion",
                 "maxVersionTested", "windowsPackagingSdk", "makeappx", "makepri"}
FOUNDATION = "http://schemas.microsoft.com/appx/manifest/foundation/windows10"
NAMESPACES = {"": FOUNDATION,
              "uap": "http://schemas.microsoft.com/appx/manifest/uap/windows10",
              "rescap": "http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"}
PUBLISHER_ALPHABET = "0123456789abcdefghjkmnpqrstvwxyz"
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def load_module(name: str, filename: str):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


RELEASE = load_module("package_release", "package-release.py")
ICON = load_module("render_app_icon", "render-app-icon.py")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# --- the identity table -----------------------------------------------------------------------


def publisher_hash(publisher: str) -> str:
    """The 13 characters after '_' in a package family name (SHA-256 of UTF-16LE, 65 bits, base 32)."""
    number = int.from_bytes(hashlib.sha256(publisher.encode("utf-16-le")).digest()[:8], "big") << 1
    return "".join(PUBLISHER_ALPHABET[(number >> (5 * (12 - index))) & 31] for index in range(13))


def windows_version(text: str) -> tuple[int, ...]:
    if not re.fullmatch(r"\d+\.\d+\.\d+\.\d+", text):
        raise ValueError(f"not a four-part Windows version: {text}")
    numbers = tuple(int(part) for part in text.split("."))
    if any(number > 65535 for number in numbers):
        raise ValueError(f"a Windows version part exceeds 65535: {text}")
    return numbers


def load_identity(table: dict) -> dict:
    if set(table) != IDENTITY_KEYS or table["schema"] != 1:
        raise ValueError(f"{IDENTITY} must hold exactly the schema-1 keys")
    for key in IDENTITY_KEYS - {"schema", "languages"}:
        if not isinstance(table[key], str) or not table[key].strip() or table[key] != table[key].strip():
            raise ValueError(f"{IDENTITY}: {key} must be a non-empty trimmed string")
    if not re.fullmatch(r"[A-Za-z0-9.-]{3,50}", table["identityName"]):
        raise ValueError(f"{IDENTITY}: identityName is not a package name")
    if not table["publisher"].startswith("CN="):
        raise ValueError(f"{IDENTITY}: publisher must be a distinguished name")
    family = f"{table['identityName']}_{publisher_hash(table['publisher'])}"
    if table["packageFamilyName"] != family:
        raise ValueError(f"{IDENTITY}: name and publisher give {family}, "
                         f"not the reserved {table['packageFamilyName']}")
    if not re.fullmatch(r"9[0-9A-Z]{11}", table["storeId"]):
        raise ValueError(f"{IDENTITY}: storeId is not a Store ID")
    languages = table["languages"]
    if (not isinstance(languages, list) or not languages or len(set(languages)) != len(languages)
            or not all(isinstance(item, str) and re.fullmatch(r"[a-z]{2}(-[A-Za-z]{2,4})+", item)
                       for item in languages)):
        raise ValueError(f"{IDENTITY}: languages must be distinct BCP-47 tags")
    if windows_version(table["minVersion"]) > windows_version(table["maxVersionTested"]):
        raise ValueError(f"{IDENTITY}: minVersion is above maxVersionTested")
    for key in ("windowsPackagingSdk", "makeappx", "makepri"):
        windows_version(table[key])
    return table


def read_identity(root: Path = ROOT) -> dict:
    return load_identity(json.loads((root / IDENTITY).read_text(encoding="utf-8")))


# --- the version ------------------------------------------------------------------------------


def package_version(product_version: str, store: bool) -> str:
    """project(... VERSION x.y.z) -> x.y.z.0; the Store refuses a leading 0."""
    if not re.fullmatch(r"\d+\.\d+\.\d+", product_version):
        raise ValueError(f"the product version is not x.y.z: {product_version}")
    version = product_version + ".0"
    check_package_version(version, product_version, store)
    return version


def check_package_version(version: str, product_version: str, store: bool) -> None:
    numbers = windows_version(version)
    if numbers[3] != 0:
        raise ValueError(f"the fourth part of the package version must be 0: {version}")
    if version != product_version + ".0":
        raise ValueError(f"the package version {version} is not project() {product_version} + .0")
    if store and numbers[0] == 0:
        raise ValueError(f"the Microsoft Store refuses {version}: the first part must not be 0")


# --- the manifest -----------------------------------------------------------------------------


def manifest_xml(identity: dict, version: str) -> bytes:
    package = ET.Element("Package", {("xmlns:" + prefix if prefix else "xmlns"): uri
                                     for prefix, uri in NAMESPACES.items()})
    package.set("IgnorableNamespaces", "uap rescap")
    ET.SubElement(package, "Identity", {"Name": identity["identityName"], "Publisher": identity["publisher"],
                                        "Version": version, "ProcessorArchitecture": "x64"})
    properties = ET.SubElement(package, "Properties")
    for name, text in (("DisplayName", identity["displayName"]),
                       ("PublisherDisplayName", identity["publisherDisplayName"]),
                       ("Logo", "Assets\\StoreLogo.png")):
        ET.SubElement(properties, name).text = text
    dependencies = ET.SubElement(package, "Dependencies")
    ET.SubElement(dependencies, "TargetDeviceFamily", {"Name": "Windows.Desktop",
                                                       "MinVersion": identity["minVersion"],
                                                       "MaxVersionTested": identity["maxVersionTested"]})
    resources = ET.SubElement(package, "Resources")
    for language in identity["languages"]:
        ET.SubElement(resources, "Resource", {"Language": language})
    applications = ET.SubElement(package, "Applications")
    application = ET.SubElement(applications, "Application", {
        "Id": "NeNeNib", "Executable": EXECUTABLE, "EntryPoint": "Windows.FullTrustApplication"})
    ET.SubElement(application, "uap:VisualElements", {
        "DisplayName": identity["displayName"], "Description": identity["description"],
        "BackgroundColor": "transparent", "Square150x150Logo": "Assets\\Square150x150Logo.png",
        "Square44x44Logo": "Assets\\Square44x44Logo.png"})
    capabilities = ET.SubElement(package, "Capabilities")
    ET.SubElement(capabilities, "rescap:Capability", {"Name": "runFullTrust"})
    ET.indent(package, space="  ")
    return ET.tostring(package, encoding="utf-8", xml_declaration=True) + b"\n"


def check_manifest(data: bytes, identity: dict, product_version: str, store: bool) -> None:
    """The manifest found in a package names the table's identity and the CMake version."""
    tree = ET.fromstring(data)
    space = "{" + FOUNDATION + "}"
    found = tree.find(space + "Identity")
    if tree.tag != space + "Package" or found is None:
        raise ValueError("the manifest has no Package/Identity")
    expected = {"Name": identity["identityName"], "Publisher": identity["publisher"],
                "ProcessorArchitecture": "x64"}
    for key, value in expected.items():
        if found.get(key) != value:
            raise ValueError(f"the manifest Identity {key} is {found.get(key)}, not {value}")
    check_package_version(found.get("Version", ""), product_version, store)
    shown = tree.findtext(f"{space}Properties/{space}PublisherDisplayName")
    if shown != identity["publisherDisplayName"]:
        raise ValueError(f"the manifest PublisherDisplayName is {shown}")
    if data != manifest_xml(identity, found.get("Version")):
        raise ValueError("the manifest differs from the one this tool generates from the table")


# --- the logos --------------------------------------------------------------------------------


def logo_plan(icon_manifest: dict) -> dict[str, int]:
    plan = ICON.msix_logos(icon_manifest)
    if len(plan) != LOGO_COUNT:
        raise ValueError(f"eng/app-icon.json declares {len(plan)} MSIX logos, not {LOGO_COUNT}")
    return plan


def png_size(data: bytes) -> tuple[int, int]:
    if not data.startswith(PNG_SIGNATURE) or data[12:16] != b"IHDR":
        raise ValueError("not a PNG")
    return int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")


def check_logos(directory: Path, plan: dict[str, int]) -> dict[str, bytes]:
    files = sorted(directory.iterdir()) if directory.is_dir() else []
    if len(files) != LOGO_COUNT:
        raise ValueError(f"{directory} holds {len(files)} files, not the {LOGO_COUNT} MSIX logos")
    names = {path.name for path in files}
    if names != set(plan):
        raise ValueError(f"MSIX logo names differ: extra {sorted(names - set(plan))}, "
                         f"missing {sorted(set(plan) - names)}")
    logos = {}
    for path in files:
        data = path.read_bytes()
        if png_size(data) != (plan[path.name], plan[path.name]):
            raise ValueError(f"{path.name} is {png_size(data)}, not {plan[path.name]} square")
        logos[path.name] = data
    return logos


# --- the package contents ---------------------------------------------------------------------


def expected_files(plan: dict[str, int]) -> set[str]:
    return {EXECUTABLE, *NOTICES, *GENERATED} | {f"Assets/{name}" for name in plan}


def tree_files(folder: Path) -> dict[str, Path]:
    return {path.relative_to(folder).as_posix(): path for path in folder.rglob("*") if path.is_file()}


def check_file_set(actual: set[str], expected: set[str], where: str) -> None:
    if actual != expected:
        raise ValueError(f"{where}: extra {sorted(actual - expected)}, missing {sorted(expected - actual)}")


def check_archive(archive: Path, layout: Path, plan: dict[str, int]) -> list[dict]:
    """The MSIX is a ZIP: its entries are the layout's files plus the two SDK footprints, byte for byte."""
    expected = tree_files(layout)
    check_file_set(set(expected), expected_files(plan), "layout")
    entries = []
    with zipfile.ZipFile(archive) as bundle:
        names = [info.filename for info in bundle.infolist()]
        if len(names) != len({name.casefold() for name in names}):
            raise ValueError("the MSIX holds duplicate entries")
        check_file_set(set(names), set(expected) | set(FOOTPRINTS), "MSIX")
        for name in sorted(names):
            data = bundle.read(name)
            if name in expected and data != expected[name].read_bytes():
                raise ValueError(f"MSIX bytes differ for {name}")
            entries.append({"name": name, "bytes": len(data), "sha256": digest(data)})
    return entries


def check_pri_dump(data: bytes, identity: dict, plan: dict[str, int]) -> None:
    tree = ET.fromstring(data)
    maps = tree.findall("ResourceMap")
    if len(maps) != 1 or maps[0].get("name") != identity["identityName"]:
        raise ValueError("resources.pri does not name the package identity")
    candidates = {candidate.findtext("Value", "").replace("\\", "/")
                  for candidate in maps[0].iter("Candidate") if candidate.get("type") == "Path"}
    logos = {value for value in candidates if value.startswith("Assets/")}
    check_file_set(logos, {f"Assets/{name}" for name in plan}, "resources.pri logo candidates")


# --- the SDK ----------------------------------------------------------------------------------


def file_version(path: Path) -> str:
    text = RELEASE.run(["pwsh", "-NoProfile", "-Command",
                        f"(Get-Item -LiteralPath '{path}').VersionInfo.FileVersion"])
    return text.split(" ")[0]


def sdk_tools(identity: dict, program_files: Path) -> dict[str, Path]:
    folder = program_files / "Windows Kits/10/bin" / identity["windowsPackagingSdk"] / "x64"
    tools = {}
    for name in ("makeappx", "makepri"):
        path = folder / f"{name}.exe"
        if not path.is_file():
            raise ValueError(f"{path} is missing: install the Windows SDK {identity['windowsPackagingSdk']} "
                             "(MSIX packaging tools) or change the pin in eng/store-identity.json")
        tools[name] = path
    return tools


def check_tool_versions(tools: dict[str, Path], identity: dict) -> dict:
    record = {}
    for name, path in tools.items():
        version = file_version(path)
        if version != identity[name]:
            raise ValueError(f"QLT-011: {name} must be {identity[name]}; found {version} at {path}")
        record[name] = {"path": str(path), "version": version, "sha256": digest(path.read_bytes())}
    return record


def call(arguments: list) -> None:
    subprocess.run([str(item) for item in arguments], check=True, capture_output=True)


def build_pri(layout: Path, work: Path, makepri: Path, identity: dict, plan: dict[str, int]) -> dict:
    config = work / "priconfig.xml"
    call([makepri, "createconfig", "/cf", config, "/dq", identity["languages"][0], "/pv", "10.0.0", "/o"])
    tree = ET.parse(config)
    packaging = tree.getroot().findall("packaging")
    if len(packaging) != 1:
        raise ValueError("MakePRI wrote an unexpected resource packaging configuration")
    # One package holds one index: drop the split into per-language / per-scale resource packages.
    tree.getroot().remove(packaging[0])
    tree.write(config, encoding="utf-8", xml_declaration=True)
    index = layout / "resources.pri"
    call([makepri, "new", "/pr", layout, "/cf", config, "/mn", layout / "AppxManifest.xml", "/of", index, "/o"])
    dump = work / "resources.pri.xml"
    call([makepri, "dump", "/if", index, "/of", dump, "/dt", "detailed", "/o"])
    check_pri_dump(dump.read_bytes(), identity, plan)
    return {"sha256": digest(index.read_bytes()), "bytes": index.stat().st_size}


def pack_and_unpack(layout: Path, archive: Path, unpacked: Path, makeappx: Path) -> None:
    call([makeappx, "pack", "/d", layout, "/p", archive, "/h", "SHA256", "/no"])
    call([makeappx, "unpack", "/p", archive, "/d", unpacked, "/no"])


def check_unpacked(unpacked: Path, layout: Path, plan: dict[str, int], executable_sha: str,
                   identity: dict, product_version: str, store: bool) -> None:
    files = tree_files(unpacked)
    check_file_set(set(files), expected_files(plan) | set(UNPACKED_FOOTPRINTS), "unpacked MSIX")
    if digest(files[EXECUTABLE].read_bytes()) != executable_sha:
        raise ValueError("the unpacked exe differs from the input exe")
    for name, path in tree_files(layout).items():
        if files[name].read_bytes() != path.read_bytes():
            raise ValueError(f"the unpacked {name} differs from the layout")
    check_manifest(files["AppxManifest.xml"].read_bytes(), identity, product_version, store)


# --- the run ----------------------------------------------------------------------------------


def zip_executable(archive: Path) -> bytes:
    with zipfile.ZipFile(archive) as bundle:
        return bundle.read(EXECUTABLE)


def stage(layout: Path, binary: bytes, logos: dict[str, bytes], manifest: bytes, root: Path = ROOT) -> None:
    (layout / "Assets").mkdir(parents=True)
    (layout / EXECUTABLE).write_bytes(binary)
    for name in NOTICES:
        shutil.copyfile(root / name, layout / name)
    for name, data in logos.items():
        (layout / "Assets" / name).write_bytes(data)
    (layout / "AppxManifest.xml").write_bytes(manifest)


def package(args: argparse.Namespace) -> Path:
    output = RELEASE.validate_output(args.output, OUTPUT_ROOTS)
    if args.store and args.zip is None:
        raise ValueError("--store needs --zip: the Store exe must be the ZIP's exe")
    commit, branch = RELEASE.source_state(ROOT, not args.store)
    product_version = RELEASE.version_of((ROOT / "CMakeLists.txt").read_text(encoding="utf-8"))
    version = package_version(product_version, args.store)
    identity = read_identity()
    icon_findings = ICON.verify(ROOT)
    if icon_findings:
        raise ValueError("the committed icons differ from eng/app-icon.json: " + "; ".join(icon_findings))
    plan = logo_plan(ICON.load_manifest(ROOT))
    logos = check_logos(ROOT / ICON.load_manifest(ROOT)["msixDirectory"], plan)
    short = RELEASE.run(["git", "rev-parse", "--short=7", commit])
    executable = ROOT / "build" / f"release-{short}" / EXECUTABLE
    release = json.loads((ROOT / "out/release" / f"{short}.json").read_text(encoding="utf-8-sig"))
    binary = executable.read_bytes()
    executable_sha = digest(binary)
    if release["commit"] != commit or release["head"] != commit or release["sha256"].lower() != executable_sha:
        raise ValueError("the Release build record does not name HEAD and this exe; run eng/build-release.ps1 -Ref HEAD")
    allowed = RELEASE.platform_dlls(json.loads((ROOT / "eng/architecture.json").read_text(encoding="utf-8")))
    report = RELEASE.run(["llvm-readobj", "--file-headers", "--coff-imports", str(executable)])
    metadata = json.loads(RELEASE.run(["pwsh", "-NoProfile", "-File",
                                       str(ROOT / "eng/release/inspect-executable.ps1"), str(executable)]))
    imports = RELEASE.validate_pe(report, metadata, product_version, allowed)
    zip_record = None
    if args.zip is not None:
        if zip_executable(args.zip) != binary:
            raise ValueError(f"the exe in {args.zip} differs from {executable}")
        zip_record = {"path": str(args.zip), "sha256": digest(args.zip.read_bytes()),
                      "executableSha256": executable_sha, "executableMatches": True}
    tools = sdk_tools(identity, Path(args.program_files))
    sdk = check_tool_versions(tools, identity)
    manifest = manifest_xml(identity, version)
    check_manifest(manifest, identity, product_version, args.store)
    output.mkdir(parents=True)
    layout, work, unpacked = output / "layout", output / "work", output / "unpacked"
    work.mkdir()
    stage(layout, binary, logos, manifest)
    pri = build_pri(layout, work, tools["makepri"], identity, plan)
    suffix = "-store" if args.store else ""
    archive = output / f"NeNeNib-{version}-windows-x64{suffix}.msix"
    pack_and_unpack(layout, archive, unpacked, tools["makeappx"])
    entries = check_archive(archive, layout, plan)
    check_unpacked(unpacked, layout, plan, executable_sha, identity, product_version, args.store)
    archive_sha = digest(archive.read_bytes())
    (output / "SHA256SUMS").write_text(f"{archive_sha}  {archive.name}\n", encoding="ascii")
    record = {"schema": 1, "commit": commit, "branch": branch, "store": args.store,
              "productVersion": product_version, "packageVersion": version,
              "identity": {key: identity[key] for key in ("identityName", "publisher", "publisherDisplayName",
                                                          "packageFamilyName", "storeId")},
              "release": release, "metadata": metadata, "imports": imports,
              "executable": {"path": executable.relative_to(ROOT).as_posix(), "sha256": executable_sha,
                             "bytes": len(binary)},
              "zip": zip_record, "sdk": sdk, "resourceIndex": pri,
              "msix": {"name": archive.name, "sha256": archive_sha, "bytes": archive.stat().st_size},
              "entries": entries, "unpackedMatches": True,
              "signed": False, "installed": False, "launched": False, "submitted": False}
    (output / "msix.json").write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return archive


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", type=Path, required=True,
                        help="a new directory below D:/NeNeNib/outputs, D:/NeNeNib/releases or out/")
    parser.add_argument("--zip", type=Path, help="the release ZIP whose NeNeNib.exe must be the same bytes")
    parser.add_argument("--store", action="store_true",
                        help="the submission: clean main at origin/main, --zip, and a first part above 0")
    parser.add_argument("--program-files", default=os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"),
                        help="where Windows Kits lives (the SDK pin is in eng/store-identity.json)")
    args = parser.parse_args()
    print(package(args))


if __name__ == "__main__":
    main()
