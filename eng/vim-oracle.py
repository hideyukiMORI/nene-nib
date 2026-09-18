"""Generate tests/vim/VimFixtures.hpp from the real Vim (ADR 0005 / ADR 0012 decision 7).

The fixtures in tests/vim/fixtures.json name a buffer, a key sequence and the extra settings that
belong to that case. This script feeds each one to a headless Vim 9.1 and writes what came back
(the buffer, the cursor as line and byte column, and the unnamed register with its type) into a
constexpr array.
The unit tests replay that header; they never need Vim, so the gate runs on machines without it.

Vim is started exactly the way the Phase 0 probe V2 did, from its full path, because `where vim`
finds the 9.0 that ships with Git: `vim.exe -u NONE -i NONE -N -n -es -S probe.vim input.txt`.
`-u NONE` reads no vimrc, so the settings a fixture depends on are written into probe.vim and are
part of the fixture's input. The defaults are the two lines in DEFAULT_SETTINGS; `defaults.vim`
itself is not read, because its contents move between releases (ADR 0012 decision 8).

Two rules keep the fixture and the editor comparable, and the script refuses input that breaks them:

* `text` never ends with a newline. Vim's buffer of "a\nb\n" has two lines, ours has three, so a
  trailing newline would make `getline(1, '$')` and our TextBuffer disagree about the last line.
* `keys` must leave Vim in NORMAL mode. An unterminated insert leaves the cursor one to the right
  of where a real Esc would leave it. The check is mechanical: the same keys with one more `<Esc>`
  must produce exactly the same buffer, cursor and register.

Two runs of `--regenerate` must produce the same file; that equality is the record in
docs/quality/gate-proofs.md section 5. Python standard library only.

The header also records the SHA-256 of the fixtures file it came from and how many fixtures that
file held. CI has no Vim and cannot regenerate, so that one line is what CNF-010 in
eng/conformance.py compares against tests/vim/fixtures.json to see that the two have not drifted
apart (Issue #44). The digest covers the bytes of the file as they are stored; .gitattributes
keeps them LF, so nothing is normalised here.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

VIM = Path(r"C:\Program Files\Vim\vim91\vim.exe")
# 既定の設定（ADR 0012 の決定 8）。backspace は defaults.vim が入れる値で、これが無い Vim は
# INSERT の Backspace が挿入開始位置より前を消せず、利用者が触る「既定の Vim」と違う。
DEFAULT_SETTINGS = ["set nocompatible", "set backspace=indent,eol,start"]
# fixture の記法 → Vim の二重引用符つき文字列の記法。写すのはここ 1 か所だけ（C++ 側は別の 1 か所）。
KEY_NAMES = {"<Esc>": "\\<Esc>", "<CR>": "\\<CR>", "<BS>": "\\<BS>", "<C-r>": "\\<C-r>",
             "<Home>": "\\<Home>", "<End>": "\\<End>"}
BANNER = "// 生成物。手で編集しない。python eng/vim-oracle.py --regenerate（Vim 9.1）"


def vim_version() -> str:
    """The first line of `vim --version`, which names the release and its patch level."""
    result = subprocess.run([str(VIM), "--version"], capture_output=True, check=True)
    return result.stdout.decode("utf-8", errors="replace").splitlines()[0].strip()


def vim_keys(keys: str) -> str:
    """The key sequence as the body of a Vim double-quoted string."""
    escaped = keys.replace("\\", "\\\\").replace('"', '\\"')
    for name, code in KEY_NAMES.items():
        escaped = escaped.replace(name, code)
    return escaped


def probe_script(settings: list[str], keys: str) -> str:
    # getregtype は "v"（文字単位）/ "V"（行単位）/ 一度も使っていないレジスタでは空を返す
    # (ADR 0015 decision 3). p の貼り方はその種類で決まるので、本文だけでは fixture が足りない。
    report = ("call writefile(getline(1, '$') + ['cursor=' . line('.') . ',' . col('.')]"
              " + ['regtype=' . getregtype('\"')]"
              " + ['reg=' . getreg('\"')], 'out.txt')")
    # Ex モードで開いた直後のカーソルは先頭ではないので、毎回 (1, 1) に置いてから鍵を流す。
    lines = [*DEFAULT_SETTINGS, *settings, "call cursor(1, 1)",
             'execute "normal! " . "%s"' % vim_keys(keys), report, "qa!"]
    return "\n".join(lines) + "\n"


def run_vim(work: Path, text: str, keys: str,
            settings: list[str]) -> tuple[str, int, int, str, str]:
    (work / "input.txt").write_bytes(text.encode("utf-8"))
    (work / "probe.vim").write_text(probe_script(settings, keys), encoding="utf-8")
    output = work / "out.txt"
    output.unlink(missing_ok=True)
    finished = subprocess.run([str(VIM), "-u", "NONE", "-i", "NONE", "-N", "-n", "-es",
                               "-S", "probe.vim", "input.txt"], cwd=work, capture_output=True)
    if finished.returncode != 0 or not output.is_file():
        raise RuntimeError(f"vim failed ({finished.returncode}) on {keys!r}: "
                           f"{finished.stderr.decode('utf-8', errors='replace')}")
    lines = output.read_text(encoding="utf-8").split("\n")
    while lines and lines[-1] == "":
        lines.pop()
    # writefile は文字列の中の改行を NUL で書くので、レジスタの改行をここで戻す。
    register = lines.pop().removeprefix("reg=").replace("\x00", "\n")
    kind = lines.pop().removeprefix("regtype=")
    line, column = (int(part) for part in lines.pop().removeprefix("cursor=").split(","))
    return "\n".join(lines), line, column, register, kind


def measure(work: Path, fixture: dict) -> dict:
    name, text, keys = fixture["name"], fixture["text"], fixture["keys"]
    settings = fixture.get("settings", [])
    if not name or not keys:
        raise ValueError(f"{name!r}: a fixture needs a name and a key sequence")
    if text.endswith("\n"):
        raise ValueError(f"{name}: fixture text must not end with a newline")
    measured = run_vim(work, text, keys, settings)
    # NORMAL で終わっていれば、もう 1 つ Esc を足しても何も変わらない（上の注記）。
    if run_vim(work, text, keys + "<Esc>", settings) != measured:
        raise ValueError(f"{name}: the keys do not leave Vim in NORMAL mode; end them with <Esc>")
    body, line, column, register, kind = measured
    return {"name": name, "text": text, "keys": keys, "expected": body,
            "line": line, "column": column, "register": register, "register_kind": kind}


def literal(value: str) -> str:
    """A C++ string literal. The header is UTF-8 and the build passes /utf-8, so bytes pass through."""
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    escaped = escaped.replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r")
    if any(character < " " or character == "\x7f" for character in escaped):
        raise ValueError(f"unexpected control character in {value!r}")
    return f'"{escaped}"'


def header(records: list[dict], version: str, digest: str) -> str:
    rows = []
    for record in records:
        rows.append("    {%s, %s, %s, %s, %d, %d, %s, %s},"
                    % (literal(record["name"]), literal(record["text"]), literal(record["keys"]),
                       literal(record["expected"]), record["line"], record["column"],
                       literal(record["register"]), literal(record["register_kind"])))
    body = "\n".join(rows)
    settings = " / ".join(DEFAULT_SETTINGS)
    # 生成物にも clang-format は掛かるので、ファイルまるごと整形の対象から外す（QLT-004）。
    return ("// clang-format off\n"
            f"{BANNER}\n"
            f"// oracle: {VIM} — {version}\n"
            f"// 既定の設定（ADR 0012 の決定 8）: {settings}\n"
            f"// fixtures.json: sha256 {digest} / {len(records)} fixtures\n"
            "#pragma once\n"
            "\n"
            '#include "VimFixture.hpp"\n'
            "\n"
            "#include <array>\n"
            "\n"
            "namespace nenenib::tests\n"
            "{\n"
            f"constexpr std::array<VimFixture, {len(records)}> vim_fixtures{{{{\n"
            f"{body}\n"
            "}};\n"
            "} // namespace nenenib::tests\n"
            "// clang-format on\n")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--regenerate", action="store_true",
                        help="run Vim and rewrite tests/vim/VimFixtures.hpp")
    arguments = parser.parse_args()
    source = root / "tests/vim/fixtures.json"
    target = root / "tests/vim/VimFixtures.hpp"
    content = source.read_bytes()
    fixtures = json.loads(content.decode("utf-8"))
    names = [fixture["name"] for fixture in fixtures]
    if len(set(names)) != len(names):
        raise ValueError("fixture names must be unique")
    if not arguments.regenerate:
        print(f"{len(fixtures)} fixture(s) in {source}; pass --regenerate to run {VIM}")
        return 0
    if not VIM.is_file():
        raise RuntimeError(f"the oracle needs Vim 9.1 at {VIM} (ADR 0005)")
    version = vim_version()
    work = Path(tempfile.mkdtemp(prefix="vim-oracle-"))
    try:
        records = [measure(work, fixture) for fixture in fixtures]
    finally:
        shutil.rmtree(work, ignore_errors=True)
    # 生成物も LF で書く（.gitattributes の eol=lf）。
    target.write_text(header(records, version, hashlib.sha256(content).hexdigest()),
                      encoding="utf-8", newline="\n")
    print(f"Vim oracle: {len(records)} fixture(s) from {version} written to {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
