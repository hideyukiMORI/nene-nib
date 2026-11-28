# Installing and first start

NeNe Nib is a text editor for Windows 11. By default, it runs in Basic mode, where you write just as you do in Notepad. Press `Ctrl+Shift+Space` to switch to Vim mode, where you write with Vim commands. This chapter covers everything from installing Nib to saving your first document, and how to switch between the two modes.

> If you switched to Vim mode by mistake and the characters you type do not appear, press `Ctrl+Shift+Space` to return to Basic mode.

## System requirements

- Runs on the x64 edition of Windows 11 (22H2 or later).
- Windows 10 is not supported.
- No additional runtime is needed. Nib runs from a single executable file.

## Install NeNe Nib

You can install Nib in two ways. Both contain the same executable. If you are not sure which to choose, choose the Microsoft Store. It updates Nib to new versions automatically.

### Install from the Microsoft Store

1. Open the [NeNe Nib page on the Microsoft Store](https://apps.microsoft.com/detail/9nclgdtfq529).
2. Follow the instructions on the page to install it.
3. Start NeNe Nib from the Start menu.

The Store updates Nib to new versions automatically.

<!-- Screenshot: the NeNe Nib page on the Microsoft Store (to be added) -->

### Use the ZIP from GitHub

1. Download `NeNeNib-1.2.0-windows-x64.zip` from the [Release page on GitHub](https://github.com/hideyukiMORI/nene-nib/releases/tag/v1.2.0).
2. Extract it to any folder you like.
3. Run `NeNeNib.exe`.

- The ZIP contains four files: `NeNeNib.exe`, `README.txt`, `LICENSE`, and `THIRD_PARTY_NOTICES.txt`.
- There is no installer and no automatic update. To use a new version, download the ZIP for that version from the Release page on GitHub and replace your files with it.
- The exe is not signed (a signature is an electronic mark that lets you confirm who distributed the file).
- To check that the files are not damaged, compare them with `SHA256SUMS` on the same page (a list of check values, one for each file).

With the ZIP version, you can add NeNe Nib to the File Explorer right-click menu and to **Open with**. In **All Commands** (`F1`), choose **Add to Windows Right-Click Menu**. To undo this, choose **Remove from Windows Right-Click Menu**. You do not need administrator rights. In the Store version, these two commands do not appear in the list.

## Start Nib for the first time

The first time you start Nib, it opens an empty **Untitled** tab that shows the start screen.

<!-- Screenshot: the start screen (to be added) -->

From top to bottom, the start screen shows the following.

| Heading | What it shows |
|---|---|
| **Recent Files** | Up to 10 files you closed recently. While there is no history, it shows "No history yet. Open a file or just start typing". |
| **Bookmarks** | Up to 10 files you have bookmarked, if there are any. |
| **Get Started** | Three items: **New File** (`Ctrl+T`), **Open File** (`Ctrl+O`), and **Open Folder** (`Ctrl+Shift+R`). |
| **Shortcuts** | The keys for **Files and Tabs** (`Ctrl+P`), **Help** (`F1`), and **Show or Hide Explorer** (`Ctrl+Shift+E`). |

The version number appears next to the title, and the copyright and the GitHub address appear at the bottom.

- Use `↑` / `↓` to select an item, and press `Enter` or click the item to run it.
- The start screen disappears when you type something or open a document. You can just start writing.

When you start Nib again, the tabs you had open (saved files only) are restored, with their order, the caret position, and the scroll position. The caret is the blinking line that shows where text goes; it is also called the cursor. If a file from your last session no longer exists, Nib shows a one-line message and closes that tab.

## Write, open, and save documents

| To do this | Do this | What happens |
|---|---|---|
| Create a new document | `Ctrl+T` or `Ctrl+N` | An empty **Untitled** tab opens. |
| Open a file | `Ctrl+O` | The **Open** dialog box appears. The file you choose opens. |
| Save | `Ctrl+S` | Saves changes to the current file. For a document that has no name yet, the **Save As** dialog box appears. |
| Save with a new name | `Ctrl+Shift+S` | The **Save As** dialog box appears every time, even if the document already has a name. |
| Close a tab | `Ctrl+W` or `Ctrl+F4` | Closes the current tab. |

- You can also open a file by dragging it from File Explorer and dropping it on the window. If you drag and drop several files at once, each opens in its own tab. If a file is already open, Nib switches to that tab.
- A tab with unsaved changes shows "● " before its name.
- If you try to close a tab or the window while there are unsaved changes, Nib asks "Your changes have not been saved. Do you want to save them?" Choose **Save**, **Don't Save**, or **Cancel**.
- Pressing `Esc` does not close the Nib window. In Basic mode, `Esc` only clears the selection.
- Nib copies unsaved changes to a separate location every 30 seconds. If the app quits unexpectedly, the next time you start it, Nib asks once: "There are unsaved edits from last time. Do you want to restore them?"

To choose the encoding and the line ending to save with, click the encoding (such as `UTF-8`) or the line ending (`CRLF` or `LF`) on the right side of the status bar. `CRLF` is the standard line ending on Windows, and `LF` is the standard line ending on macOS and Linux. A file you opened is saved with the encoding and line ending it had when you opened it.

## Switch between Basic mode and Vim mode

Nib has two modes.

- Basic mode lets you write with the same keys as Notepad. Nib always starts in Basic mode.
- Vim mode lets you write with Vim commands. Vim has states: NORMAL, where keys move the caret and give commands; INSERT, where you type text; and VISUAL, where you select a range.

Press `Ctrl+Shift+Space` to switch from either mode to the other. You can also switch by clicking **Basic | Vim** on the left side of the status bar.

<!-- Screenshot: the Basic | Vim switch and the mode name on the left side of the status bar (to be added) -->

To see which mode you are in, look at the left side of the status bar.

- In the **Basic | Vim** switch, the side for the current mode is colored.
- To its right, the name of the current mode appears: `Basic` in Basic mode, and `NORMAL`, `INSERT`, `VISUAL`, and so on in Vim mode.

If the characters you type do not appear in the text, check the status bar to see whether you are in NORMAL in Vim mode. Press `Ctrl+Shift+Space` to return to Basic mode.

- When you switch modes, the text, the undo history, and the caret position stay as they are.
- When you switch to Vim mode, you start in NORMAL.
- While the input method (IME) is composing text, `Ctrl+Shift+Space` does not work. Confirm or cancel the composition first, and then press it.

Some keys work differently in the two modes. For example, `Ctrl+O` in Vim mode is the Vim key that goes back to where you were before, so the key to open a file in Vim mode is `Ctrl+Shift+O`. Commands that have no key in Vim mode are available from **All Commands**, described next.

## All Commands (`F1`)

Even if you do not remember a key, you can find a command and run it with `F1`.

| To do this | Do this | What happens |
|---|---|---|
| Open the list of commands | Press `F1` | A list opens that shows the name, a short description, and the key of each command. |
| Narrow the list | Type text | Only the commands whose names match remain. |
| Run a command | Select it with `↑` / `↓` and press `Enter` | The selected command runs right away. |
| Close the list | Press `Esc` | The list closes. |

<!-- Screenshot: All Commands (F1) (to be added) -->

- The list shows the commands you can use in the current mode. Commands that have no key, such as **Print**, **Choose Encoding**, and **Choose Line Ending**, can also be run from here.
- Right after the list opens, the input method (IME) is off. To type with it, turn it on before you type.
- When the display language is Japanese, you can also search by the hiragana reading of a command name (such as ほぞん).
- Pressing `Ctrl+P` and then typing `?` opens the same list.

## Choose the display language

You can choose Japanese, English, or Simplified Chinese for the text on the screen. By default, Nib follows the Windows display language. To change it, choose **Choose Display Language** in **All Commands** (`F1`). The language changes as soon as you choose it, and stays the same the next time you start Nib.

The names of the Vim states (such as `NORMAL`), Vim messages, and theme names are in English in every language.

## Change settings

You can change settings in the **Settings** panel, which opens on the right when you press `Ctrl+,`. Your changes are saved when you close it. The [Settings](08-settings.md) chapter describes it in detail.

Nib does not collect personal information, and the app itself does not connect to the network.

## Going further: Where Nib stores its data

Settings, the tabs from your last session, recently closed files, bookmarks, and the automatic backup copies are stored in `%LOCALAPPDATA%\NeNeNib`. `%LOCALAPPDATA%` is the Windows folder that holds app data for each user (usually `C:\Users\<user name>\AppData\Local`). To open it, type `%LOCALAPPDATA%\NeNeNib` in the address bar of File Explorer.

| File | Contents |
|---|---|
| `settings.v6` | Settings |
| `session.v2` | The tabs from your last session, and their positions |
| `history.v1` | Recently closed files (up to 100) |
| `bookmarks.v1` | Bookmarks (up to 1,024) |

If you delete this folder, Nib returns to its initial state. In the Microsoft Store version, Windows may place this folder in a storage location reserved for the app; in that case, the folder is deleted when you uninstall the app. If a folder from the ZIP version already exists, the Store version may use that folder as it is.

Next: [Basic editing](02-basic-editing.md) covers what works the same as in Notepad and what is unique to Nib.
