# ADR 0121 — 改行索引の容量は本文のバイト数で制限する

- 状態: 技術受理・対象契約と実機4条件を確認、既存の速度基準超過は#460へ分離
- 日付: 2026-10-12
- Issue: #446
- 影響する規則: ARC-001 / ARC-003 / ARC-005 / ARC-007 / ARC-009 / ARC-012 / CPP-003 / CPP-008 / CPP-011 / CPP-012 / CPP-018 / QLT-001 / QLT-010 / QLT-012 / QLT-014

## 文脈

段階1Aの[大容量試験計画](../plans/2026-10-12-release-scope-and-large-file-probe.md)は2 GiBを試す。ADR 0047 / 0080の改行ごとの64 bit Offset列では、全LFの本文に16 GiBの索引が要る。密度によって増える索引はprivate bytes差分512 MiB条件に適さない。一方、行位置を引くたびに本文を走査すると描画と入力を止める。

## 決定

1. **originalと各add chunkの索引は同じ`NewlineIndex`とする。** 1 byteにつき1 bitの`uint64_t`列と、512 byte（8 word）ごとの累積改行数の`size_t`列を持つ。bitはLFの位置だけ立てる。CRLFの意味はADR 0036を維持する。本文も改行ごとのOffset列も重複して持たない。
2. **容量を先に確保する。** originalの生成は`ceil(N/64)` wordと`floor(N/512)`集計値をreserveしてから同じ追記実装へ渡す。2 GiBの索引payloadは256 + 32 = 288 MiB。これは64 bit環境の要素領域の計算値であり、allocator・本文・piece・描画等を含むプロセス実測ではない。128 byteごとにLFのL2では旧128 MiBより増えるが、全LFでもこの上限に収まる。空の索引は要素領域を確保しない。
3. **問いは索引だけを読む。** `count_before(offset)`は完了ブロックの集計と最大8 wordのpopcount、`at(ordinal)`は集計列のupper_boundと最大8 wordの探索で答える。末尾wordの未使用bitは0。`count_before`は本文末尾へ丸め、`at`は`ordinal < size()`を呼出しの事前条件とする。8 bit値の個数/末尾0数を標準の`popcount` / `countr_zero`でコンパイル時に256要素の不変表へ作り、実行時は1 wordにつき最大8回の表参照で求める。SIMD intrinsicsやCPU判別をcoreへ書かない。
4. **公開は構築と読み取りだけ。** `from_text`・`empty`・`size`・`count_before`・`at`を公開し、追記はfriendの`TextBuffer`の非公開`appended`だけが行う。任意の呼出し元が共有済み索引を壊す口、測定専用の容量API、密度で切り替える第二の索引は足さない。
5. **pieceは既存のordinal窓を保持する。** 切断はsource全体の`count_before`で窓の両端を求め、行位置は同じ索引の`at`から得る。addはADR 0044の「先端を持つ値だけ」の条件でbytesと索引を一緒に伸ばす。最終wordは追記でbitが増えるが、旧pieceはbyte範囲とordinal窓の端を越えず、旧値からの分岐は別chunkを作る。索引の生viewを貸さないため内部vectorの再配置は借用本文を壊さない。
6. **読み込み・保存・並行性は既存経路を維持する。** UTF-8検証と所有移管はADR 0080、本文断片の保存はADR 0119。64 MiB読込上限、同期モデル、保存schema、undoの意味はこの変更で拡張しない。2 GiB実測とmapping/workerは後続。

## 検証と採用

bit/集計境界・密度・NUL・CRLF・EOFのrank/selectを本文の単純な走査と比較する。共有addの追記・分岐・piece切断・64 KiB境界・大きい挿入と旧値の行位置を契約で確かめる。既存buffer-range、wrapped-view、vim-line-jumpsを直接の利用側として選ぶ。

Debugのclang-tidy/ASan/UBSan、変更ファイル整形、core/application symbols、規約検査を行う。Releaseを準備した後、ADR 0114どおりhideと測定時間を調整する。通常入力・長行入力・16 MiB連打・16 MiB openを既存基準と比較し、閾値も基準値も変更しない。小差を高速化の根拠にしない。成功済み検証は関連入力が変わらない限り再利用する。

## 既存決定との関係・限界

ADR 0047決定1/3/5とADR 0080決定6の物理表現・検索方法を本ADRで置き換える。sourceごとの共有、piece窓、本文を再走査しない意味は維持する。細かい二次集計は問い合わせを8 wordへ制限するためで、4096 byte間隔の260 MiB案より28 MiB使う。疎な文書の索引増加、全LFでの構築時間、atの追加演算は測定で判断する。2 GiBの実private bytes合格や非同期応答性の達成は本ADRだけでは主張しない。

**実装中の確認。** 固定MSVC STLの実行時`std::popcount` / `std::countr_zero`は`__isa_available`を参照した（NewlineIndex.cpp.objのllvm-nm出力）。許可シンボルを追加せず、標準関数をconstexpr表の生成にだけ用いる。実行時はscalarの表参照とシフトだけで、CPU別の経路を追加しない。

**2026-10-12実機結果。** 並行作業ありの条件で窓選択の不具合を#448で修正した後、4ベンチとも終了待ちtimeoutとなり、正式な性能値を確定できなかった。[全記録](../quality/newline-index-2026-10-12.md)。基準値を変えず、PR447のmain統合を保留する。

**2026-10-13再開と採否。** hideの再開指示後、4条件とも5試行が成立し欠測0。通常入力0.613 ms、長行6.341 ms、16MiB open 213.292 msは既存基準内。16MiB連打は15.468 msで基準超過だが、変更前7b117e2も同じ条件で15.349 msと超過した。差0.119 ms（約0.8%）から回帰の有無を断定せず、基準超過が変更前にもある事実を#460へ記録した。QLT-012に従って既存失敗を分離し本変更を受理する。4件すべての速度合格とは扱わず、閾値・基準値・waiverは変更しない。2GiB全LFの構築時間/private bytesは後続の試作で判断する。

## 2026-10-22: 塊ごとの疎 / 密の 2 表現（#792・親 #791・D63）

巨大ファイルの段 1（[計画](../plans/2026-10-21-large-file.md) §3.1）。決定 1〜3 の**物理表現**を次で置き換える。決定 4〜6（公開は構築と読み取りだけ・piece の序数の窓・読み込みと保存の経路）と公開 API（`from_text` `empty` `size` `count_before` `at` `lengths`・friend の `TextBuffer` の `append`）は変えない。

1. **目標は「典型の本文で 5% 以下・最悪でも今以下」。** 1 byte に 1 bit（12.5%）は密度に依らない代わりに疎な本文で無駄になる。全 LF の本文は行頭が本文と同じ数あるので、密度に依らず 5% を切る厳密な表現は無い。そこで疎と密の 2 表現を塊ごとに選ぶ。全 LF の最悪は 12.5% と小さな集計で、今の 0.203 倍より増えない。
2. **塊は 64 KiB。** source を 64 KiB（`1 << 16`・`VirtualColumnIndex` と同じ刻み）の塊に切る。塊ごとに「塊の先頭までの累積の改行数」（`std::size_t`）と、塊の表現 `NewlineChunk`（`std::variant<NewlineSparseChunk, NewlineDenseChunk>`・読む所は `std::visit` で両方を書く）と、行の長さの要約 `LineLengths` を 1 つずつ持つ。
3. **疎の表現。** LF が 4,096 個未満の塊は、塊の先頭からの LF の位置（`std::uint16_t`・昇順）の列（2 byte × 個数 ≤ 8 KiB）。書き終えた塊は列の余りの容量を返す。
4. **密の表現。** LF が 4,096 個以上の塊は、1 byte に 1 bit の `std::uint64_t` 1,024 word（8 KiB）と、512 byte の区切りごとに「その区切りの前まで」の塊の中の LF の数（`std::uint16_t` 128 個・最大 65,024 なので収まる）。`at` と `count_before` が読む word は最大 8 で、決定 3 の性質を保つ。
5. **問いは索引だけを読む（決定 3 を維持）。** `count_before(offset)` は本文の末尾以上なら全数、そうでなければ塊の累積に、疎は `lower_bound`（offset より前の位置の数）・密は区切りの累積と最大 8 word の数を足す。`at(ordinal)` は塊の累積の `upper_bound` で塊を引き、疎は列の添字・密は区切りの累積の `upper_bound` と最大 8 word の走査。`lengths(begin, end)` は 64 KiB の塊ごとの要約を畳み、両端の欠けた塊だけを疎なら位置の列・密なら bit から数える（読む量は区間に比例・最大 2 塊）。
6. **追記は末尾の塊だけ（決定 4・5 を維持）。** `append` は書きかけの末尾の塊を疎で伸ばし、4,096 個目の LF で密に写す（1 塊に最大 1 回）。その後は bit を立て、書いたバイトの区切りまで累積を書く（書きかけの密の塊で正しい区切りの累積は書いたバイトの区切りまでで、`at` はその範囲だけを探す）。書き終えた塊は不変。旧 piece は自分の byte の範囲と序数の窓の中だけを読むので、塊が疎から密へ変わっても答えは同じ。
7. **第二の索引・密度で切り替える別の class・測定専用の API を足さない。** 表現の選択は塊の中の実装で、`NewlineIndex` の外からは見えない。
8. **上限の正本は 1 つ。** `core::maximum_document_bytes = 64 MiB`（`src/core/DocumentByteLimit.hpp`）を application の開く道（`EditorController` の `maximum_file_bytes`）と adapters の退避の読み戻し（`Win32AutosaveAdapter::read`・旧 `maximum_autosave_body_bytes` は消した）が読む。値は変えない（上げるのは段 2 以降）。
9. **実測で示す。** payload（索引を作る間に増えた確保の byte 数）を 16 MiB の 3 本の本文で数える。数えるのは probe の exe（`tests/performance/CountedHeap.cpp`）で、Release は exe の `operator new` / `delete` を置き換えて `_msize` を、Debug は ASan の確保の統計を読む（どちらも同じ値になった）。製品に測る口は無い。結果は [記録](../quality/newline-index-2026-10-22.md): 全 LF 2,197,326 byte（0.131 倍）・1 行 60 byte の英文 583,894 byte（0.035 倍）・1 行 16 MiB 24,654 byte（0.0015 倍）。変更前はどれも約 3,407,9xx byte（0.203 倍）。`G` `gg`・16 MiB open・16 MiB 連打の速さは設計席が 8 本のゲートで示す（席は測らない）。
10. **限界。** 疎 / 密の境（4,096）は byte 数が等しくなる点で、ヒステリシスは無い（追記は片方向なので要らない）。`lengths` の両端の走査は最大 2 × 64 KiB で、今の 2 × 512 byte より多い（問いは frame ごとの数回なので許す）。費用は probe `newline-index-lengths-all-lf-16mib`（全 LF・両端が塊の途中の窓を 64 回）で設計席が測り、1 回が 50 µs を超えたら報告する。塊ごとの固定の費用（累積 8 byte・要約 32 byte・`std::variant` の 56 byte）は 64 KiB に約 0.15% で、1 行 16 MiB の本文の payload はほぼこれである。
