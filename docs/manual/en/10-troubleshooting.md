# Troubleshooting

This chapter collects common problems that new NeNe Nib users run into. Each section describes a symptom and how to fix it. Topics explained in other chapters are covered briefly here, with a link to that chapter.

> When something goes wrong, first look at the left side of the status bar. If it shows `NORMAL` or `VISUAL`, you are in Vim mode. Press `Ctrl+Shift+Space` to return to Basic mode.

## Typing does not insert text

You may be in NORMAL in Vim mode. In NORMAL, the keys you press work as Vim commands, not as text.

- Press `Ctrl+Shift+Space` to return to Basic mode. You can then type the same way as in Notepad.
- To type while staying in Vim mode, press `i` to enter INSERT. Press `Esc` to return to NORMAL.
- `Ctrl+Shift+Space` does not work while the input method (IME) is composing text. Confirm or cancel the composition first, then press it.
- In NORMAL in Vim mode, the input method (IME) is turned off automatically. It comes back when you enter INSERT.

How to tell the modes apart is explained in the [Installing and first start](01-install.md) chapter, and how to use Vim mode is explained in the [Vim editing](03-vim.md) chapter.

## `Ctrl+O` does not open a file

In Vim mode, `Ctrl+O` is the Vim key that goes back to your previous position.

- In Vim mode, the key that opens a file is `Ctrl+Shift+O`.
- You can also open a file by choosing **Open File** in **All Commands** (`F1`).
- If you switch back to Basic mode, `Ctrl+O` opens the **Open** dialog box.

## There is no key for printing

In Nib, `Ctrl+P` opens **Files and Tabs**, not printing. No key is assigned to printing.

- Choose **Print** in **All Commands** (`F1`) to choose a printer and print.
- In Vim mode, you can also print with `:hardcopy`.
- You can print documents up to 16 MiB.

## Search messages appear in English

Search messages use the same English text as Vim. They stay the same even when the display language is not English. Here is what the main messages mean.

| Message at the bottom of the window | Meaning |
|---|---|
| `E486: Pattern not found: <search text>` | The text you searched for was not found in the document. |
| `search hit BOTTOM, continuing at TOP` | The search reached the end of the document, so it continued from the beginning. |
| `search hit TOP, continuing at BOTTOM` | The search reached the beginning of the document, so it continued from the end (when searching backward). |
| `E385: Search hit BOTTOM without match for: <search text>` | The search reached the end without finding a match (when the setting to stop at the end is on). |

If the text is not found, check these points.

- Search matches uppercase and lowercase exactly. How to search without matching case is explained in the [Settings](08-settings.md) chapter.
- The text you type is searched for exactly as typed.
- Project search (`Ctrl+Shift+F`) cannot search the contents of Shift_JIS files. Searching within an open document can.

How to search is explained in the [Basic editing](02-basic-editing.md) chapter.

## Reset settings to their defaults

To reset a single setting, use a command with `&` after the setting name. For example, to reset word wrap:

| Mode | Do this |
|---|---|
| Basic mode | Press `Ctrl+P`, type `:`, then type `set wrap&` and press `Enter`. |
| Vim mode | In NORMAL, type `:set wrap&` and press `Enter`. |

The setting returns to its default value. You can also change the item back to its original value in **Settings** (`Ctrl+,`). How to type commands is explained in the [Settings](08-settings.md) chapter. There is no command that resets all settings at once.

To reset all settings, rename the settings file. The file is kept instead of deleted, so you can restore it later.

1. Close all Nib windows.
2. Type `%LOCALAPPDATA%\NeNeNib` in the File Explorer address bar to open the folder.
3. Rename every file whose name starts with `settings.v` (`settings.v6`, `settings.v5`, and so on). For example, rename `settings.v6` to `settings.v6.bak`.
4. Start Nib. Your settings are now the defaults.

- If there is no `settings.v6`, Nib reads the settings file of an older version (such as `settings.v5`). That is why you need to rename every file that starts with `settings.v`.
- To get your original settings back, close Nib, then change the names back.
- The tabs from your last session, recently closed files, and bookmarks are kept in separate files, so they stay as they are.
- To make Nib return to its initial state, including those, rename the `NeNeNib` folder instead of the files (for example, to `NeNeNib.bak`). To restore it, delete the newly created `NeNeNib` folder, then rename your folder back to `NeNeNib`.
- With the Store version, Windows may place this folder in a storage location reserved for the app. If the ZIP version's folder already exists, the Store version may use that folder. See the end of the [Installing and first start](01-install.md) chapter for details.

If a message such as "The settings file format is not valid. The file was left unchanged." appears when Nib starts, the settings file is broken. Until this is fixed, changes to settings are not saved. Rename the settings file as described above, or fix the settings file, then restart Nib.

## Nib quit unexpectedly

Nib copies unsaved changes to another place every 30 seconds. When you start it after it quit unexpectedly, it asks once: "There are unsaved edits from last time. Do you want to restore them?"

| Answer | What happens |
|---|---|
| **Yes** | The saved copies of your documents are restored to tabs. The restored tabs are unsaved ("● " in front of the tab name), so save them with `Ctrl+S`. Documents without a name are restored to **Untitled** tabs. |
| **No** | The saved copies are discarded. You cannot restore them later. |

- You can restore changes up to the last autosave. If Nib is forced to quit or the power goes out, changes made after the last autosave (up to 30 seconds' worth) are lost.
- When Nib ends because of an unexpected error, it autosaves once more just before it ends.
- Documents larger than 64 MiB are not autosaved. See the [Large files and encodings](09-large-files-and-encodings.md) chapter for details.
- Autosaved files are kept in `%LOCALAPPDATA%\NeNeNib\autosave`. They are deleted when Nib closes normally.

## Text is garbled

Nib detects the encoding of a file automatically when you open it. There is no step for choosing a different encoding when you open a file.

- Nib handles UTF-8, UTF-8 (with BOM), Shift_JIS, and UTF-16 (with BOM). EUC-JP files open as Shift_JIS and JIS (ISO-2022-JP) files open as UTF-8, so their text is garbled.
- If "The encoding could not be detected." appears and the file does not open, the file is also in an encoding that is not supported.

The supported encodings and how they are detected are explained in the [Large files and encodings](09-large-files-and-encodings.md) chapter.

## Compare the Store version and the ZIP version

The executable inside is the same. The differences are as follows.

| Item | Microsoft Store version | GitHub ZIP version |
|---|---|---|
| Updating to a new version | Updated automatically by the Store. | Not updated automatically. Download the new ZIP again from the Release page. |
| Adding to the File Explorer right-click menu | Not available. It does not appear in **All Commands** either. | Available with **Add to Windows Right-Click Menu** in **All Commands** (`F1`). |
| Where Nib stores its data | Windows may place it in a storage location reserved for the app. In that case, it is deleted when you uninstall. | `%LOCALAPPDATA%\NeNeNib`. |

The exe in the ZIP version has no signature (an electronic mark that confirms who distributed it).

How to install is explained in the [Installing and first start](01-install.md) chapter.

## Report a problem

If you find a bug, or there is something about using Nib that you do not understand, write it on the [NeNe Nib Issues page](https://github.com/hideyukiMORI/nene-nib/issues) on GitHub. You need a GitHub account to write there.

Including the following information helps find the cause faster.

- The Nib version (choose **About NeNe Nib** in **All Commands** (`F1`), and it appears in the status bar)
- Whether you use the Store version or the ZIP version
- Your Windows version
- Whether you use Basic mode or Vim mode
- What you did and what happened, and what you expected to happen

Nib has no feature that sends problem reports automatically. The app itself does not connect to the network, so the developer does not hear about a problem unless you report it on the Issues page.

Anyone can read the Issues page. When you paste document contents or screenshots, make sure they do not show information you would not want others to see.

## Going further: Read the messages when a file cannot be opened or saved

| Message | Reason |
|---|---|
| "The file could not be found." | The file has been moved or deleted. |
| "You do not have permission to open the file." | Windows permissions do not allow you to read it. |
| "Files larger than 1 GB cannot be opened." | The file is larger than the size that can be opened (1 GiB). |
| "Shift_JIS / UTF-16 files larger than 64 MiB cannot be opened." | The file is larger than the size that can be opened for Shift_JIS and UTF-16. |
| "The encoding could not be detected." | The file is in an encoding that is not supported. |
| "UTF-32 files are not supported. Save the file again as UTF-8 or UTF-16." | UTF-32 is not supported. |
| "There is not enough memory to open the file." | Your PC does not have enough memory. Close other apps, then open the file again. |
| "The file could not be saved. The original file has not been changed." | The file could not be saved, for example because writing to that location is not allowed. The original file stays as it was. |
| "Some characters cannot be saved in this encoding. Do you want to save as UTF-8?" | Some characters cannot be written in the chosen encoding. Choose **Yes** to save as UTF-8. |

The table of contents for this manual is in the [NeNe Nib manual](README.md).
