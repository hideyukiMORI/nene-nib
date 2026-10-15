# 5-ej — 映画の着想30種を追加し125配色へ（Issue #421・ADR 0117）

出典: docs/quality/gate-proofs.md の節 5-ej（2026-10-16 に D50 で移動・本文は移動時のまま）

既存95種の名前/順序/全色を維持し、Marvel12作品・その他18作品から独自30種を既存Theme/ThemeCatalogへ追加した。暗色22・明色8。FR-016、ARC-001/002/003/007/009、CPP-002/003/011/012/014/016、QLT-001/002/004/008/011/012/013、GIT-001〜004、Waivers none。

themes5585、ex-settings573、user-theme-values21、user-theme-selection97、theme-codec544、command-palette613 checksが成功。通常Debug/tidy/ASan/UBSan、C++6ファイル整形、規約0違反、core symbols0違反。旧95件のC++全要素とコンパイル済み色、新30件と設計値を照合し、125件のHTML見本と新30枚のカードを目視した。[対象・コマンド・証跡・準備失敗の記録](../cinema30-2026-10-12.md)。

レンダラー/選択/保存の実装、旧95配色、梱包機構は不変なので#419の実画面7件と梱包9 testsを再利用。新30色の契約と名称境界だけを対象にし、全件Vim/編集/性能は実施しない。Python/TypeScriptは役割の準備まで、保存schema不変。

ローカルpreview ZIPを正規経路で作り、公開beta.2は変更しない。統合/恒久収載/監査整理の実結果はD:/NeNeNib/evidence/cinema30-20261012/closeout.json。枝/commitとZIPは保持する。

#421の正規preview ZIPは製品commit b38a011で梱包成功（226.603秒、634904 bytes）。版/DLL/4エントリ/展開bytesを照合。PR422へ記録し、製品不変の結果を再利用する。
