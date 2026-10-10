# 高速化26候補の採否 — 2026-10-10

原調査のC1〜C9・R1〜R8・IO1〜IO9を同じ番号で追う。20件は全部または一部を採用、2件は実験不採用、4件は本工程で見送り。R3はblockに続き#362でtintも専用比較して採用した（gate-proofs 5-dk）。C1/R1等の重複を独立の速度効果として加算しない。全ての将来の最適化や全26件の速度実験を尽くしたという記録ではない。

数値・コマンド・検証の正本は[gate-proofs 5-cz〜5-dc](../quality/gate-proofs.md#5-cz--読込状態履歴描画の閉じた比較issue-320324330333335)。詳細な旧台帳・調査・固定plan・全raw・前後exeは `D:/NeNeNib/evidence/speed-optimizations-20261009/` にSHA/元パス/commit対応付きで収載する。

| 原ID | 候補 | 判断と範囲 |
| --- | --- | --- |
| C1 | applyとpaintのframe二重構築 | #322採用。意図の反映値を軽量化し、frameは必要時に一経路で生成 |
| C2 | 1意図のEditorStateの反復copy | #321採用。内部一時値の所有を渡し、no-op更新を省く |
| C3 | 履歴・レジスタ・記録鍵列の深いcopy | #323採用。private不変値を共有。成長中の入力やpointer列等の残る費用はある |
| C4 | display_lineの再encode省略 | #324実験不採用。20→21usで利益未確認、元の実装へ復元済み |
| C5 | 行取得時のpiece再走査 | #338採用。範囲走査を共用し、必要な位置で終える。全piece索引の追加ではない |
| C6 | 候補ごとのlowercase写し | #339採用。ASCII case比較を同じ照合器内で行う。場所＋名前の一時連結も#358の固定Count案で除去（5-di） |
| C7 | query追加時の全候補照合 | #339採用。不変候補の前結果を部分対象として再採点。削除/途中編集/出どころ変更/候補到着は全照合 |
| C8 | frameごとのpattern再parse | 確定検索を#376/ADR0102で独立比較して採用。frame/nNは不変解析値を借用、rawも共有。入力中の解析保持は対象外。固定利益/費用と代償はgate-proofs 5-do |
| C9 | position_ofのprefix文字列copy | #338採用。pieceのviewを同じ文字数計数へ渡す |
| R1 | deliverとpaintの二重frame | C1と同じ#322。独立の改善量を加算しない |
| R2 | 行番号の反復文字組み | #324採用。現在/直前の保持値と既存失効境界を使用。単独速度は未分離 |
| R3 | block/tintの行全体再描画 | #335でblock、#362でtintを既存保持glyph経路へ。tintは混在長行で大幅短縮、ASCII横ばい/短行4.5µs増（5-dk） |
| R4 | palette題名の二度文字組み | #324採用。同じlocal layoutで描画と計測。単独速度は未分離 |
| R5 | 毎frameのGetBuffer/bitmap作成 | #324採用。所有する描画先を既存resize/device境界まで保持。単独速度は未分離 |
| R6 | UTF16位置換算のprefix確保 | #324採用。既存のscalar検証と長さ計数を共用。単独速度は未分離 |
| R7 | 録画表示のmode文字組み | #335採用。既存status slotを借用。R3との組合せ測定 |
| R8 | closed IME状態の問い合わせ省略 | 未実験で見送り。OSの現在値を保証する通知/失効境界が未成立 |
| IO1 | UTF8の二重検証 | #320採用。防御copy後に判定済みのprivate所有値を本文へ渡す |
| IO2 | 全文copyとread前のzero fill | #320採用。外部rawの防御copyを残し、内部移管と一回書込を使用 |
| IO3 | 改行索引のbyte走査/再確保 | #320採用。既存find/countと初回reserve。独自SIMDは追加しない |
| IO4 | ASCIIの一般UTF8検証 | #330採用。範囲内word読取とscalar fallbackを同じ検証器内で使用 |
| IO5 | ThinLTO | #333実験不採用。現symbolsで違反、主要区間の利益も未確認。flags/allowlistを緩めない |
| IO6 | 描画DLLの遅延load | 未実験で見送り。delayimp依存とDLL/entrypoint不足のSEH失敗境界が未設計 |
| IO7 | 引数openをShowWindow後へ | 未実験で見送り。総仕事を減らさず、初回画面/失敗提示/節目の意味を変える |
| IO8 | read/decodeとdeviceを並列化 | 未実験で見送り。worker一本のrequest/completion/取消/復元/初回frameの別設計が必要 |
| IO9 | CP932変換の二周/予約/copy | #340 A/B/C採用。内部成功値の移管、日本語等のUTF8出力予約、限定したCP932一回変換。容量の代償は下記 |

## 採用の限界と保留の理由

#340のA（内部copy移管）は対応比0.986で分布が重なり、単独の速度利益を主張しない。Bは日本語と補助平面で改善を観測したが、ASCIIでは改善未確認。要求する初期capacityはASCIIでも最大3bytes/unitで、従来のunit数の3倍。CはCP932の1〜2bytes→UTF16一単位という一次仕様に限定し、wideの要求capacityは2byte日本語では実使用の最大2倍。実メモリ使用量やRSSを測った倍率ではない。この代償を許容して採用し、汎用codepageへ一般化しない。

#339はopened/extendedの公開raw境界で防御copyを一回行う。移動前に得られた可変aliasでsnapshotが書き換わる問題を独立レビューで修正し、その不変性の上で部分対象を使う。固定4区間と正式palette入力benchは入口のcopyを測らず、一覧作成/到着全体の高速化を実証していない。caretはafterの20試料中10が0usとなり、比較器が分解能未満として拒否した。原記録を残し、0を補正せず、倍率は算出しない。

C8に必要なのは、入力中SearchLineと確定したVimSearchPatternの異なる寿命、空の再利用、向き、未対応構文、IME、highlight切替を一つの不変の解析済み所有値へ閉じる設計。公開aggregateのraw文字列へ可変cacheだけを足す案は採らない。正規のimmutable値を設計すること自体は禁止されていない。当初は未実装・未実験で見送ったが、#376/ADR0102で確定済みだけの不変所有を先行設計し、検索専用比較と通常state copy/短履歴/入力中/確定の費用を含めて採用した。長3条件の利益と5費用条件が成立し、短履歴200入力+10.5µs・入力中64frame+7µsと解析結果の寿命までの保持を代償として明記する。入力中解析の保持は未実験のままで、C8全範囲の最適化完了を意味しない。

R3のtint_runsは#362で専用比較した。本文/一覧の実IME、双方向run、origin/clip/NONE/fallbackを37場面で確認し、通常Releaseの混在1024置換は約1793→20.6ms、120組すべて短縮。ASCIIは横ばい、短行は4.5µs増。一覧入力行はbody_layoutsへ登録せず既存fallbackを保ち、取得成功の空glyph列と取得不可を区別する。blockの成功をtintの代用にせず、新しい実機同値/計測/独立レビューで採用した。全入力/任意字体/DPI/資源失敗/RSSは未測（gate-proofs 5-dk）。

R8の復帰用ime_open_をOS現在値のcacheと見なさず、focus/TSF/context/言語変更の通知保証を先に設計する。IO6は初回呼出しへ費用を移すだけになり得るので、起動と初回frame、依存欠落の失敗を同時に確認する必要がある。IO7は文書が編集可能になるまでを評価し、空画面の早出しだけを同じ仕事の高速化としない。IO8は共有可変core/rendererや第二decoderで迂回せず、typed portの状態遷移とqueue/read/変換/適用全体を先に規定する。これらの見送りは永久禁止や利益なしの実証とは区別する。

元調査の推定msを実測へ繰り上げず、重複候補や別batchの削減率を足さない。設定schema、基準値、許容値、抑制、allowlistの変更なし。waiver none。

## 追加実験: 完全画面外clip #364

R3の採用済みglyph経路とは別の追加実験。tintのtarget外clip省略は混在で短縮を観測したが、一覧IME1場面の原6px差により固定全画素条件が未達、ASCII固定比較も旧版終了Timeoutで欠測となった。条件の事後限定や成功試料の補完をせず実験不採用。二挿入を戻し#362の受理済み実装を保つ。外部窓所有は#365へ分離したが製品退行の有無や具体的生成原因を断定しない。原26候補の採否件数を変更するものではない。全結果/失敗/未測はgate-proofs 5-dl。

## 追加実験: 検索preview位置の反復変換 #378

C8入力中parse保持とは別に、同じpreview位置のUTF-8prefix走査を可視行ごとから一画面一度へ移した。新しい永続cacheなし。固定長30/120行の64frameで対応比.337326/.177893、各120組短縮、5費用条件も成立。直接契約/GUI12場面/正式2指標と独立レビューにより技術受理（[gate-proofs 5-dp](../quality/gate-proofs.md#5-dp--検索プレビューの位置変換を一画面に一度へ移すissue-378adr0103)）。元26件の20採用/2実験不採用/4未実験を変更しない。C8入力中parse保持、R8、IO6〜IO8は未実験のまま。
