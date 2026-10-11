# ADR 0115 — ベータ配布物は受理済みmainの同じRelease経路から作る

- 状態: 技術受理（ツール9 tests・実PE検査・dirty実呼出し拒否を確認。実配布はmain統合後）
- 日付: 2026-10-11
- Issue: #413
- 影響: ARC-001/002、QLT-001/011/012/013、GIT-001〜004

## 文脈

hideのD44は折り返し・通常ドラッグを含むベータのタグとZIP公開までを求める。DEVELOPMENT_WORKFLOW第9節は配布物の内容とmainからの再生成を定めているが、正規生成コマンドがまだ無い。実機確認用の`eng/build-release.ps1`は既に正典である。

## 決定

1. `eng/package-release.ps1 -Beta <正整数> -OutputDirectory <D側の新規ディレクトリ> -Verification <追跡済み検証記録の相対パス>`をベータの梱包入口にする。Python標準ライブラリの実装へ渡し、ビルドは既存`eng/build-release.ps1 -Ref HEAD`だけを呼ぶ。製品を起動せず、タグ作成・公開もしない。
2. cleanな作業ツリーを要求する。通常はmainまたはそのcommitのdetached作業木で、HEADがGitHubのorigin/mainの現在値と一致する必要がある。D側のdetached作業木により既存C側のmainを動かさず配布ビルドを作れる。`-Preview`はcleanな作業ブランチでツールを確かめるためだけに用い、その記録を公開用として扱わない。既存の`build/release-<SHA>`があれば停止し、削除や増分ビルドによる代用はしない。
3. 版の数値はCMakeのproject VERSIONだけから読み、配布の識別子に`-beta.<番号>`を付ける。PEの製品版は元の数値、ファイル版は同じ数値と末尾0で照合する。ベータ番号をCMakeや製品コードに重複して置かない。
4. x64 PE、埋め込み版、Release記録のcommitとexeのSHAを検査する。DLL名は`eng/architecture.json`のappから到達できるproduction moduleの`platformLibraries`から導出し、未宣言依存を拒否する。実行時依存追加は今回扱わず、`runtimeDependencies`が空であることを要求する。
5. 出力は`D:/NeNeNib/outputs`または`D:/NeNeNib/releases`配下の新規ディレクトリだけとし、親のリンク/ジャンクションを拒否する。ZIP直下は`NeNeNib.exe`・テンプレートから導出した`README.txt`・リポジトリの`LICENSE`の3点だけ。作成したZIPを別の新規フォルダへ展開し、全エントリ名とbytesを照合する。SHA256SUMSにはZIPのSHA-256を載せ、隣のpackage.jsonに出典・toolchain・依存・版・署名有無・展開照合を残す。
6. 必要な規約検査・製品検証は追跡済みの検証記録を参照し、梱包のたびには繰り返さない（QLT-012）。ツール自体は正例と反例で、汚れた作業木・既存出力・リンク・版不一致・未宣言DLL・余分なZIP項目等の拒否を確認する。
7. #410と梱包ツールのPR/CI/main統合後、cleanなmainの新規Releaseを梱包し、調整済みの短い実機起動を行う。その後だけ同じcommitへ`v<配布版>`のtagとGitHub pre-releaseを作り、ZIPとSHA256SUMSを添付する。再ダウンロードした両者とローカルSHAを照合する。署名・インストーラ・自動更新は別Issueのまま。

## 代替案

- feature branchで測定したexeをそのまま公開する: squash後のmainから作り直す既存方針に反する。
- 独自のCMakeコマンドで梱包用Releaseを作る: ビルドの正典が二つになる。
- 梱包で全製品テストを無条件に再実行する: 関連入力が不変でも重複するためQLT-012に反する。
