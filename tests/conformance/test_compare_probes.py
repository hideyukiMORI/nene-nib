"""Focused protocol and fixed-order checks for the local comparison tool (#329)."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("compare_probes", ROOT / "eng/compare-probes.py")
PROBES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROBES)
COMMIT = "a" * 40


class ProbeComparisonTests(unittest.TestCase):
    def setUp(self):
        directory = ROOT / "out/probe-tool-tests"
        directory.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=directory)
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        before = self.folder / "before.exe"
        after = self.folder / "after.exe"
        before.write_bytes(b"before executable")
        after.write_bytes(b"after executable")
        self.options = argparse.Namespace(before=before, after=after, before_ref="before-ref",
                                          after_ref="after-ref", before_commit=COMMIT,
                                          after_commit=COMMIT, workload="display-line-long",
                                          iterations=2, blocks=1, timeout=1.0,
                                          output=self.folder / "comparison.json")

    def runner(self, command, timeout):
        self.assertEqual(timeout, 1.0)
        iterations = int(command[2])
        data = PROBES.fixed_input(command[1])
        metadata = {"schema": 1, "workload": command[1], "iterations": iterations, "warmup": 1,
                    "inputBytes": len(data), "inputHashAlgorithm": "fnv1a64",
                    "inputHash": PROBES.fnv1a64(data), "outputChecksum": "123",
                    "commit": COMMIT, "dirty": 0, "configuration": "Release",
                    "compilerVersion": "19.1.5", "toolchainSha256": "b" * 64}
        marks = [{"milestone": name, "qpcMicroseconds": index * 100 + offset}
                 for index in range(iterations)
                 for name, offset in (("probe_started", 10), ("probe_finished", 20))]
        Path(command[3]).write_text(json.dumps({"qpcFrequency": 1000000, "marks": marks}), encoding="utf-8")
        return subprocess.CompletedProcess(command, 0, json.dumps(metadata), "")

    def test_operation_choice_fixed_inputs(self):
        cases = (
            ('operation-choice-reading-ordinary-1024', b'operation-choice-v1\nreading-ordinary\nordinary\n\xe3\x81\xbb\xe3\x81\x9e\xe3\x82\x93\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', '3ee5c99cf6209f03ee3a30aa0f0028b5cf7c4cc9d8a7e56f1ed5fec7c9990e13', 126, '9827213030368951609'),
            ('operation-choice-miss-ordinary-1024', b'operation-choice-v1\nmiss-ordinary\nordinary\nqqq\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', 'e662130907262183ff09c8544266c743c0d92d3d66befd34271f90840525f2d9', 117, '10923022879623698261'),
            ('operation-choice-all-ordinary-256', b'operation-choice-v1\nall-ordinary\nordinary\n\n256\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', '0837c348069de39896079ff7e32841af6ef3f4d07cae87678593eef6ab67f9c6', 112, '11167900429985717533'),
            ('operation-choice-all-vim-256', b'operation-choice-v1\nall-vim\nvim\n\n256\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', '30a49f787db9e2084179e23ec13e3e41fd1df48395302b68f96293f5fc2034e1', 102, '6653010295187654273'),
            ('operation-choice-space-ordinary-256', b'operation-choice-v1\nspace-ordinary\nordinary\n \n256\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', '51795f5da6e427c9945dc016c9fc787d9a0b039ff64003605ac54d6c138650a9', 115, '6929459950631445612'),
            ('operation-choice-key-vim-1024', b'operation-choice-v1\nkey-vim\nvim\ncTrL+s\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', 'affe623909a5b1ab4de1716eff764fba295bdab5b9910ce0f3ea5848fa2c1690', 109, '16597966209003581918'),
            ('operation-choice-name-ordinary-1024', b'operation-choice-v1\nname-ordinary\nordinary\n\xe3\x82\xbf\xe3\x83\x96\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', 'bb25e8820691efe1b16058aebc77bb1cd8b992d8ac50f64f9435eeccaca012f4', 120, '1663071639732138461'),
            ('operation-choice-close-key-vim-1024', b'operation-choice-v1\nclose-key-vim\nvim\nCTRL+F4\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', '45d86035ab2de460c3f6ba43d7c1bfada134a7894f66b778bb05c6b0fa4bcf34', 116, '15629853484372669928'),
            ('operation-choice-description-miss-1024', b'operation-choice-v1\ndescription-miss\nordinary\n\xe4\xb8\x8a\xe6\x9b\xb8\xe3\x81\x8d\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', 'cde9db53e269bc5da2feaac3b744e3bd5953b14ec9d1c270369a9b33d7b24c9c', 126, '12690603971269174588'),
            ('operation-choice-short-name-miss-1024', b'operation-choice-v1\nshort-name-miss\nordinary\n\xe3\x83\x98\xe3\x83\xab\xe3\x83\x97\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n', 'aef9a14afb48d8ddf5e9cb35d4de9bb4bd1c73029bfb4d0a24c071b26f5b1de3', 125, '15438258473254498258'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[138:148])
        for workload, data, sha, size, fnv in cases:
            with self.subTest(workload=workload):
                self.assertEqual(PROBES.fixed_input(workload), data)
                self.assertEqual(len(data), size)
                self.assertEqual(hashlib.sha256(data).hexdigest(), sha)
                self.assertEqual(PROBES.fnv1a64(data), fnv)

    def test_text_object_fixed_inputs(self):
        """#732 の 4 本。fnv は probe の metadata の inputHash と同じ値。"""
        cases = (
            ('text-object-sentence-giant-1line-16384', 16384, '10307392757304409428',
             '3fe71a942b20a3333f759f27ae99aecb1ead34c490649e553a547dd15e12b55d'),
            ('text-object-sentence-dense-4096', 4096, '4676608582205631058',
             '140e178f8cd4274dd5c27668aff4bae5c479924fa0d14ea8a0064a66f4ae063d'),
            ('text-object-tag-nested-4096', 4096, '13678402818844899221',
             '649fc60affc5e52f2b262d8a5f149730c84b942ac3373d45afd9235997eddbd8'),
            ('text-object-tag-unclosed-4096', 4096, '10657823208925583041',
             'c94b7388218ee17d2fd21171e6a08f3d1a854558997f5c44f8ae2d10ea6eed7f'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[148:152])
        giant = PROBES.fixed_input(cases[0][0])
        self.assertEqual(len(giant), PROBES.giant_line_threshold())
        self.assertNotIn(b".", giant)
        for name, size, fnv, sha in cases:
            with self.subTest(workload=name):
                data = PROBES.fixed_input(name)
                self.assertEqual(len(data), size)
                self.assertEqual(PROBES.fnv1a64(data), fnv)
                self.assertEqual(hashlib.sha256(data).hexdigest(), sha)

    def test_lexer_fixed_inputs(self):
        """#815 の 2 本。fnv は probe の metadata の inputHash と同じ値（Debug の probe で確かめた）。"""
        cases = (
            ('lexer-cpp-visible-60', 6060, '135524141240447113',
             '0b046e9fb84c3326fdefe7615e3c754c8d8f810a8822685017d6137edbcf52dc'),
            ('lexer-cpp-backfill-200', 20200, '1493491185133298014',
             '17f0009a6a7dd4c10ed86085de56e7cc6fa118c8f47880972b4df6fa08173191'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[152:154])
        for name, size, fnv, sha in cases:
            with self.subTest(workload=name):
                data = PROBES.fixed_input(name)
                self.assertEqual(len(data), size)
                self.assertTrue(all(len(line) == 100 for line in data.split(b"\n")[:-1]))
                self.assertEqual(PROBES.fnv1a64(data), fnv)
                self.assertEqual(hashlib.sha256(data).hexdigest(), sha)

    def test_operation_choice_bad_name(self):
        for name in ("operation-choice-missing-1024", "Operation-choice-all-vim-256"):
            with self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_operation_choice_preserves_old_registry(self):
        old = ('controller-open-utf8-16mib', 'buffer-from-utf8-16mib', 'controller-insert-200', 'display-line-long', 'controller-insert-200-after-delete-1mib', 'controller-insert-200-after-delete-16mib', 'utf8-validate-ascii-16mib', 'utf8-validate-japanese-6mib', 'controller-vim-insert-200-register-1mib', 'controller-vim-insert-200-register-16mib', 'controller-vim-record-insert-200', 'controller-vim-record-insert-2000', 'buffer-line-text-scattered-crlf-4096', 'buffer-line-text-scattered-lf-4096', 'buffer-position-long-utf8-57344', 'buffer-position-scattered-utf8-57344', 'palette-listed-name-5000', 'palette-listed-location-5000', 'palette-append-narrow-5000-to-50', 'palette-caret-left-5000', 'codepage-to-utf8-cp932-japanese-16mib', 'utf16-to-utf8-japanese-8m-units', 'utf16-to-utf8-ascii-8m-units', 'utf16-to-utf8-supplementary-8m-units', 'controller-save-utf8-16mib', 'controller-save-utf8-bom-16mib', 'buffer-erase-scattered-head-4096', 'buffer-erase-scattered-middle-4096', 'buffer-erase-scattered-tail-4096', 'buffer-erase-scattered-all-4096', 'buffer-erase-single-middle-4096', 'buffer-offset-long-head-57344', 'buffer-offset-long-middle-57344', 'buffer-offset-long-end-57344', 'buffer-offset-scattered-middle-57344', 'search-forward-head-many-4096', 'search-forward-middle-many-4096', 'search-forward-tail-many-4096', 'search-backward-head-many-4096', 'search-backward-middle-many-4096', 'search-backward-tail-many-4096', 'pattern-star-miss-1024', 'pattern-star-miss-2048', 'pattern-star-miss-4096', 'pattern-multistar-miss-32', 'pattern-greedy-hit-4096', 'pattern-literal-tail-4102', 'pattern-literal-long-4096', 'frame-search-dense-ascii-4096', 'frame-search-dense-mixed-2340', 'frame-search-sparse-tail-16384', 'frame-selection-ascii-16384', 'frame-search-visual-ascii-4096', 'frame-rows-empty-64', 'frame-rows-short-64', 'frame-rows-30-64', 'frame-rows-120-64', 'search-snapshot-frame-short-64', 'search-snapshot-frame-long-64', 'search-snapshot-repeat-long-64', 'search-snapshot-retained-long-200', 'search-snapshot-retained-short-200', 'search-snapshot-unsearched-200', 'search-snapshot-commit-short-64', 'search-snapshot-typing-frame-short-64', 'preview-caret-long-30-64', 'preview-caret-long-120-64', 'preview-caret-short-30-64', 'preview-caret-confirmed-30-64', 'preview-caret-unsearched-30-64', 'preview-caret-absent-30-64', 'preview-caret-disabled-30-64', 'frame-document-short-saved-256', 'frame-document-long-saved-256', 'frame-document-long-failed-256', 'frame-document-untitled-256', 'status-items-utf8-crlf-1024', 'status-items-bom-lf-1024', 'status-items-sjis-crlf-1024', 'status-items-max-utf8-lf-1024', 'status-items-large-bom-crlf-1024', 'status-items-sjis-lf-1024', 'file-name-windows-4096', 'file-name-deep-ascii-4096', 'file-name-mixed-utf8-4096', 'file-name-bare-short-4096', 'file-name-bare-long-4096', 'file-name-trailing-4096', 'file-name-root-4096', 'tab-display-ordinary-saved-1024', 'tab-display-deep-saved-1024', 'tab-display-wide-saved-1024', 'tab-display-short-saved-1024', 'tab-display-exact-saved-1024', 'tab-display-modified-boundary-1024', 'tab-display-folder-wide-1024', 'tab-display-root-saved-1024', 'tab-display-untitled-modified-1024', 'command-input-search-long-1024', 'command-input-ex-long-1024', 'command-input-palette-long-1024', 'command-input-search-short-1024', 'command-input-ex-short-1024', 'command-input-palette-short-1024', 'command-input-ex-left-long-1024', 'command-input-palette-fill-long-1024', 'command-input-controller-search-64', 'command-input-controller-ex-64', 'command-input-controller-palette-64', 'command-input-controller-fill-64', 'ex-completion-option-1024', 'ex-completion-miss-1024', 'ex-completion-custom-narrow-128', 'ex-completion-empty-1024', 'ex-completion-set-wide-1024', 'ex-completion-theme-wide-1024', 'ex-completion-all-builtins-1024', 'ex-completion-custom-wide-128', 'ex-completion-all-custom-128', 'ex-completion-frame-ex-option-64', 'ex-completion-frame-ex-empty-64', 'ex-completion-frame-palette-64', 'completion-prefix-long-miss-4096', 'completion-prefix-theme-raw-4096', 'completion-prefix-theme-seeded-4096', 'completion-prefix-short-raw-4096', 'completion-prefix-short-seeded-4096', 'completion-prefix-empty-raw-4096', 'completion-prefix-empty-seeded-4096', 'palette-choice-builtin-all-512', 'palette-choice-custom-all-128', 'palette-choice-miss-1024', 'palette-choice-short-1024', 'palette-choice-theme-512', 'palette-choice-direct-fill-1024', 'palette-choice-direct-long-1024', 'palette-choice-fallback-long-512', 'palette-choice-custom-narrow-128')
        self.assertEqual(PROBES.WORKLOADS[:138], old)
        self.assertEqual(len(PROBES.WORKLOADS), 154)
        self.assertEqual(len(set(PROBES.WORKLOADS)), 154)

    def test_palette_choice_fixed_inputs(self) -> None:
        cases = (
            ('palette-choice-builtin-all-512', b'builtin-all\n:\n512\nbuiltins\n', '9e3c1cb60c6221c4cd0d53314f38f14f6f25d5f6fcdbd7307bab994eb09d1082', 27, '8889764623621042536'),
            ('palette-choice-custom-all-128', b'custom-all\n:\n128\ncustom256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n', '4b87a1cac39a536a1cc6377a570c703928c0fe235015308d03344e3d77fa9918', 2601, '6189637806432945297'),
            ('palette-choice-miss-1024', b'miss\n:?\n1024\nbuiltins\n', '977719b8b5745ab0d627ac44a35d3cc65cdcefb95a5b0795af18bb9d0c1fc8c0', 22, '12576992529045698783'),
            ('palette-choice-short-1024', b'short\n:fz\n1024\nbuiltins\n', '2ae6df3b9c904483d648d099c7bb268db0e0fd2ffc8171239a55d6132c26ff74', 24, '11234399941817021486'),
            ('palette-choice-theme-512', b'theme\n:DRAC\n512\nbuiltins\n', 'd8c904ad53d0e787700ac51243ea6e73b0eaa37cee6c513ea0d1e59f0650fb4e', 25, '7457777600508381066'),
            ('palette-choice-direct-fill-1024', b'direct-fill\n:set guifont=\n1024\nbuiltins\n', '9ba2cb0de2b177daf1a64caa8d4f4d8c80461dfee6c896d0c7c38123ae9e708b', 40, '17242868323109750012'),
            ('palette-choice-direct-long-1024', b'direct-long\n:set guifont=\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5AB:h18\n1024\nbuiltins\n', 'ea7bbad3e601ada2e9c7c94f0b21ebe2aa88df2b53163927eb454e1b1f935a5b', 283, '16321591902785174983'),
            ('palette-choice-fallback-long-512', b'fallback-long\n:colorscheme \xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\n512\nbuiltins\n', '804f057b22db15192e8c380bbe05ea5eeca16a66eafe7b305e044fb29a6b9448', 284, '16846957479756158044'),
            ('palette-choice-custom-narrow-128', b'custom-narrow\n:user-0123\n128\ncustom256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n', 'a0cb8513adfb3742abdcd3ebf0c443496c8376580733f093dc2d893074517e1f', 2613, '6082781093668288885'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[129:138])
        for name, expected, sha, size, fnv in cases:
            actual = PROBES.fixed_input(name)
            self.assertEqual(actual, expected)
            self.assertEqual(hashlib.sha256(actual).hexdigest(), sha)
            self.assertEqual(len(actual), size)
            self.assertEqual(str(PROBES.fnv1a64(actual)), fnv)

    def test_palette_choice_bad_name(self) -> None:
        for name in ("palette-choice-missing-512", "Palette-choice-builtin-all-512"):
            with self.assertRaises(ValueError):
                PROBES.fixed_input(name)

    def test_completion_prefix_fixed_inputs(self) -> None:
        cases = (
            ('completion-prefix-long-miss-4096', b'mode=raw\ninput=edit C:\\nib-probe\\\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5Z\nrounds=4096\ncatalog=builtins\n', 'b1a072b90ca451b1d1804fee1642d1fe4d38c968b02f8546fced78f3a99935af', 301, '9479249360357031464'),
            ('completion-prefix-theme-raw-4096', b'mode=raw\ninput=colorscheme drac\nrounds=4096\ncatalog=builtins\n', '501e45639b833cc660a29dcddaddfcfb5559bd448a5e9bcea1fa1da2618081b4', 61, '16180892351729709303'),
            ('completion-prefix-theme-seeded-4096', b'mode=tab-next\ninput=colorscheme drac\nrounds=4096\ncatalog=builtins\n', '0f1ae71c0d6a39f82af900f7aaa8c3eee932b84c254392d1f10dfbae9dd9f73f', 66, '2768231059722921498'),
            ('completion-prefix-short-raw-4096', b'mode=raw\ninput=set f\nrounds=4096\ncatalog=builtins\n', 'cbcd15e4deba80b85ea0bb203767b59e169a0a84af8a5a2edbb3b792654f05b1', 50, '613311958452090131'),
            ('completion-prefix-short-seeded-4096', b'mode=tab-next\ninput=set f\nrounds=4096\ncatalog=builtins\n', '3d0e82cbbb3121d756b46aeb27912f2a0afd5a97a0e0a8177c456f42c2609a68', 55, '12241856587300413340'),
            ('completion-prefix-empty-raw-4096', b'mode=raw\ninput=\nrounds=4096\ncatalog=builtins\n', '5d1255df7eccd0d785017a8ee76b53671a5352f98f6032c5e68aec8cb5986ba7', 45, '2206316838578377269'),
            ('completion-prefix-empty-seeded-4096', b'mode=tab-next\ninput=\nrounds=4096\ncatalog=builtins\n', 'fa7e8344e6771c325fdd5614057b15259cb393bbb15da5ff551abc2937baa4a7', 50, '12679685552769987680'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[122:129])
        for name, expected, sha, size, fnv in cases:
            actual = PROBES.fixed_input(name)
            self.assertEqual(actual, expected)
            self.assertEqual(hashlib.sha256(actual).hexdigest(), sha)
            self.assertEqual(len(actual), size)
            self.assertEqual(str(PROBES.fnv1a64(actual)), fnv)

    def test_completion_prefix_bad_name(self) -> None:
        for name in ("completion-prefix-missing-4096", "Completion-prefix-long-miss-4096"):
            with self.assertRaises(ValueError):
                PROBES.fixed_input(name)

    def test_ex_completion_fixed_inputs(self):
        cases = (
            ('ex-completion-option-1024', b'mode=prefix\nprefix=set f\ncatalog=builtins\n', 'ffe433056cd47975f9b83b23a9778fe49b9b130c4c81db5665b507a19fd35f42', 42, '2463955861418822004'),
            ('ex-completion-miss-1024', b'mode=prefix\nprefix=unknown\ncatalog=builtins\n', '49d93901aec669fb52c984a81aa541aea83d898711424ef1f40be1d9f32872b0', 44, '1528986535300989964'),
            ('ex-completion-custom-narrow-128', b'mode=prefix\nprefix=colorscheme user-0123\ncatalog=custom-256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n', '462b36af13620cdb820ee449f4bcbc0278cf4510b1947484ca4b26d71c82b8e4', 2634, '14198602422797423021'),
            ('ex-completion-empty-1024', b'mode=prefix\nprefix=\ncatalog=builtins\n', '1c364a7d92e5f5cbc06409d64e21399ca762b8ab3bf89efd7a041d92e6950aac', 37, '6476461437863874986'),
            ('ex-completion-set-wide-1024', b'mode=prefix\nprefix=set \ncatalog=builtins\n', '5bfe7f67187954e254fefba23848147b12cc56e45f264c16f5cc2c2cf378d9e6', 41, '3466819681304591034'),
            ('ex-completion-theme-wide-1024', b'mode=prefix\nprefix=colorscheme \ncatalog=builtins\n', 'e2b3f82f4670467d8876fe42bac7ce174bff347589cec96ff4cae041112edf20', 49, '6786850609777670638'),
            ('ex-completion-all-builtins-1024', b'mode=all\nprefix=\ncatalog=builtins\n', '60c0f91b0629903ba4a8bd8e8fe34b8f83a666a1028c8fe04a57503e99ffb4c1', 34, '6326771985681754643'),
            ('ex-completion-custom-wide-128', b'mode=prefix\nprefix=colorscheme \ncatalog=custom-256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n', 'b34653c7bc27b1dcfb7669f466801a871a673e634552f54ea85fca036f6049d6', 2625, '38645121121050355'),
            ('ex-completion-all-custom-128', b'mode=all\nprefix=\ncatalog=custom-256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n', '1babb770bbc7a26656b931db24dbd5d2a3620b387ce39f3e93b5d0f8332e6eca', 2610, '1878973266631997966'),
            ('ex-completion-frame-ex-option-64', b'mode=frame-ex\nprefix=set f\ncatalog=builtins\nbody=a x a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n', 'e8732e1ad5495a94f64f888c902046a59b36c5b40552f5d2b8414f908377106f', 109, '17432621004528936371'),
            ('ex-completion-frame-ex-empty-64', b'mode=frame-ex\nprefix=\ncatalog=builtins\nbody=a x a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n', '1ca3cde2aa3b4f22aee745678a6352cf0cf2520a3def555a0a8a24651e42fcab', 104, '17343572492446846795'),
            ('ex-completion-frame-palette-64', b'mode=frame-palette\nprefix=:fz\ncatalog=builtins\nbody=a x a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n', '02ec887240e69fecef8b9e58e3a2a97c9e57fa2eacd1da44bd6fcb49396bca47', 112, '12276415075478257975'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[110:122])
        for workload, data, sha, size, fnv in cases:
            with self.subTest(workload=workload):
                self.assertEqual(PROBES.fixed_input(workload), data)
                self.assertEqual(len(data), size)
                self.assertEqual(hashlib.sha256(data).hexdigest(), sha)
                self.assertEqual(PROBES.fnv1a64(data), fnv)

    def test_ex_completion_bad_name(self):
        for name in ("ex-completion-missing-1024", "Ex-completion-option-1024"):
            with self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_fixed_abba_preserves_every_sample(self):
        report, code = PROBES.compare(self.options, self.runner)
        self.assertEqual(code, 0)
        self.assertEqual([run["side"] for run in report["runs"]], ["before", "after", "after", "before"])
        self.assertEqual(report["summary"]["pairedRatiosAfterOverBefore"], [1.0] * 4)
        self.assertEqual([run["microseconds"] for run in report["runs"]], [[10, 10]] * 4)
        self.assertFalse(report["warmupMeasured"])
        self.assertNotEqual(report["executables"]["before"]["sha256"], report["executables"]["after"]["sha256"])
        with self.assertRaises(PROBES.ProbeNotObserved):
            PROBES.compare(self.options, self.runner)

    def test_one_side_failure_is_not_a_comparison(self):
        def failed(command, timeout):
            if Path(command[0]).name == "after.exe":
                return subprocess.CompletedProcess(command, 1, "", "wrong result")
            return self.runner(command, timeout)
        report, code = PROBES.compare(self.options, failed)
        self.assertEqual(code, 2)
        self.assertNotIn("summary", report)
        self.assertEqual(report["runs"][-1]["stderr"], "wrong result")
        self.assertEqual(len(report["plannedOrder"]), 4)

    def test_checksum_mismatch_is_not_a_comparison(self):
        def changed(command, timeout):
            result = self.runner(command, timeout)
            if Path(command[0]).name == "after.exe":
                data = json.loads(result.stdout)
                data["outputChecksum"] = "124"
                result.stdout = json.dumps(data)
            return result
        report, code = PROBES.compare(self.options, changed)
        self.assertEqual(code, 2)
        self.assertIn("checksum", report["failure"])
        self.assertNotIn("summary", report)

    def test_timeout_preserves_missing_trial(self):
        def timed_out(command, timeout):
            raise subprocess.TimeoutExpired(command, timeout)
        report, code = PROBES.compare(self.options, timed_out)
        self.assertEqual(code, 2)
        self.assertIn("TimeoutExpired", report["failure"])
        self.assertEqual(len(report["runs"]), 1)

    def test_broken_timing_reports_are_rejected(self):
        valid = [{"milestone": "probe_started", "qpcMicroseconds": 10},
                 {"milestone": "probe_finished", "qpcMicroseconds": 20}]
        cases = ([], {"qpcFrequency": 0, "marks": valid},
                 {"qpcFrequency": 1, "marks": valid[:1]},
                 {"qpcFrequency": 1, "marks": valid[::-1]},
                 {"qpcFrequency": 1, "marks": [valid[0], {**valid[1], "qpcMicroseconds": 10}]},
                 {"qpcFrequency": 1, "marks": [{**valid[0], "qpcMicroseconds": True}, valid[1]]})
        for report in cases:
            with self.subTest(report=report), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.samples_from(report, 1)
        self.assertEqual(PROBES.samples_from({"qpcFrequency": 1, "marks": valid}, 1), [10])

    def test_invalid_json_and_metadata_are_rejected(self):
        for text in ('{"marks":[],"marks":[]}', '{"value":NaN}', '[1,2]'):
            with self.subTest(text=text), self.assertRaises((PROBES.ProbeNotObserved, TypeError)):
                PROBES.metadata_from(text, {"workload": "display-line-long", "iterations": 2, "commit": COMMIT}, {})
        result = self.runner([str(self.options.before), self.options.workload, "2", str(self.folder / "marks.json")], 1.0)
        original = json.loads(result.stdout)
        expected = {key: original[key] for key in ("inputBytes", "inputHashAlgorithm", "inputHash")}
        for key, value in (("configuration", "Debug"), ("commit", "c" * 40), ("dirty", 1),
                           ("iterations", 3), ("inputHash", "0"), ("warmup", True)):
            with self.subTest(key=key), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.metadata_from(json.dumps({**original, key: value}),
                                     {"workload": self.options.workload, "iterations": 2, "commit": COMMIT}, expected)

    def test_fixed_deleted_input_sizes(self):
        self.assertEqual(len(PROBES.fixed_input("controller-insert-200-after-delete-1mib")), 1048572)
        self.assertEqual(len(PROBES.fixed_input("controller-insert-200-after-delete-16mib")), 16800000)

    def test_fixed_utf8_validation_counts(self):
        ascii_input = PROBES.fixed_input("utf8-validate-ascii-16mib")
        japanese = PROBES.fixed_input("utf8-validate-japanese-6mib")
        self.assertEqual(len(ascii_input), 16800000)
        self.assertEqual(len(ascii_input.decode("utf-8")), 16800000)
        self.assertEqual(len(japanese), 6219000)
        self.assertEqual(len(japanese.decode("utf-8")), 2079000)

    def test_stage_two_fixed_vim_inputs(self):
        cases = (
            ("controller-vim-insert-200-register-1mib", b"r", 1048576,
             "1f763ea478ec75459ed5b2b86463a21ebffe4c3ce8604d1e8c8ca7018f091ab1", "17443442650570171173"),
            ("controller-vim-insert-200-register-16mib", b"r", 16777216,
             "d578db1458827855ca402ad0a78c5bb8003d9302409d547f814f0a690c10dadd", "7866584971205550885"),
            ("controller-vim-record-insert-200", b"x", 200,
             "aa20c23e3201834050679e1d88941b9a6fed0557c9a705cb2c315e2e63fd486d", "7989751381043797061"),
            ("controller-vim-record-insert-2000", b"x", 2000,
             "5c0e0ea421571c300b5df6aec0a118b5c3dc02e0683a546341d5efc689df2f58", "5526198219355535973"))
        for workload, byte, count, sha256, fnv in cases:
            with self.subTest(workload=workload):
                self.assertIn(workload, PROBES.WORKLOADS)
                data = PROBES.fixed_input(workload)
                self.assertEqual(data, byte * count)
                self.assertEqual(len(data), count)
                self.assertEqual(hashlib.sha256(data).hexdigest(), sha256)
                self.assertEqual(PROBES.fnv1a64(data), fnv)


    def test_stage_five_pattern_inputs(self):
        cases = (("pattern-star-miss-1024", b"a" * 1024),
                 ("pattern-star-miss-2048", b"a" * 2048),
                 ("pattern-star-miss-4096", b"a" * 4096),
                 ("pattern-multistar-miss-32", b"a" * 32),
                 ("pattern-greedy-hit-4096", b"a" * 4096),
                 ("pattern-literal-tail-4102", b"x" * 4096 + b"needle"),
                 ("pattern-literal-long-4096", b"a" * 4096))
        self.assertEqual(tuple(name for name, _ in cases), PROBES.WORKLOADS[41:48])
        self.assertEqual(len(PROBES.WORKLOADS[:48]), 48)
        self.assertEqual(len(set(PROBES.WORKLOADS[:48])), 48)
        for name, expected in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
        for name in ("pattern-star-miss-1023", "pattern-literal-tail-4101", "pattern-greedy-hit-4095"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_stage_six_frame_inputs(self):
        # 1 行は巨大な行のしきい値 16384 以下（#690）。
        self.assertEqual(PROBES.giant_line_threshold(), 16384)
        cases = (("frame-search-dense-ascii-4096", b"a x " * 4096, "6455779677883867941"),
                 ("frame-search-dense-mixed-2340", "日a\t🖋\x01 ".encode("utf-8") * 2340,
                  "6798451794477431505"),
                 ("frame-search-sparse-tail-16384", b"x" * 16383 + b"a", "8594458239011605050"),
                 ("frame-selection-ascii-16384", b"a x " * 4096, "6455779677883867941"),
                 ("frame-search-visual-ascii-4096", b"a x " * 4096, "6455779677883867941"))
        self.assertEqual(tuple(name for name, _, _ in cases), PROBES.WORKLOADS[48:53])
        self.assertEqual(len(PROBES.WORKLOADS[:53]), 53)
        self.assertEqual(len(set(PROBES.WORKLOADS[:53])), 53)
        for name, expected, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)
        for name in ("frame-search-dense-ascii-8192", "frame-search-sparse-tail-32768"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_frame_rows_inputs_and_registry(self):
        unit = bytes.fromhex("61 e6 97 a5 09 f0 9f 96 8b 01 20 72 6f 77")
        cases = (("frame-rows-empty-64", b"", 0, "14695981039346656037"),
                 ("frame-rows-short-64", unit + b"\r\ntail", 20, "17662855059157275171"),
                 ("frame-rows-30-64", (unit + b"\r\n") * 119 + b"tail", 1908,
                  "11632098944160318499"),
                 ("frame-rows-120-64", (unit + b"\r\n") * 119 + b"tail", 1908,
                  "11632098944160318499"))
        self.assertEqual(tuple(name for name, _, _, _ in cases), PROBES.WORKLOADS[53:57])
        self.assertEqual(len(PROBES.WORKLOADS[:57]), 57)
        self.assertEqual(len(set(PROBES.WORKLOADS[:57])), 57)
        for name, expected, size, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)

    def test_command_input_inputs_and_registry(self):
        cases = (
            ('command-input-search-long-1024', b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 240, '10285162318351609365', '9b3043905ca79556acb1d2d12f9a4c0bed8ced2bd1a82e7cdf26743069be53e3'),
            ('command-input-ex-long-1024', b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 240, '10285162318351609365', '9b3043905ca79556acb1d2d12f9a4c0bed8ced2bd1a82e7cdf26743069be53e3'),
            ('command-input-palette-long-1024', b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 240, '10285162318351609365', '9b3043905ca79556acb1d2d12f9a4c0bed8ced2bd1a82e7cdf26743069be53e3'),
            ('command-input-search-short-1024', b'a', 1, '12638187200555641996', 'ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb'),
            ('command-input-ex-short-1024', b'a', 1, '12638187200555641996', 'ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb'),
            ('command-input-palette-short-1024', b'a', 1, '12638187200555641996', 'ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb'),
            ('command-input-ex-left-long-1024', b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\xf0\x9f\x96\x8b', 244, '14968357790484435179', 'a2e3393a196799747e1c6d91151ec79b38d2a00b72ab41421c934ac08627132b'),
            ('command-input-palette-fill-long-1024', b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 240, '10285162318351609365', '9b3043905ca79556acb1d2d12f9a4c0bed8ced2bd1a82e7cdf26743069be53e3'),
            ('command-input-controller-search-64', b'a', 1, '12638187200555641996', 'ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb'),
            ('command-input-controller-ex-64', b'a', 1, '12638187200555641996', 'ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb'),
            ('command-input-controller-palette-64', b'a', 1, '12638187200555641996', 'ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb'),
            ('command-input-controller-fill-64', b':fz', 3, '11941784898562763057', 'd58613416642baf2b9a49d9ee7107e85ac886af97e0086632f0892ef85fc331b'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[98:110])
        self.assertEqual(len(PROBES.WORKLOADS), 154)
        self.assertEqual(len(set(PROBES.WORKLOADS)), 154)
        for name, expected, size, fnv, sha in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)
                self.assertEqual(hashlib.sha256(expected).hexdigest(), sha)

    def test_command_input_invalid_names(self):
        for name in ("command-input-search-long-1023", "command-input-unknown-1024",
                     "command-input-controller-fill-63"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_tab_display_inputs_and_registry(self):
        cases = (
            ('tab-display-ordinary-saved-1024', b'C:\\Users\\hide\\Documents\\NeNeNib\\projects\\editor\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt', 116, '7839206126360133014'),
            ('tab-display-deep-saved-1024', b'C:\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\note.txt', 1035, '14257691521225177198'),
            ('tab-display-wide-saved-1024', b'C:\\work\\\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e.txt', 588, '9536536151321469209'),
            ('tab-display-short-saved-1024', b'C:\\n.txt', 8, '16743635261450646272'),
            ('tab-display-exact-saved-1024', b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 256, '18242136092491110437'),
            ('tab-display-modified-boundary-1024', b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\xf0\x9f\x96\x8b.txt', 255, '6578936846377594134'),
            ('tab-display-folder-wide-1024', b'C:/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/note.txt', 459, '918048000425786379'),
            ('tab-display-root-saved-1024', b'C:\\', 3, '909794305184803180'),
            ('tab-display-untitled-modified-1024', b'', 0, '14695981039346656037'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[89:98])
        self.assertEqual(len(PROBES.WORKLOADS), 154)
        self.assertEqual(len(set(PROBES.WORKLOADS)), 154)
        for name, expected, size, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)

    def test_tab_display_invalid_names(self):
        for name in ("tab-display-short-saved-1023", "tab-display-unknown-1024"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_file_name_inputs_and_registry(self):
        cases = (
            ('file-name-windows-4096', b'C:\\Users\\hide\\Documents\\NeNeNib\\projects\\editor\\note.txt', 56, '7860286326920136938'),
            ('file-name-deep-ascii-4096', b'C:\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\note.txt', 1035, '14257691521225177198'),
            ('file-name-mixed-utf8-4096', b'C:/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe8\xb3\x87\xe6\x96\x99\\\xe9\x9a\x8e\xe5\xb1\xa4/\xe6\x97\xa5\xe8\xaa\x8c\xf0\x9f\x96\x8b.txt', 913, '3805312051846199945'),
            ('file-name-bare-short-4096', b'note.txt', 8, '1116535845637733659'),
            ('file-name-bare-long-4096', b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', 1024, '18395275555644700453'),
            ('file-name-trailing-4096', b'C:\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\', 1027, '2593822827728123500'),
            ('file-name-root-4096', b'/', 1, '12638123428881205758'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[82:89])
        self.assertEqual(len(PROBES.WORKLOADS), 154)
        self.assertEqual(len(set(PROBES.WORKLOADS)), 154)
        for name, expected, size, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)

    def test_file_name_invalid_names(self):
        for name in ("file-name-windows-4095", "file-name-unknown-4096"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_status_items_inputs_and_registry(self):
        cases = (
            ('status-items-utf8-crlf-1024', b'UTF-8|CRLF|1|1', 14, '2425243683614802670'),
            ('status-items-bom-lf-1024', b'UTF-8 BOM|LF|9|24', 17, '1768796593162922628'),
            ('status-items-sjis-crlf-1024', b'Shift_JIS|CRLF|123|456', 22, '8885631928267894942'),
            ('status-items-max-utf8-lf-1024', b'UTF-8|LF|18446744073709551615|18446744073709551615', 50, '13581726349480759485'),
            ('status-items-large-bom-crlf-1024', b'UTF-8 BOM|CRLF|123456|654321', 28, '7783354242304457080'),
            ('status-items-sjis-lf-1024', b'Shift_JIS|LF|1|1', 16, '1943232094207065852'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[76:82])
        self.assertEqual(len(PROBES.WORKLOADS[:82]), 82)
        self.assertEqual(len(set(PROBES.WORKLOADS[:82])), 82)
        for name, expected, size, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)

    def test_status_items_invalid_names(self):
        for name in ("status-items-utf8-crlf-1023", "status-items-unknown-1024"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_frame_document_inputs_and_registry(self):
        cases = (
            ('frame-document-short-saved-256', b'C:\\nib-probe\\frame-document.txt', 31, '3165114504819470625'),
            ('frame-document-long-saved-256', b'C:\\nib-probe\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt', 241, '3259355916674798828'),
            ('frame-document-long-failed-256', b'C:\\nib-probe\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt', 241, '3259355916674798828'),
            ('frame-document-untitled-256', b'', 0, '14695981039346656037'),
        )
        self.assertEqual(tuple(row[0] for row in cases), PROBES.WORKLOADS[72:76])
        self.assertEqual(len(PROBES.WORKLOADS[:76]), 76)
        self.assertEqual(len(set(PROBES.WORKLOADS[:76])), 76)
        for name, expected, size, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)

    def test_frame_document_invalid_names(self):
        for name in ("frame-document-long-saved-255", "frame-document-short-saved-64",
                     "frame-document-unknown-256"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_preview_caret_inputs_and_registry(self):
        cases = (
            ("preview-caret-long-30-64", 3674, "3943064766360993406"),
            ("preview-caret-long-120-64", 3674, "3943064766360993406"),
            ("preview-caret-short-30-64", 605, "5659432768607348982"),
            ("preview-caret-confirmed-30-64", 3674, "3943064766360993406"),
            ("preview-caret-unsearched-30-64", 3674, "3943064766360993406"),
            ("preview-caret-absent-30-64", 3674, "3943064766360993406"),
            ("preview-caret-disabled-30-64", 3674, "3943064766360993406"),
        )
        self.assertEqual(tuple(name for name, _, _ in cases), PROBES.WORKLOADS[65:72])
        self.assertEqual(len(PROBES.WORKLOADS[:72]), 72)
        self.assertEqual(len(set(PROBES.WORKLOADS[:72])), 72)
        for name, size, fnv in cases:
            count = 1 if name == "preview-caret-short-30-64" else 1024
            expected = bytes.fromhex("e6 97 a5") * count + bytes.fromhex("7a 09 f0 9f 96 8b 01")
            expected += b"\r\nz x" * 119
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(len(expected), size)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)

    def test_preview_caret_invalid_names(self):
        for name in ("preview-caret-long-31-64", "preview-caret-short-30-63",
                     "preview-caret-disabled-30-65"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_search_snapshot_inputs_and_registry(self):
        cases = (
            ("search-snapshot-frame-short-64", 1, "12638187200555641996"),
            ("search-snapshot-frame-long-64", 256, "18242136092491110437"),
            ("search-snapshot-repeat-long-64", 256, "18242136092491110437"),
            ("search-snapshot-retained-long-200", 256, "18242136092491110437"),
            ("search-snapshot-retained-short-200", 1, "12638187200555641996"),
            ("search-snapshot-unsearched-200", 0, "14695981039346656037"),
            ("search-snapshot-commit-short-64", 1, "12638187200555641996"),
            ("search-snapshot-typing-frame-short-64", 1, "12638187200555641996"),
        )
        self.assertEqual(tuple(name for name, _, _ in cases), PROBES.WORKLOADS[57:65])
        self.assertEqual(len(PROBES.WORKLOADS[:65]), 65)
        self.assertEqual(len(set(PROBES.WORKLOADS[:65])), 65)
        for name, size, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), b"a" * size)
                self.assertEqual(PROBES.fnv1a64(b"a" * size), fnv)

    def test_search_snapshot_invalid_names(self):
        for name in ("search-snapshot-frame-long-63", "search-snapshot-retained-long-201",
                     "search-snapshot-typing-frame-short-65"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_frame_rows_invalid_names(self):
        for name in ("frame-rows-empty-63", "frame-rows-short-65", "frame-rows-31-64",
                     "frame-rows-121-64"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_stage_four_registry_and_fixed_inputs(self):
        self.assertEqual(len(PROBES.WORKLOADS[:41]), 41)
        self.assertEqual(len(set(PROBES.WORKLOADS[:41])), 41)
        groups = ((PROBES.WORKLOADS[24:26], PROBES.fixed_input("controller-open-utf8-16mib"), 16800000),
                  (PROBES.WORKLOADS[26:31], (b"a" * 78 + b"\r\n") * 4096, 327680),
                  (PROBES.WORKLOADS[31:35], "a日本語🖋".encode("utf-8") * 4096, 57344),
                  (PROBES.WORKLOADS[35:41], "a日本語🖋 ".encode("utf-8") * 4096, 61440))
        for names, expected, size in groups:
            for name in names:
                with self.subTest(workload=name):
                    data = PROBES.fixed_input(name)
                    self.assertEqual(data, expected)
                    self.assertEqual(len(data), size)
        for name in ("buffer-erase-scattered-head-4095", "buffer-offset-long-head-57343",
                     "search-forward-head-many-4095", "controller-save-utf8-16mb"):
            with self.subTest(workload=name), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(name)

    def test_stage_three_registry_and_unknown_names(self):
        self.assertEqual(len(PROBES.WORKLOADS[:24]), 24)
        self.assertEqual(len(set(PROBES.WORKLOADS[:24])), 24)
        for workload in ("buffer-line-text-scattered-crlf-4095", "palette-listed-name-4999", "unknown"):
            with self.subTest(workload=workload), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.fixed_input(workload)

    def test_stage_three_buffer_inputs(self):
        crlf = PROBES.fixed_input("buffer-line-text-scattered-crlf-4096")
        lf = PROBES.fixed_input("buffer-line-text-scattered-lf-4096")
        self.assertEqual(crlf, (b"a" * 78 + b"\r\n") * 4096)
        self.assertEqual(lf, (b"a" * 78 + b"\n") * 4096)
        self.assertEqual((len(crlf), len(lf)), (327680, 323584))
        position = PROBES.fixed_input("buffer-position-long-utf8-57344")
        self.assertEqual(position, PROBES.fixed_input("buffer-position-scattered-utf8-57344"))
        self.assertEqual(position.decode("utf-8"), "a日本語🖋" * 4096)
        self.assertEqual((len(position), len(position.decode("utf-8"))), (57344, 20480))

    def test_stage_three_palette_serialization(self):
        for workload, query in (("palette-listed-name-5000", "entry"),
                                ("palette-listed-location-5000", "zroot")):
            rows = PROBES.fixed_input(workload).decode("utf-8").splitlines()
            self.assertEqual(rows[0], f"files\t{query}")
            self.assertEqual(len(rows), 5001)
            for index, row in enumerate(rows[1:]):
                self.assertEqual(row.split("\t"), ["folder", "open", f"probe-{index:05d}",
                    f"ENTRY_{index:05d}_ABCDEFGHIJKLMNOPQRSTUV.txt",
                    r"D:\ZROOT\LONG_DIRECTORY_COMPONENT\GROUP_00"])
                self.assertNotIn("Z", row.split("\t")[3])
        data = PROBES.fixed_input("palette-append-narrow-5000-to-50")
        self.assertEqual(data, PROBES.fixed_input("palette-caret-left-5000"))
        rows = data.decode("utf-8").splitlines()
        self.assertEqual(rows[0], "files\tqx")
        labels = [row.split("\t")[3] for row in rows[1:]]
        self.assertEqual(sum(label.startswith("QX_") for label in labels), 500)
        self.assertEqual(sum(label.startswith("QX_Y_") for label in labels), 50)
        self.assertEqual([row.split("\t")[2] for row in rows[1:]],
                         [f"probe-{index:05d}" for index in range(5000)])
        self.assertTrue(all(row.endswith("\t") for row in rows[1:]))

    def test_stage_three_conversion_bytes_and_units(self):
        cases = (("codepage-to-utf8-cp932-japanese-16mib", "cp932", "日本" * 4194304),
                 ("utf16-to-utf8-japanese-8m-units", "utf-16-le", "日本" * 4194304),
                 ("utf16-to-utf8-ascii-8m-units", "utf-16-le", "a" * 8388608),
                 ("utf16-to-utf8-supplementary-8m-units", "utf-16-le", "🖋" * 4194304))
        for workload, encoding, expected in cases:
            with self.subTest(workload=workload):
                data = PROBES.fixed_input(workload)
                self.assertEqual(len(data), 16777216)
                self.assertEqual(data.decode(encoding), expected)

    def test_stage_three_metadata_uses_actual_serialized_input(self):
        self.options.workload = "palette-caret-left-5000"
        result = self.runner([str(self.options.before), self.options.workload, "2",
                              str(self.folder / "new-marks.json")], 1.0)
        data = PROBES.fixed_input(self.options.workload)
        expected = {"inputBytes": len(data), "inputHashAlgorithm": "fnv1a64",
                    "inputHash": PROBES.fnv1a64(data)}
        request = {"workload": self.options.workload, "iterations": 2, "commit": COMMIT}
        self.assertEqual(PROBES.metadata_from(result.stdout, request, expected)["inputBytes"], len(data))
        metadata = json.loads(result.stdout)
        for key, value in (("inputBytes", len(data) + 1), ("inputHash", "0"),
                           ("workload", "palette-listed-name-5000")):
            with self.subTest(key=key), self.assertRaises(PROBES.ProbeNotObserved):
                PROBES.metadata_from(json.dumps({**metadata, key: value}), request, expected)


if __name__ == "__main__":
    unittest.main()
