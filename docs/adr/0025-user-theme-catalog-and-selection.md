# ADR 0025 — 利用者テーマの不変カタログと共通選択

- 状態: 受理
- 日付: 2026-09-20
- Issue: #70（C4b）
- 規則: ARC-001 / ARC-003 / ARC-004 / ARC-007 / ARC-008 / ARC-009 / ARC-011 / CPP-002 / CPP-004 / CPP-007 / CPP-011 / CPP-012 / CPP-013 / QLT-001 / QLT-008 / QLT-012 / QLT-013

## 文脈

C4aは利用者ファイルのcodecと所有まで。選択/保存/復元に必要なカタログを追加する。組み込み専用の参照から、存在が確認できた配色を所有する参照へ進める。壊れたテーマの診断は名前を失わず、無効なsettingsの保存禁止（ADR 0020）を維持する。

## 決定

1. `ThemeChoice` はBuiltinThemeか不変ThemeDocumentの共有参照を包む。生成は同じ `from` factory、nullの利用者参照は生成できない。`view` / `name` はlvalueの選択参照だけから借りる。catalogのrecordsとselected_themeもrvalueの所有者を拒否する。EditorSettingsの任意themeはこの型に置換し、無しは引き続きsystem。等値は正規名。同一プロセス中にテーマ内容は更新しない。
2. `ThemeRecord` は正規名とThemeChoiceまたはThemeFailure。coreの `ThemeCatalog` が利用者recordを名前順に不変共有し、組み込みは既存表を引く。重複/予約名/名前不一致はfactoryで拒否する。組み込みだけの空catalogに割当は要らない。EditorStateがカタログの唯一の状態所有者となり、copyは不変データを共有する。
3. テーマの失敗はC4aのenumをcoreへ移す。OS操作は移さず、adaptersがOSの失敗をdomainの理由へ写す。`ThemeLookupFailure` はThemeNameとThemeFailureを持つ。catalogの検索はこれを返す。Exの評価失敗はExFailureとの閉じた和型、設定失敗はSettingsFailureとの閉じた和型にし、同じ名前付き説明へ写す。
4. `ThemePort::read` はcatalogと任意の起動時noticeを返す。controllerは設定より先に一度読む。SettingsPort/codecはそのcatalogを受け、テーマ名を解決してThemeChoiceを復元する。codec・adapter・UIがテーマ検索を複製しない。読込失敗時はnamed failureを保持して後のwriteも拒否する。settings.v1の4キー、既存組み込み名/system、競合保護は変えない。
5. Win32ThemeAdapterは既存LOCALAPPDATA解決を使う `themes` フォルダを、再帰せず起動時に一度だけ列挙する。対象は `.v1.theme`、最大128件、各16KiB（C4a）。129件目を見つけたら部分集合を恣意的に選ばず、組み込みのみと上限のnoticeにする。フォルダ無しは正常。名前はC4aの正規basename関数だけで検証し、内容も `load_theme` だけで読む。
6. 正規名だが壊れたファイルは失敗recordとして候補へ残し、選択時に名前+理由を示す。非正規名の最初の診断は起動時statusへ載せ、他の正常なrecordの利用を妨げない。フォルダ列挙自体が初回または途中で失敗した場合は、上限と集合の完全性を確認できないため利用者テーマを全廃し、組み込みとnoticeだけにする。列挙順に依存する部分集合は採用しない。初期ファイルはcontroller構築の任意OpenDocumentとして通常のapply経路で開き、その後に診断を載せて最初の描画まで保持する。OS列挙順に依存させずファイル名順に読む。データを勝手に修復・書き換えない。
7. ExとCtrl+Pは同じcatalog由来の候補を使う。CommandLineは開始時の不変catalogを借りず値で共有し、編集/補完/fillでも維持する。入力が無い通常打鍵で候補生成やディレクトリ走査をしない。採用済み画面/サイズ/操作は変えない。
8. 設定が指すテーマが欠落/破損していたら、起動時に名前付きの設定エラーを知らせて既定配色で表示する。元settingsは書けない状態を保持する。設定が同値でもエラー中は保存ポートの判断を省略せず、成功と表示したりエラーを消したりしない。修正と再起動でのみ再読込する。新しいファイルや編集したテーマの反映も再起動で行う。

## 検証

共有参照とcatalogの値/失敗、Ex/Tab/パレットの同一候補、保存前検証、設定のnamed failureとwrite block、Win32列挙の上限/欠落/不正名を対象にする。nativeは隔離profileでテーマ選択・本文保持・配色/明暗・再起動・壊れたテーマを確認。起動時読込/共通状態変更の負荷は起動/打鍵4指標だけ。C4aの不変codec、無関係なVimとGUI/ファイル保存の成功は再利用する。

## 帰結

選択参照は必ず生存するThemeを持ち、未解決の名前だけを描画時に引く経路は無い。coreはファイルや時刻を読まない。UIは表示値だけを写す。ホットリロード・テーマ編集UI・新しいruntime依存・waiverは追加しない。
