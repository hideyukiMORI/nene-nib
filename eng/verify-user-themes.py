"""Focused native user-theme selection/restore/error checks (#70), isolated profile."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import tempfile
import time

from window_driver import (acknowledge_dialog, await_dialog, close, click, press, rectangle,
                           start, stop, user, window_title, write_text)

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("palette_verification", ROOT / "eng/verify-palette.py")
PALETTE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PALETTE)
EX, SETTINGS, VIEW = PALETTE.EX, PALETTE.SETTINGS, PALETTE.VIEW


def theme(name, light=False):
    sample = (ROOT / "docs/design/user-theme-format.md").read_text(encoding="utf-8").split("```text\n")[1].split("```")[0]
    sample = sample.replace("name=my-theme", "name=" + name)
    if light:
        sample = sample.replace("appearance=dark", "appearance=light").replace("#300A24", "#FFF8E1").replace("#EEEEEC", "#202020").replace("#1E0516", "#E1D7B4")
    else:
        sample = sample.replace("#300A24", "#101820").replace("#1E0516", "#172431")
    return sample


def colours(window, output, name, background, title):
    pixels, width, height = EX.shot(window, output, name)
    dpi = user.GetDpiForWindow(window)
    assert VIEW.pixel(pixels, width, width // 2, height // 2) == background, "custom body background missing"
    assert VIEW.pixel(pixels, width, width // 2, round(10 * dpi / 96)) == title, "custom title-bar override missing"
    return dpi


def dialog_text(process):
    dialog = await_dialog(process)
    assert dialog, "settings diagnostic was not shown"
    parts, child = [], None
    while True:
        child = user.FindWindowExW(dialog, child, "Static", None)
        if not child:
            break
        parts.append(window_title(child))
    return "\n".join(parts)


def verify_saved_error(executable, environment, document, settings, light, output):
    before = settings.read_bytes()
    light.write_text("broken", encoding="utf-8")
    process, window, _ = start(executable, environment, [str(document)])
    try:
        text = dialog_text(process)
        assert "hide-light" in text and "Expected key=value lines" in text, text
        assert acknowledge_dialog(process)
        SETTINGS.prepare(window)
        PALETTE.execute(window, "colorscheme dracula")
        # Ex/palette write errors are inline; the direct font shortcut uses the settings dialog.
        EX.shot(window, output, "blocked-palette-write")
        assert settings.read_bytes() == before
        # A default-valued command must not erase the blocked startup state.
        PALETTE.execute(window, "colorscheme system")
        EX.shot(window, output, "blocked-noop")
        assert settings.read_bytes() == before
        SETTINGS.chord(window, 0xBB)
        assert "hide-light" in dialog_text(process), "save block lost original theme failure"
        assert acknowledge_dialog(process)
        assert settings.read_bytes() == before, "broken saved theme was replaced by defaults"
        close(window)
        assert process.wait(timeout=5) == 0
    finally:
        stop(process)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/NeNeNib.exe")
    parser.add_argument("--only", choices=("workflow", "saved-error", "startup-notice"), default="workflow")
    args = parser.parse_args()
    output = ROOT / "out/user-theme-verification"
    output.mkdir(parents=True, exist_ok=True)
    executable = output / "NeNeNib.exe"
    shutil.copy2(args.executable, executable)
    profile = Path(tempfile.mkdtemp(prefix="profile-", dir=output)).resolve()
    directory = profile / "NeNeNib/themes"
    directory.mkdir(parents=True)
    (directory / "hide-dark.v1.theme").write_text(theme("hide-dark"), encoding="utf-8")
    light = directory / "hide-light.v1.theme"
    light.write_text(theme("hide-light", True), encoding="utf-8")
    (directory / "broken.v1.theme").write_text("version=1\n", encoding="utf-8")
    environment = dict(os.environ, LOCALAPPDATA=str(profile), APPDATA=str(profile))
    settings = profile / "NeNeNib/settings.v1"
    document = output / "theme-document.txt"
    original = "\nuser themes leave this document intact\n"
    document.write_text(original, encoding="utf-8", newline="\n")
    VIEW.become_dpi_aware()
    if args.only == "startup-notice":
        (directory / "invalid_name.v1.theme").write_text("broken", encoding="utf-8")
        process, window, _ = start(executable, environment, [str(document)])
        try:
            SETTINGS.prepare(window)
            EX.shot(window, output, "startup-notice-with-document")
            assert "theme-document.txt" in window_title(window)
            close(window)
            assert process.wait(timeout=5) == 0
        finally:
            stop(process)
        print("Startup notice screenshot captured with initial document")
        return
    if args.only == "saved-error":
        settings.write_text("version=1\ncolorscheme=hide-light\nfont_family=Consolas\nfont_size=13.5\n", encoding="utf-8")
        verify_saved_error(executable, environment, document, settings, light, output)
        result = {"brokenSavedThemeNamedAndWriteBlocked": True}
        (output / "saved-error-results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, indent=2))
        return
    process, window, _ = start(executable, environment, [str(document)])
    try:
        SETTINGS.prepare(window)
        PALETTE.open_palette(window, "hide-l")
        EX.shot(window, output, "user-palette")
        press(window, 0x0D)
        time.sleep(0.25)
        dpi = colours(window, output, "user-light", [255, 248, 225], [225, 215, 180])
        assert EX.fields(settings)["colorscheme"] == "hide-light"
        _, _, width, height = rectangle(window, user.GetClientRect)
        # The result message occupies the toggle: first click dismisses it, second selects Vim.
        click(window, *VIEW.toggle_points(width, height, dpi)["vim"])
        time.sleep(0.1)
        click(window, *VIEW.toggle_points(width, height, dpi)["vim"])
        time.sleep(0.2)
        write_text(window, ":colorscheme hide-d")
        press(window, 0x09)
        EX.shot(window, output, "user-ex-completion")
        press(window, 0x0D)
        time.sleep(0.25)
        colours(window, output, "user-dark", [16, 24, 32], [23, 36, 49])
        assert EX.fields(settings)["colorscheme"] == "hide-dark"
        before = settings.read_bytes()
        EX.command(window, "colorscheme broken")
        EX.shot(window, output, "broken-ex")
        assert settings.read_bytes() == before, "broken theme overwrote settings"
        PALETTE.execute(window, "broken")
        EX.shot(window, output, "broken-palette")
        assert settings.read_bytes() == before
        EX.command(window, "colorscheme hide-light")
        SETTINGS.chord(window, ord('S'))
        assert document.read_text(encoding="utf-8") == original, "theme selection modified body"
        close(window)
        assert process.wait(timeout=5) == 0
    finally:
        stop(process)
    process, window, _ = start(executable, environment, [str(document)])
    try:
        SETTINGS.prepare(window)
        colours(window, output, "user-restart", [255, 248, 225], [225, 215, 180])
        close(window)
        assert process.wait(timeout=5) == 0
    finally:
        stop(process)
    verify_saved_error(executable, environment, document, settings, light, output)
    result = {"dpi": dpi, "paletteUserChoice": True, "exTabUserChoice": True,
              "customBodyAndTitleColours": True, "documentUnchanged": True,
              "brokenSelectionKeepsSettings": True, "restartRestoresTheme": True,
              "brokenSavedThemeNamedAndWriteBlocked": True}
    (output / "results.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
