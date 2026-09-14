"""One validator for commit subjects and pull request titles (GIT-003)."""

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


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("message_file", type=Path)
    parser.add_argument("--title-only", action="store_true")
    args = parser.parse_args()
    errors = validate(args.message_file.read_text(encoding="utf-8-sig"), args.title_only)
    for error in errors:
        print(error)
    raise SystemExit(int(bool(errors)))
