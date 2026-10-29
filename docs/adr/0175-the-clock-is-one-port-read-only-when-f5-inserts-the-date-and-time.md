# ADR 0175 — 現在時刻は `ClockPort` の 1 本で、読むのは F5 で日時を挿入するときだけ

- 状態: 受理（施主決定 D71・2026-10-28 夜。実装は 1 席・受理は D41 の形）
- 日付: 2026-10-29
- Issue: #1050
- 規則: FR-031、ARC-003 / ARC-007 / ARC-010 / ARC-012、CPP-002 / CPP-005 / CPP-011 / CPP-013、QLT-014、ADR 0014（IME の確定は 1 意図・Vim INSERT は打鍵として engine へ）/ 0078（操作の表）/ 0123（表示言語）/ 0137（自動退避は時刻を読まない・改訂番号で比べる）

## 文脈

メモ帳の F5 は日時を挿入する。Nib は現在時刻を持てる場所が `src/adapters/win32` だけ（ARC-007・リンカのシンボルで検査）で、core と application は時刻を読まない。自動退避も時刻ではなく改訂番号で動く（ADR 0137）。日時の挿入には型のある port が 1 本要る。

## 決定

1. **port は 1 本。** `application::ClockPort{ [[nodiscard]] virtual core::LocalDateTime now() = 0; }`（`FolderPort` と同じ 5 つの削除と仮想デストラクタ）。`core::LocalDateTime{std::uint16_t year; std::uint8_t month, day, hour, minute, second;}`（公開 aggregate・比較は非メンバー）。`EditorPorts` に `ClockPort &clock`。adapter は `Win32Clock`（`GetLocalTime`・`SYSTEMTIME` を写すだけ）。替え玉は `ScriptedClock`（試験が値を置く）。

2. **形は core の純関数。** `date_time_text(LocalDateTime) -> std::string` は `HH:mm yyyy/MM/dd`（Windows のメモ帳の日本語の既定・0 埋め）。表示言語の表（ADR 0123 の改訂・#1019）が入ったら、形は表の 1 行（`UiMessage::date_time_format`）になり言語で変わる。それまでは 1 つ。ロケールの短い日付の形は読まない（adapters が読めるが、決定性のため形は Nib が持つ）。

3. **操作は 1 つ、鍵は F5、両モード。** `EditorOperation::insert_date_time`（「日時を挿入」・`KeyChord{F5}`・`OperationModes::both`・`OperationKey::f5`）。実行は `run_operation` の 1 本で、意図 `InsertDateTime`（空の struct・使う人の操作）を 1 つ送る。controller は `ports_.clock.now()` をこのときだけ呼ぶ。
   - 通常モード: 選択があれば置き換え、無ければキャレットに挿入（`InsertText` と同じ `Edit` 1 回・undo 1 単位）。
   - Vim INSERT: 文字の列を打鍵として engine へ（IME の確定と同じ道・ADR 0014）。undo は INSERT 1 回のまま。
   - Vim NORMAL / VISUAL / OPERATOR-PENDING: 挿入せず「INSERT モードで F5 を押してください」を 1 行知らせる（鳴らさない）。
   - 入力行（`:` と検索）が開いているとき: 入力行に文字として入れる（メモ帳の検索欄と同じ・`CommandEdit` の挿入）。

4. **静かな道。** 起動・開く・切り替え・保存・打鍵・自動退避で `ClockPort` を呼ばない。契約 `verify_quiet_paths` に替え玉の呼ばれた回数の欄を足す。

5. **契約。** `--clock`（替え玉の値 → 通常モードの挿入の本文 `12:34 2026/10/29`・選択の置き換え・Vim INSERT の挿入と undo 1 単位・NORMAL の拒否と知らせ・入力行への挿入・`now()` の回数が 1）。`eng/symbols.py` は既に `GetLocalTime` 系を adapters の外で拒む（`_Xtime_get_ticks` と同じ扱い・変えない）。速さ: 打鍵の道に載らない。

6. **限界（後続）。** 形の選択（`:set datetimeformat=`）・秒やタイムゾーンの表示・`strftime` 風の書式・Vim の `:r !date` 相当の Ex は後続。

## 強制

- 時刻を読む場所が adapters だけ: **active**（`eng/symbols.py`・ARC-007）。
- `ClockPort` が F5 のときだけ呼ばれる: **planned**（`verify_quiet_paths` の時計の回数・#1050 の PR #1120 の merge で active にする）。

## 結果

得られるもの: メモ帳と同じ F5。時刻の出どころが port 1 本で、試験は決定的。
失うもの・残る穴: 形はまず 1 つ。NORMAL では挿入しない（Vim 優先・D71）。

## 却下した選択肢

| 選択肢 | 却下の理由 |
|---|---|
| core が `std::chrono::system_clock::now()` を呼ぶ | ARC-007 / CPP-013。リンカの検査が落ちる |
| ロケールの短い日付の形を OS から読む | 試験が決定的でなくなる。形は Nib が持ち、言語の表で変える |
| NORMAL でも挿入（`i` を挟んで） | Vim の鍵の意味を黙って変える。D71 は Vim 優先 |

## 追補（2026-10-30・#1050 の実装の結果）

- `EditorPorts` の欄名は `clock` ではなく **`wall_clock`**。識別子 `clock` は ARC-007 の字句検査（`eng/conformance-rules.json` の nondeterministicNames）が adapters の外で拒む。型の名前 `ClockPort` はそのまま。
- handler は `EditorControllerEditing.cpp` の 1 本（IME の確定の `type_as_vim_keys` と共用するため・決定 4 の「Vim INSERT は打鍵として」はこの道）。変換中は何もしない。入力行が開いていれば `CommandText` の道。
- OPERATOR-PENDING（`d` や回数の待ち）の中の F5 は、Vim が知らない鍵を受けたときと同じく待ちを取り消してから 1 行知らせる（レビューの R3-1）。
- 通常モードで打った直後の F5 は IME の確定と同じく前の打鍵と 1 つの undo 単位にまとまる（決定 4 のまま・別の単位にしたければ hide の判断）。
