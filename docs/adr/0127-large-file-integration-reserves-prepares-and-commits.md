# ADR 0127 — 大容量本文の接続は資源を予約し、準備した操作と保存版を一度だけ確定する

- 状態: 受理（製品接続の設計契約のみ。実装・実容量・性能の受理ではない）
- 日付: 2026-10-13
- Issue: #468
- 規則: ARC-001 / ARC-003 / ARC-004 / ARC-005 / ARC-007 / ARC-008 / ARC-009 / ARC-010 / ARC-012、CPP-003 / CPP-005 / CPP-013 / CPP-016、QLT-001 / QLT-010 / QLT-012 / QLT-013 / QLT-014

## 対象と判断の位置

#450の[本文所有案](../design/2026-10-12-large-file-source-ownership.md)を、製品へ接続する前の契約へ進める。同期の資源寿命は#465/ADR0125、実行中の取消は#467/ADR0126の別実験で扱う。本ADRはその実験コードの採用や、2GiB対応の宣言ではない。製品の64MiB上限、settings.v3、session.v2、同期FilePortは変更しない。未使用portと第二のTextBuffer/Vim/保存経路を先に足さない。

実装に先行して、(1)予約と回収、(2)必要範囲を得てからの一回確定、(3)実際に保存した版の識別を固定する。以降の名前は責務を説明する語で、空の公開型を作る指示ではない。本文取得の全callerと移行単位は[移行表](../design/2026-10-13-large-file-integration-map.md)を正本とする。

## 1. 予算の所有と予約

applicationの一つの受付台帳が、文書/操作の予算を割り当てる。workerへは予約済みの不変な要求と上限を渡し、workerが共有の可変EditorStateを参照して追加取得しない。adapterはその要求に与えた上限を守り、I/Oの寿命と実資源解放を報告する。表示、検索、保存ごとに独立した無制限cacheや待機列を作らない。

| 資源 | 課金する期間・量 | 返却の条件 |
| --- | --- | --- |
| 要求slot | queuedから実行/完了棚を経て、collectまたは明示discardされるまで。取消drain中も含む | 終端を回収し、その要求の完了処理を一度消費 |
| I/O領域 | buffer/event/要求metadata、発行中と取消呼出し中のpinが持つ容量 | OS終端だけでなく最後のpinが解放されたというadapterの確認 |
| 取得結果 | read bufferと所有結果が同時に存在する最大量を発行前に予約。未collect・採用後cache・表示/準備中の利用を同じ勘定へ移す | 使用中の借用がなく、最後の所有を実際に破棄 |
| 本文と履歴 | 索引の実capacity、piece/chunk列、add、removed/inserted、undo/redo、凍結版が残す旧資源 | 最後の版/履歴/保存から参照されなくなった割当てだけ |
| 表示と操作 | raw行、表示文字列/位置表、UTF-16、layout/glyph、検索結果、register、clipboard変換、準備済み編集と一時コピー | 対応するframe/cache/取引の解放。新旧同時保持も課金 |
| ディスク | 作成中/封印済み退避、保存temp、結果不明で保全する資源をvolume別に数える | 自分の所有物の削除/配置を確認。失敗を未使用容量と扱わない |

同じ割当てを共有する複数の版はpayloadを一度だけ数え、pointer列や管理物は別に数える。防御コピーは同じbytesでも別割当てであり、コピー元/先が同居する最大量を予約する。reservationの移管は元を消費して先へ渡し、copyして二重に返却できる値にしない。容量加算は`requested <= limit - used`で検査し、overflow・0・上限一致を含める。

受付時にはslotと必要な最大作業領域を一組で予約し、片方だけを持ったままもう片方を無期限に待たない。待機中の操作が追加範囲を要求した場合も、pinした領域を保持したままcache evictionを待つ循環を作らない。不要な範囲を手放すか、既存入力から再開可能な小さいcontinuationへ縮め、それでも入らなければ資源不足として確定前に失敗させる。未保存本文やundoを黙って捨てて空けない。

coreの処理へ渡す取得済みrangeはapplicationが所有し、純粋な一段の呼出し中だけ不変viewを貸す。continuationは位置/要求identity/小さい走査状態を所有し、借用viewを次回へ持ち越さない。cacheはpinされていない値だけを追い出す。結果を履歴やregisterへコピーする場合は、その新しい所有も予約する。core値のコピー/破棄にファイルcloseや隠れたI/O callbackを持たせない。

[試作計画](../plans/2026-10-12-release-scope-and-large-file-probe.md)のprivate bytes増分512MiBは実測の受理条件であり、論理台帳の合計だけで達成とはしない。2GiBの改行索引payload 288MiBとの差224MiBには、上記の重複・capacity・allocator・UI/OS libraryの費用も入る。上限値は設定された有限の値で試験するが、製品の最大文書数/行長/編集量をこのADRで狭めない。具体的な配分は実容量の計測前にplanへ固定し、不足が判明した場合は表現を見直すか、根拠とともにhideへ製品範囲の判断を示す。

## 2. 退避と保存tempの回収を分ける

製品候補の退避先は、合成時に確定したprofileの専用作業領域`work-v1`とする。既定profileは現在のLocalSettingsPathの正典を使い、独立した環境変数解釈を増やさない。試作/検証ではprofileと全出力をD:/NeNeNib配下へ明示注入する。このADRだけで既存profile/save/本体を移動しない。

元本文退避は専用領域内でCREATE_NEW、共有0、非継承、DELETE_ON_CLOSEを使い、同じhandleを封印後も保持する（ADR0125）。名前の一致を所有の証明にせず、元pathや利用者ファイルをcleanup対象へ登録しない。作業領域ごとの所有markerと排他的な生存leaseを、payloadを作る前に確立する。markerに任意の削除pathを書いて実行する方式は採らない。

通常終了は全要求を終端まで回収し、最後のhandleを閉じる。異常終了後の掃除は、専用領域内の正しい版のmarkerと所有roleを確認し、生存leaseを排他的に取得できた既知の退避だけに限定する。PID/時刻だけで生存を推定しない。pathの正規化と領域内確認、reparse point/予期しない子要素の拒否、実際の対象identityの照合を行い、リンクを辿る再帰削除をしない。不明/壊れたmarker、lease取得失敗、unknown fileは保持して所在を報告する。電源断でmarkerだけ壊れた場合の自動削除は保証しない。

保存tempは保存先と同じvolumeに置く既存の意味を守る。私有退避の回収器へ混ぜず、一意のCREATE_NEWと要求ごとの所有記録で他者の既存tempを上書きしない。配置前の中止なら自分のtempだけを削除する。配置失敗でdisk上の状態が不明なら、候補本文を含む資源を保全して結果を報告し、同じ要求を自動再試行したりtempを一律削除したりしない。再起動時も保存候補を退避cacheとして掃除しない。

[GetDiskFreeSpaceExW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getdiskfreespaceexw)から得る利用可能量は事前情報であり、他の書込を排除する予約ではない。設計上のvolume別予算と区別し、全write/Flush/配置の失敗を引き続き扱う。退避、保存temp、既存保存先の同時存在を数える。[CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)の共有条件とdelete-on-closeを使っても、媒体障害や電源断まで成功回収とは保証しない。

## 3. 純粋な一段の処理と一回確定

本文の意味は一つのTextBuffer/piece modelに残す。byteを必要とする関数は、取得済みの不変入力から`ready / need ranges / failure`の閉じた結果を返す。`need`はI/Oを実行せず、applicationが予算を予約して正典portへ渡す。空文字/NUL/現在のcache末尾を、欠けた本文やEOFの代用にしない。索引だけで答えられる行位置と、CR判定や文字数走査を伴う位置変換を区別する。

各操作は文書incarnation、要求identity、元本文identity、本文revision、操作に必要な選択/モードの開始状態を固定する。窓共通のVim/register/録画/repeatにも編集世代を持たせ、候補が依存する世代を保存する。完了を受けたときに同じタブ位置/pathで照合せず、これらが一致する要求だけを再開する。別タブ中はbytesの回収/cacheまでにとどめ、本文/Vimのcommitは対象文書がactiveで依存状態も一致するときだけ行う。parked文書へ直接編集する第二経路を作らない。

別タブで共通register等が変わった場合は、元文書のrevision/selection/modeが同じでも未commit候補を無効にし、明示取消として扱う。新しい共通状態で古い意図を再解釈しない。閉鎖/再open/対象変更で無効になった完了も資源解放だけに使う。通常の描画更新や空の完了合図だけで共通編集世代を進めない。

本文・選択・履歴・保存印・Vim/register/録画/repeatの変更候補を、外部副作用のない準備段階で作る。必要な旧本文と新しい所有量、commit後に不可欠なcaret補正まで成功してから、EditorStateの一つのcommit口で候補を一度だけ採用する。履歴/本文だけを先に適用して、後からread失敗を返す方式は採らない。`EditorState::with_edit`を起点に、他の入口も同じ準備/確定へ移行する。

操作開始時のnotice消去・tab歩き確定・待機表示は、本文の成功とは別の明示した開始効果とする。失敗/取消時は本文・選択・履歴・Vimの編集効果を残さず、開始効果と失敗通知だけを残せる。確定後の描画/layout失敗は既に成功した編集を巻き戻さず、最後の成功frameと再描画要求で扱う。編集成立と表示成立を同じboolへ畳まない。

同じ文書で準備が待機している間、次の意図をその時点の選択へ無断で再解釈しない。有限の受付slot内で順番と元の意図を保持し、文書切替/close/cancel等の制御操作は先に処理できる。上限超は明示した受付失敗で知らせ、黙って鍵を捨てない。具体的な待機中UIと有限値は製品接続時に採用し、小さい試験では上限を注入する。

clipboard cutは、削除範囲・履歴・候補状態を先に準備し、clipboard writeが成功したときだけread不要のcommitを一度行う。write失敗なら本文を保持する。Vimのclipboard入力は一度だけ読んだ所有snapshotとして準備へ渡し、再開のたびにOSから読み直さない。外部clipboard出力と本文変更をOS全体で原子的にする保証は加えず、既存の出力失敗意味を対象契約へ明記する。

macro/dotの全体を失敗時rollbackへ変えない。既存どおり、完了した打鍵の効果は残し、意味上の失敗で後続を打ち切る。I/Oの`need`は失敗ではなく、まだcommitしていない現在の一段だけを継続する。完了した打鍵やclipboard副作用を最初から再実行しない。最後のundo単位への集約は、before/after全文コピーをやめ、確定した編集列から同じ正典の合成を行う。集約不能/予算超の扱いは、再生開始前の予約と小さい契約で固定する。

IMEのOS確定文字列は一度受け取って所有し、I/O待ちの間もcompositionと別の受領済み入力として保持する。OSへ未確定状態を戻せるとは扱わない。通常入力の一回確定と、Vimで文字ごとに解釈する既存意味を保ち、未commitの文字を失敗通知とともに保持して再入力/取消できる製品UIを接続時に定める。range失敗を空入力成功にしない。

## 4. 本文revisionと保存済み地点

本文revisionは履歴position・entry数・path・時刻・Editのpointerで作らない。文書incarnation内の専用発行者が、新しい本文commitへ再使用しないIDを割り当てる。発行者はundoで巻き戻す履歴やコピーした状態に含めず、同一文書の分岐を通じて単調に進め、枯渇は変更前の型付き失敗にする。文書incarnation自体も、古い結果が生存する間に同path再openで再使用しない。

履歴は初期revisionと各entryのbefore/afterを保持し、現在revisionはその履歴から一意に導く。本文と履歴の対応を確定する口は一つにし、任意bytesへ任意revisionを付ける公開ctorを作らない。Documentは保存したrevisionを持ち、現在と一致するときだけその保存先/符号化条件でsavedとする。

| 操作 | revisionの変化 |
| --- | --- |
| 新しい編集 | 新しいafter。redoを切って同じpositionに戻っても別ID |
| coalesce/absorb | 最初のbeforeを保持し、新しいafterへ置換。position不変でもIDは変わる |
| undo / redo | entryが持つbefore / afterへ戻る。新しい版を発行しない |
| history seal | 本文不変なのでIDを変えない |
| macro/dotの非空の履歴集約 | 既に確定した始点/終点revisionを保持。表現の集約だけでfresh IDを発行しない |
| 既存のnet-zero集約分岐 | 合成したnet差分が空なら、現在の`before.sealed()`と同じく集約前履歴と始点revisionへ戻す。完了済みregister/clipboard/notice等の副作用は巻き戻さない |
| 内容を変えない操作 | 現revisionを保持。上記net-zero集約以外の独立編集で同じ文字列へ戻しただけなら、自動で同じ版とはみなさない |
| 保存中にundo→別入力→同position | 保存対象Aと分岐Bは異なる。A完了でBをsavedにしない |
| 同pathを閉じて再open | 新しい文書incarnation。旧保存完了で新しい文書を変更しない |

保存開始で編集単位をsealし、対象revision、piece列、元本文の保持、addの不変snapshot、保存先/encoding/mode/SaveIdentityを確定する。TextBufferの浅いcopyだけを凍結とは呼ばない。保存が読むAddChunkのbytesと索引は以後一切変更せず、新しい入力は新しいchunkへ書く。変更しないchunk/元索引は共有してよい。保存へ借用string_view列だけを渡さず、供給中は同じ不変版と予約を所有する。

初回の接続では保存要求を一つの正典queueで直列化し、同じ文書/保存先の配置が追い越さない。保存先や保存方針の変更は世代を持ち、古い完了が最新のdocument path/encodingを上書きしない。同じ保存先/方針への次のSaveだけではその世代を進めない。完了には配置順を結び付け、古い配置の遅い回収で新しい保存地点を巻き戻さない。`retain_document`の保存は既存どおり現在文書名/保存印を変えない。受付済みの古い要求も、実際のdisk配置結果は回収して報告する。待機列を無制限にせず、二重Saveの受付/統合は同じbudgetと要求契約へ置く。

同じ文書incarnation/保存先世代で`retain_document`以外の配置が成功したら、現在revisionが異なっても実際に保存したAをsaved revisionとして記録する。現在Bの表示はmodifiedであり、undoでAへ戻ればsavedになる。保存先世代が変わった古い成功は、新しいpath/encoding/saved地点を変更しない。

保存snapshotを確定する受付境界で、既存の`close_replayed_unit/reopen_replayed_unit`の正典を再利用して再生区間を分ける。配置完了まで分割を遅らせず、保存対象revisionを跨ぐ集約をしない。現同期saveはこの2関数を呼んでおらず、保存入口への結線は製品移行時に追加する。

保存供給は同じFilePortのread/全write/Flush/配置の意味へ一括移行する。UTF-8 BOMは要求の先頭へ一度だけ付ける。Shift_JISの小さい既存入力を落とさず、大容量Shift_JISの採用は先取りしない。全量取得と符号化の成功前に配置を始めない。

| 取消/結果の時点 | applicationとdiskの扱い |
| --- | --- |
| 配置開始前に取消が勝つ | 配置を発行せず自分のtempだけ回収。未保存本文を保持 |
| 配置を始める境界 | 同じadapter制御で取消latch確認とplacingへの変更を不可分に行う |
| placing後に取消が届く | 取消を理由に配置済みを未実行へ変えない。配置APIの実結果を回収 |
| 配置成功 | 実際に保存したrevisionを結果へ含め、同世代のsaved地点へ記録。現在版との一致で表示を判断し、版の違いだけで成功記録を捨てない |
| read/write/Flush失敗 | 配置へ進まない。本文と未保存印を保持し、自分の未配置tempを回収 |
| 配置失敗・結果不明 | diskが不変とは宣言せず、errorと保全資源の所在を報告。自動再試行/一律削除なし |

[ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)には失敗codeにより元/置換fileの残り方が異なる契約がある。事前のfile identity比較と配置の間の外部変更も、このAPIだけで排除したとは宣言しない。元本文の私有snapshotと保存先の外部変更保護を混同しない。

## 強制・検証・採用の境界

本ADRは設計文書であり、製品の機械強制は追加しない。callerと現行副作用順を読み取り照合し、独立設計レビューで指摘されたnet-zero集約、窓共通状態の世代、本文/保存の同時移行、保存snapshot受付時の再生区間分割を反映した。文書差分を確認し、文書だけのために製品試験/CI/性能/GUIを実行しない。小さい必須契約と移行順は移行表へ固定し、実装した単位だけ対象試験を行う。関連入力不変の成功結果を工程変更だけで繰り返さない。

2GiB/512MiB/100ms、最大行長/編集量、同時文書数、実driverの取消性、全保存先での外部競合保護は未受理。公開APIと全callerを同じ製品変更で移行できなければ、先に期限と撤去条件を持つwaiverを審査し、黙って二経路を残さない。現在のwaiverはnone。
