"""eng/package-msix.py の検査を、表・manifest・ロゴ・中身の集合の正例と反例で回す（Issue #1581）。

manifest の生成と照合・版・ロゴの集合は SDK の要らない純関数で試す。MakeAppx と MakePRI を通す 1 件は、
eng/store-identity.json が固定した Windows SDK が無い機械（CI）では理由を書いて飛ばす。
写しと一時のファイルは作業木の out/ の下に作る（C ドライブの Temp に書かない）。
"""

import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("package_msix", ROOT / "eng/package-msix.py")
MSIX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MSIX)

TABLE = json.loads((ROOT / "eng/store-identity.json").read_text(encoding="utf-8"))
ICON_MANIFEST = MSIX.ICON.load_manifest(ROOT)
PLAN = MSIX.logo_plan(ICON_MANIFEST)
LOGO_DIRECTORY = ROOT / ICON_MANIFEST["msixDirectory"]
SPACE = "{" + MSIX.FOUNDATION + "}"


class PackageMsixTests(unittest.TestCase):
    def setUp(self):
        directory = ROOT / "out/package-msix-tests"
        directory.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=directory)
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name)

    def identity(self, **changes):
        table = copy.deepcopy(TABLE)
        table.update(changes)
        return table

    # --- the table ----------------------------------------------------------------------------

    def test_the_table_holds_the_reserved_identity_of_d88(self):
        identity = MSIX.read_identity(ROOT)
        self.assertEqual((identity["identityName"], identity["publisher"], identity["publisherDisplayName"],
                          identity["packageFamilyName"], identity["storeId"], identity["displayName"]),
                         ("HideyukiMori.NeNeNib", "CN=C37230AA-B52D-403B-9BFD-E7980F088422", "Hideyuki Mori",
                          "HideyukiMori.NeNeNib_3sft4ch5kzywy", "9NCLGDTFQ529", "NeNe Nib"))
        self.assertEqual(identity["languages"], ["ja-JP", "en-US", "zh-Hans"])
        self.assertEqual(identity["minVersion"], "10.0.22621.0")

    def test_the_family_name_is_computed_from_the_publisher(self):
        self.assertEqual(MSIX.publisher_hash(TABLE["publisher"]), "3sft4ch5kzywy")
        for changes in ({"publisher": "CN=C37230AA-B52D-403B-9BFD-E7980F088423"},
                        {"identityName": "HideyukiMori.NeNeNib2"},
                        {"packageFamilyName": "HideyukiMori.NeNeNib_3sft4ch5kzywz"}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                MSIX.load_identity(self.identity(**changes))

    def test_a_malformed_table_is_refused(self):
        extra = self.identity(fileTypeAssociation="txt")
        missing = self.identity()
        del missing["storeId"]
        for table in (extra, missing, self.identity(schema=2), self.identity(publisher="C37230AA"),
                      self.identity(storeId="ABC"), self.identity(languages=[]),
                      self.identity(languages=["ja-JP", "ja-JP"]), self.identity(displayName=" "),
                      self.identity(minVersion="10.0.26200.0", maxVersionTested="10.0.26100.0"),
                      self.identity(makeappx="10.0.26100")):
            with self.subTest(table=table), self.assertRaises(ValueError):
                MSIX.load_identity(table)

    # --- the version --------------------------------------------------------------------------

    def test_the_version_is_project_version_with_a_zero(self):
        self.assertEqual(MSIX.package_version("0.1.0", store=False), "0.1.0.0")
        self.assertEqual(MSIX.package_version("1.0.0", store=True), "1.0.0.0")
        for text, store in (("0.1.0", True), ("1.0", False), ("1.0.0.0", False), ("1.0.0-beta.1", False),
                            ("65536.0.0", False)):
            with self.subTest(text=text, store=store), self.assertRaises(ValueError):
                MSIX.package_version(text, store)

    def test_a_package_version_off_project_or_with_a_fourth_part_is_refused(self):
        MSIX.check_package_version("1.2.3.0", "1.2.3", store=True)
        for version in ("1.2.3.1", "1.2.4.0", "1.2.3", "0.1.0.0"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                MSIX.check_package_version(version, "1.2.3" if version != "0.1.0.0" else "0.1.0", store=True)

    def test_the_version_comes_from_cmake(self):
        product = MSIX.RELEASE.version_of((ROOT / "CMakeLists.txt").read_text(encoding="utf-8"))
        manifest = MSIX.manifest_xml(TABLE, MSIX.package_version(product, store=False))
        self.assertEqual(ET.fromstring(manifest).find(SPACE + "Identity").get("Version"), product + ".0")

    # --- the manifest -------------------------------------------------------------------------

    def test_the_manifest_is_the_minimal_full_trust_desktop_package(self):
        tree = ET.fromstring(MSIX.manifest_xml(TABLE, "1.0.0.0"))
        uap = "{" + MSIX.NAMESPACES["uap"] + "}"
        rescap = "{" + MSIX.NAMESPACES["rescap"] + "}"
        self.assertEqual(tree.find(SPACE + "Identity").attrib,
                         {"Name": "HideyukiMori.NeNeNib", "Publisher": TABLE["publisher"],
                          "Version": "1.0.0.0", "ProcessorArchitecture": "x64"})
        family = tree.find(f"{SPACE}Dependencies/{SPACE}TargetDeviceFamily").attrib
        self.assertEqual(family, {"Name": "Windows.Desktop", "MinVersion": "10.0.22621.0",
                                  "MaxVersionTested": "10.0.26100.0"})
        self.assertEqual([item.get("Language") for item in tree.iter(SPACE + "Resource")],
                         ["ja-JP", "en-US", "zh-Hans"])
        application = tree.find(f"{SPACE}Applications/{SPACE}Application")
        self.assertEqual(application.get("EntryPoint"), "Windows.FullTrustApplication")
        self.assertEqual(application.get("Executable"), "NeNeNib.exe")
        self.assertEqual([element.tag for element in application], [uap + "VisualElements"])
        self.assertEqual([(item.tag, item.get("Name")) for item in tree.find(SPACE + "Capabilities")],
                         [(rescap + "Capability", "runFullTrust")])

    def test_the_generated_manifest_passes_its_own_check(self):
        MSIX.check_manifest(MSIX.manifest_xml(TABLE, "0.1.0.0"), TABLE, "0.1.0", store=False)
        MSIX.check_manifest(MSIX.manifest_xml(TABLE, "1.0.0.0"), TABLE, "1.0.0", store=True)

    def test_a_manifest_off_the_table_or_off_cmake_is_refused(self):
        def changed(attribute, value, version="1.0.0.0"):
            tree = ET.fromstring(MSIX.manifest_xml(TABLE, version))
            tree.find(SPACE + "Identity").set(attribute, value)
            return ET.tostring(tree, encoding="utf-8", xml_declaration=True)

        cases = (
            (changed("Name", "HideyukiMori.NeNeLoupe"), "1.0.0", True),
            (changed("Publisher", "CN=NeNe Nib MSIX Local Test"), "1.0.0", True),
            (changed("ProcessorArchitecture", "arm64"), "1.0.0", True),
            (changed("Version", "1.0.1.0"), "1.0.0", True),
            (changed("Version", "1.0.0.1"), "1.0.0", True),
            (MSIX.manifest_xml(TABLE, "0.1.0.0"), "0.1.0", True),
            (MSIX.manifest_xml(self.identity(publisherDisplayName="hideyukiMORI"), "1.0.0.0"), "1.0.0", True),
            (MSIX.manifest_xml(self.identity(minVersion="10.0.19041.0"), "1.0.0.0"), "1.0.0", True),
            (MSIX.manifest_xml(TABLE, "1.0.0.0").replace(b"runFullTrust", b"broadFileSystemAccess"), "1.0.0", True),
        )
        for data, product, store in cases:
            with self.subTest(data=data[:200]), self.assertRaises(ValueError):
                MSIX.check_manifest(data, TABLE, product, store)

    # --- the logos ----------------------------------------------------------------------------

    def logo_copy(self) -> Path:
        directory = self.work / "logos"
        shutil.copytree(LOGO_DIRECTORY, directory)
        return directory

    def test_the_committed_logos_are_the_25_of_the_plan(self):
        logos = MSIX.check_logos(LOGO_DIRECTORY, PLAN)
        self.assertEqual(len(logos), 25)
        self.assertEqual(MSIX.png_size(logos["Square150x150Logo.scale-400.png"]), (600, 600))

    def test_a_missing_extra_renamed_or_resized_logo_is_refused(self):
        def remove(directory):
            (directory / "StoreLogo.scale-100.png").unlink()

        def extra(directory):
            shutil.copyfile(directory / "StoreLogo.scale-100.png", directory / "StoreLogo.scale-300.png")

        def rename(directory):
            (directory / "StoreLogo.scale-100.png").rename(directory / "StoreLogo.scale-101.png")

        def resize(directory):
            path = directory / "StoreLogo.scale-100.png"
            data = bytearray(path.read_bytes())
            data[16:24] = (51).to_bytes(4, "big") * 2
            path.write_bytes(bytes(data))

        for change in (remove, extra, rename, resize):
            with self.subTest(change=change.__name__):
                directory = self.logo_copy()
                change(directory)
                with self.assertRaises(ValueError):
                    MSIX.check_logos(directory, PLAN)
                shutil.rmtree(directory)

    def test_an_icon_manifest_declaring_another_count_is_refused(self):
        manifest = copy.deepcopy(ICON_MANIFEST)
        manifest["scales"] = manifest["scales"][:-1]
        with self.assertRaises(ValueError):
            MSIX.logo_plan(manifest)

    # --- the package contents -----------------------------------------------------------------

    def layout(self) -> Path:
        layout = self.work / "layout"
        MSIX.stage(layout, b"fixture exe; never launched", MSIX.check_logos(LOGO_DIRECTORY, PLAN),
                   MSIX.manifest_xml(TABLE, "0.1.0.0"))
        (layout / "resources.pri").write_bytes(b"fixture index")
        return layout

    def archive(self, layout: Path, name: str, change=None) -> Path:
        files = {key: path.read_bytes() for key, path in MSIX.tree_files(layout).items()}
        files.update({footprint: b"<fixture/>" for footprint in MSIX.FOOTPRINTS})
        if change:
            change(files)
        path = self.work / f"{name}.msix"
        with zipfile.ZipFile(path, "x") as bundle:
            for key, data in files.items():
                bundle.writestr(key, data)
        return path

    def test_the_package_holds_exactly_the_expected_files(self):
        layout = self.layout()
        self.assertEqual(len(MSIX.expected_files(PLAN)), 25 + 5)
        entries = MSIX.check_archive(self.archive(layout, "good"), layout, PLAN)
        self.assertEqual(len(entries), 25 + 5 + 2)
        exe = next(entry for entry in entries if entry["name"] == "NeNeNib.exe")
        self.assertEqual(exe["sha256"], MSIX.digest(b"fixture exe; never launched"))

    def test_an_extra_missing_or_different_entry_is_refused(self):
        layout = self.layout()
        changes = {
            "extra": lambda files: files.update({"README.txt": b"x"}),
            "missing": lambda files: files.pop("Assets/StoreLogo.scale-100.png"),
            "exe": lambda files: files.update({"NeNeNib.exe": b"another exe"}),
            "manifest": lambda files: files.update({"AppxManifest.xml": b"<Package/>"}),
            "footprint": lambda files: files.pop("[Content_Types].xml"),
        }
        for name, change in changes.items():
            with self.subTest(name=name), self.assertRaises(ValueError):
                MSIX.check_archive(self.archive(layout, name, change), layout, PLAN)

    def test_the_resource_index_must_name_the_identity_and_every_logo(self):
        def dump(name, logos):
            candidates = "".join(f'<NamedResource><Candidate type="Path"><Value>Assets\\{logo}</Value>'
                                 "</Candidate></NamedResource>" for logo in logos)
            return f'<PriInfo><ResourceMap name="{name}">{candidates}</ResourceMap></PriInfo>'.encode()

        MSIX.check_pri_dump(dump(TABLE["identityName"], PLAN), TABLE, PLAN)
        for data in (dump("HideyukiMori.NeNeLoupe", PLAN), dump(TABLE["identityName"], list(PLAN)[1:])):
            with self.subTest(data=data[:60]), self.assertRaises(ValueError):
                MSIX.check_pri_dump(data, TABLE, PLAN)

    def test_the_store_form_needs_the_zip(self):
        args = argparse.Namespace(output=self.work / "store", zip=None, store=True, program_files="")
        with self.assertRaisesRegex(ValueError, "--store needs --zip"):
            MSIX.package(args)

    def test_a_missing_sdk_is_named(self):
        with self.assertRaisesRegex(ValueError, "Windows SDK 10.0.26100.0"):
            MSIX.sdk_tools(TABLE, self.work / "no-program-files")

    # --- the SDK ------------------------------------------------------------------------------

    def test_makeappx_and_makepri_pack_and_unpack_the_same_bytes(self):
        program_files = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
        try:
            tools = MSIX.sdk_tools(TABLE, program_files)
        except ValueError as missing:
            self.skipTest(f"skipped, the pinned Windows SDK is not on this machine: {missing}")
        layout = self.work / "layout"
        binary = b"fixture exe; never launched"
        MSIX.stage(layout, binary, MSIX.check_logos(LOGO_DIRECTORY, PLAN), MSIX.manifest_xml(TABLE, "0.1.0.0"))
        (self.work / "pri").mkdir()
        MSIX.build_pri(layout, self.work / "pri", tools["makepri"], TABLE, PLAN)
        archive, unpacked = self.work / "fixture.msix", self.work / "unpacked"
        MSIX.pack_and_unpack(layout, archive, unpacked, tools["makeappx"])
        self.assertEqual(len(MSIX.check_archive(archive, layout, PLAN)), 25 + 5 + 2)
        MSIX.check_unpacked(unpacked, layout, PLAN, MSIX.digest(binary), TABLE, "0.1.0", store=False)
        (unpacked / "NeNeNib.exe").write_bytes(b"another exe")
        with self.assertRaises(ValueError):
            MSIX.check_unpacked(unpacked, layout, PLAN, MSIX.digest(binary), TABLE, "0.1.0", store=False)


if __name__ == "__main__":
    unittest.main()
