# 測定窓の所有選択 — Issue #448

#446の性能確認で、通常版PID21624/window464492が先に見つかり、測定用PID48272/window1709038が可視・応答中でも`start`がtimeoutした。初回の4ベンチは全て`window unavailable; recorded only`で、終了0でも速度判定は成立していない。初回ログは`D:/NeNeNib/evidence/newline-index-446/`に保存する。

既存`owned_dialog`のFindWindowExWによる全候補のPID/可視性照合を`owned_window`へ集約し、編集窓の`start`とdialogを接続した。通常版を終了したり、他PIDへ入力やcloseを送る必要がなくなる。timeoutと起動済み子プロセスの後片付け、刺激、採時、性能基準は維持する。規則はARC-001/012、QLT-001/010/012/013/014、GIT-001〜004。保存schema変更なし、Waivers: none。

確認する退行は他PID/不可視の誤選択、対象不在時の不正handle参照、dialogとstartの経路分岐、timeout時の他プロセス終了。`python -m unittest discover -s tests/conformance -p test_window_owner.py -v`で6 tests成功。Win32境界をmockし、実窓・実時刻・製品buildを使わない。`git diff --check`も成功。初回検証であり、同じソース/テストのpush・review・mergeでは結果を再利用する。無関係なテストや全件回帰は実行しない。

実窓での接続は、#446で固定した4項目各5回の未成立測定を、修正した同じdriverで行う。旧試行を削除せず、driver SHAと修正の理由を測定前に記録する。並行作業の負荷と画面の遮蔽はこの修正で保証しない。
