# ADR 0121 — 改行索引の容量は本文のバイト数で制限する

- 状態: 設計決定・対象契約確認済み、実機性能未成立で保留
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
