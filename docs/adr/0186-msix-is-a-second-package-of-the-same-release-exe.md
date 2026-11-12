# ADR 0186 — MSIX は同じ Release の exe を包む 2 つ目の配布の形で、身元は表 1 つ・版は `project()` から導く

- 状態: 提案（実装席 MSIX1・2026-11-13・設計席の受理待ち）
- 日付: 2026-11-13
- Issue: #1581
- 規則: ARC-001 / ARC-012、CNF-007、QLT-007 / QLT-010 / QLT-011 / QLT-013、施主決定 D1 / D73 / D87 / D88

## 文脈

施主決定 D88 で NeNe Nib を Microsoft Store に 1.0.0 で申請すると決まった。Partner Center で名前は予約済みで、身元の値（Name・Publisher・PublisherDisplayName・Package Family Name・Store ID）がある。
配布はいま GitHub の ZIP（`eng/package-release.py`・ADR 0115）の 1 本で、Store に要る MSIX を作る道が無い。アイコン一式（ICO・MSIX のロゴ 25 枚・Store の 300 × 300）は ADR 0185 で入った。

手本は 2 つある。NeNe Loupe（`eng/package-msix.ps1`・ADR 0008）は ZIP の exe と同じバイト列を MSIX に入れ、身元を `eng/store-identity.json` に置き、ロゴ 25 枚と `resources.pri` を入れる。
NeNe Folio（`eng/release.py`・ADR 0080 / 0088）は manifest を道具が生成し、Windows SDK の版を固定し、MSIX の ZIP の中身と block map と unpack の結果を照合する。

## 決定

1. **配布の形を 2 つにする。GitHub の ZIP は残し、MSIX は Store へ出す物としてだけ作る**（GitHub Release には載せない・Loupe と同じ）。中の exe は ZIP と同じバイト列で、製品を MSIX のために作り直さない。
2. **入口は `eng/package-msix.ps1`（toolchain を通す）→ `eng/package-msix.py` の 1 本**。exe は `eng/build-release.ps1 -Ref HEAD` の出力（`build/release-<short>/NeNeNib.exe` と記録 `out/release/<short>.json`）を受け、ビルドしない。exe の検査（commit・SHA-256・x64・宣言した OS の DLL だけ・VERSIONINFO と `project()` の一致）は ZIP の道具の関数をそのまま使う。`--zip` を渡すと ZIP の中の `NeNeNib.exe` とバイト列を比べ、`--store`（申請用）では `--zip` を必須にする。
3. **身元・表示名・言語・最小と検証済みの Windows の版・固定する SDK の版は表 `eng/store-identity.json` の 1 つ**（CNF-007 の `config-bindings.json` に登録）。道具と試験は同じ表を読む。Package Family Name は Name と Publisher から計算（Publisher の UTF-16LE の SHA-256 の先頭 64 bit を 13 文字の base 32）して表の値と比べるので、Publisher の打ち間違いは道具が拒む。
4. **版は `project(NeNeNib VERSION x.y.z)` から `x.y.z.0`**。道具に版のリテラルを持たない。4 つ目は 0 だけ。`--store` は先頭が 0 の版を拒む（Store が受け付けない）。手元の確かめ（自己署名で入れる）は `0.1.0.0` でも作れる。版を 1.0.0 に上げるのは公開の手順で、この ADR は上げない。
5. **manifest は道具が表から生成し、リポジトリに置かない**（Folio と同じ）。最小の形にする。
   - `Windows.Desktop`・`MinVersion` `10.0.22621.0`（D1・Windows 11 22H2 以降。Loupe / Folio の `10.0.19041.0` は Windows 10 を支えるため）・`MaxVersionTested` `10.0.26100.0`（固定した SDK と手本の値）。
   - `EntryPoint="Windows.FullTrustApplication"` と `rescap:runFullTrust` だけ（Loupe と同じ形。申請の理由の文はこの EntryPoint を名指ししている）。
   - `Resources` は `en-US` `ja-JP` `zh-Hans`（D88 の表示言語）。最初の `en-US` が既定の言語（使う人の言語がどれにも当たらないとき OS は先頭を使う・manifest の直書きの文も英語・独立レビュー V-1589 の指摘で ja-JP 先頭から改めた）。
   - `uap:FileTypeAssociation`・右クリックのメニュー・`AppExecutionAlias` は入れない。
6. **中身はちょうど 30 個**: `NeNeNib.exe`・`LICENSE`・`THIRD_PARTY_NOTICES.txt`（ZIP と同じバイト列。md4c の MIT の告知を写しに添える）・`AppxManifest.xml`・`resources.pri`・`Assets/` のロゴ 25 枚。ZIP の `README.txt`（持ち運びの使い方）は入れない。SDK が足す `AppxBlockMap.xml` と `[Content_Types].xml` のほかに余計な物があれば道具が落ちる。
7. **ロゴの集合は `eng/app-icon.json` から**（`eng/render-app-icon.py` の `msix_logos`）。数が 25 でない・名前が違う・PNG の大きさが違う・`--verify` が落ちる、のどれでも道具が落ちる。`resources.pri` は Loupe / Folio と同じ手順（`makepri createconfig /dq ja-JP /pv 10.0.0` → `<packaging>` を消す → `new`）で作り、`dump` が身元の名前と 25 枚の候補を持つことを確かめる。
8. **SDK は版を固定して名指しで探す**（Folio と同じ）: `%ProgramFiles(x86)%/Windows Kits/10/bin/<windowsPackagingSdk>/x64/{makeappx,makepri}.exe` の実ファイルの版を表と比べる（QLT-011）。無ければ版と探した場所を書いた文で落ちる。PATH の道具は使わない。
9. **照合は 3 段**: MakeAppx の `pack /h SHA256` の後に (a) MSIX を ZIP として読み、項目の集合と各バイト列が配置と同じ、(b) `unpack` した木の集合と各バイト列が配置と同じで exe の SHA-256 が入力と同じ、(c) unpack した manifest の身元と版が表と `project()` のとおりで、道具の生成とバイト列が同じ。記録 `msix.json`（入力の exe・ZIP・SDK・`resources.pri`・MSIX の SHA-256 と大きさ・項目の一覧・版・`signed=false`）と `SHA256SUMS` を出力に書く。
10. **署名・導入・起動・申請をしない**。Store が署名する。自己署名の確かめ・WACK・申請は施主の手（管理者権限）で、手順は [docs/store/msix.md](../store/msix.md)。証明書と `.pfx` をリポジトリに置かない。出力は `D:/NeNeNib/releases`・`D:/NeNeNib/outputs`・作業木の `out/` の下の新しいディレクトリだけ。
11. **conformance の規則（CNF）は足さない**。manifest はリポジトリに置かず毎回表から生成するので、表と manifest の 2 つの写しが食い違う場所が無い。守るのは生成と照合の関数で、それは `tests/conformance/test_package_msix.py`（`eng/test-conformance.py` が CI で回す）の正例と反例が見る。SDK を通す 1 件だけは、固定した SDK が無い機械では理由を出して飛ばす。

## 結果

- ZIP と MSIX の exe が同じバイト列であることを、記録の SHA-256 で言える。
- 身元を変えるのは表の 1 か所。表の Name か Publisher の打ち間違いは Package Family Name の計算で拒まれる。
- Store へ上げる版は `project()` を 1.0.0 に上げてから作る。GitHub の公開の名前（`0.1.0-beta.N`）との関係は公開の手順で決める（Folio は `project()` を 1.x.y にして、GitHub の Release の題に「Beta」を付けて pre-release で出している）。

## 分からないこと（packaged の実機の確かめの後に別の ADR）

- 設定の置き場の仮想化: `%LOCALAPPDATA%\NeNeNib\` が packaged で `Packages\HideyukiMori.NeNeNib_3sft4ch5kzywy\LocalCache\Local\NeNeNib\` に写るか、ZIP 版の既存のフォルダがあればそこを使うか（Loupe の実測からの読み・未実測）。
- エクスプローラーへの登録（ADR 0178・D73 段 1）: packaged の HKCU への書き込みがエクスプローラーから見えるか。見えなければ Store 版で F1 の操作を隠し、manifest の `uap:FileTypeAssociation` と Windows 11 の最上位のメニュー（D73 段 2）で宣言する。
- full trust の子プロセス（LSP・ADR 0160）・`\\wsl.localhost`・印刷・一時ファイルの置き換え（ADR 0135）。
- WACK の結果（Loupe は DPI の WARNING 1 件で認定された）。
