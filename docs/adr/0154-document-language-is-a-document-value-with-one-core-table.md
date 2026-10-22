# ADR 0154 — 文書の言語は文書の値で、表は core に 1 つ。設定とセッションには入れない

- 状態: 提案（hide の試用の後に受理）
- 日付: 2026-10-22
- Issue: #776
- 規則: ARC-001 / ARC-004 / ARC-007 / CPP-002 / CPP-005 / CPP-011 / CPP-012、ADR 0022（Ex の `:set`）/ 0056（文書の束）/ 0059（`UnloadedDocument`）/ 0066 / 0067（名前付き保存）/ 0070 / 0079（`write_status` の枠）/ 0140（表示形式は文書の値）

## 文脈

貼り付けの切り分け（#774）・Markdown のプレビュー（#763）・構文の色（S 段 1）は、どれも「この文書は何の言語か」で振る舞いを変える（hide の確認 2026-10-21 夜）。判定がそれぞれの所に散ると、拡張子の表が 3 つになり、`:set filetype=` で変えたときに一部だけが追いかける。

表示形式（ADR 0140）は文書ごとの値として `DocumentState` と `EditorState` のアクティブな文書の欄に置き、設定には入れない形がすでにある。言語も同じ形に置く。

## 決定

1. **言語は文書の値。** `core::Language` は閉じた enum（`plain` `markdown` `html` `c` `cpp` `python` `json` `javascript` `typescript` `yaml` `toml` `xml` `css` `powershell` `shell` `cmake`）。`DocumentState.language` と `EditorState`（アクティブな文書の `language()`）が `DisplayForm` と同じ所有で持ち、タブの切り替えの 1 か所（`parked_active` と `spread`）だけが置いて広げる（ADR 0056）。設定（`settings.v3`）とセッション（`session.v2`）には入れない。起動の復元は拡張子から引き直し、`:set filetype=` の上書きは窓を閉じると消える。
2. **表は core に 1 つ。** `language_of_path(std::string_view path) noexcept -> Language`（`LanguageTable.hpp`）は、パスの最後の区切り（`\` か `/`）の後ろの名前が `CMakeLists.txt` なら `cmake`、ほかは最後の `.` の後ろの拡張子を ASCII の大文字小文字を区別せずに `constexpr` の表で引き、無ければ `plain`。名前の無い文書・拡張子の無い名前も `plain`。初版の拡張子: `md markdown` → markdown・`html htm` → html・`c h` → c・`cpp cc cxx hpp hh hxx ipp` → cpp・`py pyi` → python・`json` → json・`js mjs cjs` → javascript・`ts mts cts` → typescript・`yml yaml` → yaml・`toml` → toml・`xml` → xml・`css` → css・`ps1 psm1 psd1` → powershell・`sh bash zsh` → shell・`cmake` → cmake・`txt` → plain。application はパスの `Document` から引く `language_of(const Document &)`（`Document.hpp`）だけを呼び、呼び出し元で拡張子を見ない。
3. **名前は Vim の filetype 名。** `language_name(Language) -> std::string_view` は `plain` が `"text"`、ほかは `"markdown"` `"html"` `"c"` `"cpp"` `"python"` `"json"` `"javascript"` `"typescript"` `"yaml"` `"toml"` `"xml"` `"css"` `"ps1"` `"sh"` `"cmake"`。逆引き `language_of_name(std::string_view) -> std::optional<Language>` は小文字の名前だけを受ける。名前の表は enum の宣言順の 1 つの配列で、2 本の関数と補完の候補（`all_language_names`）が同じ配列を読む。表の長さと enum の数は `static_assert` で揃える（`OperationTexts.cpp` と同じ形）。
4. **`:set filetype=<名前>` と `:set ft=<名前>`。** `:set display=` の隣（`ExResult.cpp` の設定に載せない option）で評価し、答えは `ExResult.language`（`ExLanguageRequest`・選んだ言語か、空なら問い）。名前を `language_of_name` で引けなければ **E474: Invalid argument**（`ExFailure::invalid_argument`。Vim は任意の文字列を受けるが Nib の言語は閉じているので断る）。`:set filetype?` `:set ft?` は今の言語を `filetype=<名前>` の 1 行で知らせ、選んだときも同じ 1 行を出す（controller の `answer_language`）。controller は意図 `SelectLanguage`（`SelectDisplay` と同じ形）で文書の値を置く。使う人の操作なので `keeps_tab_walk` と `begin_intent` の表には載せない。補完の候補の表に `set filetype=` と `set ft=` を足し、`=` まで打ったときだけ言語の名前を `set ft=<名前>` の形で表の順に足す（字体の候補と同じ形。全件の一覧には命令の頭だけ）。
5. **いつ決まるか。** 開くとき（`open_document` の 1 本・`EditorState::with_opened` がパスから引く）、名前が変わる保存（`:saveas` / 無題の `:w file` / Ctrl+Shift+S で新しいパスになったとき・`with_document` がパスの変化を見て引き直し `:set filetype=` の上書きは捨てる。同じパスの保存と、名前を変えない `:w file` の写しは保つ）、`:set filetype=`。`UnloadedDocument`（まだ読んでいないタブ）は `reach_tab` で読んだ束（`reached_bundle`）でパスから決める。自動退避から戻した束もパスから引く。NewTab（名前なし）は `plain`。
6. **ステータスバーの右側の 4 項目目は右端。** `status_items_for(caret, encoding, ending, language)` が言語の名前（`language_name`）を最後に足し、`status_item_count` は 4。並びは左から「位置・文字コード・改行・言語」で、既存の 3 項目の順は変えない（VS Code と同じく言語は右端）。`StatusBarLayout` の項目の幅は 96 / 72 / 36 / 72 DIP（言語は最も長い `javascript` `typescript` が入る幅）で、右寄せの既存の規則のまま右から並べる。`write_status` の保持の枠は 10 → 11（通常 7 枠と案内 4 枠・録画中は 8・ADR 0079 の決定 11 の追補）。保持の外で毎 frame 文字組みを作る形（表示形式の印の `write_right`）は打鍵のたびに layout を作るので採らない。右の項目が 88 DIP 広がるので、ステータスの案内（ADR 0079 の決定 9）と `:` の入力の欄はその分だけ狭くなる。右の 4 項目を置いて `:` の入力欄の右端（最初の項目の左から 16 DIP）が窓の左から `command_input_minimum_dip`（96 DIP・`CommandLayout.hpp`）に届かない窓では、`StatusBarLayout` の 1 か所が言語の欄を畳み（右端の幅 0 の矩形・ほかの 3 項目は言語が無いときの位置・renderer は幅 0 の欄を書かず枠も使わない）、案内を消す規則（ADR 0079 の決定 9）はその後の配置で判断する（96 DPI では 448 DIP 未満で畳む・最小の 360 DIP で入力欄の右端は 96 DIP）。
7. **ほかの所は言語を読むだけ。** 貼り付け（#774）・プレビュー（#763）・構文の色（S 段 1）は `EditorState::language()` / `DocumentState.language` を読む。判定の表を別に作らない。
8. **限界。** shebang・modeline・中身からの判定は無い。`.txt` と拡張子なしは `plain`。言語の一覧は閉じている（増やすのは enum と名前の表と拡張子の表の各 1 行）。`:set filetype=` は Vim と違い閉じた名前の外を E474 で断る。`:set filetype`（`?` なし）の問いの形は受けない。`:set ft=`（空）も Vim は filetype を空にするが、Nib は閉じた名前だけを受けるので E474。

## 強制

- `Language` の値が増えたら `is_language` の `switch` が落ち（CPP-002）、名前の表の行が無ければ `static_assert` が落ちる。
- 意図 `SelectLanguage` は `EditorIntent` の閉じた和型に入り、`std::visit` の写し先が無ければコンパイルが落ちる（ADR 0009）。
- scope `--language`（`tests/unit/LanguageTests.cpp`）: 拡張子の表（大文字小文字・区切り・`CMakeLists.txt`・拡張子なし）・名前の往復 16 本と小文字だけ・Ex の評価と E474・補完・開く / `:set ft=` / NewTab / 切り替え / 写しの `:w file` / `:saveas` / 同じパスの `:w`・まだ読んでいない文書を読んだとき・設定に書かないこと・ステータスの 4 項目目と配置・狭い窓で言語の欄を畳むこと（360 / 447 / 448 / 800 DIP）・閉じて隣へ移るとき・自動退避から戻した束。

## 結果

- 言語で振る舞いを変える所は、文書の値を 1 つ読むだけになる。表を足すのは core の 1 か所。
- 言語は保存されないので、`:set filetype=` の選び直しは窓を閉じると消える（拡張子から引き直す）。

## 追補（2026-10-22・#855）

- **言語を 3 つ足す（決定 1・2・3 の表の各 1 行）。** `Language` の末尾に `gdscript` `dosini` `gdresource`（名前は Vim 9.1 の filetype 名）を足し、拡張子は Vim の `filetype.vim` に寄せて `gd` → gdscript・`ini` → dosini・`tscn` `tres` → gdresource。`godot`（`project.godot`）は Vim に行が無いが同じ書式なので gdresource。`cfg` は Vim では別の filetype（`cfg`・中身で `rapid` も）なので表に入れず plain のまま（Issue #855 の初めの案と違う所）。名前の往復は 19 本。貼り付けの切り分け（`paste_kind_of`）では 3 つとも text。

## 追補（2026-10-22・#857）

- **言語を 3 つ足す（決定 1・2・3 の表の各 1 行）。** `Language` の末尾に `cs` `rust` `go`（名前は Vim 9.1 の filetype 名）を足し、拡張子は Vim の `filetype.vim`（`dist/ft.vim` の拡張子の表）に寄せて `cs` `csx` `cake` → cs・`rs` → rust・`go` → go（`csx` `cake` は Issue #857 の初めの案に無く Vim に寄せて足した所・`go.mod` は Vim では別の filetype なので plain のまま）。名前の往復は 22 本。貼り付けの切り分け（`paste_kind_of`）では 3 つとも text。`:set ft=` の名前の外の例（試験）は `rust` から `zig` に替えた。

## 追補（2026-10-22・#860）

- **言語を 4 つ足す（決定 1・2・3 の表の各 1 行）。** `Language` の末尾に `php` `sql` `lua` `dockerfile`（名前は Vim 9.1 の filetype 名）を足し、拡張子は Vim の `filetype.vim` に寄せて `php` `php0`〜`php9`（Vim の `*.php\d`）`phtml` `ctp` `phpt` `theme` → php・`sql` → sql（Vim は `g:filetype_sql` が無ければ sql）・`lua` `rockspec` → lua・`dockerfile`（`*.[dD]ockerfile`）→ dockerfile。名前の行（`CMakeLists.txt` と同じ道・大小を問わない）に `Dockerfile` `Containerfile` → dockerfile と `.luacheckrc` → lua を足し、Vim の最後の節の `Dockerfile.*` `Containerfile.*` は拡張子の表に当たらなかった名前だけ dockerfile にする（`Dockerfile.json` は json・`Dockerfile.txt` は plain・`Dockerfile.dev` は dockerfile）。`luau` は Vim では別の filetype なので plain のまま。名前の往復は 26 本。貼り付けの切り分け（`paste_kind_of`）では 4 つとも text。
