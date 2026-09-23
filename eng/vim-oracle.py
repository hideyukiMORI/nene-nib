"""Generate tests/vim/VimFixtures.hpp from the real Vim (ADR 0005 / ADR 0012 decision 7).

The fixtures in tests/vim/fixtures.json name a buffer, a key sequence and the extra settings that
belong to that case. This script feeds each one to a headless Vim 9.1 and writes what came back
(the buffer, the cursor as line and byte column, and the unnamed register with its type) into a
constexpr array.  A fixture with ``viewport`` starts Vim in ordinary terminal mode (without
``-es``), resizes its edit window, and records the resulting first visible line and ``'scroll'``.
That branch passes Vim's ``--not-a-term`` option because the oracle uses standard subprocess pipes;
it recognizes the non-terminal stream and skips Vim's warning and startup delay.  The report is the
deterministic output file.  The viewport probe verifies its requested initial height, first visible line, and cursor,
then verifies that the final saved top line equals ``line('w0')`` and that ``line('w$')`` is not
above it before recording the result.
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

A fixture may also name one macro register (`register`: `{"name": "a", "keys": "llx"}`, ADR 0046
decision 6). probe.vim writes it with `let @a = "..."` before the keys run, which is how a macro is
replayed under `:normal!`. Recording (`q`) does not work inside `:normal` at all, so a `q` typed as
a NORMAL or VISUAL command would silently measure the wrong answer; `canonical_fixture` refuses a
fixture whose `keys` or register keys hold one (#190). `records_a_macro` walks the keys through a
conservative subset of the modes, so a `q` typed as text, a character argument (`fq` `rq`), a
register name (`@q` `"qyy`) or inside a `/.../<CR>` search passes, and a doubtful `q` is refused.

`--regenerate --only f- --only df-` measures just matching fixture-name prefixes and reuses every
other generated row from `--reuse-ref` (default `HEAD`). Reuse is refused before measurement when
a prefix is empty or unknown, the old JSON/header/source do not prove their origin, a non-selected
input changed, or the measurement implementation changed. Repeating the same selected generation
must produce the same file; that equality is the record in docs/quality/gate-proofs.md section 5.
Python standard library only.

The header also records the SHA-256 of the fixtures file it came from and how many fixtures that
file held. CI has no Vim and cannot regenerate, so that one line is what CNF-010 in
eng/conformance.py compares against tests/vim/fixtures.json to see that the two have not drifted
apart (Issue #44). The digest covers the bytes of the file as they are stored; .gitattributes
keeps them LF, so nothing is normalised here.

`canonical_fixtures_json` is the one definition of how tests/vim/fixtures.json is written: `[` and
`]` alone on their own lines, one fixture per line indented by two spaces, compact separators, the
keys in the order `name`, `text`, `keys`, `settings` (only when it holds something), `register`
(only when the case has one) and `viewport` (only when the case has one), non-ASCII text as itself, LF endings and a final newline. Both
writers call it -- `--regenerate` rewrites the file before it measures anything, so the digest it
records is the digest of the canonical bytes -- and CNF-011 in eng/conformance.py compares the
stored bytes with what it returns, so the form cannot drift from the check (Issue #98).

`--format` does that rewrite alone: it needs no Vim, reuses every generated row from `--reuse-ref`
verbatim, and only writes the new digest into the header. It refuses to do so unless the reuse
ref's fixtures hold exactly the same records, so reformatting can never change an expectation.
One consequence of the one-time reformatting: `--regenerate --only` against a ref from before
Issue #98 is refused, because dropping an empty `settings` reads as a changed input there.
"""

from __future__ import annotations

import argparse
import ast
from collections.abc import Iterator
import copy
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

VIM = Path(r"C:\Program Files\Vim\vim91\vim.exe")
ORACLE_TIMEOUT_SECONDS = 30
# 既定の設定（ADR 0012 の決定 8）。backspace は defaults.vim が入れる値で、これが無い Vim は
# INSERT の Backspace が挿入開始位置より前を消せず、利用者が触る「既定の Vim」と違う。
DEFAULT_SETTINGS = ["set nocompatible", "set backspace=indent,eol,start"]
# fixture の記法 → Vim の二重引用符つき文字列の記法。写すのはここ 1 か所だけ（C++ 側は別の 1 か所）。
KEY_NAMES = {"<Esc>": "\\<Esc>", "<CR>": "\\<CR>", "<BS>": "\\<BS>", "<C-r>": "\\<C-r>",
             "<C-d>": "\\<C-d>", "<C-u>": "\\<C-u>", "<C-f>": "\\<C-f>", "<C-b>": "\\<C-b>",
             "<C-v>": "\\<C-v>", "<NL>": "\\<NL>",
             "<Home>": "\\<Home>", "<End>": "\\<End>",
             "<PageUp>": "\\<PageUp>", "<PageDown>": "\\<PageDown>"}
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


def viewport_of(fixture: dict) -> dict | None:
    viewport = fixture.get("viewport")
    if viewport is None:
        return None
    required = ("visible_lines", "first_visible", "line", "column")
    if not isinstance(viewport, dict) or set(viewport) != set(required):
        raise ValueError(f"{fixture['name']}: viewport needs exactly {', '.join(required)}")
    if any(type(viewport[key]) is not int or viewport[key] < 1 for key in required):
        raise ValueError(f"{fixture['name']}: viewport values must be positive integers")
    return viewport


def register_of(fixture: dict) -> dict | None:
    """The fixture's macro register (ADR 0046 decision 6): one lowercase name and its keys."""
    register = fixture.get("register")
    if register is None:
        return None
    if not isinstance(register, dict) or set(register) != {"name", "keys"}:
        raise ValueError(f"{fixture['name']}: register needs exactly name, keys")
    name, keys = register["name"], register["keys"]
    if not isinstance(name, str) or len(name) != 1 or not "a" <= name <= "z":
        raise ValueError(f"{fixture['name']}: register name must be one of a-z")
    if not isinstance(keys, str) or not keys:
        raise ValueError(f"{fixture['name']}: register keys must be a non-empty string")
    return {"name": name, "keys": keys}


# q の拒否の状態機械（ADR 0046 の決定 6・#190）。Vim の文法の写しではなく oracle 側の保守的な検査で、
# q が NORMAL / VISUAL のコマンドとして届くかだけを見る。迷う位置は拒む側（NORMAL / VISUAL）に倒す。
RECORDS, NORMAL, INSERT, VISUAL = "records", "normal", "insert", "visual"
COUNT_START, COUNT_DIGITS = frozenset("123456789"), frozenset("0123456789")
NORMAL_ARGUMENT = frozenset("fFtTr@\"g")
NORMAL_INSERT = frozenset("iaIAoOCsS")
OPERATORS = frozenset("dcy")
VISUAL_START = frozenset({"v", "V", "<C-v>"})
OPERATOR_ARGUMENT = frozenset("iafFtTg")
VISUAL_ARGUMENT = frozenset("iafFtT")
VISUAL_INSERT = frozenset("cs")
VISUAL_END = frozenset({"d", "x", "y", "D", "X", "Y", "p", "P", "J", "~", "u", "U", ">", "<",
                        "<Esc>"})


def key_tokens(keys: str) -> list[str]:
    """The keys one by one: a name from KEY_NAMES is one key, any other character is one key."""
    tokens: list[str] = []
    index = 0
    while index < len(keys):
        name = next((name for name in KEY_NAMES if keys.startswith(name, index)), keys[index])
        tokens.append(name)
        index += len(name)
    return tokens


def read_line(tokens: Iterator[str]) -> str | None:
    """Skips a search or Ex line up to the key that ends it, which it returns."""
    return next((key for key in tokens if key in ("<CR>", "<Esc>")), None)


def after_normal(key: str, tokens: Iterator[str]) -> str:
    if key == "q":
        return RECORDS
    if key in ("/", "?", ":"):
        read_line(tokens)
        return NORMAL
    if key in NORMAL_ARGUMENT:
        argument = next(tokens, None)
        return RECORDS if key == "g" and argument == "q" else NORMAL
    if key in NORMAL_INSERT:
        return INSERT
    if key in OPERATORS:
        return after_operator(key, tokens)
    return VISUAL if key in VISUAL_START else NORMAL


def after_operator(operator: str, tokens: Iterator[str]) -> str:
    # dq cq yq は録画を始めないが打ち切りで fixture として意味が無いので拒む。dfq diq は通す。
    motion = next(tokens, None)
    if motion in COUNT_START:
        while motion in COUNT_DIGITS:
            motion = next(tokens, None)
    done = INSERT if operator == "c" else NORMAL
    if motion is None or motion == "<Esc>":
        return NORMAL
    if motion == "q":
        return RECORDS
    if motion in ("/", "?"):
        return NORMAL if read_line(tokens) == "<Esc>" else done
    if motion in OPERATOR_ARGUMENT:
        next(tokens, None)
    return done


def after_visual(key: str, tokens: Iterator[str]) -> str:
    if key == "q":
        return RECORDS
    if key in ("/", "?"):
        read_line(tokens)
        return VISUAL
    if key in VISUAL_ARGUMENT:
        next(tokens, None)
        return VISUAL
    if key == "r":
        next(tokens, None)
        return NORMAL
    if key in VISUAL_INSERT:
        return INSERT
    return NORMAL if key in VISUAL_END else VISUAL


def records_a_macro(keys: str) -> bool:
    """Whether a `q` reaches Vim as a NORMAL or VISUAL command (ADR 0046 decision 6, #190).
    A `q` typed as text, a character argument or a register name does not record."""
    tokens = iter(key_tokens(keys))
    mode = NORMAL
    for key in tokens:
        if mode == INSERT:
            mode = NORMAL if key == "<Esc>" else INSERT
        elif mode == NORMAL:
            mode = after_normal(key, tokens)
        else:
            mode = after_visual(key, tokens)
        if mode == RECORDS:
            return True
    return False


def macro_script(register: dict | None) -> list[str]:
    # マクロの再生だけを測る（ADR 0046 の決定 6）。録画は :normal の中で動かないので、
    # レジスタは鍵を流す前に let で置く。
    if register is None:
        return []
    return ['let @%s = "%s"' % (register["name"], vim_keys(register["keys"]))]


def probe_script(settings: list[str], keys: str, viewport: dict | None = None,
                 register: dict | None = None) -> str:
    # getregtype は "v"（文字単位）/ "V"（行単位）/ 一度も使っていないレジスタでは空を返す
    # (ADR 0015 decision 3). p の貼り方はその種類で決まるので、本文だけでは fixture が足りない。
    report = ("call writefile(getline(1, '$') + ['cursor=' . line('.') . ',' . col('.')]"
              " + ['regtype=' . getregtype('\"')]"
              " + ['reg=' . getreg('\"')]"
              + (" + ['topline=' . line('w0')] + ['scroll=' . &scroll]" if viewport else "")
              + ", 'out.txt')")
    # Ex モードで開いた直後のカーソルは先頭ではないので、毎回 (1, 1) に置いてから鍵を流す。
    setup = ["call cursor(1, 1)"]
    if viewport:
        setup = ["set nowrap", f"resize {viewport['visible_lines']}",
                 f"call cursor({viewport['line']}, {viewport['column']})",
                 "call winrestview({'lnum': %d, 'topline': %d})"
                 % (viewport["line"], viewport["first_visible"]),
                 "redraw!",
                 "if winheight(0) != %d || line('.') != %d || col('.') != %d || line('w0') != %d"
                 " || winsaveview().topline != line('w0')"
                 " | cquit | endif" % (viewport["visible_lines"], viewport["line"],
                                        viewport["column"], viewport["first_visible"])]
    settle = []
    if viewport:
        settle = ["redraw!",
                  "if winsaveview().topline != line('w0') || line('w$') < line('w0')"
                  " || line('w$') != min([line('$'), line('w0') + winheight(0) - 1])"
                  " | cquit | endif"]
    lines = [*DEFAULT_SETTINGS, *settings, *setup, *macro_script(register),
             'execute "normal! " . "%s"' % vim_keys(keys), *settle, report, "qa!"]
    return "\n".join(lines) + "\n"


def run_vim(work: Path, text: str, keys: str, settings: list[str],
            viewport: dict | None = None,
            register: dict | None = None) -> tuple[str, int, int, str, str, dict | None]:
    (work / "input.txt").write_bytes(text.encode("utf-8"))
    (work / "probe.vim").write_text(probe_script(settings, keys, viewport, register),
                                    encoding="utf-8")
    output = work / "out.txt"
    output.unlink(missing_ok=True)
    command = [str(VIM), "-u", "NONE", "-i", "NONE", "-N", "-n"]
    if viewport is None:
        command.append("-es")
    else:
        command.append("--not-a-term")
    command.extend(["-S", "probe.vim", "input.txt"])
    try:
        finished = subprocess.run(command, cwd=work, capture_output=True,
                                 timeout=ORACLE_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f"vim timed out after {ORACLE_TIMEOUT_SECONDS}s on {keys!r}") from error
    if finished.returncode != 0 or not output.is_file():
        raise RuntimeError(f"vim failed ({finished.returncode}) on {keys!r}: "
                           f"{finished.stderr.decode('utf-8', errors='replace')}")
    # バイト列で読んで '\n' だけで分ける。read_text の universal newline は行の中の literal CR を
    # LF に変えてしまい、Vim が文字として書いた '\r' が改行に化ける（ADR 0036 の決定 4）。
    lines = output.read_bytes().decode("utf-8").split("\n")
    while lines and lines[-1] == "":
        lines.pop()
    # writefile は文字列の中の改行を NUL で書くので、レジスタの改行をここで戻す。
    measured_viewport = None
    if viewport:
        scroll = int(lines.pop().removeprefix("scroll="))
        first_visible = int(lines.pop().removeprefix("topline="))
        measured_viewport = {**viewport, "expected_first_visible": first_visible,
                             "expected_scroll_lines": scroll}
    register = lines.pop().removeprefix("reg=").replace("\x00", "\n")
    kind = lines.pop().removeprefix("regtype=")
    line, column = (int(part) for part in lines.pop().removeprefix("cursor=").split(","))
    return "\n".join(lines), line, column, register, kind, measured_viewport


def check_literal_cr(name: str, text: str) -> None:
    """literal CR を含む text が LF 文書として読まれることを確かめる（ADR 0036 の決定 4）。

    Vim は 'fileformats' の既定で「全部の行が CR で終わる」ときだけ dos と読んで CR を落とす
    （`:help 'fileformats'`）。私たちの `detect_line_ending`（ADR 0010 の決定 4）は最初の `\\n` の
    直前だけを見るので、最初の行が CR で終わる text は CRLF と読む。どちらかに当たる text は
    fixture として採れないので、黙って通さずに拒否する。
    """
    if "\r" not in text:
        return
    lines = text.split("\n")
    if all(line.endswith("\r") for line in lines):
        raise ValueError(f"{name}: every line ends with CR, so Vim reads the text as dos and "
                         "drops it; leave at least one line without a trailing CR")
    if lines[0].endswith("\r"):
        raise ValueError(f"{name}: the first line ends with CR, so detect_line_ending reads the "
                         "text as CRLF; put the literal CR somewhere else")


def measure(work: Path, fixture: dict) -> dict:
    name, text, keys = fixture["name"], fixture["text"], fixture["keys"]
    settings = fixture.get("settings", [])
    if not name or not keys:
        raise ValueError(f"{name!r}: a fixture needs a name and a key sequence")
    if text.endswith("\n"):
        raise ValueError(f"{name}: fixture text must not end with a newline")
    check_literal_cr(name, text)
    viewport = viewport_of(fixture)
    macro = register_of(fixture)
    measured = run_vim(work, text, keys, settings, viewport, macro)
    # NORMAL で終わっていれば、もう 1 つ Esc を足しても何も変わらない（上の注記）。
    if run_vim(work, text, keys + "<Esc>", settings, viewport, macro) != measured:
        raise ValueError(f"{name}: the keys do not leave Vim in NORMAL mode; end them with <Esc>")
    body, line, column, register, kind, measured_viewport = measured
    return {"name": name, "text": text, "keys": keys, "expected": body,
            "line": line, "column": column, "register": register, "register_kind": kind,
            "viewport": measured_viewport, "macro": macro}


# Partial regeneration compares every measurement constant and function above this boundary.
# Keep rendering/reuse helpers below it; moving measurement code below it invalidates the proof.
def literal(value: str) -> str:
    """A C++ string literal. The header is UTF-8 and the build passes /utf-8, so bytes pass through."""
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    escaped = escaped.replace("\n", "\\n").replace("\t", "\\t").replace("\r", "\\r")
    # 矩形レジスタの種類は Ctrl-V (0x16) に幅の 10 進が続く。8 進のエスケープは 3 桁で止まる
    # ので続く数字と混ざらない（16 進の \\x は貪欲に読むので使えない・ADR 0035 の決定 10）。
    escaped = escaped.replace("\x16", "\\026")
    if any(character < " " or character == "\x7f" for character in escaped):
        raise ValueError(f"unexpected control character in {value!r}")
    return f'"{escaped}"'


def fixture_row(record: dict) -> str:
    viewport = record.get("viewport")
    viewport_value = "std::nullopt" if viewport is None else (
        "VimViewportFixture{%d, %d, %d, %d, %d, %d}" %
        (viewport["visible_lines"], viewport["first_visible"], viewport["line"],
         viewport["column"], viewport["expected_first_visible"],
         viewport["expected_scroll_lines"]))
    # マクロのレジスタは在るときだけ末尾に足す（VimFixture の既定値は空・ADR 0046 の決定 6）。
    # 無い行は前と同じ字面のまま残る。
    macro = record.get("macro")
    macro_value = "" if macro is None else ", VimMacroFixture{'%s', %s}" % (
        macro["name"], literal(macro["keys"]))
    return "    {%s, %s, %s, %s, %d, %d, %s, %s, %s%s}," % (
        literal(record["name"]), literal(record["text"]), literal(record["keys"]),
        literal(record["expected"]), record["line"], record["column"],
        literal(record["register"]), literal(record["register_kind"]), viewport_value,
        macro_value)


def header_from_rows(rows: list[str], version: str, digest: str) -> str:
    body = "\n".join(rows)
    settings = " / ".join(DEFAULT_SETTINGS)
    # 生成物にも clang-format は掛かるので、ファイルまるごと整形の対象から外す（QLT-004）。
    return ("// clang-format off\n"
            f"{BANNER}\n"
            f"// oracle: {VIM} — {version}\n"
            f"// 既定の設定（ADR 0012 の決定 8）: {settings}\n"
            f"// fixtures.json: sha256 {digest} / {len(rows)} fixtures\n"
            "#pragma once\n"
            "\n"
            '#include "VimFixture.hpp"\n'
            "\n"
            "#include <array>\n"
            "\n"
            "namespace nenenib::tests\n"
            "{\n"
            f"constexpr std::array<VimFixture, {len(rows)}> vim_fixtures{{{{\n"
            f"{body}\n"
            "}};\n"
            "} // namespace nenenib::tests\n"
            "// clang-format on\n")


def header(records: list[dict], version: str, digest: str) -> str:
    return header_from_rows([fixture_row(record) for record in records], version, digest)


def selected_names(fixtures: list[dict], prefixes: list[str]) -> set[str]:
    if any(not prefix for prefix in prefixes):
        raise ValueError("--only prefixes must not be empty")
    names = {fixture["name"] for fixture in fixtures}
    selected = {name for name in names if any(name.startswith(prefix) for prefix in prefixes)}
    unknown = [prefix for prefix in prefixes if not any(name.startswith(prefix) for name in names)]
    if unknown:
        raise ValueError(f"--only prefix matched no fixture: {', '.join(unknown)}")
    return selected


def measurement_ast(source: str) -> str:
    tree = ast.parse(source)
    compared = []
    found_literal = False
    for node in tree.body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name == "literal":
            found_literal = True
            break
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            continue
        if isinstance(node, ast.Expr) and isinstance(node.value, ast.Constant) \
                and isinstance(node.value.value, str):
            continue
        if isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = node.targets if isinstance(node, ast.Assign) else [node.target]
            if any(isinstance(target, ast.Name) and target.id == "BANNER" for target in targets):
                continue
        normalized = copy.deepcopy(node)
        if isinstance(normalized, (ast.FunctionDef, ast.AsyncFunctionDef)) \
                and normalized.body and isinstance(normalized.body[0], ast.Expr) \
                and isinstance(normalized.body[0].value, ast.Constant) \
                and isinstance(normalized.body[0].value.value, str):
            normalized.body.pop(0)
        compared.append(ast.dump(normalized, include_attributes=False))
    if not found_literal:
        raise ValueError("oracle source has no literal() boundary")
    return "\n".join(compared)


def measurement_imports(source: str) -> set[tuple[str, ...]]:
    imports = set()
    for node in ast.parse(source).body:
        if isinstance(node, ast.Import):
            imports.update(("import", alias.name, alias.asname or "") for alias in node.names)
        elif isinstance(node, ast.ImportFrom):
            imports.update(("from", node.module or "", str(node.level), alias.name,
                            alias.asname or "") for alias in node.names)
    return imports


def measurement_sources_match(old_source: str, current_source: str) -> bool:
    old_imports = measurement_imports(old_source)
    current_imports = measurement_imports(current_source)
    permitted_additions = {("import", "ast", ""), ("import", "copy", ""),
                           ("import", "re", "")}
    imports_match = old_imports <= current_imports \
        and current_imports - old_imports <= permitted_additions
    return imports_match and measurement_ast(old_source) == measurement_ast(current_source)


def parsed_header(header_content: str,
                  fixture_content: bytes) -> tuple[dict[str, str], dict[str, str]]:
    fixture_digest = hashlib.sha256(fixture_content).hexdigest()
    fixtures = json.loads(fixture_content.decode("utf-8"))
    fixture_names = [fixture["name"] for fixture in fixtures]
    if len(set(fixture_names)) != len(fixture_names):
        raise ValueError("reuse fixture names must be unique")

    oracle_match = re.search(r"^// oracle: (.+) — (.+)$", header_content, re.MULTILINE)
    settings_match = re.search(r"^// 既定の設定（ADR 0012 の決定 8）: (.*)$",
                               header_content, re.MULTILINE)
    fixture_match = re.search(r"^// fixtures\.json: sha256 ([0-9a-f]{64}) / (\d+) fixtures$",
                              header_content, re.MULTILINE)
    array_match = re.search(r"^constexpr std::array<VimFixture, (\d+)> vim_fixtures\{\{$",
                            header_content, re.MULTILINE)
    if not oracle_match or not settings_match or not fixture_match or not array_match:
        raise ValueError("reuse header metadata is incomplete")
    declared_count = int(fixture_match.group(2))
    if fixture_match.group(1) != fixture_digest or declared_count != len(fixtures):
        raise ValueError("reuse header fixtures SHA or count does not match fixtures.json")
    if int(array_match.group(1)) != len(fixtures):
        raise ValueError("reuse header array count does not match fixtures.json")

    rows = [line for line in header_content.splitlines() if line.startswith("    {")]
    row_names = []
    row_by_name = {}
    for row in rows:
        name_match = re.match(r'^    \{("(?:[^"\\]|\\.)*"),', row)
        if not name_match:
            raise ValueError("reuse header has an unrecognized fixture row")
        name = json.loads(name_match.group(1))
        row_names.append(name)
        row_by_name[name] = row
    if row_names != fixture_names or len(row_by_name) != len(fixture_names):
        raise ValueError("reuse header fixture rows do not match fixtures.json name and order")
    metadata = {"path": oracle_match.group(1), "version": oracle_match.group(2),
                "settings": settings_match.group(1)}
    return row_by_name, metadata


def partial_header(fixtures: list[dict], fixture_content: bytes, prefixes: list[str],
                   reuse_fixture_content: bytes, reuse_header_content: str,
                   reuse_oracle_source: str, current_oracle_source: str,
                   version: str, work: Path) -> str:
    selected = selected_names(fixtures, prefixes)
    old_fixtures = json.loads(reuse_fixture_content.decode("utf-8"))
    old_by_name = {fixture["name"]: fixture for fixture in old_fixtures}
    old_rows, metadata = parsed_header(reuse_header_content, reuse_fixture_content)
    if metadata != {"path": str(VIM), "version": version,
                    "settings": " / ".join(DEFAULT_SETTINGS)}:
        raise ValueError("reuse oracle version, path, or default settings do not match")
    if not measurement_sources_match(reuse_oracle_source, current_oracle_source):
        raise ValueError("oracle measurement code changed; regenerate all fixtures")
    current_names = {fixture["name"] for fixture in fixtures}
    removed_nonselected = [name for name in old_by_name
                           if name not in current_names
                           and not any(name.startswith(prefix) for prefix in prefixes)]
    if removed_nonselected:
        raise ValueError(f"non-selected fixture was removed: {removed_nonselected[0]}")
    for fixture in fixtures:
        name = fixture["name"]
        if name not in selected and old_by_name.get(name) != fixture:
            raise ValueError(f"non-selected fixture input changed: {name}")

    rows = []
    for fixture in fixtures:
        name = fixture["name"]
        if name in selected:
            rows.append(fixture_row(measure(work, fixture)))
        else:
            rows.append(old_rows[name])
    digest = hashlib.sha256(fixture_content).hexdigest()
    return header_from_rows(rows, version, digest)


def git_output(root: Path, arguments: list[str]) -> bytes:
    result = subprocess.run(["git", *arguments], cwd=root, capture_output=True)
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise ValueError(f"git {' '.join(arguments)} failed: {detail}")
    return result.stdout


# tests/vim/fixtures.json の整形を決めるのはここ 1 か所だけである（ARC-001・Issue #98）。
# 書き戻す側（--format / --regenerate）と検査する側（CNF-011・eng/conformance.py）が同じ関数を呼ぶ。
FIXTURE_KEYS = ("name", "text", "keys", "settings", "register", "viewport")
VIEWPORT_KEYS = ("visible_lines", "first_visible", "line", "column")


def canonical_fixture(fixture: dict) -> dict:
    """One fixture with the canonical keys in the canonical order.

    An empty `settings` is left out entirely instead of being written as `[]`: the oracle reads it
    with `fixture.get("settings", [])`, so the two spellings are the same input and only one of
    them may be stored. An unknown key is refused rather than dropped, because dropping it would
    silently throw away part of a fixture's input.
    """
    if not isinstance(fixture, dict):
        raise ValueError("a fixture must be a JSON object")
    name = fixture.get("name", "?")
    unknown = sorted(set(fixture) - set(FIXTURE_KEYS))
    if unknown:
        raise ValueError(f"{name}: unknown fixture key(s): {', '.join(unknown)}")
    missing = [key for key in ("name", "text", "keys") if key not in fixture]
    if missing:
        raise ValueError(f"{name}: a fixture needs {', '.join(missing)}")
    canonical = {key: fixture[key] for key in ("name", "text", "keys")}
    if fixture.get("settings"):
        canonical["settings"] = fixture["settings"]
    register = register_of(fixture)
    if register is not None:
        canonical["register"] = register
    # 録画は oracle の :normal! の中で動かないので、黙って間違った期待値を書く前に拒む（ADR 0046）。
    if records_a_macro(fixture["keys"]) or (register is not None
                                            and records_a_macro(register["keys"])):
        raise ValueError(f"{name}: a fixture cannot type q as a command; the oracle's :normal! "
                         "never records a macro, so put the macro in register instead")
    viewport = viewport_of(fixture)
    if viewport is not None:
        canonical["viewport"] = {key: viewport[key] for key in VIEWPORT_KEYS}
    return canonical


def canonical_fixtures_json(fixtures: list[dict]) -> bytes:
    """The canonical bytes of tests/vim/fixtures.json: one fixture per line, UTF-8, LF.

    One line per fixture is what makes a new case a one-line diff and keeps a merge conflict inside
    the case it belongs to; the brackets stay on their own lines so the first and last case read
    like every other one.
    """
    if not isinstance(fixtures, list):
        raise ValueError("fixtures.json must hold a JSON array of fixtures")
    rows = [json.dumps(canonical_fixture(fixture), ensure_ascii=False, separators=(",", ":"))
            for fixture in fixtures]
    if not rows:
        return b"[]\n"
    body = ",\n".join(f"  {row}" for row in rows)
    return f"[\n{body}\n]\n".encode("utf-8")


def write_atomically(target: Path, content: bytes) -> None:
    temporary_path = None
    try:
        with tempfile.NamedTemporaryFile("wb", delete=False, dir=target.parent,
                                         prefix=f".{target.name}.") as temporary:
            temporary.write(content)
            temporary_path = Path(temporary.name)
        temporary_path.replace(target)
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def write_header(target: Path, rendered: str) -> None:
    # 生成物も LF で書く（.gitattributes の eol=lf）。rendered の改行は LF だけである。
    write_atomically(target, rendered.encode("utf-8"))


def write_canonical_fixtures(source: Path) -> bytes:
    """Store tests/vim/fixtures.json in the canonical form and return the bytes it now holds."""
    content = source.read_bytes()
    canonical = canonical_fixtures_json(json.loads(content.decode("utf-8")))
    if canonical != content:
        write_atomically(source, canonical)
    return canonical


def reformatted_header(fixtures: list[dict], fixture_content: bytes,
                       reuse_fixture_content: bytes,
                       reuse_header_content: str) -> tuple[str, str]:
    """The reuse ref's header with the digest of the reformatted JSON, and the version it names.

    Reformatting may change the bytes of the fixtures file and nothing else, so the reuse ref has
    to hold the same records: its canonical bytes are compared with the ones just written. Once
    that holds, every generated row and the oracle metadata come from the reuse ref verbatim, which
    is why no Vim is needed and why nothing here can invent an expectation.
    """
    old_rows, metadata = parsed_header(reuse_header_content, reuse_fixture_content)
    old_fixtures = json.loads(reuse_fixture_content.decode("utf-8"))
    if canonical_fixtures_json(old_fixtures) != fixture_content:
        raise ValueError("--format would change fixture inputs, not only bytes; regenerate instead")
    if metadata["path"] != str(VIM) or metadata["settings"] != " / ".join(DEFAULT_SETTINGS):
        raise ValueError("reuse oracle path or default settings do not match")
    rows = [old_rows[fixture["name"]] for fixture in fixtures]
    digest = hashlib.sha256(fixture_content).hexdigest()
    return header_from_rows(rows, metadata["version"], digest), metadata["version"]


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--regenerate", action="store_true",
                        help="run Vim and rewrite tests/vim/VimFixtures.hpp")
    parser.add_argument("--only", action="append", default=[], metavar="FIXTURE-PREFIX",
                        help="measure matching fixtures and reuse the remaining rows")
    parser.add_argument("--reuse-ref", default="HEAD", metavar="GIT-REF",
                        help="commit whose unchanged fixture rows are reused (default: HEAD)")
    parser.add_argument("--format", action="store_true",
                        help="rewrite tests/vim/fixtures.json in the canonical form without Vim, "
                             "reusing every generated row from --reuse-ref")
    arguments = parser.parse_args()
    source = root / "tests/vim/fixtures.json"
    target = root / "tests/vim/VimFixtures.hpp"
    content = source.read_bytes()
    fixtures = json.loads(content.decode("utf-8"))
    names = [fixture["name"] for fixture in fixtures]
    if len(set(names)) != len(names):
        raise ValueError("fixture names must be unique")
    if arguments.format and (arguments.regenerate or arguments.only):
        parser.error("--format measures nothing and selects nothing; use it on its own")
    if not arguments.regenerate and not arguments.format:
        if arguments.only:
            parser.error("--only requires --regenerate")
        print(f"{len(fixtures)} fixture(s) in {source}; pass --regenerate to run {VIM}")
        return 0
    # 整形はここで 1 回だけ書き戻し、以降の SHA-256 は正準形のバイト列で取る（CNF-011）。
    content = write_canonical_fixtures(source)
    fixtures = json.loads(content.decode("utf-8"))
    if arguments.format:
        commit = git_output(root, ["rev-parse", "--verify", "--end-of-options",
                                   f"{arguments.reuse_ref}^{{commit}}"]).decode("ascii").strip()
        rendered, version = reformatted_header(
            fixtures, content,
            git_output(root, ["show", f"{commit}:tests/vim/fixtures.json"]),
            git_output(root, ["show", f"{commit}:tests/vim/VimFixtures.hpp"]).decode("utf-8"))
        write_header(target, rendered)
        print(f"Vim oracle: 0 measured / {len(fixtures)} reused from {commit}; "
              f"{version}; written to {target}")
        return 0
    selected_names(fixtures, arguments.only) if arguments.only else set()
    if not VIM.is_file():
        raise RuntimeError(f"the oracle needs Vim 9.1 at {VIM} (ADR 0005)")
    version = vim_version()
    work = Path(tempfile.mkdtemp(prefix="vim-oracle-"))
    try:
        if arguments.only:
            commit = git_output(root, ["rev-parse", "--verify", "--end-of-options",
                                       f"{arguments.reuse_ref}^{{commit}}"]).decode("ascii").strip()
            reuse_fixture_content = git_output(
                root, ["show", f"{commit}:tests/vim/fixtures.json"])
            reuse_header_content = git_output(
                root, ["show", f"{commit}:tests/vim/VimFixtures.hpp"]).decode("utf-8")
            reuse_oracle_source = git_output(
                root, ["show", f"{commit}:eng/vim-oracle.py"]).decode("utf-8")
            rendered = partial_header(
                fixtures, content, arguments.only, reuse_fixture_content, reuse_header_content,
                reuse_oracle_source, Path(__file__).read_text(encoding="utf-8"), version, work)
        else:
            records = [measure(work, fixture) for fixture in fixtures]
            rendered = header(records, version, hashlib.sha256(content).hexdigest())
    finally:
        shutil.rmtree(work, ignore_errors=True)
    write_header(target, rendered)
    measured_count = len(fixtures) if not arguments.only else len(selected_names(fixtures, arguments.only))
    reused_count = len(fixtures) - measured_count
    reuse_detail = "" if not arguments.only else f" from {commit}"
    print(f"Vim oracle: {measured_count} measured / {reused_count} reused{reuse_detail}; "
          f"{version}; written to {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
