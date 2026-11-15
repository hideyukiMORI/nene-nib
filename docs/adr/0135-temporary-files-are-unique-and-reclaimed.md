# ADR 0135 — 一時ファイルは一意な名前で排他的に作り、残ったものは自分で回収する

- 状態: 提案（設計席 2026-10-15。実装は #494 で）
- 日付: 2026-10-15
- Issue: #494（外部講評 2026-10-15 の P0-4・共通指示 §4-3「一時ファイル」・参照実装 nene-folio）
- 規則: ARC-007 / ARC-010、CPP-005、QLT-013、ADR 0010 / 0119

## 文脈

`src/adapters/win32/Win32FileAdapter.cpp` は、保存先 `<target>` に対して固定名 `<target>.nib-tmp` を作り、全断片を書いて `FlushFileBuffers` し、`ReplaceFileW`（保存先があるとき）か `MoveFileExW(MOVEFILE_WRITE_THROUGH)` で置く（ADR 0010 / 0119）。一時ファイルは共有 0 で開くが、置き換えの前に閉じる。事実確認（`D:\NeNeNib\outputs\winapps-review-20261015\fact-check.md` P0-4）:

- 普通の保存（`Mode::replace`）は一時ファイルを `CREATE_ALWAYS` で開く。排他的（`CREATE_NEW`）なのは新規保存だけ。
- 2 つの Nib が同じファイルを保存すると、片方が閉じてから置き換えるまでの間にもう片方が同じ名前を切り詰めて書き始め、途中の内容で置き換わる余地がある（元のファイルは壊れないが、保存した内容が相手の途中のものになる）。普通は片方の `CreateFileW` が共有違反で失敗して「保存できません」。
- 残った一時ファイル（クラッシュ・電源断）は誰も回収しない。固定名なので次の保存が `CREATE_ALWAYS` で上書きし、新規保存は `CREATE_NEW` で詰まる。
- 同じ adapter を設定・履歴・一覧・ブックマークも使う。

## 決定

1. **一時ファイルの名前は一意にする。** `<target>.nib-tmp-<プロセス ID の 16 進>-<連番>`。連番は**プロセスで 1 つ**で、保存ごとに進める（文書・設定・履歴…の adapter が別でも衝突しない。`InterlockedIncrement`・`<atomic>` は CNF-009 で使わない）。固定名を使わない。
2. **どのモードでも `CREATE_NEW` で排他的に作る。** 同じ名前が既にあれば（プロセス ID の再利用）連番を進めて最大 8 回まで試し、それでも作れなければ `unwritable`。共有 0 で開き、全断片の後に `FlushFileBuffers`、閉じてから `ReplaceFileW` / `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`（今日どおり）。
3. **残った一時ファイルは自分で回収する。** 回収の規則は「名前が `<target>.nib-tmp-<16 進の pid>-<10 進の連番>` の形で、(a) pid が自分でなく、そのプロセスが生きていない、または (b) pid が自分で、連番が今の保存より古い（払い出し済みの連番以下）もの」。名前の形は作る名前と同じ（小文字の 16 進・先頭の 0 なし・32 bit に収まる）ことを純関数 `reclaimable_temporary` で厳密に確かめ、`a.txt.nib-tmp-backup` のような使う人のファイルは消さない。生きているかは `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` で確かめ、開けて `GetExitCodeProcess` が `STILL_ACTIVE` なら生きている、`ERROR_INVALID_PARAMETER`（無い pid）なら生きていない、`ERROR_ACCESS_DENIED` とほかの失敗は生きている側に倒す（消さない）。当たったものを `DeleteFileW` し、共有違反などで消せなければ残す（pid が再利用された相手が開いている一時ファイルを消さない）。回収する時: (a) その `<target>` を保存する直前（文書・設定・履歴・一覧・ブックマークすべて）、(b) 起動時に設定の置き場（`LocalSettingsPath`）の `*.nib-tmp-*`。文書の置き場は起動時には分からないので (a) だけ。回収の失敗は無視する（保存は続ける）。
4. **置き換えの直前に、一時ファイルが自分の書いたものであることを名前（プロセス ID と連番）で確かめる。** 別のプロセスの一時ファイルを置き換えに使わない。
5. **試験（`nib_adapter_tests`・隔離した一時フォルダ）。** (a) 同じ保存先に 2 本の一時ファイルを同時に作れる（名前が違う・両方 `CREATE_NEW` が通る）。(b) `<target>.nib-tmp-deadbeef-1` を残した状態で保存すると、保存が通り、残骸が消えている。(c) 開いたまま（共有 0）の残骸は消えず保存は通る。(d) 設定の置き場に残骸を置いて `remember` / 設定の保存を呼ぶと消える。(e) 誰も開いていなくても、自分の pid の払い出していない連番・生きているプロセス（System の pid 4）の名前・形の違う名前（`backup`・大文字の 16 進）は消えず、自分の古い連番は消える。(f) 名前の読み `reclaimable_temporary` の単体の検査（形の外・生きている／いない・自分の古い／新しい連番）。(g) 試す 8 つの名前が塞がれると `create_new` も `unwritable` で、保存先は作られない。既存の保存の契約（ADR 0119 の断片・flush・置換の順）は変えない。

## 却下した選択肢

- GUID を名前にする: 一意だが「自分のものか」が名前から分からず、回収の規則が時刻に頼る。プロセス ID と連番で足りる。
- 回収を起動時だけにする: 文書の置き場は起動時に分からない。保存の直前が確実。
- 残骸の古さ（時刻）で回収する: 時計に頼り、長く開いている保存を誤って消し得る。名前の pid が生きているかで決める。（注記 2026-11-13・#1634: ここで退けたのは「古さ（何秒たったか）で決める」こと。自動退避の置き場の名前に入れる process の作成時刻は、等しいかだけを見る識別の値で、大小も経過も見ないので、これに当たらない・ADR 0137 の追補 #1634。一時ファイルの名前は変えない。）
- `DeleteFileW` が通るかだけで決める（この ADR の最初の版）: 別の Nib が一時ファイルを閉じてから置き換えるまでの隙間（共有 0 が外れる）の一時ファイルを消し、その保存を「保存できません」で失敗させる。起動時の回収 (b) は設定の置き場の全部（設定・一覧・履歴・ブックマーク）を歩くので、窓を閉じる Nib の書き込みと同時に別の Nib を起動しただけで起き得る（独立レビュー D-2）。名前の形を見ないので使う人の `<target>.nib-tmp-backup` も消す（D-1）。
- 連番を adapter ごとに持つ: 同じプロセスの別の adapter が同じ保存先の連番を重ね、「自分の古い連番」を判定できない。
- 一時ファイルを `%TEMP%` に作る: 別ボリュームになると `ReplaceFileW` / `MoveFileExW` が複写になり原子性を失う（ADR 0010）。保存先と同じフォルダのまま。

## 検証

- adapters の試験 (a)〜(d)、既存の `--files`（225 checks）・保存の契約、Debug（tidy / ASan / UBSan）、symbols・conformance。

## 限界

一時ファイルを置けないフォルダ（読み取り専用）では今日どおり保存できない。回収は生きているプロセスの名前を消さないので、生きている別の Nib の閉じてから置き換えるまでの隙間を消すことは無い。代わりに、pid が再利用されて生きている別のプロセスに当たった残骸、別のユーザーのプロセス（`OpenProcess` が拒否する）の残骸、自分の pid の払い出していない連番の残骸（前に同じ pid だったプロセスのもの）は回収されずに残る（害は無い。次の保存は `CREATE_NEW` で別の連番を使う）。`GetExitCodeProcess` が `STILL_ACTIVE`（259）で終わったプロセスは生きているとみなす。回収は 8.3 形式の短い名前で当たったものを消さない（名前の頭と `.nib-tmp-` を確かめる）。前の版の固定名 `<target>.nib-tmp` は形に当たらないので、だれも回収しない。一時ファイルの名前は保存先より最大 28 文字（`.nib-tmp-` 9・pid 8・`-` 1・連番 10）長く、前の固定名より最大 20 文字長い。exe の manifest に `longPathAware` が無い（#498）ので、パスが MAX_PATH に近い保存先は、前は保存できても今は `unwritable` になり得る。

## 追補（2026-11-15・#1689）: 置き換えの失敗の後始末は失敗の番号で決め、新しいバイト列を消さない

`placed_temporary` は `ReplaceFileW` の失敗の番号を問わず一時ファイルを消していた。backup の名前を渡さない呼び方（決定 2）での、失敗の後の 2 つのファイルの場所は Microsoft の文書（[ReplaceFileW の Return value](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)・2026-11-15 に読んだ）のとおり:

| 番号 | 文書の文（引用） | 元のファイル | 一時ファイル（新しいバイト列） | 始末 |
| --- | --- | --- | --- | --- |
| 1175 `ERROR_UNABLE_TO_REMOVE_REPLACED` | "The replaced and replacement files retain their original file names." | 元の名前のまま | 一時ファイルの名前のまま | 消す（今の形） |
| 1176 `ERROR_UNABLE_TO_MOVE_REPLACEMENT` | "Otherwise, the replaced file no longer exists and the replacement file exists under its original name." | 無い | 一時ファイルの名前のまま | 保存先の名前へ `MoveFileExW(MOVEFILE_REPLACE_EXISTING \| MOVEFILE_WRITE_THROUGH)` で置き直す。通れば保存は成功、通らなければ残す |
| 1177 `ERROR_UNABLE_TO_MOVE_REPLACEMENT_2` | "The replacement file still exists under its original name; however, it has inherited the file streams and attributes from the file it is replacing. The file to be replaced still exists with a different name." | 別の名前（OS が決めた名前）で残る | 一時ファイルの名前のまま・元の属性を継ぐ | 消さずに残す |
| ほか（`ERROR_ACCESS_DENIED` `ERROR_SHARING_VIOLATION` など） | "If any other error is returned, such as ERROR_INVALID_PARAMETER, the replaced and replacement files will retain their original file names." | 元の名前のまま | 一時ファイルの名前のまま | 消す（今の形） |

1. 始末は閉じた `enum class PlacementCleanup`（`remove_temporary` / `move_temporary` / `keep_temporary`）で、純関数 `placement_cleanup(番号, モード)`（`src/adapters/win32/PlacementCleanup`）が決める。番号は OS の開いた値なので、名前のある 1176 と 1177 だけを選び、残りは 1 か所の既定（消す）に閉じて注釈する。新規保存（`create_new`）は `ReplaceFileW` を使わないので、いつも消す（置き直しの `MOVEFILE_REPLACE_EXISTING` で後から現れたファイルを上書きしない）。
2. 失敗は今と同じ `FileFailure`（`unwritable` / `already_exists`）で返す。本文と未保存の印は application が今のまま持つ。知らせの文は足さない。
3. **回収（決定 3 の (a)）は、置き換えが成功した保存の後に行う**（前は保存の直前）。残した一時ファイルは名前が `<target>.nib-tmp-<自分の pid>-<古い連番>` で、決定 3 の (b) 自分の古い連番に当たる。直前の回収のままだと、失敗が続く次の保存がそれを消し、ディスクの上の新しいバイト列の写しが無くなる。成功の後なら保存先が新しいバイト列を持っているので、消してよい。失敗が続く間は消えない。決定 3 の回収の規則（名前の形と pid）は変えない。
4. 試験: 純関数の番号 → 始末（1175 1176 1177・`ERROR_ACCESS_DENIED` `ERROR_SHARING_VIOLATION`・新規保存）。実ファイルで、読み取り専用・ロック中は元のファイルが残り一時ファイルが残らない（今のまま）、自分の古い連番の残骸（残した一時ファイルと同じ名前の形）はロックで失敗する保存 2 回の間は残り、成功した保存の後に消える。1176 / 1177 は実機で起こせないので、偽の OS を足さず純関数だけで守る。

限界: 1177 の後に OS が別の名前で残した元のファイルは名前が分からないので、Nib は回収しない（使う人の目に残る）。起動時の回収（決定 3 の (b)・設定の置き場）は、前のプロセスが設定の保存で残した一時ファイルも消す。設定の保存が 1176 の置き直しにも失敗した後に Nib を閉じると、次の起動でその写しは消える（前のプロセスのメモリの設定も無いので、ここでは変えない）。
