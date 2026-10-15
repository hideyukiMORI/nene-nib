# 5-ee — 折り返しと通常モードのドラッグ（Issue #410・ADR 0114）

出典: docs/quality/gate-proofs.md の節 5-ee（2026-10-16 に D50 で移動・本文は移動時のまま）

D44に従い、共通BodyTextLayouts/TextLayoutPortと画面行/論理行、caret affinity、composition、Vim viewportの測定要求、通常dragのcapture/edge scroll、settings.v3/session.v2を実装した。既存renderer本文cacheは撤去。規則ARC-001/003/007/010/012、CPP-002/004/017、QLT-001/008/012/013/014、GIT-001〜004、FR-002/003/012/014/015/019。Waivers none。

各対象の退行理由、実command/cwd/所要時間/結果/全途中失敗/再利用は[対象検証の正本](../wrap-beta-2026-10-11.md)。原記録はD:/NeNeNib/outputs/20261011-wrap-beta/commands、GUIはgui-02/gui-ime-03/gui-vim-ime-04/gui-session-03/gui-edges-04/gui-horizontal-fix-01、性能全生データはruntime-02/runtime-fix-01。最終製品commit32732046666e98b40d8652c6872a6534f74fae91のRelease SHAは9e332f8b9895de7b5b4a60242423b906a50cb5cba34fd80ecf3e8f11416b2bd9、1,483,776 bytes。

主要契約: wrapped-view31、vim-wrapped2555、application16009、frame-selection56、ordinary-characters233、tabs291、ex-settings225、palette495、user-theme97、session203、settings adapter386、session adapter499、theme catalog303成功。新nowrap画素を含むwindow text-layout961と既存glyphs1484は0 failures/0 not measured。通常Debug/tidy/ASan/UBSan、conformance/symbols、28性能fixtureのNoTiming照合を実施。全件ゲートは選ばず、C側旧exeを誤起動した2原記録は証拠から除外した。

実機DPI120/指紋bc8a356f37c68491。固定45試行でA/N/Wの起動223.6004/222.4695/228.4467 ms、単打0.543/0.539/0.605 ms、長行5.105/10.072/5.196 ms。nowrapの退行を検出し、横範囲の可視字形だけ保持する修正後の固定10試行でN5.654/W5.385 ms、両条件QLT-014内、欠測0。旧A比nowrap+0.549 msを残る費用として記録する。wrapは見える論理行数が異なり同等作業の高速化率とはしない。途中停止・初回失敗・外部IME窓の撮影重なりも保持し、閾値/基準を変えず、成功試料だけに取り直していない。

自己レビューで正典所有・失敗伝播・版移行・捕捉終了・描画資源の無効化を照合し技術受理。関連入力不変の対象結果と初回起動/短い単打を再利用し、字形修正の横scroll/画素/長行だけ再確認した。既存Vim3件の桁差は#411、gj/gk等・全monitor/DPI・全字体は未対応/未測の区別を維持。通常PR412/CI/main後に#413へ進む。恒久先D:/NeNeNib/evidence/wrap-beta-20261011、実統合/整理はそのcloseoutで記録する。
