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

    def test_completion_prefix_fixed_inputs(self) -> None:
        cases = (
            ('completion-prefix-long-miss-4096', b'mode=raw\ninput=edit C:\\nib-probe\\\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5Z\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', 'cb386e0d366e4241d2a28fc2cf93fea60ee05cfa9d9611f5f0a594f1ca72c439', 414, '17899993968014254022'),
            ('completion-prefix-theme-raw-4096', b'mode=raw\ninput=colorscheme drac\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', '405dcf07b55a336d216344f812f97831d6b00c31a29253547523f891983e519e', 174, '6355724148303210051'),
            ('completion-prefix-theme-seeded-4096', b'mode=tab-next\ninput=colorscheme drac\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', 'dbc1247a7a1ea0ed7cfddf74837d094d824d336e479405b10a823c1dd6228ac4', 179, '9157927312824126276'),
            ('completion-prefix-short-raw-4096', b'mode=raw\ninput=set f\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', '1cf960f7f144aae7ec776ca5e0933d505443827b64242fa43a13ba3322f9c1ab', 163, '5146725973529891191'),
            ('completion-prefix-short-seeded-4096', b'mode=tab-next\ninput=set f\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', '509e2b051917c692b26ff795f7c5a599ce3192e4343e4798aae2309e97021c5c', 168, '4940635072069788562'),
            ('completion-prefix-empty-raw-4096', b'mode=raw\ninput=\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', 'd27b11e75b76da09bb146accc8d6fb8bbf5c170e286cefee3169396f031f90ed', 158, '7231983662249126709'),
            ('completion-prefix-empty-seeded-4096', b'mode=tab-next\ninput=\nrounds=4096\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', '4bfaa8c17714520ba3eaa11d8fb9205defeff22f215f19afcffd5c96cfb05bf8', 163, '16657912587326077742'),
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
            ('ex-completion-option-1024', b'mode=prefix\nprefix=set f\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', 'b9355144eb7c0346bbfc5509d1e7fc9c08e30e4e900de239f853818a06bbcb38', 155, '4958441773274844346'),
            ('ex-completion-miss-1024', b'mode=prefix\nprefix=unknown\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', 'c997ec2db881a604efcec5c8c2fadedf5be16c9fa146900c1bc26046c2aea192', 157, '6699480406065360290'),
            ('ex-completion-custom-narrow-128', b'mode=prefix\nprefix=colorscheme user-0123\ncatalog=custom-256-invalid-color\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n', '4e8c4e744af0b1cb6ee4dfefb7d55a59c583b721e7d640b8eb2a8c7efbbba953', 2747, '7858797942040090021'),
            ('ex-completion-empty-1024', b'mode=prefix\nprefix=\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', 'b397c422e1bac49241a296faea16e7bd420e01a85d47862035c658fe7fb245ca', 150, '18242349949620908404'),
            ('ex-completion-set-wide-1024', b'mode=prefix\nprefix=set \ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', 'b87e960c95988ac7b4fadc0c1b34a7bedd7f4be3fedc586d7f3bb8b11cf05052', 154, '9553897328609134628'),
            ('ex-completion-theme-wide-1024', b'mode=prefix\nprefix=colorscheme \ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', 'f105b3fed2e968ae2c3a00bdeb6ee09e73a97c4aea347392e46ad7d6c3bc45bd', 162, '1449422857361388192'),
            ('ex-completion-all-builtins-1024', b'mode=all\nprefix=\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\n', '75e492c74cdd1b9963e810f674ce564c8ebdf48d264a40dd6cb3343c388947b3', 147, '9382821741934878839'),
            ('ex-completion-custom-wide-128', b'mode=prefix\nprefix=colorscheme \ncatalog=custom-256-invalid-color\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n', '79d46e8c1eef15d17b55efdc0f4272a4c5bbddd528d1bd80a4a1aa932e58e4d1', 2738, '9480571665409447711'),
            ('ex-completion-all-custom-128', b'mode=all\nprefix=\ncatalog=custom-256-invalid-color\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n', 'f617e87e10d68d566eed2808a30793706267f74117554fa89c15ab074689c362', 2723, '7981154346645823048'),
            ('ex-completion-frame-ex-option-64', b'mode=frame-ex\nprefix=set f\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\nbody=a x a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n', '9f759f4cdcb26f13e75bd97dcf70a726ecd131c2e6e4671143bf02fed6a44c52', 222, '1915732327603549999'),
            ('ex-completion-frame-ex-empty-64', b'mode=frame-ex\nprefix=\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\nbody=a x a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n', '6fcd260a7e48b9a5921e18dffa88602286e6a2dd100f8e51c118f5d43d9f65aa', 217, '14254370962528029111'),
            ('ex-completion-frame-palette-64', b'mode=frame-palette\nprefix=:fz\ncatalog=builtins\nubuntu-aubergine\nneutral-light\nsolarized-dark\nsolarized-light\nmonokai\ndracula\none-dark\nnight-owl\nnight-owl-light\nbody=a x a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n', 'e3859d3d5f1112fa89fa777cbbe7f45a1c5b65b6470effb80735265f02f12a25', 225, '4426202880856066747'),
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
        cases = (("frame-search-dense-ascii-8192", b"a x " * 8192, "2381364893216809765"),
                 ("frame-search-dense-mixed-4096", "日a\t🖋\x01 ".encode("utf-8") * 4096,
                  "10068659747010089765"),
                 ("frame-search-sparse-tail-32768", b"x" * 32767 + b"a", "6746773668288547386"),
                 ("frame-selection-ascii-32768", b"a x " * 8192, "2381364893216809765"),
                 ("frame-search-visual-ascii-8192", b"a x " * 8192, "2381364893216809765"))
        self.assertEqual(tuple(name for name, _, _ in cases), PROBES.WORKLOADS[48:53])
        self.assertEqual(len(PROBES.WORKLOADS[:53]), 53)
        self.assertEqual(len(set(PROBES.WORKLOADS[:53])), 53)
        for name, expected, fnv in cases:
            with self.subTest(workload=name):
                self.assertEqual(PROBES.fixed_input(name), expected)
                self.assertEqual(PROBES.fnv1a64(expected), fnv)
        for name in ("frame-search-dense-ascii-8191", "frame-search-sparse-tail-32767"):
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
        self.assertEqual(len(PROBES.WORKLOADS), 129)
        self.assertEqual(len(set(PROBES.WORKLOADS)), 129)
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
        self.assertEqual(len(PROBES.WORKLOADS), 129)
        self.assertEqual(len(set(PROBES.WORKLOADS)), 129)
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
        self.assertEqual(len(PROBES.WORKLOADS), 129)
        self.assertEqual(len(set(PROBES.WORKLOADS)), 129)
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
