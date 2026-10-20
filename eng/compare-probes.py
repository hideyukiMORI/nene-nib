"""Compare the same window-free workloads in a fixed ABBA order (ADR 0082 / #329)."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import statistics
import subprocess
import sys

WORKLOADS = ("controller-open-utf8-16mib", "buffer-from-utf8-16mib",
             "controller-insert-200", "display-line-long",
             "controller-insert-200-after-delete-1mib", "controller-insert-200-after-delete-16mib",
             "utf8-validate-ascii-16mib", "utf8-validate-japanese-6mib",
             "controller-vim-insert-200-register-1mib", "controller-vim-insert-200-register-16mib",
             "controller-vim-record-insert-200", "controller-vim-record-insert-2000",
             "buffer-line-text-scattered-crlf-4096",
             "buffer-line-text-scattered-lf-4096",
             "buffer-position-long-utf8-57344",
             "buffer-position-scattered-utf8-57344",
             "palette-listed-name-5000",
             "palette-listed-location-5000",
             "palette-append-narrow-5000-to-50",
             "palette-caret-left-5000",
             "codepage-to-utf8-cp932-japanese-16mib",
             "utf16-to-utf8-japanese-8m-units",
             "utf16-to-utf8-ascii-8m-units",
             "utf16-to-utf8-supplementary-8m-units",
             "controller-save-utf8-16mib",
             "controller-save-utf8-bom-16mib",
             "buffer-erase-scattered-head-4096",
             "buffer-erase-scattered-middle-4096",
             "buffer-erase-scattered-tail-4096",
             "buffer-erase-scattered-all-4096",
             "buffer-erase-single-middle-4096",
             "buffer-offset-long-head-57344",
             "buffer-offset-long-middle-57344",
             "buffer-offset-long-end-57344",
             "buffer-offset-scattered-middle-57344",
             "search-forward-head-many-4096",
             "search-forward-middle-many-4096",
             "search-forward-tail-many-4096",
             "search-backward-head-many-4096",
             "search-backward-middle-many-4096",
             "search-backward-tail-many-4096",
             "pattern-star-miss-1024",
             "pattern-star-miss-2048",
             "pattern-star-miss-4096",
             "pattern-multistar-miss-32",
             "pattern-greedy-hit-4096",
             "pattern-literal-tail-4102",
             "pattern-literal-long-4096",
             "frame-search-dense-ascii-4096",
             "frame-search-dense-mixed-2340",
             "frame-search-sparse-tail-16384",
             "frame-selection-ascii-16384",
             "frame-search-visual-ascii-4096",
             "frame-rows-empty-64", "frame-rows-short-64",
             "frame-rows-30-64", "frame-rows-120-64",
             "search-snapshot-frame-short-64",
             "search-snapshot-frame-long-64",
             "search-snapshot-repeat-long-64",
             "search-snapshot-retained-long-200",
             "search-snapshot-retained-short-200",
             "search-snapshot-unsearched-200",
             "search-snapshot-commit-short-64",
             "search-snapshot-typing-frame-short-64",
             "preview-caret-long-30-64",
             "preview-caret-long-120-64",
             "preview-caret-short-30-64",
             "preview-caret-confirmed-30-64",
             "preview-caret-unsearched-30-64",
             "preview-caret-absent-30-64",
             "preview-caret-disabled-30-64",
             "frame-document-short-saved-256",
             "frame-document-long-saved-256",
             "frame-document-long-failed-256",
             "frame-document-untitled-256",
             "status-items-utf8-crlf-1024",
             "status-items-bom-lf-1024",
             "status-items-sjis-crlf-1024",
             "status-items-max-utf8-lf-1024",
             "status-items-large-bom-crlf-1024",
             "status-items-sjis-lf-1024",
             "file-name-windows-4096",
             "file-name-deep-ascii-4096",
             "file-name-mixed-utf8-4096",
             "file-name-bare-short-4096",
             "file-name-bare-long-4096",
             "file-name-trailing-4096",
             "file-name-root-4096",
             "tab-display-ordinary-saved-1024",
             "tab-display-deep-saved-1024",
             "tab-display-wide-saved-1024",
             "tab-display-short-saved-1024",
             "tab-display-exact-saved-1024",
             "tab-display-modified-boundary-1024",
             "tab-display-folder-wide-1024",
             "tab-display-root-saved-1024",
             "tab-display-untitled-modified-1024",
             'command-input-search-long-1024',
             'command-input-ex-long-1024',
             'command-input-palette-long-1024',
             'command-input-search-short-1024',
             'command-input-ex-short-1024',
             'command-input-palette-short-1024',
             'command-input-ex-left-long-1024',
             'command-input-palette-fill-long-1024',
             'command-input-controller-search-64',
             'command-input-controller-ex-64',
             'command-input-controller-palette-64',
             'command-input-controller-fill-64',
             'ex-completion-option-1024',
             'ex-completion-miss-1024',
             'ex-completion-custom-narrow-128',
             'ex-completion-empty-1024',
             'ex-completion-set-wide-1024',
             'ex-completion-theme-wide-1024',
             'ex-completion-all-builtins-1024',
             'ex-completion-custom-wide-128',
             'ex-completion-all-custom-128',
             'ex-completion-frame-ex-option-64',
             'ex-completion-frame-ex-empty-64',
             'ex-completion-frame-palette-64',
             'completion-prefix-long-miss-4096',
             'completion-prefix-theme-raw-4096',
             'completion-prefix-theme-seeded-4096',
             'completion-prefix-short-raw-4096',
             'completion-prefix-short-seeded-4096',
             'completion-prefix-empty-raw-4096',
             'completion-prefix-empty-seeded-4096',
             'palette-choice-builtin-all-512',
             'palette-choice-custom-all-128',
             'palette-choice-miss-1024',
             'palette-choice-short-1024',
             'palette-choice-theme-512',
             'palette-choice-direct-fill-1024',
             'palette-choice-direct-long-1024',
             'palette-choice-fallback-long-512',
             'palette-choice-custom-narrow-128',
             'operation-choice-reading-ordinary-1024',
             'operation-choice-miss-ordinary-1024',
             'operation-choice-all-ordinary-256',
             'operation-choice-all-vim-256',
             'operation-choice-space-ordinary-256',
             'operation-choice-key-vim-1024',
             'operation-choice-name-ordinary-1024',
             'operation-choice-close-key-vim-1024',
             'operation-choice-description-miss-1024',
             'operation-choice-short-name-miss-1024',
             'text-object-sentence-giant-1line-16384',
             'text-object-sentence-dense-4096',
             'text-object-tag-nested-4096',
             'text-object-tag-unclosed-4096')
ORDER = ("before", "after", "after", "before")


def giant_line_threshold() -> int:
    """frame-* の 1 行の長さ。正本 src/core/GiantLine.hpp の giant_line_threshold を読む（#690）。"""
    header = Path(__file__).resolve().parent.parent / "src" / "core" / "GiantLine.hpp"
    match = re.search(r"giant_line_threshold = ([0-9']+);", header.read_text(encoding="utf-8"))
    if match is None:
        raise ProbeNotObserved("giant_line_threshold is not found in src/core/GiantLine.hpp")
    return int(match.group(1).replace("'", ""))


class ProbeNotObserved(ValueError):
    """The requested work has no complete, trustworthy pair of observations."""


def unique_object(pairs: list) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ProbeNotObserved(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(text: str):
    def reject_constant(value):
        raise ProbeNotObserved(f"nonfinite JSON constant: {value}")
    return json.loads(text, object_pairs_hook=unique_object, parse_constant=reject_constant)


def cycled_words(length: int) -> str:
    words = "lorem ipsum dolor sit amet "
    return (words * (length // len(words) + 1))[:length]


def text_object_input(workload: str) -> bytes:
    """文とタグのテキストオブジェクトの 4 本（#732）。巨大な行の長さは正本のしきい値から引く。"""
    if workload == "text-object-sentence-giant-1line-16384":
        return (cycled_words(giant_line_threshold())[:-1] + "z").encode("utf-8")
    if workload == "text-object-sentence-dense-4096":
        return ((cycled_words(48) + ". ") * 82)[:4096].encode("utf-8")
    if workload == "text-object-tag-nested-4096":
        return ("<a>y" * 200 + "x" * 2496 + "</a>" * 200).encode("utf-8")
    return ("<a>" * 500 + "x" * 2596).encode("utf-8")


def fixed_input(workload: str) -> bytes:
    if workload in WORKLOADS[148:152]:
        return text_object_input(workload)
    if workload in WORKLOADS[138:148]:
        return (
            b'operation-choice-v1\nreading-ordinary\nordinary\n\xe3\x81\xbb\xe3\x81\x9e\xe3\x82\x93\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
            b'operation-choice-v1\nmiss-ordinary\nordinary\nqqq\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
            b'operation-choice-v1\nall-ordinary\nordinary\n\n256\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
            b'operation-choice-v1\nall-vim\nvim\n\n256\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
            b'operation-choice-v1\nspace-ordinary\nordinary\n \n256\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
            b'operation-choice-v1\nkey-vim\nvim\ncTrL+s\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
            b'operation-choice-v1\nname-ordinary\nordinary\n\xe3\x82\xbf\xe3\x83\x96\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
            b'operation-choice-v1\nclose-key-vim\nvim\nCTRL+F4\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
            b'operation-choice-v1\ndescription-miss\nordinary\n\xe4\xb8\x8a\xe6\x9b\xb8\xe3\x81\x8d\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
            b'operation-choice-v1\nshort-name-miss\nordinary\n\xe3\x83\x98\xe3\x83\xab\xe3\x83\x97\n1024\n55b30a1557204a36ed235a4349521f2742ab0a887bf0f6765a358f0f68820d98\n',
        )[WORKLOADS.index(workload) - 138]
    if workload in WORKLOADS[129:138]:
        return (
            b'builtin-all\n:\n512\nbuiltins\n',
            b'custom-all\n:\n128\ncustom256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n',
            b'miss\n:?\n1024\nbuiltins\n',
            b'short\n:fz\n1024\nbuiltins\n',
            b'theme\n:DRAC\n512\nbuiltins\n',
            b'direct-fill\n:set guifont=\n1024\nbuiltins\n',
            b'direct-long\n:set guifont=\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5AB:h18\n1024\nbuiltins\n',
            b'fallback-long\n:colorscheme \xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\n512\nbuiltins\n',
            b'custom-narrow\n:user-0123\n128\ncustom256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n',
        )[WORKLOADS.index(workload) - 129]
    if workload in WORKLOADS[122:129]:
        return (
            b'mode=raw\ninput=edit C:\\nib-probe\\\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5\xe6\x97\xa5Z\nrounds=4096\ncatalog=builtins\n',
            b'mode=raw\ninput=colorscheme drac\nrounds=4096\ncatalog=builtins\n',
            b'mode=tab-next\ninput=colorscheme drac\nrounds=4096\ncatalog=builtins\n',
            b'mode=raw\ninput=set f\nrounds=4096\ncatalog=builtins\n',
            b'mode=tab-next\ninput=set f\nrounds=4096\ncatalog=builtins\n',
            b'mode=raw\ninput=\nrounds=4096\ncatalog=builtins\n',
            b'mode=tab-next\ninput=\nrounds=4096\ncatalog=builtins\n',
        )[WORKLOADS.index(workload) - 122]
    if workload in WORKLOADS[110:122]:
        inputs = (
            b'mode=prefix\nprefix=set f\ncatalog=builtins\n',
            b'mode=prefix\nprefix=unknown\ncatalog=builtins\n',
            b'mode=prefix\nprefix=colorscheme user-0123\ncatalog=custom-256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n',
            b'mode=prefix\nprefix=\ncatalog=builtins\n',
            b'mode=prefix\nprefix=set \ncatalog=builtins\n',
            b'mode=prefix\nprefix=colorscheme \ncatalog=builtins\n',
            b'mode=all\nprefix=\ncatalog=builtins\n',
            b'mode=prefix\nprefix=colorscheme \ncatalog=custom-256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n',
            b'mode=all\nprefix=\ncatalog=custom-256-invalid-color\nuser-0000\nuser-0001\nuser-0002\nuser-0003\nuser-0004\nuser-0005\nuser-0006\nuser-0007\nuser-0008\nuser-0009\nuser-0010\nuser-0011\nuser-0012\nuser-0013\nuser-0014\nuser-0015\nuser-0016\nuser-0017\nuser-0018\nuser-0019\nuser-0020\nuser-0021\nuser-0022\nuser-0023\nuser-0024\nuser-0025\nuser-0026\nuser-0027\nuser-0028\nuser-0029\nuser-0030\nuser-0031\nuser-0032\nuser-0033\nuser-0034\nuser-0035\nuser-0036\nuser-0037\nuser-0038\nuser-0039\nuser-0040\nuser-0041\nuser-0042\nuser-0043\nuser-0044\nuser-0045\nuser-0046\nuser-0047\nuser-0048\nuser-0049\nuser-0050\nuser-0051\nuser-0052\nuser-0053\nuser-0054\nuser-0055\nuser-0056\nuser-0057\nuser-0058\nuser-0059\nuser-0060\nuser-0061\nuser-0062\nuser-0063\nuser-0064\nuser-0065\nuser-0066\nuser-0067\nuser-0068\nuser-0069\nuser-0070\nuser-0071\nuser-0072\nuser-0073\nuser-0074\nuser-0075\nuser-0076\nuser-0077\nuser-0078\nuser-0079\nuser-0080\nuser-0081\nuser-0082\nuser-0083\nuser-0084\nuser-0085\nuser-0086\nuser-0087\nuser-0088\nuser-0089\nuser-0090\nuser-0091\nuser-0092\nuser-0093\nuser-0094\nuser-0095\nuser-0096\nuser-0097\nuser-0098\nuser-0099\nuser-0100\nuser-0101\nuser-0102\nuser-0103\nuser-0104\nuser-0105\nuser-0106\nuser-0107\nuser-0108\nuser-0109\nuser-0110\nuser-0111\nuser-0112\nuser-0113\nuser-0114\nuser-0115\nuser-0116\nuser-0117\nuser-0118\nuser-0119\nuser-0120\nuser-0121\nuser-0122\nuser-0123\nuser-0124\nuser-0125\nuser-0126\nuser-0127\nuser-0128\nuser-0129\nuser-0130\nuser-0131\nuser-0132\nuser-0133\nuser-0134\nuser-0135\nuser-0136\nuser-0137\nuser-0138\nuser-0139\nuser-0140\nuser-0141\nuser-0142\nuser-0143\nuser-0144\nuser-0145\nuser-0146\nuser-0147\nuser-0148\nuser-0149\nuser-0150\nuser-0151\nuser-0152\nuser-0153\nuser-0154\nuser-0155\nuser-0156\nuser-0157\nuser-0158\nuser-0159\nuser-0160\nuser-0161\nuser-0162\nuser-0163\nuser-0164\nuser-0165\nuser-0166\nuser-0167\nuser-0168\nuser-0169\nuser-0170\nuser-0171\nuser-0172\nuser-0173\nuser-0174\nuser-0175\nuser-0176\nuser-0177\nuser-0178\nuser-0179\nuser-0180\nuser-0181\nuser-0182\nuser-0183\nuser-0184\nuser-0185\nuser-0186\nuser-0187\nuser-0188\nuser-0189\nuser-0190\nuser-0191\nuser-0192\nuser-0193\nuser-0194\nuser-0195\nuser-0196\nuser-0197\nuser-0198\nuser-0199\nuser-0200\nuser-0201\nuser-0202\nuser-0203\nuser-0204\nuser-0205\nuser-0206\nuser-0207\nuser-0208\nuser-0209\nuser-0210\nuser-0211\nuser-0212\nuser-0213\nuser-0214\nuser-0215\nuser-0216\nuser-0217\nuser-0218\nuser-0219\nuser-0220\nuser-0221\nuser-0222\nuser-0223\nuser-0224\nuser-0225\nuser-0226\nuser-0227\nuser-0228\nuser-0229\nuser-0230\nuser-0231\nuser-0232\nuser-0233\nuser-0234\nuser-0235\nuser-0236\nuser-0237\nuser-0238\nuser-0239\nuser-0240\nuser-0241\nuser-0242\nuser-0243\nuser-0244\nuser-0245\nuser-0246\nuser-0247\nuser-0248\nuser-0249\nuser-0250\nuser-0251\nuser-0252\nuser-0253\nuser-0254\nuser-0255\n',
            b'mode=frame-ex\nprefix=set f\ncatalog=builtins\nbody=a x a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n',
            b'mode=frame-ex\nprefix=\ncatalog=builtins\nbody=a x a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n',
            b'mode=frame-palette\nprefix=:fz\ncatalog=builtins\nbody=a x a\npath=C:\\nib-probe\\probe.txt\nvisible=1\neditor-mode=vim\n',
        )
        return inputs[WORKLOADS[110:122].index(workload)]
    if workload in WORKLOADS[98:110]:
        inputs = (
            b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
            b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
            b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
            b'a',
            b'a',
            b'a',
            b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\xf0\x9f\x96\x8b',
            b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
            b'a',
            b'a',
            b'a',
            b':fz',
        )
        return inputs[WORKLOADS[98:110].index(workload)]
    if workload in (*WORKLOADS[:2], *WORKLOADS[4:7]):
        filler = "nenenib speed sample line for the open-large-file bench "
        lines = (1024 * 1024) // 84 if workload.endswith("delete-1mib") else 200000
        return "".join((f"{line:06d} " + filler + filler)[:82] + "\r\n"
                       for line in range(lines)).encode("utf-8")
    if workload == "controller-insert-200":
        return b"a" * 200
    if workload == "display-line-long":
        return ("000000 " + "日本語の長い行と分割表示の文字組みを測定する。" * 90).encode("utf-8")
    if workload == "utf8-validate-japanese-6mib":
        return (fixed_input("display-line-long") + b"\r\n") * 1000
    if workload == "controller-vim-insert-200-register-1mib":
        return b"r" * 1048576
    if workload == "controller-vim-insert-200-register-16mib":
        return b"r" * 16777216
    if workload == "controller-vim-record-insert-200":
        return b"x" * 200
    if workload == "controller-vim-record-insert-2000":
        return b"x" * 2000
    if workload in WORKLOADS[24:26]:
        return fixed_input("controller-open-utf8-16mib")
    if workload in WORKLOADS[26:31]:
        return (b"a" * 78 + b"\r\n") * 4096
    if workload in WORKLOADS[31:35]:
        return "a日本語🖋".encode("utf-8") * 4096
    if workload in WORKLOADS[35:41]:
        return "a日本語🖋 ".encode("utf-8") * 4096
    if workload in WORKLOADS[41:46]:
        return b"a" * (1024, 2048, 4096, 32, 4096)[WORKLOADS.index(workload) - 41]
    if workload == "pattern-literal-tail-4102":
        return b"x" * 4096 + b"needle"
    if workload == "pattern-literal-long-4096":
        return b"a" * 4096
    if workload in ("frame-search-dense-ascii-4096", "frame-selection-ascii-16384",
                    "frame-search-visual-ascii-4096"):
        return b"a x " * (giant_line_threshold() // 4)
    if workload == "frame-search-dense-mixed-2340":
        return "日a\t🖋\x01 ".encode("utf-8") * (giant_line_threshold() // 7)
    if workload == "frame-search-sparse-tail-16384":
        return b"x" * (giant_line_threshold() - 1) + b"a"
    if workload == "frame-rows-empty-64":
        return b""
    if workload == "frame-rows-short-64":
        return "a日\t🖋\x01 row\r\ntail".encode("utf-8")
    if workload in ("frame-rows-30-64", "frame-rows-120-64"):
        return "a日\t🖋\x01 row\r\n".encode("utf-8") * 119 + b"tail"
    if workload in WORKLOADS[57:65]:
        index = WORKLOADS.index(workload) - 57
        return b"" if index == 5 else b"a" * (256 if 1 <= index <= 3 else 1)
    if workload in WORKLOADS[65:72]:
        count = 1 if workload == "preview-caret-short-30-64" else 1024
        return ("日" * count + "z\t🖋\x01" + "\r\nz x" * 119).encode("utf-8")
    if workload in WORKLOADS[72:76]:
        if workload == "frame-document-untitled-256":
            return b""
        if workload == "frame-document-short-saved-256":
            return b"C:\\nib-probe\\frame-document.txt"
        return b"C:\\nib-probe\\" + b"segment\\" * 20 + b"a" * 64 + b".txt"
    if workload in WORKLOADS[76:82]:
        inputs = (
            b'UTF-8|CRLF|1|1',
            b'UTF-8 BOM|LF|9|24',
            b'Shift_JIS|CRLF|123|456',
            b'UTF-8|LF|18446744073709551615|18446744073709551615',
            b'UTF-8 BOM|CRLF|123456|654321',
            b'Shift_JIS|LF|1|1',
        )
        return inputs[WORKLOADS[76:82].index(workload)]
    if workload in WORKLOADS[82:89]:
        inputs = (
            b"C:\\Users\\hide\\Documents\\NeNeNib\\projects\\editor\\note.txt",
            b"C:\\" + b"segment\\" * 128 + b"note.txt",
            ("C:/" + "資料\\階層/" * 64 + "日誌🖋.txt").encode("utf-8"),
            b"note.txt",
            b"a" * 1024,
            b"C:\\" + b"segment\\" * 128,
            b"/",
        )
        return inputs[WORKLOADS[82:89].index(workload)]
    if workload in WORKLOADS[89:98]:
        inputs = (
            b'C:\\Users\\hide\\Documents\\NeNeNib\\projects\\editor\\aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt',
            b'C:\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\segment\\note.txt',
            b'C:\\work\\\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e.txt',
            b'C:\\n.txt',
            b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
            b'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\xf0\x9f\x96\x8b.txt',
            b'C:/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/\xe8\xb3\x87\xe6\x96\x99/note.txt',
            b'C:\\',
            b'',
        )
        return inputs[WORKLOADS[89:98].index(workload)]
    return scoped_fixed_input(workload)


def scoped_palette_input(query: str, detail: str) -> bytes:
    rows = [f"files\t{query}\n"]
    for index in range(5000):
        if detail:
            label = f"ENTRY_{index:05d}_ABCDEFGHIJKLMNOPQRSTUV.txt"
        elif index < 50:
            label = f"QX_Y_{index:05d}_file.txt"
        elif index < 500:
            label = f"QX_N_{index:05d}_file.txt"
        else:
            label = f"AA_N_{index:05d}_file.txt"
        rows.append(f"folder\topen\tprobe-{index:05d}\t{label}\t{detail}\n")
    return "".join(rows).encode("utf-8")


def scoped_fixed_input(workload: str) -> bytes:
    if workload == "buffer-line-text-scattered-crlf-4096":
        return (b"a" * 78 + b"\r\n") * 4096
    if workload == "buffer-line-text-scattered-lf-4096":
        return (b"a" * 78 + b"\n") * 4096
    if workload in ("buffer-position-long-utf8-57344", "buffer-position-scattered-utf8-57344"):
        return "a日本語🖋".encode("utf-8") * 4096
    if workload in ("palette-listed-name-5000", "palette-listed-location-5000"):
        query = "entry" if workload == "palette-listed-name-5000" else "zroot"
        return scoped_palette_input(query, r"D:\ZROOT\LONG_DIRECTORY_COMPONENT\GROUP_00")
    if workload in ("palette-append-narrow-5000-to-50", "palette-caret-left-5000"):
        return scoped_palette_input("qx", "")
    if workload == "codepage-to-utf8-cp932-japanese-16mib":
        return b"\x93\xfa\x96\x7b" * 4194304
    if workload == "utf16-to-utf8-japanese-8m-units":
        return b"\xe5\x65\x2c\x67" * 4194304
    if workload == "utf16-to-utf8-ascii-8m-units":
        return b"a\0" * 8388608
    if workload == "utf16-to-utf8-supplementary-8m-units":
        return b"\x3d\xd8\x8b\xdd" * 4194304
    raise ProbeNotObserved("unknown workload")


def fnv1a64(data: bytes) -> str:
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return str(value)


def positive_integer(value, label: str) -> int:
    if type(value) is not int or value <= 0:
        raise ProbeNotObserved(f"{label} must be a positive integer")
    return value


def samples_from(report: dict, iterations: int) -> list[int]:
    if not isinstance(report, dict):
        raise ProbeNotObserved("timing report must be an object")
    positive_integer(report.get("qpcFrequency"), "qpcFrequency")
    marks = report.get("marks")
    if not isinstance(marks, list) or len(marks) != iterations * 2:
        raise ProbeNotObserved("missing or extra marks")
    samples = []
    previous = -1
    for index in range(iterations):
        pair = marks[index * 2:index * 2 + 2]
        values = []
        for mark, expected in zip(pair, ("probe_started", "probe_finished")):
            if not isinstance(mark, dict) or mark.get("milestone") != expected:
                raise ProbeNotObserved("unpaired or unexpected milestone")
            value = mark.get("qpcMicroseconds")
            if type(value) is not int or value < 0 or value < previous:
                raise ProbeNotObserved("invalid or reversed timestamp")
            previous = value
            values.append(value)
        duration = values[1] - values[0]
        if duration <= 0:
            raise ProbeNotObserved("sample is below the timing resolution")
        samples.append(duration)
    return samples


def metadata_from(text: str, request: dict, expected_input: dict) -> dict:
    metadata = load_json(text)
    if not isinstance(metadata, dict):
        raise ProbeNotObserved("probe metadata must be an object")
    expected = {"schema": 1, "workload": request["workload"],
                "iterations": request["iterations"], "warmup": 1, **expected_input}
    if any(type(metadata.get(key)) is not type(value) or metadata.get(key) != value
           for key, value in expected.items()):
        raise ProbeNotObserved("workload, iterations, warmup or input differs")
    if metadata.get("commit") != request["commit"] or metadata.get("dirty") != 0:
        raise ProbeNotObserved("built commit differs or build source was dirty")
    if metadata.get("configuration") != "Release":
        raise ProbeNotObserved("comparison requires the development target's Release build")
    if not isinstance(metadata.get("compilerVersion"), str) or not metadata["compilerVersion"]:
        raise ProbeNotObserved("compiler version is missing")
    if not re.fullmatch(r"[0-9a-f]{64}", str(metadata.get("toolchainSha256", ""))):
        raise ProbeNotObserved("toolchain manifest hash is missing")
    if not re.fullmatch(r"[0-9]+", str(metadata.get("outputChecksum", ""))):
        raise ProbeNotObserved("output checksum is missing")
    return metadata


def summarize(runs: list[dict], blocks: int) -> dict:
    ratios = []
    for block in range(blocks):
        group = runs[block * 4:block * 4 + 4]
        for left, right in ((group[0], group[1]), (group[3], group[2])):
            ratios.extend(after / before for before, after in zip(left["microseconds"],
                                                                   right["microseconds"]))
    values = {side: [value for run in runs if run["side"] == side
                     for value in run["microseconds"]] for side in ("before", "after")}
    return {"pairedRatiosAfterOverBefore": ratios,
            "ratioMedian": statistics.median(ratios), "ratioRange": [min(ratios), max(ratios)],
            "microseconds": {side: {"median": statistics.median(samples),
                                    "range": [min(samples), max(samples)]}
                             for side, samples in values.items()}}


def execute_trial(command: list[str], timeout: float):
    options = {"creationflags": subprocess.CREATE_NO_WINDOW} if sys.platform == "win32" else {}
    return subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                          errors="replace", timeout=timeout, check=False, **options)


def compare(options, runner=execute_trial) -> tuple[dict, int]:
    output = options.output.resolve()
    private = output.with_suffix(output.suffix + ".runs")
    if output.exists() or private.exists():
        raise ProbeNotObserved("output or private trial directory already exists; preserve earlier results")
    output.parent.mkdir(parents=True, exist_ok=True)
    private.mkdir()
    data = fixed_input(options.workload)
    expected_input = {"inputBytes": len(data), "inputHashAlgorithm": "fnv1a64", "inputHash": fnv1a64(data)}
    executables = {side: getattr(options, side).resolve() for side in ("before", "after")}
    report = {"schema": 1, "purpose": "local workload comparison; not QLT-014",
              "workload": options.workload, "iterations": options.iterations, "blocks": options.blocks,
              "warmupPerProcess": 1, "warmupMeasured": False, "timeoutSeconds": options.timeout,
              "input": {**expected_input, "sha256": hashlib.sha256(data).hexdigest()},
              "plannedOrder": list(ORDER) * options.blocks, "executables": {}, "runs": [],
              "status": "not-observed"}
    if options.workload.endswith(("delete-1mib", "delete-16mib")):
        report["input"]["sizeLabel"] = "approximate MiB; inputBytes is authoritative"
    try:
        for side, path in executables.items():
            report["executables"][side] = {"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                                           "ref": getattr(options, side + "_ref"),
                                           "commit": getattr(options, side + "_commit")}
        for index, side in enumerate(report["plannedOrder"]):
            request = {"workload": options.workload, "iterations": options.iterations,
                       "commit": report["executables"][side]["commit"]}
            marks_path = private / f"{index:03d}-{side}.json"
            command = [str(executables[side]), options.workload, str(options.iterations), str(marks_path)]
            run = {"index": index, "side": side, "command": command, "timingReport": str(marks_path)}
            report["runs"].append(run)
            result = runner(command, options.timeout)
            run.update(returncode=result.returncode, stdout=result.stdout, stderr=result.stderr)
            if result.returncode != 0:
                raise ProbeNotObserved(f"{side} process failed: {result.returncode}")
            run["metadata"] = metadata_from(result.stdout, request, expected_input)
            run["microseconds"] = samples_from(load_json(marks_path.read_text(encoding="utf-8")), options.iterations)
            first = report["runs"][0]["metadata"]
            if any(run["metadata"][key] != first[key] for key in ("outputChecksum", "compilerVersion", "toolchainSha256")):
                raise ProbeNotObserved("output checksum or toolchain differs between trials")
        if any(hashlib.sha256(path.read_bytes()).hexdigest() != report["executables"][side]["sha256"]
               for side, path in executables.items()):
            raise ProbeNotObserved("an executable changed during comparison")
        report.update(status="observed", summary=summarize(report["runs"], options.blocks))
    except (OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        report["failure"] = f"{type(error).__name__}: {error}"
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report, 0 if report["status"] == "observed" else 2


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for side in ("before", "after"):
        parser.add_argument("--" + side, type=Path, required=True)
        parser.add_argument("--" + side + "-ref", required=True)
        parser.add_argument("--" + side + "-commit", required=True)
    parser.add_argument("--workload", choices=WORKLOADS, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--iterations", type=int, default=20)
    parser.add_argument("--blocks", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=60)
    options = parser.parse_args(argv)
    if not 1 <= options.iterations <= 2048 or not 1 <= options.blocks <= 100:
        parser.error("iterations must be 1..2048 and blocks 1..100")
    if not math.isfinite(options.timeout) or options.timeout <= 0:
        parser.error("timeout must be finite and positive")
    for side in ("before", "after"):
        if not re.fullmatch(r"[0-9a-f]{40}", getattr(options, side + "_commit")):
            parser.error(side + "-commit must be the full lowercase commit SHA")
    return options


def main(argv=None) -> int:
    try:
        report, code = compare(arguments(argv))
    except (OSError, ValueError) as error:
        print(f"Probes: not observed: {error}", file=sys.stderr)
        return 2
    if code:
        print(f"Probes: not observed: {report['failure']}", file=sys.stderr)
    else:
        print(f"Probes: observed; after/before median {report['summary']['ratioMedian']:.6f}; "
              "local comparison only, no acceptance threshold")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
