"""Commit/title conventions and the required PR verification record (GIT-003 / GIT-004)."""

import argparse
import re
from pathlib import Path


def validate(message: str, title_only: bool = False) -> list[str]:
    subject = message.splitlines()[0] if message.splitlines() else ""
    match = re.fullmatch(r"(?:feat|fix|docs|refactor|test|build|ci|chore)(?:\([a-z0-9][a-z0-9.-]*\))?(?P<breaking>!)?: (?P<description>.+) \(#[1-9]\d*\)", subject)
    errors = []
    if not match or len(subject) > 100:
        return ["GIT-003: subject must follow the documented format and contain at most 100 characters"]
    if not re.search(r"[぀-ヿ㐀-鿿]", match["description"]):
        errors.append("GIT-003: description must contain Japanese text")
    if match["breaking"] and not title_only and not re.search(r"^BREAKING CHANGE: .+", message, re.M):
        errors.append("GIT-003: breaking commit needs a BREAKING CHANGE footer")
    return errors


def validate_pr(body: str) -> list[str]:
    """Check record presence, not the truth or sufficiency of the reported verification."""
    fields = ("確認する退行", "対象・依存", "検証結果", "再利用")
    return [f"GIT-004: missing non-empty verification field {field}:"
            for field in fields
            if not re.search(rf"^{re.escape(field)}:[ \t]*\S[^\r\n]*\r?$", body, re.M)]


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("message_file", type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--title-only", action="store_true")
    mode.add_argument("--pr-body", action="store_true")
    args = parser.parse_args()
    text = args.message_file.read_text(encoding="utf-8-sig")
    errors = validate_pr(text) if args.pr_body else validate(text, args.title_only)
    for error in errors:
        print(error)
    raise SystemExit(int(bool(errors)))
