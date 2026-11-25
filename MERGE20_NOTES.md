# Merge20 フォーク — 作業メモ（Phase 0〜1）

「マージ支援20号」（HTMLツール v17.15）の考え方を WinMerge に移植するフォークの
土台作りの記録。仕様書は `~/workspace/goals/muse-merge-rebuild-competing-with-gemini-neon/files/winmerge-fork-spec/winmerge-fork-spec.pdf`
（18ページ、v1.0、ベース WinMerge 2.16.60、GPLv2）。
動作見本の HTML は `~/workspace/user/files/17_v17.15.html`（元の HTML は変更していない）。

## 1. クローン結果

- 場所: `~/workspace/winmerge-fork/`
- 上流: `https://github.com/WinMerge/winmerge.git`
- 取得方法: `git clone --depth 1 --branch v2.16.60`（shallow clone）
- タグ: `v2.16.60`、HEAD コミット `195fbda`（"Update GNU patch to 2.7.6-3"）
- 上流 HEAD（取得時）: `de2d48f7e91420191a452126823f8a3e75d00e83`（v2.16.60 より先に進んでいる）
- ブランチ: `merge20/phase1`（v2.16.60 から分岐、ローカルのみ）
- サブモジュール: **未初期化**（12件: frhed, winimerge, patch, tidy-html5, jq, wil,
  winwebdiff, md4c, darkmodelib, cliphcat, tree-sitter-grammars, 7zip）。
  Linux 側の調査・スクリプト検証には不要と判断。Windows でのビルド時は
  `DownloadDeps.cmd` 実行または `git submodule update --init --recursive` が必要。

## 2. ツールチェーン判定 — この Linux VM ではビルドできない

| 項目 | 結果 |
|---|---|
| OS | Ubuntu Linux（この VM） |
| git | 2.43.0 あり |
| gcc / g++ | 13.3.0 あり |
| clang | なし |
| wine | なし |
| mingw（x86_64-w64-mingw32-gcc） | なし |
| dotnet | なし |
| gh CLI | 2.102.0 あり、**未ログイン**（`gh auth status`: "You are not logged into any GitHub hosts"） |
| 空きディスク | 約 149 GB、メモリ約 8 GB |

**結論: ローカルビルドは不可能。** WinMerge は C++/MFC 製の Windows 専用アプリで、
MFC は Linux に存在しない。クロスコンパイラもない。したがって C++ の変更は
コンパイル検証できていない（下記「変更一覧」の検証状態を参照）。

### クラウド（フォークの CI）でビルドできる見込み — 確認済み

`.github/workflows/main.yml` を読んで確認した（実行はしていない）:

- トリガー: `master` への push と `master` への PR
- 実行環境: `runs-on: windows-latest`
- チェックアウト: `submodules: 'recursive'`
- ビルド: `DownloadDeps.cmd` → `BuildAll.cmd x64 -ci`
- 成果物: `Build/Releases/*.zip` と `*.exe` が Actions の Artifacts にアップロードされる

`appveyor.yml` も同様（Visual Studio 2022 イメージ、同じビルドコマンド）。
つまり **フォークを GitHub に push すれば（master へ、または master への PR として）、
Windows バイナリがクラウドで自動ビルドされる見込み**。ただしこの VM からは
push していない（gh 未ログイン、アカウント作成もしていない）。

## 3. 仕様 → ソースのマッピング

| 仕様書の機能 | WinMerge 側の実装場所（実読で確認） | 対応方針 |
|---|---|---|
| 3-way 比較・マージ結果ペイン | `Src/MergeDoc.cpp/.h`（`m_ptBuf[3]`、`m_nBuffers`）、`MergeDocResultPane.cpp`、`MergeResultView.cpp/.h`（`MergeResultTextBuffer`） | 流用（既に存在）。差分ごとの出所選択 1/2/3、`/al` `/am` `/ar` 自動マージも既存 |
| フォルダ比較・一覧 | `Src/DirDoc.cpp`、`DirView.cpp`、`DirScan.cpp`、`DirTravel.cpp`、`DirFrame.cpp`、`DirItem.cpp`、`DirViewColItems.cpp`。列は `DirColInfo` 構造体（`regName` / `idName` / `getfnc`） | 流用。対応区分・リスク・確認内容・除外行数の新列は Phase 2 でここに追加 |
| 差分エンジン | `Src/CompareEngines/Wrap_DiffUtils.*`、`Src/diffutils/`（diffutils ベース） | 流用。HTML の LCS は移植しない |
| 除外ワード（% ワイルドカード）・2行目バージョン無視 | プラグイン `Plugins/dlls/*.sct`（`BUFFER_PREDIFF`）、`Src/Plugins.cpp` | **Phase 1 で新規 prediffer スクリプトレットとして実装済み**（下記） |
| ファイルフィルタ（隠しファイル除外） | `Filters/*.flt`（`name:` / `desc:` / `def:` / `f:` / `d:` / `f!:` / `d!:`）、パーサ `Src/FileFilterMgr.cpp`、判定 `Src/FileFilter.cpp` `TestFileNameAgainstFilter` | **Phase 1 で `Merge20.flt` を新規追加済み** |
| 比較オプション既定値（空白・大文字小文字を無視） | `Src/OptionsDiffOptions.cpp` `Init()`（`OPT_CMP_IGNORE_WHITESPACE`、`OPT_CMP_IGNORE_CASE`）、`Src/CompareOptions.h`（`WhitespaceIgnoreChoices`） | **Phase 1 で既定値を変更済み（未コンパイル）** |
| 設定の永続化 | `Src/Common/OptionsMgr.*`、`RegOptionsMgr.*`（レジストリ）、`IniOptionsMgr.*`（ポータブル版は INI）。プラグイン設定は `mergeApp.GetOption/SaveOption`（`Plugins\<名前>/` 下） | 流用。prediffer はこの仕組みを利用 |
| 文字コード（Shift_JIS / EUC-JP / UTF-8） | `Src/Common/unicoder.cpp`、`UniFile.cpp`、`multiformatText.cpp` | 流用。EUC-JP の往復は Windows 側で要テスト（仕様 4(d)） |
| 学習機能（JSON v2、出典別管理） | 対応する既存機能なし | Phase 2 以降の C++ 実装。仕様 4(e) 参照 |
| 確認内容 5 区分（未確認／済／保留／個別対応要／現状維持）・出力チェック連動 | 対応する既存機能なし（フォルダ比較のチェックは別物） | Phase 2 以降。`DirColInfo` への列追加と `MergeDoc` 側の状態保持が必要 |
| CI / ビルド | `.github/workflows/main.yml`、`appveyor.yml`、`BuildAll*.cmd`、`DownloadDeps.cmd`、`ALL.sln` / `WinMerge.sln` | 流用（上記 2. 参照） |
| インストーラ | InnoSetup `Installer/InnoSetup/*.iss`（`[Files]` は `.sct` を個別列挙、`.flt` はワイルドカード）、WiX `Installer/WiX/Fragments/Filters.wxs`・`Features.wxs`（`.flt` を個別列挙、`.sct` は WiX では出荷しない） | **Phase 1 で新規ファイル分を追加済み（未コンパイル）** |
| 日本語 UI | `Translations/WinMerge/Japanese.po` | 新規文字列の翻訳は Windows 側のフォローアップ（下記「決定事項」参照） |

## 4. 実施した変更の一覧（ブランチ `merge20/phase1`）

### 検証済み（Node / Python でテスト済み）

| ファイル | 内容 | 検証 |
|---|---|---|
| `Plugins/dlls/Merge20Exclude.sct`（新規、17,049 バイト） | JScript `BUFFER_PREDIFF` スクリプトレット。`%` ワイルドカード付き除外ワード（HTML の `wildcardRe` と同じアルゴリズム: `%` で分割→各部分をエスケープ→`.*` で結合、部分一致）と 2行目バージョン無視（HTML の `isL2Ver` 相当）。設定は `mergeApp.GetOption/SaveOption`（`Plugins\Merge20Exclude.sct/` 下: `Count`、`Enabled<i>`、`IgnoreCase<i>`、`UseRegExp<i>`、`Pattern<i>`、`IgnoreL2`、既定 ON）。HTA 設定ダイアログ付き（Enabled / Ignore Case / Use RegExp / Pattern の一覧、追加・削除、2行目バージョン無視チェックボックス）。`PluginIsAutomatic=true`、`PluginFileFilters="."`（全ファイル対象）。 | **検証済み**。`node --check`（本体・HTA 両方）合格。機能テスト 27/27 合格（バージョン行の無視・非バージョン行の保持・`--1.0.8%` の一致／不一致・複数 `%`・正規表現・無効化ルール・大文字小文字・CRLF/LF 保持・空入力・不正正規表現の安全処理・changed フラグ）。構造チェック合格（タグの対応、必須メソッドの存在、ASCII のみ）。 |
| `Filters/Merge20.flt`（新規） | `def: include` のフィルタ。隠しフォルダ（`d: \\\.[^\\]*$`）と隠しファイル（`f: \\\.[^\\]*$`）を除外。HTML ツールの「ドットファイル／ドットフォルダは読み込まない」規則に対応。 | **検証済み**。Python `re` で 10/10 合格（`.git` / `.svn` / `.vs` / `.gitignore` を除外、`src` / `code.java` / `file.txt` / `.svn` 配下を保持）。 |

### 未コンパイル（変更は小さいが、この VM では C++ をビルドできないため要 Windows 検証）

| ファイル | 内容 | 検証 |
|---|---|---|
| `Src/OptionsDiffOptions.cpp`（2行） | `Init()` の既定値を変更。`OPT_CMP_IGNORE_WHITESPACE`: `(int)0`（`WHITESPACE_COMPARE_ALL`）→ `(int)WHITESPACE_IGNORE_CHANGE`。`OPT_CMP_IGNORE_CASE`: `false` → `true`。短い英語コメント付き（仕様 3.1 を引用）。 | **未コンパイル**。変更は 2 行のみで、定数名は `Src/CompareOptions.h` で実在を確認済み。 |
| `Installer/InnoSetup/WinMergeARM64.is6.iss`、`WinMergeX64.is6.iss`、`WinMergeX64.iss`、`WinMergeX64NonAdmin.iss`、`WinMergeX86.iss`（各 2 行） | `[Files]` に `Merge20Exclude.sct` の行を `PrediffLineFilter.sct` 行の直後に追加（同じ Flags）。`[UninstallDelete]` に `Merge20.flt` の行を `SourceControl.flt` 行の直後に追加。 | **未コンパイル**。各ファイルで置換が exactly 1 回であることをスクリプトで assert 済み。改行コードは CRLF のまま維持を確認済み。 |
| `Installer/WiX/Fragments/Filters.wxs`（7行） | `Merge20` コンポーネントを `Merge_VC_loose` と `SourceControl` の間（アルファベット順）に追加。 | **未コンパイル**。同上。 |
| `Installer/WiX/Fragments/Features.wxs`（1行） | `<ComponentRef Id="Merge20" />` を同位置に追加。 | **未コンパイル**。同上。 |

なお `WinMergeX86.iss` など InnoSetup の `[Files]` には `.flt` のワイルドカード
（`Source: ..\..\Filters\*.flt`）が既にあるため、新規 `.flt` のインストール自体は
自動で含まれる。追加した `[UninstallDelete]` 行はアンインストール時の掃除用。
`.sct` は WiX では出荷されていないため、WiX 側の変更は `.flt` 分のみ。

## 5. 検証結果のまとめ

- `Merge20Exclude.sct`: `node --check`（本体 JScript・HTA JavaScript）合格。
  機能テスト 27/27 合格。テストは `/tmp/test_merge20.js`（一時ファイルのため
  残っていない場合がある。再現する場合は本メモの検証項目から再作成すること）。
- `Merge20.flt`: Python `re` による 10/10 合格。
- C++ / インストーラの変更: **未コンパイル**。Windows または CI でのビルドが必須。
- 実機（Windows）での動作確認は一切できていない。特に次の点は未確認:
  `PrediffBufferW` が WinMerge 本体から呼ばれること、設定ダイアログ（HTA）が
  表示されること、`.flt` がフォルダ比較で実際に隠しファイルを除外すること、
  既定値の変更が初回起動時に反映されること。

## 6. ここでできなかったことと、必要なもの・次の手順

### できなかったこと

- **C++ のビルド・テスト**: MFC が Linux に存在しないため不可（上記 2. 参照）。
- **Phase 2 以降の機能**（仕様書 第9章）: 確認内容 5 区分と出力チェック連動、
  対応区分・リスク列の追加、ディレクトリ単位の一括チェック、表示件数中の
  チェック件数表示、選択ファイル ZIP（OLD / NEW / MERGED の階層保持）、
  学習機能（JSON v2・出典別管理・再取込時の置換・統合出力）、
  ⊘N（除外行数）表示、マージ後検証（再差分）。いずれも C++ 側の実装が必要で、
  未コンパイルのまま大規模な書き換えを進めるのは責任を持てないため着手していない。
- **日本語 UI 文字列の翻訳**: 新規文字列は英語のまま（下記「決定事項」参照）。
  `Japanese.po` への追加は Windows 側での作業。

### 必要なもの

1. **Windows マシン + Visual Studio 2022**（C++ デスクトップ開発、MFC/ATL、
   Windows SDK）— ローカルビルド用。手順は下記。
2. **または GitHub アカウント + フォーク** — CI ビルド用。手順は下記。

### 次の手順 A: Windows マシンでビルドする

1. このフォーク（ブランチ `merge20/phase1`）を Windows マシンに持っていく
   （USB / 社内共有など。GitHub を使わない場合は `git clone` の代わりに
   フォルダごとコピーでもよいが、`.git` も一緒に持っていくと履歴が残る）。
2. `git submodule update --init --recursive` を実行する
   （または `DownloadDeps.cmd` を実行する。CI も同じことをしている）。
3. `BuildAll.vs2022.cmd` を実行する（または `BuildAll.cmd x64 -ci`）。
   成果物: `Build\X64\Release\WinMergeU.exe`。
4. インストーラを作る場合は `Installer/InnoSetup/` の `.iss` を
   Inno Setup 6 で開いてビルドする（WiX 版は `Installer/WiX/`）。
5. 動作確認: `Merge20Exclude.sct` がプラグイン一覧に出ること、
   除外ワード（`--1.0.8%` など）と 2行目バージョン無視が働くこと、
   `Merge20.flt` で隠しファイルが除外されること、初回起動時の既定値が
   空白無視・大文字小文字無視 ON であること。

### 次の手順 B: GitHub フォークの CI でビルドする

1. GitHub アカウントを用意する（この VM では作成していない）。
2. `WinMerge/winmerge` をフォークする。
3. このブランチ `merge20/phase1` をフォークの `master` に push する
   （または `master` へのプルリクエストを出す）。
   CI（`.github/workflows/main.yml`）が `windows-latest` で自動実行され、
   `Build/Releases/*.zip` と `*.exe` が Actions の Artifacts に上がる。
4. 成果物をダウンロードして、手順 A の 5. と同じ動作確認をする。

## 7. 決定事項・注意点

- **UI 文字列は ASCII（英語）のみ**。既存の `.sct` ファイルは ASCII で、
  スクリプトホストでの文字化けを避けるため新規ファイルも ASCII にした。
  日本語訳は `Translations/WinMerge/Japanese.po` への追加として別途行う
  （標準の `${...}` トークンは既存の翻訳が適用される）。
- **空白無視は `WHITESPACE_IGNORE_CHANGE`（1）を採用**。`WHITESPACE_IGNORE_ALL`（2）
  ではない。理由: HTML ツールの空白処理は「空白の連続を 1 つに潰す＋前後トリム」で、
  `"abc def"` と `"abc    def"` は等しいが `"abcdef"` とは等しくない。
  これは `CompareOptions.h` のコメントにある `IGNORE_CHANGE` の定義と一致する。
- **prediffer の既定ルールは 0 件**。HTML のプリセット（`//` コメント、
  Last Modified / Updated Date ヘッダ、`@version` / `@author`）は
  推奨スタータールールとして `.sct` のヘッダコメントに記載したが、
  既定で入れることはしなかった（利用者が自分の除外ワードを入れる設計）。
- **prediffer の限界**: prediffer は行の追加・削除・移動ができないため、
  除外した行は「空行」に置き換える。片側だけに存在する除外行は、
  空行の差分として表示される可能性がある。HTML ツールの ⊘N（除外行数）の
  完全な再現には C++ 側の実装（Phase 2）が必要。この限界は `.sct` の
  ヘッダコメントにも明記してある。
- **学習機能の JSON v2 仕様は未実装**。出典別管理・再取込時の置換・
  統合出力は仕様書 4(e) の C++ 実装待ち。
- **外部への送信・公開はしていない**。GitHub への push、アカウント作成は
  行っていない。クローンは読み取りのみの `git clone`。
- **元の HTML（`17_v17.15.html`）と仕様書 PDF は変更していない**。
- ライセンス: WinMerge は GPLv2。フォークも GPLv2 の条件に従うこと
  （仕様書 第8章参照）。


## GitHub fork and CI

- Fork: https://github.com/askhim659/winmerge (public).
- CI workflow `.github/workflows/main.yml` (job `build`, windows-latest)
  runs on push / pull request to `master` and uploads
  `Build/Releases/*.zip` and `*.exe` as artifacts.
- The fork's workflows were registered only after the first push to
  `master`; API ref updates do not produce the push event Actions
  listens to, so CI is triggered with pull requests.
- Phase 1 was pushed through the Git Database API (the stored token
  cannot be used for git-over-HTTPS password auth from this Linux VM).
