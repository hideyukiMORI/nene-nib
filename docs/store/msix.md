# MSIX の作り方と、手元での確かめ・WACK・Partner Center へ上げる物

> Issue #1581・[ADR 0186](../adr/0186-msix-is-a-second-package-of-the-same-release-exe.md)・施主決定 D88。
> 道具（`eng/package-msix.ps1` → `eng/package-msix.py`）は MSIX を作って照合するだけで、署名・導入・起動・申請はしない。
> 証明書を作る・信頼の登録・`Add-AppxPackage`・WACK・申請は hide の手（管理者権限が要る）。`.pfx` と `.msix` をリポジトリに置かない。

## 何が入るか

| 項目 | 値・出どころ |
| --- | --- |
| 身元 | `eng/store-identity.json`（Name `HideyukiMori.NeNeNib`・Publisher `CN=C37230AA-B52D-403B-9BFD-E7980F088422`・PublisherDisplayName `Hideyuki Mori`・PFN `HideyukiMori.NeNeNib_3sft4ch5kzywy`・Store ID `9NCLGDTFQ529`） |
| 版 | `CMakeLists.txt` の `project(NeNeNib VERSION x.y.z)` → `x.y.z.0`。`-Store` は先頭が 0 の版を断る |
| 形 | `Windows.Desktop`・MinVersion `10.0.22621.0`・MaxVersionTested `10.0.26100.0`・`Windows.FullTrustApplication`・`runFullTrust`・言語 `ja-JP` `en-US` `zh-Hans`。関連付け・右クリックのメニュー・実行の別名は無い |
| 中身（30 個） | `NeNeNib.exe`（ZIP と同じバイト列）・`LICENSE`・`THIRD_PARTY_NOTICES.txt`・`AppxManifest.xml`（道具が生成）・`resources.pri`・`Assets/` のロゴ 25 枚（`src/app/msix/`・ADR 0185） |
| SDK | `%ProgramFiles(x86)%/Windows Kits/10/bin/10.0.26100.0/x64/` の `makeappx.exe` と `makepri.exe`（版 10.0.26100.7705・表で固定） |

## 1. 作る（設計席・席）

Store へ上げる物は clean な `main`（GitHub の main と同じ commit）から作る。先に ZIP を作り、同じ Release の exe を MSIX に包む。

```powershell
# 1) ZIP（Release のビルドもここで 1 回だけ走る・ADR 0115）
pwsh -NoProfile -File eng/package-release.ps1 -Beta <N> -OutputDirectory D:/NeNeNib/releases/<版>/zip -Verification docs/quality/<accepted-record>.md
# 2) MSIX（同じ build/release-<short>/NeNeNib.exe を受ける。ZIP の exe とバイト列を比べる）
pwsh -NoProfile -File eng/package-msix.ps1 -OutputDirectory D:/NeNeNib/releases/<版>/msix -Zip D:/NeNeNib/releases/<版>/zip/<ZIP の名前> -Store
```

- `-Store` は版の先頭が 0 なら断る。Store の 1.0.0 は `project()` を 1.0.0 に上げてから作る（版を上げるのは公開の手順）。
- 手元の確かめだけなら、作業枝の clean な木で `-Store` と `-Zip` を外して作れる（`eng/build-release.ps1 -Ref HEAD` の後に、出力は作業木の `out/` の下でもよい）。名前は `NeNeNib-<x.y.z.0>-windows-x64.msix`、`-Store` は `…-store.msix`。
- 出力のディレクトリ: `layout/`（包む前の配置）・`work/`（PRI の設定と dump）・`unpacked/`（MakeAppx が開き直した木）・`.msix`・`SHA256SUMS`・`msix.json`（入力の exe と ZIP と MSIX の SHA-256 と大きさ・項目の一覧・版・SDK の版と SHA-256・`signed=false`）。
- 道具が落ちる所: 表の身元が予約の値と合わない（PFN の計算）・版が `project()` と違う / 4 つ目が 0 でない / `-Store` で先頭 0・ロゴが 25 枚でない / 名前や大きさが違う・exe が Release の記録や ZIP と違う・SDK の版が違う・MSIX か unpack の中身の集合やバイト列が配置と違う・manifest の身元と版が表と違う。

## 2. 自己署名で入れてみる（hide の手・管理者の PowerShell）

Store に上げる `.msix` は署名しない（Store が署名する）。確かめには**写し**に署名して入れる。証明書の Subject は manifest の Publisher と同じでなければ入らない。

```powershell
# 1) 自己署名の証明書（CurrentUser\My に作る。.pfx に書き出さない）
$cert = New-SelfSignedCertificate -Type Custom -Subject 'CN=C37230AA-B52D-403B-9BFD-E7980F088422' `
    -KeyUsage DigitalSignature -FriendlyName 'NeNe Nib MSIX local test' -CertStoreLocation 'Cert:\CurrentUser\My' `
    -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')
# 2) 公開鍵だけを LocalMachine\TrustedPeople に入れる（管理者権限が要る）
Export-Certificate -Cert $cert -FilePath D:/NeNeNib/temp/nib-msix-test.cer
Import-Certificate -FilePath D:/NeNeNib/temp/nib-msix-test.cer -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople'
# 3) 写しに署名する
Copy-Item <出力>/NeNeNib-<版>-windows-x64.msix D:/NeNeNib/temp/nib-signed.msix
& "${env:ProgramFiles(x86)}/Windows Kits/10/bin/10.0.26100.0/x64/signtool.exe" sign /fd SHA256 /s My /sha1 $cert.Thumbprint D:/NeNeNib/temp/nib-signed.msix
# 4) 入れる・外す
Add-AppxPackage -Path D:/NeNeNib/temp/nib-signed.msix
Get-AppxPackage HideyukiMori.NeNeNib
Remove-AppxPackage (Get-AppxPackage HideyukiMori.NeNeNib).PackageFullName
```

- 入れた後は「スタート」の NeNe Nib から起動する。確かめる表は `docs/store/submission.md` の「packaged の実機の確かめ」（設定の置き場・F1 の登録・言語サーバー・`\\wsl.localhost`・保存・印刷）。
- 終わったら証明書を両方の置き場から消す（`Remove-Item Cert:\CurrentUser\My\<拇印>` と `Cert:\LocalMachine\TrustedPeople\<拇印>`）。
- 自己署名で入れた後に Store 版を入れるときは、先に `Remove-AppxPackage` で外す（同じ身元で署名が違う）。

## 3. WACK（hide の手・管理者）

Windows App Certification Kit は Windows SDK に入っている。**署名していない Store 用の `.msix`** を対象に回す（Loupe・Folio と同じ）。

```powershell
$kit = "${env:ProgramFiles(x86)}/Windows Kits/10/App Certification Kit/appcert.exe"
& $kit reset
& $kit test -appxpackagepath <出力>/NeNeNib-<版>-windows-x64-store.msix -reportoutputpath D:/NeNeNib/releases/<版>/wack/report.xml
```

- 画面から回すなら「Windows App Cert Kit」→「Validate Store App」→ パッケージを選ぶ。
- FAIL は 0 であること。WARNING と任意の試験の FAIL（Folio は「Blocked executables」）は、`docs/store/submission.md` の「認定の注意書き」の末尾に試験の名前と理由を 1 段落で足す。
- この席は `appcert.exe` を回していない（管理者権限が要る）。引数の形は Microsoft の文書による。

## 4. Partner Center へ上げる物（hide の手）

| 上げる物 | 所在 |
| --- | --- |
| パッケージ | `D:/NeNeNib/releases/<版>/msix/NeNeNib-<版>-windows-x64-store.msix`（**署名なし**。「パッケージ」の欄へ。上げると Partner Center が身元と版を読んで検証する） |
| 中身と SHA-256 の記録 | 同じディレクトリの `msix.json` と `SHA256SUMS`（上げない。設計席が `docs/quality/` の記録に写す） |
| 掲載文・画像・申告 | `docs/store/listing.md`・`docs/store/screenshots.md`・`docs/store/submission.md` |
| Store の 300 × 300 のロゴ | `docs/images/store/store-logo-300.png`（ADR 0185） |

`submission.md`・`listing.md`・`screenshots.md` は Store の文書の PR（#1582）で入る。

- 版は前に上げた版より大きくなければならない。同じ版で作り直したときは、上げる前に `msix.json` の `msix.sha256` が上げる物の SHA-256 と同じか確かめる。
- `runFullTrust` の承認の警告は想定どおり（理由の文は submission.md）。

## 確かめていないこと

- 自己署名での導入・起動・packaged での振る舞い（hide の手）。
- WACK の結果。
- `Assets/` に plated の `Square44x44Logo.targetsize-*.png`（altform の無い版）が無いことで、タスクバーの小さい大きさに困る所があるか（Loupe と同じ 25 枚・Loupe は認定された）。
