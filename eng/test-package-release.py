"""Focused positive/negative tests of the package tool; no product build or GUI."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import unittest
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location("package_release", Path(__file__).with_name("package-release.py"))
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)
WORK = None


class PackageTests(unittest.TestCase):
    def test_version_has_one_source(self):
        self.assertEqual(PACKAGE.version_of("project(NeNeNib VERSION 2.3.4 LANGUAGES C CXX)"), "2.3.4")
        for text in ("", "project(NeNeNib VERSION 2.3 LANGUAGES C CXX)",
                     "project(NeNeNib VERSION 1.2.3 LANGUAGES CXX)",
                     "project(NeNeNib VERSION 1.2.3 LANGUAGES C CXX)\n" * 2):
            with self.subTest(text=text), self.assertRaises(ValueError):
                PACKAGE.version_of(text)

    def test_output_bounds_and_existing_path(self):
        target = WORK / "safe" / "new"
        self.assertEqual(PACKAGE.validate_output(target, (WORK,)), target)
        for path in (WORK, WORK / ".." / "outside", WORK / "existing"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                PACKAGE.validate_output(path, (WORK,))

    def test_reparse_ancestor(self):
        link = WORK / "junction"
        script = WORK / "make-junction.ps1"
        script.write_text("param($link,$target)\nNew-Item -ItemType Junction -Path $link -Target $target | Out-Null\n",
                          encoding="utf-8")
        subprocess.run(["pwsh", "-NoProfile", "-File", str(script),
                        str(link), str(WORK / "existing")], check=True, capture_output=True)
        with self.assertRaises(ValueError):
            PACKAGE.validate_output(link / "new", (WORK,))

    def test_platform_dependency_graph(self):
        architecture = {"runtimeDependencies": [], "modules": {
            "app": {"dependencies": ["ui", "core"]}, "ui": {"dependencies": ["core"]},
            "core": {"dependencies": []}},
            "platformLibraries": {"app": ["kernel32"], "ui": ["dwrite"], "tests": ["forbidden"]}}
        self.assertEqual(PACKAGE.platform_dlls(architecture), {"kernel32.dll", "dwrite.dll"})
        architecture["runtimeDependencies"] = ["additional"]
        with self.assertRaises(ValueError):
            PACKAGE.platform_dlls(architecture)

    def test_pe_architecture_version_and_imports(self):
        report = "Machine: IMAGE_FILE_MACHINE_AMD64 (0x8664)\n  Name: KERNEL32.dll\n"
        meta = {"fileVersion": "1.2.3.0", "productVersion": "1.2.3"}
        allowed = {"kernel32.dll"}
        self.assertEqual(PACKAGE.validate_pe(report, meta, "1.2.3", allowed), ["kernel32.dll"])
        for bad_report, bad_meta in ((report.replace("AMD64", "I386"), meta),
                                     (report + "  Name: additional.dll\n", meta),
                                     (report, {**meta, "fileVersion": "1.2.4.0"}),
                                     (report, {**meta, "productVersion": "2.0.0"}),
                                     ("Machine: IMAGE_FILE_MACHINE_AMD64 (0x8664)", meta)):
            with self.subTest(report=bad_report, meta=bad_meta), self.assertRaises(ValueError):
                PACKAGE.validate_pe(bad_report, bad_meta, "1.2.3", allowed)

    def test_archive_exact_content(self):
        contents = {name: name.encode() for name in PACKAGE.ENTRIES}
        for label, entries in (("good", contents), ("extra", {**contents, "extra": b"x"}),
                               ("wrong", {**contents, "LICENSE": b"wrong"}),
                               ("missing", {"NeNeNib.exe": b"x"})):
            archive = WORK / f"{label}.zip"
            with zipfile.ZipFile(archive, "x") as bundle:
                for name, data in entries.items():
                    bundle.writestr(name, data)
            if label == "good":
                PACKAGE.verify_zip(archive, contents, WORK / label)
                self.assertEqual((WORK / label / "LICENSE").read_bytes(), contents["LICENSE"])
            else:
                with self.subTest(label=label), self.assertRaises(ValueError):
                    PACKAGE.verify_zip(archive, contents, WORK / label)

    def test_source_state(self):
        commit = "a" * 40
        for outputs, preview, expected in ((["dirty"], False, False),
                                           (["", commit, "topic", commit + "\trefs/heads/main"], False, False),
                                           (["", commit, "main", "b" * 40 + "\trefs/heads/main"], False, False),
                                           (["", commit, "main", commit + "\trefs/heads/main"], False, True),
                                           (["", commit, "", commit + "\trefs/heads/main"], False, True),
                                           (["", commit, "topic"], True, True)):
            with self.subTest(outputs=outputs), patch.object(PACKAGE, "run", side_effect=outputs):
                if expected:
                    self.assertEqual(PACKAGE.source_state(WORK, preview), (commit, outputs[2]))
                else:
                    with self.assertRaises(ValueError):
                        PACKAGE.source_state(WORK, preview)

    def test_positive_beta(self):
        self.assertEqual(PACKAGE.positive("1"), 1)
        for value in ("0", "-1"):
            with self.assertRaises(argparse.ArgumentTypeError):
                PACKAGE.positive(value)

    def test_complete_package_after_build_boundary(self):
        root = WORK / "fixture-source"
        (root / "eng/release").mkdir(parents=True)
        (root / "CMakeLists.txt").write_text("project(NeNeNib VERSION 1.2.3 LANGUAGES C CXX)")
        (root / "LICENSE").write_bytes(b"fixture license")
        (root / "THIRD_PARTY_NOTICES.txt").write_bytes(b"fixture palette notices")
        (root / "eng/release/README.txt.in").write_text("fixture @VERSION@")
        (root / "eng/architecture.json").write_text(json.dumps({"runtimeDependencies": [],
            "modules": {"app": {"dependencies": []}}, "platformLibraries": {"app": ["kernel32"]}}))
        binary, commit = b"fixture executable; never launched", "a" * 40

        def build(arguments, **options):
            self.assertEqual(arguments[-2:], ["-Ref", "HEAD"])
            self.assertEqual(Path(arguments[3]), root / "eng/build-release.ps1")
            directory = root / "build/release-aaaaaaa"
            directory.mkdir(parents=True)
            (directory / "NeNeNib.exe").write_bytes(binary)
            record = root / "out/release/aaaaaaa.json"
            record.parent.mkdir(parents=True)
            record.write_text(json.dumps({"commit": commit, "head": commit,
                                          "sha256": PACKAGE.digest(binary)}))

        def inspect(arguments, *unused):
            if arguments[0] == "git":
                return "aaaaaaa"
            if arguments[0] == "llvm-readobj":
                return "Machine: IMAGE_FILE_MACHINE_AMD64 (0x8664)\n  Name: KERNEL32.dll\n"
            self.assertEqual(arguments[0], "pwsh")
            return json.dumps({"fileVersion": "1.2.3.0", "productVersion": "1.2.3", "signature": "NotSigned"})

        output = WORK / "complete-package"
        args = argparse.Namespace(output=output, preview=True, verification="proof.md", beta=2)
        with patch.object(PACKAGE, "ROOT", root), patch.object(PACKAGE, "source_state", return_value=(commit, "topic")), \
             patch.object(PACKAGE, "verification_record", return_value={"path": "proof.md", "sha256": "fixture"}), \
             patch.object(PACKAGE, "run", side_effect=inspect), patch.object(PACKAGE.subprocess, "run", side_effect=build):
            archive = PACKAGE.package(args)
        self.assertEqual(archive.name, "NeNeNib-1.2.3-beta.2-windows-x64.zip")
        self.assertEqual((output / "extracted/NeNeNib.exe").read_bytes(), binary)
        self.assertEqual((output / "extracted/README.txt").read_text(), "fixture 1.2.3-beta.2")
        self.assertEqual((output / "extracted/THIRD_PARTY_NOTICES.txt").read_bytes(),
                         b"fixture palette notices")
        record = json.loads((output / "package.json").read_text())
        self.assertTrue(record["preview"] and record["extractedBytesMatch"])
        self.assertFalse(record["launched"] or record["published"])
        self.assertEqual((output / "SHA256SUMS").read_text().split()[0], PACKAGE.digest(archive.read_bytes()))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    WORK = PACKAGE.validate_output(args.work_dir)
    WORK.mkdir(parents=True)
    (WORK / "existing").mkdir()
    unittest.main(argv=[__file__], verbosity=2)
