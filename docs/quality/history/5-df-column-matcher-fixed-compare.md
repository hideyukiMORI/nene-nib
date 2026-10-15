# 5-df — 桁位置と照合器の固定比較（#348A / #350）

出典: docs/quality/gate-proofs.md の節 5-df（2026-10-16 に D50 で移動・本文は移動時のまま）

規則ARC-001/003/005/007/008/009/012、CPP-002/004/005/008/012/014/016、QLT-001/008/012/013/014、D41。ADR0094を先行してoffsetを共通visitorの停止へ移し、ADR0093を先行して照合の再帰をprivate優先状態列へ移した。原記録は`D:/NeNeNib/outputs/20261010-speed-stability/`、有限比較は`outputs/20261010-pattern-design/`。基準値/fixture/schema/抑制/allowlist不変、waiver none。

### 桁位置の比較

`python compare-offset-a.py`の事前planで20iterations・3 ABBA blocks・warmup1区間外・120対応組を固定。各区間は128回の同じ変換と全結果照合、通常Release。B1時点a2d534dとA追加2e719acの比較であり、B1のraw/exeを上書きしない。全4条件observed、外側retry/除外なし。

| 条件（57344bytes） | before中央値µs | after中央値µs | 対応比中央値 | 短縮組/120 |
| --- | ---: | ---: | ---: | ---: |
| 長行先頭 | 155.5 | 1 | 0.006667 | 120 |
| 長行中央 | 3328.5 | 3118 | 0.933786 | 111 |
| 長行末尾 | 6516 | 6272.5 | 0.964706 | 108 |
| 8193断片中央 | 8968 | 5719.5 | 0.636695 | 120 |

先頭after1〜3µsは分解能近傍。中央と末尾の全試料改善は主張しない。初回nesting4の拒否をflattenし、symbolsの未初期化失敗は正規toolchainを初期化して確認した。buffer-range282checksと規約/symbols成功。独立`review-348-a.md`は受理阻害なし。要求容量やRSS改善の計測は行っていない。

### 照合器の初期比較と失敗

同じ48workloadのharness（追加7はADR0082で先行固定）を旧製品とcd972c8候補へ適用。`python compare-pattern-350.py`は5iterations・3 ABBA・30対応組、1区間16matchedを事前固定した。正常6条件は全観測。

| 条件 | before中央値µs | 初期候補中央値µs | 対応比中央値 | 短縮組/30 |
| --- | ---: | ---: | ---: | ---: |
| a*b不一致1024 | 168273 | 505 | 0.003012 | 30 |
| a*b不一致2048 | 654599.5 | 1007 | 0.001538 | 30 |
| a*b不一致4096 | 2558359 | 2012 | 0.000787 | 30 |
| a*a*a*a*b不一致32 | 352023 | 40 | 0.000112 | 30 |
| a.*a成功4096 | 1282.5 | 2066 | 1.610136 | 0 |
| literal末尾4102 | 622 | 458 | 0.734727 | 29 |

旧4096literalは最初のDebug smokeでASan stack-overflow、Release smokeで0xC00000FD。いずれもwarmup中でmetadata/marksなし。既知失敗を再試行せず比較の7本目は欠測、比率は作らない。候補単独は同じ4096literalの完全照合/marks/正常終了を確認。harness初回のnesting4拒否とgenerator抽出誤りを残し、表/期待/flagsを変えず補助関数の境界を直した。製品初回のnesting/optional診断と修正も保持。最終3検索scope1606checks成功。

### 同位置消費の共有と採否

初期greedyの悪化を受け、ADR0093追加決定ea09079を先にcommitし、0bd669fで消費判定を共用した。優先順/4vector/意味は不変。同位置の文字取得/前進を一度にする。`python compare-shared-consumption.py`は20iterations・3 ABBA・120対応組、16matched/区間を事前固定。正常な初期候補と最終候補の7条件に加え、旧版対最終版のgreedyだけを直接比較した。全8observed/外側retryなし。

| 条件 | 初期中央値µs | 最終中央値µs | 対応比中央値 | 短縮組/120 |
| --- | ---: | ---: | ---: | ---: |
| a*b不一致1024 | 505 | 396 | 0.783810 | 120 |
| a*b不一致2048 | 1008 | 787 | 0.780318 | 120 |
| a*b不一致4096 | 2011 | 1572 | 0.781701 | 116 |
| 複数star不一致32 | 40 | 31 | 0.756757 | 120 |
| greedy成功4096 | 2059 | 1632 | 0.792031 | 120 |
| literal末尾4102 | 458 | 458 | 1.000000 | 50（同値25/遅い45） |
| literal4096 | 377 | 377 | 1.000000 | 23（同値41/遅い56） |

別の直接比較の旧版→最終版greedyは1212→1633µs、対応比1.350581（範囲1.003688〜2.111511）、全120組で遅い。中央値差421µs/16=26.3125µs/照合は算術値でGUI応答の実測ではない。初期悪化61%を隠さず、異なるbatchを合成して改善率を作らない。再帰と組合せ探索を除く安定性、原子数に閉じた保持量を優先し、greedy約35%の代償込みで採用する。全条件高速化・平均/体感無影響・RSS改善は主張しない。

共用消費後のDebug検索3scope1606checks、source規約と完成core.libのsymbols成功。最初にobjectを渡したsymbolsで内部6参照が未宣言扱いとなったrawも保持し、allowlistは変更しない。新Release probe112steps成功、exe SHA256 `22AB4579BDA49379DA20C2CCC6CD893D1742581375E40D6087316A3D809F9BD4`。公開APIの有限C++は同じ14token/短本文/anchor/from系列1,382,940行、23,202,990bytesを旧版・初期版・共用版で全byte一致確認。SHA256 `8b4e57d670b3c41a636ae3ce4c0e028e1a7bf1d37e47645481b33dbea4f8f7c1`。`finite-cpp-shared-comparison.json`、build/run scriptと初回compile/link指定失敗も保存。これは有限範囲のC++比較でVim oracleや任意入力の証明ではない。UTF8検証済本文/codepoint先頭の既存前提外へ保証を拡張しない。
