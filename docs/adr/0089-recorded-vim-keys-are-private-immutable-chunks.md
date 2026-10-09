# ADR 0089 — 記録したVim鍵列は私有の不変chunkとして共有する

- 状態: 受理（工程3の設計、性能・製品統合の受理は別）
- 日付: 2026-10-09
- Issue: #323
- 影響する規則: ARC-001 / ARC-004 / ARC-005 / ARC-008 / CPP-002 / CPP-003 / CPP-004 / CPP-007 / CPP-008 / CPP-016 / QLT-001 / QLT-012 / QLT-013 / QLT-014

## 文脈

VimStateのrecording/last_change/macro_recordingは所有vector<VimKey>を持つ。状態を写すたびに確定済み鍵も複製され、録画2000入力の前測定では200入力より大きな費用が残った。履歴の工程1とregister本文の工程2とは別枝で、64f0ab5をbaseとして鍵列の費用を分離する。本文・履歴・レジスタ・VimInsertRepeatのcounted text・成長中insertedは本工程で変えない。

## 決定

1. core::VimRecordedKeysは不変値とし、private storageだけがvector<shared_ptr<const vector<VimKey>>>（chunks）を所有する。chunk容量は64鍵、最後以外は64、最後は1〜64。空列はinlineの空chunks、非空列はshared_ptr<const chunks>を持つvariantで表す。public constructorやpointer/span/可変collectionの読み口を作らず、null entryを生成しない。
2. 唯一の公開factoryはfrom(std::span<const VimKey>)。検索patternの所有stringを含め、外部のVimKeyを必ず防御コピーして64鍵ずつconst chunkへ置く。空spanはinline空列。raw vector/keyをmove受理する別口は作らない。private constructorは内部で作った所有済みstorageだけを受ける。
3. appended(const VimKey&)は新しいsnapshotを返す。outer pointer列をコピーし、末尾が64未満なら最大63既存鍵と新keyの防御コピーから新const chunkを作って末尾だけ置換する。64/空なら1鍵のchunkを追加する。outer列はprivate owned localから新const chunksへmoveし、旧snapshot/payloadは書き換えない。通常snapshot copyはO(1)。appendのpointer列コピーは鍵数/64に比例し、鍵中の長い検索patternを含む末尾copyも残る。全体O(1)や完全線形とは主張しない。
4. owned_keys()だけが必要時にvector<VimKey>の独立した所有値を返す。製品ではreplayed_visual/repeated_changeの再生入口とstopped_macroのregister text確定でだけ平坦化する。VimReplay.keys、controller queue、vim_register_text/numbered_advanced/replayed_keysのraw owned/span APIと唯一の再生アルゴリズムは維持する。state copy/通常appendでは平坦化しない。
5. 明示default copy constructor/assignmentで暗黙moveを抑え、snapshot rvalueも共有copyする。元と先を有効に保ち、共有pointerのnullを「移動済み」の意味に使わない。raw外部入力の防御copyとは別契約。public default/implicit変換constructorは足さない。
6. VimRepeatRecord.keys/VimMacroRecording.keysをVimRecordedKeysへ置換し、空member initializerは唯一factoryを使う。両型はメソッド無しaggregateのまま。明示aggregateの空/1鍵初期値もfrom(empty span)またはfrom(empty span).appended(key)へ移す。追記は既存appended(record,key)/macro_recordedの2箇所だけで、新snapshotを代入する。replayableによる検索起点from消去の順序は既存どおり。container factory自体はdirection/fromを含む鍵の値を保存する。
7. private aliases/補助関数で1file1主要型と複雑度を守り、storageのstd::visitは2型を明示して網羅する。default/else/抑制・可変cache・flag・旧実装切替・gate/baseline変更は作らない。記録開始/停止/取り直し、dot/macro replay、count/visual、Undo単位、取消/IME/タブ切替と保存schemaは変えない。

## 対象検証と成果物

- 新--vim-recorded-keysで0/1/63/64/65/127/128/129の順序、旧snapshotから二枝、raw vector/長いpatternの古いdata alias、所有平坦化結果の改変による隔離、rvalue copy構築/代入後の元と先、異種鍵とsearch direction/fromを確認する。既定実行も同じ関数を使う。既存keys比較はowned_keys経由へ機械的に移し、期待値/1853fixtureとharness12本は維持する。
- Debug nib_tests+NeNeNib、strict/tidyを並列2・自身の同時build1本で実行。新selectorと--application（状態copy/既存Vimfixture）、--vim-dot、--vim-macro、--vim-search-incrementalだけを選ぶ。後者3本の専用契約はapplication scopeに含まれないため必要で、selectorのfixture重複は現CLIの制約として記録する。他scope/全件checkは実行しない。
- 変更format/source/document/configuration conformance、File API graph、core/application symbols、protected diff/Git/whitespaceを確認する。今回原因の失敗だけ修正して最小再試行。入力が不変の成功結果は再利用する（ADR0021・QLT-001/012）。waiverなし。
- clean commitから正典eng/build-release.ps1 -Ref HEADを並列2で製品buildし、その同Release cacheを明示configureしてnib_perf_probes targetだけ並列2でbuildする。製品/比較exeのSHA/commitを別々に記録し、GUI/速度本番/PR/push/mergeは親が担当する。probeを勝手にrunしない。

## 限界と次工程

pointer列と末尾最大64鍵の所有copy、平坦化時の全鍵copy、VimInsertRepeatの成長中text、EditHistoryの成長中inserted、register本文の費用は残る。本工程を#323全体完了としない。親が固定前測定と同一harnessで対応比較し、正式GUI benchmark/基準値採用とは分けて受理する。

作業木はD:/NeNeNib/worktrees/323-shared-keys。out/reports/done-323-recorded-keys.mdへ規則/変更/対象検証/失敗と修正/再利用/waiver/残る費用を記録する。親の測定参照中は木・build・outを保持し、統合と証拠収載後に親が安全確認して整理する。branchとcommitは保持する。他木/register席は編集しない。
