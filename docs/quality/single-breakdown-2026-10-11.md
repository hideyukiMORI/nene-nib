# 長い日本語行の単打内訳（Issue #406）

結論は **現条件では厳しい**。通常Releaseの上下Hは6.3525ms、左右Vは6.4180msで、旧総2msまで4.3525/4.4180ms（68.52/68.84%）必要。全ての通常試料が2msを超える。変更行の全文文字組みが支配的で、約4.3msを削る検証済みの具体策は得られなかった。将来の設計変更による達成不能を証明したものではない。旧split採用判定は保留のまま、閾値を変えない。

hideの今回だけの依頼に従い単体で実施。計画は[事前固定と容量欠陥への改訂](../plans/2026-10-11-single-breakdown.md)、全試料・対応差・生tick監査は[JSON](single-breakdown-2026-10-11.json)。先行#404のmainは18f3c4b、既存の継続改善は再開しない。

## 入力と境界

#404と同じ6,219,000 bytes/1,000行、1行6,217 bytes・2,077文字の文書（SHA-256 ef2d1511a696984dcd4184498a852d944a8ca366bf847f440d1e46b73b32a6c5）。窓/クライアント1280×800、座標80,80、DPI120、Cascadia Code13.5pt、毎回新しいprofile、通常モード、先頭へ暖機w→0.5秒→測定a→0.3秒。PostMessageWでWM_CHARを送る。IME合成なし。Aは19行、Hは9行×2、Vは19行×2。H/Vは同じframeを共有し、独立文書・scroll・二つ目frameの費用を測っていない。

既存input_received（WM_CHAR受信）からframe_presented（Present(1,0)後にrendererが成功確認して戻った直後）まで。OS post→受信と発光は含まない。追加時計は既存TimingPort経由でWin32TimingAdapterだけがQPCを読む。UIは節目だけを送る。tickと周波数、µs丸め、drop/clockFailedを保存し、終了後にJSON化する。時計・port呼出しの費用は隣接区間へ含まれる。新しいGetMetricsや文字組み強制呼出しは足していない。

親区間はinclusive、最深の子へ各event間隔を一度だけ帰属させる区分はexclusive。各試料の7区分の和とinput→frameのtickが一致し、18詳細試料すべて残差0。不正入れ子なし。別実装のinterval-tree差引きでも一致。個別中央値の和を総時間にしない。分類外を未分類へ残し、本文外UIなどを描画へ推測配賦しない。

## 通常版と計測負荷

各条件6試料。奇数A0,AQ,AP,H0,HP,V0,VP、偶数は完全逆順。0は通常Release、Pは詳細計測版、AQはAPと同一exeで追加時計/記録だけをadapterで無効化し、追加call・入力カウント判定は残す。A0は#402、H0/V0は#404の恒久Releaseをsource/exe SHAで照合して再利用。全6構成200 compile commandsのsource/build path・未build probeのcommit metadataだけを正規化すると一致。

| 条件 | 全6値 ms（時系列順） | 中央値 | min–max | 前後半安定条件 |
| --- | --- | ---: | --- | --- |
| A0 | 5.448, 5.381, 6.333, 6.689, 6.327, 5.078 | 5.8875 | 5.078–6.689 | 超過 |
| AQ | 6.477, 6.544, 5.158, 5.075, 5.109, 6.480 | 5.8175 | 5.075–6.544 | 超過 |
| AP | 6.785, 6.639, 5.247, 5.254, 6.620, 6.602 | 6.6110 | 5.247–6.785 | 範囲内 |
| H0 | 7.062, 6.545, 6.160, 5.050, 5.695, 6.729 | 6.3525 | 5.050–7.062 | 超過 |
| HP | 6.322, 6.906, 5.455, 4.987, 5.172, 5.985 | 5.7200 | 4.987–6.906 | 超過 |
| V0 | 6.246, 4.972, 6.590, 5.163, 6.592, 6.784 | 6.4180 | 4.972–6.784 | 範囲内 |
| VP | 6.553, 6.518, 7.087, 5.925, 6.436, 6.970 | 6.5355 | 5.925–7.087 | 範囲内 |

前後半3本の中央値差をmax(0.10ms,総中央値の10%)と比較。A0/AQ/H0/HPで超過。host/power/LastInput/依存SHA/exe SHAは前後一致したが、ノイズやスケジューラ要因の不在は意味しない。原因は未特定。全42試料を採用し、外れ値除去0・欠測0。

| 対応差 | 差の中央値 ms | 範囲 ms | 事前の負荷判定 |
| --- | ---: | --- | --- |
| AQ-A0 | -0.0730 | -1.614–+1.402 | 閾値以下（無負荷ではない） |
| AP-AQ | +0.1505 | +0.089–+1.511 | 無視できない |
| AP-A0 | +0.7755 | -1.435–+1.524 | 無視できない |
| HP-H0 | -0.6140 | -0.744–+0.361 | 無視できない |
| VP-V0 | +0.4020 | -0.156–+1.546 | 無視できない |

判定はabs(対応差中央値)>max(0.10ms,参照版中央値の2%)。AP−AQは時計/記録を足した観測差、AP−A0等はコード配置・call・容量の差と測定揺れも含む。HPの負値を高速化の証拠にはしない。通常版の総時間からPの区間を引いた補正値は作らない。細かな区間の節約量を通常版へそのまま適用できない。

## 重複を除いた内訳

単位ms、各列はその分類の中央値。総行は各試料の入力→提示から直接求めた中央値なので、列の中央値を加算した値とは一致しない。

| 分類 | A計測版 | H計測版 | V計測版 | 境界 |
| --- | ---: | ---: | ---: | --- |
| 編集 | 0.01700 | 0.01670 | 0.01755 | EditorController::apply（undo・EditorDeliveryを含む） |
| 文字列準備 | 0.57630 | 0.56165 | 0.58590 | controller.frame()とUTF-8→UTF-16。frameにはstatus等も含む |
| 文字組み | 4.75605 | 3.98785 | 4.81755 | layout照合/生成/Drawによる遅延評価・fallback・glyph収集 |
| 描画 | 0.71075 | 0.47800 | 0.60990 | draw_body_textとEndDraw。CPU側経過でGPU実行時間ではない |
| 待ち | 0.13190 | 0.13425 | 0.13990 | input_done→paint_enter、swap wait、Present。API内部の仕事も混在 |
| その他 | 0.02710 | 0.02740 | 0.02685 | renderer設定/表面取得の残り、資源解放 |
| 未分類 | 0.28150 | 0.53130 | 0.35830 | 本文外UI描画、caret等、明示scope外 |
| 入力→提示の総時間 | 6.61100 | 5.72000 | 6.53550 | 既存終端で直接計算 |

文字組みの試料ごとの占有率中央値はA71.68%、H70.27%、V72.63%。全18試料で68.06〜74.56%。Present復帰の追加節目から既存終端までの差もJSONに保持。待ちの大半はPresent内で、swap wait自体は約0.001ms、待ち全体は約0.13〜0.14msに留まる。

## 支配関数と削減候補

以下のscopeはinclusiveとexclusiveを明記する。親子関係があるので横断して足さない。ファイルは製品mainの経路、計測位置の差分は隔離branchと恒久snapshotに保持する。

| ファイル・関数 / scope | A/H/Vの中央値 ms | 根拠と候補の限界 |
| --- | --- | --- |
| src/ui/win32/Direct2DRenderer.cpp・layout_of → IDWriteTextLayout::Draw＋BodyGlyphCollector::take / layout_collect inclusive | 4.7303 / 3.9717 / 4.7848 | 各単打1回。全文keyが変わった先頭行を再構築。既存行の再利用は既に効いている |
| 同layout_collectからfallback/glyph_collectを除いたexclusive残差 | 3.8262 / 3.2960 / 3.8855 | DirectWrite内の解析・文字組み・列挙と境界費用。OS内部関数ごとの分離は未実施 |
| src/ui/win32/FontFallbackCache.cpp・map / fallback inclusive | 0.8046 / 0.5739 / 0.8155 | 1,081呼出し、OS委譲12回。key生成/find/copy等とsystem_fallbackを含む。短い要求cacheは既存実装 |
| 同map / system_fallback inclusive | 0.2180 / 0.1999 / 0.2198 | OS MapCharacters＋成功時retain/戻りまで。OS単体の純時間ではない |
| src/ui/win32/BodyGlyphCollector.cpp・collect | 0.0915 / 0.0956 / 0.0950 | 1,081回。画面外runの判定/字形保持を軽くしても、この区間全部が理論上限 |
| src/application/EditorController.cpp・frame → visible_lines → line_view | 0.5519 / 0.5419 / 0.5627 | TextBuffer::line_textとcore::display_line等で19行を準備。変更のない行の再利用は候補だが失効・所有の設計と実測がない |
| src/ui/win32/Direct2DRenderer.cpp・draw_body_text | 0.6547 / 0.4629 / 0.4840 | A19/H18/V38回。画面外run描画は既に省く。全消去は正しい画面を失うため実現値ではない |

text_layoutのCreateTextLayout自体は2回で約0.005ms、UTF-16変換は約0.020〜0.023ms。生成APIだけを改善しても主区間は消えない。編集は約0.017msで、挿入処理の高速化だけでは必要量に届かない。

実測できたのは上記の費用であり、新しい高速化の利益は未実測。局所候補の現実的な削減量について、正の下限や達成率を約束する根拠はない。fallback key/findの所有・探索改善、frame再利用、collector判定の軽量化は調査候補だが、その区間を100%消す上限でさえ約4.3msに達しない。

同一試料での上限試算（改善実績ではない）:

| 仮定 | Hの残る総時間 中央値 ms | Vの残る総時間 中央値 ms |
| --- | ---: | ---: |
| 文字列準備を全消去 | 5.1587 | 5.9548 |
| fallback全体とglyph収集を全消去 | 5.0448 | 5.6321 |
| layout_collectのexclusive残差を全消去 | 2.4198 | 2.7063 |
| 文字組みを50%削減 | 3.7265 | 4.1528 |
| 文字組みを90%削減 | 2.1313 | 2.2466 |

文字組み全体を0にした場合でもH中央値1.7315ms（範囲1.4834〜2.0412）、V1.7714ms（1.6374〜1.9369）。他の費用を固定して2msへ収めるための文字組み削減率を各試料で求めると、H中央値93.21%（85.26〜100.85%）、V95.19%（92.44〜98.75%）。これらは計測版内の仮想上限であり、通常版の改善予測ではない。

主区間を大きく削るには、変更行の再解析を避ける部分更新・区切りごとの保持などの設計検証が要る。現行ADR0069は全文/幾何をkeyとして全文をDirectWriteへ渡し、ADR0071は完結した字体要求だけを再利用する。見えている文字数で切る案や字体を固定する案には、結合文字・双方向・Tab・fallback・IME・HitTestを保つ証明がない。既存ADR0071でも字体明示の試作は文字位置差により不採用だった。安全な部分更新の範囲・利益・費用は未確認で、今回の実装や次の継続改善を開始しない。

## 失敗を含む検証と保存

初回は計画どおり51枠を一度実施したが、長行AP/HP/VPの全18本が記録容量65,536件を使い切り欠測（33有効）。起動描画の大量節目が原因。初組AP/HP/VPのdropは65,780/370/66,130、時計失敗0。全raw・旧exe・初版source・欠測・外れ値を保持し、成功へ書き換えていない。

19:00 JSTに再測定前の計画改訂をcommit240ee15へ固定し、adapterが追加節目を2個目input_received〜そのframe_presentedだけ記録する修正を一度行った。既存13節目は常時保持。修正版の長行42枠は19:08:34〜19:10:36 JSTに一度だけ実施、42有効。空文書9本は初版だけ（A0/AQ/AP中央値0.657/0.684/0.671ms）、修正版の対照へ流用せず、新旧を混ぜて集計しない。追加系列再試行0。

全14原PNGを目視。通常モード・行1桁3・wa入力後の本文/キャレット/UTF-8/CRLFを確認。初回4対と修正版3対は無mask全画素0px差、1280×800。A19行/H9行×2/V19行×2。同一frame共有試作の画面一致であり独立文書splitの機能受理ではない。

原コマンドのcwd・引数・開始/終了・native exit・stdout/stderrはsnapshot内outputs/20261011-single-breakdown/command-captureおよびcorrected/command-capture。実行コマンドとリスクは[gate-proofs 5-ec](gate-proofs.md#5-ec--長行単打の内訳と総2msの見通しissue-406)にまとめる。準備のcapture引数位置、File API未生成、依存凍結のpath判定、実行前format pathの失敗もsession-journalと原ログへ保持。ゲート・allowlist・除外は弱めていない。

統合対象は計画・本報告・JSON・gate-proofs・日報・引き継ぎの6文書のみ。計測用11ソースの変更はtest/406-trace-a,h,vに隔離し、製品mainの振る舞い・設定schema・公開APIは変更しない。診断rawだけにtick/frequency/drop/clockFailed/detailを追加。Waivers: none。

恒久先はD:/NeNeNib/evidence/single-breakdown-20261011/snapshot-406。原絶対pathはpath-mapping.jsonで追跡し、SHA付きmanifest、旧/新exeと全build metadata、初回依存凍結、source/差分/Git bundleを残す。通常draft PR→自己レビュー→Ready→必須CI→eng/merge-pr.py→main同期後に、作業用4worktreeとOUTだけを監査整理する。branch/commitは保持。統合・収載・整理の実完了は同evidence/acceptance-406/closeout.mdへ記録し、この調査の区切りで停止する。
