# ADR 0097 — 描画の桁変換は表示行の端点を順に数える

- 状態: 設計固定・実装と実測による採否待ち
- 日付: 2026-10-10
- Issue: #360
- 規則: ARC-001 / ARC-002 / ARC-004 / ARC-005 / ARC-007 / ARC-008 / ARC-011 / ARC-012 / CPP-001 / CPP-003 / CPP-007 / CPP-008 / CPP-011 / CPP-012 / CPP-014 / CPP-016 / CPP-017 / QLT-001 / QLT-008 / QLT-012 / QLT-013 / QLT-014

## 文脈

`Direct2DRenderer::range_of`は各一致の両端について表示行を最初から歩き、UTF16の単位数もprefix全体を数える。`replaced_ranges`も置換した各文字で同じ操作を繰り返す。ADR0095はapplicationのbyte→column投影を短縮したが、DirectWriteへ渡すcolumn→UTF16は別の境界である。通常の単一端点の費用と実画面の同値も確かめる必要がある。

## 決定

1. `src/ui/win32/LineUtf16Evaluation.hpp/.cpp`に短命な私有型を一つ置く。constructor・method・stateを非公開にし、実製品の利用者`Direct2DRenderer`だけをfriendにする。既に所有された検証済み`DisplayLine.text`を同期描画中に限り借用する。公開防御copy境界・EditorFrameの所有・公開APIを変更せず、viewをframeやrendererの保持へ保存しない。
2. 保持するものは`string_view text_`、現在byte、1始まりの現在column、UTF16単位数だけ。`byte_of(Column)`と`units_of(Column)`は一つの私有`move_to(Column)`を通す。0列は1列へ畳み、目的が現在列より前ならbyte/column/unitsを0/1/0へ戻す。既存`core::next_code_point`で目的列または終端まで前進し、今回進んだ完全なUTF8区間だけを既存`core::utf16_length(...).value_or(0)`で数えて加える。独自decoder・字幅分類・索引を作らない。終端超過は実際の終端列で止まり、繰返し要求で存在しない列を記録しない。UINT32への変換は従来のDirectWrite境界と同じにする。
3. 前提は検証済みの完全UTF8表示行とcode point境界である。publicな不正UTF8の扱いを変更するものではない。byte/column/unitsは同じ位置を指し、同点で増分0、後退後も同じ正本で数える。借用元を途中で変更せず、IMEで作る`shown`とは別の元の表示行を借りる。
4. 旧自由関数`byte_of_column`と`utf16_offset`を削除し、全てのcolumn入力をこの型へ置換する。`range_of`は私有の第3引数で変換器を受ける。`draw_line_matches`は行ごとに一つ作り、順序列の各spanへ再利用する。selection/current/block caretでは各呼出し内に一つ、bar caretも同じ型を使う。raw→表示columnの`displayed`/`core::display_position`は変えない。draw関数の引数5個化や型の公開を避ける。
5. `replaced_ranges`はRendererのprivate staticに移し、同じ表示行の変換器を一つ使う。置換判定・順・挿入位置以降のshift・rangeの所有は従来のまま。IMEは同じ変換器でbyteの差込位置と同じcolumnのUTF16 baseを得る。差込前prefixは元の行と同じである。composition節/カーソルやcommandのbyte→UTF16は既存`utf16_at`、逆変換は既存`code_points_before`を維持する。
6. layout/fallback/字形保持、paint順、HitTest、色、clipping、selection/検索の意味、正規のbuild flags・警告・sanitizer・基準・fixtureは変更しない。CMake変更は新cppの既存ui targetへの登録だけ。永続cache・所有copy・別decoder・特化分岐を追加しない。

## 対象検証

通常Debugの製品を正規clang-tidy/ASan/UBSanでbuildし、実Rendererを通るGUIを動かす。既存`nib_window`は窓/Rendererを作らないので新型の直接検証として数えない。試験専用公開口・friend・製品にない呼出し経路を足さず、専用の実画面比較は既存`window_driver`を使いD側に記録する。

- ASCIIの密な検索、BMP/補助平面/結合文字/Tab、制御文字置換、順逆のVISUAL選択・改行までの選択、検索currentとzero-length、空行/行末のbar/blockを固定する。実日本語IMEは中間・Space・Enterを既存の実入力経路で確認し、IME open statusを戻す。入力欄も既存byte変換が保たれることを見る。
- 通常Releaseのbefore/afterで同一文書/設定/絶対パス/サイズ/DPI/刺激の本文とstatusを完全画素比較する。各入力到達の差分と本文/設定の不変も残す。IMEは実際に生成されたcompositionを記録し、OSが異なる候補を選んだ場合は一致と偽らない。
- 0列や範囲外など公開GUIから指定できない入力は独立コードレビューで畳み方を確かめる。私有数値APIの全入力をruntime検証済みとは主張しない。対象外の既存全件試験は回さない。

## 固定速度比較と採否

通常Release製品自身の`--measure`、既存`input_received`→次の`frame_presented`を使う。固定文書はASCII `a `×4096、混在`日a🖋 `×2048、置換`a\x01 `×1024、通常`a `×32（いずれも末尾CRLF）。検索`/a<CR>`を設定後にn/N一組をwarmupとして除外し、20回の交互n/Nを各0.4秒間隔で送る。3回ABBA、各条件120対応組を一度測定する。起動/準備/最初の検索生成を比較値へ混ぜない。各入力から次の入力までに提示があることを要求し、coalescing/欠測を数値にしない。全trial/mark/画像/失敗/悪化を保持し、同じ結果を得るまで再試行しない。buildと他GUIを止め、機械/電源/DPI/製品とscriptのhashを残す。

正式速度は本変更のbody/caretを直接通る既存4編集条件（single、burst200、burst200-16MiB、single-long-line）を既存の5sample/基準/許容で一度測る。起動/open/paletteは今回の直接比較に加えない。速度と同値と所有/正典維持を合わせて採否を決め、小さい条件の悪化も記録する。まだ速度効果・技術受理を主張しない。

## 限界

画面外を含む全ての桁の同値証明ではなく、固定条件と実機上の観測である。描画以外の応答、RSS、他機械/フォント/DPI、一般の複雑な書記素、任意IME/異なる候補順へ一般化しない。waiver none。
