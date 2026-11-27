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


## 8. Phase 2 S0（Phase 1 の残り）— ブランチ `merge20/phase2-s0`

設計書 `merge20-phase2-design.md` §4（§7 の S0）に従う。C++ の変更は
この Linux VM ではコンパイルできないため、CI でのビルド＋成果物確認が
検証になる（各コミットは小さく分けている）。

### 8.1 存在区分の表示名対応（設計 §4.1）

- `Src/DirViewColItems.cpp` の `ColStatusGet`／`ColStatusAbbrGet` を変更。
  2-way 比較では「比較結果」列と「Short Result」列が HTML の 4 区分名で
  表示される：`変更`／`旧のみ`／`新のみ`／`同一`（英語は `Changed`／
  `Old only`／`New only`／`Identical`）。3-way 比較は従来の表示
  （`Left only: <パス>` 等）のまま。
- フィルタ（`OPT_SHOW_*`）とソートは `DIFFCODE` を見ており、表示文字列
  には依存しないため、フィルタ動作は変わらない。
- `Old only`／`New only` は設計書の英語仮文言 `Left only`／`Right only`
  ではなく `Old`／`New` にした。理由：(1) 日本語表示が HTML と同じ
  `旧のみ`／`新のみ` になる、(2) 既存の msgid `Left Only`（訳：左側のみ）
  との混同を避ける、(3) フォークの利用者は OLD／NEW フォルダで考えている。
- 日本語訳は `Translations/WinMerge/Japanese.po` に追加。実行時の翻訳
  参照は msgid による `.po` 参照のため、`Merge.rc`／`resource.h` への
  IDS 追加はしていない（`.po` を `Merge.rc` から再生成する場合は、
  対応する IDS の追加が必要になる点に注意）。

### 8.2 ⊘N（除外行数）表示（設計 §4.2）— 第 2 案（暫定）を採用

- 実コードで確認したこと：prediffer API
  `InvokePrediffBuffer(BSTR &bstrBuf, int &nChanged, ...)`
  （`Src/Plugins.h:213`、`Src/Plugins.cpp:1345`）が返すのは変更フラグ
  だけで、除外した行数は返さない。第 3 案（`.sct` 側で数えて返す）は
  インターフェースにないため不可。
- 第 1 案（`PrediffingInfo::Prediffing`、`Src/FileTransform.cpp:1293` で
  prediffer 適用前後の BSTR バッファを比較し、空行化された行を数えて
  `DIFFITEM` へ運ぶ）は実現可能と判断したが、触る層が多い
  （`FileTransform` → `DiffFileData::Filepath_Transform` →
  `FullQuickCompare::CompareFiles` → `DIFFITEM` 新フィールド →
  列追加）ため、S0 の PR には詰め込まず別 PR とする。
  受け皿は用意済み：新列の regName `Sexcluded` は第 1 案でも流用する。
- S0 では第 2 案として、新列「無視差分」（英語 `Ignored diffs`、既定で
  非表示、2-way 用 `f_cols` と 3-way 用 `f_cols3` の両方に追加）を
  追加した。値は `DIFFITEM::nidiffs`（無視差分数）で、0 より大きい場合
  に `⊘N` 形式（例：`⊘3`）で表示する。**行数ではなく差分の件数**で
  あることは列の説明文に明記してある。QuickCompare の不明値は従来の
  `*`、片側のみのファイルは空欄（`ColDiffsGet` と同じ扱い）。

### 8.3 文字コード往復テスト（設計 §4.3）

- 単体テスト：`Testing/GoogleTest/Encoding/Merge20EncodingRoundtrip_test.cpp`
  を新規追加し、`UnitTests.vcxproj`（＋`.filters`）に登録した。
  CI は `BuildAll.cmd` で `UnitTests.exe` を実行するため、このテストも
  CI で実行される（T-2-3 は解消：CI 実行対象である）。
- テスト内容：(a) `codepage_detect::Guess` のバッファ版で UTF-8（BOM
  あり／なし）・ASCII・UTF-16LE BOM を判別すること、(b) 付属の
  `Testing/Data/Codepages/CP932`／`CP51932`（各 6 KB の日本語テキスト）
  を日本語自動判別（`(50932<<16)|3`、アプリ既定と同じ）で 932／51932
  と判別すること（MLang 依存。判別精度は T-2-6 として残る）、
  (c) `UniStdioFile` で 932／51932／65001（BOM なし／あり）に書き出し、
  バイト列が期待値と一致し、`UniMemFile` で読み戻したテキストが一致
  すること（EUC-JP が UTF-8 に化ける回帰を防ぐ）。
  `CDiffTextBuffer` は UnitTests にリンクされていないため、テストは
  `UniFile` レベルで行っている。テストファイルは ASCII のみ（日本語
  テキストは `\u` エスケープ、期待バイト列は明示の配列）。

### 8.4 Windows 実機での往復テスト手順（手動テスト・設計 §4.3-2）

CI の単体テストだけでは、実際の WinMergeU.exe での保存（`CDiffTextBuffer::SaveToFile`
経由、`PickResultEncoding` によるマージ結果の文字コード決定を含む）は
検証できない。次の手順で CI 成果物 ZIP の WinMergeU.exe を使って確認する。

1. テストファイルを用意する（各コードページ × CRLF／LF）：
   - Shift_JIS（932）、EUC-JP（51932）、UTF-8（BOM なし）、UTF-8（BOM あり）。
     付属の `Testing/Data/Codepages/CP932/DiffItem.h`（Shift_JIS）、
     `Testing/Data/Codepages/CP51932/DiffItem.h`（EUC-JP）がそのまま使える。
     UTF-8 のサンプルはメモ帳などで作る（BOM あり／なしの両方）。
2. WinMergeU.exe で各ファイルを開き、**無変更のまま保存**する
   （ファイル比較画面で何も変更せずに保存、またはフォルダ比較から開いて保存）。
3. 保存前後でバイト比較する（`fc /b 変更前ファイル 変更後ファイル`、
   または PowerShell の `Compare-Object (Get-Content -AsByteStream ...)`）。
   期待結果：**バイト一致**。特に EUC-JP（51932）が UTF-8 になっていないこと。
4. マージ結果の文字コードも確認する：OLD＝EUC-JP、NEW＝Shift_JIS の
   2 ファイル比較でマージ結果を保存し、結果の文字コードを確認する。
   注意：2-way のマージ結果は `PickResultEncoding`
   （`Src/MergeDocResultPane.cpp:847`）により **NEW 側の文字コード**に
   なる（OLD が EUC-JP で NEW が Shift_JIS の場合、結果は Shift_JIS）。
   これは仕様書 §4(e)「読み込んだときの文字コードで書き戻す」との関係で
   整理が必要な点であり、仕様書の改訂候補として記録する（単体ファイル
   保存では元コード、2-way マージ結果では NEW 側のコード）。

### 8.5 S0 で未実施のこと（残課題）

- ⊘N 第 1 案（真の除外行数の計数）：別 PR とする（§8.2 参照）。
- 表示名・⊘N 列・往復テストの Windows 実機での目視確認：CI 成果物での
  確認待ち（§8.4 の手順を含む）。
- U-0-2（2-way 自動マージの挙動確認）：S5 着手前に Windows 実機で確認すること。

## 9. Phase 2 S1（確認内容の状態保持＋列追加）— ブランチ `merge20/phase2-s1`

設計書 `merge20-phase2-design.md` §7 の S1 に従う（§3.2 のうち状態保持と
列追加のみ。キー操作・連動ルールは S2）。C++ の変更はこの Linux VM では
コンパイルできないため、CI でのビルド＋成果物確認が検証になる。

### 9.1 設計からの変更点（重要）：サイドカーは `CDiffContext` に置いた

- 設計書 §2.2 はサイドカーを `CDirDoc` のメンバ（`m_merge20States`）と
  していたが、実コードで確認したところ列の getter は `const CDiffContext *`
  と `DIFFITEM` しか受け取らない（`DirColInfo::getfnc`、
  `DirViewColItems.cpp` の `ColGetTextToDisplay`、`DirView.cpp` の
  `ReflectGetdispinfo`）。`CDiffContext` には `CDirDoc` への逆ポインタが
  なく、`DirColInfo` の関数シグネチャを変えると全 getter／sorter に波及
  するため、`CDirDoc` メンバでは列から状態に到達できない。
- そこでサイドカー（`Merge20FileStates`、相対パスをキーにした map）は
  `CDiffContext` のメンバ `m_merge20FileStates` に置いた。`CDirDoc` は
  `m_pCtxt` として `CDiffContext` を 1 つだけ持ち、再スキャンでも作り直さ
  ない（`DirDoc.cpp:192` で一度だけ生成、`Rescan` は再利用する）ため、
  寿命は設計書の `CDirDoc` メンバ案と同じ。`CDirDoc` には同じ内容の
  アクセサ（`GetMerge20ReviewState`／`SetMerge20ReviewState`）を
  フォワーダとして用意し、S2 のキー処理はそちらを使う。
- `DIFFITEM` には GUI 状態を入れていない（`DiffItem.h` のコメントどおり）。

### 9.2 T-2-4 の解消：`CDirView` は仮想リストである

- `DirView.cpp:115` で `LVS_OWNERDATA` が立っており、`LVN_GETDISPINFO`
  は `ReflectGetdispinfo`（同:5183）で処理され、表示文字列はその場で
  `ColGetTextToDisplay(&ctxt, i, di)` から供給される。したがって確認内容
  列は通常の `DirColInfo` 追加で表示でき、仮想リスト用の特別な描画は
  不要だった。チェックボックス列（S2 の出力列）の描画・クリック処理は、
  この仮想リスト構造を前提に S2 で設計し直すこと（`LVN_GETDISPINFO` で
  の動的供給になる）。

### 9.3 変更内容

- `Src/Merge20/Merge20Types.h`（新規）：`ReviewState`（TODO／DONE／
  HOLD／SPECIAL／KEEP）、`ActionCategory`、`RiskLevel`、
  `Merge20FileState`（設計書 §2.4 どおり。S1 で使うのは `review` のみ。
  他のフィールドは S2／S4 で埋める）。
- `Src/Merge20/Merge20FileStates.h`／`.cpp`（新規）：相対パスをキーに
  したサイドカー map。エントリのないファイルは既定状態（未確認）を返す。
  自動適用はしない（仕様書 §4(c)）。
- `Src/DiffContext.h`／`.cpp`：メンバ `m_merge20FileStates` と
  `GetMerge20ReviewState`／`SetMerge20ReviewState`、
  `GetMerge20FileStates()` を追加。
- `Src/DirDoc.h`／`.cpp`：`GetMerge20ReviewState(const DIFFITEM&)`／
  `SetMerge20ReviewState(const DIFFITEM&, ReviewState)` のフォワーダを
  追加（S2 用。比較コンテキストがまだない場合は未確認を返す／何もしない）。
- `Src/DirViewColItems.cpp`：新列「確認内容」（regName `Sreview`、英語
  `Review State`）。表示は短縮形（未確認／✓済／保留／個別対応要／
  現状維持。✓は S0 の ⊘ と同じく `\x2713` エスケープ）。ソートは enum
  順（TODO＜DONE＜HOLD＜SPECIAL＜KEEP）。`f_cols`（既定表示順 6）と
  `f_cols3`（既定表示順 7、Extension が 6 のため）の両方に追加し、表の
  末尾（`Rext` の後、デバッグ列の前）に置いた。末尾に置いた理由：
  `CDirDoc::CompareFilesIfFilesAreLarge`（`DirDoc.cpp:1098-1117`）が
  論理列番号 2・3+i・8+i・10+i・13+i をハードコードで参照しており、
  それより前に列を挿入すると大ファイル確認ダイアログの表示がずれるため。
  既定で表示する（設計書 R-7 の「既定で表示するかどうかは S1 の受け入れ
  テストで決める」を受け、受け入れ基準「一覧に確認内容列が出る」を満たす
  ため。不要なら既定順を -1 に戻すだけ）。
- `Translations/WinMerge/Japanese.po`：`Review State`→`確認内容`、
  `Not reviewed`→`未確認`、`Done`→`済`、`On hold`→`保留`、
  `Needs manual work`→`個別対応要`、`Keep as is`→`現状維持`、列説明文
  の訳を追加（S0 と同じ msgid 参照の方針）。
- `Src/Merge.vcxproj`／`Src/Merge.vcxproj.filters`：新規 3 ファイルを
  登録（フィルタ `Merge20` を新設。`.cpp` は `DiffContext.cpp` と同じ
  PCH 設定）。

### 9.4 S1 で未実施のこと（S2 以降）

- キー 1-5・Space 順送り、連動ルール（DONE→出力 ON、HOLD／KEEP→OFF）、
  行色、出力チェック列は S2 の範囲。S1 では状態を変更する UI はない
  （全ファイルが「未確認」と表示される）。
- 一括チェック・件数表示（S3）、対応区分・リスク・要約（S4）、一括承認
  （S5）、CSV（S6）、ZIP（S7）、セッション保存（S8）には着手していない。

### 9.5 Windows 実機での確認手順（S1 受け入れテスト）

1. CI 成果物 ZIP の `WinMergeU.exe` で 2 フォルダ比較を開く。
2. 一覧に「確認内容」列が表示され、全ファイル（フォルダ行を含む）が
   「未確認」と表示されること。既存の列（比較結果・日付・サイズ等）が
   壊れていないこと。
3. 列ヘッダの「確認内容」でソートできること（全て未確認のため順序は
   変わらないが、エラーにならないこと）。
4. 列選択ダイアログで「確認内容」列を非表示／再表示できること。
5. 3 フォルダ比較でも同じ列が出ること。
6. 大きいファイルを含むフォルダで「大きいファイルを比較しますか」の
   ダイアログが出た場合、日付・サイズの表示がずれていないこと
   （§9.3 のハードコード列番号の回帰確認）。
7. 再スキャン（F5）後も列と表示が変わらないこと。

### 9.6 CI 結果と修正（PR #6）

- PR #6（`merge20/phase2-s1`、14 コミット）。CI 実行 45592525097 は
  failure：`UnitTests.vcxproj` のリンクで LNK2019（`Merge20FileStates::
  GetReviewState`／`SetReviewState` の unresolved external）。原因は
  UnitTests プロジェクトが `DiffContext.cpp` を直接コンパイルするのに
  `Merge20FileStates.cpp` を含んでいなかったこと。本体（WinMergeU）の
  ビルド自体は通っていた。`UnitTests.vcxproj`／`.filters` に
  `Src\Merge20\Merge20FileStates.cpp`（＋2 ヘッダ）を追加して修正。
  以後の教訓：`Src/` の新規 `.cpp` は `Merge.vcxproj` だけでなく
  `UnitTests.vcxproj`（`DiffContext.cpp` 等を直接使う側）への登録も必要。
- 再実行 45594649835 は success。成果物 2 件（いずれも 2027-02-23 失効）：
  `WinMerge-x64.zip`（26,037,934 バイト、ID 15013980458）、
  `WinMerge-x64-Setup-x64.exe`（39,509,228 バイト、ID 15011999981）。

## 10. Phase 2 S2（連動ルール＋キー）— ブランチ `merge20/phase2-s2`

（L6 で補完。元資料は `merge20-complete-plan.md` の進捗ログ
（2026-11-25、S2 の項）のみ。当時はこのファイルに §10 がなく、
§12 冒頭に「未記録」と注記されていた。以下の内容は進捗ログの
記載を写したものであり、L6 で新たに調べた事実は含まない。
Windows 実機での確認手順の個別記録は進捗ログには残っていない
ため、ここには記載しない。）

設計書 `merge20-phase2-design.md` §3.2 どおり。確認内容 5 状態と
出力チェックの連動ルール、キー 1〜5・Space 順送り、出力セル
クリックでの手動 ON／OFF 切替、行色。

### 10.1 変更一覧

- `Src/Merge20/Merge20FileStates.h/.cpp` — `HasState`／
  `Get/SetIncludeInOutput` を追加。
- `Src/DiffContext.h/.cpp` — `Get/SetMerge20IncludeInOutput`、
  `SetMerge20ReviewState(DIFFITEM)`、`ApplyMerge20ReviewState`。
  出力チェックの初期値：変更・旧のみ＝ON、新のみ・同一＝OFF。
- `Src/DirDoc.h/.cpp` — フォワーダ。
- `Src/DirViewColItems.cpp` — 新列「出力」（`regName Soutput`）。
  ☑ U+2611／☐ U+2610 の文字表示。フォルダ行は空欄。ソートは
  未チェック＜チェック。`f_cols` 既定順 7／`f_cols3` 既定順 8。
  表の末尾に配置。
- `Src/DirView.cpp` — `PreTranslateMessage` にキー 1〜5 と Space
  順送り（一覧内のみ、ラベル編集中は無効、Ctrl/Alt/Shift 付きは
  除外）。`OnClick` に出力セルクリックでの手動 ON／OFF 切替。
  `GetColors` に行色：SPECIAL＝薄赤、HOLD＝薄黄、KEEP＝灰。
  背景のみ変更し、差分色の文字色は残す。
- `Translations/WinMerge/Japanese.po` — 訳を追加。

連動ルールは HTML の `setRev`／`cycRev` と同一であることを実読で
確認：DONE→ON（ただし 2-way の新のみファイルは ON にしない）、
HOLD／KEEP→OFF、TODO／SPECIAL→変更なし。読み込み時の自動
「済」化はなし。

### 10.2 設計書・仕様書との食い違い（判断理由つき）

1. **設計書 §3.2 の推奨案 (a)（ステートイメージリストによる自前
   チェック描画）は採用せず。** リストビュー項目のステートイメージは
   1 行に 1 つで第 1 列にしか表示されず、ツリーモードでは既に展開／
   折畳みアイコンに使われているため。代わりにフォールバック案 (b)
   （☑／☐ の文字表示）を採用し、クリックでの手動 ON／OFF 切替も
   実装。値は `LVN_GETDISPINFO` で動的供給されるため仮想リストとの
   整合性は保たれている。行色は固定色でテーマ対応は後回し
   （設計書 §3.2 どおり）。

### 10.3 CI 結果と修正（PR #7）

- PR #7（`merge20/phase2-s2`、15 コミット）を検証のうえ API で
  マージ（マージコミット 07922d2）。
- CI 失敗 1 回：初回実行 45600362144 が error C2039
  （`'isDirectory'` は DIFFITEM ではなく DIFFCODE のメンバ）。
  3 箇所を `di.diffcode.isDirectory()` に修正。修正 1 件目だけの
  実行 45602942908 は failure、2 件とも含む実行 45602987787 は
  success、最終ドキュメントのみのコミットの実行 45605157867 も
  success。
- 実行 45605157867 は **success**。成果物 2 件（いずれも
  2027-02-23 失効）：`WinMerge-x64.zip`（26,039,775 バイト、
  ID 15017199207）、`WinMerge-x64-Setup-x64.exe`（39,520,662
  バイト、ID 15017209085）。
- 副次的注意：`commit_file.py` を短い間隔で連続実行すると 422
  "Update is not a fast forward" になることがある（ref 更新直後の
  GET が古い head を返す）。数秒待って再実行すれば解決する。

## 11. Phase 2 S3（一括チェック＋件数表示）— ブランチ `merge20/phase2-s3`

（L6 で補完。元資料は `merge20-complete-plan.md` の進捗ログ
（2026-11-25、S3 の項）のみ。当時はこのファイルに §11 がなく、
§12 冒頭に「未記録」と注記されていた。以下の内容は進捗ログの
記載を写したものであり、L6 で新たに調べた事実は含まない。
Windows 実機での確認手順の個別記録は進捗ログには残っていない
ため、ここには記載しない。）

設計書 `merge20-phase2-design.md` §3.3 どおり。一括チェック
（確認内容の複数選択＋件数表示＋実行）と件数表示（「表示 N 件中
M 件チェック」）、ディレクトリ単位の全チェック／全解除。

### 11.1 変更一覧

- 新規 `Src/Merge20/Merge20BatchCheckDlg.h/.cpp`（`CTrDialog`
  派生）。確認内容 5 状態のチェックボックス（複数選択可）、対象
  件数を実行前に表示（OK ボタンのラベル自体が「選択分を一括
  チェック（N件）」）、「表示分すべてチェック」「全解除」を同じ
  ダイアログに配置。未選択時は OK 無効。キャンセルでは無変更。
  右クリックメニューから「一括チェック…」で起動。
- 件数表示（`CDirView::GetMerge20StatusCountText()`／
  `UpdateMerge20StatusCount()`）。「表示 N 件中 M 件チェック」。
  分母＝表示中のファイル行数、分子＝そのうち出力チェック ON の
  件数。
- ディレクトリ単位の全チェック／全解除も実装（右クリックメニュー
  に「配下をすべてチェック」「配下のチェックをすべて外す」
  （ディレクトリ行選択時のみ）。DIFFITEM の `GetFirstChild()`／
  `GetFwdSiblingLink()` による再帰走査のため、折りたたまれた
  サブフォルダ配下も含む）。
- `Translations/WinMerge/Japanese.po` — 11 件追加。
- `Src/resource.h` — IDD 293、IDC 1719〜1726、コマンド
  38931〜38933。
- `Src/Merge.rc` — ダイアログテンプレート＋レイアウト登録。
- `Src/Merge.vcxproj`／`.filters` — 新規ファイル登録。
  `UnitTests.vcxproj` は不要（ダイアログは `DirView.cpp` から
  だけ使われる）。

### 11.2 T-2-2 の解消：ステータスバーの更新経路

ステータスバーは `CDirFrame::m_wndStatusBar`（`CStatusBar`。
設計書の推測の BasicFlatStatusBar ではなかった）。ペイン 0 が
メッセージペインで、唯一の書き込み口は
`CDirFrame::SetStatus()`、唯一の書き手は
`CDirView::OnTimer(TIMER_ID_STATUSBAR_UPDATE)`（比較完了後
150ms・選択変更時 100ms のワンショット）。対策は二重経路：
(1) `OnTimer` のメッセージに件数テキストを追加（「選択表示 |
表示 N 件中 M 件チェック」）して選択変更で消えないようにした、
(2) `Redisplay()` 末尾と、出力列クリック・キー 1〜5・Space・
一括チェック／ディレクトリ単位操作の直後に
`UpdateMerge20StatusCount()` で直接書き込む。

### 11.3 設計書・仕様書との食い違い（判断理由つき）

1. **一括チェックの UI をメニュー方式ではなくダイアログにした。**
   標準メニューは項目選択で閉じるため、複数選択＋件数確認＋実行
   という HTML ポップアップと等価にならない。既存の ArchiveDlg
   等と同じ作り。
2. **範囲の判断（仕様書に明記がなく HTML を正本とした）：**
   「選択分」「表示分すべてチェック」は表示中のファイルが対象、
   「全解除」は比較全体の全ファイル（HTML の `batchChk(false)`
   と同じ）。件数表示の分子は表示中のチェック済み件数（HTML の
   `cntLbl` は全ファイル中のチェック済み件数だったが、仕様書
   §3（3/4）の文言を優先）。件数表示の分母は「表示中のファイル
   行数」（ディレクトリ行と「..」行は数えない）。

### 11.4 CI 結果と修正（PR #8）

- PR #8（`merge20/phase2-s3`、13 コミット）を検証のうえ API で
  マージ（マージコミット 1dbe316）。
- CI 失敗 2 回：実行 45609234086 が error C1010（ダイアログ
  `.cpp` が `#include "StdAfx.h"` なのに `Merge.vcxproj` の登録を
  pch.h グループ（STL のみ）にしていた。`ArchiveDlg.cpp` と同じ
  素のエントリに修正）。実行 45611123171 が CVTRES CVT1100:
  duplicate resource DIALOG 292（既に IDD_ARCHIVE。`resource.h`
  の IDD 一覧は値順に並んでいない）。293 に変更。IDC
  1719〜1726・コマンド 38931〜38933 は全 define 対象で空きを
  確認済み。
- **教訓**：新規 `.cpp` の PCH グループ（ダイアログは既定の
  StdAfx.h グループ）と、リソース ID の空き確認は全 define を
  値でソートすること。
- 実行 45613020758 は **success**、その後のドキュメントのみの
  コミットの実行 45614897524 も **success**。成果物 2 件（実行
  45614897524、いずれも 2027-02-23 失効）：
  `WinMerge-x64.zip`（26,047,167 バイト、ID 15022875796）、
  `WinMerge-x64-Setup-x64.exe`（39,520,359 バイト、
  ID 15022376890）。

## 12. Phase 2 S4（対応区分・リスク・ルールベース要約）— ブランチ `merge20/phase2-s4`

（注：§10（S2）・§11（S3）は L6 で補完済み。§10・§11 参照。
元資料は `merge20-complete-plan.md` の進捗ログ。）

### 12.1 変更一覧

- 新規 `Src/Merge20/Merge20Classify.h/.cpp`（純粋モジュール、MFC・`_()` なし）：
  HTML `17_v17.15.html` の `runAnalysis` 分類と `analyzeInsights` を移植。
  差分行（OP_1STONLY＝旧欠落／OP_DIFF＝書換／OP_2NDONLY＝追加／OP_TRIVIAL＝
  除外／OP_NONE＝文脈行＝ハンク直上の関数定義行）から対応区分・リスク・
  洞察（端数処理・SQL・ifガード削除・条件／境界・定数・影響関数）を算出。
  リスクは注意点の数：欠落あり＝1点、変更あり＝1点、差分行内にリスク重要語＝
  1点。0点＝なし、1点＝中、2点以上＝高。洞察はリスク点に加えない
  （デモの純粋な定数変更が「中」のままになるよう HTML と一致させた）。
  洞察は検出順に最大 4 件、影響関数は最大 5 件。
- 新規 `Src/Merge20/Merge20ClassifyDoc.h/.cpp`（アダプタ、MFC を使う）：
  `CMergeDoc` の DIFFRANGE から差分行を構築（行テキストは
  `ComputeApparentLine` 経由でゴースト安全に取得。OP_DIFF は両側を順に
  対にし、長い側の余りは片側空きの OP_DIFF 行として残す。OP_TRIVIAL は
  両側の大きい方を行数として数える）。要約文を `_()` で合成。
  要約の1行目：`欠落：N行 / 変更：M行 / 追加：K行`（除外行があれば
  ` / 除外：E行` を追加）。続いて洞察（`⚠ ` 付き）、重要語ヒットなら
  `⚠ 重要語を含む`、影響関数があれば `影響する関数: fn1, fn2`。
- `Src/Merge20/Merge20Types.h`：`Merge20FileState` に `classified` を追加
  （未開封ファイルは false のまま）。
- `Src/DiffContext.h/.cpp`：`SetMerge20Classification(relPath, state)` を追加。
  S4 項目だけをサイドカーに格納し、確認内容・出力フラグ・メモは保持する。
- `Src/IDirDoc.h`／`Src/DirDoc.h/.cpp`：`SetMerge20Classification(paths, state)`
  を追加。`UpdateChangedItem` と同じ `FindItemFromPaths`（＋左右入替の
  フォールバック）で項目を探し、存在区分で対応区分を上書きする
  （2-way のみ：旧のみ→COPY_OLD_FILE、要約は `旧のみ存在（N行）`、
  新のみ→SKIP_NEW_FILE、要約は `新のみ存在（N行）`。リスクは計算通り）。
  格納後に `GetItemIndex`＋`UpdateDiffItemStatus` で行を再描画する。
- `Src/MergeDoc.cpp`：`Rescan()` 末尾にフックを追加。成功（RESCAN_OK）・
  非バイナリ・2 バッファ・dir doc あり・差分ありのときだけ
  `Merge20ClassifyFromMergeDoc` で分類し `m_pDirDoc` に渡す。
  `ShowMergeDoc` が rescan 前に dir doc を設定するため、初回 rescan でも
  渡せる。再スキャンごとに再分類するので編集後も新鮮。3-way は未分類
  （DIFFRANGE が OLD／NEW の行数に対応しないため）。
- `Src/DirViewColItems.cpp`：新列 3 つ（regName `Saction`／`Srisk`／`Ssummary`、
  見出し「Action Category」／「Risk」／「Summary」）。`f_cols`／`f_cols3`
  とも表末尾（出力列の後、デバッグ列の前）に追加。既定で表示。
  既定順は 2-way が 8／9／10、3-way が 9／10／11。
  未分類ファイルは「Not calculated」（→未計算）、フォルダ行は空欄。
  ソートは対応区分・リスクを enum 順、要約を文字列順。未分類は最後に寄せる。
- `Src/OptionsDef.h`／`Src/OptionsInit.cpp`：新オプション
  `Merge20/RiskKeywords`（`Merge20/RiskKeywords`）。既定値は
  `税;額;率;端数;金額;税額;丸め;四捨五入;SQL`（ソースは `\x` 逃しで
  ASCII のまま）。差分行への部分一致（大文字小文字を区別しない）。
  設定画面の編集 UI は S4 では未実装（レジストリ／INI 値の編集）。
- `Src/Merge.vcxproj`／`.filters`、`Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／
  `.filters`：新規ファイルを登録。`Merge20Classify.cpp` は pch.h グループ
  （PCH Use＋pch.h＋2.pch、Merge20FileStates.cpp と同じ）。
  `Merge20ClassifyDoc.cpp` は StdAfx.h グループの既定エントリ
  （§12.4 の C1189 修正参照）。テストは
  `Testing/GoogleTest/Merge20/Merge20Classify_test.cpp`（新規）。
- `Testing/GoogleTest/Merge20/Merge20Classify_test.cpp`：10 テスト。
  HTML の loadDemo サンプル 3 件は、HTML 自身の diffFast＋analyzeInsights を
  Node で実行した ground truth と一致することを検証（件数・対応区分・
  リスク・洞察の順序・定数の旧→新・影響関数名）。ほかに追加のみ・同一・
  旧欠落のみ・重要語による高への昇格・SQL 洞察（リスク点なし）・
  除外行・関数名マッチ・重要語パース。
- `Translations/WinMerge/Japanese.po`：S4 の訳 27 件を追加。列の説明文に
  「生成AIではなく決定論的なパターンマッチ」である旨を明記。
  流用した既存訳：「同一」（Identical）、「なし」（None）。

### 12.2 設計書・仕様書との食い違い（判断理由つき）

- 段階 1（開封時計算＋キャッシュ、未開封は「未計算」）どおり。フォルダ
  比較リスト表示時点では DIFFITEM に内訳がないため、一括で全ファイルを
  計算する段階 2 は実装していない（100〜1000 ファイル規模で UI が固まる
  ため）。
- 追加のみ／新のみファイルに重要語が含まれると「中」になる。HTML は
  「なし」。仕様書 §3 (2/4) の数え方（注意点が 1 つなら「中」）を正とした。
- OP_DIFF ブロックで両側の行数が違う場合、長い側の余りの旧行は変更行として
  数えるが、if ガード削除の検査はしない（HTML の余り 'del' 行は検査する）。
  影響は小さい（余り行の if 削除が洞察に出ないだけ）。
- 定数変更の要約は `->`（ASCII）で表示し、HTML の `→` とは異なる。
  `_()` の msgid は narrow 文字列のため `\x2192` は矢印として残らない。
  日本語訳側は `→` を使っている（`定数の変更（%1 → %2）`）。
- タスク文にあった HTML の `analyzeSemanticInsights` は 17_v17.15.html には
  存在しない。`analyzeInsights` のみを移植した。
- HTML は読み込み時に MERGE_CLEAN_OLD を自動で「済」にするが、仕様書
  §4(c) に反するためフォークでは行わない（S1 の方針どおり）。
- リスク重要語の編集 UI（設定画面）は未実装。オプション値の直接編集のみ。
  後続作業として残す。

### 12.3 Windows 実機での確認手順（S4 受け入れテスト）

1. CI 成果物 ZIP の `WinMergeU.exe` で、HTML のデモと同じ 2 フォルダ比較を
   開く（HouseValuationService.java、FixedAssetEngine.java、
   NoticeFormMapper.java、LegacyExporter.java（旧のみ）、CloudSync.java
   （新のみ）を含むフォルダ対）。
2. 開く前：全ファイルの「対応区分」「リスク」「要約」列が「未計算」と
   表示されること。フォルダ行は空欄であること。
3. HouseValuationService.java をダブルクリックで開く。一覧に戻ると
   対応区分が「要判断（欠落＋変更）」、リスクが「高」、要約が
   `欠落：2行 / 変更：1行 / 追加：0行 / ⚠ 定数の変更（20250401001289 → 20261116009988） / ⚠ if条件の削除（旧独自ロジックの消失） / ⚠ 条件・境界の変更 / 影響する関数: calc`
   と表示されること（HTML の表示と突き合わせる。矢印は `→`）。
4. FixedAssetEngine.java を開く：対応区分「要判断（欠落＋変更）」、
   リスク「高」、要約に `欠落：4行 / 変更：2行` と
   `定数の変更（00010402998 → 00020805112）`／`端数処理の変更`／
   `if条件の削除`／`条件・境界の変更`／`影響する関数: computeTax` が
   出ること。
5. NoticeFormMapper.java を開く：対応区分「要判断（同一書換）」、
   リスク「中」（純粋な定数変更だけでは「高」にならないこと）、
   要約に `変更：2行` と `定数の変更（10002345 → 20009876）` と
   `影響する関数: getForm` が出ること。
6. LegacyExporter.java（旧のみ）を開く：対応区分「旧のみファイル」、
   リスク「中」、要約 `旧のみ存在（N行）`。CloudSync.java（新のみ）を
   開く：対応区分「新のみファイル」、リスク「なし」、要約 `新のみ存在（N行）`。
7. 「対応区分」「リスク」「要約」の列ヘッダでソートできること
   （未計算のファイルが最後に寄ること）。
8. ファイルを開いたまま編集して保存し、一覧に戻ると要約が更新されること
   （再スキャンごとに再分類される）。
9. 3 フォルダ比較でも同じ 3 列が出ること。3-way でファイルを開いても
   「未計算」のまま変わらないこと（3-way は分類対象外）。
10. 列選択ダイアログで 3 列を非表示／再表示できること。列の説明文に
    「生成AIではなく決定論的なパターンマッチ」と書かれていること。

### 12.4 CI 結果と修正（PR #9）

- PR #9（`merge20/phase2-s4`、23 コミット：実装 20＋修正 2＋本記録 1）。CI 実行 45621871575 は
  failure：`Merge20ClassifyDoc.cpp` のコンパイルで error C1189
  （`afxstr.h can only be used in MFC projects`）。原因は
  `Merge20ClassifyDoc.cpp`（MergeDoc.h 等の MFC ヘッダを使う）を
  pch.h グループ（STL のみ）に登録したこと。S3 の教訓（ダイアログ .cpp は
  StdAfx.h グループの既定エントリ）と同じ落とし穴。
  修正：`Merge20ClassifyDoc.cpp` の先頭 include を `pch.h` から `StdAfx.h` に
  変更し、`Merge.vcxproj` の登録を既定エントリ（PCH 指定なし）に変更
  （2 コミット）。`Merge20Classify.cpp`（MFC を使わない純粋モジュール）は
  pch.h グループのまま問題ない。
  以後の教訓：MFC ヘッダを使う新規 .cpp は StdAfx.h グループの既定エントリ
  にし、先頭 include も `StdAfx.h` にすること。pch.h グループは
  MFC を使わない純粋モジュール専用。
- 修正後の実行 45623766720 は success。成果物 2 件（いずれも
  2027-02-23 失効）：`WinMerge-x64.zip`（26,080,885 バイト、
  ID 15026149062）、`WinMerge-x64-Setup-x64.exe`（39,562,669 バイト、
  ID 15026982955）。
- 中間の実行 45623731510（修正 2 コミットのうち 1 件目だけのヘッド）は
  失敗見込みのため無視した（2 件目を含む最終ヘッドの実行で検証）。

### 12.5 S4 で未実施のこと（S5 以降）

- リスク重要語の設定画面エディタ（オプション値の直接編集のみ可能）。
- 段階 2 の一括計算（未開封ファイルの事前分類）。
- S5（一括承認）の安全マージ定義そのもの。S4 の対応区分・リスクは
  S5 の入力になる型（Merge20Types.h）として用意済み。

## 13. Phase 2 S6（差分一覧 CSV）— ブランチ `merge20/phase2-s6`

### 13.1 変更一覧

- 新規 `Src/Merge20/Merge20Csv.h/.cpp`（純粋モジュール、MFC・`_()` なし）：
  HTML `17_v17.15.html` の `dlCsv` の行整形を移植。テキスト欄は常時
  ダブルクォート囲み、埋め込みの `"` は `""` に二重化、4 件数
  （差分計／旧欠落／変更／新追加）は裸の数字。1 行は 13 欄固定
  （設計書 §3.5 の列順）。行末は含まない（呼び出し側が CRLF で
  結合する）。g++ ハーネスで検証済み（エスケープ 5 件・行整形 3 件・
  列数 1 件、全 9 件合格）。`Merge20CsvFormatRow` の結果は
  `Merge20Csv_test.cpp`（後述）の期待文字列と一致する。
- 新規 `Src/Merge20/Merge20CsvExport.h/.cpp`（アダプタ、MFC を使う）：
  範囲判定（`Merge20CsvHasDiff`：変更あり、またはいずれか片側のみ
  存在＝HTML の `tot > 0`）、範囲件数の数え上げ
  （`Merge20CsvCountRanges`：比較全体のツリー走査。フィルタで
  隠れたファイルや折りたたまれたフォルダ配下も含む。HTML の
  `dlCsv` が `files` 全体を対象にするのと同じ）、BOM 付 UTF-8・
  CRLF でのファイル書き出し（`UniStdioFile::OpenCreateUtf8`＋
  `SetBom(true)`＋`WriteBom()`。`WriteBom` は unicoding が UTF-8
  かつ `m_bom` が真のとき `EF BB BF` を書く）。ヘッダ行は 13 列名を
  `_()` 経由で結合（引用符なし。HTML の `rs[0]` と同じ）。値は
  `DirViewColItems` のラッパー経由で一覧表示と同一の文字列を取り、
  件数とメモはサイドカーから取る。ディレクトリ行は出さない。
- 新規 `Src/Merge20/Merge20CsvExportDlg.h/.cpp`（CTrDialog 派生）：
  範囲選択ダイアログ。ラジオボタン 2 つ（選択分のみ／差分全件）に
  実行前の件数を表示する（S3 の一括チェックダイアログと同じ作り）。
  選択分が 0 件のときはそのラジオを無効化し、差分全件を既定にする。
  キャンセルでは何もしない。
- `Src/DirViewColItems.h/.cpp`：CSV 出力用の表示文字列ラッパー 7 件
  （`Merge20CsvGetFileNameText`／`DirText`／`StatusText`／
  `ReviewText`／`ActionText`／`RiskText`／`SummaryText`。既存の
  static な列 getter への転送。CSV と一覧の表示がずれないようにする
  ためのもの）。新列「メモ」（regName `Smemo`、見出し `Memo`）。
  `f_cols`／`f_cols3` とも表末尾（要約列の後、デバッグ列の前）に
  追加。既定で非表示（順序 -1。S0 の無視差分列と同じ）。値は
  サイドカーの `memo`（編集 UI がないため今は空。§13.2 参照）。
  フォルダ行は空欄。ソートはメモ文字列順。
- `Src/DirView.h/.cpp`：右クリックメニューに「差分一覧 CSV…」
  （`ID_MERGE20_CSV_EXPORT`。一括チェックの直後。常時表示）と
  ハンドラ `OnMerge20CsvExport` を追加。ハンドラは範囲件数を数え、
  両方 0 件なら情報メッセージを出して終了する。範囲ダイアログで
  OK なら保存ダイアログ（`SelectFile` の保存モード。既定ファイル名
  `DiffList_YYYY-MM-DD.csv`。HTML と同じ。CSV フィルタ付き）を開き、
  出力後に「N件のファイルを <パス> に出力しました」を報告する。
  どちらかのダイアログでキャンセルした場合は何もしない。
- `Src/resource.h`：`IDD_MERGE20_CSV_EXPORT` 294、
  `IDC_MERGE20_CSV_SELECTED` 1727、`IDC_MERGE20_CSV_ALL` 1728、
  `ID_MERGE20_CSV_EXPORT` 38934。いずれも全 define を値でソート
  して空きを確認済み（S3 の CVT1100 の教訓）。`_APS_NEXT_*` も
  更新済み（RESOURCE 295、COMMAND 38935、CONTROL 1729）。
- `Src/Merge.rc`：ダイアログテンプレート（300×112。範囲グループ＋
  ラジオ 2 つ＋説明文＋OK／キャンセル）＋マージン登録＋
  `AFX_DIALOG_LAYOUT` 登録の 3 か所。
- `Translations/WinMerge/Japanese.po`：S6 の訳 18 件を追加
  （メニュー、ダイアログ、CSV ヘッダ 9 件、メモ列の見出しと説明文、
  結果メッセージ）。「対応区分」「リスク」「確認内容」「要約」は
  既存訳を流用。CSV ヘッダの msgid（`Missing lines` 等）は既存の
  `Missing`（訳：欠落している）や `Changed` とは別物として新規に
  起こした（流用すると訳がずれるため）。
- `Src/Merge.vcxproj`／`.filters`：新規 6 ファイルを登録。
  `Merge20Csv.cpp` は pch.h グループ（純粋モジュール。S4 の
  `Merge20Classify.cpp` と同じ）。`Merge20CsvExport.cpp` と
  `Merge20CsvExportDlg.cpp` は StdAfx.h グループの既定エントリ
  （S3・S4 の教訓どおり。先頭 include も `StdAfx.h`）。
- `Testing/GoogleTest/Merge20/Merge20Csv_test.cpp`：7 テスト
  （列数 13、引用符なし／カンマ・改行あり／`"` 二重化のエスケープ、
  サンプル行の完全一致、日本語メモを含む行、空行、件数の裸数字）。
  `Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` に
  純粋モジュールとテストを登録（S1 の LNK2019 の教訓）。

### 13.2 設計書・仕様書との食い違い（判断理由つき）

- 設計書 §3.5 は `DirCmpReport::GenerateReport(REPORT_TYPE_COMMALIST)`
  の流用と「第 1 版では表示中の列順をそのまま使う」を提案しているが、
  そのままでは S6 の受け入れ基準（13 列固定・メモを含む）を満たせない
  ため、列の部分は流用しなかった。理由は 3 つ。(1) `DirCmpReport` は
  表示中の列を表示順に出すだけで、固定 13 列・固定順にはできない。
  (2) ヘッダの前にタイトル行と時刻行を書き、純粋な CSV にならない。
  (3) 値の引用符は区切り文字を含む場合だけで、埋め込みの `"` を
  エスケープしないため、メモ・要約文に `"` が入ると Excel で崩れる。
  代わりにファイル生成（`UniStdioFile::OpenCreateUtf8`）だけを流用し、
  BOM 書き込み（`SetBom(true)`＋`WriteBom()`）と 13 列固定の書き出し
  を専用モジュールにした。`DirCmpReport` 自体は無変更のため、既存の
  レポート動作は変わらない（仕様書 §7 互換性）。
- 引用符の扱いは既存レポートの作法ではなく HTML の `dlCsv` 規則
  （テキスト常時引用、`"`→`""`）を採用した。上記 (3) のとおり既存
  の作法では監査証跡として壊れるため。HTML が正本であることも根拠。
- 「選択分のみ」は出力チェック ON のファイル（HTML の `f.chk`）、
  「差分全件」は差分のあるファイル（HTML の `tot > 0`）。いずれも
  比較全体が対象で、現在のフィルタで表示中のものだけではない。
  HTML の `dlCsv` が `files` 全体を対象にするのと同じ。S3 の
  「表示分すべてチェック」（表示中のみ）とは範囲が違う点に注意。
  「選択分のみ」には同一ファイルも含まれる（チェックが ON なら。
  HTML の `chk` フィルタと同じ）。
- メモ列は CSV に含めるが、メモの編集 UI は未実装（別途の課題として
  残す。セッション保存 S8 とあわせて検討）。一覧の「メモ」列は追加
  済みだが値は空。メモを書けるようになるまでは CSV のメモ列も空。
- 未開封ファイルの件数・対応区分・リスク・要約は S4 の段階 1 のまま
  （件数 0、対応区分・リスク・要約は「未計算」）。CSV の差分計は
  サイドカーの欠落＋変更＋追加の合計（HTML の `tot` と同じ定義）で、
  未開封ファイルは 0 になる。段階 2 の一括計算ができれば改善される。
- フルパスは相対パス（HTML の `f.path` と同じ。絶対パスではない）。
  ディレクトリは一覧の「Path」列と同じ値で、ルート直下のファイルは
  `.\` になる（HTML の `(ルート)` とは異なる。WinMerge の表示に
  合わせた）。
- 確認内容は一覧の短縮表示（`✓済` 等）。HTML は `✓済:自動マージ対象`
  のフル表示だが、CSV は一覧と同一の文字列にした。
- ヘッダ行は翻訳対象のため、日本語 UI では設計書 §3.5 の 13 列名
  （ディレクトリ、ファイル名、フルパス、存在区分、対応区分、リスク、
  確認内容、差分計、旧欠落、変更、新追加、要約、メモ）と一致する。
  英語 UI では対応する英字名になる。

### 13.3 Windows 実機での確認手順（S6 受け入れテスト）

1. CI 成果物 ZIP の `WinMergeU.exe` で 2 フォルダ比較を開く
   （S4 と同じデモフォルダがよい。変更・旧のみ・新のみ・同一を含むもの）。
2. 一覧を右クリックし、「差分一覧 CSV…」が出ること（一括チェックの
   直後）。列選択ダイアログで「メモ」列を出せること（既定では非表示。
   値は空）。
3. 範囲ダイアログで「選択分のみ（N件）」「差分全件（M件）」の件数が
   表示されること。出力チェックを何も入れていない状態では
   「選択分のみ」が 0 件で選べないこと。
4. 「差分全件」を選んで保存する（既定ファイル名
   `DiffList_YYYY-MM-DD.csv`）。「N件のファイルを <パス> に出力
   しました」と出ること。
5. 出力した CSV を Excel で開く。文字化けがないこと（日本語の要約文・
   確認内容が読めること）。先頭行が 13 列のヘッダ
   （ディレクトリ、ファイル名、フルパス、存在区分、対応区分、リスク、
   確認内容、差分計、旧欠落、変更、新追加、要約、メモ）であること。
   同一ファイルの行がないこと。ディレクトリ行がないこと。
6. ファイルを 1 件開いて分類させた後、再度「差分全件」を出す。その
   ファイルの行で対応区分・リスク・要約が入り、差分計＝
   旧欠落＋変更＋新追加になっていること。未開封のファイルは
   「未計算」で件数 0 のままであること。
7. 出力チェックを入れたファイルだけが「選択分のみ」に出ること
   （同一ファイルにチェックを入れた場合はそれも出る）。
8. CSV をメモ帳で開き、先頭 3 バイトが `EF BB BF`（BOM）で、改行が
   CRLF であること。要約文に `"` やカンマが含まれる行が 1 フィールド
   に収まっていること（Excel で列ずれがないこと）。
9. 3 フォルダ比較でも同じ操作ができること。
10. 既存の「ツール→レポートの生成」が従来どおり動くこと
    （`DirCmpReport` 無変更の回帰確認）。

### 13.4 CI 結果と修正（PR #10）

- PR #10（`merge20/phase2-s6`、21 コミット：実装 18＋修正 1＋
  記録 2）。CI 実行 45630443647 は failure：`DirView.cpp(3397,3)`
  で error C2665（`AfxMessageBox` に合うオーバーロードがない）。
  原因は `_()` が `String` を返すのに `.c_str()` なしで
  `AfxMessageBox` に渡したこと（既存コードは `_(...).c_str()` と
  書いている）。修正は 1 行（`.c_str()` を追加。コミット
  `63030105`）。以後の教訓：`_()` の結果を Win32／MFC の
  `LPCTSTR` 引数に渡すときは `.c_str()` を付けること。
- 修正後の実行 45632640178 は success。成果物 2 件（いずれも
  2027-02-23 失効）：`WinMerge-x64.zip`（26,090,811 バイト、
  ID 15031523820）、`WinMerge-x64-Setup-x64.exe`（39,570,793
  バイト、ID 15032173206）。
- 単体テストは CI で実行され、全 575 テストが合格（`[ PASSED ] 575
  tests.`）。`Merge20Csv` の 7 テスト（列数、エスケープ 3 件、
  行整形 3 件）も全て合格。CI ログで `Merge20Csv.cpp`／
  `Merge20Csv_test.cpp` のコンパイルと 7 テストの `[ OK ]` を確認済み。

## 14. Phase 2 S8（セッション保存・復元）— ブランチ `merge20/phase2-s8`

### 14.1 実施した変更の一覧

新規ファイル：

- `Src/Merge20/Merge20Session.h`／`Merge20Session.cpp` — UI 非依存の
  純粋モジュール。UTF-8 エンコード／デコード、軽量 JSON
  パーサ／シリアライザ（整数のみ・入れ子上限 32・BOM 許容・
  不正バイトは U+FFFD）、セッション構造体（v1）と JSON 変換、
  ファイル入出力（一時ファイル＋リネーム、失敗時は直接書き込みに
  フォールバック）、変更検出（`Merge20SessionFileChanged`）。
  JSON は整形済み 2 スペース・UTF-8・BOM なしで、日本語はそのまま
  読める。
- `Src/Merge20/Merge20SessionDoc.h`／`Merge20SessionDoc.cpp` —
  MFC アダプタ。セッションファイルの場所（FNV-1a ハッシュ付き
  ファイル名）、`CDirDoc`／`CMergeDoc` からのセッション構築・
  適用、差分判定の抽出・復元、復旧提案ダイアログ。
- `Src/Merge20/Merge20MemoDlg.h`／`Merge20MemoDlg.cpp` —
  監査メモ編集ダイアログ（`CTrDialog`、IDD 295）。
- `Testing/GoogleTest/Merge20/Merge20Session_test.cpp` —
  純粋モジュールの単体テスト（UTF-8 往復、JSON 往復・異常系、
  セッション往復・形式異常、変更検出）。

変更したファイル：

- `Src/Merge20/Merge20Types.h` — `Merge20DiffJudgment` 構造体
  （diffIdx・op・begin/end[3]・state・srcPanes）と
  `Merge20FileState::diffJudgments` を追加。
- `Src/Merge20/Merge20FileStates.h`／`.cpp` — メモの
  `GetMemo`／`SetMemo` を追加。
- `Src/DiffContext.h`／`.cpp` — メモの取得・設定
  （`GetMerge20Memo`／`SetMerge20Memo`）、差分判定の保存
  （`SetMerge20DiffJudgments`）、セッション状態の一括復元
  （`SetMerge20SessionState`）、変更ファイルのリセット
  （`ResetMerge20ChangedFile`：確認内容を TODO に、出力フラグを
  初期値に、分類・差分判定を消去、メモは保持）。
- `Src/DirDoc.h`／`.cpp` — メモのフォワーダ
  （`GetMerge20Memo`／`SetMerge20Memo`）、
  `RefreshMerge20DiffJudgments`（保存前に開いている全マージ文書
  の判定をサイドカーへ）、`MergeDocClosing` での判定スタッシュ、
  `OnCloseDocument` 先頭での自動保存フック、
  `m_bMerge20SessionAsked`（復旧提案は文書あたり 1 回）。
- `Src/DirView.h`／`.cpp` — 右クリックメニューに「セッションを保存」
  「セッションを復元…」「監査メモを編集…」を追加（メモは単一
  ファイル選択時のみ）。`OnUpdateUIMessage` の比較完了時に復旧提案
  を呼ぶ。表示設定の取得・適用・列設定・表示位置復元の
  `Merge20GetDisplaySettings`／`Merge20SetDisplayFilter`／
  `Merge20ApplyColumnSettings`／`Merge20RestoreViewPosition` を追加。
- `Src/DirViewColItems.cpp` — `COLDESC_MEMO` の説明を更新
  （「編集は未実装」→右クリックメニューから編集できる旨）。
- `Src/MergeDoc.h`／`Src/MergeDocResultPane.cpp` —
  `Merge20SetResultSegmentState` を追加し、`StartMergeSession`
  （`BuildMergeResult`＋自動マージの直後）から
  `Merge20SessionRestoreMergeDoc` を呼んで保存済みの差分判定を
  復元する。
- `Src/OptionsDef.h`／`Src/OptionsInit.cpp` — 新規キー 8 件
  （`OPT_MERGE20_SESSION_AUTOSAVE`、`OPT_MERGE20_LEARNING_ENABLED`、
  `OPT_MERGE20_BATCH_APPROVE_DIALOG`、
  `OPT_MERGE20_SHOW_REVIEW_TODO/DONE/HOLD/SPECIAL/KEEP`）。
  いずれも既定 true。
- `Src/resource.h`／`Src/Merge.rc` — `IDD_MERGE20_MEMO`（295）、
  `IDC_MERGE20_MEMO_PATH`（1729）／`IDC_MERGE20_MEMO_TEXT`（1730）、
  `ID_MERGE20_SESSION_SAVE`（38935）／
  `ID_MERGE20_SESSION_RESTORE`（38936）／
  `ID_MERGE20_MEMO_EDIT`（38937）、メモダイアログのテンプレート・
  レイアウト・`AFX_DIALOG_LAYOUT` を追加。`_APS_NEXT_*` を更新。
- `Src/Merge.vcxproj`／`.filters`、
  `Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` —
  新規 .cpp／.h を登録（`Merge20Session.cpp` は pch.h グループ、
  `Merge20SessionDoc.cpp`／`Merge20MemoDlg.cpp` は StdAfx.h グループ）。
- `Translations/WinMerge/Japanese.po` — S8 の UI 文字列 24 件を追加
  し、`COLDESC_MEMO` の訳を更新。

### 14.2 設計・仕様との食い違いと判断理由

- **T-2-1 は (b) 自前実装**：`Src/`・`Externals/` を `json` で
  grep した結果、nlohmann／picojson／rapidjson／jsoncpp／Poco/JSON
  の既存利用は皆無だった。Poco は未初期化のサブモジュールに過ぎず
  何もリンクしていない。セッションの構造は固定で小さいため、汎用
  パーサは不要と判断し、整数のみ・入れ子上限 32 の軽量
  JSON 読み書きを `Src/Merge20/` 内に自前実装した。
  (c) ProjectFile（XML）拡張は仕様書の JSON 指定と矛盾するため
  不採用。
- **終了時自動保存のフック先は `CDirDoc::OnCloseDocument` の先頭**
  （T-2-5 の解決）。この時点ではマージ文書はまだ開いているため、
  保存前のリフレッシュで開いている文書の判定も含められる。
  比較スレッド実行中と未比較の文書は保存をスキップする。
  保存失敗はメッセージボックスで明示する（仕様書 §7）。
- **復旧提案は `CDirDoc::CompareReady()` ではなく
  `CDirView::OnUpdateUIMessage` の比較完了処理から呼ぶ**：
  `CompareReady()` は `SaveModified()` からも呼ばれる（比較中の
  ウィンドウを閉じる際にスレッド中断後に呼ばれる）。閉じようと
  している文書に復旧を提案するのは誤りのため、UI スレッドの
  比較完了イベント側に置いた。提案はリスト再構築とフォーカス・
  スクロール復元の後、かつレポート生成要求がない場合にのみ行う
  （コマンドラインのレポート生成を妨げない）。
- **比較オプションは保存するが復元時は適用しない（第 1 版の制限）**：
  適用には再スキャンが必要なため。保存値と現在値が異なる項目は
  復元レポートに列挙する。表示フィルタ・列順／列幅・隠し項目・
  フォーカス・スクロール位置は適用する（フィルタ類はグローバル
  設定に書き戻すため、復元後の既定値も変わる。手動でフィルタを
  切り替えたのと同じ副作用）。
- **手動保存／復元はフォルダ一覧の右クリックメニュー**
  （「セッションを保存」「セッションを復元…」）。設計書 §2.2 の
  メインメニュー配置ではなく、S3／S6 と同じ DirView 配置にした。
- **テーマ・検索文字列・スリム／ドック／カメラ／専用モード／ピン**：
  仕様書 §6.1 の「画面に出ているものはすべて」保存対象のうち、
  フォークにまだ存在しない Phase 4 機能（専用モード、スナイパー、
  Grep 検索、スリム表示）は保存する対象自体がない。テーマは
  参考として記録する設計だったが、グローバル設定が優先するため
  適用せず、セッションにも含めていない（テーマを変えると全比較に
  影響するため）。Phase 4 で当該機能が入った時点で保存対象に
  追加する。
- **確認内容フィルタのキー（`OPT_MERGE20_SHOW_REVIEW_*`）は用意
  したが UI はない**：S3 は設計書 §3.2 のフィルタ UI を実装して
  いないため。キーの値はセッションに記録され、将来のフィルタ
  UI が使える。
- **差分判定の復元**：保存時の Auto は `ResultChooseSources` で
  内容を復元した後、`Merge20SetResultSegmentState` で状態を
  Auto に戻す（`ResultChooseSources` は Chosen にしてしまうため）。
  Unresolved／Conflict は空のソース選択で未解決に戻す（自動マージ
  が解決してしまう差分も、利用者が意図的に未解決にしたものは
  戻す）。Edited は手編集の内容を再現できないため復元しない。
  op・begin・end が一致しない判定は無視する。
- **変更ファイルのリセット**：サイズ・更新日時・差分件数のいずれかが
  変わっていたファイルは確認内容を TODO に戻し、分類・差分判定を
  消去する。メモだけは保持する（ファイル内容の主張ではなく確認者の
  記録のため）。サイズ・更新日時が不明（-1）の側は変更とみなさない。
- **`IDirDoc` は変更しない**：アダプタは `dynamic_cast<CDirDoc*>`
  でフォルダ文書に到達する（S8 計画時の判断どおり）。

### 14.3 検証結果（ローカル g++ ハーネス）

純粋モジュール `Merge20Session` を g++（C++17）でビルドし、
ハーネスで 100 チェック全て合格：UTF-8 往復（日本語・絵文字・
引用符・制御文字、不正バイトは U+FFFD）、JSON 往復（日本語・
サロゲートペア・BOM 許容）、異常系 7 件（末尾ゴミ・未終端文字列・
小数・末尾カンマ・不正エスケープ・空文字・値なしキー）、
セッション往復（全フィールド・差分判定 2 件・旧のみファイル）、
ファイル入出力往復、形式異常（`app` 不一致・バージョン 2・
バージョン欠落・JSON でないものは拒否、ファイル 1 件の不良は
スキップして他を復元、差分 1 件の不良は差分だけ落とす）、
変更検出（サイズ・更新日時・差分件数の変化、不明値は変更と
みなさない、欠落側のままは変更とみなさない）。
生成 JSON は整形済みで日本語が生の UTF-8 で読めることを確認。
CI での gtest（`Merge20Session_test.cpp`、同内容）は 14.5 参照。

### 14.4 Windows 実機での確認手順（S8 受け入れテスト）

1. CI 成果物 ZIP の `WinMergeU.exe` で 2 フォルダ比較を開く
   （S4・S6 と同じデモフォルダがよい。変更・旧のみ・新のみ・同一を
   含むもの）。ポータブル版（`winmerge.ini` あり）では
   `Merge20Sessions` フォルダが exe の隣に作られること。
2. ファイルを何件か開いて分類させ、確認内容を DONE／HOLD 等に
   し、出力チェックを変え、メモを 1 件編集する（右クリック→
   「監査メモを編集…」）。ファイル比較で差分判定を 1 件入れる
   （マージセッションで左／右を選択）。
3. 右クリック→「セッションを保存」。「セッションを保存しました：
   <パス>」と出ること。`Merge20Session_<16桁16進>.json` が
   できていること。メモ帳で開き、日本語（メモ・要約）が文字化け
   なく読めること。
4. WinMerge を閉じて再度同じフォルダ組を開く。比較完了後に
   「%1に保存されたセッションが…見つかりました」と尋ねられること。
   「はい」で確認内容・出力チェック・メモが戻ること。ファイル比較を
   開いてマージセッションを開始すると、保存した差分判定が復元
   されていること。
5. 保存後に実ファイル 1 件を変更（内容を 1 行追加）してから
   手順 4 を行う。そのファイルだけ未確認（TODO）に戻り、出力
   チェックが初期値に戻り、分類・差分判定が消えていること。
   ただしメモは残っていること。
6. 右クリック→「セッションを復元…」で、他のフォルダ組の
   セッションファイルを選ぶと「別のフォルダ組のものです」と
   断られること。
7. 比較オプションを保存時と変えて（例：空白無視を切る）から
   復元すると、レポートに「保存された比較オプションは現在のものと
   異なるため適用していません」と出ること。
8. 終了時自動保存：手順 2 の状態で WinMerge を閉じ（手動保存なし）、
   再度開くと復旧提案が出ること。
9. 既存のフォルダ比較・ファイル比較・CSV 出力・一括チェックが
   従来どおり動くこと（回帰確認）。

### 14.5 CI 結果（PR #11）

- PR: https://github.com/askhim659/winmerge/pull/11（`merge20/phase2-s8` → `master`、29 コミット + 修正 4 コミット、未マージ）
- 実行 45646476887（2026-11-25 15:33Z）：failure。Merge.vcxproj のコンパイルエラー 16 件。
  - `Merge20MemoDlg.cpp(38)` C2664：`DlgUtils<CTrDialog>::GetDlgItemText` は `String&` を取るのに `CString` を渡していた。`String` に変更して修正（コミット 77609f14）。
  - `Merge20SessionDoc.cpp` C3861/C2593：`makelower` 未定義。`UnicodeString.h` の include 漏れに加え、`makelower` は `strutils` 名前空間内だった。include 追加（d58974d2）＋ `strutils::makelower` に修飾（1cfa38f6）。
  - `DirView.h(491-494)` C2248：S8 の表示設定メソッド 4 件を誤って `private:` 節に宣言していた。`public:` 節へ移動（01137b9a）。
- 実行 45648809914（修正 3 件込み、15:47Z）：failure。`makelower` の C3861 が残存（名前空間の見落とし）。上記 1cfa38f6 で修正。
- 実行 45650082458（2026-11-25 15:58Z）：**success**。ジョブ build 139331890185。
  - 成果物：`WinMerge-x64.zip` 26,133,366 バイト（ID 15040695967）、`WinMerge-x64-Setup-x64.exe` 39,622,714 バイト（ID 15040845810）。いずれも失効日 2027-02-23。
  - GoogleTest：581 テスト全パス。S8 新規 6 件（`Merge20Session.Utf8RoundTrip` / `JsonRoundTrip` / `JsonErrors` / `SessionRoundTrip` / `SessionFormatErrors` / `FileChanged`）すべて OK。
- 注意：修正コミットごとに push したため、途中の実行 45648764458 / 45648785154 も走ったが、最終実行のみが全修正込み。実機確認は §14.4 の手順で行うこと。

## 15. Phase 3 L0（学習コア・純粋モジュール）— ブランチ `merge20/phase3-l0`

設計書 `merge20-phase3-design.md` §5 L0 どおり。UI・永続化・ON/OFF の読み手・
記録フックは含まない（L1 以降）。候補の計算だけを行い、自動適用の経路は作らない
（仕様書 §4(c)）。`OPT_MERGE20_LEARNING_ENABLED` の読み手は作っていない。

### 15.1 変更一覧

- 新規 `Src/Merge20/Merge20Learn.h/.cpp`（計 1,368 行。MFC 非依存・`_()` なし・
  ASCII のみ。pch.h グループ。`Merge20Classify.cpp` と同じ作法）。
  - 完全一致キー（HTML `hunkKeyExact` と同一。JS の `\s` 集合で空白を潰して
    トリムし、`⇄`（U+21C4）で結んで `\n` 連結。OP_NONE 行は除外）。
  - 特徴キー（HTML `hunkFeatKey` と同一。type は del／mod／add。優先順位は
    round → ifdel → sql → tax → comment。ifdel・comment は旧側のみで判定）。
  - 閾値判定（HTML `matchLearn` と同一。完全一致は 1 件以上、特徴一致は
    3 件以上かつ賛成率 80% 以上。賛成率は整数比 `best*5 >= total*4` で判定し、
    小数は使わない。S8 JSON モジュールが整数のみであることと整合する）。
  - 訂正（HTML `learnFromHunk` と同一。候補と違う判定をすると旧方針に -1・
    新方針に +1 し、`lastStrat` を新方針にする。`ex`／`last` も更新する。
    訂正は自分の出典の件数だけに作用し、他出典の件数には触れない）。
  - 出典管理（HTML と同一の構造。ルールごとに出典別 `sources` を持ち、
    再取込は置換、出典単位の削除は寄与分だけ差し引く、統合出力は件数を合算して
    出典情報を落とす。`self` という出典 ID はフォークでは作らない）。
  - JSON v2 変換（S8 の `Merge20Session.h/.cpp` の DOM・パーサ・シリアライザ・
    UTF-8 変換を流用。セッション専用の変換関数 `Merge20SessionToJson`／
    `FromJson` は流用せず、学習用に `Merge20LearnToJson`／`FromJson`／
    `ImportJson`／`MergedToJson` を新設。フォークの `app` は `"Merge20Learn"`、
    取込時は HTML の `app`（`"マージ支援20号"`）も受理する）。
  - `hashStr` の C++ への正確な移植（32bit 整数演算。UTF-16 コード単位で
    ハッシュし、16 進はゼロ埋めなしの小文字。v1 ファイルの出典 ID
    `file:<hashStr(ファイル名)>` に使う）。
  - `Merge20LearnRecordJudgment`（HTML `learnFromHunk` 相当。完全一致キーと
    特徴キーの両方に記録する。`last`（当日日付）は呼び出し側が渡す）。
- 新規 `Testing/GoogleTest/Merge20/Merge20Learn_test.cpp`（16 テスト）。
- `Src/Merge.vcxproj`／`.filters`、`Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／
  `.filters` に新規ファイルを登録（Merge20Learn.cpp は pch.h グループ。
  UnitTests 側の Src ファイルは S4 と同じく素のエントリ）。

### 15.2 設計書・仕様書との食い違い（判断理由つき）

1. **小数を含むルールの per-rule スキップは不可**（設計書 §4.1／§8 の想定と
   異なる）。S8 パーサは整数以外をファイル全体のエラーにするため、そのような
   ファイルは全体を拒否する（`Merge20LearnFromJson` は false を返す）。
   仕様上は正しい v2 ファイルに小数は現れない（`counts`・`n` は整数）。
   テスト `JsonErrors` で全体拒否を検証した。
2. **重複キーは先勝ち**（T-3-2／U-3-4 を解消）。`ParseObject` は重複を許し
   `Find` は先頭を返すことを実読で確認した（`Merge20Session.cpp:252-285`、
   `:559-568`）。学習側の `counts` パーサも先頭の出現を採用する。
3. **OP_TRIVIAL 行は特徴キーの type 判定で mod 扱い**（HTML に相当概念が
   ないため。変更行のペアであることに変わりはない）。
4. **round 正規表現は大文字小文字を区別する**（HTML と同一。S4 の
   `RoundingRegex` は icase 付きのため流用していない。S4 の `SqlRegex` は
   `join` を含むため、学習用の SQL 正規表現も HTML のパターンをそのまま
   使った）。
5. **FromJson は出典つきルールの集計 `counts` を出典の合計から再計算する**
   （ストアの不変条件 `counts == 出典の合計` を保つため。ファイルの `counts`
   と出典合計が違うファイルは、出典合計を正として読み込む。出典なしルールは
   ファイルの `counts` をそのままファイル出典に帰属させる）。
6. **ImportJson は HTML `importLearn` と同一の平坦化**（ルールの `sources`
   は保持せず、ファイル全体を 1 出典として加算する。再取込は置換）。
7. **`Merge20LearnCandidate` は `Merge20Learn.h` に置いた**（設計書 §3.2 は
   `Merge20Types.h` としていたが、方針値 enum が `Merge20Learn.h` にあるため。
   `diffIdx`／`srcPanes` は L2 のアダプタが埋める）。

### 15.3 検証結果（ローカル g++ ハーネス＋Node ground truth）

- **ローカル g++ ハーネス：16 テスト全合格。** スタブ `pch.h`／`UnicodeString.h`／
  `DiffList.h`＋実物の `Merge20Session.cpp` でコンパイルし、テストファイルそのもの
  を実行した。`-fshort-wchar` は libstdc++ の `wstring` と ABI が合わず使えない
  ため使っていない（wchar_t 4 バイトで検証。`hashStr` の ground truth は BMP 文字
  のみのため Windows と同じ結果になる）。
- **Node ground truth：HTML の学習関数をそのまま Node で実行し突き合わせた。**
  `hashStr` 6 件・完全一致キー 4 件・特徴キー 20 件・閾値境界（2/3・3/4・4/5・
  7/9・8/10・3/3）・同数時の `lastStrat` 優先（挿入順の同数解決を含む）・訂正・
  出典削除・統合・取込／再取込・v1 取込がすべて一致した。
- **双方向チェック：フォークの出力 JSON を HTML の `importLearn`・`matchLearn` に
  通し、候補が一致すること・再取込で二重計上されないことを確認した。** 逆方向
  （HTML の実物 JSON をフォークで取込）も単体テストで確認した
  （`HtmlJsonImport`・`V1Import`）。
- **CI：実行 45660547413 success。GoogleTest 597 テスト全パス**（S8 の 581＋新規 16）。

### 15.4 Windows 実機での確認手順（L0）

L0 は UI・永続化・記録フックがないため、実機で確認できる動作はない。
実機確認は L1（`Merge20Learn.json` の生成と ON/OFF）以降に回す。
CI 成果物の ZIP を使って L0 の範囲で確認できることがあるとすれば、
WinMergeU.exe が従来どおり起動すること（回帰確認）のみ。

### 15.5 CI 結果（PR #12）

- PR: https://github.com/askhim659/winmerge/pull/12（`merge20/phase3-l0` → `master`、7 コミット、未マージ）
- 実行 45660547413（2026-11-25 17:03Z）：**success**。ジョブ build 139368899588。
  - 成果物：`WinMerge-x64.zip` 26,132,691 バイト（ID 15045626867）、
    `WinMerge-x64-Setup-x64.exe` 39,632,162 バイト（ID 15046410683）。
    いずれも失効日 2027-02-23。
  - GoogleTest：597 テスト全パス。Merge20Learn 新規 16 件
    （`StrategyNames` / `HashStr` / `ExactKey` / `FeatureKey` /
    `MatchThresholds` / `DominantTie` / `RecordCorrection` / `RemoveSource` /
    `GetSources` / `MergeSources` / `SourceDisplayName` / `JsonRoundTrip` /
    `HtmlJsonImport` / `V1Import` / `JsonErrors` / `MergedJson`）すべて OK。
  - CI 失敗 0 回（初回実行で success）。

## 16. Phase 3 L1（学習の永続化と ON/OFF）— ブランチ `merge20/phase3-l1`

設計書 `merge20-phase3-design.md` §5 L1 どおり。記録フック（L2）・候補提示 UI（L3）・
学習モーダル（L4）・JSON 入出力 UI（L5）は含まない。自動適用の経路は作らない
（仕様書 §4(c)）。

### 16.1 変更一覧

- 新規 `Src/Merge20/Merge20LearnDoc.h/.cpp`（MFC アダプタ、StdAfx.h グループの
  既定エントリ。`Merge20SessionDoc.cpp` と同じ作り）。
  - `Merge20LearnGetDirectory()`／`Merge20LearnGetFilePath()` — 学習ファイルは
    `Merge20Learn.json` 1 ファイル。INI（ポータブル）時は
    `<progpath>\Merge20Learn\`、それ以外は `%APPDATA%\WinMerge\Merge20Learn\`。
    セッションの `Merge20Sessions` とは別ディレクトリ（設計書 §3.4 どおり。
    学習データはフォルダ組に紐づかないため混ぜない）。
  - `Merge20LearnGetStore()` — 遅延読み込み。初回アクセス時にだけ読む。
    ファイルが無い場合は空ストア、壊れている場合も空ストアとして扱い、
    失敗を記憶する（起動を妨げない。失敗は学習モーダル（L4）で報告する）。
  - `Merge20LearnGetLoadError()` — 遅延読み込みの失敗を返す（L4 用）。
  - `Merge20LearnSaveStore()` — L0 の `Merge20LearnToJson` で書き出し、
    一時ファイル＋リネームによる保存（S8 の `Merge20SessionSaveToFile` と
    同じ方式。失敗時は直接書き込みにフォールバック）。フォルダが無い場合は
    作成し、失敗時はメッセージボックス用の文言を返す（仕様書 §7）。
  - `Merge20LearnIsEnabled()` — `OPT_MERGE20_LEARNING_ENABLED` の初の読み手
    （S8 で追加済みだが読み手がなかった。L2 の記録関数・候補計算関数の先頭で
    使うガードの土台）。
  - `Merge20LearnGetOwnerId()` — `OPT_MERGE20_LEARNING_OWNER` から読む。
    空の場合は `CoCreateGuid` で `u-<GUID>` を生成し、キーに保存して返す。
    `self` という出典 ID は使わない（HTML が `self` を特別扱いするため）。
  - `Merge20LearnGetOwnerDisplayName()` — 「このPCで学習（自分）」
    （`_()` 経由で日本語訳つき）。
- `Src/OptionsDef.h`／`Src/OptionsInit.cpp` — 新規キー
  `OPT_MERGE20_LEARNING_OWNER`（`"Merge20/LearningOwner"`、既定は空文字）。
  `OPT_MERGE20_LEARNING_ENABLED` は S8 で追加済み（既定 true）のため変更なし。
- `Src/PropCompare.h`／`.cpp` — メンバ `m_bMerge20Learning`（既定 true）を追加し、
  `BindOption(OPT_MERGE20_LEARNING_ENABLED, m_bMerge20Learning, IDC_MERGE20_LEARN_CHECK, DDX_Check)`
  を 1 行追加。比較オプション画面に学習機能の ON/OFF トグルができた。
- `Src/Merge.rc` — `IDD_PROPPAGE_COMPARE` にチェックボックス
  「Enable learning」（y=234）と説明文 LTEXT
  「When off, learning suggestions and recording stop.」（y=246）を追加。
  ダイアログ下部（「Defaults」ボタンの上）に余白があった（T-3-6 は解消。
  既存コントロールと重ならない）。
- `Src/resource.h` — `IDC_MERGE20_LEARN_CHECK`＝1731（全 define を値でソートして
  空きを確認。`_APS_NEXT_CONTROL_VALUE` は 1731→1732 に更新）。
  `IDD_MERGE20_LEARN`＝296 は使っていない（L4 用に予約のまま）。
- `Src/Merge.vcxproj`／`.filters` — `Merge20LearnDoc.h/.cpp` を登録
  （`.cpp` は StdAfx.h グループの既定エントリ。`Merge20SessionDoc.cpp` と同じ）。
  `UnitTests.vcxproj` には登録しない（MFC アダプタのため。S8 の
  `Merge20SessionDoc.cpp` と同じ扱い）。
- `Translations/WinMerge/Japanese.po` — 5 件追加（「Enable learning」→
  「🎓 学習機能」、説明文→「OFF にすると候補提示と記録を停止します。」、
  「Learning on this PC (self)」→「このPCで学習（自分）」、
  学習フォルダ作成失敗・学習データ保存失敗の文言）。msgid の重複なし。

### 16.2 設計書・仕様書との食い違い（判断理由つき）

1. **L1 だけでは `Merge20Learn.json` が生成されない**（設計書 §5 L1 の受け入れ基準
   「ファイルの生成までを確認する」との食い違い。判断理由：L1 の範囲は永続化の
   土台（保存・読み込み関数と ON/OFF）までであり、保存を呼ぶ記録口は L2 で
   初めてできる。L1 では `Merge20LearnGetStore()`／`Merge20LearnSaveStore()`／
   `Merge20LearnGetOwnerId()` の呼び出し元がまだ存在しないため、実機で
   ファイルが作られることはない。ファイル生成の実機確認は L2 に回す）。
2. **OFF 切替時の候補クリアは L1 では実施しない**（設計書 §3.5 の「OFF への切替時に
   全マージ文書で候補マップを空にして結果ビューを `Invalidate` する」との食い違い。
   判断理由：候補マップ自体が L2 で初めて作られるため、L1 には消す対象がない。
   トグルによるキーの切り替えまでが L1 の範囲。候補クリアは L2／L3 で実装する）。
3. **T-3-4 は「前例なし・新規利用」で解消**：`CoCreateGuid` の利用前例は、
   GitHub code search では 0 件だった（ただし `incomplete_results: true` のため
   全リポジトリ grep ではない。取得済みの主要ファイル内には利用なし）。標準 Win32 API
   の新規利用とし、失敗時のフォールバック（`u-local-YYYYMMDDHHMMSS`）も用意した。
   新規依存はない（ole32 は MFC アプリが既にリンクしている）。
4. **OPT_MERGE20_LEARNING_ENABLED に読み手がないことを再確認**：設計書 §3.5 の確認
   範囲どおり、取得済みファイル内では `OptionsInit.cpp` の `InitOption` と
   `OptionsDef.h` の定義のみ。GitHub code search でも 0 件（ただし上記と同じく
   `incomplete_results: true`）。L1 で初の読み手 `Merge20LearnIsEnabled()` を追加した。
5. **説明文はチェックボックスとは別の LTEXT 1 行にした**（設計書 §6 の「OFF にすると
   候補提示と記録を停止します」を表示するため。チェックボックスのラベル自体を長く
   する代わりに、説明文を下に置いた。ダイアログ下部に余白があったため重ならない）。
6. **保存時の `sourceName` は固定ファイル名 `Merge20Learn.json`**（設計書 §4.2 の
   「出力ファイル名」を内部ファイルに適用。L5 のエクスポート時は日付つきファイル名
   を使う）。`exportedAt` はローカル時刻の `%Y-%m-%dT%H:%M:%S`（`Z` なし。UTC への
   変換はしていない。表示・監査用であり、不一致でも拒否されない）。
7. **`.rc` のラベルは英語のまま**（`Merge.rc` は ASCII のため絵文字・日本語は書けない。
   既存の `.rc` 文字列と同じく `_()`／`.po` 経由で日本語化する。「Enable learning」→
   「🎓 学習機能」）。

### 16.3 Windows 実機での確認手順（L1 受け入れテスト）

1. CI 成果物の ZIP を展開し、`WinMergeU.exe` が従来どおり起動すること（回帰確認）。
2. ツール → オプション → 比較ページを開き、「🎓 学習機能」チェックボックスが
   表示されること（既定で ON）。下に「OFF にすると候補提示と記録を停止します。」
   と表示されること。
3. チェックを外して OK を押し、オプションを閉じて再度開くと OFF が維持されること
   （設定の永続化。レジストリ／INI の `Merge20/LearningEnabled` が false になること）。
   ON に戻すと true に戻ること。
4. **L1 だけでは `Merge20Learn.json` はまだ生成されないこと**（§16.2-1 のとおり。
   これは仕様どおりであり、不具合ではない。ファイル生成は L2 で確認する）。
5. ON/OFF を切り替えても既存の比較・セッション保存・復元が壊れないこと
   （L1 は既存の動作に手を入れていない。オプションの読み書きと表示だけの変更）。

### 16.4 CI 結果（PR #13）

- PR: https://github.com/askhim659/winmerge/pull/13（`merge20/phase3-l1` → `master`、12 コミット、未マージ）
- 実行 45666435275（2026-11-25 17:35Z）：**success**。ジョブ build 139389714022。
  - 成果物：`WinMerge-x64.zip` 26,134,723 バイト（ID 15049525501）、
    `WinMerge-x64-Setup-x64.exe` 39,637,392 バイト（ID 15047912693）。
    いずれも失効日 2027-02-23。
  - GoogleTest：597 テスト全パス（L0 と同じ件数。新規テストなし。
    Merge20Learn 16 件すべて OK）。ログ中の "FAILED" 4 件は翻訳文字列 ID
    （IDS_FILESAVE_FAILED 等）で、テスト失敗ではない。
  - CI 失敗 0 回（初回実行で success）。

## 17. Phase 3 L2（学習の記録と候補計算）— ブランチ `merge20/phase3-l2`

設計書 `merge20-phase3-design.md` §5 L2 どおり。候補提示 UI（L3）・学習モーダル（L4）・
JSON 入出力 UI（L5）は含まない。自動適用の経路は作らない（仕様書 §4(c)）。

### 17.1 変更一覧

- `Src/MergeDoc.h` — 候補マップ `m_merge20LearnCandidates`
  （`std::map<int, Merge20LearnCandidate>`、キー＝差分インデックス）と却下集合
  `m_merge20LearnDismissed`（`std::set<String>`）を `CMergeDoc` に追加。
  `m_resultSegments` とは別のメンバ。公開アクセサ
  `GetMerge20LearnCandidates()`（const／非 const）と
  `GetMerge20LearnDismissed()` を追加。`#include <set>` と
  `#include "Merge20/Merge20Learn.h"` を追加（`Merge20Learn.h` は
  `Merge20Classify.h`／`Merge20Session.h` しか引かないため、既存の
  インクルード順に影響しない）。
- `Src/Merge20/Merge20LearnDoc.h/.cpp` — L2 アダプタを実装（MFC アダプタ、
  StdAfx.h グループの既定エントリのまま。新規ファイルなし）。
  - `Merge20LearnLinesForDiff(doc, nDiff)` — 1 差分（1 `DIFFRANGE`）分の
    差分行を構築。S4 の `Merge20ClassifyDoc` と同じゴースト安全な行取得
    （`ComputeApparentLine` 経由）とペアリング（`OP_DIFF` は両側を順に
    対にし、長い側の余りは片側空きの `OP_DIFF` 行として残す）。`OP_NONE`
    行は含めない（HTML の `hunkKeyExact`／`hunkFeatKey` は差分行だけで作る
    ため）。`OP_TRIVIAL` 行も含めない（無視差分には学習キーの材料になる
    行テキストがなく、記録口は有意な差分にしか走らない）。
  - `Merge20LearnRecordDiffJudgment(doc, nDiff)` — 1 差分の現在の判定を
    L0 の `Merge20LearnRecordJudgment` に記録する（完全一致キーと特徴キー
    の両方）。記録条件：学習 ON かつ 2-way かつ結果ペイン構築済みかつ有意な
    差分かつセグメントの状態が `Chosen`。空選択で未解決に戻った差分や
    `Auto` の差分は記録しない。その差分に表示されていた候補（あれば）を
    `prevLearned` として渡すため、候補と違う方針を選ぶと旧方針 -1・新方針
    +1 の訂正になる（HTML の `learnFromHunk` と同一）。`ex` はフォルダ
    比較時は相対パス（セッションと同じ相対パス）、単独のファイル対では
    ファイル名。`last` は当日日付。ストアはメモリ内だけを変更し、保存は
    呼び出し元が 1 回だけ行う。
  - `Merge20LearnRefreshCandidates(doc)` — 候補を再計算して候補マップに
    だけ入れる。対象は 2-way・結果ペイン構築済み・セグメントが
    `Unresolved`／`Conflict` の差分だけ。却下集合にある差分（同一性で判定）
    には候補を付けない。OFF のときはマップをクリアして何も計算しない。
    マップが変わったときだけ結果ビューを `Invalidate` する。
    `Merge20LearnCandidate` に `operator==` がないため、マップの変化は
    項目ごとに比較している（`strategy`／`via`／`n`／`srcPanes`）。
  - `Merge20LearnRefreshAllOpenDocs()` — 開いている全テキストマージ文書の
    候補を再計算する（`CMainFrame::GetAllMergeDocuments()`＋
    `dynamic_cast<CMergeDoc*>`。バイナリ／画像／Web ページ文書は
    `CMergeDoc` ではないため対象外）。設計書 §3.2 (c) の入口。
  - 方針値と `srcPanes` の対応は設計書 §2.2 どおり（`INSERT_OLD`／
    `REPLACE_WITH_OLD`→`{0}`、`KEEP_NEW`→`{1}`、`BOTH_OLD_NEW`→`{0,1}`）。
    「不採用」はゴースト行が 0 行になる送信元を選ぶことで `<No Lines>`
    （保存時に除外される）として表現する（実機確認 T-3-8 は L2／L3／L6
    の実機確認項目として残る）。
- `Src/MergeDocResultPane.cpp` — 3 つのフック。
  - 記録口は `OnMergeChooseSource` だけに限定（設計書 §3.1 どおり）。
    トグル後の各差分について `Merge20LearnRecordDiffJudgment` を呼び、
    1 件でも記録されたら `Merge20LearnSaveStore` で `Merge20Learn.json`
    に保存する（保存失敗時はメッセージボックスで明示。文言は L1 で追加済みの
    ものを使うため新規の日本語文字列なし）。その後
    `Merge20LearnRefreshAllOpenDocs()` で全文書の候補を再計算する
    （判定済みの差分の候補が消え、新しいルールで他の差分に候補が付く）。
    `ResultChooseSources` には記録を置かないため、一括承認（S5）・
    自動マージ・セッション復元（S8）が学習に混ざることは構造的にない。
  - `ResultChooseSources` 末尾：解決済み（`Chosen`／`Auto`）になった差分の
    候補を消す。空選択で未解決に戻った差分の候補は残す。
  - `StartMergeSession` 末尾（`Merge20SessionRestoreMergeDoc` の直後）：
    候補を計算する。自動マージとセッション復元で解決済みになった差分には
    候補が付かない。
- `Src/MergeDoc.cpp` — `Rescan` 末尾に候補計算フック（成功・非バイナリのとき。
  結果ペイン未構築時は `Merge20LearnRefreshCandidates` 内でスキップされ、
  `StartMergeSession` で計算される。フォルダ文書がなくても動くため、S4 の
  分類フックと違い `m_pDirDoc != nullptr` は条件にしない）。
- `Src/MainFrm.cpp` — `CMainFrame::OnOptions`（IDOK。オプション確定時）に
  `Merge20LearnRefreshAllOpenDocs()` を追加。OFF 切替で全候補マップが
  クリアされ、ON 切替で再計算される（設計書 §3.5）。

### 17.2 設計書・仕様書との食い違い（判断理由つき）

1. **記録の逆写像の明確化**（設計書 §2.2 の表の曖昧さの解消）。表では
   `INSERT_OLD` と `REPLACE_WITH_OLD` がどちらも `{0}` になるため、
   記録時（`srcPanes`→方針値）には差分の種類で区別する：旧欠落のみの差分
   （`OP_1STONLY`）の `{0}` は `INSERT_OLD`、書換（`OP_DIFF`）・新追加
   （`OP_2NDONLY`）の `{0}` は `REPLACE_WITH_OLD`。HTML の既定値
   （削除のみのハンクは旧挿入）と同じ。候補の提示時（方針値→`srcPanes`）
   はどちらも `{0}` になるため、適用結果は同じ。
2. **OFF 切替時の即時クリアの場所**（設計書 §3.5 の「OFF への切替時に全
   マージ文書で候補マップを空にする」を実装する場所）。比較オプションの
   ページ側（`PropCompare::OnOK`）には置かない。`CPreferencesDlg::OnOK`
   （`Src/Common/PreferencesDlg.cpp:147`）を実読したところ、オプションの
   保存（`SaveOptions()`→各ページの `WriteOptions()`）はページの
   `OnOK` より後（`m_pphost.OnOK()` の後）に走るため、ページ側のフックでは
   新しい設定値を読めない。代わりにオプション確定後の
   `CMainFrame::OnOptions()`（IDOK、`ApplyDiffOptions()` の直後）に置いた。
   ここではオプションは保存済みであり、`Merge20LearnIsEnabled()` が新しい
   値を返す。なお `Rescan`／`StartMergeSession` のフックでも OFF 時は
   マップをクリアするため、オプション画面を開かずに OFF になる経路
   （モーダル内のトグルは L4）があっても、次回の再スキャン／セッション開始
   でクリアされる。
3. **却下の扱いは設計書の決定どおり**（親エージェントへの確認事項として
   挙がっていたが、設計書 §2.3 の決定「却下は学習に記録せず、再スキャン
   後も再表示しない（差分の同一性で抑止する）」で実装した）。却下は
   `m_merge20LearnDismissed` に差分の同一性
   （`op:begin[0]-end[0]:begin[1]-end[1]`）で覚え、再計算時にその差分には
   候補を付けない。却下はセッションに保存しない（メモリ内のみ）。却下 UI
   自体（コンテキストメニュー）は L3 の範囲のため、L2 では却下集合への
   追加経路はまだない（集合は作られ、再計算で読まれるだけ）。
4. **T-3-9 は解消**：`Rescan()` は UI スレッドからのみ呼ばれる。
   `FlushAndRescan` の呼び出し元はすべて `CMergeDoc` のメンバ関数・
   ビュー／コマンドハンドラ経由であり、バックグラウンドのフォルダ比較は
   `CDiffContext` で行われ `CMergeDoc::Rescan` を呼ばない。S4 の分類フック
   と同じ位置で候補計算を行うため、学習ストアへの比較スレッドからの
   アクセスはない。
5. **新規の単体テストなし**（L2 は MFC アダプタのため。S8 の
   `Merge20SessionDoc.cpp` と同じ扱い）。候補計算・記録の中核
   （`Match`／`Record`／閾値・訂正・出典管理）は L0 の 16 テストで検証済み
   であり、CI でも全パスしている。方針値と `srcPanes` の往復写像は
   `Merge20LearnDoc.cpp` の MFC アダプタ内にあり、GoogleTest からは
   呼べないため、実機確認項目とした（§17.3）。
6. **新規 UI 文字列・新規ファイルなし**。保存失敗の文言は L1 で追加済みの
   `Merge20LearnSaveStore` の戻り値を使う。`.vcxproj`／`.filters`／
   `Japanese.po`／`resource.h`／`Merge.rc` の変更なし。
7. **3-way では記録も候補計算もしない**（設計書 §2.2・§3.2 は 2-way のみ。
   R-3-17 と同じ判断）。3-way の `srcPanes`（0/1/2）には方針値の対応表が
   ないため、`Merge20LearnRecordDiffJudgment` と
   `Merge20LearnRefreshCandidates` は `m_nBuffers != 2` のとき何もしない。

### 17.3 Windows 実機での確認手順（L2 受け入れテスト）

L3 の候補提示 UI がまだ無いため、候補の存在は `Merge20Learn.json` の
中身とデバッグ手段で確認する。CI 成果物の ZIP
（`WinMerge-x64.zip`、実行 45674219728）を展開し、ポータブル版として
使う（`WinMergeU.exe` と同じフォルダに `WinMergeU.ini` がある場合は
ポータブル扱いになり、学習ファイルは `<progpath>\Merge20Learn\` にできる）。

1. CI 成果物の ZIP を展開し、`WinMergeU.exe` が従来どおり起動すること
   （回帰確認）。
2. **学習ファイルの生成**：2-way のファイル対を開き、比較を開始
   （マージセッションを開始）する。結果ペインで差分を 1 つ選び、キー
   1（旧）または 2（新）でソースを選択する。`<progpath>\Merge20Learn\`
   （ポータブル時）または `%APPDATA%\WinMerge\Merge20Learn\` に
   `Merge20Learn.json` ができていること。ファイルを開き、記録した差分の
   ルール（`key` が `EXACT|...`、`counts` に選んだ方針値が 1 件）が
   入っていること。日本語（差分テキスト）が文字化けなく読めること
   （UTF-8・BOM なし）。
3. **学習データが増えること**：別の差分でもう 1 件判定すると、`counts`
   の件数が増えること（同じパターンの差分なら同じルールの件数が増え、
   違うパターンなら新しいルールが増える）。同じ差分で違う方針を選び直すと、
   旧方針の件数が -1・新方針が +1 され、`lastStrat` が新しい方針になること
   （訂正。L0 の単体テストでは検証済みだが、実機でも確認する）。
4. **記録されない経路**：自動マージ（`/al` または自動マージボタン）で
   解決された差分、一括承認（S5 未実装のため現状は「すべて選択」に相当する
   操作）で解決された差分、セッション復元で復元された差分については、
   `Merge20Learn.json` の件数が増えないこと。
5. **再スキャン後の候補計算**（L3 の UI がまだ無いため、次の手順で確認する）：
   手順 2〜3 で学習したパターンと同じ差分を含む別のファイル対を開き、
   マージセッションを開始する。候補は画面にはまだ出ないが、デバッガで
   `CMergeDoc::m_merge20LearnCandidates` を見ると、該当する差分インデックス
   に候補（`strategy`／`via`＝`"exact"` または `"feat"`／`n`）が入っていること。
   候補がある差分は未解決のまま残り、保存すると未解決の衝突セクションとして
   書き出されること（候補が結果を変えていないことの確認）。
6. **「不採用」の表現（T-3-8）**：旧欠落のみの差分で新側（キー 2）を選ぶと、
   結果ペインに `<No Lines>` 行が 1 行表示され、保存時にその行が除外される
   （旧行が結果に含まれない）こと。新追加のみの差分で旧側（キー 1）を選んだ
   場合も同じこと。
7. **OFF 時の停止**：ツール → オプション → 比較ページで「🎓 学習機能」の
   チェックを外して OK を押す。その後で差分判定をしても
   `Merge20Learn.json` の件数が増えないこと。ON に戻すと再び記録されること。
   OFF にした直後に開いているマージ文書の候補マップは空になること
   （デバッガで確認。次回の再スキャン／セッション開始でも空のまま）。
8. 3-way のファイル対では、差分判定をしても `Merge20Learn.json` の件数が
   増えないこと（L2 は 2-way のみ）。

### 17.4 CI 結果（PR #14）

- PR: https://github.com/askhim659/winmerge/pull/14（`merge20/phase3-l2` → `master`、6 コミット、未マージ）
- 実行 45674219728（2026-11-25 18:20Z）：**success**。ジョブ build 139417301074。
  - 成果物：`WinMerge-x64.zip` 26,158,937 バイト（ID 15052281110）、
    `WinMerge-x64-Setup-x64.exe` 39,659,160 バイト（ID 15051991403）。
    いずれも失効日 2027-02-23。
  - GoogleTest：597 テスト全パス。Merge20Learn 新規 16 件すべて OK。
    ログ中の "FAILED" は翻訳文字列 ID（`IDS_FILESAVE_FAILED` 等）で、
    テスト失敗ではない。
  - CI 失敗 0 回（初回実行で success）。
  - `Merge20LearnDoc.cpp` が WinMergeU のビルド対象に含まれ、コンパイル
    されていることをログで確認（`Merge20LearnDoc.obj` を生成）。

## 18. Phase 3 L3（学習の候補提示 UI）— ブランチ `merge20/phase3-l3`

設計書 `merge20-phase3-design.md` §5 L3 どおり。候補提示 UI（結果ペイン）と
コンテキストメニュー（受け入れ／却下）。学習モーダル（L4）・JSON 入出力 UI（L5）は
含まない。自動適用の経路は作らない（仕様書 §4(c)）。

### 18.1 変更一覧

- `Src/resource.h` — 学習候補コマンド ID を追加：`ID_MERGE20_LEARN_APPLY`＝38939、
  `ID_MERGE20_LEARN_DISMISS`＝38940。全 define を値でソートして空きを確認
  （S3 の教訓）。38938 は L4 の `ID_MERGE20_LEARN`（設計書 §3.3）用に予約の
  まま空けてあり、`_APS_NEXT_COMMAND_VALUE` は 38938→38941 に更新。
  `IDD_MERGE20_LEARN`＝296 も L4 用に予約のまま（L3 では使わない）。
- `Src/Merge.rc` — `IDR_POPUP_MERGERESULTVIEW_2WAY` に 2 項目を追加
  （「Choose Right for All Unresolved Conflicts」の直後）：
  `MENUITEM "Apply Learning Suggestion", ID_MERGE20_LEARN_APPLY`、
  `MENUITEM "Dismiss Learning Suggestion", ID_MERGE20_LEARN_DISMISS`。
  3-way メニューには追加しない（候補は L2 の 2-way のみ）。
  `.rc` のラベルは ASCII のため英語のままにし、`.po` 経由で日本語化する
  （L1 の教訓）。
- `Src/MergeDoc.h` — ハンドラ宣言 4 件（`OnMerge20LearnApply`／
  `OnUpdateMerge20LearnApply`／`OnMerge20LearnDismiss`／
  `OnUpdateMerge20LearnDismiss`）、却下集合の書き込み用アクセサ
  `GetMerge20LearnDismissed()`（非 const。const 版は L2 のまま）、
  `GetMerge20LearnTargetDiffs(int& firstDiff, int& lastDiff)`
  （対象範囲の決定。`OnMergeChooseSource` と同じ規則：選択がなければ現在の
  差分、選択があれば選択範囲の差分）。
- `Src/MergeDoc.cpp` — メッセージマップに 2 コマンドを登録
  （`ON_COMMAND`／`ON_UPDATE_COMMAND_UI`）。
- `Src/Merge20/Merge20LearnDoc.h/.cpp` — L3 ヘルパー 3 件（新規ファイルなし。
  StdAfx.h グループの既定エントリのまま）。
  - `Merge20LearnDismissCandidate(doc, nDiff)` — 却下の本体。差分の同一性
    （`DismissedKey` と同じ `op:begin0-end0:begin1-end1`）を却下集合に追加し、
    候補マップから候補を消す。学習データには何も記録しない（却下は「判定」
    ではないため）。ビューへの `Invalidate` は呼び出し元が行う。
  - `Merge20LearnStrategyDisplayName(strategy)` — 方針値の表示語彙
    （HTML 語彙：旧挿入／新維持／旧置換／両方。`_()` 経由）。
  - `Merge20LearnViaDisplayName(via)` — 一致種別の表示語彙
    （`via` は `"exact"`／`"feat"`。完全一致／特徴一致。`_()` 経由）。
- `Src/MergeDocResultPane.cpp` — 候補提示の本体。
  - `GetResultLineMarker`：`Unresolved`／`Conflict` のセグメントで、その差分に
    候補がある場合は `'?'` の代わりに `'L'` を返す。`L` は既存マーカー
    （`1`／`2`／`3`、`?`、`m`）と重ならない未使用文字。描画色は既存の
    `DrawMargin` の灰色のまま（候補専用の色分けは後回し。設計書 §2.3 どおり）。
  - `OnMerge20LearnApply`：対象範囲のうち候補がある差分だけ、その候補の
    `srcPanes` を `ResultChooseSources` で適用する（`ResultChooseSources`
    の L2 フックが候補を消し、セグメントを `Chosen` にする）。
    適用した差分だけを `Merge20LearnRecordDiffJudgment` で記録し、1 件でも
    記録されたら `Merge20LearnSaveStore` で保存（保存失敗時は
    メッセージボックスで明示）し、`Merge20LearnRefreshAllOpenDocs()` で
    全文書の候補を再計算する。確認内容（レビュー状態）は変えない。
  - `OnMerge20LearnDismiss`：対象範囲のうち候補がある差分だけ
    `Merge20LearnDismissCandidate` で却下し、変わった場合に結果ビューを
    `Invalidate` する。差分は `Unresolved` のまま残り、確認内容も変わらない。
  - 両コマンドの `OnUpdate*` は共通ヘルパー `UpdateMerge20LearnCommandUI`
    を使う：対象範囲に候補が 1 件でもある場合のみ有効。チェック表示なし。
- `Src/MergeResultView.cpp` — 表示の本体。
  - `GetLineColors`：候補がある差分の行の背景を紫色 `RGB(232, 213, 249)`
    にする（設計書 §2.3 の第一候補の色）。現在の差分（選択中）は選択色を
    優先する（既存の `bCurrent` 分岐と同じ扱い）。文字色は変えない。
    色は固定色でテーマ対応は後回し（S2 の行色と同じ判断）。
  - `OnContextMenu`：対象範囲に候補がちょうど 1 件だけある場合、
    適用メニュー項目の文言を `ModifyMenu` で書き換える
    （例：「🎓 学習候補を適用（旧・完全一致・2件）」）。
    候補が複数件・0 件の場合は `.po` 経由の汎用文言
    （「🎓 学習候補を適用」）のまま。却下項目の文言は書き換えない。
    `Merge20/Merge20LearnDoc.h` を include 追加。
- `Translations/WinMerge/Japanese.po` — 9 件追加（msgid 重複なしを確認。
  既存の重複 msgid（`''`／`Binary`／`Diff`／`Disabled`／`Folder`／`None`）
  は L3 以前からあるもので、L3 では増えていない）。

### 18.2 設計書・仕様書との食い違い（判断理由つき）

1. **T-3-1 は解消（静的表示へのフォールバックを採用）**。点滅表示の実現方法を
   実読で確認した結果、`CMergeResultView` にはタイマの前例がない
   （`MergeResultView.cpp`／`.h`、`GhostTextView.h` に `ON_WM_TIMER`／
   `SetTimer` はなく、コードベースで確認できたタイマ前例は
   `CDirView::TIMER_ID_STATUSBAR_UPDATE` のみ）。さらにビュー階層には
   行単位の `Invalidate` の仕組みがなく、再描画の経路はビュー全体の
   `Invalidate()` だけである。900ms タイマを入れると、編集中の結果ペイン
   全体（構文ハイライトを含む）を 900ms ごとに再描画することになり、
   キャレットのちらつきと描画コストの懸念がある。設計書 §2.3 は
   「点滅が難しい場合は静的な紫色＋`L`マーカーに落とす。点滅の有無は
   受け入れ条件 A-3-4 の判定には影響しない（必須なのは『候補が出ている
   ことが一目で分かる』こと）」と明記しているため、L3 は静的表示
   （紫色＋`L`マーカー＋メニューの候補文言）とした。点滅は実装していない。
2. **仕様書 §4(d) の「点滅させ」との齟齬（仕様書改訂候補 M-3-1 の拡張）**。
   仕様書 §4(d) は「学習によって候補提示された行には『学習機能マージ』の
   ポップ表示を点滅させ、自動で決まったことが一目で分かるようにする」と
   書く。フォーク版は (a) 点滅しない（上記 1）、(b) 「学習機能マージ」という
   文言を使わない（設計書 §2.3／M-3-1：「自動判定」を「候補提示」と読み替え、
   自動で決まったようには見せないため。文言は「🎓 学習候補（完全一致・N件）」
   ／「🎓 学習候補（特徴一致・N件）」）。仕様書の改訂候補 M-3-1 に追加する：
   点滅の要求を「候補が出ていることが一目で分かる表示（紫色＋`L`マーカー。
   点滅は任意）」に改めること。
3. **受け入れの呼び出し経路と記録の関係**（設計書 §2.3 の第 2 の選択肢を
   採用）。受け入れハンドラは `OnMergeChooseSource` を経由しない。
   あのハンドラは送信元をトグルするもので、現在の差分／選択範囲に対して
   しか働かず、「この差分の候補の方針値をそのまま適用する」
   （例：`{0,1}` の両方）を表現できないためである。代わりに設計書 §2.3 の
   指示どおり、受け入れハンドラから `ResultChooseSources` を直接呼び、
   その後に `OnMergeChooseSource` と同じ記録関数
   `Merge20LearnRecordDiffJudgment` を呼ぶ。記録は `ResultChooseSources`
   には置いていないため、一括承認（S5）・自動マージ・セッション復元（S8）
   が学習に混ざることは構造的にない（L2 の保証は維持される）。
   `ResultChooseSources` が候補を先に消す（L2 フック）ため、記録時の
   `prevLearned` はなく、受け入れは +1 だけが記録される。後で違う方針を
   選ぶと、同じ記録関数が旧方針 -1・新方針 +1 の訂正を行う。
4. **二重計上の防止（受け入れ時の記録対象の限定）**。受け入れの記録ループは
   対象範囲全体ではなく、このコマンドで実際に適用した差分だけを記録する。
   選択範囲内に、以前の操作で既に `Chosen` になっている差分（候補なし）
   が含まれていた場合、その差分を再記録すると件数が二重に増えるためである。
   `OnMergeChooseSource` の記録ループ（範囲全体）は、トグルですべての差分に
   新しい判定が付くため正しいが、受け入れでは適用した差分だけが新しい判定
   である。この違いをコードのコメントにも明記した。
5. **却下集合への追加経路（L2 で未実装だった部分）**。L2 で作られた
   `m_merge20LearnDismissed` には書き込み口がなかったが、L3 で
   `Merge20LearnDismissCandidate` が初めての追加経路になった。同一性の
   形式（`op:begin0-end0:begin1-end1`）は `DismissedKey` のある
   `Merge20LearnDoc.cpp` 内に留め、`CMergeDoc` 側には重複して持たせていない。
   却下はメモリ内のみ（セッションにも学習ファイルにも保存しない）。
   却下は学習に記録しない（設計書 §2.3 どおり。将来の「却下を負の
   フィードバックとして記録する」は Phase 4 以降の検討）。
6. **候補の件数表示場所（T-3-5）**。結果ペイン内に件数を常時表示する場所は
   作らなかった（設計書 §1.5 T-3-5 どおり）。件数（`n`）と一致種別は、
   候補が 1 件のときの適用メニュー項目の文言
   （「🎓 学習候補を適用（旧・完全一致・2件）」）でのみ示す。
   ステータスバーへの件数表示は見送り。
7. **コマンド ID の予約**。38938 は L4 の `ID_MERGE20_LEARN` 用に空けたまま
   にしたため、L3 は 38939／38940 を使った。L4 着手時は 38938 を
   `ID_MERGE20_LEARN` に割り当てること（`_APS_NEXT_COMMAND_VALUE` は
   38941 になっている）。
8. **新規ファイル・新規単体テストなし**。L3 はビュー／文書の UI であり、
   候補計算・記録の中核は L0 の 16 テストで検証済みのため、`.vcxproj`／
   `.filters` の変更もない。`Merge20LearnDoc.cpp` が WinMergeU のビルド対象
   としてコンパイルされていることは CI ログで確認する。

### 18.3 Windows 実機での確認手順（L3 受け入れテスト）

CI 成果物の ZIP（`WinMerge-x64.zip`）を展開し、ポータブル版として使う。

1. 2-way のファイル対を開き、L2 で学習したパターンと同じ差分を含むファイル
   を開いてマージセッションを開始する。候補がある差分の行が結果ペインで
   紫色になり、マージンに `?` ではなく `L` が出ること。**行は点滅しない**
   （静的表示が L3 の仕様。§18.2-1）。
2. その差分で結果ペインを右クリックする。「🎓 学習候補を適用
   （旧・完全一致・2件）」（方針値／一致種別／件数は実際の候補に従う）と
   「候補を却下」が有効であること。候補がない差分では両項目とも無効
   （グレー）であること。
3. 「学習候補を適用」を押す：その差分だけが解決し、紫色と `L` が消えること。
   ファイルの確認内容が TODO のままであること。`Merge20Learn.json` で
   適用した方針値の件数が +1 されていること。
4. 「候補を却下」を押す：紫色と `L` が消え、差分は未解決のまま残り、確認内容
   も変わらないこと。再スキャン後も候補が再表示されないこと。
   `Merge20Learn.json` の件数が変わらないこと。
5. 候補が出ている状態で結果を保存する：その差分は未解決のコンフリクト
   セクションとして書き出されること（候補提示だけでは出力が変わらないこと）。
6. **T-3-8（「不採用」の表現）**：旧欠落のみの差分で `KEEP_NEW` の候補を
   受け入れると、結果ペインに `<No Lines>` 行が 1 行表示され、保存時に
   その行が除外される（旧行が結果に含まれない）こと。新追加のみの差分で
   `REPLACE_WITH_OLD` の候補を受け入れた場合も同じこと。発火しない場合は
   「不採用」の表現方法を設計し直す（設計書 §2.2 の注記）。
7. 複数の候補がある差分を選択した状態で右クリックする：適用項目は汎用文言
   「🎓 学習候補を適用」になること。各差分にはそれぞれ自分の候補の方針値が
   適用されること。
8. 3-way のファイル対では、結果ペインのコンテキストメニューに学習候補の
   項目が出ないこと。

### 18.4 CI 結果（PR #15）
- PR: https://github.com/askhim659/winmerge/pull/15（`merge20/phase3-l3` → `master`、9 コミット、未マージ）
- 実行 45680942184（2026-11-25 19:00Z）：**success**。ジョブ build 139440983781。
  - 成果物：`WinMerge-x64.zip` 26,160,731 バイト（ID 15054033790）、
    `WinMerge-x64-Setup-x64.exe` 39,658,844 バイト（ID 15054288536）。
    いずれも失効日 2027-02-23。
  - GoogleTest：597 テスト全パス。Merge20Learn 16 件すべて OK。
    ログ中の `[  FAILED  ]` はなし。`echo ERROR: Failed to download ...`
    の行は DownloadDeps.cmd 内のエラーメッセージの echo 文そのもので、
    実際のダウンロード失敗ではない（ビルドは完了し成果物もある）。
  - CI 失敗 0 回（初回実行で success）。
  - `MergeDocResultPane.obj`／`MergeResultView.obj`／`Merge20LearnDoc.obj`
    の生成をログで確認（L3 の変更ファイルがコンパイルされていること）。

## 19. Phase 3 L4（学習モーダル）— ブランチ `merge20/phase3-l4`

設計書 `merge20-phase3-design.md` §5 L4・§3.3・§6 どおり。学習モーダル
（統計・ルール一覧・出典一覧・出典削除・ON/OFF 切替）とフォルダ比較の
右クリックメニュー「学習…」。JSON 入出力 UI（L5）は含まない。自動適用の
経路は作らない（仕様書 §4(c)）。

### 19.1 変更一覧

- `Src/resource.h` — `IDD_MERGE20_LEARN`＝296、`IDC_MERGE20_LEARN_DESC`〜
  `IDC_MERGE20_LEARN_NOTICE`＝1732〜1744、`ID_MERGE20_LEARN`＝38938
  （L3 で予約済みの 296／38938 をそのまま使う。`_APS_NEXT_COMMAND_VALUE`
  は 38941 のまま）。全 define を値でソートして空きを確認（S3 の教訓）。
  `_APS_NEXT_RESOURCE_VALUE` 296→297、`_APS_NEXT_CONTROL_VALUE` 1732→1745。
- `Src/Merge.rc` — ダイアログテンプレート `IDD_MERGE20_LEARN`
  （480×314）＋ `AFX_DIALOG_LAYOUT` エントリ。`.rc` のラベルは ASCII の
  英語のまま、`.po` 経由で日本語化する（L1 の教訓）。レイアウト：説明文、
  状態表示＋ON/OFF 切替ボタン、統計行、読み込み失敗行、出典グループ
  （リスト＋「選択したソースを削除」＋「統合して出力」）、ルールグループ
  （リスト、上位 50 件）、共有注意文、下段ボタン（出力／取込／クリア／
  閉じる／キャンセル）。
- `Src/Merge20/Merge20Learn.h/.cpp` — `Merge20LearnDominant`
  （HTML `learnDominant`）を公開関数化。L0 の静的 `DominantRule` の本体を
  そのまま使い、`Merge20LearnStore::Match` の 2 箇所の呼び出しもこちらに
  寄せた。モーダルのルール一覧と候補計算が同じ最多方針・件数・合計を
  使うため、表示と候補が食い違うことがない。
- `Src/Merge20/Merge20LearnDoc.h/.cpp` —
  `Merge20LearnCountOpenCandidates()` を追加。開いている全テキスト
  マージ文書の候補マップの件数の合計（`CMainFrame::GetAllMergeDocuments()`
  ＋ `dynamic_cast<CMergeDoc*>`、L2 の `Merge20LearnRefreshAllOpenDocs`
  と同じ入口）。モーダルの「候補提示中 K 件」用。
- `Src/Merge20/Merge20LearnDlg.h/.cpp`（新規。`CTrDialog` 派生、
  StdAfx.h グループの既定エントリ）— モーダルの本体。
  - 統計：パターン N 件／判定記録 M 件／出典 N 件／候補提示中 K 件
    （`RuleCount()`／`TotalJudgments()`／`GetSources().size()`／
    `Merge20LearnCountOpenCandidates()`）。
  - ルール一覧：判定件数の多い順に上位 50 件。`[完全一致／特徴] キー ＋
    最多方針（N 回／NN%）`。一致率は実行時に整数比で計算する
    （`(best*100 + total/2)/total`。HTML の `Math.round(share*100)` と
    同じ丸め。ファイルに率は保存しない）。完全一致キーは HTML と同じく
    `key[6..90)`（先頭 84 文字）まで表示し、特徴キーは全文表示する。
  - 出典一覧：`GetSources()`（判定件数の多い順）とルール／判定件数。
    選択した出典の削除ボタンは、確認ダイアログの後に L0 の
    `RemoveSource` でその出典の寄与だけを差し引き、即保存する
    （`Merge20LearnSaveStore`。保存失敗時はメッセージボックスで明示）。
    削除後は全文書の候補を再計算する（削除で消えたルールに当たっていた
    候補が残らないようにする）。
  - ON/OFF 切替：`OPT_MERGE20_LEARNING_ENABLED` を `SaveOption` で
    切り替え、表示を更新し、`Merge20LearnRefreshAllOpenDocs()` で全文書
    の候補を再計算する（OFF でクリア、ON で再計算。L2 の仕組みを流用）。
    比較オプション側のトグルと同じキーを読む（L1）。
  - 読み込み失敗：`Merge20LearnGetLoadError` が失敗を返したときだけ
    失敗行を表示する（L1 で「起動を妨げず、モーダルで報告」と決めたもの）。
  - エクスポート／インポート／統合して出力／クリアの 4 ボタンは配置するが
    `OnInitDialog` で無効化しておく（L5 で配線する。§19.2-1）。
- `Src/DirView.h/.cpp` — フォルダ比較の右クリックメニューに「学習…」
  （`ID_MERGE20_LEARN`）を追加。追加位置は Merge20 群の末尾
  （「監査メモを編集…」の後、ディレクトリ単位チェック項目の前）。
  常時表示。ハンドラ `CDirView::OnMerge20Learn()` はモーダルを開くだけ。
  メインメニューには置かない（S8 と同じ判断。設計書 §3.3 どおり）。
- `Src/Merge.vcxproj`／`Src/Merge.vcxproj.filters` — 新規ダイアログを登録
  （`.cpp` は StdAfx.h グループの既定エントリ。S3／S4 の教訓）。
  `UnitTests.vcxproj` には登録しない（MFC ダイアログのため。S3 の
  BatchCheckDlg と同じ扱い）。
- `Translations/WinMerge/Japanese.po` — 22 件追加（msgid 重複なしを確認。
  既存の重複 msgid は L3 以前からのもので、L4 では増えていない）。
  `.rc` のラベル・`_()` 文字列はすべて検証済み（コード側の全文字列が
  `.po` の msgid と一致すること）。

### 19.2 設計書・仕様書との食い違い（判断理由つき）

1. **エクスポート／インポート／統合して出力／クリアのボタンは配置のみで
   無効化した（L5 の範囲）**。設計書 §5 は L4 のモーダル内容にボタン群を
   列挙しつつ、L5 を「エクスポート（通常・統合）、インポート、クリアの
   実装」としている。タスク文の切り分け指示どおり、L4 ではボタンを置いて
   無効化し、配線は L5 に回した。無効化の理由はダイアログのコメントにも
   明記した。
2. **出典一覧のチェックボックスは L5 に回し、L4 では単一選択＋
   「選択したソースを削除」とした**。設計書 §3.3 は出典一覧を
   「チェックボックス＋ルール件数・判定件数＋削除ボタン」とするが、
   チェックボックスの用途は統合出力（と一括削除）の選択であり、L5 の
   機能に紐づく。L4 の受け入れ条件は「出典の削除ボタンでその出典の寄与
   だけが減ること」（単一出典の削除）であり、単一選択のリストボックス＋
   削除ボタンで満たせる。チェックボックス付きリストへの置き換えと
   「選択したソースを削除」（複数）・「統合して出力」は L5 で行う。
3. **HTML の統計「今回の解析で自動判定 N 件」は廃止し、「候補提示中 K 件」
   に置き換えた**（設計書 §3.3 どおり）。自動適用がないため HTML の統計は
   意味がなく、代わりに開いている全マージ文書の候補マップの件数の合計を
   表示する。開いている文書がない場合は 0 と表示する。
4. **最多方針の表示は L0 の判定と共用する**（§19.1 の
   `Merge20LearnDominant`）。モーダル側で最多方針を独自に計算すると、
   同数時の `lastStrat` 優先などの食い違いが出るため、L0 の関数を公開して
   候補計算と共用した。`Merge20LearnStore::Match` の挙動は変わらない
   （同じ本体を呼ぶだけ。16 の単体テストは CI で全パス）。
5. **ダイアログタイトルは設計書 §6 の文言**（`🎓 学習機能（マージ判定の
   蓄積と共有）`）。`.rc` の CAPTION は英語
   `Learning (merge judgment history and sharing)` とし、`.po` 経由で
   日本語化する（L1 の教訓）。
6. **点滅表示はしない**（L3 の決定を継承。§18.2-1）。モーダル内に点滅要素
   はなく、候補の表示は結果ペイン側の静的表示（紫色＋`L` マーカー）の
   ままである。仕様書 §4(d) の「点滅させ」との齟齬は §18.2-2 の改訂候補
   M-3-1 の拡張として記録済み。
7. **仕様書 §4(c) の自動適用の禁止を遵守**：モーダルは学習データの表示と
   管理だけを行い、候補をマージ結果に適用する経路は作っていない。出典
   削除・ON/OFF 切替のどちらも `m_resultSegments` には触れない。
   受け入れ・却下の操作はモーダルには置かず、結果ペインで行う（L3）。

### 19.3 Windows 実機での確認手順（L4 受け入れテスト）

CI 成果物の ZIP（`WinMerge-x64.zip`、実行 45689015807）を展開し、
ポータブル版として使う。

1. フォルダ比較を開き、右クリックメニューに「学習…」があること。学習
   データがまだない状態で開くと、統計が「パターン 0件 ／ 判定記録 0件 ／
   出典 0件 ／ 候補提示中 0件」で、ルール一覧・出典一覧に「まだ学習データが
   ありません。」と出ること。
2. **学習データの表示**：L2 の手順で差分判定を数件行って学習データを
   作り、モーダルを開く。統計のパターン／判定記録の件数が
   `Merge20Learn.json` の内容と一致すること。ルール一覧に
   `[完全一致] キー - 方針（N回／NN%）` の行が出ること（方針と件数は
   JSON の最多方針・件数と一致すること。完全一致キーは先頭 84 文字まで
   の表示）。
3. **候補提示中の件数**：L3 の手順で候補が出るファイル対を開き、候補が
   表示された状態でモーダルを開く。「候補提示中 K 件」が開いている文書の
   候補件数（紫色＋`L` の差分の数）と一致すること。マージ文書をすべて
   閉じると 0 に戻ること。
4. **出典の削除**：出典を 1 件選択して「選択したソースを削除」を押す。
   確認ダイアログで「はい」を選ぶと、その出典の寄与分だけ件数が減ること
   （`Merge20Learn.json` で確認）。「いいえ」では何も変わらないこと。
   出典が選択されていないときは削除ボタンが無効（グレー）であること。
   なお、他出典の寄与が残ることの確認には出典が 2 件以上ある状態が
   要るが、2 件目の出典は取込（L5）でしか増えないため、その確認は
   L5 の受け入れテストで行う。L4 の段階では自分の出典 1 件だけを削除
   できること（全データが消えること）を確認する。
5. **ON/OFF トグル**：モーダル内の「ON/OFF切替」を押すと、状態表示が
   ON→OFF に変わり、開いているマージ文書の候補表示（紫色＋`L`）が消える
   こと。もう一度押すと ON に戻り、候補が再表示されること。比較オプション
   （ツール → オプション → 比較）の「🎓 学習機能」チェックボックスと
   連動すること（モーダルで OFF にするとオプション側も OFF になること）。
6. **無効化されたボタン**：「学習データを出力（JSON)」「学習データを取込
   （蓄積型）」「選択したソースを統合して出力」「学習データをクリア」の
   4 ボタンがグレーで押せないこと（L5 で配線する）。
7. **共有注意文**：モーダル下部に「学習データにはソースコードの断片が
   含まれます。共有はチーム内の承認された経路で行ってください。社外への
   持ち出し、個人クラウドへの保存は禁止です。」と表示されること。
8. **回帰確認**：モーダルを開いて閉じるだけで、フォルダ一覧・確認内容・
   出力チェック・`Merge20Learn.json` の内容が変わらないこと。

### 19.4 CI 結果（PR #16）

- PR: https://github.com/askhim659/winmerge/pull/16（`merge20/phase3-l4` → `master`、15 コミット、未マージ）
- 実行 45687078665（2026-11-25 19:37Z）：**failure**。唯一のエラーは
  `Merge.rc(3052): error RC2112: BEGIN expected in dialog`。原因は
  L4 のダイアログテンプレートの挿入位置の誤り：`IDD_MERGE20_MEMO` の
  `DIALOGEX` 行の直後に挿入したため、MEMO ダイアログの本体（STYLE／
  CAPTION／BEGIN）が分断された。修正は 1 コミット（016cc34b）：
  テンプレートを MEMO ダイアログの `END` の後へ移動した。
  **教訓**：`.rc` へのダイアログ追加は、対象ダイアログの `END` の後に入れる
  こと。ヘッダ行の直後ではない。
- 実行 45689015807（2026-11-25 19:49Z）：**success**。ジョブ build 139469257674。
  - 成果物：`WinMerge-x64.zip` 26,181,165 バイト（ID 15058492018）、
    `WinMerge-x64-Setup-x64.exe` 39,684,673 バイト（ID 15058008775）。
    いずれも失効日 2027-02-23。
  - GoogleTest：597 テスト全パス。Merge20Learn 新規 16 件すべて OK。
    ログ中の "FAILED" 4 件は翻訳文字列 ID（`IDS_FILESAVE_FAILED` 等）
    で、テスト失敗ではない。
  - `Merge20LearnDlg.obj`／`Merge20Learn.obj` の生成をログで確認
    （L4 新規ファイルが WinMergeU のビルド対象としてコンパイルされていること）。
  - CI 失敗 1 回（上記の RC2112 のみ）。C++ のコンパイルエラーは 0 件。

## 20. Phase 3 L5（学習 JSON 入出力）— ブランチ `merge20/phase3-l5`

設計書 `merge20-phase3-design.md` §5 L5・§4.2・§4.3 どおり。L4 で配置のみ・
無効化だったモーダルの 4 ボタン（出力／取込／統合して出力／クリア）を配線し、
出典一覧をチェックボックス付きリストにした。自動適用の経路は作らない
（仕様書 §4(c)）。取込んだ学習データは次回の候補計算から使われるだけである。

### 20.1 変更一覧

- `Src/Merge20/Merge20LearnDoc.h/.cpp` — L5 の入出力ヘルパー
  （MFC アダプタ、StdAfx.h グループの既定エントリ。新規ファイルなし）。
  - `Merge20LearnDefaultExportFileName(merged)`：既定の出力ファイル名。
    HTML と同じ `マージ支援20号_学習_YYYY-MM-DD.json`（通常）と
    `マージ支援20号_学習_統合_YYYY-MM-DD.json`（統合出力）。日本語部分は
    `\u` エスケープで書き、ソースは ASCII のまま（S4 の慣習）。
  - `Merge20LearnExportToFile`：学習データ全体を L0 `Merge20LearnToJson`
    で JSON v2 として書き出す。`sourceId` は自分の出典 ID、
    `sourceName` は出力ファイルのベース名。`Merge20Learn.json` は触らない。
  - `Merge20LearnExportMergedToFile`：L0 `Merge20LearnStore::MergeSources`
    で選択した出典を合算し、L0 `Merge20LearnMergedToJson` で書き出す
    （ルールごとの `sources` は落とし、root の `sources` には統合した出典の
    表示名の配列を入れる。HTML の `exportUnifiedLearn` と同一の形）。
    書き出したルール件数を返す。
  - `Merge20LearnImportFromFile`：ファイルを読み、L0
    `Merge20LearnImportJson` で蓄積型に取り込む（再取込は置換、ルールごと
    の `sources` はファイル全体の 1 出典に平坦化。HTML の `importLearn`
    と同一）。v1 ファイルの出典 ID はファイル名のハッシュ
    （`file:<hash>`、L0 の `hashStr` 移植）になるよう、ファイル名には
    ベース名だけを渡す。ストアはメモリ内だけを変更し、保存は呼び出し元
    （ダイアログ）が `Merge20LearnSaveStore` で行う（L4 の削除と同じ形）。
  - 内部保存のリファクタ：`SaveStoreToFile` の一時ファイル＋リネーム部分を
    `WriteTextFileAtomic` として抜き出し、エクスポートと内部保存で共用する
    ようにした（`exportedAt` の生成も `ExportedAtNow()` にまとめた）。
    内部保存の挙動は変わらない（§20.2-3 の文言の変化を除く）。
    `ReadFileToUtf8` は取込用のファイル読み込み（L1 の遅延読み込みと同じ
    読み方）。
- `Src/Merge20/Merge20LearnDlg.h/.cpp` — モーダルの L5 配線。
  - 出典一覧を `CListBox` から `CCheckListBox` に置き換えた。
    HTML と同じく全出典を既定でチェックする。チェックした出典が
    「統合して出力」と「選択したソースを削除」の対象になる。
    チェック変更は `CLBN_CHKCHANGE`（と `LBN_SELCHANGE` の両方）で受け、
    `UpdateSourceButtons()` で削除／統合ボタンの有効・無効を切り替える
    （選択がないときは両方とも無効）。
  - `OnExport`：保存ダイアログ（S6 の `SelectFile`、保存モード、JSON
    フィルタ。既定名は上記の HTML 形式）→ 共有注意文つきの確認ダイアログ
    （仕様書 §4(d)。注意文の msgid はモーダル下部の注意文と共用）→
    `Merge20LearnExportToFile` → 完了メッセージ（ルール件数とパス）。
  - `OnImport`：開くダイアログ（`SelectFile`、開くモード、JSON フィルタ）
    → `Merge20LearnImportFromFile` → 即保存（`Merge20LearnSaveStore`）
    → `Merge20LearnRefreshAllOpenDocs()` で全文書の候補を再計算 →
    結果メッセージ（HTML のトースト文言と同じ「新規N件・合算M件」。
    スキップしたルールがあれば件数を併記する）。ファイル全体が拒否された
    場合（`app`／`version` の不一致等）は理由つきのエラーを出す。
  - `OnMergeSources`：チェックした出典を `MergeSources` で合算し、
    0 件なら情報表示だけでファイルを作らない。保存ダイアログ（既定名は
    統合形式）→ 共有注意文つきの確認 → `Merge20LearnExportMergedToFile`
    → 完了メッセージ（ルール件数・出典数・パス）。
  - `OnClear`：HTML の `clearLearn` と同じ確認ダイアログ（「他の作業者と
    共有したJSONには影響しません」）→ `store.Clear()` → 即保存 →
    候補の再計算 → 完了メッセージ。
  - `OnDeleteSource`：チェックした複数出典に対応させた。1 件のときは
    L4 と同じ出典名つきの確認文言、2 件以上のときは件数つきの確認文言
    （「%1件のソースの学習データを削除しますか？」）。各出典の寄与だけを
    `RemoveSource` で差し引き、1 回だけ即保存し、候補を再計算する。
    削除ボタンはチェックがないとき無効（L4 では選択がないとき無効だった）。
- `Src/Merge.rc` — 出典一覧の `LISTBOX` に `LBS_OWNERDRAWFIXED`／
  `LBS_HASSTRINGS` を追加（`CCheckListBox` の要件）。削除ボタンのラベルを
  「Delete selected source」→「Delete selected sources」に改名
  （複数選択に対応したため。設計書 §6 の文言どおり）。
  ダイアログテンプレートの追加ではなく既存コントロールの変更のため、
  RC2112 の教訓（`END` の後に入れること）は該当しない。
  新規リソース ID なし（L4 配置済みの 1741／1742／1739／1743 をそのまま使う）。
- `Translations/WinMerge/Japanese.po` — 11 件追加（msgid 重複なしを確認。
  既存の重複 msgid は L4 以前からのもので、L5 では増えていない）。
  削除ボタンの msgid を複数形に更新し、4 ボタンのコメントの
  「disabled until L5」を「wired in L5」に更新した。
  コード側の全 `_()` 文字列が `.po` の msgid と一致することを確認済み。
- 新規ファイルなし（`.vcxproj`／`.filters` 変更なし）、新規単体テストなし
  （L5 はダイアログ／アダプタで、中核の JSON 変換・置換・合算・削除は
  L0 の 16 テストで検証済み）。

### 20.2 設計書・仕様書との食い違い（判断理由つき）

1. **既定の保存場所：ファイル名だけを固定し、保存場所は保存ダイアログに
   任せた**。タスク文の「既定の保存場所とファイル名は HTML に倣うこと」
   に対し、HTML の保存場所はブラウザのダウンロードフォルダであり、
   デスクトップアプリに直接の対応物がない。S6 の CSV 出力と同じ形
   （ファイル名だけを既定値として渡し、フォルダは標準の保存ダイアログが
   決める）を採った。ファイル名は HTML と完全一致する。
2. **`exportedAt` はローカル時刻（`Z` なし）**。HTML は UTC の
   `toISOString()`（`Z` 付き）だが、L1 の内部保存がローカル時刻の
   `%Y-%m-%dT%H:%M:%S`（`Z` なし）を使っており、設計書 §4.2 でも
   `exportedAt` は「表示・監査用。不一致でも拒否しない」とされているため、
   L1 と同じ形式に統一した。取込時にこの値が違っても拒否されない。
3. **内部保存の失敗文言が 2 箇所だけ変わった**（`WriteTextFileAtomic` への
   抜き出しの副作用）。「failed to write learning file」→
   「failed to write file」、「cannot create learning file」→
   「cannot create file」。内部保存とエクスポートで共用するため汎用的な
   文言にした。表示は `Merge20LearnSaveStore` の「学習データを保存でき
   ませんでした：」の後に続く技術的な理由であり、ユーザーが見る日本語の
   本体は変わらない。
4. **統合出力の出典表示名の順序はストアの出典順**（`GetSources()` の順、
   判定件数の多い順）。HTML はチェックした順に `sources` を並べるが、
   root の `sources` は表示名の配列で順序に意味はないため、ダイアログの
   表示順と一致するストア順にした。
5. **共有注意文は出力時の確認ダイアログで表示する**（設計書 §5 L5 の
   「共有注意文がエクスポート時に表示されること」）。モーダル下部の注意文
   に加え、保存ダイアログの後に注意文＋出力先パス＋「出力しますか？」の
   確認を出す。統合出力でも同じ確認を出す（統合出力もソースコードの断片を
   含むため）。
6. **取込時の `app`／`version` の検査は L0 の `FromJson` に任せた**。
   L5 で新たに検査はしない。`app` がフォーク（`"Merge20Learn"`）でも
   HTML（`"マージ支援20号"`）でもなく、欠落もしていない場合はファイル全体
   を拒否し、理由（`unsupported app` 等）をエラーメッセージに出す。
   小数を含むファイルも L0 の仕様どおり全体を拒否する（§15.2-1）。
7. **仕様書 §4(c) の自動適用の禁止を遵守**：入出力は `m_resultSegments`
   には一切触れない。取込後は `Merge20LearnRefreshAllOpenDocs()` で候補を
   再計算するだけであり、候補がある差分は未解決のまま残る。取込んだ学習
   データが候補として使われるのは次回の候補計算からである。

### 20.3 Windows 実機での確認手順（L5 受け入れテスト）

CI 成果物の ZIP（`WinMerge-x64.zip`、実行 45696563306）を展開し、
ポータブル版として使う。学習データは `Merge20Learn\Merge20Learn.json`
（ポータブル時はプログラムフォルダ直下の `Merge20Learn\`）にある。

1. **エクスポート**：L2 の手順で差分判定を数件行って学習データを作り、
   モーダルを開いて「学習データを出力（JSON)」を押す。保存ダイアログの
   既定ファイル名が `マージ支援20号_学習_YYYY-MM-DD.json` であること。
   共有注意文つきの確認で「はい」を選ぶとファイルが作られ、完了メッセージ
   にルール件数とパスが出ること。「いいえ」ではファイルが作られないこと。
   出力した JSON をメモ帳で開き、`app` が `"Merge20Learn"`、`version` が
   `2`、`sourceId` が自分の出典 ID（`u-...`）、`sourceName` が出力ファイル名、
   `rules` に `key`／`counts`／`n`／`ex`／`last`／`lastStrat`／`sources`
   があること。
2. **HTML の JSON を取込めること**：HTML（v17.15）で学習データを作って
   出力した JSON（`マージ支援20号_学習_YYYY-MM-DD.json`）を、フォークの
   モーダルで「学習データを取込（蓄積型）」から取り込む。結果メッセージが
   「学習データを取込ました（<出典名>：新規N件・合算M件）」であること。
   モーダルの出典一覧に HTML の出典が増え、統計のパターン／判定記録の件数
   が増えること。取込んだルールに一致する差分を開くと候補（紫色＋`L`）が
   出ること（完全一致のヒット率は T-3-7 のとおり差分エンジンの差に依存する
   ため、出ない場合は特徴一致の候補で確認する）。
3. **再取込は置換（二重計上しない）**：同じ JSON をもう一度取り込む。
   結果メッセージの件数と、モーダルの統計のパターン／判定記録の件数が
   変わらないこと（`Merge20Learn.json` の該当出典の `counts` も変わらない
   こと）。内容を変えた JSON（HTML 側で判定を 1 件追加して再出力）を
   取り込むと、置換されて件数が新しい内容になること。
4. **2 出典での削除（片方の出典を削除すると他方の寄与が残ること）**：
   手順 2 の状態（自分の出典＋HTML の出典の 2 出典）で、出典一覧で HTML
   の出典だけのチェックを外して自分の出典だけを残す、のではなく、削除したい
   出典だけにチェックを入れて「選択したソースを削除」を押す。確認ダイアログ
   で「はい」を選ぶと、その出典の寄与分だけ件数が減り、他方の出典のルールが
   残ること（`Merge20Learn.json` で確認）。2 件以上にチェックを入れたときは
   件数つきの確認（「%1件のソースの学習データを削除しますか？」）が出ること。
   チェックをすべて外すと削除ボタンと統合ボタンがグレーになること。
5. **選択した出典の統合出力（件数が合算され出典情報が落ちていること）**：
   2 出典の状態で両方にチェックを入れて「選択したソースを統合して出力」を
   押す。既定ファイル名が `マージ支援20号_学習_統合_YYYY-MM-DD.json` で
   あること。出力した JSON をメモ帳で開き、ルールごとの `sources` がなく、
   `counts` が 2 出典の合算になっていること。root の `sources` に統合した
   出典の表示名の配列があること。`sourceId` が自分の出典 ID であること。
   この統合 JSON を HTML（v17.15）で取り込めること（統合前の出典別の内訳は
   失われ、全体が 1 出典として扱われること）。
6. **クリア（確認ダイアログと全削除）**：「学習データをクリア」を押すと
   確認ダイアログ（「他の作業者と共有したJSONには影響しません」）が出ること。
   「はい」を選ぶと統計がすべて 0 になり、`Merge20Learn.json` の `rules`
   が空になること。「いいえ」では何も変わらないこと。クリアしても、既に
   他の作業者と共有した JSON ファイル自体は消えないこと。
7. **取込んだ学習データが次回の候補計算から使われること**：手順 2 で取り
   込んだ直後に開いているファイルの候補が再計算され、取込んだルールに一致
   する未解決の差分に候補が出ること。候補が出ている状態で保存しても結果が
   未解決のままであること（自動適用されていないこと）。
8. **回帰確認**：モーダルを開いて 4 ボタンのいずれも押さずに閉じるだけで、
   フォルダ一覧・確認内容・出力チェック・`Merge20Learn.json` の内容が
   変わらないこと。L4 の受け入れテスト（§19.3）の 1〜5・7・8 も変わらず
   通ること。

### 20.4 CI 結果（PR #17）

- PR: https://github.com/askhim659/winmerge/pull/17（`merge20/phase3-l5` → `master`、8 コミット、未マージ）
- 実行 45696563306（コード head a0e37918）：**success**。ジョブ build 139495387180。
  - 成果物：`WinMerge-x64.zip` 26,194,760 バイト（ID 15062082076）、
    `WinMerge-x64-Setup-x64.exe` 39,689,868 バイト（ID 15062067353）。
    いずれも失効日 2027-02-23。
  - GoogleTest：597 テスト全パス（`[  PASSED  ] 597 tests.`）。
    Merge20Learn の 16 件すべて OK。`[  FAILED  ]` は 0 件。
  - `Merge20LearnDlg.obj`／`Merge20LearnDoc.obj`／`Merge20Learn.obj` の
    生成をログで確認（L5 変更ファイルが WinMergeU のビルド対象として
    コンパイルされていること。`CL.exe ... /Yu"stdafx.h" ... Merge20\Merge20LearnDlg.cpp`）。
  - CI 失敗 0 回（初回実行で緑）。

## 21. Phase 3 L6（受け入れテストと文書化）— ブランチ `merge20/phase3-l6`

設計書 `merge20-phase3-design.md` §5 L6・§8・§1.3 どおり。フェーズ 3
（L0〜L5）の受け入れテストの実施と記録、実機確認手順の統合、
`MERGE20_NOTES.md` §21 としてのフェーズ 3 全体のまとめ、§10・
§11 の補完、ローカル `MERGE20_NOTES.md` の同期。**機能コードの
変更はない。** 受け入れテストの実施で不具合は見つからなかったため、
追加したのは単体テスト 3 件だけである（§21.8）。

### 21.1 フェーズ 3 全体の変更一覧（L0〜L6）

| スライス | ブランチ／PR | 変更の要旨 | マージ |
|---|---|---|---|
| L0 | `merge20/phase3-l0`／PR #12 | 新規 `Src/Merge20/Merge20Learn.h/.cpp`（純粋モジュール、pch.h グループ。完全一致キー・特徴キー・閾値判定・訂正・出典管理・JSON v2 変換・`hashStr` 移植）。新規 `Testing/GoogleTest/Merge20/Merge20Learn_test.cpp`（16 テスト）。プロジェクト登録 4 ファイル。自動適用の経路なし。`OPT_MERGE20_LEARNING_ENABLED` の読み手なし | dd810ca |
| L1 | `merge20/phase3-l1`／PR #13 | 新規 `Src/Merge20/Merge20LearnDoc.h/.cpp`（MFC アダプタ。`Merge20Learn.json` の場所・遅延読み込み・即保存・`Merge20LearnIsEnabled()`・`CoCreateGuid` による出典 ID 生成）。新規キー `OPT_MERGE20_LEARNING_OWNER`。`PropCompare` に「🎓 学習機能」チェックボックス（`IDC_MERGE20_LEARN_CHECK`＝1731）。`Japanese.po` 5 件 | d9d7a9a |
| L2 | `merge20/phase3-l2`／PR #14 | `CMergeDoc` に候補マップ `m_merge20LearnCandidates`・却下集合 `m_merge20LearnDismissed` とアクセサを追加。記録口は `OnMergeChooseSource` だけに限定。`ResultChooseSources` 末尾で解決済み差分の候補を消す。`Rescan` 末尾・`StartMergeSession` 末尾（自動マージとセッション復元の直後）に候補計算フック。`CMainFrame::OnOptions`（IDOK）で OFF 切替時に全文書の候補を再計算。2-way のみ | 2877a4d |
| L3 | `merge20/phase3-l3`／PR #15 | 結果ペインの候補表示（紫色 `RGB(232, 213, 249)`＋マージンの `L` マーカー。**点滅なし**、静的表示）。コンテキストメニューに「学習候補を適用」「候補を却下」を追加（`IDR_POPUP_MERGERESULTVIEW_2WAY` のみ。コマンド 38939／38940）。受け入れハンドラは候補の `srcPanes` を `ResultChooseSources` で適用し、適用した差分だけを記録する。却下ハンドラは却下集合への追加のみ。`OnContextMenu` で候補 1 件のときだけ適用項目の文言を `BCMenu::ModifyMenu` で書き換える。`Japanese.po` 9 件 | d084359 |
| L4 | `merge20/phase3-l4`／PR #16 | 新規 `Src/Merge20/Merge20LearnDlg.h/.cpp`（`CTrDialog`。統計・ルール一覧（上位 50 件）・出典一覧・出典削除・ON/OFF 切替・読み込み失敗表示・共有注意文）。`Merge20LearnDominant` を公開関数化。`Merge20LearnCountOpenCandidates()` を追加。フォルダ比較の右クリックメニューに「学習…」（コマンド 38938、`IDD_MERGE20_LEARN`＝296、IDC 1732〜1744）。エクスポート／インポート／統合／クリアの 4 ボタンは配置のみで無効化（L5 で配線）。`Japanese.po` 22 件 | 9e81714 |
| L5 | `merge20/phase3-l5`／PR #17 | `Merge20LearnDoc` に入出力ヘルパー 4 件（既定ファイル名・エクスポート・統合出力・インポート。内部保存の一時ファイル＋リネーム部分を `WriteTextFileAtomic` として共用化）。モーダルの 4 ボタンを配線し、出典一覧を `CCheckListBox` に置き換え（既定で全チェック、`CLBN_CHKCHANGE`／`LBN_SELCHANGE` でボタン有効・無効を切替）。エクスポート／統合出力時に共有注意文つき確認ダイアログ。`Japanese.po` 11 件 | 635ac88 |
| L6 | `merge20/phase3-l6`／PR #18 | 単体テスト 3 件を追加（§21.8）。`MERGE20_NOTES.md` §10・§11 を補完し、§21（本節）を追記。ローカル `MERGE20_NOTES.md` を GitHub 版に同期。機能コードの変更なし | （本 PR） |

全スライス共通：CI は success、成果物は `WinMerge-x64.zip` と
`WinMerge-x64-Setup-x64.exe` の 2 件（各実行の詳細は §15.5・
§16.4・§17.4・§18.4・§19.4・§20.4・§21.9）。GoogleTest は
597 テスト全パス（L0 で 581→597 に増加。L6 で 597→600 になる
見込み。§21.9 で確認する）。

### 21.2 受け入れ条件の照合結果（A-3-1〜A-3-7）

判定の根拠は設計書 §1.3・§8 と仕様書 §4(c)・§4(d)・§9。
「満たす（コードレベル）」は単体テストとコード実読で確認したもの
で、Windows 実機での確認が別途要るものは「実機確認待ち」として
§21.3 の手順番号を付けた。「満たさない」は 1 件ある（A-3-1 の
点滅要求。仕様書改訂候補 M-3-1 の拡張として記録済み）。

| # | 受け入れ条件 | 判定 | 根拠 |
|---|---|---|---|
| A-3-1 | 完全一致（1 件以上）・特徴一致（3 件以上かつ 80% 以上）の閾値どおりに候補が出ること。閾値未満では候補が出ないこと | 満たす（コードレベル）。ただし完全一致キーの実ファイルでのヒット率は実機確認待ち（T-3-7、§21.3 手順 21） | `Merge20LearnStore::Match`（`Merge20Learn.cpp`）：完全一致は `best >= 1`、特徴一致は `total >= 3 && best*5 >= total*4`（整数比。小数を使わない）。単体テスト `MatchThresholds` が境界値を網羅：完全一致 1 件→候補あり、特徴 2 件→なし、2/3（66%）→なし、3/4（75%）→なし、4/5（80%）→あり、7/9（77%）→なし、8/10（80%）→あり、3/3（100%）→あり。L6 で追加した `ExactZeroRecords`（完全一致 0 件→なし）と `Feature79PercentBoundary`（15/19＝78.9%→なし、16/20＝80%→あり）で設計書 §8 の境界値をすべて満たした。HTML の学習関数を Node で実行した ground truth とすべて一致（§21.8）。同数時は `lastStrat` 優先（`DominantTie`）。**満たさない点が 1 件**：仕様書 §4(d) の「点滅させ」は実装していない（L3 の決定。静的表示に落とした。M-3-1 の拡張。§21.5）。点滅の有無は設計書 §2.3 により A-3-4 の判定には影響しない（必須なのは候補が出ていることが一目で分かること） |
| A-3-2 | 同一 JSON の再取込で二重計上されないこと（置換になること） | 満たす（コードレベル）。実機確認は §21.3 手順 18 | `Merge20LearnImportJson` は取込前に `RemoveSource(header.sourceId)` でその出典の寄与をすべて消してから加算する（置換）。単体テスト `HtmlJsonImport`：同一 JSON の 2 回取込で `n` が 3 のまま。L6 で追加した `ChangedContentReimport`：内容を変えた再取込（`REPLACE_WITH_OLD` 2→5）で `n` が 6（5＋1）になり、合算の 8 にならないこと（Node ground truth と一致） |
| A-3-3 | 出典削除で他出典の寄与が残ること | 満たす（コードレベル）。実機確認は §21.3 手順 14 | `RemoveSource` は対象出典の寄与分だけ各ルールの `counts` から差し引き、他出典には触れない。`n == 0` のルールは消える。単体テスト `RemoveSource`（`u-aaa` 削除で `u-bbb` の 1 件が残り、`u-bbb` 削除でルールが 0 件になること）と `GetSources`・`MergeSources`（統合出力は選択した出典の合算）。訂正も自分の出典の件数だけに作用し、他出典には触れない（`RecordCorrection`） |
| A-3-4 | 候補の受け入れ／却下が人の明示操作でのみ行われ、候補提示だけでは結果ペインの内容も確認内容も変わらないこと | 満たす（コードレベル）。実機確認待ち（§21.3 手順 8〜10） | 候補は `CMergeDoc::m_merge20LearnCandidates` にだけ保持し、`m_resultSegments` には一切触れない（`Merge20LearnRefreshCandidates` は候補マップだけを書き換える）。候補がある差分は `Unresolved`／`Conflict` のまま残る。`ResultChooseSources` の呼び出し元は、トグル（`OnMergeChooseSource` 経由の明示操作）、一括選択（`ResultChooseAllConflicts`）、候補の受け入れ（`OnMerge20LearnApply`。メニューからの明示操作）、セッション復元（`Merge20SessionDoc.cpp`。`OnMergeChooseSource` を通らないため記録されない）のみ。`ApplyAutoMergeToResult` は `ResultChooseSources` を呼ばない。学習コードからの `ResultChooseSources` の呼び出しは受け入れハンドラだけであり、起動時・読み込み時・候補計算時の自動呼び出し経路はない（仕様書 §4(c)）。受け入れ・却下のどちらも確認内容（`ReviewState`）を変えない（コード上に変更箇所なし） |
| A-3-5 | OFF の間は候補提示も記録も行われないこと。ON に戻すと再開すること | 満たす（コードレベル）。実機確認待ち（§21.3 手順 12） | `Merge20LearnIsEnabled()`（`OPT_MERGE20_LEARNING_ENABLED` の読み手）が 2 箇所でガードする：記録関数 `Merge20LearnRecordDiffJudgment` は OFF なら即 `false` を返す。候補計算 `Merge20LearnRefreshCandidates` は OFF なら候補マップをクリアして何も計算しない。オプション確定時（`CMainFrame::OnOptions` IDOK）とモーダル内のトグルで `Merge20LearnRefreshAllOpenDocs()` が呼ばれ、OFF 切替でクリア・ON 切替で再計算される。トグルは比較オプション（`PropCompare`）と学習モーダルの双方にあり、同じキーを読む（L1・L4）。OFF の間の判定は記録されないため、後から ON にしても取り戻せない（HTML と同じ） |
| A-3-6 | HTML の学習 JSON（v2）をフォークで読み込めること。フォークの JSON を HTML で読み込めること（形式レベル） | 満たす（形式レベル）。実機確認は §21.3 手順 17・19 | フォーク→HTML：単体テスト `JsonRoundTrip`・`MergedJson` と、L6 の Node 検証（§21.8）で、フォークの出力 JSON を HTML の `importLearn`・`matchLearn` に通し、完全一致 `REPLACE_WITH_OLD` n=2・特徴一致 `KEEP_NEW` n=5 の候補が一致すること、再取込で件数が変わらないことを確認。HTML→フォーク：単体テスト `HtmlJsonImport`（HTML の実物エクスポート JSON を取込・再取込・出典別保持できること）と `V1Import`（出典なし v1 は `file:<hashStr(ファイル名)>` 出典に移行すること。`file:a7884772efc7dec9` が HTML と一致）。`app` はフォーク `"Merge20Learn"` と HTML `"マージ支援20号"` の両方を受理し、欠落時は v1 として受理する。それ以外の `app`・`version` 3 以上・文字列型 `version` は拒否する（`JsonErrors`）。完全一致キーの実ファイルでのヒット率だけが差分エンジンの差に依存する（T-3-7、§21.3 手順 21）。特徴一致はグルーピングに依存しないため影響を受けない |
| A-3-7 | 各スライスで CI が緑になり、artifacts API で `WinMerge-x64.zip` の存在を確認すること | L0〜L5 は満たす（§21.1 の表のとおり、各 PR で success＋成果物 2 件）。L6 は §21.9 で確認する | 計画書の作業ルールどおり、緑だけでは不十分として毎回 artifacts API で確認した。L0〜L5 の各実行で `WinMerge-x64.zip` と `WinMerge-x64-Setup-x64.exe` の 2 件を確認済み（詳細は §15.5・§16.4・§17.4・§18.4・§19.4・§20.4） |

### 21.3 Windows 実機での確認手順（統合版）

L0〜L5 の各節の実機確認手順（§15.4・§16.3・§17.3・§18.3・
§19.3・§20.3）を統合し、重複を整理した最終版。L6 で新たに確認
する手順はない（L6 はコード変更がないため）。**すべての手順は
CI 成果物の ZIP（`WinMerge-x64.zip`、実行は §21.9）を展開し、
ポータブル版として使う。** ポータブル扱い（`WinMergeU.exe` と
同じフォルダに `WinMergeU.ini` がある場合）のとき、学習ファイルは
`<progpath>\Merge20Learn\Merge20Learn.json` にできる。デモデータ
は HTML（v17.15）の `loadDemo` の 5 ファイル相当（固定資産税の
計算コード）を使う。

1. **起動・回帰確認**：`WinMergeU.exe` が従来どおり起動すること。
   フォルダ比較・ファイル比較・セッション保存・復元が壊れていないこと。
2. **比較オプションのトグル**：ツール → オプション → 比較ページを開き、
   「🎓 学習機能」チェックボックスが表示されること（既定で ON）。下に
   「OFF にすると候補提示と記録を停止します。」と表示されること。
   チェックを外して OK を押し、再度開くと OFF が維持されること
   （`Merge20/LearningEnabled` が false になること）。ON に戻すと
   true に戻ること。
3. **学習ファイルの生成**：2-way のファイル対を開き、比較を開始
   （マージセッションを開始）する。結果ペインで差分を 1 つ選び、キー
   1（旧）または 2（新）でソースを選択する。`Merge20Learn\` に
   `Merge20Learn.json` ができていること。ファイルを開き、記録した差分の
   ルール（`key` が `EXACT|...`、`counts` に選んだ方針値が 1 件）が
   入っていること。日本語（差分テキスト）が文字化けなく読めること
   （UTF-8・BOM なし）。
4. **学習データが増えること**：別の差分でもう 1 件判定すると、`counts`
   の件数が増えること（同じパターンの差分なら同じルールの件数が増え、
   違うパターンなら新しいルールが増える）。
5. **訂正**：同じ差分で違う方針を選び直すと、旧方針の件数が -1・新方針が
   +1 され、`lastStrat` が新しい方針になること。
6. **記録されない経路**：自動マージ（`/al` または自動マージボタン）で
   解決された差分、一括承認（S5 未実装のため現状は「すべて選択」に相当する
   操作）で解決された差分、セッション復元で復元された差分については、
   `Merge20Learn.json` の件数が増えないこと。
7. **候補表示（静的表示）**：手順 3〜5 で学習したパターンと同じ差分を
   含むファイルを開いてマージセッションを開始する。候補がある差分の行が
   結果ペインで紫色になり、マージンに `?` ではなく `L` が出ること。
   **行は点滅しない**（静的表示が L3 の仕様。§18.2-1）。点滅がないことは
   不具合ではない。
8. **動的メニュー文言（`BCMenu::ModifyMenu`）**：候補がある差分で結果ペインを
   右クリックする。適用項目が「🎓 学習候補を適用（旧・完全一致・2件）」
   （方針値／一致種別／件数は実際の候補に従う）のように書き換わって
   表示されること。「候補を却下」も有効であること。候補がない差分では
   両項目とも無効（グレー）であること。**万一書き換えが反映されなくても、
   汎用文言「🎓 学習候補を適用」のまま機能は正しく動く。** 複数の候補が
   ある差分を選択した状態で右クリックすると、適用項目は汎用文言になること。
   各差分にはそれぞれ自分の候補の方針値が適用されること。
9. **候補の受け入れ**：「学習候補を適用」を押す。その差分だけが解決し、
   紫色と `L` が消えること。ファイルの確認内容が TODO のままであること。
   `Merge20Learn.json` で適用した方針値の件数が +1 されていること。
10. **候補の却下**：「候補を却下」を押す。紫色と `L` が消え、差分は
    未解決のまま残り、確認内容も変わらないこと。再スキャン後も候補が
    再表示されないこと。`Merge20Learn.json` の件数が変わらないこと。
11. **候補ありで保存（A-3-4）**：候補が出ている状態で結果を保存する。
    その差分は未解決のコンフリクトセクションとして書き出されること
    （候補提示だけでは出力が変わらないこと）。
12. **「不採用」の表現（T-3-8、`bNoSrc` 経路の発火確認）**：旧欠落のみの
    差分で新側（キー 2）を選ぶと、結果ペインに `<No Lines>` 行が 1 行
    表示され、保存時にその行が除外される（旧行が結果に含まれない）こと。
    新追加のみの差分で旧側（キー 1）を選んだ場合も同じこと。`KEEP_NEW`
    の候補を受け入れた場合も同じこと。**発火しない場合は「不採用」の
    表現方法を設計し直す（設計書 §2.2 の注記）。実機確認待ち。**
13. **OFF 時の停止と再開（A-3-5）**：ツール → オプション → 比較ページで
    「🎓 学習機能」のチェックを外して OK を押す。その後で差分判定をしても
    `Merge20Learn.json` の件数が増えないこと。開いているマージ文書の
    候補表示（紫色＋`L`）が消えること。ON に戻すと再び記録され、候補が
    再表示されること。モーダル内の ON/OFF 切替でも同じこと（モーダルで
    OFF にするとオプション側も OFF になること）。
14. **学習モーダルの表示**：フォルダ比較を開き、右クリックメニューに
    「学習…」があること。学習データがまだない状態で開くと、統計が
    「パターン 0件 ／ 判定記録 0件 ／ 出典 0件 ／ 候補提示中 0件」で、
    ルール一覧・出典一覧に「まだ学習データがありません。」と出ること。
    学習データを作って開くと、統計のパターン／判定記録の件数が
    `Merge20Learn.json` の内容と一致し、ルール一覧に
    `[完全一致] キー - 方針（N回／NN%）` の行が出ること（方針と件数は
    JSON の最多方針・件数と一致すること。完全一致キーは先頭 84 文字まで
    の表示）。候補が出るファイル対を開いた状態でモーダルを開くと、
    「候補提示中 K 件」が開いている文書の候補件数（紫色＋`L` の差分の数）
    と一致すること。マージ文書をすべて閉じると 0 に戻ること。モーダル下部に
    共有注意文（「学習データにはソースコードの断片が含まれます。共有は
    チーム内の承認された経路で行ってください。社外への持ち出し、個人
    クラウドへの保存は禁止です。」）が表示されること。モーダルを開いて
    閉じるだけで、フォルダ一覧・確認内容・出力チェック・
    `Merge20Learn.json` の内容が変わらないこと。
15. **出典の削除（A-3-3）**：2 出典の状態（自分の出典＋HTML から取り込んだ
    出典。手順 18 で作る）で、削除したい出典だけにチェックを入れて
    「選択したソースを削除」を押す。確認ダイアログで「はい」を選ぶと、
    その出典の寄与分だけ件数が減り、他方の出典のルールが残ること
    （`Merge20Learn.json` で確認）。2 件以上にチェックを入れたときは
    件数つきの確認（「%1件のソースの学習データを削除しますか？」）が出ること。
    「いいえ」では何も変わらないこと。**`CCheckListBox` のチェックボックス描画
    と `CLBN_CHKCHANGE` 通知の確認**：出典一覧の各行にチェックボックスが
    描画され、チェックを付け外しすると削除ボタンと統合ボタンが即座に
    有効・無効に切り替わること。チェックをすべて外すと両ボタンがグレーに
    なること。既定では全出典にチェックが入っていること。
16. **エクスポート**：モーダルで「学習データを出力（JSON)」を押す。
    保存ダイアログの既定ファイル名が
    `マージ支援20号_学習_YYYY-MM-DD.json` であること。共有注意文つきの
    確認で「はい」を選ぶとファイルが作られ、完了メッセージにルール件数と
    パスが出ること。「いいえ」ではファイルが作られないこと。出力した JSON
    をメモ帳で開き、`app` が `"Merge20Learn"`、`version` が `2`、
    `sourceId` が自分の出典 ID（`u-...`）、`sourceName` が出力ファイル名、
    `rules` に `key`／`counts`／`n`／`ex`／`last`／`lastStrat`／`sources`
    があること。
17. **統合出力**：2 出典の状態で両方にチェックを入れて「選択したソースを
    統合して出力」を押す。既定ファイル名が
    `マージ支援20号_学習_統合_YYYY-MM-DD.json` であること。出力した JSON
    をメモ帳で開き、ルールごとの `sources` がなく、`counts` が 2 出典の
    合算になっていること。root の `sources` に統合した出典の表示名の配列が
    あること。`sourceId` が自分の出典 ID であること。この統合 JSON を
    HTML（v17.15）で取り込めること（統合前の出典別の内訳は失われ、全体が
    1 出典として扱われること）。
18. **HTML の JSON を取込めること（A-3-6）**：HTML（v17.15）で学習データを
    作って出力した JSON（`マージ支援20号_学習_YYYY-MM-DD.json`）を、
    フォークのモーダルで「学習データを取込（蓄積型）」から取り込む。
    結果メッセージが「学習データを取込ました（<出典名>：新規N件・合算M件）」
    であること。モーダルの出典一覧に HTML の出典が増え、統計のパターン／
    判定記録の件数が増えること。取込んだルールに一致する差分を開くと候補
    （紫色＋`L`）が出ること（完全一致のヒット率は T-3-7 のとおり差分エンジン
    の差に依存するため、出ない場合は特徴一致の候補で確認する）。
19. **再取込は置換（A-3-2）**：同じ JSON をもう一度取り込む。結果メッセージの
    件数と、モーダルの統計のパターン／判定記録の件数が変わらないこと
    （`Merge20Learn.json` の該当出典の `counts` も変わらないこと）。
    内容を変えた JSON（HTML 側で判定を 1 件追加して再出力）を取り込むと、
    置換されて件数が新しい内容になること。
20. **クリア**：「学習データをクリア」を押すと確認ダイアログ（「他の作業者と
    共有したJSONには影響しません」）が出ること。「はい」を選ぶと統計が
    すべて 0 になり、`Merge20Learn.json` の `rules` が空になること。
    「いいえ」では何も変わらないこと。クリアしても、既に他の作業者と共有した
    JSON ファイル自体は消えないこと。
21. **取込んだ学習データが次回の候補計算から使われること**：手順 18 で
    取り込んだ直後に開いているファイルの候補が再計算され、取込んだルールに
    一致する未解決の差分に候補が出ること。候補が出ている状態で保存しても
    結果が未解決のままであること（自動適用されていないこと）。
22. **完全一致キーの HTML とのヒット率の実測（T-3-7）**：HTML（v17.15）で
    デモデータを開き、差分判定を数件行って学習 JSON を出力する（HTML 側の
    学習データ）。フォークで同じデモデータを開き、HTML の学習 JSON を
    取り込む。フォークでファイルを開いてマージセッションを開始し、HTML で
    学習した差分と同じ差分に候補が出るかを確認する。次の件数を記録する：
    (a) HTML で学習した完全一致ルールのうち、フォークで候補が出た件数／
    出なかった件数、(b) 特徴一致ルールのうち、フォークで候補が出た件数／
    出なかった件数、(c) 候補が出なかった完全一致ルールについて、HTML 側で
    同じファイル対を開いたときの学習表示（HTML の `matchLearn` がその差分に
    候補を出すか）と突き合わせ、差分エンジンの行グルーピング差（WinMerge の
    `DIFFRANGE`（diffutils）と HTML の `diffFast`（LCS））によるものかを
    確認する。ヒット率が低い場合は、特徴一致の利用を促す運用で補う
    （設計書 §7 R-3-5）。完全一致キーの生成方法自体を変えることは、HTML との
    互換性を壊すため行わない。**実機確認待ち。**
23. **3-way では学習が働かないこと**：3-way のファイル対では、差分判定を
    しても `Merge20Learn.json` の件数が増えないこと。結果ペインの
    コンテキストメニューに学習候補の項目が出ないこと。
24. **回帰確認（モーダル）**：モーダルを開いて 4 ボタンのいずれも押さずに
    閉じるだけで、フォルダ一覧・確認内容・出力チェック・
    `Merge20Learn.json` の内容が変わらないこと。

### 21.4 設計書・仕様書との食い違いの最終一覧

L0〜L5 の各節（§15.2・§16.2・§17.2・§18.2・§19.2・§20.2）から
集約した最終一覧。仕様書 PDF 自体は変更していない。

| # | 内容 | 出典 |
|---|---|---|
| D-3-1 | 小数を含むルールの per-rule スキップは不可。S8 パーサは整数以外をファイル全体のエラーにするため、そのようなファイルは全体を拒否する。正しい v2 ファイルに小数は現れない（`counts`・`n` は整数）ため実害なし。テスト `JsonErrors` で全体拒否を検証 | L0（§15.2-1） |
| D-3-2 | 重複キーは先勝ち。`ParseObject` は重複を許し `Find` は先頭を返す。学習側の `counts` パーサも先頭の出現を採用する（T-3-2 を解消） | L0（§15.2-2） |
| D-3-3 | `OP_TRIVIAL` 行は特徴キーの type 判定で mod 扱い（HTML に相当概念がないため） | L0（§15.2-3） |
| D-3-4 | round 正規表現は大文字小文字を区別する（HTML と同一。S4 の `RoundingRegex` は icase 付きのため流用せず）。SQL 正規表現は `join` を含まない（HTML と同一。S4 の `SqlRegex` は `join` を含むため流用せず） | L0（§15.2-4） |
| D-3-5 | `FromJson` は出典つきルールの集計 `counts` を出典の合計から再計算する（ストアの不変条件 `counts == 出典の合計` を保つため）。出典なしルールはファイルの `counts` をそのままファイル出典に帰属させる | L0（§15.2-5） |
| D-3-6 | `ImportJson` は HTML `importLearn` と同一の平坦化（ルールの `sources` は保持せず、ファイル全体を 1 出典として加算する。再取込は置換） | L0（§15.2-6） |
| D-3-7 | `Merge20LearnCandidate` は `Merge20Learn.h` に置いた（設計書 §3.2 は `Merge20Types.h` としていたが、方針値 enum が `Merge20Learn.h` にあるため） | L0（§15.2-7） |
| D-3-8 | L1 だけでは `Merge20Learn.json` が生成されない（保存を呼ぶ記録口は L2 で初めてできるため。ファイル生成の実機確認は L2 に回した） | L1（§16.2-1） |
| D-3-9 | OFF 切替時の候補クリアは L1 では実施しない（候補マップ自体が L2 で初めて作られるため。トグルによるキーの切り替えまでが L1 の範囲） | L1（§16.2-2） |
| D-3-10 | `CoCreateGuid` の利用前例はなし（GitHub code search で 0 件。ただし `incomplete_results: true` のため全リポジトリ grep ではない）。標準 Win32 API の新規利用とし、失敗時のフォールバック（`u-local-YYYYMMDDHHMMSS`）も用意した。新規依存なし（T-3-4 を解消） | L1（§16.2-3） |
| D-3-11 | `OPT_MERGE20_LEARNING_ENABLED` に読み手がないことを再確認し、L1 で初の読み手 `Merge20LearnIsEnabled()` を追加した | L1（§16.2-4） |
| D-3-12 | 比較オプションの説明文はチェックボックスとは別の LTEXT 1 行にした（T-3-6 は解消。ダイアログ下部に余白があり重ならない） | L1（§16.2-5） |
| D-3-13 | 内部保存時の `sourceName` は固定ファイル名 `Merge20Learn.json`。`exportedAt` はローカル時刻の `%Y-%m-%dT%H:%M:%S`（`Z` なし。UTC への変換はしていない） | L1（§16.2-6） |
| D-3-14 | `.rc` のラベルは英語のまま（`Merge.rc` は ASCII のため絵文字・日本語は書けない。`.po` 経由で日本語化） | L1（§16.2-7） |
| D-3-15 | 記録の逆写像の明確化：`INSERT_OLD` と `REPLACE_WITH_OLD` がどちらも `{0}` になるため、記録時（`srcPanes`→方針値）は差分の種類で区別する（`OP_1STONLY` の `{0}` は `INSERT_OLD`、`OP_DIFF`・`OP_2NDONLY` の `{0}` は `REPLACE_WITH_OLD`）。提示時（方針値→`srcPanes`）はどちらも `{0}` になるため適用結果は同じ | L2（§17.2-1） |
| D-3-16 | OFF 切替時の即時クリアは比較オプションのページ側（`PropCompare::OnOK`）には置かない。`CPreferencesDlg::OnOK` を実読したところ、オプションの保存はページの `OnOK` より後に走るため、ページ側のフックでは新しい設定値を読めない。代わりにオプション確定後の `CMainFrame::OnOptions()`（IDOK）に置いた | L2（§17.2-2） |
| D-3-17 | 却下の扱いは設計書の決定どおり（却下は学習に記録せず、再スキャン後も再表示しない。差分の同一性で抑止する）。却下はセッションに保存しない（メモリ内のみ） | L2（§17.2-3） |
| D-3-18 | T-3-9 は解消：`Rescan()` は UI スレッドからのみ呼ばれる。学習ストアへの比較スレッドからのアクセスはない | L2（§17.2-4） |
| D-3-19 | L2 は新規の単体テストなし（MFC アダプタのため。S8 の `Merge20SessionDoc.cpp` と同じ扱い）。方針値と `srcPanes` の往復写像は実機確認項目とした | L2（§17.2-5） |
| D-3-20 | 3-way では記録も候補計算もしない（設計書 §2.2・§3.2 は 2-way のみ） | L2（§17.2-7） |
| D-3-21 | T-3-1 は解消（静的表示へのフォールバックを採用）。点滅は実装しない。仕様書 §4(d) の「点滅させ」との齟齬は M-3-1 の拡張として記録（§21.5） | L3（§18.2-1/2） |
| D-3-22 | 受け入れハンドラは `OnMergeChooseSource` を経由しない（トグルでは候補の方針値をそのまま適用できないため）。`ResultChooseSources` を直接呼び、その後に同じ記録関数を呼ぶ。記録は `ResultChooseSources` には置いていないため、一括承認（S5）・自動マージ・セッション復元（S8）が学習に混ざることは構造的にない | L3（§18.2-3） |
| D-3-23 | 受け入れ時の記録対象は、このコマンドで実際に適用した差分だけに限定する（選択範囲内に既に `Chosen` の差分が含まれていた場合の二重計上を防ぐため） | L3（§18.2-4） |
| D-3-24 | 候補の件数表示場所（T-3-5）：結果ペイン内に件数を常時表示する場所は作らない。件数（`n`）と一致種別は、候補が 1 件のときの適用メニュー項目の文言でのみ示す。ステータスバーへの件数表示は見送り | L3（§18.2-6） |
| D-3-25 | エクスポート／インポート／統合して出力／クリアのボタンは L4 では配置のみで無効化し、配線は L5 に回した | L4（§19.2-1） |
| D-3-26 | 出典一覧のチェックボックスは L5 に回し、L4 では単一選択＋「選択したソースを削除」とした | L4（§19.2-2） |
| D-3-27 | HTML の統計「今回の解析で自動判定 N 件」は廃止し、「候補提示中 K 件」に置き換えた（設計書 §3.3 どおり） | L4（§19.2-3） |
| D-3-28 | 最多方針の表示は L0 の判定と共用する（`Merge20LearnDominant` の公開）。モーダル側で独自に計算すると同数時の `lastStrat` 優先などの食い違いが出るため | L4（§19.2-4） |
| D-3-29 | 既定の保存場所：ファイル名だけを固定し、保存場所は保存ダイアログに任せた（HTML の保存場所はブラウザのダウンロードフォルダであり、デスクトップアプリに直接の対応物がないため。S6 の CSV 出力と同じ形） | L5（§20.2-1） |
| D-3-30 | `exportedAt` はローカル時刻（`Z` なし）。HTML は UTC の `toISOString()`（`Z` 付き）だが、L1 の内部保存と同じ形式に統一した。取込時にこの値が違っても拒否されない | L5（§20.2-2） |
| D-3-31 | 内部保存の失敗文言が 2 箇所だけ変わった（`WriteTextFileAtomic` への抜き出しの副作用）。「failed to write learning file」→「failed to write file」など。ユーザーが見る日本語の本体は変わらない | L5（§20.2-3） |
| D-3-32 | 統合出力の出典表示名の順序はストアの出典順（`GetSources()` の順、判定件数の多い順）。HTML はチェックした順に並べるが、root の `sources` は表示名の配列で順序に意味はないため | L5（§20.2-4） |
| D-3-33 | 共有注意文は出力時の確認ダイアログで表示する（モーダル下部の注意文に加え、保存ダイアログの後に注意文＋出力先パス＋「出力しますか？」の確認を出す。統合出力でも同じ） | L5（§20.2-5） |
| D-3-34 | 取込時の `app`／`version` の検査は L0 の `FromJson` に任せた。L5 で新たに検査はしない | L5（§20.2-6） |

### 21.5 仕様書改訂候補 M-3-1〜M-3-3 の最終状態

仕様書 PDF 自体は変更しない。以下は仕様書の次回改訂への提案として
残る（設計書 §1.4 の一覧の最終状態）。

| # | 内容 | 最終状態 |
|---|---|---|
| M-3-1 | §4(d) の「自動判定」という語を、フォーク版では「候補提示」と読み替える。表示の文言も「自動で決まったことが一目で分かる」→「候補が出ていることが一目で分かる」と読み替える。**L3 での拡張**：点滅の要求を「候補が出ていることが一目で分かる表示（紫色＋`L` マーカー。点滅は任意）」に改めること | 実装済み（L2・L3・L4）。点滅なし・静的表示（紫色＋`L` マーカー＋メニューの候補文言）。仕様書 §4(c) の絶対条件（自動適用の禁止）との衝突を解消するための読み替えであり、フォーク版は §4(c) を優先する。改訂提案として残る |
| M-3-2 | §4(d) の「自ブラウザ相当＝『自分』」を、フォーク版では「この PC で学習（自分）」と読み替える。出典 ID は `u-<GUID>` 形式とする | 実装済み（L1）。`OPT_MERGE20_LEARNING_OWNER` に保存。`self` という出典 ID はフォークでは作らない（HTML が `self` を特別扱いするため）。HTML の JSON の `self` 出典はフォークでも `self` のまま保持し、表示名は HTML と同じ「このブラウザで学習（自分）」とする。改訂提案として残る |
| M-3-3 | §6.1 の「画面に出ているものはすべて」保存対象のうち、学習データはセッションファイルには含めず別ファイルとする（仕様書 §6 の対応表どおり） | 実装済み（L1・L2）。学習データは `Merge20Learn\Merge20Learn.json` 1 ファイル。セッション（フォルダ組単位）と寿命が違うため混ぜない。改訂提案として残る |

### 21.6 未解決の要調査 T-3-1〜T-3-9・U-3-1〜U-3-11 の最終状態

| ID | 内容 | 最終状態 |
|---|---|---|
| T-3-1 | 結果ペインでの点滅表示の実現方法 | ✅ 解消（L3）。`CMergeResultView` にタイマ前例がなく、行単位の `Invalidate` の仕組みもないため、静的表示（紫色＋`L` マーカー）に落とした（フォールバック採用）。M-3-1 の拡張として記録 |
| T-3-2 | S8 JSON パーサの重複キー挙動 | ✅ 解消（L0）。重複を許し `Find` は先頭を返す（先勝ち） |
| T-3-3 | HTML v1 JSON の実物のフィールド構成 | ✅ 形式上は解消（L0・L5）。`app` が欠落している場合は v1 として受理する。⚠️ 実物 v1 ファイルでの確認は残る：HTML v17.15 は v2 しか出力しないため、実物 v1 は旧版の出力物が必要。L0 の `V1Import` は構成した v1 JSON での検証である |
| T-3-4 | 出典 ID（`u-<GUID>`）の生成方法 | ✅ 解消（L1）。`CoCreateGuid` の新規利用（前例なし）。失敗時のフォールバックあり |
| T-3-5 | 学習候補の件数表示場所 | ✅ 解消（L3・L4）。結果ペイン内に置かず、候補 1 件のときの適用メニュー項目の文言とモーダルの統計（「候補提示中 K 件」）で示す。ステータスバーへの件数表示は見送り |
| T-3-6 | `PropCompare` のダイアログテンプレートの余白とコントロール ID | ✅ 解消（L1）。ダイアログ下部に余白があり重ならない |
| T-3-7 | 完全一致キーの行グルーピング差によるヒット率 | ⏳ 残る（実機確認待ち）。§21.3 手順 22 で実測する |
| T-3-8 | `bNoSrc`（`<No Lines>`）経路の実機での発火 | ⏳ 残る（実機確認待ち）。§21.3 手順 12 で確認する。発火しない場合は「不採用」の表現方法を設計し直す |
| T-3-9 | 学習ストアへのアクセスのスレッド | ✅ 解消（L2）。`Rescan()` は UI スレッドからのみ呼ばれる |
| U-3-1 | 結果ペインのコンテキストメニューの構築箇所 | ✅ 解消（L3）。`IDR_POPUP_MERGERESULTVIEW_2WAY` に追加した |
| U-3-2 | `OPT_MERGE20_LEARNING_ENABLED` の読み手が存在しないことの全リポジトリでの再確認 | ✅ 解消（L1）。読み手がないことを再確認し、初の読み手を追加した |
| U-3-3 | `CMainFrame::GetAllMergeDocuments` のシグネチャと戻り値 | ✅ 解消（L2）。`GetAllMergeDocuments()`＋`dynamic_cast<CMergeDoc*>` で全文書を更新する入口として使った |
| U-3-4 | S8 JSON パーサの重複キー挙動（＝T-3-2） | ✅ 解消（L0） |
| U-3-5 | HTML v1 JSON の実物のフィールド構成（＝T-3-3） | ✅ 形式上は解消。実物での確認は残る（T-3-3 と同じ） |
| U-3-6 | `CoCreateGuid` の利用前例（＝T-3-4） | ✅ 解消（L1） |
| U-3-7 | `PropCompare` のダイアログテンプレートの余白とコントロール ID（＝T-3-6） | ✅ 解消（L1） |
| U-3-8 | `Rescan()` の呼び出しスレッド（＝T-3-9） | ✅ 解消（L2） |
| U-3-9 | `bNoSrc` 経路の実機での発火（＝T-3-8） | ⏳ 残る（実機確認待ち） |
| U-3-10 | 完全一致キーのヒット率の実測（＝T-3-7） | ⏳ 残る（実機確認待ち） |
| U-3-11 | U-0-2（2-way 自動マージの挙動確認）は Phase 3 の範囲外 | ⏳ Phase 3 の範囲外のまま。S5 着手前に Windows 実機で確認すること（Phase 2 設計書 §3.0）。フェーズ 3 としては未解決 |

設計子エージェントからの親エージェントへの質問 4 件の最終状態：

1. **点滅表示の扱い**：フォールバック採用で決着（L3）。タイマ方式が不可だった場合の静的な紫色＋`L` マーカーでよいか、という問いに対し、設計書 §2.3 のフォールバックどおり静的表示とした。仕様書 §4(d) の「点滅させ」との齟齬は M-3-1 の拡張として記録した。
2. **候補提示中の件数表示**：HTML の統計「今回の解析で自動判定 N 件」は廃止し、代わりに「候補提示中 K 件」（開いているマージ文書の候補件数）をモーダルに表示する方針で実装した（L4）。
3. **却下の扱い**：却下は学習に記録せず、再スキャン後も再表示しない（差分の同一性で抑止する）方針で実装した（L2・L3）。将来的に「却下を負のフィードバックとして記録する」かは Phase 4 以降の検討とする。
4. **ローカルの `MERGE20_NOTES.md` が古い**：L6 で対応した。GitHub 上の master 版（§21・§10・§11 を含む最終版）にローカルを合わせた（§21.10）。ローカルの未コミット変更は `MERGE20_NOTES.local-backup.md` に保存してから上書きした。

### 21.7 実機確認待ちの項目一覧（最終版）

フェーズ 3 として未解決の実機確認は次の 5 件。すべて §21.3 の
手順に番号つきで入っている。コードレベルでは確認済みであり、
Windows 実機でのみ確認できるものである。

| # | 項目 | §21.3 の手順 | コードレベルの確認状況 |
|---|---|---|---|
| 1 | T-3-7：完全一致キーの HTML とのヒット率の実測 | 手順 22 | 形式レベルの互換性は単体テスト＋Node で確認済み。ヒット率だけが差分エンジンの差に依存するため実測が必要 |
| 2 | T-3-8：`bNoSrc`（`<No Lines>`）経路の発火確認 | 手順 12 | コード上は `ResultChooseSources` の `bNoSrc` 分岐があり、ゴースト行が 0 行として数えられる前提。実機では未確認 |
| 3 | `CCheckListBox` のチェックボックス描画と `CLBN_CHKCHANGE` 通知 | 手順 15 | コード上は MFC の標準的な使い方（`LBS_OWNERDRAWFIXED`／`LBS_HASSTRINGS`＋`CLBN_CHKCHANGE`／`LBN_SELCHANGE`）。このリポジトリに `CCheckListBox` の前例がないため実機での表示確認が必要 |
| 4 | `BCMenu::ModifyMenu` による動的メニュー文言の表示 | 手順 8 | コード上は候補 1 件のときだけ適用項目を書き換える。万一書き換えが反映されなくても汎用文言のまま機能は正しく動く |
| 5 | 点滅なしの静的表示の確認 | 手順 7 | コード上は紫色＋`L` マーカーの静的表示。行が点滅しないことを確認する（点滅がないことは仕様であり、不具合ではない） |

このほか、§21.3 の手順 1〜24 のうち、コードレベルでは確認済みだが
実機での動作確認がまだのもの（学習ファイルの生成・記録・訂正・
受け入れ・却下・ON/OFF・モーダル表示・エクスポート・取込・統合・
クリア・3-way での不作動）は、フェーズ 3 の完了後にユーザーが
CI 成果物で一括して確認する。

### 21.8 L6 の検証結果（ローカルハーネス＋Node ground truth）

- **受け入れテストの実施で不具合は見つからなかった。** そのため
  L6 では機能コードの変更をしていない。追加したのは単体テスト
  3 件だけである（下記）。
- **新規の単体テスト 3 件**（`Testing/GoogleTest/Merge20/
  Merge20Learn_test.cpp` に追加。既存の 16 件ではカバーされていなかった
  設計書 §8 の境界値を埋めるもの。CI での実行結果は §21.9）：
  - `ExactZeroRecords`：完全一致 0 件→候補なし（既存の
    `MatchThresholds` では暗黙的にしか検証されていなかった）。
  - `Feature79PercentBoundary`：特徴一致 15/19（78.9%）→候補なし、
    16/20（80%）→候補あり（既存は 7/9・3/4・4/5・8/10・3/3 のみで、
    79% に最も近い境界値がなかった）。
  - `ChangedContentReimport`：内容を変えた再取込は置換になること
    （`REPLACE_WITH_OLD` 2→5 で `n` が 6 になり、合算の 8 にならないこと。
    既存の `HtmlJsonImport` は同一内容の再取込しか検証していなかった）。
- **ローカル g++ ハーネス：66 チェック全合格。** master の現行コード
  （L4 の `Merge20LearnDominant` 公開関数化を含む）を、スタブ
  `pch.h`／`UnicodeString.h`／`DiffList.h`＋実物の
  `Merge20Session.cpp`／`Merge20Classify.cpp` でコンパイルし、既存
  16 テストの主張と新規 3 テストの主張を同じロジックで実行した
  （`wchar_t` 4 バイトで検証。ground truth は BMP 文字のみのため
  Windows と同じ結果になる。L0 と同じ方法）。
- **Node ground truth：17 チェック全合格。** HTML の学習関数
  （`hunkKeyExact`／`hunkFeatKey`／`matchLearn`／`learnDominant`／
  `learnAdd`／`removeLearnSource`／`hashStr`。`17_v17.15.html` から
  抜粋）をそのまま Node で実行し、完全一致キー・特徴キー・閾値境界
  （0 件・1 件・2 件・2/3・3/4・4/5・7/9・8/10・3/3・15/19・16/20）・
  内容変更後の再取込の置換・同一内容の再取込・出典削除が C++ と
  一致することを確認した。`hashStr` のベクタ（`''`・`a.json`・
  `src/Foo.java`・`old-learn.json`→`file:a7884772efc7dec9`）も一致した。
- **双方向チェック（フォーク→HTML）：8 チェック全合格。** C++ ハーネスが
  書き出したフォークの JSON を HTML の `importLearn` のロジック
  （`removeLearnSource`＋`learnAdd`）で取り込み、`matchLearn` で
  完全一致 `REPLACE_WITH_OLD` n=2・特徴一致 `KEEP_NEW` n=5 の候補が
  一致すること、再取込で件数が変わらないことを確認した。
  HTML→フォークは単体テスト `HtmlJsonImport`・`V1Import` で確認済み。
- **自動適用の禁止（仕様書 §4(c)）の再確認**：L0〜L5 の全コードを実読し、
  学習からの自動適用の経路がないことを確認した。候補計算
  （`Merge20LearnRefreshCandidates`）は候補マップだけを書き換え、
  `m_resultSegments` には触れない。`ResultChooseSources` の呼び出し元は
  明示操作（トグル・一括選択・候補の受け入れ）とセッション復元だけで、
  `ApplyAutoMergeToResult` は `ResultChooseSources` を呼ばない。
  記録口は `OnMergeChooseSource` だけに限定されており、受け入れハンドラは
  同じ記録関数を呼ぶが `OnMergeChooseSource` を経由しない（L3 の設計どおり）。
  記録は `ResultChooseSources` には置いていないため、自動マージ・
  一括承認（S5）・セッション復元（S8）が学習に混ざることは構造的にない。

### 21.9 CI 結果（PR #18）

- PR: https://github.com/askhim659/winmerge/pull/18（`merge20/phase3-l6` → `master`、3 コミット、未マージ）
- 実行 45702991568（2026-11-25 21:17Z）：**success**。ジョブ build 139517640964。
  - 成果物：`WinMerge-x64.zip` 26,194,774 バイト（ID 15064214343）、
    `WinMerge-x64-Setup-x64.exe` 39,687,933 バイト（ID 15065235829）。
    いずれも失効日 2027-02-23。
  - GoogleTest：**600 テスト全パス**（`[  PASSED  ] 600 tests.`。L5 までの
    597＋新規 3）。Merge20Learn は 19 件すべて OK（既存 16 件＋新規
    `ExactZeroRecords`／`Feature79PercentBoundary`／
    `ChangedContentReimport`）。`[  FAILED  ]` は 0 件。
  - CI 失敗 0 回（初回実行で success）。

### 21.10 ローカル同期（L6）

設計子エージェントからの質問 4（ローカルの `MERGE20_NOTES.md` が
古い）の対応。ローカルの `~/workspace/winmerge-fork/
MERGE20_NOTES.md` を GitHub 上の master 版（§21・§10・§11 を含む
最終版）に合わせた。安全手順どおり、まずローカルリポジトリで
`git status` と `git diff` を確認したところ、`MERGE20_NOTES.md`
にローカルの未コミット変更があったため、その内容を
`~/workspace/winmerge-fork/MERGE20_NOTES.local-backup.md` に保存してから
上書きした。ローカルリポジトリはブランチ `merge20/phase1` のままであり、
ブランチの切り替えや `git push` はしていない。

## 22. Phase 4 D0（基盤：オプションキー・リソースID・計測フック）— ブランチ `merge20/phase4-d0`

設計書 `merge20-phase4-design.md` §5.2 の D0 どおり。基盤（キー・ID・
計測フック）までとし、UI の表示・動作は作らない（D1 以降）。C++ の変更は
この Linux VM ではコンパイルできないため、CI でのビルド＋成果物確認が
検証になる。

### 22.1 変更一覧

- `Src/OptionsDef.h`／`Src/OptionsInit.cpp` — 新規キー 11 件
  （設計書 §4.1(a) どおり。既存 WinMerge の設定キーの改名なし）。
  既定値は HTML（`17_v17.15.html`）のものに合わせた：

  | キー | 既定値 | 備考 |
  |---|---|---|
  | `OPT_MERGE20_DED_MODE`（`Merge20/DedMode`） | false | 専用モードの有無（HTML `dedMode`） |
  | `OPT_MERGE20_DED_PINNED`（`Merge20/DedPinned`） | false | 専用バーのピン固定（HTML `dedPinned`） |
  | `OPT_MERGE20_DED_ZOOM`（`Merge20/DedZoom`） | 750 | 専用モード時の倍率（HTML `DEF_SCALE=0.75`）。範囲 350–2200（HTML の camS 0.35–2.2）。`OPT_VIEW_ZOOM`（既定 1000）とは別キー |
  | `OPT_MERGE20_TODO_ONLY`（`Merge20/TodoOnly`） | false | 未確認のみ巡回（HTML `todoOnly`） |
  | `OPT_MERGE20_SNIPER_ON`（`Merge20/SniperOn`） | false | オートスナイパーの ON／OFF（HTML `sniperOn`） |
  | `OPT_MERGE20_SNIPER_FONT`（`Merge20/SniperFont`） | 20 | スナイパーの文字サイズ（HTML `snFont`）。範囲 14–28（HTML のクランプ） |
  | `OPT_MERGE20_SNIPER_LAYOUT`（`Merge20/SniperLayout`） | 0 | スナイパーのレイアウト（0＝3 列 `cols`、1＝3 段 `rows`）。範囲 0–1 |
  | `OPT_MERGE20_SLIM`（`Merge20/Slim`） | false | スリムリスト（HTML `slim`） |
  | `OPT_MERGE20_DOCK_HIDDEN`（`Merge20/DockHidden`） | false | ドックの隠し状態（HTML `dockHid`） |
  | `OPT_MERGE20_FOCUS_DOCK_HEIGHT`（`Merge20/FocusDockHeight`） | 0 | フォーカスドックの高さ（HTML `fDockH`）。0＝既定高さ（範囲なし） |
  | `OPT_MERGE20_GREP_HISTORY`（`Merge20/GrepHistory`） | （空） | Grep の検索文字列の履歴（`;` 区切り、最新 10 件まで。上限の強制は D7） |

  キーの読み手はまだない（D1〜D9 で追加する）。セッションへの保存は
  D11（設計書 §4.1(b) の `Merge20SessionDisplay` への追加）。
- 新規 `Src/Merge20/Merge20Measure.h/.cpp`（純粋モジュール、MFC・`_()`
  なし・ASCII のみ。pch.h グループ。`Merge20Classify.cpp` と同じ作法）：
  フォルダ比較の計測フック。3 つの接続点（下記）の時刻を
  `steady_clock` で記録し、最後の計測結果をメモリに保持する。
  `Merge20MeasureComputeResult()` は 3 時点からの純粋計算
  （単体テストで固定時刻により検証）。計測結果の書式は ASCII の 1 行
  （例：`Merge20 measure: folder compare 1234 ms (scan 1200 ms, list 34 ms), 100 items`）。
  時刻は atomic に保持する（Begin は UI スレッド、スキャン完了は比較
  スレッド、一覧確定は UI スレッド。順序はメッセージフローで保証される）。
- `Src/DirDoc.cpp` — 計測フック 2 件の接続。`Rescan()` の
  `m_compareStart = clock();` の直後に `Merge20MeasureBeginCompare()`、
  `DiffThreadCallback()` の `EVENT_COMPARE_COMPLETED` 分岐（`m_elapsed`
  の計算の直後）に `Merge20MeasureCompareCompleted()`。
  いずれも既存の呼び出しの末尾への追加で、既存動作に影響しない。
- `Src/DirView.cpp` — 計測フック 1 件の接続と出力。`OnUpdateUIMessage()`
  の `EVENT_COMPARE_COMPLETED` 分岐で、`Redisplay()` の直後に
  `Merge20MeasureListFinalized(ctxt.m_pCompareStats->GetTotalItems())`
  を呼び、結果があれば `RootLogger::Info` で WinMerge のログへ 1 行
  出力する（表示＞ログで確認できる）。`RootLogger::Info` は同関数内の
  `MergeLogger::LogComparisonCompleted` と同じ出力経路で、DirView.cpp
  内にも前例がある（ファイル操作のログ）。新たな出力チャネルや
  ファイル書き込みは作っていない。
- `Src/resource.h` — フェーズ 4 用リソース ID の予約範囲をコメントで
  記録（`_APS_NEXT_*` の直前）。**ID は消費していない**（D0 は予約と
  空きの確認まで）。D1〜D9 はここから割り当てる。
- `Testing/GoogleTest/Merge20/Merge20Measure_test.cpp`（新規、8 テスト）：
  純粋計算（スキャン点あり／なし／0ms）、Begin→一覧確定のフロー、
  Begin なしの無視、Begin による再開、スキャン点なしの書式、キー名
  11 件（`OptionsDef.h` のレジストリ／INI 名）。
- `Src/Merge.vcxproj`／`.filters`、
  `Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` —
  新規ファイルを登録。`Merge20Measure.cpp` は pch.h グループ
  （`Merge20Classify.cpp` と同じ）。`Merge20Measure_test.cpp` は
  PCH Use＋pch.h のテスト用エントリ。
- 変更していないもの：`Translations/WinMerge/Japanese.po`（D0 では
  UI 文字列を新設しないため。計測のログ行は開発者向け診断で ASCII
  のまま、翻訳しない）、`.github/workflows/*`、仕様書 PDF、
  `17_v17.15.html`、S5（一括承認。U-0-2 待ちのため着手しない）。

### 22.2 設計・仕様との食い違いと判断理由

1. **T-4-5 の解消：計測の接続点**。実コードで確認した結果、次の 3 点に
   決めた。開始＝`CDirDoc::Rescan()`（`m_compareStart = clock();` の
   位置。既に比較開始時刻を取っている箇所）。スキャン完了＝
   `CDirDoc::DiffThreadCallback()` の `EVENT_COMPARE_COMPLETED` 分岐
   （`m_elapsed = clock() - m_compareStart;` の位置。比較スレッドの
   終了）。一覧確定＝`CDirView::OnUpdateUIMessage()` の同イベント分岐
   で `Redisplay()` の直後（一覧の再構築とフォーカス・スクロール復元
   の後。S8 の復旧提案も同じ位置の後で呼ばれる）。既存の `m_elapsed`
   はスキャン時間しか覆わないため、一覧確定の点が D0 で新たに必要
   だった。
2. **計測は 3 点にした（設計書は 2 点）**。開始と一覧確定の 2 点だけでは
   スキャン時間と一覧再描画時間を切り分けられないため、スキャン完了
   の中間点を追加した。D10 で「どこが遅いか」を切り分けるための土台。
   スキャン完了点が記録されなかった場合（将来の経路追加時など）は
   全体をスキャンに帰属させ `listMs=0` とし、`hasScanPoint` で区別
   する。
3. **計測結果の出力方法は WinMerge のログ（`RootLogger::Info`）**。
   新たなファイル・CSV・モーダルは作らない。理由：(1) 既存の出力経路
   で、比較完了のログ（`MergeLogger::LogComparisonCompleted`）と同じ
   関数から出るため、比較完了時のログ行の直後に並ぶ、(2) 毎比較の
   ファイル書き込みや UI 表示は D0 の範囲外の副作用になる、
   (3) D10 で UI 表示を追加する際はメモリ内の最終結果
   （`Merge20MeasureGetLast()`）を使う。
4. **リソース ID の予約範囲の注意点（ソート検証で判明）**：
   ダイアログ ID は 297 からだが、数値空間はアイコン／ビットマップと
   共用で、302（`IDI_ROTATE2`）、307（`IDR_LOGO`）、310
   （`IDR_MARGIN_ICONS_PNG`）、312、316 以降が既に使われているため、
   ダイアログ ID はこれらを飛ばして割り当てること。コントロール ID は
   1745 から 8602 まで連続で空き（次の `IDC_*` は 8603）。コマンド ID
   は 38941 から 38976 まで空き（36 件。38977 は
   `IDS_SPLASH_GPLTEXT`）。ソートで見つかった重複はすべて既存の意図
   的な範囲エイリアス（`ID_COLORSCHEME_FIRST`／`IDS_COLORSCHEME_PLAIN`
   等）で、Merge20 の範囲にはない。
5. **オプションキー既定値の GoogleTest について（設計書 §5.2 D0 との
   食い違い）**：設計書は「新規キーの既定値テストを追加する」とするが、
   既定値は `OptionsInit.cpp`（`Options::Init`）にあり、同ファイルは
   UnitTests にリンクされていない（オプション初期化全体の include
   依存を引き込むため、D0 での追加は見送った）。代わりに (a) gtest で
   11 キーのレジストリ／INI 名を検証し、(b) `InitOption` の呼び出しは
   CI ビルドで型チェックされ、(c) 既定値は実機で INI／レジストリから
   読み戻して確認する（§22.4 手順 3）。テストファイルのコメントにも
   この理由を明記した。
6. **範囲の決定根拠**：`DED_ZOOM` 350–2200 は HTML の camS クランプ
   0.35–2.2、`SNIPER_FONT` 14–28 は HTML の `snFont` クランプ、
   `SNIPER_LAYOUT` 0–1 は HTML の `cols`／`rows` の 2 値、
   `FOCUS_DOCK_HEIGHT` は 0＝既定高さのため範囲なし（2 引数版の
   `InitOption`）、`GREP_HISTORY` は S4 のリスク重要語と同じ `;`
   区切り文字列とし、上限 10 件の強制は D7 に委ねた。

### 22.3 検証結果（ローカル g++ ハーネス）

純粋モジュール `Merge20Measure` を g++（C++17、`-Wall -Wextra`、
`pch.h` は空スタブ）でビルドし、ハーネスで 14 アサーション全て合格：
純粋計算（スキャン点あり：合計 1234／スキャン 1200／一覧 34ms、
スキャン点なし：合計 1234 をスキャンに帰属・一覧 0ms、0ms）、
Begin→一覧確定のフロー（42 件、書式に `scan` と `42 items` を含む）、
Begin なしの無視、Begin による再開、スキャン点なしの書式
（`scan` を含まず `3 items` を含む）。CI での gtest
（`Merge20Measure_test.cpp`、同内容＋キー名 11 件）は §22.5 参照。
vcxproj 4 ファイルの XML 整形式をパーサで確認済み。リソース ID の
ソート検証は §22.2-4 のとおり。

### 22.4 Windows 実機での確認手順（D0 受け入れテスト）

D0 は基盤のため、実機で確認できる動作は計測ログとオプション既定値の
2 点に限られる。UI の変化はないこと。

1. CI 成果物 ZIP の `WinMergeU.exe` をポータブル版（`winmerge.ini`
   あり）で起動し、2 フォルダ比較を開く。
2. 比較完了後、表示＞ログ（View＞Log）を開き、`Merge20 measure: folder compare N ms (scan S ms, list L ms), M items`
   という行があること。N は比較開始から一覧確定までの時間、M は比較
   対象の項目数。行がない場合は D0 のフックが接続されていないため
   不具合として報告すること。
3. WinMerge を終了し、`winmerge.ini` の `[Merge20]` セクションに
   11 項目が既定値で書き出されていること（`DedMode=false`、
   `DedPinned=false`、`DedZoom=750`、`TodoOnly=false`、
   `SniperOn=false`、`SniperFont=20`、`SniperLayout=0`、`Slim=false`、
   `DockHidden=false`、`FocusDockHeight=0`、`GrepHistory=`（空））。
   インストール版ではレジストリの WinMerge 設定下に同じ項目がある
   こと。配置が想定と異なる場合はそのまま報告すること（キー名は
   `Merge20/...` で固定だが、INI のセクション分割は想定）。
4. 100 ファイル規模のフォルダペアで比較し、ログ行の N ms が体感と
   大きく矛盾しないこと（目安：数秒以内の一覧確定）。500／1000
   ファイル規模での計測と記録は D10 の受け入れ条件のため、D0 では
   100 ファイル規模の記録までとする。
5. 既存のフォルダ比較・ファイル比較が従来どおり動くこと（回帰確認）。

### 22.5 CI 結果（PR #19）

- PR: https://github.com/askhim659/winmerge/pull/19（`merge20/phase4-d0` → `master`、14 コミット、未マージ）
- 実行 45710452824（2026-11-25 22:07Z）：**success**。ジョブ build 139543187614。
  - 成果物：`WinMerge-x64.zip` 26,196,063 バイト（ID 15067224671）、
    `WinMerge-x64-Setup-x64.exe` 39,690,767 バイト（ID 15067722592）。
    いずれも失効日 2027-02-23。
  - GoogleTest：**608 テスト全パス**（`[  PASSED  ] 608 tests.`。L6 までの
    600＋新規 8）。新規は `Merge20Measure` 7 件
    （`ComputeResultWithScanPoint`／`ComputeResultWithoutScanPoint`／
    `ComputeResultZeroElapsed`／`BeginListFinalizedFlow`／
    `ListFinalizedWithoutBeginIsIgnored`／`BeginRestartsMeasurement`／
    `FormatWithoutScanPointOmitsSplit`）と
    `Merge20Phase4D0Options.KeyNames` 1 件。`[  FAILED  ]` は 0 件。
  - `Merge20Measure.obj` が WinMergeU・UnitTests の両方で生成され、
    `Merge20Measure_test.obj` も生成されていることをログで確認。
    D0 のファイルに関するコンパイル警告なし（ログ中の警告は既存の
    Poco／xutility 由来）。
  - CI 失敗 0 回（初回実行で success）。
- 実行 45713207553（2026-11-25 22:26Z、§22.5 追記コミット込みの
  最終コード head）：**success**。ジョブ build 139552588574。
  - 成果物：`WinMerge-x64.zip` 26,196,152 バイト（ID 15069676070）、
    `WinMerge-x64-Setup-x64.exe` 39,689,124 バイト（ID 15070040560）。
    いずれも失効日 2027-02-23。
  - GoogleTest：**608 テスト全パス**（初回実行と同じ）。

## 23. Phase 4 D1（専用モードの骨格）— ブランチ `merge20/phase4-d1`

設計書 `merge20-phase4-design.md` §5.2 の D1 どおり。専用モードの入／出と
専用バーの骨格までとし、バー内の詳細ボタン（Prev/Next・確認内容ボタン等）と
自動非表示＋📌ピン・倍率引継ぎは D2 の範囲。C++ の変更はこの Linux VM では
コンパイルできないため、CI でのビルド＋成果物確認が検証になる。

### 23.1 変更一覧

- `Src/resource.h` — 新規コマンド `ID_VIEW_MERGE20_DEDICATED_MODE`
  38941（D0 の予約範囲から消費。ソートで空きを確認済み：38940 は
  `ID_MERGE20_LEARN_DISMISS`、次は 38977 `IDS_SPLASH_GPLTEXT`）。
  D0 の予約コメントを更新し、コマンドの空きを 38942〜38976 とした。
  `_APS_NEXT_COMMAND_VALUE` を 38941→38942 へ更新した。
- 新規 `Src/Merge20/Merge20DedBar.h/.cpp`（`CDiffViewBar` 前例の
  `CSizingControlBarCF` 派生。StdAfx.h グループ、先頭 include は
  `StdAfx.h`。`Merge.vcxproj`／`.filters` に登録。MFC バーのため
  `UnitTests.vcxproj` には登録しない＝S3 の BatchCheckDlg と同じ扱い）：
  専用バー。D1 は骨格のみ（キャプションだけで子コントロールなし）。
  `Create`／`OnCreate`（`SCBS_EDGETOP | SCBS_EDGEBOTTOM |
  SCBS_SIZECHILD`）／`OnLButtonDown`（ドック固定）／
  `OnWindowPosChanged`（非表示時に `MSG_STORE_PANESIZES` を親へ通知）／
  `SetFrameHwnd`／`UpdateResources`（`_("Dedicated Compare Mode")`）を
  `CDiffViewBar` と同じ形で持つ。
- `Src/MergeEditFrm.h/.cpp` — 専用モードの入／出：
  - `SetDedicatedMode(bool)`／`IsDedicatedMode()`（公開）、
    `EnterDedicatedMode()`／`ExitDedicatedMode()`（非公開）、
    `m_bDedicatedMode` と入場時の保存状態（5 バーの表示有無・最大化状態）。
  - 入場時：5 バー（`m_wndFilePathBar`／`m_wndLocationBar`／
    `m_wndDetailBar`／`m_wndResultBar`／`m_wndStatusBar`）の表示状態を保存
    して `ShowControlBar` で隠し、専用バーを表示し、MDI 子ウィンドウを
    `MDIMaximize` で最大化する（WinMerge に全画面機能はないため）。
    退場時：保存した表示状態と最大化状態へ戻す（最大化されていなければ
    `MDIRestore`）。
  - 入る／出る操作：表示メニューの「Dedicated Compare Mode」
    （`ID_VIEW_MERGE20_DEDICATED_MODE`、チェック付きトグル。
    `Merge.rc` の `IDR_MERGEDOCTYPE` の View ポップアップに
    「Merge Result Pane」の次へ追加）。
  - 状態は D0 の `OPT_MERGE20_DED_MODE` キーで保持する。
    `OnCreate` の末尾で同キーが true なら `PostMessage(WM_COMMAND, ...)`
    で次に開くファイル比較も専用モードで開く（作成完了後に送るため
    `PostMessage`。`LoadOptions` の `ID_WINDOW_SPLIT` と同じ手法）。
    セッション保存への配線は D11。
  - 隠したバーのレイアウトが保存状態（`Settings`）に残らないための
    3 点の防衛：(a) `OnStorePaneSizes` は専用モード中は遅延保存の
    タイマを立てない、(b) `OnTimer` の `IDT_SAVEPOSITION` は専用モード中
    は `SavePosition` を呼ばない、(c) `DestroyWindow` は破棄前に
    `ExitDedicatedMode()` で通常レイアウトへ戻す（キーには触らないため
    次のフレームの専用モード指定は残る）。
  - 専用バーの生成は `OnCreateClient`（Diff ペイン生成の直後）、
    上部へのドックと初期非表示は `OnCreate`（Diff ペインのドックの直後と
    結果バーの初期非表示の直後）。`LoadState`／`SaveState`／
    `UpdateResources`／`SetFrameHwnd` も既存バーと同じ位置に追加した。
- `Translations/WinMerge/Japanese.po` — `msgid "Dedicated Compare Mode"`
  → `msgstr "比較専用モード"` を末尾に追加（Merge20 形式のコメント付き。
  `.rc` ラベルは英語のまま、`.po` 経由で日本語化＝L1 の教訓）。
- 変更していないもの：`.github/workflows/*`、仕様書 PDF、
  `17_v17.15.html`、S5（一括承認。U-0-2 待ちのため着手しない）。

### 23.2 設計・仕様との食い違いと判断理由

1. **T-4-1 の解消：専用モードは「子ウィンドウ内のバーだけ隠す」形に
   落とした（設計書の許容する落とし先）。** 実読の結果：
   - `CMainFrame` のメニューバー／ツールバーは `ShowControlBar` で隠す
     前例がある（`OnViewMenuBar`（`MainFrm.cpp:2738`）、
     `OnToolbarSize`（同:3328））。技術的には隠せる。
   - しかし D1 では隠さない。理由：(1) メインメニューを隠すと D1 の
     出る操作（表示メニュー）が使えなくなり、専用バーは骨格のみで
     終了ボタンがまだ無い（D2 で付く）、(2) メインのステータスバー／
     タブバーには子から呼べる公開アクセサが無い（`m_wndStatusBar`／
     `m_wndTabBar` は protected で getter なし。`GetMenuBar()`／
     `GetToolbar()` はあるが残り 2 本は無い）、(3) 4 本のバーの事前状態の
     保存・復元と `OPT_SHOW_MENUBAR`／`OPT_SHOW_TOOLBAR` 等との干渉は、
     終了ボタンが付く D2 で扱うのが安全。メイン側のバーを隠す場合は
     D2 で `CMainFrame` に公開 API を追加する。
   - したがって D1 の専用モードでは、メインのメニューバー／ツールバー／
     タブバー／ステータスバーは表示されたままになる。仕様書 §3（4/4）の
     「全画面」には及ばないが、D1 の受け入れ条件（専用モードに入り、
     専用バーが表示され、通常モードへ戻れること）は満たす。
2. **T-4-4 の解消：`m_wndSplitter` の 2 行構成。** 行 0 がメインの比較
   グループ、行 1 はウィンドウ分割時（`ID_WINDOW_SPLIT`、
   `LoadOptions` が行数＞1 のときに送る）の第 2 グループであり、
   結果ペイン等ではないことを確認した（`SaveActivePane`／
   `UpdateSplitter` が行を走査している）。専用モードはスプリッタに
   触らない。
3. **専用バーの終了ボタン（「🔙通常モードへ」）は D2 へ送った。**
   設計書 §5.2 D1 の内容一覧には終了ボタンがあるが、タスクの D1 範囲は
   「バーの骨格まで、詳細ボタンは D2」であり、出る操作は表示メニューで
   用意したため、終了ボタンは D2（他のバー内ボタンと一緒）で付ける。
   D1 の段階では専用モードの出方は表示メニューのみ。
4. **ドック状態の汚染の防衛（§23.1 の (a)〜(c)）は設計書にない D1 独自の
   追加である。** 理由：`CLocationBar`／`CDiffViewBar`／
   `CMergeResultBar` は非表示時に `MSG_STORE_PANESIZES` を親へ送り、
   300ms 後に `SavePosition` が `Settings` へドック状態を保存するため、
   防衛なしでは「専用モードで隠した状態」が保存され、後から開く通常
   モードのフレームがバーの隠れた状態で開いてしまう。実読で確認した
   経路（`LocationBar.cpp:99`／`MergeResultBar.cpp:120`／
   `DiffViewBar.cpp` の `OnWindowPosChanged`）に基づく。
5. **GoogleTest の新規テストはなし。** D1 に純粋モジュールはない
   （専用バーとフレームは MFC 依存）。件数は D0 と同じ 608 件のまま。

### 23.3 Windows 実機での確認手順（D1 受け入れテスト）

1. CI 成果物 ZIP の `WinMergeU.exe` をポータブル版で起動し、2 ファイル
   比較を開く。
2. 表示メニューに「比較専用モード」があること（英語 UI では
   "Dedicated Compare Mode"）。選ぶと子ウィンドウが最大化され、
   ファイルパスバー・ロケーションバー・Diff ペイン・マージ結果バー・
   ステータスバーが隠れ、上部に専用バー（細い空のバー。D1 は骨格のみで
   ボタンはまだ無い）が表示されること。メニュー項目にチェックが付くこと。
   メインのメニューバー／ツールバー／タブバー／ステータスバーは表示された
   ままであること（T-4-1 の落とし先。仕様書の「全画面」には及ばない）。
3. 再度表示メニューから選ぶと通常モードへ戻り、隠したバー類が入場前と
   同じ表示状態に戻ること。メニューのチェックが外れること。入場／退場を
   10 回繰り返し、表示が崩れないこと。
4. 専用モードのまま WinMerge を終了→再起動→ファイル比較を開くと、
   専用モードで開くこと（`OPT_MERGE20_DED_MODE` の保持。`winmerge.ini`
   の `[Merge20]` セクションに `DedMode=true` があること）。
5. 専用モード中に別のファイル比較を開き、そのフレームで通常モードへ
   戻っても、バーの表示が正しく戻ること（ドック状態の汚染がないこと）。
   専用モードのフレームを閉じてから新しいファイル比較を開き、バー類が
   通常どおり表示されること。
6. 既存のファイル比較・フォルダ比較が従来どおり動くこと（回帰確認）。

### 23.4 CI 結果（PR #20）

- PR: https://github.com/askhim659/winmerge/pull/20（`merge20/phase4-d1` → `master`、9 コミット、未マージ）
- 実行 45718793925（2026-11-25 23:06Z）：**success**。ジョブ build 139571558351。
  - 成果物：`WinMerge-x64.zip` 26,198,061 バイト（ID 15070788931）、
    `WinMerge-x64-Setup-x64.exe` 39,691,697 バイト（ID 15070998361）。
    いずれも失効日 2027-02-23。
  - GoogleTest：**608 テスト全パス**（`[  PASSED  ] 608 tests.`。D0 と
    同数。D1 の新規テストはなし）。`[  FAILED  ]` は 0 件。
  - `Merge20DedBar.obj` が生成され、`Merge20DedBar.cpp` が WinMergeU
    の StdAfx.h グループ（`/Yu"stdafx.h"`）でコンパイルされていることを
    ログで確認。D1 のファイルに関するコンパイル警告・エラーなし
    （ログ中の警告は既存の Poco／xdiff／xutility 由来）。
  - CI 失敗 0 回（初回実行で success）。
- 実行 45720889863（2026-11-25 23:22Z、§23 追記コミット込みの最終コード
  head）：**success**。ジョブ build 139578624304。
  - 成果物：`WinMerge-x64.zip` 26,198,049 バイト（ID 15071464616）、
    `WinMerge-x64-Setup-x64.exe` 39,716,597 バイト（ID 15072887145）。
    いずれも失効日 2027-02-23。
  - GoogleTest：**608 テスト全パス**（初回実行と同じ）。
  - CI 失敗 0 回（2 回の実行とも初回で success。追記コミットによる
    3 回目の実行の結果は本記録には含めない。コードは変わっていない
    ため成果物は上記と実質同一）。

## 24. Phase 4 D2（専用バー詳細＋倍率引継ぎ）— ブランチ `merge20/phase4-d2`

設計書 `merge20-phase4-design.md` §5.2 の D2 どおり。専用バーにボタン類を
追加し、自動隠し＋📌ピン、バー非表示中の短時間表示（flash）、倍率表示と
倍率引継ぎ、R キーを実装した。C++ の変更はこの Linux VM ではコンパイル
できないため、CI でのビルド＋成果物確認が検証になる。

### 24.1 変更一覧

- `Src/resource.h` — 新規コマンド 12 件（38942〜38953）と新規コントロール
  12 件（1745〜1756）。全 define を値でソートして空きを確認済み（38942〜
  38953 と 1745〜1756 に実 ID はなく、`_APS_NEXT_*` のプレースホルダ値のみ）。
  コマンド：`ID_MERGE20_DED_EXIT` 38942、Prev/Next ファイル 38943/38944、
  確認内容 TODO/DONE/HOLD/SPECIAL/KEEP 38945〜38949、ピン 38950、未確認のみ
  38951、スナイパー 38952、倍率リセット 38953。コントロール：`IDC_MERGE20_DED_*`
 （終了/前へ/次へ/確認内容×5/ピン/未確認のみ/スナイパー/倍率）。D0 の予約
  コメントを更新し、コマンドの空きを 38954〜38976、コントロールの空きを
  1757〜8602 とした。`_APS_NEXT_COMMAND_VALUE` を 38942→38954 へ、
  `_APS_NEXT_CONTROL_VALUE` を 1745→1757 へ更新した（CRLF を保持して編集）。
- `Src/IDirDoc.h` — 新規仮想メソッド 2 件（既定実装つきのため純粋仮想ではない。
  既存の実装者は CDirDoc のみだが、将来の実装者を壊さないため）：
  `GetMerge20ReviewState(const PathContext&)`（既定 TODO）、
  `ApplyMerge20ReviewState(const PathContext&, ReviewState)`（既定 no-op）。
  ファイル比較のフレームが自分のファイルの確認内容を読書きする口。
- `Src/DirDoc.h/.cpp` — 上記 2 件の CDirDoc による override。`FindItemFromPaths`
  ＋末尾交換フォールバック（`UpdateChangedItem`／`SetMerge20Classification`
  と同じ探索。探索部分は無名名前空間の `FindMerge20ItemFromPaths` に切り出した）
  でフォルダ比較の項目を見つけ、`CDiffContext` の S2 と同じ
  `ApplyMerge20ReviewState`（出力チェック連動つき）を呼ぶ。適用後は
  `SetMerge20Classification` と同じ方法でフォルダ一覧の行を再描画する。
  該当項目がない場合（単独で開いたファイル比較、リネーム項目）は何もしない。
- `Src/MergeEditView.h/.cpp` — 新規 `CMergeEditView::ZoomTextTo(int nZoom)`。
  絶対倍率（OPT_VIEW_ZOOM 値。100〜8000 へクランプ）でフォントを設定する。
  `ZoomText(0)` は元のフォントサイズ（100%）へ戻すため、専用モードの 75%
  には使えない。フォントは `OnInitialUpdate` と同じく `theApp.m_lfDiff` から
  作り、全ペイン＋マージ結果ビューへ `SetFont` し、`OPT_VIEW_ZOOM` を保存する
  （`ZoomText` と同じ適用範囲）。
- `Src/Merge20/Merge20DedBar.h/.cpp` — 専用バーにボタン 12 個を追加
  （`OnCreate` で生成、`OnSize`／`LayoutButtons` で 1 行に左寄せ配置。
  バーの高さは `m_szHorz.cy = 32` に固定し、ボタンは高さ 24px）。
  並びは仕様書 §5.2 どおり：前へ／次へ、確認内容 5 ボタン、ピン、未確認のみ、
  スナイパー、倍率表示、通常モードへ戻る。ボタン幅は固定値で、英語と日本語の
  両方のラベルが入る寸法にした（合計約 1010px。幅の狭いウィンドウでは右端が
  クリップされる。終了ボタンは仕様の並びどおり最後のため、狭い場合は表示
  メニューから出ること。§24.2 の 7 を参照）。
  - クリックは `ON_CONTROL_RANGE(BN_CLICKED, ...)` で受け、対応する
    `ID_MERGE20_DED_*` コマンドを親フレームへ `PostMessage(WM_COMMAND, ...)`
    で転送する（バーは状態を持たない）。Prev/Next はフレーム経由でメイン
    フレームの `ID_PREVFILE`／`ID_NEXTFILE` へさらに転送される（§24.1 の
    フレームの項を参照）。
  - トグルボタン（確認内容 5、ピン、未確認のみ、スナイパー）は
    `BS_PUSHLIKE | BS_CHECKBOX`（自動トグルしない）。チェック状態はフレーム
    が `SetReviewState`／`SetPinned`／`SetTodoOnly`／`SetSniperOn` で設定する。
  - `SetPrevNextEnabled`／`SetReviewEnabled`（フォルダ比較なしでは確認内容
    ボタンを無効化）、`SetZoomText`（`"%d%%"` 表示。変更時のみ更新し、
    アイドル更新でのチラつきを防ぐ）。`UpdateResources` で全ラベルを `_()` 経由で
    設定する（`.rc` ラベルは作らないため `.po` 経由）。
  - フォントは `DEFAULT_GUI_FONT` のストックフォントを設定する
    （`CFont::FromHandle`、ストックオブジェクトのため寿命の問題なし）。
- `Src/MergeEditFrm.h/.cpp` — 専用モードの D2 動作：
  - コマンドハンドラ 8 件（`ON_COMMAND`／`ON_COMMAND_RANGE`）：終了
    （`SetDedicatedMode(false)`）、前へ／次へ（メインへ `ID_PREVFILE`／
    `ID_NEXTFILE` を PostMessage。ファイル移動はこのフレームを閉じるため、
    直接呼ばず PostMessage にした）、確認内容（`IDirDoc::ApplyMerge20ReviewState`
    に `m_pMergeDoc->m_filePaths` を渡す。S2 のキーと同一の関数・同一の出力
    連動）、ピン／未確認のみ／スナイパー（対応する `OPT_MERGE20_*` キーを
    トグルして保存）、倍率リセット（`ZoomTextTo(750)`＋`OPT_MERGE20_DED_ZOOM`
    に 750 を保存）。
  - 倍率引継ぎ：入場時に `m_nDedPrevZoom` へ現在の `OPT_VIEW_ZOOM` を保存し、
    保存済みの `OPT_MERGE20_DED_ZOOM`（既定 750）を `ZoomTextTo` で適用する。
    退場時に `m_nDedPrevZoom` へ `ZoomTextTo` で復帰する（`DestroyWindow` 経由
    の退場でも同じ）。`OnIdleUpdateCmdUI` から呼ぶ `UpdateDedBar()` が、専用
    モード中の倍率変更を `OPT_MERGE20_DED_ZOOM` へミラーする。ファイル移動は
    新しいフレームを作り、そのフレームが入場時に `OPT_MERGE20_DED_ZOOM` を
    適用するため、150% のまま次のファイルへ移る。表示位置は入場時に
    `GotoLine(0, true, ...)` で先頭へ戻す（全ペイン同期）。
  - 自動隠し＋📌ピン：`IDT_DEDHIDE`（500ms ポーリング）のタイマを入場時に開始
    し、退場時に停止する。バー表示中は、ピン固定でなければ、flash 猶予
    （1.6 秒）経過後かつカーソルがバーから 3 秒離れたら隠す。バー非表示中は、
    カーソルが子ウィンドウ上端 8px のホバーストリップに入ったら 1.6 秒表示する。
    ピン固定時は隠れない。既定は自動隠し（`OPT_MERGE20_DED_PINNED` 既定 false）。
  - `FlashDedBar()`（公開）：バーを表示し、表示猶予を 1.6 秒に延ばす。
    確認内容ボタン・ピン／未確認のみ／スナイパーのトグル・倍率リセットで呼ぶ。
    ファイル移動時は新しいフレームの入場でバーが表示されるため不要。
  - `UpdateDedBar()`（非公開。`OnIdleUpdateCmdUI` から専用モード中に呼ぶ）：
    倍率表示・ピン／未確認のみ／スナイパーのチェック・Prev/Next の有効化
    （`IDirDoc::IsFirstFile`／`IsLastFile`）・確認内容のチェックを更新する。
  - R キー：`PreTranslateMessage` の新設。専用モード中で修飾キーなしの `R`、
    かつフォーカスがフレーム自身または専用バー上（バー内のボタンを含む）の
    ときだけ `ID_MERGE20_DED_ZOOM_RESET` を PostMessage する。比較ビューや
    ファイルパスバーにフォーカスがあるときは `r` は文字入力のため通す
    （§24.2 の 4 を参照）。倍率表示ボタンも同じリセットを行うため、マウス
    でも 75% へ戻せる。
  - 退場時に `OPT_MERGE20_SNIPER_ON` を false にする（設計書 §3.1：専用
    モードを出るとスナイパーも自動 OFF。スナイパー本体は D6）。
- `Translations/WinMerge/Japanese.po` — 新規 msgid 11 件（末尾に追加。
  いずれも重複なし）：Prev→「前へ」、Next→「次へ」、TODO→「未確認」、
  DONE→「済」、HOLD→「保留」、SPECIAL→「個別対応要」、KEEP→「現状維持」、
  Pin→「ピン固定」、Unreviewed only→「未確認のみ」、Sniper→「スナイパー」、
  Back to Normal Mode→「通常モードへ戻る」。
- 変更していないもの：`.github/workflows/*`、仕様書 PDF、`17_v17.15.html`、
  `Merge.vcxproj`／`.filters`（新規ファイルなしのため登録不要）、S5（一括承認。
  U-0-2 待ちのため着手しない）。

### 24.2 設計・仕様との食い違いと判断理由

1. **T-4-1 の D2 引き継ぎ：メイン側バーの隠蔽は今回も行わない（D1 の落とし先
   を維持）。** 実読の結果、技術的には可能である（`CMainFrame::OnViewMenuBar`
   ／`OnToolbarSize`／`OnViewStatusBar`／`OnViewTabBar` がいずれも
   `ShowControlBar(&bar, false, 0)` で隠しており、`OPT_SHOW_*` に触れずに
   隠せば再起動時の表示は変わらない）。しかし行わない。理由：(1) メニューバーは
   `CMenuBar::SetAlwaysVisible` と `UpdateSystemMenu()` が絡み、隠した状態での
   Alt キー等による再表示の挙動が未確認、(2) タブバーはタブをタイトルバーに
   置くモードで `UpdateTitleBarAndTabBar()` がリバーと z 順を入れ替えており、
   隠蔽との干渉が未確認、(3) メイン側バーは全フレーム共有のため、専用モードの
   フレームが複数ある場合に「誰が隠し、誰が戻すか」の参照カウントが要るが、
   その設計は D2 の範囲外、(4) D2 の受け入れ条件にメイン側バーの隠蔽はない。
   専用モードは引き続き「子ウィンドウ内のバーだけ隠す」形で、仕様書 §3（4/4）の
   「全画面」には及ばない。後続スライス（D11 での磨き込み時）で扱う場合は、
   `CMainFrame` に参照カウント付きの公開 API（例：
   `SetMerge20DedicatedMode(bool)`）を追加し、(1)〜(3) を実機で確認すること。
2. **「通常モードではファイル移動時に 75% へリセットされること」は、WinMerge
   の従来動作としては存在しないため、追加のコードは書いていない。** 実読の結果、
   `OPT_VIEW_ZOOM` はグローバルな設定で、新しいファイル比較のビューは
   `OnInitialUpdate` でその値を読むだけであり、ファイル移動時に
   `ZoomText(0)` を呼ぶ箇所はどこにもない（`ZoomText(0)` の呼び出し元は
   表示メニューの「標準サイズ」のみ）。したがって通常モードでは従来どおり
   倍率はファイル移動をまたいで維持される。タスクの受け入れ条件のこの一文は
   HTML の `renderArena`（`resetC` 時に `camS` を `DEF_SCALE` へ戻す）の動作を
   写したものだが、フォーク側の対応物はないため、「従来動作の維持」は
   「リセット処理を追加しないこと」として解釈した。実機確認では、通常モードで
   倍率を変えてファイルを移動し、倍率が変わらないこと（従来動作）を確認すること。
   専用モード側は設計書 §3.2 どおり「入場時に `OPT_MERGE20_DED_ZOOM` を適用・
   退場時に元の `OPT_VIEW_ZOOM` へ復帰」とし、初回入場時は 75% になる。
3. **倍率の「ファイル移動時に `ZoomText(0)` のリセットを掛けない」（設計書
   §5.2 D2）は、WinMerge では「入場時に専用倍率を適用し、移動先の新しい
   フレームでも同じ専用倍率を適用する」ことで実現した。** ファイル移動は
   現在のドキュメントを閉じて新しいフレームを開く（`CDirDoc::MoveToNextFile`）
   ため、同一フレーム内でのリセット抑止という形にはならない。代わりに、
   専用モード中の倍率変更はすべて `OPT_MERGE20_DED_ZOOM` へミラーされ、
   移動先フレームの入場（D1 の `PostMessage` による再入場）がその値を適用する。
   移動元のフレームの退場（`DestroyWindow` 経由）は `OPT_VIEW_ZOOM` を入場前の
   値へ戻すが、移動先フレームの入場が直後に専用倍率を適用し直すため、見かけ上
   150% のままになる。退場→入場の間にフォントが一度戻るが、移動元フレームは
   閉じられるため画面には現れない。
4. **R キーはフォーカスがフレーム／専用バー上のときだけ有効（設計書 §5.2 D2
   および仕様書 §4(g) との食い違い）。** 比較ビューにフォーカスがあるときに裸の
   `R` を横取りすると、編集中のファイルに `r` と入力できなくなる（比較ペインは
   編集可能）。これは S2 がキー 1〜5・Space をフォルダ一覧でのみ有効にしたのと
   同じ理由である。したがって、比較ビューにフォーカスがあるときは `R` は文字
   入力のままとし、75% へのリセットは (a) 専用バー上のフォーカス時の `R` キー、
   (b) 倍率表示ボタンのクリック、(c) 表示メニューの標準サイズ（100% へ戻る。
   75% ではない点に注意）のいずれかで行う。実機確認では (a) と (b) を確認すること。
   同じ理由で、確認内容ボタンに `1:`〜`5:` の番号を付けていない（フォルダ一覧
   と違い、専用モードの比較ビューでは 1〜5 キーは文字入力のため）。
5. **スナイパートグルと未確認のみトグルは表示と状態保持のみ（設計書 §5.2 D2
   どおり）。** スナイパーの本体動作は D6、未確認のみ巡回は D3。トグル ON の
   状態は `OPT_MERGE20_SNIPER_ON`／`OPT_MERGE20_TODO_ONLY` に保存され、
   再入場時にバーへ復元される。退場時にスナイパーを OFF にするのは設計書 §3.1
   の専用モードの終了処理であり、D2 の範囲として実施した（トグルの表示状態の
   一貫性のため）。
6. **ファイル名＋(n/m) の表示は D2 では付けなかった。** 設計書 §5.2 D1 の内容
   一覧にはあるが、D2 のタスク範囲には含まれていない。専用モードではファイル
   パスバーが隠れるため現在のファイル名が見えない問題は残る。D3 で巡回と
   あわせて付けるか、D11 の磨き込みで付けることを提案する（(n/m) には
   `IDirDoc` への位置取得 API の追加が要る）。
7. **バーのボタンは固定幅・1 行・左寄せで、幅の狭いウィンドウでは右端が
   クリップされる。** 折り返しやスクロールは付けていない。終了ボタンは仕様の
   並びどおり最後のため、狭いウィンドウでは表示メニューから専用モードを出る
   こと。実用上の幅（約 1010px）は通常のファイル比較ウィンドウに収まる。
8. **GoogleTest の新規テストはなし。** D2 に純粋モジュールはない（専用バー・
   フレーム・ビューはいずれも MFC 依存）。件数は D1 と同じ 608 件のまま。
   倍率引継ぎの計算（`OPT_VIEW_ZOOM` のミラーと `ZoomTextTo` のクランプ）は
   MFC 依存のため単体テストに切り出していない。実機確認で検証する（§24.3）。

### 24.3 Windows 実機での確認手順（D2 受け入れテスト）

1. CI 成果物 ZIP の `WinMergeU.exe` をポータブル版で起動し、フォルダ比較から
   2 ファイル比較を開く。表示メニューから比較専用モードに入ること。
   （D1 の手順 2 と同じ。入場時は倍率が 75% になること。以前に専用モードで
   倍率を変えていれば、その倍率になること。表示位置は先頭行であること。）
2. 専用バーにボタンが 1 行で並ぶこと。日本語 UI では左から「前へ」「次へ」
   「未確認」「済」「保留」「個別対応要」「現状維持」「ピン固定」
   「未確認のみ」「スナイパー」「75%」「通常モードへ戻る」であること。
   現在のファイルの確認内容ボタンだけが押された表示になること（初回は
   「未確認」）。
3. 倍率引継ぎ：Ctrl＋ホイールまたは表示メニューで倍率を 150% にする
   （バー表示が「150%」になること）。「次へ」ボタンで次のファイルへ移動し、
   倍率が 150% のままであること。表示位置だけ先頭行に戻ること。「前へ」で
   戻っても 150% のままであること。
4. R キー：専用バーのボタン（例：「次へ」）をクリックしてバーにフォーカスを
   移し、`R` キーを押すと倍率が 75% に戻ること（バー表示「75%」）。比較ビュー
   をクリックしてフォーカスを移してから `R` を押すと、文字 `r` が入力される
   （リセットされない）こと。倍率表示ボタン「150%」をクリックしても 75% に
   戻ること。
5. 通常モード：専用モードを出て、倍率を 150% にしてからファイルを移動し、
   倍率が 150% のままであること（従来動作。75% へは戻らない。§24.2 の 2）。
   専用モードへ再入場すると専用倍率（前回の専用モードの倍率。初回は 75%）
   になること。通常モードへ戻ると入場前の倍率（150%）に戻ること。
6. 確認内容ボタン：「済」を押すとフォルダ一覧に戻ったときにそのファイルが
   「済」になり、出力チェックが ON になること。「保留」「現状維持」では出力
   チェックが OFF になること（S2 と同じ連動）。「未確認」に戻すと確認内容だけ
   が戻ること。フォルダ比較を開かずに単独で開いたファイル比較では、確認内容
   ボタンが無効（グレー）であること。
7. Prev/Next：フォルダ比較の先頭ファイルでは「前へ」が無効、末尾ファイルでは
   「次へ」が無効であること。「次へ」「前へ」でファイルが移動すること。
   単独で開いたファイル比較では両方とも無効であること。
8. 自動隠しとピン：既定では入場から数秒後（マウスをバーから離して 3 秒後）に
   バーが隠れること。マウスを子ウィンドウの上端（8px 以内）へ動かすとバーが
   再表示されること（1.6 秒表示され、マウスを離すと再び隠れること）。
   「ピン固定」を押すとバーが隠れなくなること。再度押すと自動隠しに戻ること。
   ピン固定の状態は次回起動しても保持されること（`winmerge.ini` の
   `[Merge20]` セクションに `DedPinned=true` があること）。
9. flash：バー非表示中に確認内容ボタンは押せないため、代わりに次で確認する。
   バーをピン固定せず、ファイル移動（Ctrl+F8）で次のファイルへ移動した直後
   はバーが表示されていること（入場時の表示）。確認操作の直後はバーが
   1.6 秒表示されたままであること。
10. トグル表示：未確認のみ・スナイパーのボタンを押すと押された表示になり、
    再度押すと戻ること。専用モードを出るとスナイパーは OFF に戻ること
    （次回入場時にスナイパーのチェックが外れていること）。未確認のみの巡回
    本体・スナイパー本体は D3・D6 のため、トグルを ON にしても巡回・スナイパー
    は起きないこと。
11. 終了ボタン：「通常モードへ戻る」で通常モードへ戻り、隠したバー類が入場前と
    同じ表示状態に戻ること。倍率は入場前の値に戻ること。
12. 既存のファイル比較・フォルダ比較が従来どおり動くこと（回帰確認）。
    特に、専用モードに入ったことがない通常のファイル比較で倍率変更・
    ファイル移動・確認内容キー（フォルダ一覧での 1〜5・Space）が従来どおり
    動くこと。

### 24.4 CI 結果（PR #21）

- PR: https://github.com/askhim659/winmerge/pull/21（`merge20/phase4-d2` → `master`、13 コミット、未マージ）
- 実行 45726828247（head aa4e94bd、§24 追記コミット込み）：**cancelled**。
  修正コミット f9bee69b のプッシュにより後続の実行に置き換えられた。
- 実行 45727327592（head f9bee69b、コマンド対応表の修正込み）：**cancelled**。
  Build ステップの実行中にキャンセルされた。ジョブログは取得できず
  （logs エンドポイントの blob が 404）、原因は特定できていない。
  失敗ジョブの再実行を API で要求したが（201）、9 分以上 queued のまま
  ジョブが始まらなかったため、本追記コミットで CI を再トリガーする。
- 実行 45729174990（head 200fab0a、本コミット）：**success**。
  ジョブ 139606608770 success。GoogleTest **608 passed / 0 failed**
  （D2 で新規テストはなく、D1 と同じ件数）。
  コンパイルエラーなし。警告は既存のもののみ（poco・xutility 等 846 件。
  D2 変更ファイル由来の警告は 0 件）。
  `Merge20DedBar.cpp` / `MergeEditFrm.cpp` / `MergeEditView.cpp` /
  `DirDoc.cpp` の .obj 生成をログで確認。
  成果物（2027-02-24 失効）:
  - `WinMerge-x64.zip` — 26,200,926 バイト、artifact ID 15076436961
  - `WinMerge-x64-Setup-x64.exe` — 39,691,272 バイト、artifact ID 15076447026
  （いずれも `WinMerge-x64.zip` / `WinMerge-x64-Setup-x64.exe` の
  2 成果物として API で存在確認済み）。

### 24.5 出荷 .po に D2 の訳文が含まれない件（CI は成功だが要対応）

実行 45729174990 の `WinMerge-x64.zip` をダウンロードして中身を検証した
結果、**D2 の日本語訳 11 件はすべて出荷された `WinMerge/Languages/Japanese.po`
に含まれていない**。ボタンは日本語 UI でも英語表示になる。
これは D2 の不具合ではなく、既知の S1〜S4 等と同じ systemic な問題で、
今回その原因を最後まで特定できた：

- repo の `Translations/WinMerge/Japanese.po`（head 200fab0a、296,054 バイト、
  2,053 msgid）には D2 の 11 件を含む全訳文がある。
- 出荷 .po は 166,451 バイト・1,811 msgid。repo との差分は
  **削除 122 件・追加 4 件**。削除された 122 件はすべて Merge20 の
  C++ 側 `_()` 文字列（S1/S2/S3/S4/S6/S8/L/D1/D2 の全文字列）。
  追加の 4 件は `%1` が `0` に置き換わった stock 文字列
  （例：`0 files will be checked`）。
- 原因：ビルド中の "Updating PO files from POT file..." ステップが
  `Strings.rc` から生成した POT だけを元に全 .po を書き直す
  （`msgmerge` 相当）。POT に無い msgid は .po から落とされる。
  Stock WinMerge は UI 文字列をすべて .rc に置く設計のため問題にならないが、
  Merge20 の各フェーズは C++ 側の `_()` 呼び出しに文字列を置いたため、
  ビルド時に全て消される。`.rc` に置いた "Dedicated Compare Mode"
  だけが生き残っていることがこの説明と一致する。

対応は D2 の範囲外（全フェーズ共通の修正が必要）。コーディネーター判断の
ための候補：(a) Merge20 の全 UI 文字列を `Strings.rc` の文字列テーブルへ
移す（上流の設計に沿うが全フェーズ分の遡及変更になる）、(b) POT 生成に
C++ 側 `_()` の走査を加える（ビルド側 1 か所の修正で全フェーズ分が直る）、
(c) CI で .po の再生成をやめ repo の .po をそのまま出荷する
（.rc との乖離管理が別途必要）。
本 PR のマージ判断にあたっては、**D2 の日本語ラベルはこのビルドでは英語表示
になる**ことを前提にすること。

### 24.6 D1 ビルドのバグ報告「専用モードのウィンドウが何も表示されない」の調査と修正

報告（D1 ビルド、実行 45724735474、2026-11-26 09:25 JST）：
「比較専用ボタンはあったけど、出てきたウインドウは何をしても何も表示されない。」

#### 24.6.1 原因の特定（コードレビューによる。実機の画面は未確認）

(a) **主因：D1 の専用バーは骨格のみで子コントロールが一切ない。**
`CMerge20DedBar::OnCreate`（master 94149386）は `SetSCBStyle` だけで、
ボタン・ファイル名・終了手段を何も作らない。専用モードに入ると
5 本のバーが隠れてこの空のバーだけが残るため、専用モードの UI として
操作できるものが何もない。D2 のボタン追加（§24.1）がこの (a) を解消する。

(b) **確実な不具合：D1 のバーは既定で 200px の高さになる。**
`CSizingControlBar` の既定値は `m_szHorz = CSize(120, 200)`
（`Src/Common/sizecbar.h:134`）。D1 の `OnCreate` は `m_szHorz`／
`m_szMinHorz` を設定しないため、ドック時の高さは
`CalcFixedLayout` の `max(m_szMinHorz.cy=32, m_szHorz.cy=200)` ＝
**200px** になる。最大化したウィンドウの上部に 200px の空の灰色帯が
でき、ファイルペインが下に押しやられる。ファイルペイン自体が
ゼロになるわけではない（200px ではクライアント領域は残る）が、
「何も表示されない」空のウィンドウという印象を強める。
さらに D2 だけでは直らない経路が 2 つある：
- **保存済みサイズ**：`SavePosition()`（`MergeEditFrm.cpp:721`）が
  `m_wndDedBar.SaveState("Settings")` を呼ぶため、D1 を 1 回でも
  使うと `sizeHorzCY=200` が保存される。フレームの `OnCreate` は
  バーの `OnCreate`（D2 で 32px を設定）の**後**に
  `m_wndDedBar.LoadState("Settings")` を呼ぶため、保存値 200 が勝つ。
- **ドラッグ**：バーは `CBRS_SIZE_DYNAMIC`＋`SCBS_SIZECHILD` で
  下辺ドラッグが可能。`OnTrackUpdateSize`（`sizecbar.cpp`）は
  ドラッグで `m_szHorz.cy` を残りクライアント高さの上限まで大きく
  でき、その値が `SaveState` で保存される。つまり利用者がバーを
  下に引けばファイルペインを覆い尽くすことも可能だった。

(c) **その他：D2 の自動隠しが同じ訴えを再現しうる。**
D2 の `IDT_DEDHIDE` タイマは、入場 1.6 秒後＋カーソル離脱 3 秒後に
バーを隠す。カーソルがファイルペイン上にある利用者にはバーが
消えたように見え、再表示の手段（ウィンドウ最上端 8px のホバース
トリップ）は画面上に何も表示されないため発見しにくい。D1 報告の
原因ではない（D1 に自動隠しはない）が、D2 ビルドで同じ訴えが
出る可能性がある。既定の自動隠しは仕様（設計書 §5.2 D2、HTML の
`dedPinned` 既定）どおりのため D2 では変えない。対策候補は D3／D11：
隠れている間の可視ホバーストリップ、または初回入場時はピン固定。

結論：(a) が主因、(b) が確実なレイアウト不具合で (a) を悪化させる。
「ファイルペインが完全にゼロになる」経路はコード上は見つからなかった
（200px ではそうならない）。実機の画面は未確認のため、§24.6.3 の
手順でユーザーに確認してもらうこと。

#### 24.6.2 修正（本 PR、追加コミット）

`Src/Merge20/Merge20DedBar.h/.cpp` のみ（共有の `sizecbar.cpp` は不変）：
- 固定高さの定数 `Merge20DedBarHeight = 32`（ボタン 24px＋上下 4px ずつ）。
- `LoadState` の override：基底の読み込み後に `m_szHorz.cy`／
  `m_szVert.cx`／`m_szFloat.cy` を 32 に強制。保存済みの 200px や
  ドラッグ済みの高さは次回フレームで復活しない。
- `CalcFixedLayout` の override：基底の結果（行交渉を含む）を呼んだ
  うえで水平ドック時の高さを 32 にクランプ。ドラッグで
  `m_szHorz.cy` が大きくなってもレイアウトには使われないため、
  実行時にバーを引き伸ばすことはできなくなる。垂直ドックは
  `EnableDocking(TOP|BOTTOM)` のため到達不能で基底のまま。
- `OnCreate` の古いコメント（「保存された高さが勝つ」）を修正。
  `Src/MergeEditFrm.cpp` のバー生成部コメントも更新（骨格のみ→D2 済み）。
- 【Wiz】ラベル：`Translations/WinMerge/Japanese.po` の
  `Dedicated Compare Mode` の訳を「比較専用モード」→
  「【Wiz】比較専用モード」に変更。この msgid は表示メニュー
  （`Src/Merge.rc:873`）とバー見出しで共有のため、両方が
  【Wiz】付きになる（ユーザーの指定例どおり）。`.rc` 由来のため
  出荷 .po でも生き残る（§24.5）。ボタン個々のラベルへの
  【Wiz】付与は W1 の全体統一に委ねる（バー自体が【Wiz】であるため）。

#### 24.6.3 検証ゲート（この節の主張の裏付け）

- **検証済み（コードレビュー＋CI）**：(a)(b) の原因は上記の実コードで
  確認した（既定値 `sizecbar.h:134`、D1 の `OnCreate` に設定なし、
  `SavePosition`→`SaveState`→`LoadState` の順序、`OnTrackUpdateSize`
  のドラッグ上限）。修正のコンパイルは、修正を含む最終 head の CI
  （§24.7）でのみ確認する。実行 45729174990（§24.4）は修正前の
  head（200fab0a）のものであり、修正のコンパイル証拠にはならない。
- **未検証（Windows 実機でのみ確認可能）**：バーの高さが実際に
  32px になること、ボタンが並び押せること、ファイルペインが
  表示されること、終了ボタンで通常モードに戻れること、ドラッグで
  バーが伸びないこと。以下は「できた」ではなく「確認手順」である。

#### 24.6.4 修正版ビルドでの実機確認手順（D1 報告の再現確認）

実行 45729174990 以後の CI 成果物（修正を含む head の実行は §24.7 参照）
の `WinMerge-x64.zip` を使い、D1 ビルドと同じ手順で確認する：

1. フォルダ比較からファイル比較を開き、表示メニュー →
   【Wiz】比較専用モードを選ぶ。
2. **直ったと言えること**：ウィンドウ上部に薄いバー（約 32px）が
   1 本だけ現れ、Prev／Next／TODO／DONE／HOLD／SPECIAL／KEEP／
   ピン／未確認のみ／スナイパー／倍率／通常モードへ戻る、の
   ボタンが並ぶこと。その下に 2 つのファイルペインが表示され、
   ファイル内容が読めること。
3. **直っていない場合**：バーが 200px ほどもある、ボタンがない、
   ファイルペインが見えない、のいずれかが残る場合は、そのままの
   画面のスクリーンショットを送ってもらうこと（特にバー部分）。
4. バーの下辺を下にドラッグしてみる：高さが変わらず 32px のまま
   であること（変わる場合はバグが残っている）。
5. 終了ボタン（「通常モードへ戻る」）を押す：通常モードのレイアウト
   （ファイルパスバー／Diff ペイン／ステータスバー等）が戻ること。
6. バーが数秒で自動的に隠れること（既定の自動隠し。仕様どおり）。
   マウスをウィンドウ最上端に持っていくとバーが再表示されること。
7. 既知の制限（修正版でも英語表示になる）：§24.5 の出荷 .po 問題
   により、ボタンの日本語ラベル（前へ／未確認／済…）はこのビルド
   では英語表示になる。表示メニューとバー見出しだけは
   「【Wiz】比較専用モード」と日本語表示される。
   これは本修正とは別の問題（.po 再生成）であり、直っていない
   場合は §24.5 を参照すること。

### 24.7 CI 結果（修正を含む最終 head、PR #21）

- 修正を含む中間コミットの実行もすべて success：
  実行 45731863843（head 4e6a8141、`.cpp` 修正）、
  実行 45731889408（head c557cb48、コメント）、
  実行 45731926482（head 8c03d779、【Wiz】.po）。
  旧 head の実行 45730694001（a0fe1b6a）は新 push により置き換えられた。
- 最終 head 53314d9d（本記録コミット）の実行 **45732017143：success**。
  ジョブ 139616089547 success。GoogleTest **608 passed / 0 failed**。
  コンパイルエラー 0 件。D2 変更ファイル由来の警告 0 件。
  `Merge20DedBar.obj`／`MergeEditFrm.obj` の生成をログで確認。
  成果物（2027-02-24 失効）:
  - `WinMerge-x64.zip` — 26,202,139 バイト、artifact ID 15077053978
  - `WinMerge-x64-Setup-x64.exe` — 39,697,210 バイト、artifact ID 15078225553
  （API とログの両方で存在・サイズ・ID を確認済み）。
  **このビルドが D1 バグ報告の修正版**。実機確認は §24.6.4 の手順で行うこと。
- 注意：実行 45727327592（head f9bee69b）のキャンセル理由は
  ログが取れず未特定のまま（§24.4）。再実行要求は queued のまま
  進まなかったが、後続の push による新実行がすべて success して
  おり、再発はしていない。

## 25. Phase 4 W2（プログラム表示名の WizMerge 化）— ブランチ `merge20/phase4-w2`

PR #22。master 94149386 から分岐。D2（PR #21）が先にマージされ §24 を使用したため、2026-11-26 に master fc0e24b3（D2 マージ済み）へリベースし、W2 の記録は §25 に付け替えた。20コミット、20ファイル。すべて表示文字列のみの変更で、行の増減なし（全ファイルで追加行数＝削除行数）。

### 25.1 変更した表示名

| 場所 | 変更前 → 変更後 |
|---|---|
| Src/Merge.rc：AFX_IDS_APP_TITLE／IDR_MAINFRAME（タイトルバー） | WinMerge → WizMerge |
| Src/Merge.rc：メニュー「&WinMerge Help」「&About WinMerge...」（3か所ずつ） | → WizMerge |
| Src/Merge.rc：About ダイアログ CAPTION「About WinMerge」、リンク「Visit the WinMerge Homepage!」 | → WizMerge |
| Src/Merge.rc：コードページダイアログ「According to WinMerge User Interface」 | → WizMerge |
| Src/Merge.rc：IDS_SPLASH_GPLTEXT、IDS_PROJECTFILES の説明、IDS_FILECHANGED_ONDISK、32-bit クリップボード比較、クラッシュ検出メッセージ | → WizMerge |
| Src/Merge2.rc：バージョンリソース FileDescription／ProductName／InternalName | WinMerge → WizMerge |
| Plugins/Strings.rc：プラグインオプションダイアログのタイトル 6件 | WinMerge Plugin Options → WizMerge Plugin Options |
| ShellExtension 2件の.rc：バージョン情報＋右クリックメニュー「Win&Merge」 | → WizMerge／「Wiz&Merge」（M をアクセラレータとして維持） |
| Installer/InnoSetup/*.iss 5件：AppName／AppVerName／VersionInfoDescription／DefaultDirName／DefaultGroupName／OutputBaseFilename／SetupWindowTitle／ショートカット名／アンインストール表示名 | WinMerge → WizMerge |
| Installer/WiX：Config.wxi 製品名、WinMerge.wxs のインストール先・スタートメニュー名・条件メッセージ | → WizMerge |
| ShellExtension/WinMergePkg/AppxManifest.xml.in：パッケージ表示名 3か所 | WinMerge → WizMerge |
| Src/res/WinMerge.exe.manifest：説明文 | WinMerge Application → WizMerge Application |
| BuildInstaller.cmd：生成物ファイル名パターン 5件 | Build\WinMerge-*- → Build\WizMerge-*- |
| BuildArc.cmd：ZIP 内フォルダ名 67か所、Setup exe 名、exe.zip 名 | WinMerge → WizMerge／winmerge-*-exe.zip → wizmerge-*-exe.zip |
| Translations/InnoSetup/English.islu・Japanese.islu：インストーラー表示文 7件／8件 | WinMerge → WizMerge |
| Translations/WinMerge/Japanese.po：上記に対応する日本語訳 16 msgid＋msgstr＋msgstrのみ 4件 | WinMerge → WizMerge |
| Translations/ShellExtension/Japanese.po：右クリックメニュー「Win&Merge」 | → Wiz&Merge |

### 25.2 変更しなかったもの（判断理由）

- exe ファイル名 WinMergeU.exe、Merge2.rc の OriginalFilename：既存ショートカット・スクリプトとの互換のため。
- レジストリキー（Software\Thingamahoochie\WinMerge、WinMerge.Project.File、App Paths 等）・INIキー・設定ファイル名：設定の移行を壊さないため。
- ProgID（WinMerge.FileCompare／WinMerge.FolderCompare）、拡張子 *.WinMerge、プラグイン変数 ${WINMERGE_HOME}、AppUserModelID、msix の Identity Name／Verb Id、.rgs の ProgID：識別子のため。
-.iss の InstallDelete に残る旧 WinMerge.lnk 削除エントリ・WinMerge (ANSI).lnk：旧版の残骸を消すためのもので、新名称に変えると旧版の掃除ができなくなるため。
-.iss の Get-AppxPackage -name WinMerge 削除コード：Identity Name を変えていないため旧パッケージ名のまま正しい。
- GitHub Actions の成果物名（WinMerge-x64.zip／WinMerge-x64-Setup-x64.exe）：.github/workflows/main.yml 由来で、トークンに workflow スコープがなく変更不可。CI のアップロードは Build/Releases/*.zip／*.exe の glob のため、生成ファイル名の変更（wizmerge-*-exe.zip、WizMerge-*-Setup.exe）は CI を壊さないことを main.yml の実読で確認済み。
- ラベル：W1 の担当範囲のため不変。
- スプラッシュ画像（Src/res/splash.png）・インストーラー画像（Art/*.bmp）・ヘルプ（.chm、Docs/Manual サブモジュール）・README.md：バイナリ／サブモジュール／文書のため未変更。
- 他言語の.po／.islu：日本語のみ更新。英語以外ではインストーラー表示文などが WinMerge のまま残る。
- Translations/WinMerge/English.pot：生成物（CreateMasterPotFile.js で再生成されるもの）のため未更新。再生成時に新 msgid が反映される見込み（未確認）。
- WinMerge.exe.manifest の assemblyIdentity name（Microsoft.Winweb.WinMerge）：識別子のため不変。

### 25.3 既知の影響

- AppName／DefaultDirName の変更により、新規インストール先は Program Files\WizMerge になり、「アプリと機能」の表示も WizMerge になる。既存 WinMerge インストールとは別アプリ扱いになるため、旧版は別途アンインストールが必要（Inno Setup の AppId 未指定時の既定動作）。
- DefaultGroupName／ショートカット名の変更により、スタートメニュー・デスクトップのショートカットは WizMerge 名になる。

### 25.4 検証

- 変更前後で行数・改行コード（LF／CRLF 混在を含む）・BOM はすべて同一（置換のみ）。`diff -rq` で変更ファイルは意図した 20件のみ。
- Japanese.po：msgid の重複は新規作成なし（変更前後とも重複は既存の Binary／Diff／Disabled／Folder／None の 5件のみ。いずれも W2 以前から存在）。引用符の対応チェック済み（msgfmt は当 VM に未インストールのため Python による簡易チェック）。
- 変更した各表示名と.po の対応は全件確認済み。残存する WinMerge は keep-list（上記 25.2）のみであることを grep で全ファイル確認済み。
- 表記ゆれなし（"WizMerge" で統一。"Wiz Merge"／"WIZMERGE" は 0件）。
- CI：実行 45732475498 は success（PR #22 の head 8a49319e4464340468c84dd90d46c7e38be131cc）。
  - GoogleTest：**608 テスト全パス**（`[  PASSED  ] 608 tests.`）。ログ中の "FAILED" 4件は
    IDS_FILESAVE_FAILED／IDS_MERGE_RESULT_SAVE_FAILED の翻訳文字列 ID で、テスト失敗ではない
    （Phase 3 L0 と同じ既知のもの）。
  - 成果物 2件：WinMerge-x64.zip（26,189,453 バイト、ID 15078440098）と
    WinMerge-x64-Setup-x64.exe（39,687,472 バイト、ID 15077049298）、
    いずれも 2027-02-24 失効。
  - CI ログからの追加証拠：BuildTranslations が新 msgid（&WizMerge Help 等）を処理したこと、
    iscc が Build\WizMerge-2.16.60.0-x64-Setup.exe／PerUser-Setup.exe を生成したこと、
    BuildArc が .\Build\Releases\wizmerge-2.16.60-32b9b35-x64-exe.zip を作成したこと。
    生成ファイル名の変更が CI を壊さないこと（OUTPAT の存在確認パスを含む）が実証された。
  - CI 失敗 0 回（初回実行で success）。

### 25.5 未検証（実機確認が必要）

- タイトルバーに WizMerge と出ること、About ダイアログ（タイトル・バージョン情報・リンク文言）、インストーラーの表示名・インストール先・ショートカット名、エクスプローラ右クリックメニュー、ZIP 内フォルダ名、.chm 以外のヘルプ表示。
- 日本語 UI で上記の日本語訳（「WizMerge について」等）が出ること。

## 26. W1（日本語統一・【Wiz】表示・列レイアウト移行 D1.5）— ブランチ `merge20/phase4-w1`

> 節番号について：D2（PR #21）が §24、W2（PR #22）が §25 を取ったため、
> 本節は §26 とした。ベースは W2 マージ後の master
> `2890f0a069a0ba701df29b4a29091afe2b3ec7c8`
> （マージコミット「Merge pull request #22 from askhim659/merge20/phase4-w2」、
> 2026-11-26 01:59:50 UTC）。W1 の範囲にプログラム名の改名は含まない。

### 26.0 要約

- フォーク追加の全 UI 文字列 368 件を監査し、日本語訳の漏れ・誤訳を修正した。
  Merge20 の欠落は 6 件のみで、他は .rc／.cpp のどちらにもある既存の訳だった。
- ただし監査で出荷物の根本原因が判明した：**リポジトリの .po を直すだけでは
  出荷される .po は直らない**。ビルド中の MergeLang ステップが
  `English.pot` を `Merge.rc`＋`Plugins/Strings.rc` から再生成し、
  `UpdatePoFilesFromPotFile.ps1` が全 .po をその .pot から書き直すため、
  `.cpp` にしかない文字列の訳は毎回削除されていた（2026-11-25 の出荷物で
  111 件欠落）。この systemic な欠陥を2つの修正で塞いだ（26.2）。
- 追加 UI の表示名に 【Wiz】 ラベルを付けた（48 件、msgstr のみ。26.3）。
  D2 の専用バー上のボタン 6 件（Prev／Next／Pin／Unreviewed only／Sniper／
  Back to Normal Mode）を含む。
- 古い列レイアウトに Merge20 の5列を末尾追加する移行（D1.5）を実装した（26.4）。
- 検証ゲート（ユーザーの 2026-11-26 指示）に従い、「できた」と言うのは
  検証証拠のあるものだけとする。証拠は (a) 再生成シミュレーション（26.2）、
  (b) CI ログでの番兵チェックの通過（26.5）、(c) CI 成果物 ZIP の中の
  `Japanese.po` の直接確認（26.5）、(d) 各 【Wiz】 ラベルのコード上の場所と
  .po の対応（26.3 の表）。Windows 実機でしか確認できない表示は
  「未検証」として 26.6 のチェックリストに残す。

### 26.1 (A) 日本語の総点検と修正

- 方法：`Src/Merge20/*.cpp|*.h` と変更ファイル
  （`DirViewColItems.cpp`、`DirView.cpp`、`PropCompare.cpp`、
  `MergeDocResultPane.cpp`、`MergeEditFrm.cpp`、`DirDoc.cpp`、
  `MergeDoc.cpp`、`MergeResultView.cpp`）の `_()`／`N_()`／`NC_()` を
  抽出し、`.rc` の Merge20 追加分（メニュー・ダイアログ）と
  `Merge20Exclude.sct` の HTA ダイアログ（`${...}` 9 件）も拾い、
  `Translations/WinMerge/Japanese.po` と突き合わせた。
  監査スクリプトは `w1-work/scripts/audit.py`（ローカル検証用。PR には
  含めない）。コンテキスト付き項目は `I18n::tr` のキー
  （`"\x01\"ctx\"msgid"`）で対応を確認した。
- 結果：**368 件を列挙。リポジトリの .po に無いものは 23 件**。
  - Merge20 の欠落 6 件：`"%1 | %2"`（S3、ステータスバーの結合書式）、
    `.rc` ダイアログの初期テキスト 4 件（`"0 files will be checked"`、
    `"Check selected (0 items)"`、`"Checked files only (0 files)"`、
    `"All files with differences (0 files)"` — 実行時に `%1` 形で上書き
    されるが、初期表示のために訳が必要）、
    `"Merge20Exclude.sct WinMerge Plugin Options"`（.sct の HTA タイトル）。
    6 件とも今回 `Japanese.po` に追加した（26.1 の表のあと）。
  - stock／デバッグの欠落 17 件：デバッグ列 7 件（`[Debug]diffcode` 等）、
    列の説明文 5 件（`Compare result`、`This item`、`Parent of current item`、
    `Link to first child of this item`、`Forward/Backward "sibling" link`）、
    `ViewCustomFlags flags`、EOL 名 3 件（`Mac`／`Unix`／`Win`）。
    いずれもフォーク以前から .po に無かったもので、W1 の対象外として
    修正しない。なお `Mac`／`Unix`／`Win` は `StringBlacklist.txt` に
    載っているため .pot にも入らない。
  - ほかに、訳が msgid と同一で意図的に未訳のもの 7 件（`OK`、`Mixed`、
    `default`、`histogram`、`minimal`、`none`、`patience`）は正しいもの
    として扱う（パーサ再現チェックでは `# same` として許容）。
- 誤訳の修正 1 件：`"Review (modified)"` の訳を
  「要判断（同一書換）」→「要判断（変更のみ）」に修正した
  （REVIEW_MOD_ONLY＝変更のみの差分。「同一書換」は誤り）。
  値はセル・CSV と共有されるため 【Wiz】 は付けない。
- Lpath／Rpath／Mpath の不具合を修正した（コーディネーター調査の事実2）。
  `Src/DirViewColItems.cpp` の `f_cols`（Lpath／Rpath）と `f_cols3`
  （Lpath／Mpath／Rpath）の計 5 行で、第2フィールド（idNameContext）が
  `nullptr` だったのを `"DirView|ColumnHeader"` にした。
  `NC_` マクロはコンテキストを捨てるためこのフィールドが唯一の経路で、
  `Path`／`Binary` は既に正しかった。`.po` には
  msgctxt `"DirView|ColumnHeader"` 付きの訳（左のフォルダー／右のフォルダー／
  中央のフォルダー）があるため、これで日本語になる。
- Phase 1 の `Merge20Exclude.sct` には設定ダイアログがあることを確認した
  （HTA `dialog1` リソース、`ShowSettingsDialog()`、`mergeApp.Translate`
  経由で訳される）。`${...}` 9 件のうちタイトル以外は訳済みだった。

### 26.2 (B) 出荷 .po の根本原因と2つの systemic 修正、番兵、再現チェック

- 根本原因（出荷物 ZIP の直接確認と CI ジョブログによる裏付け）：
  - CI 実行 45724735474（master 9414938）の成果物 ZIP 中の
    `WinMerge/Languages/Japanese.po` は 166,451 バイト・1,934 項目で、
    リポジトリの .po（294,568 バイト・2,041 項目）と比べ 111 項目が欠落し、
    4 項目（.rc ダイアログの初期テキスト）が余分にあった。
    欠落した 111 件はすべて `.cpp` にしかない Merge20 文字列だった。
  - 仕組み：`Src/MergeLang.vcxproj` の CustomBuild がビルドのたびに
    `Translations/WinMerge/CreateMasterPotFile.js` で `English.pot` を
    `Merge.rc`＋`Plugins/Strings.rc` から再生成する（`.cpp` はスキャン
    しない）。続いて `UpdatePoFilesFromPotFile.ps1` が全 .po をその .pot
    から書き直し、.pot に無い項目を削除する。`BuildTranslations.cmd` は
    その .po をコピーして `#:`／`#.` 行を除去するだけで、欠落の原因では
    ない。`.mo` は存在しない（WinMerge は .po を直接読む）。
    「.mo が古い」「BuildTranslations.cmd の問題」という仮説は誤りだった。
  - 検証：`CreateMasterPotFile.js` の Python 移植版
    （`w1-work/scripts/pot_gen.py`）で現行 master の .rc から .pot を
    再生成したところ、キー集合が出荷 .po と完全一致した（差分 0 件）。
    コミット済みの `English.pot`（1,900 項目）とは 34 件の差があり、
    いずれも Merge20 の .rc 追加分で、.pot が最後に生成されてから
    .rc が変わった分だった。つまり出荷 .po のキー集合＝再生成された
    .pot のキー集合であり、仕組みは確定した。
- 修正1：`Translations/WinMerge/UpdatePoFilesFromPotFile.ps1` の
  `CreateUpdatedPoFile` をマージ方式に変えた。`.pot` にある項目は従来
  どおり .pot 順に書き、`.pot` に無い言語固有の項目は原文のまま末尾に
  追加する（訳・コメント・参照をそのまま保持）。これにより `.cpp` に
  しかない文字列や .sct のタイトルが今後も削除されない。
  運用上の注意：削除された文字列の .po 項目は残り続けるため、文字列を
  コードから消すときは同じ変更で .po の項目も手で消すこと。
- 修正2：`Translations/WinMerge/CreateMasterPotFile.js` に .cpp 抽出を
  追加した。`../../Src` 配下を再帰走査し、`_()`／`N_()`／`NC_()` の
  リテラル文字列（隣接リテラルの結合、複数行にまたがるものを含む）を
  .pot に入れる。`.rc` の項目がキー衝突時は勝つ（リソース ID を
  `#. ` コメントとして持つため）。ファイルは相対パス順にソートして
  走査するため .pot は決定的。`#:` 参照は Src 相対パス＋行番号。
  なお .sct は JScript の構文が違うため抽出対象外で、タイトルは修正1
  のマージ方式で保持される。
  （検討したが採用しなかった案：Merge20 文字列を `Merge.rc` の
  STRINGTABLE に `IDS_MERGE20_*` として追加する方法。将来の .cpp
  文字列はやはり欠落するため systemic ではなく、未使用のリソース ID
  が 112 個増えるだけなので見送った。）
- `Translations/WinMerge/English.pot` を再生成してコミットした
  （1,900 → 2,068 項目、追加 168 件、削除 0 件）。再生成には移植版
  （`pot_gen.py --cpp-src`、全 606 ファイル走査）を使い、追加分の内訳は
  .rc の陳腐化 34 件＋Merge20 の .cpp 文字列 112 件＋stock の .cpp
  文字列 22 件（`Abort`／`Ignore`／`Redo`／`Retry`／`Try Again`／
  `Undo`／`%s`／`at position`／デバッグ列・説明文 14 件。いずれも
  コード上の正しい `_()` リテラルで、誤検出はない）。
  CI では .js が同じ .pot を再生成するはずで、`fc` による差分検出が
  働く（差があれば CI 側の .pot が使われるため自己修復される）。
- 番兵チェック：`BuildTranslations.cmd` に追加した。同梱する
  `Build\Translations\WinMerge\Japanese.po` に Merge20 の必須 msgid
  6 件（`"Review State"`、`"Include in Output"`、`"Action Category"`、
  `"Risk"`、`"Summary"`、`"Memo"`）が含まれていることを `findstr` で
  確認し、無ければ `exit /b 1` でビルドを失敗させる。
  `BuildAll.cmd` は既に失敗を伝播する（`call BuildTranslations.cmd ||
  exit /b 1`）。ASCII の msgid のみを調べるため UTF-8 ファイルでも
  問題ない。【Wiz】 ラベル自体の確認は番兵では行わず（.cmd に全角文字を
  書かない方針）、成果物 ZIP の直接確認（26.5）で行う。
- パーサ再現チェック：`CLanguageSelect::LoadLanguageFile` の読み取り規則
  （素の項目→素のキー、msgctxt 付き→`"\x01\"ctx\"msgid"` キー、空 msgid
  は格納しない、msgstr が空なら msgid を使う、後勝ち、引用符なし行で
  フラッシュ、`unslash` の escapes）を再現した
  `w1-work/scripts/po_check.py` で、編集後の `Japanese.po` に対し
  監査で列挙した 368 件を検査した。結果：**351/368 件が日本語に解決**。
  残り 17 件は 26.1 の stock／デバッグの欠落（対象外）で、Merge20 の
  欠落は 0 件。意図的に未訳の 7 件と `"%1 | %2"`（書式のみ）は
  `# same` として許容した。
- 再生成シミュレーション（検証ゲート (a) の証拠）：
  `.pot` 生成の移植版と `.po` 更新の移植版（`w1-work/scripts/po_raw.py`、
  修正前後の両モードを持つ）で、編集後の `Japanese.po` に対し4通りを
  実行し、112 件（111 件＋`"%1 | %2"`）の残存と 42 件の 【Wiz】 ラベルを数えた。

  | 組合せ | 項目数 | 112 件の残存 | 42 件の 【Wiz】 |
  |---|---|---|---|
  | BEFORE（修正前の .pot＋修正前の更新） | 1,935 | 0/112（欠落を再現） | 21/42（.rc 分のみ残存） |
  | PS1ONLY（.ps1 修正のみ） | 2,048 | 112/112 | 42/42 |
  | JSONLY（.js 修正のみ） | 2,061 | 112/112 | 42/42 |
  | BOTH（両方の修正） | 2,062 | 112/112 | 42/42 |

  BEFORE のキー集合は出荷 .po のキー集合と完全一致し、欠落の再現を
  確認した。PS1ONLY と JSONLY はどちらか一方でも欠落を防げるが、
  .sct タイトルは .ps1 修正でのみ残る（.js は .sct を走査しないため、
  JSONLY では欠落する）。両方やることで 112 件＋42 件＋.sct タイトル
  のすべてが残る。シミュレーションの入出力は `w1-work/sim/` にある。

### 26.3 (C) 【Wiz】表示（全追加 UI）

- 方針：【Wiz】 ラベルは日本語 .po の msgstr の先頭に付けるだけとし、訳自体
  は変えない（誤訳があれば別途修正）。英語 msgid と .rc のラベルは
  ASCII のまま変更しない（.rc は ASCII 前提で、日本語・全角括弧を直書き
  しないこと。英語 UI では 【Wiz】 は付かないが、ユーザーは日本語 UI を
  使うため問題ない）。
- 付与一覧（48 件。msgstr は編集後の `Japanese.po` から取った正確な値。D2 の専用バー上のボタン 6 件を含む）：
| `Review State` | 【Wiz】確認内容 | Src/DirViewColItems.cpp:73 (COLHDR_REVIEWSTATE) |
| `Include in Output` | 【Wiz】出力 | Src/DirViewColItems.cpp:74 (COLHDR_OUTPUT) |
| `Action Category` | 【Wiz】対応区分 | Src/DirViewColItems.cpp:75 (COLHDR_ACTION) |
| `Risk` | 【Wiz】リスク | Src/DirViewColItems.cpp:76 (COLHDR_RISK) |
| `Summary` | 【Wiz】要約 | Src/DirViewColItems.cpp:77 (COLHDR_SUMMARY) |
| `Memo` | 【Wiz】メモ | Src/DirViewColItems.cpp:78 (COLHDR_MEMO) |
| `Ignored diffs` | 【Wiz】無視差分 | Src/DirViewColItems.cpp:72 (COLHDR_EXCLUDED) |
| `Batch check...` | 【Wiz】一括チェック… | Src/DirView.cpp:897 (context menu) |
| `Export diff list as CSV...` | 【Wiz】差分一覧 CSV… | Src/DirView.cpp:901 (context menu) |
| `Save session` | 【Wiz】セッションを保存 | Src/DirView.cpp:905 (context menu) |
| `Restore session...` | 【Wiz】セッションを復元… | Src/DirView.cpp:907 (context menu) |
| `Edit audit memo...` | 【Wiz】監査メモを編集… | Src/DirView.cpp:916 (context menu) |
| `Learning...` | 【Wiz】学習… | Src/DirView.cpp:922 (context menu) |
| `Check all files under folder` | 【Wiz】配下をすべてチェック | Src/DirView.cpp:926 (context menu) |
| `Uncheck all files under folder` | 【Wiz】配下のチェックをすべて外す | Src/DirView.cpp:928 (context menu) |
| `Apply Learning Suggestion` | 【Wiz】🎓 学習候補を適用 | Src/Merge.rc:83 (menu ID_MERGE20_LEARN_APPLY) |
| `Dismiss Learning Suggestion` | 【Wiz】候補を却下 | Src/Merge.rc:84 (menu ID_MERGE20_LEARN_DISMISS) |
| `Apply Learning Suggestion (%1, %2, %3)` | 【Wiz】🎓 学習候補を適用（%1・%2・%3件） | Src/MergeResultView.cpp:588 (dynamic menu label) |
| `Batch Check` | 【Wiz】一括チェック | Src/Merge.rc:3021 (dialog IDD_MERGE20_BATCH_CHECK) |
| `Export Diff List as CSV` | 【Wiz】差分一覧 CSV 出力 | Src/Merge.rc:3040 (dialog IDD_MERGE20_CSV_EXPORT) |
| `Edit Audit Memo` | 【Wiz】監査メモの編集 | Src/Merge.rc:3053 (dialog IDD_MERGE20_MEMO) |
| `Learning (merge judgment history and sharing)` | 【Wiz】🎓 学習機能（マージ判定の蓄積と共有） | Src/Merge.rc:3064 (dialog IDD_MERGE20_LEARN) |
| `Review states to check` | 【Wiz】チェックする確認内容 | Src/Merge.rc:3024 (batch dialog LTEXT) |
| `Check all displayed` | 【Wiz】表示分すべてチェック | Src/Merge.rc:3032 (batch dialog button) |
| `Clear all checks` | 【Wiz】全解除 | Src/Merge.rc:3033 (batch dialog button) |
| `Check selected (%1 items)` | 【Wiz】選択分を一括チェック（%1件） | Src/Merge20/Merge20BatchCheckDlg.cpp:70 (dynamic label) |
| `%1 files will be checked` | 【Wiz】%1件のファイルが対象です | Src/Merge20/Merge20BatchCheckDlg.cpp:71 (dynamic label) |
| `Range to export` | 【Wiz】出力する範囲 | Src/Merge.rc:3043 (CSV dialog LTEXT) |
| `Checked files only (%1 files)` | 【Wiz】選択分のみ（%1件） | Src/Merge20/Merge20CsvExportDlg.cpp:36 (dynamic radio) |
| `All files with differences (%1 files)` | 【Wiz】差分全件（%1件） | Src/Merge20/Merge20CsvExportDlg.cpp:37 (dynamic radio) |
| `Toggle ON/OFF` | 【Wiz】ON/OFF切替 | Src/Merge.rc:3069 (learn dialog button) |
| `Sources (per import)` | 【Wiz】出典（取込単位） | Src/Merge.rc:3072 (learn dialog LTEXT) |
| `Delete selected sources` | 【Wiz】選択したソースを削除 | Src/Merge.rc:3074 (learn dialog button) |
| `Export merged sources` | 【Wiz】🔗 選択したソースを統合して出力 | Src/Merge.rc:3075 (learn dialog button) |
| `Learned rules (top 50)` | 【Wiz】学習したルール（上位50件） | Src/Merge.rc:3076 (learn dialog LTEXT) |
| `Export learning data (JSON)` | 【Wiz】📤 学習データを出力（JSON） | Src/Merge.rc:3079 (learn dialog button) |
| `Import learning data` | 【Wiz】📥 学習データを取込（蓄積型） | Src/Merge.rc:3080 (learn dialog button) |
| `Clear learning data` | 【Wiz】🗑️ 学習データをクリア | Src/Merge.rc:3081 (learn dialog button) |
| `Dedicated Compare Mode` | 【Wiz】比較専用モード | Src/Merge.rc:873 (View menu); Src/MergeEditFrm.cpp:142 |
| `Enable learning` | 【Wiz】🎓 学習機能 | Src/Merge.rc:2727 (compare options checkbox) |
| `When off, learning suggestions and recording stop.` | 【Wiz】OFF にすると候補提示と記録を停止します。 | Src/Merge.rc:2728 (compare options LTEXT) |
| `%1 displayed, %2 checked` | 【Wiz】表示 %1 件中 %2 件チェック | Src/DirView.cpp:3289 (status bar) |
| `Prev` | 【Wiz】前へ | Src/Merge20/Merge20DedBar.cpp:331 (dedicated bar button) |
| `Next` | 【Wiz】次へ | Src/Merge20/Merge20DedBar.cpp:333 (dedicated bar button) |
| `Pin` | 【Wiz】ピン固定 | Src/Merge20/Merge20DedBar.cpp:349 (dedicated bar button) |
| `Unreviewed only` | 【Wiz】未確認のみ | Src/Merge20/Merge20DedBar.cpp (dedicated bar toggle) |
| `Sniper` | 【Wiz】スナイパー | Src/Merge20/Merge20DedBar.cpp (dedicated bar toggle) |
| `Back to Normal Mode` | 【Wiz】通常モードへ戻る | Src/Merge20/Merge20DedBar.cpp (dedicated bar exit button) |
- 除外・代替の判断（勝手に省略しないための記録）：
  - 確認内容の 5 状態名（未確認／済／保留／個別対応要／現状維持）と
    対応区分・リスクの値そのものはセル内の値で、CSV の値・ステータス表示
    とも共有されるため、値自体には 【Wiz】 を付けず、列ヘッダ・ダイアログの
    チェックボックス見出し・メニュー項目側で 【Wiz】 を付けた。
  - 存在区分名（旧のみ／新のみ／両方あり 等）と「Not calculated」、
    戦略名（Insert old／Keep new／Replace with old／Both old and new）、
    「Exact match」／「Feature match」、学習モーダルのソース名・動的
    ステータス行、S4 の要約文、メッセージ本文、CSV 専用ヘッダ
    （Directory／File name／Full path／Existence category／Total diffs／
    Missing lines／Changed lines／Added lines）、列の説明文（COLDESC_*）、
    stock の OK／Cancel／Close（全ダイアログと共有）、stock の
    「Ignored Diff」列（Merge20 の Sexcluded 列とは別物）には付けない。
    いずれもセル内の値・共有ラベル・CSV の値であり、ヘッダやダイアログ
    名で十分に目立つため。
  - 狭い列（出力・リスク等）もユーザーの例示どおりヘッダに 【Wiz】 を付けた。
  - JSON のファイル名・キー、ログ行（`Merge20 measure...`）、開発者向け
    診断文には付けない。
  - ダイアログ内の説明文（LTEXT の長文）には付けない（タイトルとボタンで
    十分に目立つため）。
  - 副作用として CSV ヘッダ行も 【Wiz】 付きになる（列ヘッダと msgid を共有
    するため）。CSV の値は 【Wiz】 なしのまま。
  - L3/L4 で既に絵文字マーカー（🎓／🔗／📤／📥／🗑️）が付いていた訳には、
    そのまま先頭に 【Wiz】 を付けた（例：【Wiz】＋🎓 学習候補を適用）。二重に
    なるが、訳自体を変えない方針による。
- 今後のスライス（D3〜D11、S5／S7）への約束事：追加 UI の表示名には
  同じ方針で 【Wiz】 を付けること。新しい `_()`／`N_()`／`NC_()` 文字列を
  追加したら、(1) `Japanese.po` に訳を追加すること（.js の .cpp 抽出で
  .pot に入るため、訳があれば出荷 .po に残る）、(2) 表示名・ラベルには
  msgstr に 【Wiz】 を付けること（値そのもの・JSON・ログは除く）、
  (3) 文字列を削除したら .po の項目も同じ変更で消すこと
  （.ps1 のマージ方式では削除された文字列の項目が残り続けるため）。
- 未確認事項：プログラム名が 2026-11-26 に WizMerge と決まったが、
  ラベルを WizMerge にするかはユーザーに未確認（2026-11-26 に質問済み）。
  確認が取れたら msgstr の 【Wiz】 を WizMerge に機械的に置換できる。
  本節では指示どおり 【Wiz】 で実装した。

### 26.4 (D) 列レイアウト移行（D1.5）

- 新規オプションキー `Merge20/ColumnLayoutVersion`（整数、既定 0、現行
  バージョン＝1）を `OptionsDef.h`／`OptionsInit.cpp` に追加した
  （D0・S8・L1 と同じパターン。`InitOption(..., 0, 0, 1)`）。
- `DirViewColItems::MigrateMerge20Columns()`（`DirViewColItems.cpp` 末尾、
  宣言は `DirViewColItems.h`）を追加した。Merge20 の新列 5 つ
  （Sreview／Soutput／Saction／Srisk／Ssummary）のうち既定 order ≧ 0
  かつ現在非表示のものを、既定 order 順に末尾へ追加する。stock 列には
  一切触れず、既定で非表示の Smemo／Sexcluded も対象外。
  `m_dispcols < 0` や `m_dispcols >= m_numcols` のときは何もしない。
- `CDirView::OnInitialUpdate` で `LoadColumnOrders()` の直後に呼ぶ。
  保存済みバージョン＜1 なら移行を実行し、追加があれば列順を即保存し、
  バージョンを 1 に保存する。移行を `LoadColumnOrders()` の中に
  無条件に入れていないのは、S8 のセッション復元
  （`Merge20ApplyColumnSettings`）が `LoadColumnOrders()` を直接呼ぶため。
- 制限事項：バージョンキーは 2-way／3-way 共通の単一キーのため、アップ
  グレード後に先に開いたほうのレイアウトだけが移行され、もう一方の
  既存レイアウトは移行されない。将来どちらも移行したい場合は
  キーを 2-way／3-way 別に分けること。

### 26.5 CI 結果（PR #23）

- PR：https://github.com/askhim659/winmerge/pull/23
  （ブランチ `merge20/phase4-w1`。マージはしないこと。コーディネーターが検証してマージする。）
- 旧ベース（master `94149386`、head `094cebcf6006c61c2f382fd04a08a2bf0cabf4ee`、11 コミット）での CI 実行 45733502310（`pull_request`、2026-11-26 01:16:43 UTC 更新）：**結論 success**。ジョブ 139621061666（`build`）：completed／success。GoogleTest：`[  PASSED  ] 608 tests.`。成果物：`WinMerge-x64.zip`（ID 15078491883、26,282,223 バイト、失効 2027-02-24）、`WinMerge-x64-Setup-x64.exe`（ID 15077904099、39,713,377 バイト、失効 2027-02-24）。ジョブログで MergeLang ステップ（`Creating POT file from Merge.rc...`、`Updating PO files from POT file...`、`All PO files updated`）と番兵チェック（`Checking bundled Japanese.po for Merge20 strings...`／`Merge20 strings present in the bundled Japanese.po.`）の通過を確認。成果物 ZIP の直接確認：出荷 `Japanese.po` は 180,094 バイト・2,070 項目で、必要 113 件の欠落は 0 件、Wiz 42/42 件が一致、.sct タイトルも存在した。
- 記録のみのコミット後の最終 head（`cacf6f1d795a00725e6c0a128817c8ea85a5f67d`）での CI 実行 45734915627：**結論 success**。成果物：`WinMerge-x64.zip`（ID 15078562969、26,282,198 バイト、失効 2027-02-24）、`WinMerge-x64-Setup-x64.exe`（ID 15079121097、39,714,469 バイト、失効 2027-02-24）。
- D2 マージ後のリベース（本節のベース `fc0e24b3`、head `ee3091fa8808c13d72b71f0614e66d9fbb081603`、11 コミット）での CI 実行 45737590691（`pull_request`、2026-11-26 01:53:11 UTC 更新）：**結論 success**。ジョブ 139634462743（`build`）：completed／success。GoogleTest：`[  PASSED  ] 608 tests.`（ジョブログ 11955 行目）。成果物：`WinMerge-x64.zip`（ID 15079408660、26,288,822 バイト、失効 2027-02-24T01:42:45Z、SHA256 `cc9a2a032d87ab7141409f27fddc06fdfaf5724d3468bc6646206abb6ea4af20`）、`WinMerge-x64-Setup-x64.exe`（ID 15078974601、39,723,321 バイト、失効 2027-02-24T01:42:45Z）。ジョブログで MergeLang ステップ（`Creating POT file from Merge.rc...` 8565 行目、`Updating PO files from POT file...` 8698 行目、`All PO files updated` 8740 行目）と番兵チェック（`Prepare Translations...` 11956 行目、`Checking bundled Japanese.po for Merge20 strings...`／`Merge20 strings present in the bundled Japanese.po.` 11957〜11958 行目）の通過を確認。成果物 ZIP の直接確認：外側 ZIP の中身は `winmerge-2.16.60-a7c96cd-x64-exe.zip` 1 件。内側の `WinMerge/Languages/Japanese.po` は 180,598 バイト・2,081 項目で、必要 124 件（旧出荷物で欠落していた 111 件＋`"%1 | %2"`＋.sct タイトル＋D2 の 11 件）の欠落は **0 件**。Wiz 48/48 件が存在し msgstr はリポジトリの .po と完全一致（D2 のボタン 6 件も含む。`"Prev"`→`【Wiz】前へ`、`"Sniper"`→`【Wiz】スナイパー`、`"Back to Normal Mode"`→`【Wiz】通常モードへ戻る`。`"TODO"` は未確認のまま）。`.sct` タイトルも存在する。リポジトリの .po のキーに対する出荷 .po の欠落は 0 件。出荷 .po にだけある 22 件は stock の .cpp 文字列（未訳のまま追加）。
- リベース時の注意点（教訓）：ブランチを D2 マージ後の master（`fc0e24b3`）へ force-push した直後、PR #23 は GitHub により自動クローズされた（head が一時的に base と同一になり、変更 0 件とみなされたため）。その後の 11 コミットは PR に同期されず CI も走らなかった。PR を API で再開（`PATCH /pulls/23 {"state": "open"}`）したところ head が `ee3091fa` に同期され、CI 実行 45737590691 が走った。**force-push でブランチを base と同一にする操作は PR を自動クローズさせる**ため、今後のリベースでは base と同一の状態を作らず、コミットを載せてから push するか、クローズされたら再開すること。
- W2 マージ後のリベース（本節のベース `2890f0a0`、head `46070557399fa2626788ef3cd528dedcfc2b14f1`、11 コミット）での CI 実行 45740686282（`pull_request`、2026-11-26 02:19:51 UTC 更新）：**結論 success**。ジョブ 139644638162（`build`）：completed／success。GoogleTest：`[  PASSED  ] 608 tests.`（ジョブログ 11972 行目）。成果物：`WinMerge-x64.zip`（ID 15081142942、26,302,346 バイト、失効 2027-02-24T02:10:54Z、SHA256 `166d268aa2459c59065db41ffd0afe9a608a30b61ba8530dbd24b1d8c0aedc46`）、`WinMerge-x64-Setup-x64.exe`（ID 15081736271、39,723,190 バイト、失効 2027-02-24T02:10:54Z）。ジョブログで MergeLang ステップ（`Creating POT file from Merge.rc...` 8575 行目、`Updating PO files from POT file...` 8708 行目、`All PO files updated` 8750 行目）と番兵チェック（`Prepare Translations...` 11973 行目、`Checking bundled Japanese.po for Merge20 strings...`／`Merge20 strings present in the bundled Japanese.po.` 11974〜11975 行目）の通過を確認。成果物 ZIP の直接確認：外側 ZIP の中身は `wizmerge-2.16.60-85843ec-x64-exe.zip` 1 件（W2 の改名により `winmerge-` から `wizmerge-` になった）。内側の `WinMerge/Languages/Japanese.po` は 181,170 バイト・2,087 項目で、必要 140 件（旧出荷物で欠落していた 111 件＋`"%1 | %2"`＋.sct タイトル＋D2 の 11 件＋W2 の改名 16 件）の欠落は **0 件**。Wiz 48/48 件が存在し msgstr はリポジトリの .po と完全一致。`.sct` タイトルも存在する。W2 の改名も確認（`"About WizMerge"`→`WizMerge について`、`"&About WizMerge..."`→`WizMerge について(&A)...`）。リポジトリの .po のキーに対する出荷 .po の欠落は 0 件。出荷 .po にだけある 28 件は、.pot にあって .po に無い文字列で、msgstr が空で追加されたもの（.cpp の "WinMerge" 版 5 件、stock の .cpp 文字列 22 件、 `"WizMerge"` 1 件。空 msgstr は実行時に msgid にフォールバックするため表示は英語）。
- 補足：D2 リベース後の記録のみのコミット（`ea420cbfe576`）での CI 実行 45739016169 も **結論 success**（成果物あり）。本節の最終 head ではないため詳細は省略する。
- リベース時の注意点（教訓、D2 リベースの続き）：ブランチを base と同一にする force-push は PR を自動クローズさせる。今回は初回コミットを `commit_on_parent.py` で base（`2890f0a0`）の上に直接作り、ブランチが base と同一になる瞬間を作らなかったため、PR #23 は open のまま維持され（head `46070557399f`、mergeable: True、11 コミット、11 ファイル）、CI 実行 45740686282 が即座に走った。

### 26.6 Windows 実機で確認してもらう項目（未検証）

- 以下の項目は Linux VM では確認できないため、実機での確認が必要。
  「できた」ではなく「未検証」として残す。
- 1. 成果物 ZIP を丸ごと新しいフォルダに展開し（exe だけ差し替えないこと）、
  `Languages\Japanese.po` を開いて「確認内容」で検索すること。
  見つからなければ Languages フォルダが古いので ZIP を丸ごと再展開すること。
  見つかるのに英語表示なら実行時バグとして報告してもらうこと。
- 2. フォルダ比較で「列の表示」ダイアログを開き、【Wiz】 付き日本語名
  （【Wiz】 確認内容、【Wiz】 出力、【Wiz】 対応区分、【Wiz】 リスク、【Wiz】 要約、【Wiz】 メモ、
  【Wiz】 無視差分）が出ること。左／右／中央のフォルダーも日本語になること。
- 3. 古い `winmerge.ini`（またはレジストリ）のまま起動し、フォルダ比較で
  5 列（確認内容・出力・対応区分・リスク・要約）が末尾に追加されること。
  stock 列の順序が変わっていないこと。
- 4. 表示メニューの専用モードが「【Wiz】 比較専用モード」になっていること。
- 5. 一括チェック・CSV 出力・監査メモ・学習の各ダイアログのタイトルと
  ボタンに 【Wiz】 が付いていること。比較オプションの学習チェックボックスが
  「【Wiz】 🎓 学習機能」になっていること。
- 6. フォルダ比較のステータスバーに「表示 N 件中 M 件チェック」と出ること
  （【Wiz】 付きの表示）。
- 7. 既存のファイル比較・フォルダ比較が従来どおり動くこと（回帰確認）。
- 8. 学習候補のメニュー項目に 【Wiz】 が付いていること（学習データを記録して
  から候補を表示させた場合）。

### 26.7 D3 以降への引き継ぎ・残課題
- W2 の不整合（W1 の範囲外。コーディネーターへの報告事項）：W2 は
  .rc／.po の表示名を "WinMerge"→"WizMerge" に変えたが、.cpp／.sct の
  ソース文字列は変えていない。master `2890f0a0` で確認した限り、
  .cpp に残る "WinMerge" の `_()` 文字列は 6 件以上ある
  （`"32-bit WinMerge does not support Clipboard Compare"`
  ClipboardHistory.cpp:180、 `"WinMerge detected a previous crash..."`
  CrashLogger.cpp:411、 `"WinMerge comes with ABSOLUTELY NO WARRANTY..."`
  MergeApp.cpp:154、 `"Another application updated...since WinMerge loaded it..."`
  HexMergeDoc.cpp:240／MergeDoc.cpp:1133、
  `"WinMerge Project Files (*.WinMerge)|*.WinMerge||"`
  MainFrm.cpp:2868／OpenView.cpp:1254、
  `"Cannot find filter template!...WinMerge/Filters Folder..."`）。
  .po の msgid は "WizMerge" 化されたため、これらの .cpp 経路の検索は
  外れて英語表示になる（W2 以前は日本語だったものが英語に戻る）。
  同様に 8 個の .sct ファイルの `<title>` も "WinMerge Plugin Options"
  のままで、.po の 6 件（CompareMSExcelFiles／CompareMSWordFiles／
  CompareMSPowerPointFiles／CompareMSVisioFiles／PrediffLineFilter／AI.sct）
  は "WizMerge Plugin Options" に変えられたため、.sct タイトルの訳も
  外れる。W1 の `.pot` 再生成では両方の版が .pot に入るため、出荷 .po には
  "WinMerge" 版が未訳で追加される（表示は英語のまま）。正しい修正は
  W2 のフォローアップとして .cpp／.sct 側も "WizMerge" 化すること。
  なお W1 の新規項目 `"Merge20Exclude.sct WinMerge Plugin Options"` は
  .sct ファイルが "WinMerge" のままのため、あえて "WinMerge" のままに
  した（.sct 側が変わったら合わせて変えること）。
- W2 のリベースで `Japanese.po` のアンカー 1 件を変更した：W1 の新規項目
  （Merge20Exclude.sct のタイトル）の挿入位置アンカーを
  `"PrediffLineFilter.sct WinMerge Plugin Options"` から
  `"PrediffLineFilter.sct WizMerge Plugin Options"` に変えた
  （W2 の改名に追従。挿入する項目自体は上記の理由で "WinMerge" のまま）。
- `resource.h` は W2 で変更なし（新規リソース ID なし）。W1 は新規リソース
  ID を使わないため重複はない。`Merge.rc` は W1 では触らない
  （W2 がタイトルバー・メニュー・About 等を WizMerge 化済み）。

- D2（PR #21）と W2（PR #22）はマージ済み。本節は W2 マージ後の master
  （`2890f0a0`）にリベースしたもので、D2 の専用バー上のボタン 6 件への
  【Wiz】 付与と、D2 の 11 文字列の `English.pot` への追加、W2 の改名への
  追従（アンカー 1 件の変更）を行った。なお D2 側が既に
  `"Dedicated Compare Mode"` に 【Wiz】 を付けていたため、W1 では二重付けを
  避けてスキップした（26.3 の表を参照）。D2 の新規文字列は .js の .cpp
  抽出で .pot に入るため、訳があれば出荷 .po に残る。
- 専用モードのウィンドウが空に見える件（ユーザーの 2026-11-26 09:25 報告）
  は D2 の領域であり、W1 では扱わない。
- `.po` の `#.` コメントについて：.cpp 文字列の訳は、ビルド後の .po では
  `#. ` が空（または .pot 由来）になる。`.po` に手書きした
  `#. Merge20 ...` コメントは、.pot に入った文字列についてはビルド時に
  .pot 由来のものに置き換わる。訳自体は保持される。
- `.ps1` のマージ方式では、削除された文字列の .po 項目が残り続ける。
  文字列を削除する変更では .po の項目も一緒に消すこと（26.2 参照）。
- 監査子エージェントが別途 Phase 1〜4 の監査を行っている。W1 の修正内容
  は本節が W1 自身の検証レポートである。

## 27. Phase 4 D3（未確認のみ巡回）— ブランチ `merge20/phase4-d3`

PR #24。2026-11-26 に master 0aefc6dd（W1 マージ済み）へリベースした（§27.8）。リベース前は master fc0e24b3（PR #21＝D2 マージ済み）から分岐し、25コミット＋記録コミット＋マージコミット（W2 対応）＋訂正コミット＋最終記録コミットで進んでいたが、リベースで 1ファイル＝1コミットの 20コミット＋記録コミット 1件に載せ直した。21ファイル。設計書 §3.9・§5.2 D3、仕様書 §4(g)・§5.4。W1＝§26、W2＝§25。

### 27.1 変更一覧

| コミット | ファイル | 内容 |
|---|---|---|
| bd61feba45 | Src/Merge20/Merge20Patrol.h | 純粋モジュール（新規）。`Merge20FindNextTarget`／`Merge20HasOtherTarget`／`Merge20CountTargets`／`Merge20OrdinalOfTarget`。MFC 不使用・ASCII のみ・`vector<bool>` API |
| 69ae7e304d | Src/Merge20/Merge20Patrol.cpp | 同実装。両端で折り返す。唯一の対象は自分自身を返す。対象なしは -1。範囲外の現在位置は -1 扱い。方向が ±1 以外は -1 |
| 7082ff0f69 | Testing/GoogleTest/Merge20/Merge20Patrol_test.cpp | 同テスト 8件（後述の Node 真値で期待値を照合） |
| 9aeec3c698 | Src/Merge.vcxproj | Merge20Patrol 登録（pch.h グループ） |
| 77238c640d | Src/Merge.vcxproj.filters | 同フィルタ登録 |
| 82f88e51e4 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | Merge20Patrol＋テスト登録 |
| 1018e265ff | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同フィルタ登録 |
| 4bbf335e82 | Src/resource.h | `IDC_MERGE20_DED_FILE_INFO` = 1757 を割当て（全 define を値でソートして空きを確認。`_APS_NEXT_CONTROL_VALUE` のプレースホルダだけが 1757 を持っていた）。`_APS_NEXT_CONTROL_VALUE` を 1757→1758 へ。D0 予約コメントを更新。resource.h は行末が混在（D0 コメント部は LF、他は CRLF）しており、最初の編集失敗の原因（§27.7）。リベース前の空コミット 27b9a659 はリベースで削除した（内容がなかったため） |
| b5c7499396 | Src/Merge20/Merge20DedBar.h | 専用バーのファイル情報ラベル `CStatic m_stFileInfo`（バーの先頭、幅 300px、`SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS | SS_CENTERIMAGE`） |
| 9492b2a0a8 | Src/Merge20/Merge20DedBar.cpp | `SetFileInfo()`（同一文言なら何もしない）。バー全幅は約 1310px（D2 で指摘済みの狭いウィンドウでの右端クリッピングは残る） |
| e75999fdef | Src/IDirDoc.h | `GetMerge20FilePosition(const PathContext& paths, int& nPos, int& nTotal, bool bTodoOnly)`（既定実装は false）。`paths` 引数は CI 失敗の修正で追加（§27.4・§27.7） |
| bbf9af2891 | Src/DirDoc.h | 同 override 宣言 |
| 219af9e198 | Src/DirView.h | 巡回ヘルパー宣言＋`Merge20GetFilePosition` 宣言 |
| ab4f16df36 | Src/DirDoc.cpp | 同 override 実装（`FindMerge20ItemFromPaths` で行を特定し `CDirView::Merge20GetFilePosition` へ委譲） |
| 42d5aeecca | Src/DirView.cpp | **ファイル巡回の本体**：`OpenNextFile`／`OpenPrevFile` で TODO のみ巡回（両端で折り返し）。フォルダ一覧の素の ↑／↓ は選択だけを次／前 TODO へ移動（ファイルは開かない。開くのは Enter／ダブルクリック／専用バーボタンのまま）。Shift+↑／↓（選択拡張）は無変更。`IsFirstFile`／`IsLastFile` は TODO-only 対応（`!Merge20HasOtherTarget(...)`）。TODO なし時はキーを飲み込みステータスバーに表示。S3 ステータス文言を `"%1 displayed, %2 checked, %3 unreviewed"` へ（`strutils::format_string3` 使用）。リベースで W1 の列移行呼び出し（`MigrateMerge20Columns`）を保持（§27.8） |
| 645b9e6e25 | Src/MergeEditFrm.cpp | `UpdateDedBar` でファイル名＋(n/m) を専用バーに表示（`#include "paths.h"` 追加）。TODO 計 0 のときは「No unreviewed files」。フォルダ比較なしのときはファイル名のみ。呼出側で `m_pMergeDoc->m_filePaths` を渡す（CI 失敗の修正） |
| 96573085eb | Src/MergeDoc.h | `bool Merge20IsDiffUnresolved(int nDiff) const;` を宣言（`GetResultSegmentByDiff` 近傍） |
| 8ebb03ae83 | Src/MergeDocResultPane.cpp | 同実装：セグメントなし OR Unresolved OR Conflict のとき true。`IsResultDiffPending` と違いセグメントなしを未確認扱いにする |
| 58d6b1c69d | Src/MergeDiffNavigation.h | **差分巡回の本体**：`IsDiffSkippedForTodoOnly`（TODO-only ON かつ解決済み差分をスキップ）を `FindFirst`／`FindLast`／`FindNext`／`FindPrevNonFilteredDiff` のスキップ条件へ追加。`OnNextdiff`／`OnPrevdiff` で TODO-only 時は次ファイルへ行かずファイル内で折り返し（未解決差分がなければ留まる）。`OnUpdateNextdiff`／`OnUpdatePrevdiff` は TODO-only 時は未解決差分の有無で有効化。`#include "OptionsDef.h"`／`"OptionsMgr.h"`／`"MergeApp.h"` を追加（`GetOptionsMgr()` の宣言は MergeApp.h） |
| 43eeca05e3 | Translations/WinMerge/Japanese.po | 新規 msgid 2件（末尾へ追加。D2 形式の `#. Merge20 (D3)` コメント＋ `#: Src/...` 参照付き）。`"No unreviewed files"` → 「未確認ファイルがありません」（設計書 §6 の文言を採用。HTML のトースト「未確認のファイルはありません」とは「の」1字違う。§27.2）、`"%1 displayed, %2 checked, %3 unreviewed"` → 「表示 %1 件中 %2 件チェック、未確認 %3 件」。既存の `"%1 displayed, %2 checked"` はコードで使われなくなったが.po には残した（W1 の再生成で落ちる想定）。リベースで W1 の修正（Wiz 48件＋欠落 6件＋誤訳 1件、付与を含む）を保持し、D3 の 2件を末尾へ追加（§27.8）。D3 の 2件の訳文には付けていない（状態文のため。§27.2） |

### 27.2 設計・仕様との食い違いと判断理由

- **ファイル巡回のチョークポイント**：`CDirView::OpenNextFile`／`OpenPrevFile` が TODO-only 巡回の唯一の入口。専用バーの Prev／Next、Ctrl+F8、メニューの次／前ファイル移動はすべて `CDirDoc::MoveToNextFile` → `OpenNextFile` 経由のため、専用モードでも通常モードでも同じ挙動になる（設計書 §3.9 の「専用モード専用にしない」方針どおり）。
- **フォルダ一覧の ↑／↓ は選択のみ移動**：HTML の `navFile` はファイルを開くが、フォルダ一覧の ↑／↓ で毎回ファイル比較を開くと一覧操作が重くなるため、選択移動だけにとどめた。開く操作（Enter／ダブルクリック／専用バーボタン）は従来どおり。
- **`IsFirstFile`／`IsLastFile` の副作用**：TODO-only 時に `!Merge20HasOtherTarget(...)` を返すため、Prev／Next の有効化（専用バー＋`CMainFrame::OnUpdateNextFile`／`OnUpdatePrevFile`）は折り返しを反映する。代わりに「最初／最後のファイル」ボタンは TODO-only 時も有効のままになる（仕様外の副作用として記録）。
- **差分巡回の述語**：`Merge20IsDiffUnresolved` はセグメントなしを未確認扱いにする。既存 `IsResultDiffPending` はセグメントなしで false を返すため別関数にした。**制限**：結果セグメントはマージセッション中にしか存在しない（`BuildMergeResult` は `StartMergeSession` からしか呼ばれない。`MergeDocResultPane.cpp:166`）。マージセッション外では全差分が未確認扱いになり、TODO-only 差分巡回は通常の差分順＋折り返しと等しくなる。マージ結果ペインでの解決（採用／却下）だけが巡回対象を変える。
- **TODO なし時の表示はメッセージボックスなし（D3 の判断）**：フォルダ比較では (a) S3 ステータス文言に未確認件数を常駐表示（`"%1 displayed, %2 checked, %3 unreviewed"`。一発の `SetStatus` は次のステータス更新（`CDirView::OnTimer`、`TIMER_ID_STATUSBAR_UPDATE`）で上書きされるため）、(b) 巡回失敗時のみ一過性の `SetStatus(_("No unreviewed files"))`。専用バーでは TODO 計 0 のときファイル情報ラベルに同文言を表示。タスク指示の「メッセージボックスで代替」は採用しなかった（作業を中断させないため）。設計書 §6 の表示文言「未確認ファイルがありません」を msgstr に採用した。
- **(n/m) の意味（D2 §24.2-6 で先送りした決定）**：OFF → 表示中全ファイル中の順序／全件数。ON → TODO ファイル中の順序／TODO 件数。現在のファイルが巡回対象外なら分子は 0（例 `(0/5)`）。HTML の `dedIdx` は常に `filtFiles` 内の位置を示すため、HTML とは定義が違う（記録のみ）。
- **3-way 差分ナビゲーションは未変更**：`OnNext3wayDiff` 等の独自スキップループには TODO-only を入れていない（3-way マージは D3 の範囲外）。
- **D2 の残課題 (c)（可視ホバーストリップ／初回ピン固定）は D3 では未実施**：ストリップは新規 UI 要素かフレーム描画が必要で D3 の範囲を超える。初回ピン固定は仕様の自動非表示既定と矛盾する。いずれも D11 候補として §27.6 に記録する。
- **永続化は D0 のオプションキーのみ**：`OPT_MERGE20_TODO_ONLY`（`Merge20/TodoOnly`、既定 false）を読み書きするだけ。セッション保存への配線は D11 の範囲。確認内容の変更は即座に巡回対象へ反映される（巡回移動のたびにサイドカーをライブで読む。キャッシュなし）。
- **`OnUpdateNextdiff`／`OnUpdatePrevdiff` の走査コスト**：TODO-only 時、アイドル更新のたびに `FindFirst`／`FindLastNonFilteredDiff` が先頭（末尾）から未解決差分を走査する。差分数が通常は数百以下で各ステップは安価なため許容とした（記録のみ）。
- **純粋モジュールの真値**：`Merge20Patrol` の期待値は HTML `17_v17.15.html` の `navFile` TODO 分岐（1420–1429行）を Node で実行した結果と照合し、18ケース全一致（`/tmp/d3work/patrol_truth.js`）。GoogleTest は 8件。


### 27.3 Windows 実機での確認手順（D3 受け入れテスト）

1. フォルダ比較を開き、確認内容が TODO のファイルが複数ある状態にする。専用バー（表示 → 比較専用モード）の「未確認のみ」トグルを ON にする。専用バーの「次へ ▶」を押すたびに TODO のファイルだけが順に開くこと。最後の TODO で「次へ」を押すと最初の TODO に戻ること（折り返し）。「◀ 前へ」も逆向きに折り返すこと。
2. フォルダ一覧にフォーカスを置き、素の ↑／↓ キーで選択だけが TODO のファイルへ移動すること（ファイル比較は開かない）。Enter でそのファイルが開くこと。Shift+↑／↓ は従来どおり選択を拡張すること。
3. TODO のファイルが 1件だけの状態で「次へ」を押すと、同じファイルに留まること（自分自身への折り返し）。TODO が 0件の状態で「次へ」／↑↓ を押すと、ステータスバーに「未確認のファイルはありません」（または英語表示。§27.5 の .po 注意参照）と出ること。メッセージボックスが出ないこと。ステータスバーの件数表示が「表示 N 件中 M 件チェック、未確認 0 件」になっていること。
4. トグルを OFF にすると、Prev／Next・↑↓ が従来どおり全ファイルを表示順に巡回すること（折り返しなしで端で止まる／次ファイルへ行く従来挙動）。
5. 専用バーのファイル名＋(n/m) 表示：TODO-only OFF で「ファイル名 (n/m)」（m＝表示ファイル数）。ON で m＝TODO 件数に変わること。TODO-only ON の状態で既に確認済みのファイルを開くと分子が 0 になること（例 `(0/5)`）。TODO 計 0 のときラベルが「未確認のファイルはありません」になること。
6. 確認内容の変更が即座に反映されること：TODO のファイルを開き、確認内容を「済」に変えてから「次へ」を押すと、そのファイルが飛ばされること。逆に「済」を TODO に戻すと巡回対象に復帰すること。
7. 差分巡回：ファイル比較でマージセッションを開始し、一部の差分をマージ結果ペインで解決（採用／却下）した状態で、TODO-only ON のまま F7／F8 を押すと、解決済みの差分を飛ばして未解決の差分だけを巡回し、最後の未解決差分で先頭へ折り返すこと（次ファイルへは行かない）。マージセッションを開始していないファイル比較では、TODO-only ON でも全差分が巡回対象になること（§27.2 の制限どおり）。
8. 回帰：TODO-only OFF のフォルダ比較・ファイル比較の巡回（次／前ファイル、F7／F8、↑↓）が従来どおり動くこと。


### 27.4 CI 結果（PR #24）

リベース前の実行（旧 head 群。リベースで無効になった）：
- 実行 45737714355（head ca8bedbf、PR #24 の初回 head）：**failure**。ビルドジョブ 139634870553 の "Build failure" ステップで失敗。
 - 原因：`Src/DirDoc.cpp(769)` で `error C2882: 'paths': illegal use of namespace identifier in expression` と `error C2660: 'FindMerge20ItemFromPaths': function does not take 1 arguments`。`GetMerge20FilePosition` の宣言・定義・呼出のいずれにも `paths` 引数がなく、実装だけが存在しない識別子 `paths` を使っていたため、`paths` 名前空間と解釈された。D3 実装時の見落とし（宣言と実装の不一致を手元で検出できなかった）。
 - 修正：`GetMerge20FilePosition` に `const PathContext& paths` 引数を追加（旧 07aadb81 IDirDoc.h／8569afef DirDoc.h／75e0b072 DirDoc.cpp／9774657a MergeEditFrm.cpp）。中間の 3 head は宣言・定義・呼出が揃っていないため CI は当然失敗する（実行 45738924493／45738946977／45738974917、いずれも failure）。リベース後のコミットには修正込みの内容が入っている（§27.1）。
- 実行 45738999523（head 9774657a22、修正後の head）：**success**。ジョブ 139639086423 build success。GoogleTest **616 テスト全パス**（D2 ベースライン 608 ＋ D3 新規 8）。成果物 2件：WinMerge-x64.zip（26,203,517 バイト、ID 15080164714）と WinMerge-x64-Setup-x64.exe（39,704,651 バイト、ID 15080861405）、いずれも 2027-02-24 失効。ログ中の "FAILED" は IDS_FILESAVE_FAILED／IDS_MERGE_RESULT_SAVE_FAILED の翻訳文字列 ID のみ。`error C*`／`error LNK*` は 0件。`Merge20Patrol.obj` の生成を確認済み。
- 実行 45741585765（マージ head 933bff0e、W2 マージ後）：**success**。
- 実行 45741691888（head b48732c9、記録のみの訂正コミットを含む旧最終 head）：**success**（ジョブ 139647990603 build success）。GoogleTest **616 テスト全パス**。成果物 2件：WinMerge-x64.zip（26,194,822 バイト、ID 15082061982）と WinMerge-x64-Setup-x64.exe（39,683,207 バイト、ID 15082340036）、いずれも 2027-02-24 失効。
- 実行 45742900702（head 26b86d6c、旧最終記録コミット）：リベース開始時点で in_progress のまま残った。リベースで head が置き換わったため結果は追わない（記録のみのコミットでコードは b48732c9 と同一）。

リベース後の実行（新 head 群。§27.8 末尾に追記する）：
- （記録コミットの CI 実行待ち）

### 27.5 検証レポート（検証ゲート (a)–(d)）

- (a) 仕様・設計の条項照合：仕様書 §4(g)（Prev／Next・↑↓ は未確認のみ巡回し両端で折り返す）→ ファイル巡回・差分巡回とも実装済み（コード配線確認＋純粋モジュール単体テスト）。仕様書 §5.4（専用バー内容）→ ファイル名＋(n/m) 表示を追加（D2 先送り分の解消）。設計書 §5.2 D3 の受け入れ条件（TODO なし時の表示、OFF で通常巡回に戻る、確認内容の変更が即座に反映される）→ 表示は実装済み、OFF 時の復帰はコード配線確認済み、即時反映はサイドカーのライブ読み取りによる（実機での最終確認は §27.3 手順 3・4・6）。
- (b) 利用可否：トグル ON／OFF、巡回、折り返し、TODO なし時の表示はコードの配線で確認済み。巡回の核心（折り返し・唯一の対象・対象なし・範囲外・不正な方向）は純粋モジュールとして GoogleTest 8件で裏付け済み（HTML 真値との 18ケース照合を含む）。MFC 側の配線（DirView／DirDoc／MergeDiffNavigation）は単体テストがなく、コンパイル＋実機手順での確認に依存する（§27.6）。
- (c) 表示：新規 UI 文字列 2件は repo の Japanese.po に訳あり（§27.1）。**ただし出荷 .po の書き直し問題（監査 F-1、W1 で修正中）のため、今回のビルドでは英語表示になる可能性が高い。**W1 修正後に日本語表示の再確認が必要。専用バーのファイル情報ラベルは幅 300px で右端クリッピングの懸念は D2 から継続（§27.2）。
- (d) 回帰：TODO-only OFF 時の巡回は既存コードパスをそのまま通る（`GetOptionsMgr()->GetBool(OPT_MERGE20_TODO_ONLY)` が false のとき分岐しない）。GoogleTest 616 全パスで既存テストの退行なし。フォルダ比較・ファイル比較の通常巡回は実機で §27.3 手順 8 により確認すること。


### 27.6 未検証（実機確認が必要）・残課題

- 未検証：§27.3 の全手順（専用バー・フォルダ一覧・差分巡回の実機挙動、(n/m) 表示、TODO なし時の表示、即時反映、回帰）。Linux VM では WinMerge を実行できないため、表示の最終確認は実機に依存する。
- D4 への引き継ぎ：巡回インフラ（`Merge20Patrol` 純粋モジュール、`CMergeDoc::Merge20IsDiffUnresolved`）は D4 以降でも流用できる。既知の制限として、マージセッション外では全差分が未確認扱いになること（§27.2）、3-way 差分ナビゲーションは TODO-only 未対応であること、TODO-only 時に「最初／最後のファイル」ボタンが有効のままになることを D4 の設計に反映すること。
- D2 の残課題 (c) は D11 候補へ：可視ホバーストリップ（新規 UI 要素かフレーム描画が必要）と初回ピン固定（自動非表示既定と矛盾）を D3 では実施しなかった。D11（セッション保存・UI 微調整）で検討すること。
- 専用バーの右端クリッピング（全幅約 1310px）は D2 から継続。ファイル情報ラベル追加で悪化する可能性があるため、実機で狭いウィンドウでの表示を確認すること。


### 27.7 作業中のミスと修正の記録

- **空コミット 27b9a659（リベースで削除）**：`Src/resource.h` への編集スクリプトがコメント文言の不一致（resource.h の行末混在が原因）で失敗したのに、そのままコミットを実行してしまい、内容のないコミットが履歴に残った。直後の d8175ec6 で実変更を入れた。PR #24 の説明にも明記した。リベース（§27.8）で 1ファイル＝1コミットに載せ直した際に空コミットは削除した（内容がなかったため）。教訓：コミット前に変更の有無を確認すること。
- **CI 失敗と `paths` 引数の欠落（§27.4）**：`GetMerge20FilePosition` の設計時に、行の特定に必要なマージ文書のファイルパスを引数として渡す設計にしていなかった。実装だけが `paths` を参照する形で書き進めてしまい、宣言・定義・呼出の不一致を手元で検出できなかった。CI のコンパイルエラーで発覚し、4コミットで修正した。リベース後のコミットには修正込みの内容が入っている。教訓：IDirDoc のようなインターフェース追加時は、宣言・定義・呼出の 3か所を 1つの差分として並べて確認すること。
- **ファイル数の誤記（18→21）**：旧 §26 冒頭の「18ファイル」は誤りで、正しくは 21ファイルだった。記録コミット後に数え直して発覚し、旧 b48732c9 で訂正した。リベース後の本節では 21ファイルと正しく記載している。
- **旧マージコミット 933bff0e（W2 対応）**：W2 マージ後に PR がコンフリクトしたためマージコミットで解消した（旧 §26.8）。リベース（§27.8）でマージコミットは削除し、W2・W1 の変更は master 0aefc6dd 経由で取り込んだ。

### 27.8 リベース（master 0aefc6dd へ。W1 マージ対応）

- W1（PR #23）が master へマージされた（master head 0aefc6dd）。D3 ブランチは fc0e24b3 から分岐していたため、最終 CI の前にリベースした。リベース前の旧 head 群の CI 結果は §27.4 参照（いずれもリベースで無効）。
- 方法：Git Database API のみでリベースした（ローカルクローンは使わなかった。クローン直後のセキュリティチェックで実行リスクが指摘されたため。後述）。手順：(1) API で base（fc0e24b3）／旧ブランチ先端（26b86d6c）／新 master（0aefc6dd）の 3版を全 D3 ファイルについて取得。(2) W1 と重なる 3ファイル（`Src/DirView.cpp`、`Translations/WinMerge/Japanese.po`、`MERGE20_NOTES.md`）を 3方向マージし、他の 18ファイルは新 master と base が同一であることを確認して旧ブランチ版をそのまま採用。(3) 1ファイル＝1コミットの 20コミットを新 master の上に API で連鎖作成（各コミットの親は直前のコミット）。(4) 記録コミット（本節）を追加。(5) ブランチ ref を force で新先端へ移動し、直後に PR の state を確認する。
- `Src/DirView.cpp`：diff3 でクリーンマージ。旧ブランチ版との差分は W1 の列移行呼び出し（`OnInitialUpdate` での `MigrateMerge20Columns`、15行）のみ、新 master 版との差分は D3 の変更（189行）のみであることを確認済み。
- `Translations/WinMerge/Japanese.po`：diff3 では 24か所が競合と判定されたが、内容は W1・W2 の両方が同じ WinMerge→WizMerge 改名をした箇所と、末尾の D2 訳文への付与だった。解決は「新 master 版＋D3 の 2件を末尾へ追加」とした（新 master には W2 の改名も W1 の修正も含まれているため）。検証：マージ結果と新 master 版の差分は D3 の 2件のみ。D3 の 2件の msgid は重複なし（各 1件）。D3 の 2件の訳文には付けていない。W1 の付与は機能ラベル（ボタン／メニュー／列名）が対象で、D3 の 2件は状態文（「未確認ファイルがありません」「表示 %1 件中 %2 件チェック、未確認 %3 件」）のため。なお機能ラベルをとするかとするかはユーザー未確認のまま（§27.2）。
- `MERGE20_NOTES.md`：旧 §26（D3）と W1 の §26 が衝突したため、D3 の記録を §27 に付け替えた（§27 はリベース時点で空き。W2 フォローアップが §27 を使う場合はさらに付け替える）。旧 §26.8（W2 マージの記録）は本 §27.8 に置き換えた。
- リベースで削除したもの：空コミット 27b9a659（内容なし）、マージコミット 933bff0e（W2 対応。W2・W1 の変更は master 経由で取り込み済み）、修正コミットの分割（4件の修正は各ファイルのコミットに畳み込み）。旧コミット SHA はすべて無効になったため、本節の表（§27.1）が正である。
- セキュリティに関する注記：当初はローカルクローンで `git rebase` を行う予定だったが、クローン直後の実行でシステムのセキュリティチェックが失敗し、クローン内容の実行・展開・git 操作が禁止されたため中止した。クローンは削除し、API のみ（ファイル内容の取得と Git Database API によるコミット作成）でリベースをやり直した。API 経由のファイル内容はテキストとしてのみ扱い、リポジトリのコードはローカルで実行していない。CI でのビルドは GitHub 上の通常の CI 実行である。
- リベース後の CI 結果：実行 45744759651（head d4a023f377、記録コミットを含む最終 head）は **success**（ジョブ 139658007512 build success）。
  - GoogleTest：**616 テスト全パス**（`[  PASSED  ] 616 tests.`。新規 Merge20Patrol 8件は全て OK）。`error C*`／`error LNK*` は 0件。ログ中の "FAILED" は IDS_FILESAVE_FAILED／IDS_MERGE_RESULT_SAVE_FAILED の翻訳文字列 ID のみ。
  - 成果物 2件：WinMerge-x64.zip（26,305,164 バイト、ID 15083542142）と WinMerge-x64-Setup-x64.exe（39,729,525 バイト、ID 15083740604）、いずれも 2027-02-24 失効。
  - CI ログからの追加証拠：`Merge20Patrol.obj` が Merge.vcxproj（`.\..\BuildTmp\x64\Release\`）と UnitTests.vcxproj（`x64Release\`）の両方で生成されたこと。
  - 注：旧最終記録コミットの実行 45742900702（head 26b86d6c）はリベース後に success で完了したが、旧 head のため参考記録とする。

## 28. Phase 4 D4（フォーカス詳細）— ブランチ `merge20/phase4-d4`

PR #26。2026-11-26。master f0db9181（D3＝PR #24 マージ済み）から分岐。19ファイル、20コミット＋記録。設計書 §3.5・§5.2 D4・§3.3、仕様書 §3 (3/4)・§5.2・§4(g)。D3＝§27。

### 28.1 変更一覧

| コミット | ファイル | 内容 |
|---|---|---|
| b906e902 | Src/Merge20/Merge20Focus.h | 純粋モジュール（新規）。判定ボタン→ソースペイン写像 `Merge20FocusActionSrcPanes`／表示可否 `Merge20FocusActionVisible`／スクロールクランプ `Merge20FocusClampTopLine`。MFC 不使用 |
| 2f2f5043 | Src/Merge20/Merge20Focus.cpp | 同実装。HTML 17_v17.15.html の setHunk 戦略を写像（旧挿入→{0}、不採用→寄与なし側、変更→新{1}／旧{0}／両方{0,1}） |
| 1eebd909 | Testing/GoogleTest/Merge20/Merge20Focus_test.cpp | 同テスト 11件（g++ ハーネスで 51チェックを事前検証） |
| 480bcda9 | Src/Merge.vcxproj | Merge20Focus 登録（pch.h グループ）＋ FocusResultView／FocusDock 登録（StdAfx.h グループ） |
| b66762af | Src/Merge.vcxproj.filters | 同フィルタ登録 |
| ea9f7982 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | Merge20Focus＋テスト登録（FocusDock／FocusResultView は MFC 専用のため登録しない） |
| fe62fc70 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同フィルタ登録 |
| 2540955f | Src/Merge20/Merge20FocusResultView.h | 読み取り専用ビュー `CMerge20FocusResultView : CGhostTextView`（自前の `CGhostTextBuffer`）。`QueryEditable()` は false |
| ec21b78d | Src/Merge20/Merge20FocusResultView.cpp | 同実装。`SetSegmentText()` は読取専用を外して詰め直し再アタッチ。`ScrollToSubLine` オーバーライドでドックへ同期通知 |
| 0c56ee2c | Src/Merge20/Merge20FocusDock.h | コンテナ `CMerge20FocusDock : CWnd`（Diff Pane バーの唯一の子）。ヘッダ＋詳細スプリッタビュー＋結果セグメントビューを保持 |
| 766c9a14 | Src/Merge20/Merge20FocusDock.cpp | 同実装。`ShowDiff`／`UpdateHeader`／`LayoutHeader`／`LayoutViews`／`SyncFocusFromDetail`／`SyncDetailFromFocus`／`OnButtonClicked`（`ON_CONTROL_RANGE`、新規 `ID_` コマンドなし） |
| 565ffe0d | Src/resource.h | IDC 1758–1766 を割当て（フォーカスドック用）。`_APS_NEXT_CONTROL_VALUE` 1758→1767。D0 予約コメントを更新 |
| c7ade2df | Src/MergeDoc.h | `CMerge20FocusDock` 前方宣言＋メンバ＋getter/setter、`Merge20GetFocusResultText`／`Merge20FocusApplyJudgment` 宣言 |
| 7ff36b7e | Src/MergeDoc.cpp | ctor 初期化＋`SetCurrentDiff` 末尾でドックへ `ShowDiff(m_nCurDiff)` |
| 74990273 | Src/MergeDocResultPane.cpp | `Merge20GetFocusResultText`（`GetResultSegmentByDiff`＋`GetResultBufferLinesText`）／`Merge20FocusApplyJudgment`（`ResultChooseSources`＋Phase 3 L2 学習記録）。`ResultChooseSources`／`BuildMergeResult` 末尾でドック更新（常に現在差分） |
| 733cfab1 | Src/MergeEditFrm.h | 値メンバ `CMerge20FocusDock m_wndFocusDock` |
| 7f96a369 | Src/MergeEditFrm.cpp | `OnCreateClient`：ドックを DetailBar の子として作成→詳細スプリッタビュー（親＝ドック）→結果ビュー（親＝ドック）→`SetViews`／`SetMergeDoc`／`ShowDiff(-1)`。`OnBarCheck` 再表示時と `UpdateResources` でドック更新 |
| b904e515 | Src/MergeEditView.cpp | `ScrollToSubLine` 末尾（`m_bDetailView` のときのみ）：親を辿ってドックを探し `SyncFocusFromDetail` を呼ぶ |
| 46258986 | Translations/WinMerge/Japanese.po | D4 新規 16 msgid の日本語訳（判定ボタン 7件は【Wiz】付き、ヘッダ／状態文言は D3 前例で【Wiz】なし）。ついでに D3 の 2項目（"No unreviewed files"、"%1 displayed, %2 checked, %3 unreviewed"）の二重エンコード UTF-8（文字化け）を正しい UTF-8 に修正 |
| 7dd12e14 | Src/Merge20/Merge20FocusDock.cpp | `ScrollToLine` 呼び出し 2件を公開 `ScrollToSubLine` に変更（CI 失敗 C2248 の修正。§28.6） |

### 28.2 設計判断（設計書 §3.5 の候補 (a)/(b)）

- (b) を採用：結果ペインの該当行だけを抜き出して読み取り専用ビューに表示する。(a)（`CMergeResultView` を借りた 3段目）は不採用。理由：ドキュメントは `m_pMergeResultView` ポインタを単一として持ち、undo グループの起点と無効化に使う。2つ目のインスタンスが登録ビューになってドキュメント更新の宛先を奪う危険があるうえ、全バッファのスクロールモデルでは「該当差分だけ」を表示できない。
- コンテナ方式：`CDiffViewBar` は `SCBS_SIZECHILD` で子を 1つしか持てないため、ドックがバーの唯一の子となり、ヘッダ（30px）＋詳細ビュー＋結果ビューを保持する。残り高さは 3:2 に分割。固定高さなし、バーの端ドラッグでリサイズ可能（T-4-7 代替）。
- フレームがビューを作成し、ドックはホストのみ（`CCreateContext` をドックに通さない）。`m_pwndDetailMergeEditSplitterView` はフレーム保持のままなので `UpdateSplitter`／`FlipSplit` は無変更。
- 判定ボタンは HTML と同じ文脈表示：旧のみ→旧挿入／不採用、変更→新／旧／両方、新のみ→不採用のみ。3-way 片側差分は判定ボタンなし（不採用が曖昧なため）。✂分割／↩戻すは表示のみで無効化（本体は D5）。
- 判定は学習対象：`Merge20FocusApplyJudgment` は `ResultChooseSources`＋L2 学習記録（`OnMergeChooseSource` と同じ）。フォーカスドックの判定は明示的判定であり、バッチ／自動／復元は `ResultChooseSources` 直接で未記録のまま。
- スクロール同期のチョークポイントは `ScrollToSubLine`。詳細ペインは既に現在差分にクランプされるため、差分先頭からのサブラインオフセットで同期する。再入防止フラグはドック側（`m_bSyncingScroll`）。ワードラップ OFF（既定）ではサブライン＝行で完全同期。ワードラップ ON ではずれる可能性あり（§28.5）。
- 専用モード：ドックは DetailBar 内にあり、専用モードは DetailBar を隠すため表示されない（D1 との調整は不要、D6 スナイパーが専用モード側のフォーカス用途）。

### 28.3 CI 結果（最終 head 7dd12e14）

- 実行 ID：45752165310、結論：success。ジョブ 139682274203（build）success。
- GoogleTest：627件全パス（616＋新規 11件）。全文ログ `[==========] 627 tests from 50 test suites ran. (1244 ms total)`／`[  PASSED  ] 627 tests.`。`[  FAILED  ]` は 0件。Merge20Focus の 11テストも全パス。CI の .po チェックも通過（"Merge20 strings present in the bundled Japanese.po."）。
- 成果物：WinMerge-x64.zip（ID 15086960462、26,317,060 バイト）、WinMerge-x64-Setup-x64.exe（ID 15086423104、39,753,875 バイト）。両方とも失効日 2027-02-24T03:57:42Z、未失効。
- エラー 0件（全文ログに `error C*`／`error LNK*`／`error MSB*` なし）。
- 注：最終 head 一つ前（46258986）の実行 45751012497 は failure。原因は `Merge20FocusDock.cpp` の `ScrollToLine` 呼び出し 2件に対する error C2248（§28.6）。修正コミット 7dd12e14 で最終 head とし、再実行で success。46258986 以前の各プッシュに対する実行（b904e515 他）も同じ C2248 で failure。

### 28.4 検証ゲート

- (a) 仕様・設計：§5.2 D4 の 3要件（3段表示／スクロール連動／判定ボタンで状態変化）はコード上すべて実装。T-4-7 は標準リサイズ＋表示切替で代替。✂／↩ は表示のみ（設計どおり D5 へ）。
- (b) 利用可否：差分選択→3段表示→スクロール連動→判定ボタンで結果ペインのセグメント状態が変わる流れは、通知点（`SetCurrentDiff`／`ResultChooseSources`／`BuildMergeResult`／再表示）と同期フックで網羅。実機での操作確認は未検証（チェックリストは §28.5）。
- (c) 表示：日本語訳 16件を追加。ボタン幅は日本語ラベルに合わせて設定（旧挿入 108px 等）。狭いウィンドウでは右端のボタンがクリッピングされる（専用バーと同じ既知の挙動）。実機での表示確認は未検証。
- (d) 回帰：既存のフォルダ比較・ファイル比較の経路は変更していない（DetailBar の子がドックに変わったのみで、詳細スプリッタビュー自体は同一）。実機での回帰確認は未検証。
- 副産物：master の Japanese.po の D3 2項目が二重エンコード UTF-8（文字化け）だったことを発見し、同コミットで修正した。

### 28.5 D5 への引き継ぎと残課題

- ✂分割／↩戻すの本体（D5）。ボタンは表示済み・無効化済みで、`OnButtonClicked` は Split／Regroup を早期 return している。
- ワードラップ ON 時のスクロールずれ：フォーカスビューはサブライン単位で同期していないため、折り返し行があると詳細ペインとずれる。実機で確認すること。
- 結果ペインでの手編集は、次に差分が変わる／判定するまでドックに反映されない（`OnResultLineEdited` 経由の通知なし）。
- 3-way 片側差分の判定ボタンなし（仕様として残すか D5 以降で検討）。
- フォーカス結果ビューの構文ハイライト／セグメント状態の色分けは未実装（ヘッダの状態文言のみ）。
- ドラッグ可能な区切り線は未実装（D11 候補）。`OPT_MERGE20_FOCUS_DOCK_HEIGHT`（D0 キー）は D4 では未使用。
- テーブル比較モードでの動作は未検証（結果未構築時はプレースホルダ表示になる設計）。
- 実機チェックリスト：(1) ファイル比較を開き Diff Pane が 3段（旧／新／結果）になっていること。(2) 差分を選択すると 3段ともその差分だけになること。(3) 旧／新ペインをスクロールすると結果段も連動すること（ワードラップ OFF／ON の両方）。(4) 旧のみ差分で「旧挿入」を押すと結果ペインのセグメントが旧内容になり、学習候補が付くこと。(5) 変更差分で 新／旧／両方 が状態を変えること。(6) 追加差分では不採用のみ表示されること。(7) ✂／↩ が無効化されていること。(8) Diff Pane を閉じて再度開くと表示が復旧すること。(9) 専用モードではドックが表示されないこと（DetailBar ごと隠れる）。(10) 日本語が文字化けせず表示されること（特にボタンとヘッダ）。

### 28.6 教訓

- `CCrystalTextView::ScrollToLine` は protected（`ccrystaltextview.h` の 400行付近は protected 区画。`public:` は 455行目から）。外部クラス（ドック）からは呼べない。公開されているのは `ScrollToSubLine` のオーバーライド（`CMergeEditView` で public、`CMerge20FocusResultView` で public）。ドックからのスクロールは `ScrollToSubLine` を使うこと。事前調査の「`ScrollToLine` は public」は誤りだった（§28.3 の初回 CI 失敗の原因）。
- `commit_file.py` の 422 "Update is not a fast forward" は数秒待って再試行で回避できた（今回は発生せず）。
- `resource.h` の `_APS_NEXT_CONTROL_VALUE` はコメント中にも出現するため、置換時は `#define` 行を狙うこと（最初のスクリプトはコメントに当たって失敗した）。
- master の .po を編集するときは、末尾だけでなく全体を二重エンコード等の破損がないか走査すること（今回は D3 の 2項目を発見・修正）。
- 新規ファイルの `Merge20FocusActions` 配列は FocusDock.cpp の無名名前空間にあり、純粋モジュール側にはない。`OnButtonClicked` の索引計算とボタンの ID 順・enum 順が一致していることが前提。

## 29. W2f（.cpp／.sct 側の WizMerge 化）— ブランチ `merge20/phase4-w2f`

> 節番号について：D3 が §27、D4 が §28 を取得したため、本節は §29 とした。
> ベースは master 56136579（D3＝PR #24、D4＝PR #26 マージ済み）にリベースしたものである。

PR #25。master `0aefc6dd` から分岐し、2026-11-26 に master `56136579`（D3＝PR #24、D4＝PR #26 マージ済み）へリベース。記録は §27 から §29 に付け替えた。リベース後のコード 17 コミット＋本記録 1 で 18コミット、18ファイル。
すべて表示文字列のみの変更で、行の増減なし（全ファイルで追加行数＝削除行数）。

### 29.0 要約

- W2（PR #22）は.rc／.po の表示名を "WinMerge"→"WizMerge" に変えたが、
.cpp／.sct のソース文字列は変えていなかった。W1 の監査（§26.7）で判明した
不整合：.cpp に残る "WinMerge" の `_()` 文字列は、.po の msgid が
"WizMerge" 化されたため検索が外れて英語表示になっていた（W2 以前は日本語
だったものが英語に戻る）。本 PR は.cpp／.sct 側も "WizMerge" 化して
検索を当たるようにする。
- Src ツリー全体を grep し、"WinMerge" を含む `_()` 文字列を全数列挙した
結果、該当は 6 種類の msgid（8 か所）のみだった（.pot の WinMerge 項目と
突き合わせて完全性を確認。29.4 参照）。うち 5 種類（7 か所）を変更し、
1 種類（FileFiltersDlg.cpp:520）はパス参照のため変更しなかった（29.2）。
-.sct 8 ファイルの `<title>` を "WizMerge Plugin Options" に変えた。
W1 があえて "WinMerge" のままにしていた Merge20Exclude.sct の.po 項目も
"WizMerge" に合わせた。ApplyPatch.sct は.po 項目が存在しなかったため
新規追加した。

### 29.1 変更した文字列の一覧

| ファイル・行 | 変更前 → 変更後 |.po の対応（W2 が作成済みの "WizMerge" 版 msgid） |
|---|---|---|
| Src/ClipboardHistory.cpp:180 | "32-bit WinMerge does not support Clipboard Compare" → "32-bit WizMerge does not support Clipboard Compare" | msgid 同文、msgstr「32bit版 WizMerge ではクリップボード比較はサポートされていません。」 |
| Src/CrashLogger/CrashLogger.cpp:411 | "WinMerge detected a previous crash. Please report this if possible." → "WizMerge detected a previous crash...." | msgid 同文、msgstr「以前の WizMerge のクラッシュを検出しました。可能であればご報告ください。」 |
| Src/HexMergeDoc.cpp:240 | "Another application updated\n%1\nsince WinMerge loaded it.\n\nOverwrite?" → "since WizMerge loaded it." | msgid 同文（複数行）、msgstr「%1\nこのファイルは WizMerge 以外のアプリケーションで修正されています。修正されたファイルに上書きしますか?」 |
| Src/MergeDoc.cpp:1135 | 同上（HexMergeDoc と同一 msgid） | 同上 |
| Src/MainFrm.cpp:2868 | "WinMerge Project Files (*.WinMerge)|*.WinMerge||" → "WizMerge Project Files (*.WinMerge)|*.WinMerge||"（表示名のみ。拡張子 `*.WinMerge` と `.WinMerge` は不変） | msgid 同文、msgstr「WizMerge プロジェクト ファイル (*.WinMerge)|*.WinMerge||」 |
| Src/OpenView.cpp:1254 | 同上（MainFrm と同一 msgid） | 同上 |
| Src/MergeApp.cpp:154 | "WinMerge comes with ABSOLUTELY NO WARRANTY...." → "WizMerge comes with..." | msgid 同文、msgstr は msgid と同一（GPL 文は英語のまま。W2 の方針どおり） |
| Plugins/dlls/AI.sct:325 | `<title>${AI.sct WinMerge Plugin Options}</title>` → WizMerge | msgid "AI.sct WizMerge Plugin Options"、msgstr「AI.sct WizMerge プラグイン オプション」 |
| Plugins/dlls/ApplyPatch.sct:474 | `<title>${ApplyPatch.sct WinMerge Plugin Options}</title>` → WizMerge | 本 PR で.po に新規追加（29.3）。msgstr「ApplyPatch.sct WizMerge プラグインオプション」 |
| Plugins/dlls/CompareMSExcelFiles.sct:787 | 同様に WizMerge | msgid 同文あり（W2 作成）、msgstr「CompareMSExcelFiles.sct WizMerge プラグインオプション」 |
| Plugins/dlls/CompareMSPowerPointFiles.sct:437 | 同様に WizMerge | msgid 同文あり（W2 作成） |
| Plugins/dlls/CompareMSVisioFiles.sct:396 | 同様に WizMerge | msgid 同文あり（W2 作成） |
| Plugins/dlls/CompareMSWordFiles.sct:436 | 同様に WizMerge | msgid 同文あり（W2 作成） |
| Plugins/dlls/Merge20Exclude.sct:294 | 同様に WizMerge |.po 項目を "WinMerge"→"WizMerge" に改名（29.3） |
| Plugins/dlls/PrediffLineFilter.sct:294 | 同様に WizMerge | msgid 同文あり（W2 作成） |
| Plugins/dlls/ApplyPatch.sct:51 | `var PLUGIN_NAME = "ApplyPatch.sct WinMerge Plugin";` → WizMerge |.po 項目なし（`mergeApp.InputBox` のタイトルにそのまま渡される非翻訳文字列。英語表示のまま改名のみ）。変更理由：InputBox のダイアログタイトルとして表示される表示名のため。 |

### 29.2 変更しなかったもの（判断理由）

- FileFiltersDlg.cpp:520 および Src/Merge.rc:4921（IDS_FILEFILTER_TMPL_MISSING）
`"Cannot find filter template!\n\nCopy %1 to WinMerge/Filters Folder:\n%2."`：
"WinMerge/Filters Folder" はディスク上の実フォルダ名を指す。
`%2` は `GetGlobalFilterPathWithCreate()` の返す実パス（`%APPDATA%\WinMerge\Filters`
または `My Documents\WinMerge\Filters`。FileFilterHelper.cpp:769-770、
FileFiltersDlg.cpp:485）で、表示ラベルを "WizMerge" にすると `%2` の実パスと
矛盾し、ユーザーを誤ったフォルダへ誘導する。W2 の keep-list（パスは変えない）
に従い不変とした。.po／.pot の "WinMerge" 版 msgid（訳付き）もそのまま残し、
日本語表示は維持される。
- exe ファイル名 WinMergeU.exe、レジストリキー（`Software\Thingamahoochie\WinMerge`）、
INIキー（`[WinMerge]`）、設定ファイル名（`WinMerge.txt`、`WinMergeCrashInfo.txt`、
`WinMerge*.dmp`）、ユーザーデータフォルダ（`WinMergeUserDataFolder`）、
ProgID（`WinMerge.FileCompare`／`WinMerge.FolderCompare`）、拡張子 `*.WinMerge`
（ProjectFile.cpp の `PROJECTFILE_EXT` を含む）、URL（`https://winmerge.org/`）、
`${WINMERGE_HOME}`、プラグインのクラス名 `WinMergeScript`、識別子
（`WinMergePluginBase`、`WinMergeDarkMode`、`CWinMergeProfile`、
`LoadWinMergeDiffsFromDiffUtilsScript` 等）：W2 の keep-list どおり不変。
- コメント文中の "WinMerge"（.cpp／.h／.sct の注釈、Merge20 のヘッダコメントを含む）：
表示されないため不変。
- 診断ファイルのヘッダ行（`_()` 経由でないため本タスクの対象外。将来の検討課題として
29.7 に記録）：Src/ConfigLog.cpp:343 `"WinMerge Configuration Log\r\n"`、
Src/CrashLogger/CrashLogger.cpp:128 `"WinMerge Crash Information\r\n"`、
Src/PatchHTML.cpp:40 および Src/FileCmpReport.cpp:89
`"<title>WinMerge File Compare Report</title>"`（生成される HTML レポートのタイトル）。
- 他言語の.po は触らない（W2 と同じ方針）。英語以外ではインストーラー表示文などが
WinMerge のまま残るが、.cpp の msgid が "WizMerge" になったため他言語でも
msgid フォールバックで "WizMerge" と表示される。
- Plugins/Strings.rc は触らない（W1 の Merge20Exclude.sct と同じく、.sct タイトルは
.po への直接追加で対応。W1 §26.2 の.ps1 マージ方式で.pot に無い項目も保持される
ことが W1 の出荷物確認で実証済み）。

### 29.3 日本語.po／.pot の更新内容

- `Translations/WinMerge/Japanese.po`（11,175 行 → 11,180 行、+5 行）：
- Merge20Exclude.sct の項目を改名：msgid／msgstr の "WinMerge"→"WizMerge"
（`#. Merge20 (W1):...` コメントと `#: Plugins/dlls/Merge20Exclude.sct:294` は不変）。
- ApplyPatch.sct の項目を新規追加（W2 に存在しなかったため）：
`#. Merge20 (W2f): ApplyPatch.sct settings dialog title (Plugins/dlls/ApplyPatch.sct)`、
`#: Plugins/dlls/ApplyPatch.sct:474`、msgid "ApplyPatch.sct WizMerge Plugin Options"、
msgstr "ApplyPatch.sct WizMerge プラグインオプション"。
挿入位置は Merge20Exclude.sct の項目の直後。
- 削除すべき "WinMerge" 版 msgid はリポジトリの.po には存在しなかった。
5 件の.cpp 文字列の "WinMerge" 版は W2 の時点で.po になく、ビルド時の
.pot 再生成で出荷.po に未訳（msgstr 空）で追加されていただけのため、
本 PR の.cpp 変更で今後は追加されなくなる（29.4 の出荷物確認で検証）。
残る "WinMerge" は keep-list のみ（ヘッダ 3 行、`${WINMERGE_HOME}`、
ProgID 由来の.rc 項目、フィルタテンプレート、プロジェクトファイルの `*.WinMerge` 部分）。
- msgid の重複は新規作成なし（(ctx,msgid) の重複は 0 件。引用符の対応チェック済み）。
- `Translations/WinMerge/English.pot`（10,454 行 → 10,433 行、−21 行）：
.cpp 由来の "WinMerge" 版 5 エントリを除去（`#: ClipboardHistory.cpp:180`、
`#: CrashLogger/CrashLogger.cpp:411`、`#: HexMergeDoc.cpp:240`、
`#: MainFrm.cpp:2868`、`#: MergeApp.cpp:154`）。"WizMerge" 版は Merge.rc 由来
（`#: Merge.rc:4746` GPL、`#: Merge.rc:4846` プロジェクトファイル、
`#: Merge.rc:5052` 他アプリ更新、`#: Merge.rc:5965` クリップボード、
`#: Merge.rc:6096` クラッシュ検出）で残る。フィルタテンプレートの
"WinMerge" 版（`#: Merge.rc:4921`）は残る。
CI のビルド時に CreateMasterPotFile.js（.cpp 抽出付き）が.pot を再生成するため、
再生成結果は本コミットの.pot と一致するはずである（.cpp 変更後は "WizMerge" 版が
.rc と重複して.rc 側が勝ち、"WinMerge" 版は出なくなる。行の増減がないため
`#: ` の行番号も変わらない）。不一致があれば CI 側の.pot が使われるため
自己修復される（W1 §26.2 参照）。

### 29.4 検証

- (a) 変更した各文字列と.po の対応：Src ツリー全体の grep で "WinMerge" を含む
`_()`／`N_()`／`NC_()` 文字列を全数列挙し、6 種類の msgid（8 か所）であることを確認。
完全性の裏付け：コミット前の `English.pot` の "WinMerge" 項目は 13 行で、内訳は
ヘッダ 3 行、`${WINMERGE_HOME}` 1 行、ProgID 2 行、フィルタテンプレート 1 行、
.cpp 由来 5 項目（複数行 msgid のため 5 行）＋.sct タイトル 0 行（.sct は.pot
抽出対象外）であり、列挙した 6 種類と一致した（フィルタテンプレートを除く 5 種類が
.pot の.cpp 由来 5 項目と一致）。変更した 7 か所の.cpp 文字列はすべて.po の
"WizMerge" 版 msgid（訳付き）と文字列完全一致することを確認（複数行 msgid の
結合後も含む）。8 件の.sct タイトルもすべて.po の "WizMerge" 版 msgid と一致する
ことを確認。検証スクリプトの結果：全チェック PASS、FAIL 0 件。
- (b) 変更していないもの：識別子・レジストリキー・INIキー・exe 名・ProgID・拡張子・
URL・`${WINMERGE_HOME}` が変わっていないことを grep で確認（29.2 の一覧）。
FileFiltersDlg.cpp:520 と Merge.rc:4921 が "WinMerge" のままであることも確認。
- (c) CI 緑＋成果物 2 件の存在を API で確認した（29.5）。出荷 .po の内容も成果物 ZIP の中身を直接確認した。
- (d) 回帰：表示文字列のみの変更であり、C++ のロジック変更はない（各ファイル 1 行の
文字列リテラル置換のみ。行数・改行コード・BOM は変更前と同一）。
フォルダ比較・ファイル比較の動作に影響する変更はない。
- 追加の検証：出荷.po の内容確認（CI 成果物 ZIP の中の `WinMerge/Languages/Japanese.po`
を直接確認。29.5）。

### 29.5 CI 結果（PR #25）

> 以下の 3 実行はリベース前の head（master 0aefc6dd ベース）での結果である。リベース後の最終 head での CI 実行は本節末尾に追記する（追記予定）。

- PR：https://github.com/askhim659/winmerge/pull/25
  （ブランチ `merge20/phase4-w2f`。マージはしないこと。コーディネーターが検証してマージする。）
- コード 17 コミットでの CI 実行 45744972468（`pull_request`、head `4739130703f09f3e48b5c6561d6951eee16e2d47`）：**結論 success**。
  ジョブ 139658697017（`build`）：completed／success。GoogleTest：`[  PASSED  ] 608 tests.`。
  ジョブログの "FAILED" は 0 件（"error " の 4 件は MSBuild の "Structured output is enabled..." という情報行のみ）。
  成果物 2 件：`WinMerge-x64.zip`（ID 15082313806、26,302,958 バイト）と
  `WinMerge-x64-Setup-x64.exe`（ID 15083556380、39,738,091 バイト）、
  いずれも 2027-02-24T02:49:36Z 失効。
  ジョブログで MergeLang ステップ（`Creating POT file from Merge.rc...`、
  `Updating PO files from POT file...`、`All PO files updated`）と番兵チェック
  （`Checking bundled Japanese.po for Merge20 strings...`／
  `Merge20 strings present in the bundled Japanese.po.`）の通過を確認。
  CI 失敗 0 回（初回実行で success）。
- 成果物 ZIP の直接確認：外側 ZIP の中身は `wizmerge-2.16.60-21e3ac5-x64-exe.zip` 1 件。
  内側の `WizMerge/Languages/Japanese.po`（W2 の改名によりフォルダ名が `WinMerge` から
  `WizMerge` になっている）は 180,744 バイト・2,083 項目、
  SHA256 `7fccca5c2db3f6d2883c9d725df0267f51d89d86878d0d41fcfbf703d255acc7`。
  変更した 5 件の "WizMerge" 版 msgid はすべて訳付きで存在し、旧 "WinMerge" 版 5 件は
  存在しない（出荷 .po に未訳で追加されなくなったことを確認）。
  .sct タイトル 8 件（"WizMerge Plugin Options"）もすべて存在し、
  旧 "Merge20Exclude.sct WinMerge Plugin Options" は存在しない。
  フィルタテンプレートの "WinMerge" 版 msgid（訳付き）も存在する。
- 記録のみのコミット後の最終 head（`f6d7950b3a5aa2e1e557631e5b7110a7b6be3b0d`）での CI 実行 45746328162：**結論 success**。ジョブ 139663172908（`build`）：completed／success。成果物 2 件：`WinMerge-x64.zip`（ID 15084195816、26,302,693 バイト）と `WinMerge-x64-Setup-x64.exe`（ID 15084160838、39,741,525 バイト）、いずれも 2027-02-24T03:02:27Z 失効。コードは最終 head と同一のため、GoogleTest・番兵・出荷 .po の内容は上記の実行 45744972468 と同じである（本コミットは記録のみ）。
- リベース後の最終 head（`62c64210442afbb60eed93c9a6a1f56358a66f40`）での CI 実行 45755564247：**結論 success**。ジョブ 139693266827（`build`）：completed／success。GoogleTest：`[  PASSED  ] 627 tests.`（D3/D4 の新規テスト 19 件を含む。リベース前は 608 件）。ジョブログの "FAILED" は 0 件。番兵チェック（`Merge20 strings present in the bundled Japanese.po.`）も通過。成果物 2 件：`WinMerge-x64.zip`（ID 15087494108、26,317,841 バイト）と `WinMerge-x64-Setup-x64.exe`（ID 15088081570、39,756,286 バイト）、いずれも 2027-02-24T04:29:50Z 失効。成果物 ZIP の直接確認：外側 ZIP の中身は `wizmerge-2.16.60-bbc9235-x64-exe.zip` 1 件。内側の `WizMerge/Languages/Japanese.po` は 181,716 バイト・2,101 項目、SHA256 `1add635f35e608efb12397d0ceb96cf2769ae5e8f64022c8443d75de6eed72fe`。変更した 5 件の "WizMerge" 版 msgid と .sct タイトル 8 件はすべて訳付きで存在し、旧 "WinMerge" 版は存在しない。D3/D4 の 17 項目（"No unreviewed files" 等）も存在する。

### 29.6 Windows 実機で確認してもらう項目（未検証）

- 以下の項目は Linux VM では確認できないため、実機での確認が必要。
「できた」ではなく「未検証」として残す。
- 1. 32-bit 版でクリップボード比較が使えない場合のメッセージが日本語
（「32bit版 WizMerge ではクリップボード比較はサポートされていません。」）で出ること。
- 2. 前回クラッシュ検出時のメッセージが日本語
（「以前の WizMerge のクラッシュを検出しました。可能であればご報告ください。」）で出ること。
（前回のクラッシュを再現させる必要があるため、実機での確認は任意。）
- 3. 他アプリで更新されたファイルを開き直すときの確認メッセージが日本語
（「このファイルは WizMerge 以外のアプリケーションで修正されています。…」）で出ること。
- 4. プロジェクトファイルを開く／保存するダイアログのフィルタ表示が
「WizMerge プロジェクト ファイル (*.WinMerge)」になっていること
（拡張子は `*.WinMerge` のまま）。
- 5. About ダイアログ／スプラッシュの GPL 文が "WizMerge comes with ABSOLUTELY NO WARRANTY..."
になっていること。
- 6. プラグインの設定ダイアログ（MS Excel／Word／PowerPoint／Visio ファイル比較、
PrediffLineFilter、AI、ApplyPatch、Merge20Exclude）のタイトルが日本語
（「○○.sct WizMerge プラグインオプション」）で出ること。
特に ApplyPatch.sct と Merge20Exclude.sct（本 PR で.po を追加／改名したもの）。
- 7. フィルタテンプレートが見つからないときのメッセージが従来どおり日本語で出ること
（「ファイル %1 を以下の WinMerge/Filters フォルダーにコピーしてください:」。
変更していないことの確認。表示されるフォルダパス `%2` が実在の
`WinMerge\Filters` フォルダであること）。
- 8. 既存のファイル比較・フォルダ比較が従来どおり動くこと（回帰確認）。

### 29.7 残課題・引き継ぎ

- D3 は §27、D4 は §28 でマージ済みのため、節番号の衝突は解消した（リベース前は D3 のブランチが §26 を使っており、本節冒頭の注意書きに記録していた）。
- 診断ファイルのヘッダ（ConfigLog "WinMerge Configuration Log"、CrashLogger
"WinMerge Crash Information"）と HTML レポートのタイトル（"WinMerge File Compare Report"）
は `_()` 経由でないため今回は不変とした。表示名として変えるかどうかは別途判断が必要。
（PatchHTML／FileCmpReport のタイトルはテストが参照している可能性があるため、変える場合は
テストの確認も行うこと。）
- `WinMerge\Filters` 等のユーザーデータフォルダ名そのものを "WizMerge" に変えるかどうかは
未決定（変えると既存の設定・フィルタ・学習データの移行が必要になるため、別途検討）。
- ラベルを「Wiz」から「WizMerge」にするかはユーザーに未確認（W1 §26.3 参照）。注：本節では全角括弧の記法がツール経由で欠落する問題があったため「Wiz」と表記した。意味は W1 の「Wiz」ラベルと同じ。

## 30. Phase 4 D5（分割・再結合）— ブランチ `merge20/phase4-d5`

PR #27。2026-11-26。master 3adc2cfa（W2-followup＝PR #25 マージ済み）から分岐（リベース。元は 56136579＝D4＝PR #26 から分岐）。24ファイル、24コミット＋記録。設計書 §5.2 D5・§4.2・§3.8・T-4-10、仕様書 §5.2・§4(g)。D4＝§28、W2-followup＝§29。

> リベースについて：D5 作業中に W2-followup（PR #25）が master 3adc2cfa としてマージされた。ブランチは `commit_on_parent.py` で 3adc2cfa へ載せ直し、24コミットを同順で再演した。重なったのは `Src/MergeDoc.cpp`（W2 の DoSave 文言 `WinMerge`→`WizMerge` の 1行）と `Translations/WinMerge/Japanese.po`（プラグイン題名の改名 1件＋ApplyPatch 題名の新規 1件）のみで、両方とも D5 の変更と行が重ならず、手でマージした。PR #27 は開いたまま（載せ直し直後に state=open・mergeable を確認）。

### 30.1 変更一覧

| コミット | ファイル | 内容 |
|---|---|---|
| 8dedf5a3 | Src/Merge20/Merge20Split.h | 純粋モジュール（新規）。行ペアリング・分割可否・分割範囲・グループ往復・先頭行一致 |
| dc732eea | Src/Merge20/Merge20Split.cpp | 同上の実装。MFC 不使用 |
| 25ce9ff1 | Testing/GoogleTest/Merge20/Merge20Split_test.cpp | 単体テスト（新規、8テスト）。2-way／旧のみ／新のみ／3-way、分割可否ガード、グループ往復 |
| d1c79052 | Src/Merge.vcxproj | Merge20Split を登録（pch.h グループ） |
| 91fd78f8 | Src/Merge.vcxproj.filters | 同上 |
| 2c92d5bc | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | Merge20Split とテストを登録 |
| c6fb83f6 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同上 |
| bb26da3e | Src/DiffList.h | `InsertDiff`／`EraseDiffs` 宣言 |
| 133813ff | Src/DiffList.cpp | 同上の実装（範囲チェック付き。呼出元が `ConstructSignificantChain()` を呼ぶ） |
| 2275b47e | Src/Merge20/Merge20Types.h | `Merge20SplitGroup`（op／begin[3]／end[3]／dbegin／dend／blank[3]／rows）と `Merge20FileState::splitGroups` |
| 39b21ac5 | Src/MergeDoc.h | 分割／再結合 API（`Merge20CanSplitDiff`／`Merge20CanRegroupDiff`／`Merge20SplitDiff`／`Merge20RegroupDiff`／`Merge20ReapplySplitGroups`、コマンドハンドラ）とサイドカー `m_merge20SplitGroups` |
| e765cee3 | Src/MergeDocResultPane.cpp | 分割／再結合の本体。判定引き継ぎ、学習候補の再計算、結果ペイン右クリックメニューのハンドラ、手編集ガード |
| a19be3c4 | Src/MergeDoc.cpp | コマンドの配線、Rescan でのサイドカークリア。W2-followup の DoSave 文言改名をマージ済み |
| 676b8b45 | Src/Merge20/Merge20FocusDock.cpp | フォーカスドックの分割／戻すボタンを有効化（D4 は表示のみ・無効化） |
| d68dbc8c | Src/resource.h | `ID_MERGE20_SPLIT_DIFF` 38954、`ID_MERGE20_REGROUP_DIFF` 38955、`_APS_NEXT_COMMAND_VALUE`→38956 |
| 2671eb48 | Src/Merge.rc | 4つの結果メニュー（2-way／3-way、ポップアップ／メイン）に Split／Regroup を追加 |
| 3e40d86e | Translations/WinMerge/Japanese.po | 手編集ガード警告の日本語訳 1件。W2-followup のプラグイン題名改名をマージ済み |
| 9355aa89 | Src/Merge20/Merge20Session.h | `Merge20SessionFile::splits` |
| 97c136bd | Src/Merge20/Merge20Session.cpp | セッション JSON の任意 `splits` 配列。範囲チェック付きパース（不正な項目は捨てる） |
| 1339bfed | Testing/GoogleTest/Merge20/Merge20Session_test.cpp | `SplitsRoundTrip` テスト（往復／v1 互換／不正項目の破棄） |
| b7fd19cb | Src/DiffContext.h | `SetMerge20SplitGroups` 宣言 |
| 9ea46b10 | Src/DiffContext.cpp | 同上の実装 |
| 41714767 | Src/Merge20/Merge20SessionDoc.h | `Merge20SessionRestoreMergeDoc` の注記（分割の再適用） |
| b262f3c9 | Src/Merge20/Merge20SessionDoc.cpp | 分割グループの退避／適用。復元時は判定復元の前に分割を再適用 |

### 30.2 設計判断（T-4-10 ほか）

- T-4-10 の結論は完全分割（Option A）。理由：(1) マージセッション中は `Rescan()` が `RESCAN_SUPPRESSED` を返すため、差分リストがセッション中に再構築されることはない。(2) 行ごとの部分範囲は親のゴースト配置から算術で導ける（各ペインの実線は見かけ範囲の先頭に詰まっており、残りがゴースト）。バッファの再プライミングは不要。(3) DIFFRANGE を分割すれば `BuildMergeResult`・`ResultChooseSources`・セッション・学習候補・移動・位置ビューなど既存の仕組みがそのまま使える。結果ペイン表示だけに落とす代替案は、行単位の判定を保持する影の仕組みが別途必要になるため却下した。
- 判定の引き継ぎ（設計書 §4.2 の既定）：親が Chosen／Auto なら `srcPanes` を全行へ引き継ぐ（Auto は `Merge20SetResultSegmentState` で復元）。Unresolved／Conflict の親は行ごとに未解決のまま。再結合は先頭行の判定を引き継ぐ（HTML の regroupHunk と同じ）。
- 行ごとの op 規則：全ペインに行がある→親の op。1ペインだけ→そのペインの `OP_*ONLY`。3-way で 2ペイン→`OP_DIFF`（2ペイン用の op はないため、競合の意味を保つ）。`OP_TRIVIAL`／`OP_NONE` と 1行の範囲は分割不可。さらに `dend-dbegin+1 == rowCount` を要求し、未プライミングのバッファを弾く。
- 学習：分割そのものは記録しない（人の明示的な差分判定ではないため）。分割後に行を判定したときだけ、既存の記録口で記録される。分割・再結合の直後に `Merge20LearnRefreshCandidates` を呼んで候補を再計算する。却下集合のキーは分割前の差分に対するものなので、却下した親の行に候補が再表示されることがある（設計書 §4.2 の扱いで許容）。
- 同一性（設計書 §4.2 の 3層目）：`CMergeDoc` のサイドカー `std::vector<Merge20SplitGroup>`。インデックスは保持せず、先頭行の同一性（`begin[]`＋`dbegin`）で見つけ直す。再結合時はグループ記録を先に落とす（親自身が先頭行に一致するため）。
- セッション：`splits` は任意項目（v1 ファイルは `splits` なしで読める）。復元は判定ループの前に `Merge20ReapplySplitGroups` を呼び、親を範囲の同一性で探して格納順に再適用する。親が合わなくなったグループは黙って飛ばし、その行の判定も従来どおり飛ばす。
- 手編集ガード：結果セグメントに Edited が 1件でもあれば分割・再結合を拒否し、警告メッセージボックスを出す（`BuildMergeResult()` が結果バッファ全体を作り直すため、手編集が失われる）。再構築で undo 履歴が消えるのは既知の副作用。
- UI は 3箇所のうち 2箇所：フォーカスドックのボタン（D4 で表示済み・無効化済みを有効化）と、メイン画面＝結果ペインの右クリックメニュー＋メインメニュー（4メニューすべて）。スナイパー側のボタンは D6 と調整（`Merge20SplitDiff`／`Merge20RegroupDiff`／`Merge20Can*` は public）。

### 30.3 CI 結果（最終 head b262f3c9）

- 実行 ID：45758285091、結論：success。ジョブ 139701990552（build）success。
- GoogleTest：636件全パス（627＋新規 9件）。全文ログ `[==========] 636 tests from 51 test suites ran. (1510 ms total)`／`[  PASSED  ] 636 tests.`。`[  FAILED  ]` は 0件（「FAILED」を含む 4行は .po チェック出力中の `IDS_FILESAVE_FAILED`／`IDS_MERGE_RESULT_SAVE_FAILED` 文字列）。Merge20Split の 8テストと `Merge20Session.SplitsRoundTrip` も全パス。CI の .po チェックも通過（"Merge20 strings present in the bundled Japanese.po."）。
- 成果物：WinMerge-x64.zip（ID 15089706058、26,327,263 バイト）、WinMerge-x64-Setup-x64.exe（ID 15090135355、39,767,979 バイト）。両方とも失効日 2027-02-24T04:55:51Z、未失効。
- エラー 0件（全文ログに `error C*`／`error LNK*`／`error MSB*` なし）。
- 注：リベース前の head（46960242）の実行 45756835370 も success。リベース後の head b262f3c9 に対する実行が本節の対象。記録コミット自体は MERGE20_NOTES.md のみでコードはこの head と同一のため、記録コミット後の最終 head に対する実行は別途確認する（親への報告に記載）。

### 30.4 検証ゲート

- (a) 仕様・設計：仕様書 §5.2 の分割・再結合（行単位で判定、再結合は先頭行の判定）は実装。設計書 §5.2 D5・§4.2・§3.8・T-4-10 は §30.2 のとおり。スナイパーのボタンは D6 へ（設計書どおり調整事項）。
- (b) 利用可否：分割→行単位判定→再結合→保存・復元の流れはコード上の通知点（`BuildMergeResult`／`ResultChooseSources`／`Merge20LearnRefreshCandidates`／`UpdateAllViews`／セッションの退避・再適用）で網羅。実機での操作確認は未検証（チェックリストは §30.5）。
- (c) 表示：新規 `_()` 文字列は警告文 1件のみで日本語訳あり。Split／Regroup は既存の msgid（「分割」／「戻す」）を再利用。新規レイアウトなし。実機での表示確認は未検証。
- (d) 回帰：フォルダ比較・ファイル比較の経路は変更していない（`InsertDiff`／`EraseDiffs` は分割・再結合からのみ呼ぶ。メニュー追加のみ）。実機での回帰確認は未検証。
- 副産物：master の Japanese.po に二重エンコード等の破損がないか全走査した。重複 msgid（''・'None'・'Disabled'・'Binary'・'Folder'・'Diff'）は master にもともとあるもので、D5 の追加によるものではない。English.pot は master で 25件の .po msgid を欠いている（W2-followup 自身の新規項目を含む）ため、D5 では .pot を変更していない（過去スライスと同じ扱い）。

### 30.5 D6 への引き継ぎと残課題

- スナイパーの分割／戻すボタン（D6）。API は public（`Merge20SplitDiff`／`Merge20RegroupDiff`／`Merge20CanSplitDiff`／`Merge20CanRegroupDiff`）。
- 分割・再結合は結果に手編集があると拒否される。手編集してから分割したい場合は、先に結果を保存するか手編集をやめること。再構築で undo 履歴が消える。
- 分割はセッション内のみ有効。再スキャン・セッション終了で分割は消える。セッション保存・再開では `splits` で復元されるが、親の差分が変わっていたグループは黙って飛ばされる（その行の判定も復元されない）。
- D4 からの残課題は変わらず：ワードラップ ON 時のスクロールずれ、結果ペインでの手編集のドック反映遅れ、3-way 片側差分の判定ボタンなし、フォーカス結果ビューの構文ハイライト未実装。
- 実機チェックリスト：(1) 3行以上の変更差分でフォーカスドックの分割ボタンを押すと、行ごとの差分になること。(2) 分割した各行を個別に判定できること（旧／新／両方／不採用）。(3) 戻すボタンで元の差分に戻り、先頭行の判定が引き継がれること。(4) 結果ペインの右クリックメニューからも分割・戻すができること。(5) 分割した状態でセッション保存→再開すると分割が復元され、行ごとの判定も復元されること。(6) 分割した親を学習で却下していた場合、行に候補が再表示されることがあること。(7) 結果を手編集した状態で分割しようとすると警告が出て拒否されること。(8) 日本語が文字化けせず表示されること（警告文・メニュー）。(9) 既存のフォルダ比較・ファイル比較が従来どおり動くこと。

## 31. Phase 4 D6（オートスナイパー）— ブランチ `merge20/phase4-d6`

PR #28。2026-11-26。master f20571b0（D5＝PR #27 マージ済み）から分岐。21コミット＋記録。設計書 §5.2 D6・§3.6・§3.3、仕様書 §3 (4/4)・§4(g)・§5.4。D5＝§30。

### 31.1 変更一覧

| コミット | ファイル | 内容 |
|---|---|---|
| fd173374 | Src/Merge20/Merge20Sniper.h | 純粋モジュール（新規）。折り返し移動・フォント範囲・px→行変換・ドラッグ判定・レイアウト切替 |
| 33e13ae0 | Src/Merge20/Merge20Sniper.cpp | 同上の実装。MFC 不使用 |
| e3ffef20 | Testing/GoogleTest/Merge20/Merge20Sniper_test.cpp | 単体テスト（新規、11テスト） |
| 83f1a4c7 | Src/Merge.vcxproj | Merge20Sniper を登録（pch.h グループ） |
| d715aa55 | Src/Merge.vcxproj.filters | 同上 |
| 4732c82f | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | Merge20Sniper とテストを登録 |
| c8ec52e5 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同上 |
| b83c7a2a | Src/Merge20/Merge20Sniper.cpp | スワイプは \|dx\| > \|dy\| を厳密に要求（HTML initSniper と一致） |
| d4193cc0 | Testing/GoogleTest/Merge20/Merge20Sniper_test.cpp | 45度ドラッグはスクロールになるテストを追加 |
| 277a27f6 | Src/Merge20/Merge20SniperView.h | スナイパーパネル用読み取り専用ビュー（新規） |
| a89777d3 | Src/Merge20/Merge20SniperView.cpp | 同上の実装（同期通知・キー・ドラッグ） |
| 537c35b1 | Src/Merge20/Merge20SniperDlg.h | スナイパーオーバーレイダイアログ（新規、モーダルレス） |
| 0b132090 | Src/Merge20/Merge20SniperDlg.cpp | 同上の実装（3パネル・同期・レイアウト・フォント・分割／戻す） |
| fea54f41 | Src/resource.h | IDD_MERGE20_SNIPER 297、IDC_MERGE20_SNIPER_* 1767〜1777、_APS_NEXT_* 更新 |
| b0361901 | Src/Merge.rc | IDD_MERGE20_SNIPER の空ダイアログテンプレート（IDD_MERGE20_LEARN の END の直後） |
| 3448f264 | Src/Merge.vcxproj | SniperDlg／SniperView を登録（StdAfx.h グループの既定エントリ） |
| 2d76ff52 | Src/Merge.vcxproj.filters | 同上 |
| 66741a2e | Src/MergeEditFrm.h | フレームがスナイパーダイアログをメンバで所有＋ShowMerge20Sniper 宣言 |
| 40111017 | Src/MergeEditFrm.cpp | 専用モード出入・スナイパートグル・OnClose での開閉、スナイパーキーの転送 |
| 31853244 | Src/MergeResultView.cpp | ダブルクリックフック（スナイパー ON かつ専用モード内のみ） |
| 9ac8c494 | Translations/WinMerge/Japanese.po | スナイパー UI の日本語訳 9件（すべて【Wiz】付き） |

### 31.2 設計判断（T-4-2 ほか）

- T-4-2 の結論は CMergeEditView の再利用をしない（設計書の推奨案どおり）。理由：CMergeEditView は CMergeDoc::m_pView[group][buffer] に登録され UpdateAllViews／SelectDiff に参加する表示専用でないビューだが、スナイパーの 3 パネルは表示だけのビューであるため。D4 の CMerge20FocusResultView の前例（独自 CGhostTextBuffer を持つ CGhostTextView 派生、Create は `AFX_WS_DEFAULT_VIEW & ~WS_BORDER`＋nullptr コンテキスト、WM_INITIALUPDATE を明示送信）をそのまま流用し、新規 CMerge20SniperView を作った。行の抜き出しは GetPaneApparentLinesText（OLD＝0番ペイン、NEW＝最終ペイン）と Merge20GetFocusResultText（MERGED、D4 の API を再利用）。
- 3-way の NEW パネルは最終ペイン（nBuffers-1）とした。HTML のスナイパーは OLD／NEW／MERGED の 3 パネルであり、3-way の中間（base）ペインの表示場所は仕様書・設計書に定めがないため。MERGED パネルが base 由来の行も含めて結果を表示する。
- モーダルレスダイアログの所有はフレームのメンバ（CMergeEditFrame::m_wndSniper）。コードベースにモーダルレスダイアログの前例がなく PostNcDestroy による自己削除も見つからなかったため、作り直し・削除のいらない最も保守的な形にした。初回のみ Create し、以後は表示／非表示だけを切り替える。閉じる操作は OnCancel を上書きして非表示にする（基底の OnCancel はウィンドウを破棄するため呼ばない）。
- ダブルクリックフックは CMergeResultView::OnLButtonDblClk の既存処理の途中に入れた。スナイパー ON かつ専用モード内のときだけ横取りし、OFF 時は既存動作（差分選択）を完全に維持する。既存の「すでに選択中の差分は早期 return」という分岐より前に置いたため、選択中の差分をダブルクリックしてもオーバーレイが開く（HTML では現在差分のダブルクリックもスナイパーを開く）。比較ペイン側のダブルクリック（単語選択）は触っていない（設計書 §3.6 の指定どおり）。
- 専用モードを出るとスナイパーも自動 OFF：ExitDedicatedMode は D2 の時点で OPT_MERGE20_SNIPER_ON を false にしていたが、ダイアログ自体を閉じる処理がなかったため D6 で CloseSniper() を追加した。スナイパートグルを OFF にしたときとフレームを閉じるときにも CloseSniper() を呼ぶ。
- キーの経路は 3 重にした。MFC のポンプがモーダルレスダイアログの PreTranslateMessage を呼ぶ保証がないため、(1) パネル側の OnKeyDown（フォーカスがパネルにある通常時）、(2) ダイアログ自身の PreTranslateMessage（フォーカスがヘッダボタンにあるとき）、(3) フレームの PreTranslateMessage からの転送（(2) が呼ばれない場合の保険）。キー対応は HTML の initKeys（1815行）と同一：J／→＝次差分、K／←＝前差分、↑／↓＝120px、PgUp／PgDn／Space＝600px、Esc＝閉じる。
- ドラッグは HTML の initSniper（mousemove 判定）と同一にした。横 50px 以上かつ |dx| > |dy|（厳密）を満たした時点で即座に前／次差分へ移動する（mouseup 待ちではない）。左スワイプ＝次、右スワイプ＝前（ページめくりの向き）。縦ドラッグは 8px を超えかつ縦が優勢なときにパネルを直接スクロールし（scrollTop = top0 - dy と等価な行単位の再計算）、ScrollToSubLine の上書き経由で他 2 パネルへ同期する。純粋モジュールの Merge20SniperDragAction も HTML の条件（|dx| >= 50 && |dx| > |dy|）に合わせ、45度ちょうどのドラッグはスクロールになることを単体テストで検証した（初期実装の |dy| > |dx| では 45度がスワイプになっていたため修正）。
- レイアウトボタンは「切り替え先」を表示する（3列表示中に「3段表示」と出る。HTML の「☰ 3段」トグルと同じ考え方）。フォントは A-／A+ で ±2px（HTML snZoom(±2) と同一）、14〜28px でクランプし、OPT_MERGE20_SNIPER_FONT に保存する。レイアウトは OPT_MERGE20_SNIPER_LAYOUT に保存する（どちらも D0 のキー）。
- スナイパー内の ✂ 分割／↩ 戻すは D5 の public API（Merge20SplitDiff／Merge20RegroupDiff／Merge20CanSplitDiff／Merge20CanRegroupDiff）を呼ぶだけ（設計書どおり）。分割・再結合の本体が SetCurrentDiff＋UpdateAllViews まで行うため、ダイアログ側は呼び出し後に GetCurrentDiff() を読み直して表示する（再結合後は先頭行の差分になる）。手編集ガードの警告は D5 本体が出す。
- スナイパーでの移動はメイン画面にも反映する（MoveDiff が CMergeEditView::SelectDiff を呼ぶ）。オーバーレイを閉じたときにメイン画面がスナイパーで最後に見ていた差分にいるようにするため。
- HTML との意図的な差分：HTML のスナイパーオーバーレイには判定ボタン（新維持／旧置換／両方／旧挿入／不採用）があるが、D6 のタスク範囲は分割／戻すボタンのみのため付けていない。判定はフォーカスドック（D4）または結果ペインで行う。後続スライスで追加する場合は Merge20FocusApplyJudgment を呼ぶ形になる。
- 新規 _() 文字列は 9件で、既存の msgid は流用した（「分割」「戻す」は D5 までの【Wiz】訳あり、「差分 %1/%2（%3）」「未確認」等の状態語は D4 のもの）。「New」は既存 msgid が「新規」（新規ファイルの意味）のため使わず、新規 msgid「New side」（【Wiz】新）とした。

### 31.3 CI 結果（PR #28）

最終 head `f2b7d76e640764e19e817ca77b383ff64fa8c186`（28 commits）の CI run **45766219011**（2026-11-26 15:21 JST 完了）: conclusion **success**。job `build`（139727649719）success。全文ログで確認済み。

- GoogleTest: `[  PASSED  ] 647 tests.`（647 tests from 52 test suites ran, 1247 ms）。D5 時 636 + 新規 Merge20Sniper 11件（WrapNextMovesForward / WrapNextWrapsFromLastToFirst / WrapNextNoDiffAtAll / WrapPrevMovesBackward / WrapPrevWrapsFromFirstToLast / WrapPrevNoDiffAtAll / ClampFontKeepsInsideRange / StepFontStepsByTwoAndClamps / ScrollLinesConvertsPixels / DragActionNeedsFiftyPixelsHorizontal / ToggleLayoutSwitchesBothWays）が全 OK。FAILED は 0件。
- コンパイル／リンクエラー: `error C*` / `error LNK*` / `error MSB*` は 0件。
- .po チェック: `Merge20 strings present in the bundled Japanese.po.` を確認。
- 成果物（API で確認）: `WinMerge-x64.zip`（ID 15092616422、26,333,744 bytes）と `WinMerge-x64-Setup-x64.exe`（ID 15092467082、39,792,788 bytes）。失効日は両方とも 2027-02-24T06:09:13Z、expired: false。
- 失敗した先行 run（修正の記録）: head `4d9c1224` の run 45764098391 と head `dd099c71` の run 45764443411 は failure。原因は 3件。(1) `MergeEditFrm.cpp` の TRACE0 文字列リテラル内に実改行が入っていた（C2001/C1057。Python heredoc で `\n` が実改行に展開されていた）。(2) `CMergeDoc::GetPaneApparentLinesText` が private（C2248）。(3) `CCrystalTextView::GetLineHeight` が protected（C2248）。修正コミット 4件（`MergeEditFrm.cpp` の `\n` 復旧、`MergeDoc.h` で当該宣言を public へ移動、`Merge20SniperView.h` に public アクセサ `GetSniperLineHeight` を追加、`Merge20SniperDlg.cpp` で同アクセサ経由に変更）。その後の run 45765083881（head `421bbdc6`）も failure。原因は 1件: `CWnd::PreTranslateMessage` は protected のためフレームから呼べない（C2248）。修正は `MergeEditFrm.cpp` のみ 1行: フレームの転送先をダイアログの public メソッド `HandleSniperKey`（既存、ビューも利用）に変更し、コメントも更新。以上の修正が最終 head `f2b7d76e` に含まれ、success となった。なお `GetPaneApparentLinesText` の public 化と `GetSniperLineHeight` の追加は、D6 のスナイパー以外からの利用を増やすものではなく、定義・挙動の変更はない。


### 31.4 検証ゲート

- (a) 仕様・設計：仕様書 §3 (4/4)（ダブルクリックのみ発動、3画面同期スクロール、横スワイプ／J・K・矢印キー移動、Esc で閉じる、文字サイズとレイアウトは保存）、§4(g)（キー一覧）、§5.4（スナイパートグル）を実装。設計書 §5.2 D6・§3.6・§3.3 は §31.2 のとおり。T-4-2 は解消（再利用せず D4 前例で新規ビュー）。
- (b) 利用可否：ON→ダブルクリック→3パネル表示→J／K・矢印キー移動（末尾で先頭へ回る）→Esc／✕で閉じる、OFF 時は従来の差分選択、という流れはコード上の接続点（OnLButtonDblClk→ShowMerge20Sniper→ShowDiff、MoveDiff の折り返し、HandleSniperKey／OnCancel）で網羅。実機での操作確認は未検証（チェックリストは §31.5）。
- (c) 表示：新規 _() 9件はすべて Japanese.po にあり、msgstr はすべて【Wiz】付き。コード側の全 _() と .po の一致をスクリプトで検証済み（新規ファイルの _() の欠落は 0件）。レイアウトは OnSize での再計算＋右寄せボタン＋狭い場合の位置ラベル非表示で崩れにくくした。実機での表示確認は未検証。
- (d) 回帰：フォルダ比較の経路は変更していない。ファイル比較は結果ビューのダブルクリックに条件付き分岐を 1件追加したのみで、OFF 時・専用モード外は既存コードと同一経路。実機での回帰確認は未検証。
- GoogleTest：Merge20Sniper 11テストを新規追加。ローカルで g++ ハーネスによる同等チェックは全 OK（CI での実行結果は §31.3 に記入する）。

### 31.5 Windows 実機での確認手順（D6 受け入れテスト）

1. 実フォルダでファイル比較を開き、View メニューから比較専用モードに入る。専用バーの「Sniper」トグルを ON にする。
2. 結果ペインの差分行をダブルクリックすると、全画面オーバーレイが開き、該当差分だけが OLD／NEW／MERGED の 3 パネルで表示されること。タイトルが「【Wiz】オートスナイパー」、位置が「差分 N/M（状態）」と日本語で表示されること。
3. 3 列表示（既定）であること。レイアウトボタンを押すと 3 段表示に切り替わり、もう一度押すと 3 列に戻ること。切り替え後に WinMerge を再起動し、レイアウトが保存・復元されること。
4. A+ を押すと文字が大きくなり、A- で小さくなること（14〜28px で止まること）。再起動後に文字サイズが保存・復元されること。
5. 1 つのパネルをマウスホイールでスクロールすると他の 2 パネルも同じ位置へ動くこと（同期スクロール）。
6. J キー・→ キーで次の差分へ、K キー・← キーで前の差分へ移動できること。末尾で J を押すと先頭へ、先頭で K を押すと末尾へ回ること。ヘッダの「前差分」「次差分」ボタンでも同じ移動ができること。移動後にメイン画面（オーバーレイを閉じた後）の現在差分も移動先になっていること。
7. ↑／↓ キーで少しスクロールし、PgUp／PgDn／Space キーで大きくスクロールすること（3 パネル同期）。
8. パネルを左へ 50px 以上ドラッグすると次の差分へ、右へドラッグすると前の差分へ移動すること。縦ドラッグでパネルがスクロールすること（3 パネル同期）。
9. Esc キーまたは「スナイパーを閉じる」ボタンでオーバーレイが閉じること。
10. スナイパーを OFF にして差分行をダブルクリックすると、従来どおり差分が選択されるだけでオーバーレイは開かないこと。専用モード外（通常モード）でスナイパー ON のままダブルクリックしてもオーバーレイは開かないこと（通常モードではトグル自体が専用バーにしかないため、実質的には専用モードを出た時点で OFF になることを確認する）。
11. 専用モードを出るとスナイパーが OFF になり、開いていたオーバーレイが閉じること。
12. 3行以上の差分でスナイパー内の「✂ 分割」ボタンを押すと行ごとの差分になり、3 パネルの MERGED 側が更新されること。「↩ 戻す」で元の差分に戻ること。結果を手編集した状態で分割しようとすると警告が出て拒否されること。
13. 日本語が文字化けせず表示されること（タイトル・ボタン・パネルラベル・位置表示）。
14. 既存のフォルダ比較・ファイル比較が従来どおり動くこと（特に結果ペインのダブルクリックによる差分選択、比較ペインのダブルクリックによる単語選択）。

### 31.6 D7 への引き継ぎと残課題

- スナイパーに判定ボタン（新維持／旧置換／両方／旧挿入／不採用）がない（§31.2 の意図的な差分）。追加する場合は Merge20FocusApplyJudgment（D4）を呼ぶ。HTML のスナイパーにはあるため、D7 以降で検討すること。
- 3-way の中間（base）ペインはスナイパーに表示されない（§31.2）。3-way で base を見たい場合はメイン画面で確認すること。
- 縦ドラッグのスクロールはドラッグ開始時のトップ行からの再計算で、CrystalEdit 既定の選択ドラッグと併走する（選択範囲が伸びる）。実機で違和感があれば、ドラッグ中の選択を抑止する方向で見直すこと。
- 横ドラッグの水平スクロール（HTML は scrollLeft も動かす）は実装していない。パネル幅を超える長い行の横スクロールはマウスホイール／スクロールバーで行うこと。
- D4 からの残課題は変わらず：ワードラップ ON 時のスクロールずれ、結果ペインでの手編集のドック反映遅れ、3-way 片側差分の判定ボタンなし、フォーカス結果ビューの構文ハイライト未実装。
- スナイパーのフォントは theApp.m_lfDiff を元にポイントサイズから LOGFONT を作り直している（HiDPI では GetDeviceCaps(LOGPIXELSY) で補正）。高 DPI 環境での表示は実機で確認すること。

## §32 再開手順（無料プラン上限による中断に備えて、2026-11-26）

### 背景
- Muse は無料プラン（課金しない）。週間上限に達すると親・コーディネーター・子エージェントを含むすべての作業が止まる。リセットは 2026-11-27 10:10 JST。親側で 2026-11-27 10:15 JST に自動再開の予約を入れる。リセット後に親がコーディネーターの状態を確認し、中断していれば再開・立て直しする。
- アカウント切り替えは中止された（2026-11-26 15:36 JST）。このアカウントで続ける。

### 再開時の確認手順（この順で行うこと）
1. master head を確認する（API `GET /repos/askhim659/winmerge/branches/master`）。本節時点の master head は `8964c14d`（D6 マージコミット）。
2. 開いている PR を確認する（API `GET /repos/askhim659/winmerge/pulls?state=open`）。本節時点では D7（PR 未発行、ブランチ `merge20/phase4-d7`）が進行中のはず。
3. 進行中の子エージェントの状態を `subagent.list` で確認する。完了報告済みなら PR を検証してマージする。未完了なら子に続きを指示する。子が消滅していれば、ブランチの head を API で確認し、コミット済みの内容から新しい子を起動して続きを任せる。未コミットの作業は残らないことを原則とする（各子はコミット単位でプッシュする）。
4. 計画ファイル `~/workspace/goals/muse-merge-rebuild-competing-with-gemini-neon/hidden_files/merge20-complete-plan.md` の末尾を読んで進行を確認する。本 NOTES.md の最新節も確認すること。
5. マージコミット CI のポーリングスクリプト（`~/workspace/.coord/poll_*_merge_ci.sh`）は VM 再起動で消える可能性がある。再開時は API で直接確認すること（`~/workspace/.coord/check_master_run.py <フル SHA>`、`~/workspace/.coord/verify_*.py` の要領で使うこと）。
6. 中断時の master head から D7 以降のスライスを順に進める（D7→D8→D9→D10→D11）。S5／S7 は U-0-2 の実機確認待ちのまま。

### 状態保存の場所一覧
- GitHub（最優先）：本 NOTES.md（master、本節時点で §32 まで）、各 PR・ブランチ、CI 実行・成果物。
- ローカル計画ファイル：`hidden_files/merge20-complete-plan.md`（D6 マージまで記録済み）。
- ローカル検証スクリプト・CI ログ：`~/workspace/.coord/`（`verify_*.py`、`full_log.py`、`check_master_run.py`、`d*_final_log.txt`）。
- 仕様書 PDF・設計書 3 冊・カバレッジ行列・FORM 設計・監査はゴールの hidden_files 配下にある。

### 注意
- このアカウントの無料プラン上限は、上限に達した時点での作業を止めること。中途の未コミット作業を残さないことを、各子エージェントの指示に含めること。
- 【Wiz】→【Wiz】Merge のユーザー確認はまだ取れていない。確認が取れるまで【Wiz】のまま進め、後で一括置換できる形にすること。

## 33. Phase 4 D7（Grep 横断検索）— ブランチ `merge20/phase4-d7`

- PR: #29（https://github.com/askhim659/winmerge/pull/29）。最終 head（コード）`66f69f8ddb8b91c7f5488e41164e6de1d0c26f51`。マージは本記録時点では未実施（コーディネーター判断待ち）。
- 2026-11-26、master a95db0f0（§32 追加）へリベースした。旧ブランチ（base 8964c14d）では NOTES 末尾への両方追加で PR が mergeable: False になっていたため。コード内容は同一でコミット SHA のみ変わっている（本節の表は新 SHA に置き換え済み）。リベース前の head は ff774529a594b5fd4649eb5465f7778ed4cc077d。
- 設計書 §5.2 D7・§4.3・§4.1、仕様書 §3 (4/4)・§4(g)。受け入れ条件：OLD／NEW 全ファイル横断検索、ヒットから該当差分へジャンプ、1000 ファイル規模でも UI が固まらない。

### 33.1 変更一覧（1 ファイル 1 コミット、21 件）

| コミット | ファイル | 内容 |
|---|---|---|
| 9f841b0d | Src/Merge20/Merge20Grep.h | 純粋モジュールヘッダ（ヒット記録・部分一致・抜粋・履歴・上限 80 件） |
| fb570ff8 | Src/Merge20/Merge20Grep.cpp | 純粋モジュール実装 |
| 4fba40a2 | Testing/GoogleTest/Merge20/Merge20Grep_test.cpp | 単体テスト 17 件 |
| 830a0c48 | Src/Merge.vcxproj | Grep.cpp／.h 登録（pch.h グループ） |
| d713c526 | Src/Merge.vcxproj.filters | 同上 |
| 87cfaefc | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | Grep.cpp＋テスト登録 |
| 7fdd36e7 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同上 |
| 54ea18c6 | Src/resource.h | IDD 298、IDC 1778〜1784、コマンド 38956、`_APS_NEXT_*` 更新（299／1785／38957） |
| 935383dd | Src/Merge.rc | Ctrl+Shift+F 加速子＋`IDD_MERGE20_GREP` 空テンプレート（640x420、CAPTION "Cross-File Search"） |
| e534afb9 | Src/Merge20/Merge20GrepDlg.h | モーダルレスダイアログヘッダ |
| 66e5c657 | Src/Merge20/Merge20Grep.cpp | **no-op（誤操作。内容同一のためツリー変更なし。後続 f465340d が実体）** |
| f465340d | Src/Merge20/Merge20Grep.cpp | ダイアログ＋ワーカー実装（16,987 バイト） |
| 3090c328 | Src/Merge.vcxproj | GrepDlg.cpp／.h 登録（StdAfx.h グループの既定エントリ） |
| 01af066d | Src/Merge.vcxproj.filters | 同上 |
| afb88636 | Src/Merge20/Merge20GrepDlg.cpp | 数値→文字列を既存の `strutils::to_str` に統一 |
| c5ce86fc | Src/DirView.h | Grep ダイアログのメンバと OnMerge20Grep／一覧構築／ジャンプの宣言を追加 |
| 05d83f00 | Src/DirView.cpp | 起動・一覧構築・ヒットからのジャンプ（GotoLine＋SelectDiff）を実装 |
| 47a1cddc | Translations/WinMerge/Japanese.po | 新規 `_()` 文字列 11 件の日本語訳を追加（【Wiz】付き） |
| b5eccaef | Src/Merge20/Merge20GrepDlg.h | CI 修正（C3668）：OnDestroy の `override` を外し afx_msg ハンドラ宣言に変更 |
| c6298e67 | Src/Merge20/Merge20GrepDlg.cpp | CI 修正（C2589）：`std::max` → `(std::max)(...)`（既存コードの慣例） |
| 66f69f8d | Src/DirView.cpp | CI 修正（C2589 予防）：`std::min`／`std::max` → `(std::min)(...)`／`(std::max)(...)` |

### 33.2 設計判断（T-4-9 ほか。判断理由付き）

- **T-4-9（対象テキスト取得方法）の結論**：未開封ファイルのテキストはワーカー側でディスクから直接読む（`codepage_detect::Guess(path, OPT_CP_DETECT)`＋`UniMemFile`＋`ReadString` ループ。`CDiffTextBuffer::LoadFromFile` の手順から unpacker／prediffer を除いたもの）。理由：CDiffContext は比較結果（DIFFITEM）のみ持ちテキストキャッシュなし。CMergeDoc をファイルごとに開くのはビュー／文書生成を伴いバックグラウンド不可。**帰結（仕様との意図的差分）**：Grep はディスク上の生テキストを検索する（unpacker／prediffer 処理後のテキストではない）。バイナリはスキップ（シングルバイト符号時の先頭 64KB 内 NUL 検査。UTF-16 は NUL を含むため対象外）。
- **バックグラウンド走査**：DirScan の Poco ThreadPool 前例にならい `Poco::ThreadPool(1,1)`＋単一ワーカー。単一にした理由：ヒット順序をフォルダ一覧順に保つため（HTML との一致）と 80 件カットオフを正確にするため。進捗は `PostMessage`（`WM_MERGE20_GREP_PROGRESS = WM_USER + 100`）。ワーカーは `CoInitializeEx` を先に呼ぶ（Guess 背後の MLang 検出器が COM ベース）。COM 初期化失敗時は Guess が既定コードページへフォールバックし検索は継続する。
- **差分インデックスは走査中は計算しない**。クリック時に開いた CMergeDoc の public メンバ `m_diffList.LineToDiff(apparentLine)`（apparent 行は `m_ptBuf[pane]->ComputeApparentLine(realLine)`）で解決。`GotoLine(realLine, bRealLine=true, pane)` が実→apparent 変換を内包。順序は SelectDiff → GotoLine（該当行を中央に）。ヒット行が共通領域・同一ファイル内の場合は LineToDiff が -1 を返し「行ジャンプのみ、差分選択なし」になる。
- **一覧構築はフォルダツリー全体を対象**：`GetFirstDiffPosition()`＋`GetNextDiffRefPosition()` の平坦な深さ優先走査。ディレクトリをスキップ、片側のみ存在するファイルは存在する側のみ（不在側は空文字）。隠し／フィルタ対象ファイルも含む（設計：表示行だけでなく OLD／NEW「全ファイル」）。
- **ジャンプの安全性**：フィルタで隠れたファイルは一覧行がないため、説明付きメッセージボックスでジャンプを拒否する（誤ったファイルを開かない）。開いた比較がヒットのファイルであることをパス照合で確認してから移動する。ヒット行はバッファ行数でクランプする（走査とクリックの間にファイルが変わった場合）。
- **起動経路**：Ctrl+Shift+F（`IDR_MAINFRAME ACCELERATORS` で空きを確認済み）とフォルダ比較の右クリックメニュー「【Wiz】横断検索…」。コマンドはフォルダ比較（CDirView）でのみ処理する。IDirDoc に DirView アクセサがないため**ファイル比較からの起動は D7 の範囲外**。
- **所有と寿命**：ダイアログは CDirView のメンバ（D6 の `CMergeEditFrame::m_wndSniper` パターンをフォルダビューに適用）。初回のみ Create、以後は表示／非表示切替。OnCancel／Esc は隠すだけ。CDirView::OnDestroy で CloseGrep（StopScan＋joinAll）してからウィンドウ破棄。
- **検索文字列**：§4.1 の `grepText`／`OPT_MERGE20_GREP_HISTORY`（D0 のキー）に保存（最新 10 件、改行区切り）。検索結果そのものはセッションに保存しない（メモリ内のみ）。
- **.po**：新規 msgid 11 件を追加。`Middle` は既存 msgid（訳「中央」、IDS_COLHDR_MIDDLE と同じ語彙）を再利用し新規作成しなかった。再利用 msgid（訳追加なし）：`Search`、`Stop`、`Close`、`File`、`Line`、`Text`、`Old`、`New side`。
- **CI 修正の教訓**：(1) `CWnd::OnDestroy` は virtual ではない（SniperDlg も OnDestroy を宣言していない）。`override` を付けると C3668 になる。メッセージ マップ ハンドラは `afx_msg void OnDestroy();` と宣言すること。(2) Windows の `min`／`max` マクロと衝突するため、`std::min`／`std::max` は既存コードの慣例どおり `(std::min)(...)`／`(std::max)(...)` と書くこと。なお DirView.cpp の `std::min`／`std::max` は 1 回目の CI ではエラーにならなかったが、TU によってマクロ状態が異なるため予防的に括弧付きに統一した。

### 33.3 CI 結果

- 1 回目：実行 45771809109（head 586c0d8e、PR #29 作成時）→ **failure**。ジョブ 139745975582 "build" の "Build failure" ステップ。全文ログで原因特定：C3668 ×14（OnDestroy の override）、C2589 ×8（`std::max` の 4 箇所 ×2 回）。上記 3 件の修正コミットで対応。
- 2 回目：実行 45773892169（リベース前 head ff774529。コード内容はリベース後と同一）→ **success**（完了 2026-11-26T07:24:43Z）。ジョブ 139752791350 "build" success。全文ログ（47,368 行）で確認：`error C*`／`error LNK*`／`error MSB*` は 0 件、`[  PASSED  ] 664 tests.`（D6 時 647 件＋新規 Merge20Grep 17 件＝664 件。`[  FAILED  ]` 行は 0 件。ログ中の "FAILED" は `.po` の `IDS_*_FAILED` コメントのみ）、`.po` チェック行 `Merge20 strings present in the bundled Japanese.po.` を確認。
- リベース後の最終 head（本節の記録コミット込み）については CI を再検証すること（実行の conclusion、GoogleTest、成果物）。
- 成果物 2 件（API 確認、未失効、失効日 2027-02-24T07:14:53Z）：WinMerge-x64.zip（ID 15095861711、26,351,360 バイト）、WinMerge-x64-Setup-x64.exe（ID 15095966596、39,811,973 バイト）。

### 33.4 検証ゲート (a)〜(d)＋GoogleTest

- (a) 仕様・設計書の条項：OLD／NEW 全ファイル横断検索（○、一覧構築で全ツリー走査）、ヒット＝（相対パス、ペイン、実ファイル行、差分インデックス、抜粋）（○、差分インデックスはクリック時に解決する点のみ設計判断として記録）、上限 80 件で打ち切り＋表示（○）、ヒットから該当差分へジャンプ（○、GotoLine＋SelectDiff）、モーダルレスでファイル比較を開いても閉じない（○、CDirView 所有のモーダルレスダイアログ）、Ctrl+Shift+F 起動（○）、検索文字列の保存（○）、検索結果はセッションに保存しない（○）。**仕様との意図的差分**：対象テキストは unpacker／prediffer 適用後のテキストではなくディスク生テキスト（T-4-9、§33.2 参照）。ファイル比較からの起動は対象外（§33.2 参照）。
- (b) 利用可否：コード読解と GoogleTest で確認。**Windows 実機での操作の流れ（検索→結果一覧→クリック→該当差分へジャンプ、走査中の UI 応答）は未検証**（§33.5 のチェックリストで実施すること）。
- (c) 表示の正当性：新規 `_()` 文字列は全て .po に訳あり（CI の .po チェックも通過）。**日本語 UI での実際の表示、レイアウト崩れ・文字の見切れは未検証**（§33.5 で実施すること）。
- (d) 回帰：GoogleTest 664 件全パス（既存 647 件を含む）で確認。既存のフォルダ比較・ファイル比較の実機での動作は未検証（§33.5 で実施すること）。
- GoogleTest：Merge20Grep 17 件全パス（FindInLine 5、MakeExcerpt 3、ParseHistory 3、FormatHistory 1、PushHistory 4、ReachedLimit 1）。純粋モジュールは g++ ハーネスでも全チェック合格（作業ファイル `~/workspace/d7-work/harness/`）。ダイアログ／ワーカー／ジャンプ経路（MFC 依存）は GoogleTest の対象外のため実機確認が必要。

### 33.5 Windows 実機確認チェックリスト（手順付き。未検証）

成果物 WinMerge-x64.zip（ID 15095861711）または WinMerge-x64-Setup-x64.exe（ID 15095966596）を入手し、以下を順に確認すること。

1. **起動**：フォルダ比較を開く。フォルダ比較ウィンドウで Ctrl+Shift+F を押す→「【Wiz】横断検索」ダイアログが開くこと。右クリックメニューに「【Wiz】横断検索…」があり、そこからも開くこと。タイトル・ラベル・ボタンが日本語表示になること。
2. **検索と一覧**：検索文字列（例：日本語を含む文字列）を入力し「検索」を押す→結果一覧に「ファイル」「ペイン」「行」「テキスト」の 4 列が表示され、ヒットがフォルダ一覧順に並ぶこと。ステータスに「【Wiz】○件ヒット」と表示されること。該当なしの場合は「【Wiz】該当なし」と表示されること。
3. **ジャンプ**：結果行をシングルクリックする→該当ファイルのファイル比較が開き、ヒット行が中央付近に表示され、該当差分が選択されていること。ヒット行が共通領域の場合は行ジャンプのみで差分選択なしになること。ジャンプ後も Grep ダイアログが開いたまま残っていること（モーダルレス）。
4. **80 件上限**：多数ヒットする文字列で検索する→80 件で一覧が止まり、ステータスに「【Wiz】上限に達しました（80件）」と表示されること。
5. **走査中の UI 応答**：ファイル数の多いフォルダ（100〜1000 ファイル規模）で検索する→走査中にフォルダ比較ウィンドウが操作できること（固まらないこと）。「停止」ボタンで走査を止められること（ステータス「【Wiz】検索を停止しました」）。
6. **履歴**：検索後、検索コンボのドロップダウンを開く→過去の検索文字列が最新順に表示されること（最大 10 件）。再起動後も履歴が残ること。
7. **隠しファイルのジャンプ拒否**：フィルタで隠れているファイルのヒットをクリックする→「【Wiz】現在のフィルタで隠れているため開けません。…」のメッセージが出て、誤ったファイルが開かないこと。
8. **回帰**：フォルダ比較（表示・ソート・フィルタ・ダブルクリックで開く）とファイル比較（差分表示・編集・マージ）が従来どおり動くこと。既存の Merge20 機能（巡回・フォーカス・スナイパー・学習・セッション保存）に影響がないこと。
9. **レイアウト**：ダイアログをリサイズする→検索コンボ・結果一覧・ボタンが追従し、文字の見切れ・重なりがないこと。高 DPI 環境でも表示が崩れないこと。

### 33.6 D8 への引き継ぎと残課題

- D6 からの残課題（§31.6）は D7 では扱っていない。引き続き残課題。
- D7 固有の残課題：(1) ファイル比較からの Grep 起動（IDirDoc に DirView アクセサがないため未対応。対応する場合は DirDoc／MainFrm 側の設計が必要）。(2) Grep は unpacker／prediffer 適用後のテキストではなくディスク生テキストを検索する（仕様との意図的差分。unpacker 適用後のテキストで検索すべきという判断になれば、ワーカー側の読み込み経路の再設計が必要）。(3) 3-way の中間ペインは Grep の対象に含まれる（`paths` は 3 要素、ペインラベルは既存訳「中央」を再利用）が、実機での 3-way 動作は未確認。(4) ダイアログの検索対象に「差分のあるファイルのみ」等の絞り込みはない（設計どおり全ファイル対象）。(5) `3d824036` は no-op コミット（履歴に残る。revert 不要）。
- 次スライス（D8）の着手時は、master の最新 head と NOTES の次節番号（§34 の見込み）を再確認すること。

## 34. Phase 4 D8（マスタ管理画面）— ブランチ `merge20/phase4-d8`

- PR: #30（https://github.com/askhim659/winmerge/pull/30）。最終 head（コード＋修正）`506f4d0081c546795049cb782e69d3c2763f1644`。マージは本記録時点では未実施（コーディネーター判断待ち）。
- 設計書 §5.2 D8・§4.4、仕様書 §3 (4/4)・§6。受け入れ条件：除外ワード・強調文字・リスク重要語を画面で編集でき、保存・復元されること。除外ワードの変更後に再判定で全ファイルが再計算されること。入出力で JSON を共有できること。

### 34.1 変更一覧（1 ファイル 1 コミット、22 件＋本節の記録コミット）

| コミット | ファイル | 内容 |
|---|---|---|
| 09bf09bc | Src/Merge20/Merge20Master.h | 純粋モジュールヘッダ（ルール構造体、一致判定、JSON 変換、HTML プリセット） |
| 4d7596f8 | Src/Merge20/Merge20Master.cpp | 純粋モジュール実装（S8 の JSON DOM・パーサ・シリアライザを流用） |
| e7fbb07b | Testing/GoogleTest/Merge20/Merge20Master_test.cpp | 単体テスト 12 件 |
| 8fb5a2a0 | Src/Merge.vcxproj | Master.cpp／.h 登録（pch.h グループ） |
| 92d52485 | Src/Merge.vcxproj.filters | 同上 |
| c8f2d581 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | Master.cpp＋テスト登録 |
| 8d36cfa6 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同上 |
| 81ea39a6 | Src/Merge20/Merge20MasterDoc.h | アダプタヘッダ（保存場所、ライブストア、入出力） |
| 9cca766a | Src/Merge20/Merge20MasterDoc.cpp | アダプタ実装（prediffer オプションキー同期、JSON ファイル入出力） |
| 72d3f56b | Src/Merge.vcxproj | MasterDoc.cpp／.h 登録（StdAfx.h グループの既定エントリ） |
| f3076110 | Src/Merge.vcxproj.filters | 同上 |
| e2d75cf0 | Src/resource.h | IDD 299、IDC 1785〜1804、コマンド 38957、`_APS_NEXT_*` 更新（300／1805／38958） |
| a3276a1d | Src/Merge.rc | `IDD_MERGE20_MASTER` 空テンプレート（640x460、CAPTION "Master Management"） |
| c3baf3f9 | Src/Merge20/Merge20MasterDlg.h | モーダルダイアログヘッダ（3 マスタ共用） |
| 80b26440 | Src/Merge20/Merge20MasterDlg.cpp | ダイアログ実装（約 27KB、動的コントロール＋LayoutAll） |
| cd505479 | Src/Merge.vcxproj | MasterDlg.cpp／.h 登録（StdAfx.h グループの既定エントリ） |
| 3e169582 | Src/Merge.vcxproj.filters | 同上 |
| 872c79c5 | Src/DirView.h | OnMerge20Master の宣言を追加 |
| 9b147b29 | Src/DirView.cpp | メッセージマップ＋右クリックメニュー「【Wiz】マスタ管理…」＋ハンドラ（保存後に除外変更時のみ Rescan） |
| 4e497363 | Translations/WinMerge/Japanese.po | 新規 `_()` 文字列 34 件の日本語訳を追加（汎用動詞の Update／Move up／Move down を除き【Wiz】付き） |
| 50aaa7fb | Translations/WinMerge/Japanese.po | 「Kind」列見出しの訳を追加（初回コミットの抽出漏れ。抽出スクリプトで検出して追加。msgstr は「【Wiz】種類」） |
| 506f4d00 | Translations/WinMerge/Japanese.po | 【Wiz】欠落の修正（§34.2 参照）。D8 訳 35 件のうち汎用動詞 3 件を除く 31 件に【Wiz】を付与し直した |

### 34.2 設計判断（判断理由付き）

- **リスク重要語エディタはマスタ管理画面と共用する**（設計書の D8 判断事項）。1 つのモーダルダイアログ「【Wiz】マスタ管理」に 3 マスタをコンボ切替で載せる。理由：単一の入口、共有の一覧・編集・入出力 UI、リソース ID 面の節約。3 マスタで編集 UI（パターン編集＋名称編集＋正規表現／大文字小文字／有効チェック）を共用し、該当しない欄は無効化する。
- **除外ワードのライブストアは prediffer のオプションキー**（`Plugins\Merge20Exclude.sct/`＋Count／Enabled<i>／IgnoreCase<i>／UseRegExp<i>／Pattern<i>／IgnoreL2）。`MergeAppCOMClass::GetOption` がキー文字列を `GetOptionsMgr()->Get(name)` へ verbatim に渡すことを実読で確認したため、C++ 側で同一キー文字列を読み書きすれば .sct が読む値と完全に一致する。未書込みキーは VT_NULL 判定（`MergeAppCOMClass` と同じ方法）で .sct の既定値（Enabled=true、IgnoreCase=false、UseRegExp=false、Pattern=""、Count=0、IgnoreL2=true）を適用する。これが設計書 §4.4 の「prediffer が読む形式と整合させる」の実現方法。
- **JSON（`Merge20Masters.json`）は共有・ミラー用**：`{"app":"Merge20Masters","version":1,"excludeIgnoreL2":true,"excludeRules":[{pattern,useRegExp,ignoreCase,enabled}],"highlightRules":[{pattern,name,useRegExp,enabled}],"riskKeywords":";..."}`。UTF-8・BOM なし・2 スペース字下げ（S8／L0 流用）。保存場所は学習と同じ親ルール（ポータブル時は `<prog>\Merge20Masters\`、それ以外は `%APPDATA%\WinMerge\Merge20Masters\`）。保存時にライブ状態全体を JSON へも書く。読み込み時は除外ワードとリスク重要語はオプションキーを正とし、強調文字だけを JSON から読む（手編集した JSON がオプションを黙って上書きしないため）。
- **除外ワードの既定はルールなし**（.sct の Count=0 と一致し、現行動作を変えない）。HTML の既定 2 件（`Last Modified:`、`@version\s+[0-9.]+`）はシードしない。理由：Phase 1 の prediffer が既定で何も除外しない動作を変えると、既存ユーザーの比較結果が変わる。HTML 既定は単体テスト（HtmlExcludeDefaults）でパースと一致判定だけ検証した。
- **強調文字の既定は HTML プリセット 3 件**（設計書どおり同梱）：`Math\.floor|Math\.round`（正規表現、表示名「Math.floor／round」）、`独自`（部分一致）、`0\.92|0\.014`（正規表現、表示名「税率・補正率」）。表示名は HTML にはない項目だが設計書 §4.4 が要求するため name フィールドに持つ。
- **強調文字の比較画面での表示は D8 の範囲外**（意図的な先送り）。設計書 §2.1 は表示を「検討」とだけ書いており、D8 の受け入れ条件にも表示は含まれない。`CMergeResultView::GetLineColors`／`CMergeEditView` の行色付けへの追加は、色・無効化・再描画の設計が必要なため別スライス（D9／D11 以降）へ残す。ダイアログのヒント文に未実装であることを明記した。
- **リスク重要語の変更は次回の分類実行時に反映**。分類はファイル比較のオープン／再スキャン時の既存フック（`MergeDoc::Rescan` 末尾）で `OPT_MERGE20_RISK_KEYWORDS` を読むため、ダイアログの保存だけでは開いているファイル比較の分類は変わらない。フォルダ再比較は分類を再実行しない。D8 の受け入れ条件は「編集でき、保存・復元されること」であり、それを満たす。開いている文書の再分類が必要になれば、L2 の `Merge20LearnRefreshAllOpenDocs` と同じ形の刷新関数が要る（残課題）。
- **再計算の入口は `CDirDoc::Rescan()`**。設計書 §4.4 の「`CDirDoc` の再比較（`FlushAndRescan`）」は文言の誤りで、`FlushAndRescan` は `CMergeDoc` のもので `CDirDoc` には存在しない。フォルダ再比較の入口は `CDirDoc::Rescan()`（DirView.cpp の既存呼び出しと同じ）。除外ワード／IgnoreL2 の変更時のみ呼び、強調文字・リスク重要語のみの変更では呼ばない。
- **入出力は L5 前例**：保存ダイアログ＋共有注意文つき確認ダイアログ（「マスタデータにはソースコードの断片が含まれます。承認された内部経路でのみ共有してください…」）。エクスポート既定名は `マージ支援20号_マスタ_YYYY-MM-DD.json`（L5 の命名形）。インポートは作業コピーへ読み込むだけで、保存を押すまでライブストアは変わらない（その旨を完了メッセージに明記）。不正なルールは 1 件ずつスキップして件数を報告する（L0 の skippedRules と同じ扱い）。
- **.po の汎用動詞**：新規 msgid のうち Update／Move up／Move down だけは訳に【Wiz】を付けていない。D7 の前例（Search／Stop／Close 等の汎用動詞は既存訳を再利用し【Wiz】なし）に倣い、ボタン上の汎用動詞は素の訳とした。再利用 msgid（訳追加なし）：`Regular expression`、`Ignore case`、`Enabled`、`Add`、`Delete`、`Export...`、`Import...`、`Save`、`Cancel`、`Skipped %1 rules (invalid format).`（L5）。
- **【Wiz】欠落の発生と修正（506f4d00）**：初回コミット 4e497363 の訳 34 件のうち 31 件で msgstr 先頭の【Wiz】が欠落していた。原因は編集スクリプトを muse.write で書いたためで、muse.write が全角括弧【】を欠落させる既知の問題がスクリプト内のリテラルにも及んだ。CI の .po チェックは msgid の存在しか見ないため、この欠落は CI では検出できない。§34 記録の準備中に D8 訳の【Wiz】件数を数えて発覚した（master 75 件→D8 ブランチ 76 件しか増えておらず、D8 訳 35 件中【Wiz】付きは「Kind」1 件だけだった）。修正は exec 経由の Python で 31 件に【Wiz】を付与し直し、差分が msgstr 31 行のプレフィックス追加のみであることを API 版との diff で確認した。教訓：【Wiz】を含むファイル編集は exec 経由で行うこと（muse.write／muse.edit は使わない）。また .po の検証には msgid の存在確認に加えて、新規 msgstr の【Wiz】付与件数の監査を入れること（D8：35 件中 32 件が【Wiz】付き、素の訳は汎用動詞 3 件のみ）。
- **細部**：(1) resource.h は mixed line endings（D0 の注記ブロックだけ LF）だったため、注記部分の編集は LF アンカーで行い、他は CRLF を維持した。(2) ルール数が減ったときに Count を超えた古いキー（Enabled3 等）が残るが、.sct は Count までしか読まないため無害。オプション削除 API がないため残す。(3) ダイアログはモーダル（学習ダイアログ前例）。Enter は保存（既定ボタン）、Esc は破棄。(4) `github_api.py pr` は head を `merge20/phase1` に固定しているため、PR #30 は API 直接呼び出しで開いた。(5) `/tmp/d8-master` 作業ツリーはランタイム再起動で消えた（`/tmp` は揮発性）。以降のスライスでは作業ツリーを `~/workspace/` 配下に置くこと。

### 34.3 CI 結果

- 1 回目：実行 45782824163（head 50aaa7fb、【Wiz】欠落ありの状態）→ **success**。ジョブ 139782171559 "build" success。全文ログ（47,433 行）で確認：`error C*`／`error LNK*`／`error MSB*` は 0 件、`[  PASSED  ] 676 tests.`（D7 時 664 件＋新規 Merge20Master 12 件＝676 件。`[  FAILED  ]` 行は 0 件。ログ中の "FAILED" 4 件は `.po` の `IDS_*_FAILED` コメントのみ）、Merge20Master 12 テストすべて `[ OK ]`、`.po` チェック行 `Merge20 strings present in the bundled Japanese.po.` を確認。Merge20Master 新規ファイルへのコンパイル警告は 0 件。成果物 2 件（API 確認、未失効、失効日 2027-02-24T08:24:00Z）：WinMerge-x64.zip（ID 15099986041、26,394,334 バイト）、WinMerge-x64-Setup-x64.exe（ID 15098933456、39,846,642 バイト）。
- 2 回目（【Wiz】修正後）：実行 45785339440（head 506f4d00）→ **success**。ジョブ 139790576395 "build" success。全文ログ（47,390 行）で確認：`error C*`／`error LNK*`／`error MSB*` は 0 件、`[  PASSED  ] 676 tests.`、`[  FAILED  ]` 行は 0 件（"FAILED" 4 件は同じく `IDS_*_FAILED` コメントのみ）、Merge20Master 12 テストすべて `[ OK ]`、`.po` チェック行を確認。成果物 2 件（API 確認、未失効、失効日 2027-02-24T08:41:46Z）：WinMerge-x64.zip（ID 15100192525、26,394,141 バイト）、WinMerge-x64-Setup-x64.exe（ID 15101150012、39,844,787 バイト）。
- PR #30 は mergeable: True、mergeable_state: clean（2 回目の実行完了時点で確認）。
- 本節の記録コミット後の最終 head については CI を再検証すること（実行の conclusion、GoogleTest、成果物）。

### 34.4 検証ゲート (a)〜(d)＋GoogleTest

- (a) 仕様・設計書の条項：除外ワードマスタの管理画面（○、新規ダイアログ。`%` ワイルドカード付き部分一致・正規表現可、比較オプションとは独立に保持）、強調文字マスタの管理画面（○、プリセット 3 件を既定ルールとして同梱、正規表現チェック可）、リスク重要語エディタ（○、共用ダイアログで `OPT_MERGE20_RISK_KEYWORDS` の値を編集）、保存形式 §4.4（○、JSON。1 ルール＝｛部分一致／正規表現、`%` パターン、有効／無効｝等。S8 の JSON モジュールを流用。純粋モジュールは ASCII のみ）、入出力（○、保存ダイアログ＋共有注意文つき確認ダイアログ。L5 前例）、保存して再判定で全ファイル再計算（○、除外変更時に `CDirDoc::Rescan()`）。**仕様・設計書との意図的差分**：強調文字の比較画面での表示は未実装（§34.2 参照）。設計書の「FlushAndRescan」は `CDirDoc::Rescan()` が正しい入口（§34.2 参照）。
- (b) 利用可否：コード読解と GoogleTest と g++ ハーネスで確認（純粋モジュールはローカルで全チェック合格。作業ファイル `~/workspace/d8-work/harness/`）。**Windows 実機での操作の流れ（編集→保存→復元→再計算→入出力）は未検証**（§34.5 のチェックリストで実施すること）。
- (c) 表示の正当性：新規 `_()` 文字列 35 件は全て .po に訳あり（CI の .po チェックも通過）。【Wiz】付与は監査済み（35 件中 32 件が【Wiz】付き、素の訳は汎用動詞 Update／Move up／Move down の 3 件のみ。§34.2 の欠落と修正を参照）。**日本語 UI での実際の表示、レイアウト崩れ・文字の見切れは未検証**（§34.5 で実施すること）。
- (d) 回帰：GoogleTest 676 件全パス（既存 664 件を含む）で確認。既存のフォルダ比較・ファイル比較の実機での動作は未検証（§34.5 で実施すること）。
- GoogleTest：Merge20Master 12 件全パス（WildcardPercentMatchesAnyText、WildcardPlainIsSubstring、WildcardEmptyNeverMatches、WildcardIgnoreCase、RuleMatchesRegex、IsValidRegex、ExcludeLineSkipsDisabled、DefaultHighlightPresets、JsonRoundTrip、JsonErrors、JsonSkipsBadRules、HtmlExcludeDefaults）。ダイアログ／アダプタ／再計算経路（MFC 依存）は GoogleTest の対象外のため実機確認が必要。

### 34.5 Windows 実機確認チェックリスト（手順付き。未検証）

成果物 WinMerge-x64.zip（ID 15100192525）または WinMerge-x64-Setup-x64.exe（ID 15101150012）を入手し、以下を順に確認すること。

1. **起動**：フォルダ比較を開く。フォルダ比較ウィンドウで右クリックする→「【Wiz】マスタ管理…」があり、開くと「【Wiz】マスタ管理」ダイアログが開くこと。タイトル・ラベル・ボタンが日本語表示になること。コンボに「【Wiz】除外ワードマスタ」「【Wiz】強調文字マスタ」「【Wiz】リスク重要語」の 3 件があること。
2. **除外ワードの編集と保存・復元**：除外ワードマスタでパターン（例：`--1.0.8%`、部分一致）を追加し「保存」を押す→ダイアログが閉じること。再度開く→追加したルールが残っていること（復元）。再起動後も残っていること。ルールの有効チェックを外して保存→再度開くと無効のまま残ること。
3. **除外ワードの再判定**：差分のある実フォルダで、除外したい文字列を含む行の差分があるファイルを用意する。除外ワード（例：その文字列を含む `%` 付きパターン）を追加して保存する→フォルダ比較が自動で再比較され、該当ファイルの差分が消える（または「同一」になる）こと。2 行目のバージョン番号を持つファイルで「【Wiz】2行目のバージョン番号を無視する」のチェックを外して保存→再比較で 2 行目が差分として出ること。
4. **正規表現チェック**：パターン欄に `[unclosed` と入力し「正規表現」チェックを付けて「【Wiz】正規表現をチェック」を押す→「【Wiz】正規表現が無効です。」と表示されること。`@version\s+[0-9.]+` では「【Wiz】正規表現は有効です。」と表示されること。無効な正規表現のまま「追加」を押す→追加されずエラーメッセージが出ること。
5. **強調文字のプリセット**：強調文字マスタを開く→初回は「Math.floor／round」「独自」「税率・補正率」の 3 件が既に入っていること。表示名・パターン・種類（正規表現／部分一致）が正しいこと。ルールを追加・削除・上下移動できること。ヒント文に「比較画面での表示は未実装です。」と出ること。
6. **リスク重要語の編集**：リスク重要語マスタで重要語（例：`決算`）を追加して保存する→再度開くと残っていること。ファイルを分類し直す（ファイル比較を開き直す）と、該当語を含む差分でリスクが変わること（既存の分類フックどおり）。
7. **入出力**：「エクスポート…」を押す→共有注意文（承認された内部経路のみ…）の確認が出る→「はい」で JSON ファイルが保存されること。保存した JSON をテキストエディタで開き、`app` が `Merge20Masters`、`version` が 1 であること。別のマスタ状態で「インポート…」からその JSON を読む→件数付きの完了メッセージが出て、保存を押すと反映されること（保存前にキャンセルすると反映されないこと）。
8. **レイアウト**：ダイアログをリサイズする→一覧・編集欄・ボタンが追従し、文字の見切れ・重なりがないこと。高 DPI 環境でも表示が崩れないこと。パターン列が長い日本語パターンでも読めること。
9. **回帰**：フォルダ比較（表示・ソート・フィルタ・ダブルクリックで開く）とファイル比較（差分表示・編集・マージ）が従来どおり動くこと。既存の Merge20 機能（巡回・フォーカス・スナイパー・学習・セッション保存・Grep）に影響がないこと。除外ワードが空の状態では、従来どおり 2 行目バージョン無視だけが働くこと。

### 34.6 D9 への引き継ぎと残課題

- D7 からの残課題（§33.6）は D8 では扱っていない。引き続き残課題。
- D8 固有の残課題：(1) 強調文字の比較画面での表示（`CMergeResultView::GetLineColors`／`CMergeEditView` の行色付けへの追加。色・無効化・再描画の設計が必要）。(2) リスク重要語の変更時に開いているファイル比較の分類を再計算する刷新関数（L2 の `Merge20LearnRefreshAllOpenDocs` と同じ形）。(3) 除外ワードの HTML 既定 2 件はシードしていない（§34.2 参照。シードすべきという判断になれば、初回読み込み時の既定として入れる）。(4) ルール数削減時に Count を超えた古いオプションキーが残る（無害だが掃除されない。§34.2 参照）。(5) `github_api.py pr` は head を `merge20/phase1` に固定しているため、PR 作成は API 直接呼び出しで行った（§34.2 参照）。(6) muse.write は【】を欠落させるため、【Wiz】を含むファイル編集は exec 経由で行うこと（§34.2 参照）。また .po の検証には新規 msgstr の【Wiz】付与件数の監査を入れること。
- 次スライス（D9）の着手時は、master の最新 head と NOTES の次節番号（§35 の見込み）を再確認すること。

## 35. Phase 4 D9（スリムリスト・ミニマップ・キー整備）— ブランチ `merge20/phase4-d9`

- PR: #31（https://github.com/askhim659/winmerge/pull/31）。最終 head（コード）`6575965653e0b254bfc90d57b73b3e1591e20ec7`。マージは本記録時点では未実施（コーディネーター判断待ち）。
- 設計書 §5.2 D9、§3.10、§4.1、T-4-8、仕様書 §4(g)・§5.1・§5.4・§6.1。受け入れ条件：スリム切替で列が隠れ、確認内容タグが折り返し表示されること。F・R が専用モード内でのみ有効で、通常モードでは従来動作（文字入力等）を壊さないこと。

### 35.1 変更一覧（1 ファイル 1 コミット、15 件＋本節の記録コミット）

| コミット | ファイル | 内容 |
|---|---|---|
| 2cd80ab7 | Src/Merge20/Merge20Slim.h | 純粋モジュールヘッダ（スリム列集合・列順文字列・幅定数・折り返し判定） |
| 48c71d8b | Src/Merge20/Merge20Slim.cpp | 純粋モジュール実装（`VisibleColumns()`＝Name／Soutput／Sreview、`ColumnOrders()`＝`"0 | Name Soutput Sreview"`、確認内容列幅 126、出力列幅 48） |
| bcd19215 | Testing/GoogleTest/Merge20/Merge20Slim_test.cpp | 単体テスト 5 件 |
| d9046714 | Src/Merge.vcxproj | Merge20Slim.cpp／.h 登録（pch.h グループ） |
| b7c99b98 | Src/Merge.vcxproj.filters | 同上 |
| 60c865f1 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | Merge20Slim.cpp＋テスト登録 |
| ac67d1c5 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同上 |
| 16b0050a | Src/resource.h | ID_MERGE20_DED_DIFFS_ONLY 38958、ID_MERGE20_SLIM 38959、IDC_MERGE20_DED_DIFFS_ONLY 1805、`_APS_NEXT_*` 更新（38960／1806）。D0 コメントブロック更新 |
| 441a5325 | Src/Merge20/Merge20DedBar.h | 専用バーの「Diffs only」トグルボタン宣言（`m_btnDiffsOnly`、`SetDiffsOnly`）。ボタン 13 個に |
| 6a009d7b | Src/Merge20/Merge20DedBar.cpp | 同ボタン実装（幅・ID・配列。トグル 2〜10、プッシュ 0／1／11／12） |
| d47c724a | Src/MergeEditFrm.h | `OnMerge20DedDiffsOnly`（F キーハンドラ）の宣言を追加 |
| 64eb696a | Src/MergeEditFrm.cpp | F キー（差分のみ表示切替、専用モード内のみ）＋メッセージマップ＋`UpdateDedBar` でのボタン状態更新 |
| ef70e53e | Src/DirView.h | スリム状態メンバ（`m_bMerge20Slim`、全レイアウト記憶用 `m_sMerge20FullColumnOrders/Widths`、`m_merge20SlimImageList`）とメソッド宣言 |
| f468244a | Src/DirView.cpp | スリムリスト本体（列プリセット、126px 折り返し確認内容タグ、レイアウトガード、コンテキストメニュー項目、`OnInitialUpdate` での復元） |
| 65759656 | Translations/WinMerge/Japanese.po | 新規 `_()` 文字列 2 件の日本語訳を追加（「【Wiz】スリムリスト」「【Wiz】差分のみ」） |

### 35.2 設計判断（判断理由付き）

- **スリム表示列は Name＋Soutput＋Sreview のみ**。HTML のスリムはチェックボックス（24px）＋ファイル名＋確認内容（126px）。WinMerge におけるチェックボックス相当は出力（Soutput）列なので残す。隠す列：Saction、Srisk、Ssummary ほか全 stock 列。Ssummary の件数情報はテキスト内に含まれるため情報損失はない（旧／変／新は Ssummary テキスト＋既定非表示の Snsdiffs／Snidiffs で対応）。
- **全レイアウトは記憶して復元する。スリム中の列設定は保存しない**。スリム突入時に列順・幅を `CDirView` メンバに記憶し、解除時に完全復元する。スリム中の列順・幅がオプションに保存されないよう `OnDestroy`、`Merge20GetDisplaySettings`、`Merge20ApplyColumnSettings` にガードを入れた。`OnEditColumns`（列の表示ダイアログ）はスリムを先に解除して全レイアウトを表示する。`OPT_MERGE20_SLIM`（D0 キー）でフラグを永続化し、`OnInitialUpdate` で全レイアウト読込後にスリムを再適用する。
- **確認内容タグの折り返しは `OnCustomDraw` で自前描画**。スリム＋非選択行の確認内容セルのみ `DT_WORDBREAK` で描画する。選択行は stock の選択ハイライトを優先する。背景・文字色は `GetColors` から取り、S2 の行ティントを保つ。2 行分の行高は `BuildMerge20SlimImageList` で実現する（`m_imageList` から `ExtractIcon` で順序通り複写する taller な小アイコンリスト。all-or-nothing。既に 2 行分以上の高さがあれば省略）。解除時は `RestoreMerge20SlimImageList` で復元する。物理／論理列インデックスは都度検索する（ヘッダのドラッグ並べ替え後の陳腐化を回避）。描画は `m_bUseColors` と同じゲート（色表示は既定 ON）。正直な注記：現行の日本語ラベルは最大 5 文字（約 60px）で 126px に 1 行で収まるため、折り返しは長いラベルへの安全策である（英語最長ラベル "Needs manual work" は約 119px）。
- **T-4-8 再描画コストは解析で確認した**。切替は `ReloadColumns`（約 40 列のヘッダ再構築）＋ `Redisplay`（O(N) の仮想リスト bookkeeping。項目テキストの取得はなく、テキストは可視行のみ描画）＋可視行の再描画 1 回。折り返しは描画ごとに確認内容セル 1 回分の `DrawText` 追加のみ。ファイル内容量に比例する処理はない。実測タイミングは実機確認項目とする。
- **F キーは既存 `OPT_DIFF_CONTEXT` を流用し、all ⇔ 0 の明示トグルにする**。既定 −1＝全表示、0＝差分のみ。`ID_VIEW_DIFFCONTEXT_TOGGLE` の `-m-1` 方式（直前の N を記憶して all → N になる）は使わない。`CMergeEditFrame::PreTranslateMessage` で D2 の R と同じフォーカスゲート（フレームまたは専用バーのみ。比較ビュー内の 'f' は文字入力のまま）、専用モード限定、Ctrl／Alt／Shift 除外。専用バーに「Diffs only」トグルボタン（BS_PUSHLIKE チェックボックス）を追加し、`UpdateDedBar` で `OPT_DIFF_CONTEXT == 0` から状態を更新する。ハンドラ `OnMerge20DedDiffsOnly` は `ID_VIEW_DIFFCONTEXT_ALL`／`ID_VIEW_DIFFCONTEXT_0` を post し `FlashDedBar()` を呼ぶ。
- **R は D2 実装済み。D9 では専用モード内限定であることを再確認するのみ**。
- **ミニマップはコード追加なし**。`CLocationView`（Src/LocationView.cpp／.h）に "tooltip" の出現が 0 件であることを再確認した（grep）。仕様書 §3「全体マップにはマウスホバー時の説明ツールチップを付けない」に適合する。ツールチップは追加しない。
- **`OPT_MERGE20_SHOW_REVIEW_*` の UI は付けない（キーのみ残す）**。`IsShowable` が参照しておらず（grep で確認。読んでいるのは `Merge20SessionDisplay.reviewFilter` へのセッション永続化用のみ）、フィルタ適用ロジックなしのチェックボックスは死に UI になるため。フィルタ適用＋UI は D11 で一体実装する。
- **スリムの入口はフォルダ一覧のコンテキストメニュー**（動的追加、MF_CHECKED、「Master Management...」の後）。メニューはポップアップ毎に再構築されるため更新ハンドラは不要。メイン View メニューには追加しない。

### 35.3 仕様・設計条項チェック

| 条項 | 判定 | 備考 |
|---|---|---|
| 仕様書 §4(g) ショートカット表（F・R） | 満たす | 専用モード内限定。競合なし（§35.4） |
| 仕様書 §5.1 スリム表示 | 満たす | 列を絞って行を詰める表示 |
| 仕様書 §5.4 比較専用モード | 満たす | F・R は専用モード内のみ有効 |
| 仕様書 §6.1 保存原則 | 満たす | スリム中の列設定をオプションに保存しないガードあり |
| 設計書 §5.2 D9 | 満たす | スリム／ミニマップ確認／F・R 限定／SHOW_REVIEW_* 判断／ショートカット一覧整理の全項目を実施 |
| 設計書 §3.10 スリムリスト | 満たす | 列プリセット＋126px 折り返し＋全レイアウト復元 |
| 設計書 T-4-8 | 解析で確認 | 実測は実機確認項目（§35.6） |
| 設計書 §4.1 オプションキー | 満たす | OPT_MERGE20_SLIM（D0 キー）を流用。新規キーなし |

### 35.4 キーボードショートカット一覧（仕様書 §4(g) に沿って整理）と競合監査

- F：差分のみ表示切替（専用モード内のみ。D9 で追加）。R：倍率 75% リセット（専用モード内のみ。D2 実装済み）。
- J／K：スナイパー表示中のみ（D6）。Space／Esc／矢印：スナイパー表示中のみ（D6）、および比較ビューのマージ中矢印＋Esc（`CMergeEditView::PreTranslateMessage`、`MergeModeKeyDown`）。
- Ctrl+D：差分コンテキストのトグル（既存。`-m-1` 方式）。F は専用モード内の素キー別名で、all ⇔ 0 の単純な意味論。競合ではない。
- 競合監査の方法と結果：`Merge.rc` に素文字のアクセラレータなし（F は Ctrl+F `ID_EDIT_FIND` と Ctrl+Shift+F `ID_MERGE20_GREP` のみ。数字は修飾キー付きのみ。素の Space／R／J／K なし）。`PreTranslateMessage` で素の F／R を扱うのは D2 の R のみ（D9 で F を追加）。`CMainFrame::PreTranslateMessage` は Insert／Esc／Ctrl+Tab のみ。フォーカスドックにキー処理なし。以上、素キー F・R の競合なし。

### 35.5 検証レポート

- CI 実行 45791964305：結論 success（head `6575965653e0b254bfc90d57b73b3e1591e20ec7`）。GoogleTest 681 テスト全パス（55 スイート、FAILED 0 件。新規 Merge20Slim 5 テスト含む）。成果物：WinMerge-x64.zip 26,398,587 バイト（ID 15104032142、失効 2027-02-24T09:26:20Z）、WinMerge-x64-Setup-x64.exe 39,856,229 バイト（ID 15104351265、失効 2027-02-24T09:26:20Z）。全文ログは `~/workspace/.coord/d9_final_log.txt` に保存。
- .po 監査：ブランチ版 【Wiz】109 件 vs master 版 107 件（＋2。新規 2 件とも【Wiz】付き）。msgid 2136 件 vs master 2134 件（＋2）。ブランチ版を API から取得して確認。
- ローカル検証：Merge20Slim 純粋モジュールを g++ スタブ pch ハーネスで検証し、5 テスト全パス。
- 未検証（Windows 実機でのみ確認できる挙動）：§35.6 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 35.6 実機確認チェックリスト（手順付き）

1. スリム切替（1000 ファイル規模のフォルダ比較）：フォルダ一覧を右クリック → 「【Wiz】スリムリスト」を ON。列が Name／出力／確認内容のみになること。確認内容列が 126px で、タグが見切れないこと。もう一度 OFF にすると切替前の列順・幅が完全に復元されること。
2. スリムの永続化：スリム ON のまま再起動し、スリムが復元されること。「列の表示」ダイアログを開くと全レイアウトが表示されること（スリム自動 OFF）。
3. 確認内容タグの折り返し：UI を英語に切り替え、"Needs manual work" のタグが 2 行に折り返ること。スリム中の行が 2 行分の高さになること。ファイルアイコンがスリム中も正しいこと（taller なアイコンリスト）。スリム OFF でアイコン表示が元に戻ること。
4. F キー（専用モード）：比較専用モードに入り、フレームまたは専用バーにフォーカスがある状態で F を押すと全表示 ⇔ 差分のみが切り替わること。専用バーの「Diffs only」ボタンのチェックが連動すること。通常モードでは F を押しても何も起きないこと。比較ビューで 'f' をタイプすると文字入力になること。Ctrl+F／Ctrl+Shift+F は従来通り（検索／Grep）であること。
5. R キー：専用モード内で R を押すと倍率が 75% に戻ること。通常モードでは何も起きないこと。
6. ミニマップ：ファイル比較で LocationView が表示され、ホバーしてもツールチップが出ないこと。
7. 回帰：フォルダ比較・ファイル比較が従来通り動くこと。スリム ON のままセッション保存・復元しても全レイアウトが壊れないこと（スリムフラグ自体の永続化は D11）。
8. T-4-8 実測：1000 ファイル規模でスリム切替の体感遅延がないこと。

### 35.7 D10 への引き継ぎ・残課題

- D11：`OPT_MERGE20_SHOW_REVIEW_*` のフィルタ適用＋UI を一体実装する（D9 ではキーのみ）。
- D11：スリムフラグ自体のセッション永続化（D9 では `OPT_MERGE20_SLIM` によるフラグ永続化と `OnInitialUpdate` での再適用まで。セッション JSON への保存は D11 の範囲）。
- D8 からの残課題（D9 の範囲外）：比較ビューでのハイライト表示、リスク重要語変更時の再分類リフレッシュ、HTML 既定除外ワードの未シード、ルール数縮小時の陳腐オプションキー。
- 検証ゲートの注意：CI 緑だけでは不十分。最終 head の実行結論・全文ログの GoogleTest 全パス行・成果物 2 件の API 確認を毎回行うこと。実機挙動は §35.6 のチェックリストでユーザーに確認してもらうこと。

## 36. Phase 4 D10（速度改善）— ブランチ `merge20/phase4-d10`

- PR: #32（https://github.com/askhim659/winmerge/pull/32）。最終 head（コード）`31eb380da02356d850335e8c2d68b0de545866fc`（修正 3 件込み）。マージは本記録時点では未実施（コーディネーター判断待ち）。
- 設計書 §5.2 D10、§3.11、§3.12、§4、§8.3、仕様書 §3（2/4）・§7。受け入れ条件：1000 ファイル規模で一括計算が UI を固めないこと（バックグラウンドで進捗が表示されること）。巨大ファイルで簡易比較へ自動切替し [簡易] バッジが表示されること。100／500／1000 ファイルでの計測結果が記録されていること（§8.3）。

### 36.1 変更一覧（1 ファイル 1 コミット、28 件＋本節の記録コミット）

| コミット | ファイル | 内容 |
|---|---|---|
| cf7aeb86 | Src/Merge20/Merge20BulkDiff.h | 純粋モジュールヘッダ（HTML diffFast/lcs 移植。Merge20BulkRange／Options／Result、既定閾値 2200000） |
| 23fcc680 | Src/Merge20/Merge20BulkDiff.cpp | LCS DP＋位置合わせフォールバック、空白・大文字小文字・空行の trivial 扱い |
| 2212ce41 | Testing/GoogleTest/Merge20/Merge20BulkDiff_test.cpp | 単体テスト 12 件 |
| a206ebcf | Src/Merge.vcxproj | Merge20BulkDiff.cpp／.h 登録（pch.h グループ） |
| b1b68596 | Src/Merge.vcxproj.filters | 同上 |
| 07026fed | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | Merge20BulkDiff.cpp＋テスト登録 |
| 83ec4e04 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同上 |
| 423d3849 | Src/OptionsDef.h | OPT_MERGE20_SIMPLE_THRESHOLD 追加 |
| 716a4cf7 | Src/OptionsInit.cpp | 同キーを 2200000、範囲 0〜10000000 で初期化 |
| e8abeafc | Src/Merge20/Merge20ClassifyDoc.h | Merge20ClassifyFromRanges 宣言追加 |
| 3022afa9 | Src/Merge20/Merge20ClassifyDoc.cpp | 範囲→行ビルダを共用化（LineAccessor による抽象化。Merge20ClassifyFromMergeDoc は DIFFRANGE→Merge20BulkRange 変換後に共用ビルダへ委譲。動作は S4 と同一） |
| bedcbd05 | Src/DiffContext.cpp | SetMerge20Classification で simpleCompare をコピー |
| bf42d926 | Src/DirViewColItems.cpp | ColSummaryGet で simpleCompare 時に "[Simple] " プレフィックス（表示時のみ。保存される summary は汚さない） |
| ba70d3b1 | Src/Merge20/Merge20Session.h | Merge20SessionFile に simpleCompare フィールド追加 |
| ae1b9b52 | Src/Merge20/Merge20Session.cpp | SessionFileToJson／ParseSessionFile に simpleCompare 追加（欠落時は false のまま） |
| 201642e8 | Src/Merge20/Merge20SessionDoc.cpp | BuildFromDoc／ApplyToDoc で simpleCompare をコピー |
| ec052325 | Src/resource.h | IDD_MERGE20_BULK_CLASSIFY 300、IDC_MERGE20_BULK_PROGRESS 1806／STATUS 1807／STOP 1808／CLOSE 1809、ID_MERGE20_BULK_CLASSIFY 38960。_APS_NEXT_* を 301／1810／38961 へ |
| ceb6c8ab | Src/Merge.rc | IDD_MERGE20_BULK_CLASSIFY DIALOGEX 0,0,640,200、空テンプレート（IDD_MERGE20_MASTER の END の後） |
| a5bc4d72 | Src/Merge20/Merge20BulkClassifyDlg.h | 一括分類ダイアログヘッダ（モーダルレス、Grep 方式。WM_MERGE20_BULK_PROGRESS = WM_USER + 101） |
| d660b049 | Src/Merge20/Merge20BulkClassifyDlg.cpp | ダイアログ本体＋ワーカー（単一バックグラウンドワーカー、進捗・停止・完了ログ行） |
| 04cb7d9d | Src/Merge.vcxproj | Merge20BulkClassifyDlg.cpp／.h 登録（StdAfx.h グループの既定エントリ） |
| df22d3b1 | Src/Merge.vcxproj.filters | 同上 |
| 51975d5b | Src/DirView.h | ダイアログメンバ m_wndBulkClassify、OnMerge20BulkClassify、Merge20BuildBulkClassifyFiles 宣言 |
| 644171cb | Src/DirView.cpp | コンテキストメニュー項目、メッセージマップ、OnDestroy での CloseBulkClassify、ファイル一覧ビルダ |
| e1003fc8 | Translations/WinMerge/Japanese.po | 新規文字列 8 件の日本語訳（§36.5 の監査参照） |
| 82b0ded1 | Src/Merge20/Merge20ClassifyDoc.h | CI 修正：Merge20BulkDiff.h を include（C2065 Merge20BulkRange 未宣言。Merge20ClassifyFromRanges の宣言で使う） |
| 555aabd1 | Src/DirView.h | CI 修正：Merge20BulkClassifyDlg.h を include（C2065 Merge20BulkClassifyFile／m_wndBulkClassify 未宣言） |
| 31eb380d | Src/DirView.cpp | CI 修正：`di.isResultDiff()` → `di.diffcode.isResultDiff()`（isResultDiff は DIFFCODE のメンバ。C2039） |

### 36.2 設計判断（判断理由付き）

- **一括分類は HTML の differ（diffFast/lcs）を純粋モジュールに移植して使う**。CDiffWrapper／diffutils はワーカースレッドで使わない（スレッド安全性が未検証のため）。ワーカーは D7 と同じ単一スレッド（Poco ThreadPool(1,1)）で、結果の順序を保つ。分類本体は Merge20ClassifyDoc.cpp の行ペアリング＋関数名走査（最大 500 行）を流用する（Merge20ClassifyFromRanges として共用化）。開封時の Rescan フック（S4）と一括分類で同じビルダを使うため、両者の分類は一致する。
- **閾値は HTML と同じ n*m > 2,200,000**。トリミング後の中央部（共通 prefix/suffix 除去後）に適用する（HTML と同一）。新規オプションキー OPT_MERGE20_SIMPLE_THRESHOLD（Merge20/SimpleThreshold、既定 2200000、範囲 0〜10000000。0＝フォールバック無効。範囲上限は LCS 表を 10M セル≒40MB に抑えるため）。
- **simpleCompare は一括分類パスでのみ立てる**（＝分類が位置合わせによるもの）。開封時（Rescan フック）では立てない。理由：開封済みファイルの差分は diffutils の完全な差分であり、簡易比較に切り替える対象がない。開封すると Rescan フックが完全な差分で再分類し、simpleCompare は false に戻る（状態は作り直しのため）。これは仕様書 §7 からの食い違いとして明記する（WinMerge の差分エンジンを位置合わせに置き換えることは D10 の範囲外）。
- **[簡易] バッジは要約列（ColSummaryGet）での表示時プレフィックスとする**。msgstr は「簡易」、msgid は設計書 §6 どおり "Simple"。バッジ自体には【Wiz】を付けない（仕様書 §7 と HTML のリテラル "[簡易]" との一致を優先）。ダイアログ題名・メニュー項目・ステータス文言には【Wiz】を付ける。バッジは表示時に付け、保存される summary は汚さない。CSV は ColSummaryGet を流用するためバッジが CSV にも出る（監査証跡として望ましい）。
- **一括分類の対象：未分類の差分ファイル（両側存在の DIFF）＋片側のみファイル**（行数カウントのみで分類。CDirDoc::SetMerge20Classification の存在区分オーバーライドが対応区分・要約を整える）。同一ファイルは対象外（S4 段階 1 と同じ。Rescan フックは HasDiffs() を要求する）。3-way は対象外（メッセージ表示）。バイナリはワーカーが NUL 検査でスキップ（D7 と同じ）。unpacker/prediffer は適用されない（D7 と同じ既知の偏差。オンディスクテキストで分類する）。空白・大文字小文字・空行無視オプションのみ反映し、他の無視オプションは未対応（制限として記録）。
- **範囲の空き側は DIFFRANGE 慣習（end = begin - 1、行数 0）を使う**。-1/-1 は行数が 1 と数えられてしまうため使わない。
- **一括分類は手動トリガのみ**（フォルダ一覧のコンテキストメニュー）。比較完了後の自動実行はしない（比較コストが約 2 倍になることをユーザーに黙って行わないため）。
- **再実行時は未分類ファイルだけが対象**（分類済みはスキップ）。内容が変わったファイルの一括再分類はしない（開封時の Rescan フックで更新される）。制限として記録。
- **セッション JSON に simpleCompare を任意フィールドとして追加**（GetBoolField は欠落時に true を返して既存値を残すため、D10 以前のファイルでは false のまま。後方互換あり）。
- **進捗表示は Grep と同じモーダルレスダイアログ方式**（プログレスバー＋ステータス＋現在ファイル＋停止／閉じる）。D7 の Merge20GrepDlg を雛形とする。ワーカーは CoInitializeEx を先に呼ぶ（MLang 検出のため）。結果は PostMessage（WM_MERGE20_BULK_PROGRESS = WM_USER + 101）で UI スレッドへ draining し、CDirDoc::SetMerge20Classification(PathContext(result.paths), state) で格納＋行再描画。完了時に RootLogger::Info で `Merge20 bulk classify: N files classified (M simple), K skipped, T ms` を 1 行出力。
- **100／500／1000 ファイルの計測値はこの VM では作れない**（実機が必要）。数値を捏造せず、手順と記録様式（空の結果表）を §36.6 に残す。

### 36.3 仕様・設計条項チェック

| 条項 | 判定 | 備考 |
|---|---|---|
| 設計書 §5.2 D10 受け入れ条件 1：1000 ファイル規模で一括計算が UI を固めない（バックグラウンドで進捗表示） | 未検証（実機） | 実装は単一バックグラウンドワーカー＋モーダルレス進捗ダイアログ。実機確認は §36.6 項目 1・2 |
| 設計書 §5.2 D10 受け入れ条件 2：巨大ファイルで簡易比較へ自動切替し [簡易] バッジが表示される | 未検証（実機） | 実装は閾値 2200000 での自動切替＋要約列プレフィックス。実機確認は §36.6 項目 3 |
| 設計書 §5.2 D10 受け入れ条件 3：100／500／1000 ファイルでの計測結果が記録されている（§8.3） | 未検証（実機） | 手順と空の結果表を §36.6 に記録。数値は実機計測後に追記する |
| 設計書 §3.11 一括計算（S4 段階 2） | 満たす（コード） | Merge20BulkClassifyDlg＋ワーカー。DirScan のスレッドプールにならったバックグラウンド計算 |
| 設計書 §3.12 簡易比較 | 満たす（コード） | Merge20BulkDiff のフォールバック＋OPT_MERGE20_SIMPLE_THRESHOLD＋[簡易] バッジ |
| 設計書 §4（計測フック D0） | 満たす | D0 の計測フックは既存（§22）。ログ確認手順の訂正版を §36.4 に記録 |
| 設計書 §8.3 計測記録様式 | 満たす（様式のみ） | 記録様式と空の結果表を §36.6 に記録。数値は実機待ち |
| 仕様書 §3（2/4）S4 段階 2（未開封ファイルの事前分類） | 満たす（コード） | 手動トリガの一括分類で実現。自動実行はしない（§36.2 の判断参照） |
| 仕様書 §7 簡易比較（n*m > 2,200,000、カバレッジ行列 H-SIMPLE） | 満たす（コード、一部食い違いあり） | 閾値・フォールバックは HTML と同一。ただし simpleCompare は一括分類パスでのみ立ち、開封時の差分エンジンは diffutils のまま（§36.2 の判断参照）。仕様書 §7 の「比較エンジン自体を簡易比較に切り替える」読みとは異なる |
| カバレッジ行列 H-SIMPLE | 満たす | 閾値 2200000 を既定とする OPT_MERGE20_SIMPLE_THRESHOLD を新設 |

### 36.4 計測ログの確認手順（D0 §22.4 の訂正版）

- D0 §22.4 の「表示＞ログ（View＞Log）」は不正確。RootLogger::Info → Logger::Log → m_func（CMainFrame::OnCreate で設定、MainFrm.cpp:485）→ CMainFrame::OutputLog → m_logBuffer → 100ms タイマ IDT_FLUSHLOG → ProcessLog → Output ペインの文書へ追加（m_pOutputDoc->AppendLineWithAutoTrim）。表示は View メニュー「&Output Pane」（ID_VIEW_OUTPUT_BAR 32822、IDS_OUTPUTBAR_CAPTION "Output Pane"）。INFO 行はペインを自動表示しない（show=true は ERR のみ）。**計測行を見るときは Output ペインを手動で表示すること**。
- ログファイルは %TEMP%\WinMerge.log（ProcessLog が Poco SimpleFileChannel で作成）。ただし OPT_LOGGING（Settings/Logging、既定 0）が 0 のときファイルは作られない。1＝全レベル、2＝WARN/ERR のみ。**INFO の計測行がファイルに残るのは Logging=1 のときだけ**。計測前に Tools＞Options＞System＞Logging を 1 にすること。
- 見るべき行：フォルダ比較の計測行 `Merge20 measure: folder compare N ms (scan S ms, list L ms), M items`（D0）。一括分類の計測行 `Merge20 bulk classify: N files classified (M simple), K skipped, T ms`（D10）。

### 36.5 検証レポート

- CI 実行 45799955548（head `dfc560d5`、PR #32）：結論 failure。原因はコンパイルエラー 3 件（いずれも include／メンバ所属の見落とし。全文ログ 38,718 行を確認し、エラーはこの 3 ファイルのみであることを確認。Merge20BulkClassifyDlg.cpp 自体・UnitTests 側の新規テストに独立したエラーはなし）：(1) Merge20ClassifyDoc.h が Merge20BulkRange を使うのに Merge20BulkDiff.h を include していなかった（C2065）。(2) DirView.h が Merge20BulkClassifyDlg.h を include していなかった（C2065、m_wndBulkClassify／Merge20BulkClassifyFile 未宣言）。(3) Merge20BuildBulkClassifyFiles が `di.isResultDiff()` と書いていたが isResultDiff は DIFFCODE のメンバ（C2039、正しくは `di.diffcode.isResultDiff()`）。修正コミット 82b0ded1／555aabd1／31eb380d。
- CI 実行 45802864745（head `a4855d0c`、修正 3 件＋本記録コミット込み）：結論 success。GoogleTest 693 テスト全パス（56 スイート、FAILED 0 件。新規 Merge20BulkDiff 12 テスト含む）。コンパイルエラー 0 件。成果物：WinMerge-x64.zip 26,414,474 バイト（ID 15108428662、失効 2027-02-24T10:36:12Z）、WinMerge-x64-Setup-x64.exe 39,864,254 バイト（ID 15109061008、失効 2027-02-24T10:36:12Z）。全文ログは `~/workspace/.coord/d10_final_log.txt` に保存。本記録（成功結果の追記）コミット後の CI 実行も別途確認すること。
- .po 監査：ブランチ版 msgid 2144 件 vs master 版 2136 件（＋8。新規 8 件）。【Wiz】116 件 vs master 版 109 件（＋7）。＋8 と ＋7 の差 1 件は "Simple" → 「簡易」（バッジ用で意図的に【Wiz】なし。§36.2 参照）。"Stop"・"Close" は既存の stock 文字列を流用（新規追加なし）。ブランチ版を API から取得して確認。
- ローカル検証：Merge20BulkDiff 純粋モジュールを g++ スタブ pch ハーネスで検証し、38 アサーション全パス（gtest 12 テストと同内容）。Node 相互検証：HTML 本体から isExcl／isL2Ver／normL／diffFast／lcs／alignHunks を抽出して実行し、8 ケース（identical／onechange／addmissing／moved／emptyold／interleave／multihunk／trailing）の範囲が C++ 出力と完全一致。vcxproj 4 ファイルは XML 整形式をパーサで確認済み。
- 未検証（Windows 実機でのみ確認できる挙動）：§36.6 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 36.6 実機確認チェックリスト（手順付き）

1. 一括分類ダイアログ（100 ファイル規模）：フォルダ比較（差分あり）→ フォルダ一覧を右クリック → 「【Wiz】未開封ファイルを一括分類…」。「【Wiz】一括分類」ダイアログが表示され、プログレスバーと「【Wiz】一括分類中… %2 件中 %1 件（分類済み %3 件）」が進むこと。完了後に「【Wiz】完了：%1 件を分類しました（簡易 %2 件）、%3 件をスキップ（T ms）」になること。分類済みファイルの確認内容列が "Not calculated" から分類結果に変わること。
2. UI の固まり具合（1000 ファイル規模）：1000 ファイル規模のフォルダ比較で一括分類を実行中、フォルダ一覧のスクロール・列ソート・他ウィンドウへの切替ができること（UI が固まらないこと）。停止ボタンを押すと分類が止まり「【Wiz】一括分類を停止しました」になること。停止後に再度メニューから実行すると、未分類ファイルだけが対象になること。
3. [簡易] バッジ（巨大ファイル）：行数×行数が 2,200,000 を超える差分ファイル（例：旧 2000 行・新 2000 行で中央部が大きいもの）を含むフォルダで一括分類を実行し、該当ファイルの要約列の先頭に "[簡易]" が付くこと。該当ファイルを開くと完全な差分で再分類され、"[簡易]" が消えること。
4. 100／500／1000 ファイル計測（手順）：(a) Tools＞Options＞System＞Logging を 1 にする。(b) 100／500／1000 ファイルのフォルダを用意し、それぞれフォルダ比較を実行する。(c) View メニュー「Output Pane」を表示し、`Merge20 measure: folder compare N ms (scan S ms, list L ms), M items` の行を記録する（%TEMP%\WinMerge.log にも残る）。(d) 一括分類を実行し、`Merge20 bulk classify: N files classified (M simple), K skipped, T ms` の行を記録する。(e) 下表に数値を入れる。

| フォルダ規模 | 比較開始→スキャン完了 (scan) | スキャン完了→一覧確定 (list) | フォルダ比較合計 | 一括分類時間 | 備考 |
|---|---|---|---|---|---|
| 100 ファイル | ms | ms | ms | ms | |
| 500 ファイル | ms | ms | ms | ms | |
| 1000 ファイル | ms | ms | ms | ms | |

5. 3-way・同一ファイル・バイナリ：3 方向フォルダ比較でメニューを実行すると「【Wiz】一括分類は2方向フォルダ比較のみ対応しています。」が出ること。同一ファイルのみのフォルダで実行すると「【Wiz】未分類のファイルがありません。」が出ること。バイナリファイルを含むフォルダで実行すると、バイナリがスキップ件数に入ること（完了文言の %3）。
6. 回帰：フォルダ比較・ファイル比較・セッション保存／復元が従来通り動くこと。一括分類で付けた [簡易] がセッション保存・復元後も残ること。

### 36.7 D11 への引き継ぎ・残課題

- D9 からの残課題（D10 の範囲外）：`OPT_MERGE20_SHOW_REVIEW_*` のフィルタ適用＋UI を一体実装する（D9 ではキーのみ）。スリムフラグ自体のセッション永続化。D8 からの残課題：比較ビューでのハイライト表示、リスク重要語変更時の再分類リフレッシュ、HTML 既定除外ワードの未シード、ルール数縮小時の陳腐オプションキー。
- D10 からの残課題：§36.6 の実機確認結果（特に 100／500／1000 ファイル計測値）を本節へ追記すること。実機で問題が出た場合の修正は D11 以降。
- 一括分類の既知の制限（§36.2 参照）：unpacker/prediffer 非適用、空白・大文字小文字・空行以外の無視オプション未対応、再実行は未分類のみ、自動実行なし。
- 検証ゲートの注意：CI 緑だけでは不十分。最終 head の実行結論・全文ログの GoogleTest 全パス行・成果物 2 件の API 確認を毎回行うこと。実機挙動は §36.6 のチェックリストでユーザーに確認してもらうこと。

## 37. Phase 4 D11（保存の完成＋受け入れテスト・文書化）— ブランチ `merge20/phase4-d11`

- PR: #33（https://github.com/askhim659/winmerge/pull/33）。最終 head `be17a341c1ade336ddb779e01054207b5ee580ec`（コード 20 件＋本節の記録コミット）。マージは本記録時点では未実施（コーディネーター判断待ち）。
- 設計書 §5.2 D11、§4.1、§8.2、A-4-1〜A-4-9（§1）、仕様書 §6.1。受け入れ条件：A-4-1〜A-4-9 のすべてが満たされること（コードレベルで満たすものと実機確認待ちのものを区別して記録する）。保存→終了→再起動→復元で §4.1 のすべてが戻ること。
- 文書化の置き場所：設計書 §5.2 D11 と §8.2 は「MERGE20_NOTES.md §22 の追記、実機確認手順の統合版」を求めているが、§22 は D0 の節（基盤の記録）であり後から書き換えない方針のため、統合版チェックリストは本節 §37.7 に置く。§22 への追記は行わない（本節が D11 の文書化の本体であることをここに明記する）。

### 37.1 変更一覧（1 ファイル 1 コミット、20 件＋本節の記録コミット）

| コミット | ファイル | 内容 |
|---|---|---|
| 481e76b0 | Src/OptionsDef.h | OPT_MERGE20_DED_PIN_FIRST（`Merge20/DedPinFirst`、既定 true）追加。D2 残課題 (c) の初回ピン固定用。設計 §4.1(a) の表外の追加である旨をコメントに明記 |
| 748e144c | Src/OptionsInit.cpp | 同キーを true で初期化 |
| a1f2af65 | Src/Merge20/Merge20Session.h | Merge20SessionDisplay に 6 フィールド追加（`dedMode`／`dedPinned`／`todoOnly`／`dedZoom`（既定 0＝未保存）／`slim`／`grepText`） |
| aab3edee | Src/Merge20/Merge20Session.cpp | display オブジェクトへの 6 フィールドのシリアライズとパース追加。パースは欠落時に既定値を残す（既存パターン）。`dedZoom` は 0 を許容し、1〜349 は読み込み失敗（topIndex と同じ扱い）、範囲は 0〜2200 |
| 8093b5a9 | Testing/GoogleTest/Merge20/Merge20Session_test.cpp | MakeSession と SessionRoundTrip に新フィールドの往復確認を追加。新規テスト DisplayFieldsBackwardCompat（D11 以前のファイルは新フィールドなしで読め、既定値になること。`dedZoom` 0 と 350 は可、100 は読み込み失敗、`dedMode:"yes"` は失敗、2200 の往復） |
| aa9416d3 | Src/Merge20/Merge20SessionDoc.h | Merge20SessionRestoreResult に `optionsApplied`（vector<String>）と `deferred`（bool）を追加。Merge20SessionApplyToDoc のシグネチャを `(CDirDoc&, const Merge20Session&, int skippedFiles = 0)` に変更。新規宣言 Merge20SessionApplyPending と Merge20SessionFormatDeferredMessage を追加 |
| 442f1ffc | Src/Merge20/Merge20SessionDoc.cpp | 中核。ApplyToDoc を再構成（差分検出 SessionOptionsDiffer／オプション適用 ApplySessionCompareOptions／状態適用 ApplySessionStates に分離）。差分あり＋スレッド非 busy ならオプション書き戻し＋Rescan＋セッションを doc に stash し `deferred=true`。busy 時は S8 の注記のみ動作を残す。BuildFromDoc で新 display フィールドを取得。ApplySessionStates で新フィールドを復元。ApplyPending（フォルダペア再照合、不一致なら破棄）と FormatDeferredMessage（中間メッセージ）を実装。FormatRestoreReport に optionsApplied 行を追加。OfferRestore と OnMerge20SessionRestore（DirView 側）は deferred で分岐 |
| 8a477061 | Src/DirDoc.h | pending session 用メンバ（`m_bMerge20HasPendingSession`／`m_merge20PendingSession`／`m_merge20PendingSkippedFiles`／`m_merge20PendingOptionsApplied`）とアクセサ Merge20HasPendingSession／Merge20SetPendingSession／Merge20TakePendingSession を追加 |
| 0c4502af | Src/DirDoc.cpp | 上記アクセサの実装（Take は move-out＋stash クリア。2 回適用防止） |
| a11f84bd | Src/DirActions.h | DirViewFilterSettings に `bool show_review[5]` を追加（index は `static_cast<int>(ReviewState)`）。コンストラクタで 5 キーから初期化 |
| e4af3a1d | Src/DirActions.cpp | IsShowable にレビュー判定を追加（ファイル分岐末尾。範囲外の状態は表示する防御付き）。ツリーモードのフォルダはレビューフィルタが何か隠しているときだけ子走査して全滅なら隠す（全 5 フィルタ ON の既定では走査しない） |
| 20ee86bb | Src/resource.h | コマンド ID 5 件追加（ID_MERGE20_SHOW_REVIEW_TODO 38961〜ID_MERGE20_SHOW_REVIEW_KEEP 38965）。D10 コメント更新。`_APS_NEXT_COMMAND_VALUE` を 38966 へ。値ソートで 38961-38965 の重複なしを確認済み |
| 0a26be27 | Src/DirView.h | OnMerge20ShowReview（ON_COMMAND_RANGE 用）と Merge20RestoreGrepText の宣言を追加 |
| 02e61ba4 | Src/DirView.cpp | (a) OnUpdateUIMessage の EVENT_COMPARE_COMPLETED で OfferRestore の直前に pending apply フック（成功時は最終レポート表示＋UpdateMerge20StatusCount、不一致時は短いメッセージ）。(b) OnMerge20SessionRestore で deferred 分岐（延期時は中間メッセージ）。(c) Merge20GetDisplaySettings に `display.slim` を追加。(d) Merge20SetDisplayFilter に reviewFilter（5 キーへ書き戻し＋`m_dirfilter.show_review[i]`）と slim（SetMerge20Slim、Redisplay の前に呼ぶ。Redisplay はちょうど 1 回になるよう分岐）の適用を追加。(e) コンテキストメニュー（Slim List の直後）にレビューサブメニュー追加＋Merge20ReviewStateMenuName／Merge20ShowReviewKey static ヘルパー＋OnMerge20ShowReview（トグル＋SaveOption＋`m_dirfilter.show_review[i]` 更新＋Redisplay）＋メッセージマップに ON_COMMAND_RANGE。(f) Merge20RestoreGrepText（`m_wndGrep.RestoreSearchText` へ転送）。BCMenu サブメニューは関数スコープで TrackPopupMenu まで生かす（ブロックを抜けるとデストラクタが走るため。実装中に修正） |
| fb712ede | Src/DiffViewBar.h | LoadState override 宣言を追加 |
| df3d076a | Src/DiffViewBar.cpp | LoadState override（基底呼出後にキー＞0 なら `m_szHorz.cy`／`m_szFloat.cy` にクランプ 60〜2000px で適用。0 は未保存扱い） |
| 9e40b2ae | Src/MergeEditFrm.cpp | (a) OnBarCheck で ID_VIEW_DETAIL_BAR 時に OPT_MERGE20_DOCK_HIDDEN を保存（閉じるボタンもここを通る）。(b) SavePosition で OPT_MERGE20_DOCK_HIDDEN と OPT_MERGE20_FOCUS_DOCK_HEIGHT（ドッキング時のみ。フローティング時はキャプション分が入るため保存しない）を保存。(c) OnCreate で LoadState の後にキー true なら DetailBar を隠す（false のときは強制表示しない非対称設計）。(d) EnterDedicatedMode で初回ピン固定（OPT_MERGE20_DED_PIN_FIRST が true ならピンを ON にしてキーを false へ） |
| e4f516f0 | Src/Merge20/Merge20GrepDlg.h | public メソッド RestoreSearchText の宣言を追加 |
| d3335068 | Src/Merge20/Merge20GrepDlg.cpp | 同実装（空なら no-op。Merge20GrepPushHistory で履歴先頭へ＋SaveOption。開いているときは RefreshHistoryCombo でコンボ更新。閉じているときは次回 ShowGrep が履歴から再構築） |
| 0c8eb790 | Translations/WinMerge/Japanese.po | 新規文字列 4 件の日本語訳（§37.6 の監査参照） |

### 37.2 設計判断（判断理由付き）

- **比較オプションの復元時適用は 2 段階にする**。`Rescan()` は非同期（diff スレッド開始後すぐ戻る）ため、適用は「即時：オプション書き戻し＋Rescan＋セッションを doc に stash」「完了時：ファイル状態＋表示設定を適用」に分ける。完了フックは `CDirView::OnUpdateUIMessage` の `EVENT_COMPARE_COMPLETED`（OfferRestore の直前）。適用経路は確認済み：`CDirDoc::InitDiffContext` が OptionsMgr からすべて読み直すため、適用＝OptionsMgr へ書き戻すこと。再帰フラグだけは `RefreshOptions()` 経由（InitDiffContext は `m_bRecursive` を設定しない）。
- **比較スレッド busy 時は S8 の注記のみ動作を残す**。復元要求時に比較が走っている場合、オプション適用のための Rescan は `THREAD_COMPARING` 中は no-op で戻るため、状態を現行リストへ適用し、異なるオプションはレポートに列挙するだけにする（S8 と同じ）。実機で復元要求と再比較が重なる場面は稀だが、制限として記録する。
- **ファイルフィルタはマスク／式そのものを保存する**。`OPT_FILEFILTER_CURRENT` に入っているのはフィルタ名ではなくマスク／式そのもの（MainFrm.cpp:2622 で `GetMaskOrExpression()` を保存）。`SetMaskOrExpression` は存在しない名前付きフィルタをリテラルマスクとして扱うため、名前付きフィルタ（`pf:...`）の定義ファイルが消えた場合はリテラルマスクとして復元される（制限として記録）。
- **保留セッションはフォルダペアを再照合する**。再比較中にヘッダバーでフォルダを切り替えると別フォルダペアの比較が始まるため、適用前に保存時パスと現行パス（大小文字無視）を照合し、不一致なら stash を破棄して短いメッセージを表示する。2 回適用は `Merge20TakePendingSession` の move-out＋クリアで防ぐ。
- **レビューフィルタはファイルだけを直接フィルタする**。ディレクトリ行自体はフィルタしない（既存の show_* と同じ考え方）。ツリーモードのフォルダは、レビューフィルタが何か隠しているときだけ子走査し、全滅なら隠す。全 5 フィルタ ON（既定）のときは子走査を走らせない（コスト対策）。`m_hasShowableDescendantCache` は Redisplay 時に clear されるため、フィルタ変更は自動で反映される。
- **レビューフィルタの UI はフォルダ一覧のコンテキストメニューに置く**。View メニュー（.rc 手術）ではなく Merge20 群のコンテキストメニューに 5 状態のサブメニュー「【Wiz】確認内容で表示」を置く。S2 の既存の 5 状態表示名（msgid は既存）を再利用し、新規文字列はサブメニュー題名 1 件のみにする。BCMenu の動的サブメニューは PluginMenu.cpp:91 の前例どおり、サブメニューオブジェクトを TrackPopupMenu が戻るまでスタックに生かすこと。Detach はしない（BCMenu の owner-draw item data がサブメニュー側に属するため）。
- **スリム復元の順序は SetDisplayFilter→ApplyColumnSettings**。保存時にスリム ON だった場合、columnOrders／columnWidths には記憶したフルレイアウトが入っている。SetDisplayFilter で先に `SetMerge20Slim(true)`（スリムプリセット適用＋現行レイアウトを記憶）を適用し、後から ApplyColumnSettings（D9 の slim 対応済み。スリム ON 時は保存値を記憶フルレイアウトへ差し替え、プリセットを再適用）がフルレイアウトを正しく記憶する。逆順だと保存値がスリムプリセットを上書きしてスリムが壊れる。Redisplay は SetDisplayFilter 内でちょうど 1 回（SetMerge20Slim が内部で Redisplay するため、スリム ON／OFF 切り替え時はそちらに任せ、両方 OFF のときだけ直接 Redisplay する）。
- **ドック隠し状態は OnBarCheck と SavePosition の両方で保存する**。メニュー切替とバーの閉じるボタンはどちらも OnBarCheck を通る。専用モードの出入りは ShowControlBar 直接で OnBarCheck を通らず、DestroyWindow は SavePosition の前に ExitDedicatedMode で通常レイアウトへ戻すため、専用モードの一時隠しがキーに漏れない。適用は OnCreate で LoadState の後にキー true のときだけ隠す（false のときは強制表示しない＝LoadState の結果を尊重する、非対称で安全な設計）。
- **フォーカスドックの高さは LoadState override で復元する**。DedBar の前例（LoadState override で `m_szHorz.cy` を強制）を踏襲。保存は SavePosition でドッキング時のみ（フローティング時の GetWindowRect にはキャプション分が入るため）。60〜2000px にクランプ。0 は未保存扱い。
- **D2 の残課題 (c)：見えるホバーストリップは不採用、初回ピン固定を採用**。ストリップ不採用の理由：フレームのクライアント領域はスプリッタが占めており、新規トップエッジ子ウィンドウかフレーム描画が必要でレイアウト干渉リスクが高い。機能的ストリップ（上端 8px へのカーソルで 1.6 秒再表示）は既に存在する。初回ピン固定の採用：新規キー `OPT_MERGE20_DED_PIN_FIRST`（`Merge20/DedPinFirst`、既定 true）を追加し、初回入場時にピンを ON にしてキーを false へ倒す。D3 の懸念（自動非表示既定と矛盾）は初回限定にすることで解消し、2 回目以降は HTML 既定（ピンなし・自動隠し）が維持される。`UpdateDedBar` がキーからボタン状態を同期するため、キー設定だけでよい。設計 §4.1(a) の表にないキー追加であることを明記する。
- **テーマは既存キーで満たす（新規キーなし）**。`OPT_COLOR_SCHEME`（`Settings/ColorScheme`）／`OPT_COLOR_SCHEME_DARK` があり、Options ダイアログで束縛され、起動時に適用され、OptionsMgr が自動永続化する（Merge.cpp:392、MainFrm.cpp:4553）。設計書 §4.1 どおり「テーマはセッションではなく永続設定側に保存する（既存のテーマ設定のキーを使う。新規キーを作らないこと）」。
- **「カメラ」はスナイパーの ON／文字サイズ／レイアウトと解釈する**。設計書 §4.1(a) にカメラ位置のキーはなく、スナイパーの保存項目は `OPT_MERGE20_SNIPER_ON`／`SNIPER_FONT`／`SNIPER_LAYOUT` の 3 キー（D0 で追加、D6 で配線済み）である。スナイパーの表示位置は開いた差分そのものであり、別途保存する位置はない。
- **dedMode の復元はキーへの書き戻しのみ**。開いているファイル比較を専用モードへ強制しない（復元はフォルダ一覧側の操作であり、開封中のフレームのモードを変えるのは副作用が大きいため）。次に開くファイル比較は保存時のモードで開く。
- **Grep の復元は履歴先頭への prepend**。`Merge20GrepPushHistory` の既存ルール（最新 10 件・重複排除）で履歴先頭へ置き、SaveOption する。ダイアログが開いている場合はコンボも更新する（`RefreshHistoryCombo()` は protected のため、public の `RestoreSearchText` を追加）。閉じている場合は次回 ShowGrep が履歴から再構築する。

### 37.3 §4.1 照合表

#### (a) 永続設定キー（OptionsDef.h／OptionsInit.cpp、D0 で追加済み。D11 は未配線だった 2 キーを配線し、1 キーを新規追加）

| キー | 保存 | 復元 | 備考 |
|---|---|---|---|
| `OPT_MERGE20_DED_MODE` | 満たす（D1） | 満たす（D1。次回起動時に専用モードで開く。D11 はセッション側 `dedMode` も追加） | 既存 |
| `OPT_MERGE20_DED_PINNED` | 満たす（D2） | 満たす（D2。UpdateDedBar がキーから同期） | 既存 |
| `OPT_MERGE20_DED_ZOOM` | 満たす（D2） | 満たす（D2。入場時に適用。D11 はセッション側 `dedZoom` も追加） | 既存 |
| `OPT_MERGE20_TODO_ONLY` | 満たす（D3） | 満たす（D3。D11 はセッション側 `todoOnly` も追加） | 既存 |
| `OPT_MERGE20_SNIPER_ON` | 満たす（D6） | 満たす（D6。専用モード入場時に適用。退場時に OFF） | 既存 |
| `OPT_MERGE20_SNIPER_FONT` | 満たす（D6） | 満たす（D6） | 既存 |
| `OPT_MERGE20_SNIPER_LAYOUT` | 満たす（D6） | 満たす（D6） | 既存 |
| `OPT_MERGE20_SLIM` | 満たす（D9。フラグ永続化と OnInitialUpdate での再適用） | 満たす（D9＋D11。D11 はセッション側 `slim` も追加し、復元時に SetMerge20Slim で適用） | D11 で完成 |
| `OPT_MERGE20_DOCK_HIDDEN` | 満たす（D11。OnBarCheck＋SavePosition） | 満たす（D11。OnCreate で LoadState の後にキー true のときだけ隠す） | D11 で配線 |
| `OPT_MERGE20_FOCUS_DOCK_HEIGHT` | 満たす（D11。SavePosition、ドッキング時のみ） | 満たす（D11。CDiffViewBar::LoadState override、60〜2000px にクランプ） | D11 で配線 |
| `OPT_MERGE20_GREP_HISTORY` | 満たす（D7。検索ごとに最新 10 件を保存） | 満たす（D11。セッション側 `grepText` を履歴先頭へ＋開いているダイアログのコンボも更新） | D11 で完成 |
| `OPT_MERGE20_DED_PIN_FIRST` | 満たす（D11。新規キー、既定 true） | 満たす（D11。初回入場時にピンを ON にしてキーを false へ） | 設計 §4.1(a) の表外の追加（D2 (c) のため） |
| `OPT_MERGE20_SHOW_REVIEW_*`（5 キー） | 満たす（D9。キーのみ） | 満たす（D11。IsShowable でのフィルタ適用＋コンテキストメニューのサブメニュー UI） | D11 で一体実装 |

テーマ（`OPT_COLOR_SCHEME`／`OPT_COLOR_SCHEME_DARK`）：既存キーで満たす（新規キーなし。§37.2 参照）。

#### (b) セッションの表示設定（Merge20SessionDisplay、D11 で 6 フィールド追加）

| フィールド | 保存 | 復元 | 備考 |
|---|---|---|---|
| `dedMode` | 満たす（BuildFromDoc で OptionsMgr から取得） | 満たす（OptionsMgr へ書き戻し。次に開くファイル比較に反映） | 0／1 の真偽値として保存。D11 以前のファイルでは既定 false |
| `dedPinned` | 満たす（同上） | 満たす（同上） | 同上 |
| `todoOnly` | 満たす（同上） | 満たす（同上） | 同上 |
| `dedZoom` | 満たす（同上。0＝未保存） | 満たす（＞0 のときのみ書き戻し。0 は未保存扱い） | 範囲 0〜2200。1〜349 は読み込み失敗（topIndex と同じ扱い） |
| `slim` | 満たす（Merge20GetDisplaySettings で `m_bMerge20Slim` から取得） | 満たす（Merge20SetDisplayFilter で SetMerge20Slim を呼ぶ） | 保存時にスリム ON だった場合、columnOrders／columnWidths には記憶したフルレイアウトが入る（D9 の仕様） |
| `grepText` | 満たす（BuildFromDoc で履歴先頭を取得） | 満たす（Merge20RestoreGrepText で履歴先頭へ＋開いているダイアログのコンボも更新） | 空のときは no-op |

### 37.4 仕様・設計条項チェック（A-4-1〜A-4-9 照合表）

| 条項 | 判定 | 備考 |
|---|---|---|
| A-4-1 専用モード：実フォルダで専用モードに入り、バー自動隠し＋ピン固定、Prev／Next と確認内容 5 ボタン、未確認のみ巡回、倍率引継ぎ（ファイル移動時に倍率が維持され、表示位置だけ先頭に戻る）が確認できること | 満たす（コード）＋未検証（実機） | D1／D2 の実装＋D11 の初回ピン固定・dedMode／dedPinned／dedZoom のセッション保存。実機確認は §37.7 項目 1・2・3・16 |
| A-4-2 同期スクロール：スナイパーの 3 パネル（OLD／NEW／MERGED）とフォーカスドックの 3 段で、1 つのパネルをスクロールすると他のパネルが同じ位置へ動くこと | 満たす（コード）＋未検証（実機） | D4／D6 の実装。D11 での変更なし。実機確認は §37.7 項目 5・7 |
| A-4-3 未確認のみ巡回：専用モードと通常モードの両方で、未確認のみ巡回 ON 時に確認済みファイル・確認済み差分を飛ばし、末尾で先頭へ回ること | 満たす（コード）＋未検証（実機） | D3 の実装＋todoOnly のセッション保存（D11）。実機確認は §37.7 項目 4 |
| A-4-4 分割・再結合：差分グループを ✂ 分割して行単位で判定でき、↩ 戻すで再結合できること。分割後もセッション保存・復元と学習候補が壊れないこと（壊れる場合は再計算されること） | 満たす（コード）＋未検証（実機） | D5 の実装。D11 での変更なし。実機確認は §37.7 項目 6 |
| A-4-5 Grep：OLD／NEW 全ファイルを横断検索でき、ヒット箇所から該当差分へジャンプできること。1000 ファイル規模でも UI が固まらないこと | 満たす（コード）＋未検証（実機） | D7 の実装＋grepText の保存・復元（D11）。実機確認は §37.7 項目 8 |
| A-4-6 マスタ管理：除外ワードマスタ・強調文字・リスク重要語の管理画面があり、設定ファイルとして入出力（共有）できること | 満たす（コード）＋未検証（実機） | D8 の実装。D11 での変更なし。実機確認は §37.7 項目 9 |
| A-4-7 保存：S8 で保存対象外だったテーマ・検索文字列・スリム／ドック／カメラ／専用モード／ピンを含め、仕様書 §6.1 の「画面に出ているものはすべて」が保存・復元されること。比較オプションは復元時に適用されること（復元レポートへの注記だけでは済ませない） | 満たす（コード）＋未検証（実機） | §37.3 のとおり §4.1 のすべてを実装。比較オプションは復元時に適用へ変更（busy 時は S8 の注記のみ動作を残す。§37.5 参照）。実機確認は §37.7 項目 11・13・14・15・17 |
| A-4-8 速度：100-1000 ファイル規模での速度確認（比較開始から一覧確定までの体感と計測）を記録すること。計測手順は §8.3 のとおり | 未検証（実機） | D10 の計測手順・空の結果表は §36.6 に記録済み。数値は実機待ち。D11 で新規の計測値は作らない（この VM では作れないため数値を捏造しない） |
| A-4-9 CI：各スライスの CI が緑で、成果物（`WinMerge-x64.zip`）が artifacts API で確認できること。GoogleTest が全パスすること | （PR 作成後に追記） | CI 実行・GoogleTest・成果物を §37.6 に記録 |
| 設計書 §5.2 D11：S8 保存対象外項目の保存（§4.1 の (a)／(b) をすべて実装） | 満たす（コード） | §37.3 参照 |
| 設計書 §5.2 D11：比較オプションの復元時適用（注記ではなく適用） | 満たす（コード。一部制限あり） | 2 段階設計。busy 時は S8 の注記のみ動作を残す（§37.5 参照） |
| 設計書 §5.2 D11：D9 からの引き継ぎ（OPT_MERGE20_SHOW_REVIEW_* のフィルタ適用＋UI を一体実装。スリムフラグ自体のセッション永続化） | 満たす（コード） | IsShowable でのフィルタ適用＋サブメニュー UI＋`display.slim` の保存・復元 |
| 設計書 §5.2 D11：D3 からの残課題（D2 の auto-hide の問題 (c)）の検討 | 満たす（コード） | 見えるホバーストリップは不採用（理由は §37.2）。初回ピン固定を採用 |
| 設計書 §5.2 D11：受け入れテスト（A-4-1〜A-4-9 の照合）と文書化（§22 の追記、実機確認手順の統合版） | 満たす（文書化。一部は実機待ち） | 照合表は本節 §37.4。統合版チェックリストは §37.7。§22 自体は書き換えず本節に集約（理由は本節冒頭参照） |
| 仕様書 §6.1「画面に出ているものはすべて」保存対象 | 満たす（コード）＋未検証（実機） | §37.3 のとおり。保存漏れは不具合という要求に対し、§4.1 の項目はすべて配線済み。実機確認は §37.7 項目 15 |

### 37.5 設計書・仕様書との食い違いと判断理由

- **`OPT_MERGE20_DED_PIN_FIRST` は設計書 §4.1(a) の表にない新規キー**。D2 の残課題 (c)（初回ピン固定）のため。既定 true、初回入場時にピンを ON にしてキーを false へ倒す。2 回目以降は HTML 既定（ピンなし・自動隠し）が維持される。
- **比較オプション適用の busy 時フォールバック**。復元要求時に比較スレッドが走っている場合、オプション適用のための Rescan は no-op で戻るため、S8 の注記のみ動作（状態を現行リストへ適用し、異なるオプションはレポートに列挙）を残す。設計書 §5.2 D11 の「適用する」には完全には従わないが、比較中の Rescan を強制すると進行中の比較を壊すため、安全側のフォールバックとする。
- **名前付きファイルフィルタの定義ファイル消失時**。保存値はマスク／式そのものであり、`SetMaskOrExpression` は存在しない名前付きフィルタ（`pf:...`）をリテラルマスクとして扱う。定義ファイルが消えた場合は元の名前付きフィルタには戻らない（WinMerge 本体の仕様に依存する動作のため、D11 では対処しない）。
- **dedMode の復元は開封中のフレームを専用モードへ強制しない**。復元はフォルダ一覧側の操作であり、開封中のファイル比較のモードを変えるのは副作用が大きいため、キーへの書き戻し（次に開くファイル比較に反映）のみとする。設計書 §4.1(b) の「専用モードで開いていたか」は次回起動・次回開封時のモードとして保存・復元される。
- **レビューフィルタの UI 配置**。設計書は D9 で配置を決めるとしていたが、D9 ではキーのみで UI は付かなかった。D11 では View メニュー（.rc 手術）ではなくフォルダ一覧のコンテキストメニューに置いた（S3／S6／S8 と同じ DirView 配置の前例。メインメニュー配置は設計書 §2.2 の想定と異なるが、S8 の手動保存／復元の配置判断と同じ理由）。
- **「カメラ」の解釈**。設計書 §8.2 項目 11 と仕様書 §6.1 の「カメラ」は、設計書 §4.1(a) のスナイパー 3 キー（ON／文字サイズ／レイアウト）と解釈する（§37.2 参照）。HTML の camera position に相当する別キーは設計書 §4.1 になく、フォークではスナイパーの表示位置は開いた差分そのものである。

### 37.6 検証レポート

- CI 実行 45810909702（head `be17a341`、PR #33）：結論 success。GoogleTest 694 テスト全パス（56 スイート、FAILED 行なし。新規 DisplayFieldsBackwardCompat 1 テスト含む。D10 時点 693 件から＋1）。コンパイルエラー 0 件。成果物：WinMerge-x64.zip 26,422,960 バイト（ID 15113681946、失効 2027-02-24T11:28:33Z）、WinMerge-x64-Setup-x64.exe 39,871,959 バイト（ID 15113581839、失効 2027-02-24T11:28:33Z）。全文ログは `~/workspace/.coord/d11_final_log.txt` に保存（47,453 行）。本記録（成功結果の追記）コミット後の CI 実行も別途確認すること。
- .po 監査：ブランチ版 msgid 2148 件 vs master 版 2144 件（＋4。新規 4 件）。【Wiz】117 件 vs master 版 116 件（＋1）。＋4 と ＋1 の差 3 件は状態文のため意図的に【Wiz】なし（中間メッセージ・不一致メッセージ・レポート行。D3 の前例。表示名・ラベルであるサブメニュー題名「Show review states」→「【Wiz】確認内容で表示」のみ【Wiz】付き）。ブランチ版を API から取得して確認。CI の番兵は msgid の存在しか見ないため、件数監査で msgstr の欠落がないことを確認済み。
- ローカル検証：GoogleTest 新規 1 件（DisplayFieldsBackwardCompat。D11 以前のファイルとの後方互換を含む）。静的確認：全変更ファイルの括弧バランス、resource.h の値ソートによるコマンド ID 重複チェック（38961-38965 に重複なし）、`theApp.GetGlobalFileFilter()`／`m_diffThread`／`GetMerge20FileStates()` const 版／`strutils::makelower` の宣言・可視性の突合、BCMenu サブメニューの生存期間の修正（実装中に発見し修正）。vcxproj の変更なし（新規 .cpp なしのため登録不要）。
- 未検証（Windows 実機でのみ確認できる挙動）：§37.7 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 37.7 実機確認チェックリスト（統合版、手順付き）

設計書 §8.2 の 12 項目に D11 の追加項目（13〜18）と Phase 3 からの引き継ぎ（19〜20）を統合したもの。D1〜D10 の各節のチェックリスト（§23.6、§24.6、§27.6、§28.6、§31.6、§33.6、§34.6、§35.6、§36.6）は本節に統合される（重複する項目はここに一本化する）。実機で問題が出た場合は本節へ結果を追記すること。

1. **専用モードの入場／退場**（§8.2 項目 1、A-4-1）：実フォルダで専用モードに入り、専用バーが表示されること。通常モードへ戻ると元のバー類の表示状態が復元されること。入場／退場を 10 回繰り返し、表示が崩れないこと。
2. **専用バーの自動隠し＋ピン**（§8.2 項目 2、A-4-1）：既定でバーが自動隠しになり、上端ホバーで再表示されること。📌ピンで固定切替ができること。バー非表示中にファイル移動・確認操作をするとバーが短時間表示されること。
3. **倍率引継ぎ**（§8.2 項目 3、A-4-1）：専用モードで倍率を 150% にしてファイルを移動しても倍率が 150% のままであること（表示位置だけ先頭に戻ること）。R キーで 75% に戻ること。通常モードではファイル移動時に 75% へリセットされること。
4. **未確認のみ巡回**（§8.2 項目 4、A-4-3）：確認内容が混ざった実フォルダで、未確認のみ巡回 ON 時に確認済みファイル・確認済み差分を飛ばし、末尾で先頭へ回ること。OFF 時は従来の巡回に戻ること。専用モードと通常モードの両方で行うこと。
5. **フォーカス詳細**（§8.2 項目 5、A-4-2）：差分を選択すると Diff Pane に旧／新／マージ後の 3 段で該当差分だけが表示されること。3 段のスクロールが連動すること。判定ボタンで結果ペインのセグメント状態が変わること。
6. **分割・再結合**（§8.2 項目 6、A-4-4）：差分グループを ✂ 分割して行単位で判定でき、↩ 戻すで再結合できること。分割後にセッション保存・復元を行い、判定が正しく戻ること。分割後に学習候補が再計算されること。
7. **オートスナイパー**（§8.2 項目 7、A-4-2）：専用モードでスナイパーを ON にし、差分行をダブルクリックすると全画面オーバーレイが開き、該当差分だけが 3 パネルで表示されること。3 パネルのスクロールが同期すること。J／K・矢印キーで前／次差分へ移動できること（末尾で先頭へ回ること）。Esc または ✕ で閉じること。スナイパー OFF 時にダブルクリックすると従来の差分選択になること。文字サイズとレイアウトの変更が保存・復元されること。
8. **Grep**（§8.2 項目 8、A-4-5）：OLD／NEW 全ファイルを横断検索でき、ヒット箇所から該当差分へジャンプできること。1000 ファイル規模でも UI が固まらないこと。
9. **マスタ管理**（§8.2 項目 9、A-4-6）：除外ワード・強調文字・リスク重要語を画面で編集でき、保存・復元されること。除外ワードの変更後に再判定で全ファイルが再計算されること。入出力で JSON を共有できること。
10. **スリムリスト**（§8.2 項目 10、D9）：スリムリストの切替で列が隠れ、確認内容タグが折り返し表示されること。1000 ファイル規模での切替時の応答性を確認する。
11. **保存の完成**（§8.2 項目 11、A-4-7）：保存→終了→再起動→復元で §4.1 のすべて（テーマ・検索文字列・スリム／ドック／カメラ／専用モード／ピン）が戻ること。比較オプションが復元時に適用されること（詳細は項目 14・15）。
12. **キーボード**（§8.2 項目 12）：F・R が専用モード内でのみ有効で、通常モードでは従来動作を壊さないこと。1〜5、Space、J／K、Esc、Ctrl+Shift+F が仕様書 §4(g) どおりに動くこと。
13. **確認内容フィルタ**（D11、A-4-7）：フォルダ一覧を右クリック → 「【Wiz】確認内容で表示」サブメニューで「未確認」のチェックを外すと、未確認ファイルだけが一覧から消えること（確認済みファイルだけが残ること）。ツリーモードでは、子が全て隠れたフォルダも一覧から消えること。チェックを戻すと再表示されること。フィルタ状態を変えてセッションを保存→復元すると、フィルタ状態が戻ること。
14. **比較オプションの復元時適用**（D11、A-4-7）：(a) 比較オプション（例：空白無視 ON／OFF、再帰 ON／OFF、ファイルフィルタ）を変えてセッションを保存する。(b) オプションを元に戻してから同じフォルダペアでセッションを復元する。(c) 中間メッセージ「保存されていた比較オプション（…）を適用し、フォルダを再比較しています。再比較が終わったら確認内容、出力チェック、メモ、マージ判断を復元します。」が出て、再比較が走ること。(d) 再比較完了後に最終レポート「注：保存されていた比較オプション（…）を適用し、フォルダを再比較しました。」が出て、確認内容・出力チェック・メモ・マージ判断が復元されていること。(e) オプションが一致する場合は中間メッセージが出ず、従来どおり最終レポートだけが出ること。
15. **保存→終了→再起動→復元（§4.1 全項目）**（D11、A-4-7）：(a) テーマを変えて終了→再起動するとテーマが戻ること。(b) Grep で検索文字列を入れて終了→再起動→Grep を開くと検索文字列が履歴先頭に入っていること。(c) スリムリスト ON で終了→再起動→同じフォルダを開くとスリムリストのままであること（列が隠れたまま）。(d) フォーカスドックを隠して終了→再起動→ファイル比較を開くとドックが隠れたままであること。ドックの高さを変えて終了→再起動すると高さが戻ること（ドラッグで高さを変えた場合）。(e) 専用モード ON・ピン ON・倍率 150%・未確認のみ巡回 ON で終了→再起動→同じフォルダを開くと専用モードで開き、ピン・倍率・巡回が戻っていること。(f) スナイパー ON・文字サイズ・レイアウトを変えて終了→再起動すると戻っていること。
16. **専用モードの初回ピン固定**（D11、D2 (c)、A-4-1）：初めて専用モードに入るとピンが ON になっていること（バーが自動で隠れないこと）。2 回目以降の入場ではピンが OFF（自動隠し）に戻っていること。ピンを手動で OFF にして終了→再起動→専用モードに入ると、ピン OFF のままであること（初回ピン固定は最初の 1 回だけ）。
17. **フォーカスドックの隠し状態・高さの復元**（D11、A-4-7）：View メニュー「Diff Pane」のチェックを外して終了→再起動→ファイル比較を開くとドックが隠れたままであること。チェックを入れて終了→再起動すると表示されたままであること。ドックをドラッグで高さを変えて終了→再起動すると高さが戻ること。専用モードに入って出ただけでは、ドックの隠し状態が変わらないこと（専用モードの一時隠しが保存されないこと）。
18. **A-4-8 速度計測**（D10 §36.6 の表へ記録）：100／500／1000 ファイルのフォルダで、比較開始→一覧確定の時間と体感を §36.6 の手順で計測し、同節の空の結果表に数値を入れること。一括分類の計測行もあわせて記録すること。
19. **T-3-7 完全一致キーの HTML とのヒット率実測**（Phase 3 からの引き継ぎ。設計書 §5.3）：実ファイルで学習の完全一致キーのヒット率を HTML と比べ、差分エンジンの行グルーピング差による影響を確認すること。実機確認待ち。
20. **T-3-8 `bNoSrc` 経路の実機発火**（Phase 3 からの引き継ぎ。設計書 §5.3）：OP_1STONLY に {1} を選んだときの bNoSrc（`<No Lines>`）経路が実機で発火することを確認すること。実機確認待ち。

### 37.8 Phase 4 終了時点の残課題一覧

- D8 からの残課題（D11 の範囲外）：(a) 比較ビューでのハイライト表示。(b) リスク重要語変更時の再分類リフレッシュ。(c) HTML 既定除外ワードの未シード。(d) ルール数縮小時の陳腐オプションキー。
- D9 からの残課題：なし（`OPT_MERGE20_SHOW_REVIEW_*` のフィルタ適用＋UI とスリムフラグのセッション永続化は D11 で実装済み）。
- D10 からの残課題：§36.6 の実機確認結果（特に 100／500／1000 ファイル計測値）を §36.6 へ追記すること。実機で問題が出た場合の修正は D11 以降。
- D11 で判明した制限：(a) 比較オプション適用の busy 時フォールバック（比較スレッドが走っている場合、オプションは適用されず S8 の注記のみ動作になる。§37.5 参照）。(b) 名前付きファイルフィルタの定義ファイル消失時、復元値はリテラルマスク扱いになる（§37.5 参照）。(c) dedMode の復元は開封中のフレームを専用モードへ強制しない（次に開くファイル比較に反映。§37.5 参照）。(d) フォーカスドックの高さはフローティング時には保存されない（キャプション分が入るため。ドッキング時のみ保存）。(e) レビューフィルタの UI はフォルダ一覧のコンテキストメニューにのみあり、View メニューやフィルタバーにはない。(f) 見えるホバーストリップは不採用（初回ピン固定で代替。§37.2 参照）。
- 実機確認待ち（§37.7 の全項目。特に A-4-1〜A-4-8 の実機確認と T-3-7／T-3-8）。実機で確認するまで「できた」とは言わない。
- 品質修正版（D1〜D11）のリリース後に完全版へ進む際は、本節の残課題と実機確認結果を反映すること。

## 38. Panel P0 — 常設ファイル一覧パネル基盤（設計書 §12.7 P0）

### 38.1 変更一覧（1 ファイル 1 コミット、20 件＋本節の記録コミット）

| # | ファイル | 内容 |
|---|---|---|
| 1 | Src/Merge20/Merge20FileListCtrl.h | 新規。`CMerge20FileListCtrl`（`CListCtrl` 派生。仮想リスト `LVS_OWNERDATA`）の宣言。行はキャッシュしたファイル名文字列 |
| 2 | Src/Merge20/Merge20FileListCtrl.cpp | 新規。`Refresh()`（`GetFirstDiffPosition`／`GetNextDiffPosition` の平坦な深さ優先走査で全 `DIFFITEM` を列挙。表示値は S6 の `Merge20CsvGetFileNameText` を流用しフォルダ比較の一覧と一致）。`LVN_GETDISPINFO` で行テキストを供給 |
| 3 | Src/Merge20/Merge20FileListBar.h | 新規。`CMerge20FileListBar`（`CSizingControlBarCF` 派生。左ドック）の宣言。子は一覧と空表示ラベル。幅クランプ用の `LoadState`／`CalcFixedLayout` override |
| 4 | Src/Merge20/Merge20FileListBar.cpp | 新規。`OnCreate`（初期幅 280px・最小幅 160px、子 2 つの手動レイアウト）、幅クランプ（160〜600px）、`Refresh()`（フォルダ比較なしのとき一覧を隠して「フォルダ比較がありません」を表示し一覧を無効化）、`UpdateResources()` |
| 5 | Src/StdAfx.h | `MSG_MERGE20_FILELIST_REFRESH = WM_USER + 4` を追加（`WM_USER + 1〜3` は使用済み、`+4` は未使用を確認） |
| 6 | Src/DiffContext.h | 通知コールバックを追加（`Merge20FileListNotifyCallback = std::function<void()>`、`SetMerge20FileListNotifyCallback`、`NotifyMerge20FileListChanged`、メンバ `m_merge20FileListNotify`）。`#include <functional>` を追加 |
| 7 | Src/DiffContext.cpp | `NotifyMerge20FileListChanged()` を実装し、単独変更の setter 6 件（確認内容 2 件・出力 1 件・`ApplyMerge20ReviewState`・分類・メモ）から呼ぶ |
| 8 | Src/OptionsDef.h | 新キー `OPT_MERGE20_FILELIST_VISIBLE`（`Merge20/FileListVisible`）を追加 |
| 9 | Src/OptionsInit.cpp | 同キーを既定 `true` で初期化 |
| 10 | Src/resource.h | コマンド `ID_VIEW_MERGE20_FILELIST_PANEL` 38966、コントロール `IDC_MERGE20_FILELIST_LIST` 1810・`IDC_MERGE20_FILELIST_EMPTY` 1811 を確保。`_APS_NEXT_COMMAND_VALUE` 38966→38967、`_APS_NEXT_CONTROL_VALUE` 1810→1812。コメント欄に P0 の消費を記録 |
| 11 | Src/Merge.rc | 表示メニューに `MENUITEM "File List Panel", ID_VIEW_MERGE20_FILELIST_PANEL` を追加（専用モード項目の次） |
| 12 | Src/MergeEditFrm.h | `Merge20FileListBar.h` の include、`m_wndFileListBar` メンバ、`OnMerge20FileListRefresh` ハンドラ宣言を追加 |
| 13 | Src/MergeEditFrm.cpp | バーの生成（`OnCreateClient`、専用モードバーと同方式）・ドック（`OnCreate`、左）・表示判定・トグル（`OnBarCheck` でキー保存＋`Refresh()`）・幅の保存（`SavePosition` で `SaveState`）・通知登録（`OnCreateClient`）・通知ハンドラ・活性化時 `Refresh()`（`OnMDIActivate`）・言語変更時の `UpdateResources()` |
| 14 | Src/Merge.vcxproj | 新規 4 ファイルを登録（いずれも StdAfx.h グループの既定エントリ。MFC を使うため pch.h グループにはしない。S3／S4 の教訓） |
| 15 | Src/Merge.vcxproj.filters | 同上（Merge20 フィルタ） |
| 16 | Testing/GoogleTest/Merge20/Merge20FileListNotify_test.cpp | 新規。通知コールバックの単体テスト 5 件（未登録時の無通知・確認内容／分類／メモでの通知・登録の置換と解除） |
| 17 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | テストを登録（pch.h グループ） |
| 18 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | テストを登録（Tests フィルタ） |
| 19 | Translations/WinMerge/Japanese.po | 新規 2 件を追加（下記 §38.4 の監査参照） |
| 20 | Src/Merge20/Merge20FileListBar.cpp | CI 修正（C2672）：クランプ定数を `int` から `LONG` に変更（`CSize::cx` は `LONG`。下記 §38.4 参照） |

### 38.2 設計判断（判断理由付き）

- **`OPT_MERGE20_DOCK_HIDDEN` は流用しない**。コード実読で確認：`MergeEditFrm.cpp:413`（`OnBarCheck`）と `:923`（`SavePosition`）がともに `m_wndDetailBar`（Diff Pane／フォーカスドック）の表示状態を同キーに書き込む。パネルに流用すると 2 つのバーの表示状態が 1 つのキーで上書きし合う。専用キー `OPT_MERGE20_FILELIST_VISIBLE`（`Merge20/FileListVisible`、既定 `true`）を新設し、トグルの永続化は P0 で実施できた。幅はバーの `SaveState`／`LoadState`（`Settings` プロファイル、他バーと同じ方式）で永続化される。スリム化・検索文字列の保存は P4 の範囲（設計書 §12.7）。
- **通知は `std::function<void()>` コールバック経由**。`CDiffContext` は pch.h グループ（`DiffContext.cpp` は先頭 include が `pch.h`）で、`StdAfx.h`（`MSG_` 定数の置き場所）は読めず、`DiffContext.h` の include 閉包には `windows.h` がないため `HWND` をヘッダに持ち込めない。フレームが `PostMessage(hwndFrame, MSG_MERGE20_FILELIST_REFRESH, 0, 0)` するラムダを登録し、状態変更時に `CDiffContext::NotifyMerge20FileListChanged()` が呼ぶ。ポーリングはしない（設計書 §12.5 どおり）。
- **通知が届くのは最後に登録したフレームだけ**。`CDiffContext` は同じフォルダ比較から開いた全ファイル比較フレームで共有されるため、後から開いたフレームが登録を上書きする。他のフレームは `OnMDIActivate` での `Refresh()` で追いつく。登録解除はしない：破棄済み `HWND` への `PostMessage` は無害な no-op だが、閉じるフレームが解除すると後から登録したフレームの登録まで消してしまうため。複数フレームへの同報は P3 の課題とする。
- **バルク経路には通知を付けない**。`SetMerge20SessionState`・`ResetMerge20ChangedFile`・`SetMerge20DiffJudgments`・`SetMerge20SplitGroups` は復元・再計算の一括処理で、ファイルごとに通知すると 1000 件の `PostMessage` が飛ぶ。P0 のパネルはファイル名しか表示しないため実害はない。P1（行の表示値）・P3（連動）で一括処理後の通知を整備する。
- **`ApplyMerge20ReviewState` は 2 回通知する**。内部で呼ぶ `SetMerge20ReviewState(di)` が 1 回目、出力フラグの連動変更後に 2 回目（最終状態での通知）。P0 の表示には影響しないが、P1 以降で行の表示値が最終状態と一致することを保証する。
- **`SCBS_SIZECHILD` は付けない**。`sizecbar.cpp` の実装は「子は 1 つだけ」を前提に最初の子だけをバー全体に広げる（`ASSERT` 付き）。このバーは子が 2 つ（一覧＋空表示ラベル）のため、DedBar の `LayoutButtons()` と同じく `OnSize` で両方を手動レイアウトする。スタイルは左ドックの前例（`CLocationBar`）と同じ `SCBS_EDGELEFT | SCBS_EDGERIGHT`。
- **表示の既定：保存トグル ON かつフォルダ比較ありのときだけ表示する**。設計書 §12.7 (a)「空のパネルがドッキング表示される」と §12.4「空の状態でドッキングしないこと（パネルは一覧の中身がある場合だけ表示する）」の読み分け：(a) は P0 の骨格パネルが左にドックされ表示されること（フォルダ比較から開いた場合）であり、§12.4 は一覧の中身がない単独ファイル比較では自動表示しないこと。単独ファイル比較ではパネルは初期非表示だが、表示メニューのトグルで表示でき、その場合は「フォルダ比較がありません」の 1 行になる（§12.5 どおり）。
- **幅は 160〜600px にクランプ、初期幅 280px**。D1 の教訓（空のサイズ可変バーがクライアント領域を奪う）を初期幅の明示で、D2 の教訓（保存値・ドラッグによる肥大化）を `LoadState`／`CalcFixedLayout` のクランプで受ける。`m_szVert`／`m_szFloat` ともにクランプする。
- **P0 の一覧はファイル名 1 列・フラット**。全 `DIFFITEM` を比較順に列挙し、ディレクトリ行も含む（フォルダ比較の一覧と同じく行として存在するため）。階層の展開／折畳は P2、選択・対応区分・リスク・旧／変／新・確認内容の列と行色・件数表示は P1。表示値は S6 のラッパー（`Merge20CsvGetFileNameText`）経由でフォルダ比較の一覧と一致させる。
- **専用モードはパネルに触れない**。`EnterDedicatedMode`／`ExitDedicatedMode` が隠す・戻すのは 5 バー（ファイルパスバー・Location Pane・Diff Pane・Merge Result Pane・ステータスバー）のみで、パネルは対象外（設計書 §12.6 どおり）。専用モードに入ってもパネルの表示状態は維持される。スリム化（専用モード中の既定の折畳み表示）は P4 の範囲。
- **空表示メッセージに【Wiz】は付けない**。新規文字列 2 件のうち、表示名・ラベルである「File List Panel」→「【Wiz】ファイル一覧パネル」には付け、状態メッセージである「No folder comparison」→「フォルダ比較がありません」には付けない（D11 §37.6 の前例：中間メッセージ・不一致メッセージ・レポート行は意図的に【Wiz】なし）。列見出しは既存 msgid「Filename」（→「名前」）を流用し、新規文字列は増やさない。
- **新規 UI ファイルは `Merge.vcxproj` のみ登録**。バー／コントロールは MFC のため `UnitTests.vcxproj` には登録しない（`Merge20DedBar` と同じ。S1 の LNK2019 の教訓は、単体テストがリンクする純粋モジュールに限る）。テスト対象の通知コールバックは `CDiffContext`（`UnitTests` に既登録）に置いたため、テスト 5 件は `UnitTests` で実行できる。
- **クランプ定数の型は `CSize::cx`（`LONG`）に揃える**。初回 CI で error C2672 が 6 件出た：`(std::min)(...)`／`(std::max)(...)` と書いても（D7 の教訓）、引数の型が `LONG` と `int` で違うとテンプレート推論が曖昧になりコンパイルできない。定数を `LONG` にして修正した（コミット `fcc9f4c`）。

### 38.3 設計書 §12.7 P0 受け入れ条件の照合表

| 条件 | 判定 | 備考 |
|---|---|---|
| (a) ファイル比較フレームの左側に空のパネルがドッキング表示されること | 満たす（コード）＋未検証（実機） | `OnCreateClient` で生成、`OnCreate` で左ドック。フォルダ比較から開いた場合は保存トグル ON なら表示される。単独ファイル比較では初期非表示（§38.2 の読み分け参照）。実機確認は §38.5 項目 1・2 |
| (b) フォルダ比較から開いた場合は一覧が表示され、単独ファイル比較の場合は「フォルダ比較がありません」と表示されること | 満たす（コード）＋未検証（実機） | `Refresh()` が `m_pDirDoc` の有無で一覧／空表示を切り替える。一覧はファイル名 1 列（P0 の範囲）。実機確認は §38.5 項目 3・4 |
| (c) 表示／非表示の切替が正しく動くこと | 満たす（コード）＋未検証（実機） | 表示メニューの「【Wiz】ファイル一覧パネル」は `OnBarCheck` による標準のバー切替＋チェック表示（`OnUpdateControlBarMenu`）。トグルは専用キーに保存される。実機確認は §38.5 項目 5 |
| (d) 既存のファイル比較・フォルダ比較の回帰がないこと | 満たす（コード）＋未検証（実機） | 既存の比較経路への変更は、通知コールバックの追加（未登録時は no-op）とフレームへのバー追加のみ。`CDiffContext` の setter の挙動自体は不変。実機確認は §38.5 項目 7 |
| 設計書 §12.5：`m_pDirDoc` 経由の状態参照。`CDiffContext` の内部構造には直接触れないこと | 満たす（コード） | パネルは `CDirDoc::GetDiffContext()` 経由で公開 API（`DiffItemList` の走査、`Merge20CsvGetFileNameText`）だけを使う。サイドカーへの直接アクセスはない |
| 設計書 §12.5：更新の通知（`PostMessage` による再描画要求。ポーリングはしない） | 満たす（コード）＋未検証（実機） | コールバック＋`MSG_MERGE20_FILELIST_REFRESH`。単体テスト 5 件でコールバックの発火を確認。実機での通知→再描画の確認は §38.5 項目 6 |
| 設計書 §12.6：専用モードとは独立したトグル。専用モードに入ってもパネルの表示状態は維持する | 満たす（コード）＋未検証（実機） | 専用モードの出入りはパネルに触れない。実機確認は §38.5 項目 8 |
| 設計書 §12.4：初期サイズを明示し、空の状態でドッキングしないこと。D2 の教訓（固定値でクランプ） | 満たす（コード）＋未検証（実機） | 初期幅 280px、幅 160〜600px クランプ（`LoadState`／`CalcFixedLayout`）。実機確認は §38.5 項目 2 |

### 38.4 検証レポート

- CI 実行 45819589348（head `fcc9f4c`、PR #34）：結論 success。GoogleTest 699 テスト全パス（57 スイート、`[ FAILED ]` 行なし。新規 Merge20FileListNotify 5 件含む。D11 時点 694 件から＋5）。コンパイルエラー 0 件。成果物：WinMerge-x64.zip 26,426,245 バイト（ID 15117947614、失効 2027-02-24T12:26:00Z）、WinMerge-x64-Setup-x64.exe 39,872,578 バイト（ID 15118027455、失効 2027-02-24T12:26:00Z）。全文ログは `~/workspace/.coord/p0_final_log.txt` に保存（47,531 行）。ログ中の "FAILED" 4 件は翻訳文字列 ID（IDS_FILESAVE_FAILED 等）で、テスト失敗ではない。CI の番兵「Merge20 strings present in the bundled Japanese.po.」も確認。
- 初回実行 45817715295（head `03a41e5`）は failure：`Merge20FileListBar.cpp` の `LoadState`／`CalcFixedLayout` で error C2672 が 6 件（`CSize::cx` は `LONG`、クランプ定数は `int` でテンプレート推論が曖昧。§38.2 の教訓参照）。定数を `LONG` に変更して修正（コミット `fcc9f4c`）。修正後の実行 45819589348 が success。
- .po 監査：ブランチ版 msgid 2150 件 vs master 版 2148 件（＋2。新規 2 件）。【Wiz】118 件 vs master 版 117 件（＋1）。＋2 と ＋1 の差 1 件は空表示メッセージのため意図的に【Wiz】なし（§38.2 参照）。ブランチ版・master 版とも API から取得して確認。CI の番兵は msgid の存在しか見ないため、件数監査で msgstr の欠落がないことを確認済み。
- ローカル検証：全変更ファイルの括弧バランス、`resource.h` の値ソートによる ID 重複チェック（38966・1810・1811 に重複なし。既存の FIRST／LAST 別名の重複は従来どおり）、`WM_USER + 4` の未使用確認、`SCBS_SIZECHILD` の実装確認（`sizecbar.cpp`：子は 1 つだけ）、`dynamic_cast<CDirDoc*>` の前例確認（`Merge20SessionDoc.cpp:641`）、`OnBarCheck`／`OnUpdateControlBarMenu` の既存バーでの前例確認。`Merge20FileListCtrl`／`Bar` は MFC のためこの VM ではコンパイルできない（CI で確認）。
- 未検証（Windows 実機でのみ確認できる挙動）：§38.5 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 38.5 実機確認チェックリスト（手順付き）

1. **パネルの表示（フォルダ比較から開く）**：実フォルダを比較し、一覧からファイルを開く。ファイル比較ウィンドウの左側にパネルがドッキング表示され、ファイル名の一覧が出ること。パネルが Location Pane の左にあること（左端）。パネル幅はドラッグで変えられること。
2. **パネルの初期サイズとクランプ**：パネル幅を極端に狭く（または広く）ドラッグして終了→再起動→同じフォルダを開くと、幅が 160〜600px の範囲に戻っていること（極端な幅が復元されないこと）。パネルがファイル表示領域を奪って比較画面が見えなくならないこと。
3. **一覧の内容**：パネルの一覧がフォルダ比較の一覧と同じファイル名・同じ順序であること（ディレクトリ行も含む。P0 はファイル名 1 列のみ）。100 ファイル規模のフォルダで一覧表示が重くないこと。
4. **単独ファイル比較の空表示**：2 ファイルを直接開く（コマンドラインまたはドラッグ＆ドロップ）。パネルは初期非表示であること。表示メニューから「【Wiz】ファイル一覧パネル」を ON にすると、パネルに「フォルダ比較がありません」の 1 行だけが表示され、一覧・操作ができないこと。
5. **表示／非表示の切替と保存**：表示メニュー「【Wiz】ファイル一覧パネル」のチェックを外すとパネルが消え、入れると再表示されること。バーの ✕ ボタンでも閉じられること。閉じた状態で終了→再起動→同じフォルダを開くと、パネルが閉じたままであること。開いた状態でも同様に復元されること。
6. **通知による再描画**：フォルダ比較画面で確認内容を変更（キー 1〜5 など）したとき、開いているファイル比較のパネルが（表示が変わる項目がない P0 では見た目は変わらないが）エラーなく表示され続けること。複数ファイルを開いている場合、最後に開いたフレームのパネルが最新になること。
7. **回帰なし**：ファイル比較（2 ファイル）・フォルダ比較（2 フォルダ）が従来どおり動くこと。確認内容の変更・出力チェック・一括チェック・CSV 出力が従来どおり動くこと。専用モード・スナイパー・フォーカスドック・Grep が従来どおり動くこと。
8. **専用モードとの独立性**：パネルを表示したまま専用モードに入ると、パネルは表示されたままであること（5 バーだけが隠れること）。専用モードを出るとパネルの表示状態が入る前と同じであること。パネルを隠したまま専用モードに入って出ても、パネルは隠れたままであること。

### 38.6 P1 への引き継ぎ

- 列の追加（選択・対応区分・リスク・旧／変／新・確認内容）。表示値は S6 のラッパー（`Merge20CsvGet*Text`）を流用し、フォルダ比較の一覧と一致させること。旧／変／新の内訳 3 列は H-LIST の列を流用する。H-LIST がまだの場合は要約文の「欠落：N行 / 変更：M行 / 追加：K行」を 1 列にまとめる暫定形（設計書 §12.3 P-R1）。
- 行色（S2 と同じ規則：SPECIAL＝薄赤、HOLD＝薄黄、KEEP＝灰）。要判断タグ・リスク「高」の強調。件数表示（「表示 N 件中 M 件チェック」。S3 の `GetMerge20StatusCountText()` と同じ定義）。
- 通知の整備：一括処理（セッション復元・再分類）後の通知、複数フレームへの同報（§38.2 の制限の解消）。`NotifyMerge20FileListChanged()` の呼び出し箇所は `DiffContext.cpp` の setter に集約されている。
- P0 で残した制限：パネルからの操作は一切ない（行クリックでの比較切替・確認内容の変更は P3）。検索・階層展開／折畳・一括チェックは P2。表示状態・幅・スリム化・検索文字列の保存・復元の完成は P4（D11 と連携）。パネルのドック位置（Location Pane との左右関係）は実機で確認すること（§38.5 項目 1）。
- 実機確認待ち（§38.5 の全項目）。実機で問題が出た場合は本節へ結果を追記すること。

## 39. Panel P1 — 常設ファイル一覧パネル 一覧表示（設計書 §12.7 P1）

### 39.1 変更一覧（1 ファイル 1 コミット、16 件＋本節の記録コミット）

| # | ファイル | 内容 |
|---|---|---|
| 1 | Src/DirViewColItems.h | 新規ラッパー 2 件の宣言を追加（`Merge20CsvGetOutputText`、`Merge20CsvGetOldChangedNewText`） |
| 2 | Src/DirViewColItems.cpp | 同上 2 件の実装（出力グリフは `ColOutputCheckGet` へ転送。旧／変／新は要約文と同じ数値・同じ書式 msgid で 1 列にまとめる暫定形） |
| 3 | Src/DiffContext.h | 同報（`AddMerge20FileListNotifyCallback`、キー付き）と一括抑制（`Suspend/ResumeMerge20FileListNotify`）の宣言、メンバ追加（`<cstdint>`・`<utility>`・`<vector>` の include 追加） |
| 4 | Src/DiffContext.cpp | 同上の実装。`NotifyMerge20FileListChanged()` は抑制中は保留し、全登録先へ同報する |
| 5 | Src/Merge20/Merge20FileListCtrl.h | 6 列 enum・行キャッシュ struct（値で保持）・`SetupColumns()`・`UpdateColumnHeaders()`・`GetCountText()`・`OnCustomDraw` の宣言 |
| 6 | Src/Merge20/Merge20FileListCtrl.cpp | 6 列の表示・行キャッシュ・件数表示・カスタム描画（行色＋リスク「高」の赤文字）の実装。列ヘッダはリテラルの `_()` 呼び出し（.pot 生成のため） |
| 7 | Src/Merge20/Merge20FileListBar.h | 件数ラベル `m_stCount` のメンバ追加 |
| 8 | Src/Merge20/Merge20FileListBar.cpp | 件数行の生成・レイアウト・更新、既定幅 280px→480px（6 列分） |
| 9 | Src/resource.h | `IDC_MERGE20_FILELIST_COUNT` 1812 を確保。`_APS_NEXT_CONTROL_VALUE` 1812→1813 |
| 10 | Src/MergeEditFrm.cpp | 通知登録を `Set`（最後の 1 フレームのみ）から `Add`（全フレームへ同報）に変更 |
| 11 | Src/Merge20/Merge20SessionDoc.cpp | `ApplySessionStates` の末尾で通知を 1 回（セッション復元後のパネル更新） |
| 12 | Src/Merge20/Merge20BulkClassifyDlg.cpp | `DrainPendingResults` のループを抑制／再開で囲む（1 ドレイン＝通知 1 回）。`DiffContext.h` の include 追加 |
| 13 | Testing/GoogleTest/Merge20/Merge20FileListNotify_test.cpp | 単体テスト 6 件を追加（同報・同キー置換・Set＋Add 併用・抑制／再開・ネスト・抑制なし Resume） |
| 14 | Translations/WinMerge/Japanese.po | 新規 1 件を追加（下記 §39.4 の監査参照） |
| 15 | Src/Merge20/Merge20FileListCtrl.cpp | CI 対策の事前修正：列ヘッダをテーブル経由の変数 `_()` からリテラルの `_()` 呼び出しに変更（.pot 生成がリテラルしか拾わないため。§39.2 参照） |
| 16 | Src/Merge20/Merge20FileListCtrl.cpp | コメント整理（`SetupColumns` 内の重複コメントを削除） |

### 39.2 設計判断（判断理由付き）

- **表示値はすべて S6 ラッパー経由**。選択列は `Merge20CsvGetOutputText`（新規。`ColOutputCheckGet` へ転送し、フォルダ比較の一覧と同じ ☑／☐ グリフを表示）、ファイル名・対応区分・リスク・確認内容は既存ラッパー、旧／変／新は `Merge20CsvGetOldChangedNewText`（新規）。これにより (a)「パネルの列の表示値がフォルダ比較の一覧と一致すること」は、列ごとに同じ getter を呼ぶ構造で保証される（CSV と同じ S6 の方式）。
- **旧／変／新は暫定形で 1 列**。H-LIST（カバレッジ行列 §4）は未実装のため、設計書 §12.3 P-R1 の暫定形どおり、要約文の中の「欠落：N行 / 変更：M行 / 追加：K行」を 1 列にまとめて表示する。実装は要約文そのものではなく、同じ 3 フィールド（`nMissing`／`nModified`／`nAdded`）と同じ書式 msgid（`"Missing: %1 lines / Changed: %2 lines / Added: %3 lines"`、数値は同じ `locality::NumToLocaleStr`）で組み立てるため、表示値はフォルダ比較の一覧の要約列の先頭部分と一致する。「Excluded: N行」や insight（丸め・SQL 等）は含めない（旧／変／新の 3 数だけがこの列の内容）。ディレクトリ行は空、未分類（未開封）ファイルは他の S4 列と同じく「Not calculated」。H-LIST で 3 列に分ける際はこのラッパーを 3 列分の getter に置き換える（ヘッダ msgid `"Old/Changed/New"` とともに廃止する）。
- **選択列はグリフ表示で、操作はしない**。フォルダ比較の一覧と同じく、出力チェックは ☑／☐ の文字グリフで表示する（S2 の方式 (b)：仮想リストのサブアイテム列には状態イメージでチェックボックスを描けない）。LVS_EX_CHECKBOXES の本物のチェックボックスは付けない。理由：P1 は一覧表示が範囲で、パネルからの出力 ON／OFF 切替は P3（P-R8）の範囲であり、操作できないチェックボックスを付けるのは誤解を招くため。P3 でトグル操作を付けるときに再検討する。
- **列ヘッダはフォルダ比較の一覧と同じ msgid を流用し、は付けない（付け足さない）**。5 列（`"Include in Output"`・`"Filename"`・`"Action Category"`・`"Risk"`・`"Review State"`）は既存 msgid の流用で、W1 が既に msgstr にを付けている（出力・対応区分・リスク・確認内容、「名前」はなし）。流用 msgid に新たにを付けることはしていないため、フォルダ比較の一覧の列ヘッダへの影響はない。唯一の新規 msgid `"Old/Changed/New"`（→「旧／変／新」）はパネル専用で、フォルダ比較の一覧には影響しない。なお、列ヘッダを最初はテーブル経由の変数 `_()` で書いたが、`Translations/WinMerge/CreateMasterPotFile.js`（W1 のコメントと正規表現を確認）がリテラルの `_()/N_()/NC_()` 引数しか拾わないため、リテラルの `_()` 呼び出しに書き換えた（コミット 15）。変数渡しのままでは次の.pot 再生成でこの msgid が落ち、MergeLang の書き戻しで.po からも消えるおそれがあった。CI の番兵（`BuildTranslations.cmd`）は固定 6 msgid しか見ないため、この問題は番兵では検出できない（件数監査とコードレビューで担保する）。
- **行色はフォルダ比較と同一の規則・同一の RGB 値**。`CDirView::GetColors` の S2 部分とまったく同じ switch（SPECIAL＝RGB(255,200,200)、HOLD＝RGB(255,255,180)、KEEP＝RGB(230,230,230)、TODO／DONE は既定背景）。パネルにはフォルダ比較の差分状態による基本色がないため、行色はこの tint だけを `clrTextBk` に設定する。選択行はフォルダ比較と同じく標準の選択ハイライトを維持する（D9 の前例と同じ扱い）。
- **要判断タグ・未確認は確認内容列の表示値そのもの**。フォルダ比較の一覧に「タグ」の別 UI はなく、SPECIAL の表示値「Needs manual work」（→「個別対応要」）そのものが要判断タグであり、TODO の表示値「Not reviewed」（→「未確認」）そのものが未確認の表示である。パネルは同じラッパー（`Merge20CsvGetReviewText`）を使うため、表示は一致する。別途タグを描画する処理は追加していない。
- **リスク「高」の強調はパネルだけの追加**。設計書 §12.3 P-R2 は「リスク『高』の強調」を求めるが、フォルダ比較の一覧にはリスク「高」の強調が存在しないことをコード実読で確認した（`DirView.cpp` に `FW_BOLD`／太字処理なし。`RiskLevel::HIGH` の出現は `ColRiskGet` の getter のみ。`OnCustomDraw` はスリム時の確認内容タグの折り返し描画と `GetColors` のみ）。そのためパネルでは Risk セルだけを赤文字（RGB(192,0,0)）にする強調を付けた。これは受け入れ条件 (b)「強調がフォルダ比較と同じであること」との食い違いに見えるが、(b) の「同じ」は行色・タグの一致を指し、P-R2 の強調要求をパネル側で満たすための意図的な追加である。フォルダ比較の一覧側に同じ強調を付けるか、パネルの強調をやめるかは、実機確認時のユーザーの判断に委ねる（1 行の変更で外せる）。
- **件数表示は S3 と同じ定義・同じ msgid**。`CMerge20FileListCtrl::GetCountText()` は `CDirView::GetMerge20StatusCountText()` と同じ数え方（ディレクトリ行を除く表示ファイル数、出力チェック数、未確認（TODO）数）と同じ msgid（`"%1 displayed, %2 checked, %3 unreviewed"` → 「表示 %1 件中 %2 件チェック、未確認 %3 件」）を使う。パネルのヘッダ（件数行）に表示する。言語変更時は `UpdateResources()` から件数行の文言も再適用する（次の `Refresh()` を待たない）。
- **行は値でキャッシュし、DIFFITEM ポインタは保持しない**。`Refresh()` で表示文字列 6 件＋確認状態・リスク・ディレクトリ判定・出力フラグを 1 行分の struct にまとめ、`LVN_GETDISPINFO` はその文字列へのポインタを渡すだけ。再スキャンで DIFFITEM が作り直されるとポインタは宙ぶらりんになるが、キャッシュした文字列は次の `Refresh()` まで古いままなだけで安全である。表示値の取得は `CDirDoc::GetDiffContext()` 経由の公開 API のみ（設計書 §12.5 どおり。サイドカーへの直接アクセスはない）。
- **既定幅を 280px→480px に広げた**。P0 の 280px はファイル名 1 列分の幅であり、6 列では先頭 2 列しか見えない。クランプ範囲（160〜600px）は変えていない。保存済みの幅（P0 利用者の 280px）は `LoadState` で復元され、クランプされるだけで変わらないため、既存利用者の表示は維持される。
- **通知の整備（P1 でできる範囲）**。(1) 同報：`AddMerge20FileListNotifyCallback`（HWND を `std::uintptr_t` キーとする。`DiffContext.h` は MFC フリーのため HWND を名前にできない。P0 の方式と同じ）。同じフォルダ比較から開いた全ファイル比較フレームが登録し、全パネルが更新される。P0 の `Set`（最後の 1 フレームのみ）は残し、テストの置換セマンティクスも維持する。閉じたフレームの登録は残るが、コールバックは破棄済み HWND への `PostMessage`（無害な no-op）のみのため解除しない（P0 の判断と同じ）。(2) セッション復元後：`ApplySessionStates` の末尾で通知を 1 回。`SetMerge20SessionState`／`ResetMerge20ChangedFile` は通知しないため、これがないと復元後のパネルは次の活性化まで古いままだった。(3) 一括分類：`DrainPendingResults` のループを抑制／再開で囲み、1 ドレイン＝通知 1 回にした（各 `SetMerge20Classification` は通知するが、抑制中は保留され、再開時に 1 回だけ通知される。ネストにも対応）。`SetMerge20DiffJudgments`／`SetMerge20SplitGroups` には通知を付けない（P1 の表示値に影響しないため）。再スキャン後のパネル更新は未整備で、P3 へ回す（§39.6 参照）。
- **新規 UI ファイルの登録は不要**。新規 `.cpp`／`.h` はなく、既存ファイルの変更のみのため、`Merge.vcxproj`／`UnitTests.vcxproj` への登録は発生しない。

### 39.3 設計書 §12.7 P1 受け入れ条件の照合表

| 条件 | 判定 | 備考 |
|---|---|---|
| (a) パネルの列の表示値がフォルダ比較の一覧と一致すること | 満たす（コード）＋未検証（実機） | 6 列すべて S6 ラッパー経由で同じ getter を呼ぶ。旧／変／新は暫定形で、要約列の先頭部分と一致する（§39.2 参照）。実機確認は §39.5 項目 2・3 |
| (b) 行色・タグ・強調がフォルダ比較と同じであること | 満たす（コード。リスク「高」の強調だけ意図的な追加あり）＋未検証（実機） | 行色は同一の RGB 値・同一の switch。タグ・未確認は確認内容列の表示値そのもの。リスク「高」の赤文字強調だけフォルダ比較の一覧にはない追加（§39.2 参照）。実機確認は §39.5 項目 4・5 |
| (c) 件数表示が正しいこと | 満たす（コード）＋未検証（実機） | S3 と同じ定義・同じ msgid。ディレクトリ行は数えない。実機確認は §39.5 項目 6 |
| (d) 100ファイル規模での描画が重くないこと（仮想リストの効果を確認する） | 満たす（コード）＋未検証（実機） | 仮想リスト（LVS_OWNERDATA）のまま。描画は可視行のみ。`Refresh()` は行ごとに 1 struct を作るだけ（1000 行でも表示文字列の組み立て 6 件／行）。一括分類の通知は 1 ドレイン＝1 回に抑制した。実機確認は §39.5 項目 7 |
| 設計書 §12.3 P-R1：列（選択・ファイル名・対応区分・リスク・旧／変／新・確認内容）。フォルダ比較の一覧と同じ列定義を使い、表示値は一覧表示の値と一致させること | 満たす（コード。旧／変／新は暫定形） | 旧／変／新の内訳 3 列は H-LIST 未実装のため 1 列の暫定形（§39.2 参照）。H-LIST の実装は P1 の範囲外 |
| 設計書 §12.3 P-R2：行の表示（チェックボックス・要判断タグ・リスク「高」の強調・未確認の表示。行色は S2 と同じ規則） | 満たす（コード） | 選択列はグリフ表示（操作なしは P3）。要判断タグ・未確認は確認内容列の表示値。リスク「高」は赤文字で強調（§39.2 参照） |
| 設計書 §12.3 P-R6：件数表示（「表示 N 件中 M 件チェック」。S3 と同じ定義）。パネルのヘッダに表示する | 満たす（コード） | S3 と同じ定義・同じ msgid。件数行としてパネル上部に表示（未確認数も含む。S3 の定義どおり） |
| 設計書 §12.5：更新の通知（`PostMessage` による再描画要求。ポーリングはしない） | 満たす（コード）＋未検証（実機） | 同報・セッション復元後の通知・一括分類の抑制を整備。単体テスト 6 件で同報と抑制の挙動を確認。実機確認は §39.5 項目 8・9 |
| 設計書 §12.5：`m_pDirDoc` 経由の状態参照。`CDiffContext` の内部構造には直接触れないこと | 満たす（コード） | パネルは `CDirDoc::GetDiffContext()` 経由の公開 API（S6 ラッパー、`GetMerge20ReviewState`、`GetMerge20IncludeInOutput`、`GetMerge20FileStates().GetState()`）だけを使う。サイドカーへの直接アクセスはない |

### 39.4 検証レポート

- CI 実行 45826654158（head `dc167d34`、PR #35）：結論 success。GoogleTest 705 テスト全パス（57 スイート、`[ FAILED ]` 行なし。新規 Merge20FileListNotify 6 件含む。P0 時点 699 件から＋6）。コンパイルエラー 0 件（`error C` 0 件、`: error` 0 件、`Build succeeded` 1 件）。成果物：WinMerge-x64.zip 26,430,613 バイト（ID 15121660472、失効 2027-02-24T13:11:00Z）、WinMerge-x64-Setup-x64.exe 39,883,651 バイト（ID 15120898336、失効 2027-02-24T13:11:00Z）。全文ログは `~/workspace/.coord/p1_final_log.txt` に保存（47,515 行）。ログ中の "FAILED" 4 件は翻訳文字列 ID（IDS_FILESAVE_FAILED 等）で、テスト失敗ではない。CI の番兵「Merge20 strings present in the bundled Japanese.po.」も確認。
-.po 監査：ブランチ版 msgid 2151 件 vs master 版 2150 件（＋1。新規 1 件）。119 件 vs master 版 118 件（＋1）。＋1 と ＋1 が一致し、新規文字列 1 件（`"Old/Changed/New"` → 「旧／変／新」）と一致することを確認。ブランチ版・master 版とも API から取得して確認。CI の番兵は固定 6 msgid しか見ないため、件数監査で msgstr の欠落がないことを確認済み。
- ローカル検証：全変更ファイルの括弧バランス、`resource.h` の値ソートによる ID 重複チェック（1812 に重複なし。`_APS_NEXT_CONTROL_VALUE` 1812→1813）、`CreateMasterPotFile.js` の正規表現によるリテラル `_()` の確認（列ヘッダ 6 件すべてリテラル呼び出し）、S6 ラッパーの転送先確認（`ColOutputCheckGet`／要約文と同じ数値・書式 msgid）、`GetColors` の S2 部分との行色の一致確認（RGB 値を 1 件ずつ照合）、`strutils::format_string3`／`locality::NumToLocaleStr` の既存使用の確認（`DirViewColItems.cpp` で使用済み）、`std::uintptr_t` の到達性確認（`DiffContext.h` の `<cstdint>` 経由）。`Merge20FileListCtrl`／`Bar` は MFC のためこの VM ではコンパイルできない（CI で確認）。
- 未検証（Windows 実機でのみ確認できる挙動）：§39.5 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 39.5 実機確認チェックリスト（手順付き）

1. **パネルの表示（フォルダ比較から開く）**：実フォルダを比較し、一覧からファイルを開く。ファイル比較ウィンドウの左側にパネルが表示され、上部に件数行、6 列の一覧が出ること。パネル幅の既定は 480px 程度であること。
2. **列の表示値の一致**：パネルの各行について、選択（☑／☐）・ファイル名・対応区分・リスク・確認内容が、フォルダ比較の一覧の同じ行の表示値と一致すること。未開封のファイルは対応区分・リスクが「未計算」、旧／変／新が「未計算」であること。ディレクトリ行はファイル名以外が空であること（確認内容列はフォルダ比較の一覧と同じ表示になること）。
3. **旧／変／新の暫定列**：分類済み（ファイルを開いたことがある）のファイルで、旧／変／新列が「欠落：N行 / 変更：M行 / 追加：K行」の形で表示され、N・M・K がフォルダ比較の一覧の要約列の先頭部分の数値と一致すること。3 列に分かれていないこと（暫定形であること）を了解のうえ確認すること。
4. **行色・タグ**：確認内容を「個別対応要」（SPECIAL）にした行が薄赤、「保留」（HOLD）にした行が薄黄、「現状維持」（KEEP）にした行が灰色になること。フォルダ比較の一覧の同じ行と同じ色であること。「個別対応要」の行の確認内容列に「個別対応要」と表示されること。未確認の行の確認内容列に「未確認」と表示されること。
5. **リスク「高」の強調**：リスクが「高」の行で、リスク列の文字が赤く表示されること（パネルだけの追加。フォルダ比較の一覧では赤くならないこと）。この強調が不要・不自然に見える場合は、実機確認時に判断すること（外すのは 1 行の変更）。
6. **件数表示**：件数行が「表示 N 件中 M 件チェック、未確認 K 件」と表示され、N・M・K がフォルダ比較のステータスバーの件数表示と一致すること。出力チェックを付け外しすると M が変わること。確認内容を「済」にすると K が減ること。ディレクトリ行は数えられていないこと。
7. **100ファイル規模の描画**：100 ファイル程度のフォルダでファイル比較を開き、パネルの一覧表示・スクロールが重くないこと。パネルを開いたままフォルダ比較側で一括分類を実行し、パネルの更新が 1 回で済んでいること（ちらつき・固まりがないこと）。
8. **通知による再描画（セッション復元）**：ファイル比較を開いた状態で、セッションを復元する（確認内容・出力・分類が保存されたセッション）。開いているファイル比較のパネルが、復元後の確認内容・出力・分類で更新されること（パネルを閉じ開きし直さなくてもよいこと）。
9. **通知による再描画（複数フレーム同報）**：同じフォルダ比較から 2 つのファイル比較を開く。フォルダ比較画面で片方のファイルの確認内容を変更したとき、両方のファイル比較のパネルが更新されること（P0 では最後に開いたフレームのパネルだけが更新された）。
10. **回帰なし**：ファイル比較（2 ファイル）・フォルダ比較（2 フォルダ）が従来どおり動くこと。確認内容の変更・出力チェック・一括チェック・CSV 出力・セッション保存／復元・一括分類が従来どおり動くこと。専用モード・スナイパー・フォーカスドック・Grep が従来どおり動くこと。

### 39.6 P2 への引き継ぎ

- P1 の範囲外（P2 以降）：パネルからの操作（行クリックでの比較切替・確認内容の変更・出力チェックのトグル）は P3。検索・階層展開／折畳・一括チェック・フィルタタブは P2。表示状態・幅・スリム化・検索文字列の保存・復元の完成は P4（D11 と連携）。
- 旧／変／新の内訳 3 列（H-LIST）は未実装。P1 は 1 列の暫定形。H-LIST の着手時に `Merge20CsvGetOldChangedNewText` を 3 列分の getter に置き換え、ヘッダ msgid `"Old/Changed/New"` を廃止する（.po の訳もあわせて整理する）。
- 再スキャン後のパネル更新は未整備。フォルダ比較で再スキャンすると DIFFITEM が作り直され、開いているファイル比較のパネルは次の活性化（`OnMDIActivate`）または次の状態変更通知まで古い一覧のままになる。パネルは DIFFITEM ポインタを保持しないためクラッシュはしないが、表示が古くなる。P3 で再スキャン完了時の通知（またはパネル側の再取得）を検討する。
- 閉じたフレームの通知登録は残る（`Add` のみで解除なし）。登録は破棄済み HWND への `PostMessage`（無害な no-op）のみのため実害はないが、同じフォルダ比較から大量のファイル比較を開閉し続けると登録が増える。気になる場合は P3 で解除 API（`RemoveMerge20FileListNotifyCallback`）を追加する。
- リスク「高」の赤文字強調はパネルだけの追加（§39.2 参照）。フォルダ比較の一覧側に同じ強調を付けるか、パネルの強調をやめるかは、実機確認時のユーザーの判断に委ねる。
- 実機確認待ち（§39.5 の全項目）。実機で問題が出た場合は本節へ結果を追記すること。

## 40. Panel P2 — 常設ファイル一覧パネル 操作（設計書 §12.7 P2）

### 40.1 変更一覧（1 ファイル 1 コミット、16 件＋本節の記録コミット）

| # | ファイル | 内容 | コミット |
|---|---|---|---|
| 1 | Src/Merge20/Merge20FileListFilter.h | 新規。純粋モジュール。可視性判定の入出力と関数宣言 | 5b98e801 |
| 2 | Src/Merge20/Merge20FileListFilter.cpp | 新規。同上の実装（検索・区分タブ・折りたたみの可視性判定） | 679e54e5 |
| 3 | Testing/GoogleTest/Merge20/Merge20FileListFilter_test.cpp | 新規。10 テスト | 34def736 |
| 4 | Src/Merge20/Merge20FileListCtrl.h | Row の拡張（relPath／depth／actionCategory／classified／hasChildren）、検索・タブ・折りたたみの API、CountByReviewState、右クリック／ダブルクリックの宣言。Row を public 化（バーからの読み取り用） | 7684ca7d |
| 5 | Src/Merge20/Merge20FileListCtrl.cpp | Refresh() の書き換え（全行列挙＋可視性判定）、▾／▸ 前置き、CountByReviewState、右クリック／ダブルクリックの実装 | bbc1dfe0 |
| 6 | Src/Merge20/Merge20FileListBar.h | 子コントロール（検索ラベル／エディット／フィルタタブ／ボタン×3）と選択タブのメンバ、ハンドラ宣言 | 388bc98f |
| 7 | Src/Merge20/Merge20FileListBar.cpp | 子コントロールの生成・レイアウト、検索・タブ・ボタン・右クリックメニュー・ディレクトリ単位操作・一括チェックの実装 | b1a39f17 |
| 8 | Src/resource.h | 新規 IDC 5 件（1813〜1817）。_APS_NEXT_CONTROL_VALUE 1813→1818 | 19b6ad27 |
| 9 | Translations/WinMerge/Japanese.po | 新規 6 件（§40.4 の監査参照） | ce6f3bbb |
| 10 | Src/Merge.vcxproj | Merge20FileListFilter.cpp/.h の登録（pch.h グループ） | 44f782b3 |
| 11 | Src/Merge.vcxproj.filters | 同上 | 72593b54 |
| 12 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | 同上＋テストファイルの登録 | 6a9c8785 |
| 13 | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同上 | d0021594 |
| 14 | Src/Merge20/Merge20FileListCtrl.h | CI 対策：struct Row をメンバー宣言より前へ移動（C4430。ネスト型は定義後にしか見えないため） | cd9239e0 |
| 15 | Src/Merge20/Merge20FileListBar.h | CI 対策：DIFFITEM の前方宣言を追加（C4430） | a137693e |
| 16 | Src/Merge20/Merge20FileListBar.cpp | CI 対策：検索ラベルの Create から IDC_STATIC 引数を外し C4245 警告を解消（既定 ID を使用） | 1c23b91f |
| 17 | MERGE20_NOTES.md §40 | 本節の記録 | （本コミット） |

PR #36（Panel P2: file-list panel operations）。コード head a137693e（CI 検証済み head）。最終 head は本節の記録コミット（コードの変更はなく、CI 実行は親への報告を参照）。

### 40.2 設計判断（判断理由付き）

- **階層の展開／折りたたみはフラット＋パネル側の折りたたみ集合**。ツリーモードへの切替はしない。理由：(1) パネルは仮想リスト（フラットな CListCtrl）であり、CTreeCtrl 化は P2 の規模を超える。(2) フォルダ比較のツリーモードは CDirView::m_listViewItems＋DIFFITEM::customFlags の EXPANDED フラグ＋Redisplay() 再構築で動き、パネルは CDirView のリストを所有しないため流用できない。EXPANDED フラグをパネルから書き換えるとフォルダ比較側の表示状態まで変わってしまう。(3) Refresh() はすでに深さ優先のフラット走査で全行を列挙しており、相対パスをキーにした折りたたみ集合で子孫行を除外するだけで実現できる。既定はすべて展開（P1 の見え方を維持）。(4) HTML の grpMode（階層別／フラット切替）は、H-30 が本体ツリーモードで等価・現状維持としているため、パネル側に切替ボタンを新設しない。
- **検索は HTML と同じ定義**（前後空白除去・大文字小文字を区別しない部分一致・対象は相対パス全体）。検索中は折りたたみを無視する（HTML の renderTree がフィルタ後のファイルからグループを再導出するのと同じ見え方）。検索文字列の保存・復元は P4 に回す（P2 では UI と絞り込み動作まで）。
- **検索文字列の正規化（trim・小文字化）は純粋モジュール内で行う**。計画書 §2.9 は呼び出し側で小文字化済みの文字列を渡す想定だったが、モジュール内に寄せた。理由：trim・大文字小文字無視が判定ロジックの一部であり、単体テスト 5 件（部分一致・大文字小文字無視・フォルダ一致・前後空白除去・折りたたみ無視）でまとめて検証できるため。呼び出し側（Ctrl）は生の文字列を渡すだけになる。
- **一括チェックは S3 の Merge20BatchCheckDlg をそのまま流用**（新規ダイアログなし）。件数はパネルの表示分で数える（CountByReviewState を新設。CDirView::Merge20CountDisplayedByReviewState はフォルダ一覧の表示行を数えるためパネルからは使えない）。適用は relPath→DIFFITEM の対応表を作ってから行い、通知の一括抑制（Suspend／ResumeMerge20FileListNotify）で囲む。ClearAll は S3 と同じく比較全体が対象。
- **ディレクトリ単位の操作は既存コマンド ID（38932／38933）と既存 msgid を流用**し、ハンドラだけをバー側に新設する。走査は S3 と同じ DIFFITEM 木走査（折りたたまれたサブフォルダ配下を含む）。メニューは TPM_RETURNCMD で実行し、選択結果をバー内で switch する方式にしたため、ON_COMMAND の追加は不要。折りたたみ／展開のメニュー項目は新規コマンド ID を作らず、すべて展開／すべて折りたたむボタンのコントロール ID（1815／1816）をメニュー ID として流用する（TPM_RETURNCMD の戻り値で判定するため、ボタンの BN_CLICKED とは干渉しない）。
- **右クリックはコントロールからバーへのメッセージで通知する**。WM_MERGE20_FILELIST_ROW_RCLICK（WM_USER + 120、wParam＝表示行インデックス）。Merge20 のパネル関連コードに WM_USER の使用がないことを確認済み。バーはカーソル位置を自ら取得する。
- **Row を public 化した**。バーが表示行の relPath・確認内容・ディレクトリ判定を読む必要があるため（バッチチェックとディレクトリ単位操作）。バーは Row を読むだけで書かない。
- **フィルタタブは対応区分のみ・パネル内のみ**。タブのラベルは S4 の列表示文字列を流用し、「すべて」だけ新規 msgid（"All action categories"）。流用する 7 種の msgstr には【Wiz】が付いていない（W1 は列ヘッダにだけ付け、値文字列には付けていない）。タブ行の表示は「すべて」タブ（【Wiz】付き）と検索ラベル・ボタン類で担う。7 種すべてに【Wiz】付きの新規 msgid を作る代替案は、列表示値とタブ表示値の二重管理になるため採用しない。
- **D11 のレビューフィルタ UI と重複しない**。D11 の「確認内容で表示」サブメニューはフォルダ比較の一覧を確認内容で絞り込む UI であり、P2 のフィルタタブはパネルを対応区分で絞り込む UI である。適用先も次元も異なるため両方残す。パネル側に確認内容フィルタは新設しない。
- **H-FILTER との整理**：P-R10 の方向性（フィルタデッキをパネル側に寄せ、フォルダ比較画面のデッキは残さない）の判断は H-FILTER の着手時に行う。P2 ではフォルダ比較画面側の扱いを変更しない。H-FILTER 実装時に、パネルのタブとデッキの同期（またはデッキのパネル側への寄せ）を決める必要がある。P2 のタブ状態はパネル内のメンバ変数に閉じており、同期が必要になった場合は公開 API を追加する。
- **「該当なし」の専用表示は追加しない**。検索・フィルタの結果 0 件のときは件数行の「表示 0 件」で代用する（新規文字列と子要素を増やさないため）。
- **既知の制限**：パネルから出力フラグを変えても、フォルダ比較画面のステータスバーの件数表示はフォルダ側の次回更新まで古いままになる（パネルから CDirView を呼ぶ経路がないため）。P3 でフォルダ側への通知（または活性化時更新）を検討する。
- **レイアウト**：上から検索行（26px：ラベル 160px＋エディット）、フィルタタブ行（26px）、ボタン行（28px：一括チェック 110px／すべて展開 90px／すべて折りたたむ 110px）、件数行（22px、既存）、リスト（残り）。幅のクランプ（160〜600px）は変えない。単独ファイル比較では検索・タブ・ボタンも無効化する（P0／P1 の空表示の分岐を拡張）。
- **DIFFITEM::GetDepth() の基数を実測で確認**：ルート直下が 0（DiffItem.cpp の実装どおり）。折りたたみ判定はこの基数で動く。

### 40.3 設計書 §12.7 P2 受け入れ条件の照合表

| 条件 | 判定 | 備考 |
|---|---|---|
| (a) 検索で絞り込みができること（H-29） | 満たす（コード）＋未検証（実機） | HTML と同じ定義。単体テスト 5 件。実機確認は §40.5 項目 1・2 |
| (b) 展開／折りたたみが正しく動くこと | 満たす（コード）＋未検証（実機） | フラット＋折りたたみ集合。単体テスト 2 件。実機確認は §40.5 項目 3・4 |
| (c) 一括チェックが S3 のダイアログで開き、正しく動くこと | 満たす（コード）＋未検証（実機） | ダイアログは S3 の実績あり。実機確認は §40.5 項目 5・6 |
| (d) フィルタタブが正しく動くこと | 満たす（コード）＋未検証（実機） | 単体テスト 3 件。実機確認は §40.5 項目 7・8 |
| P-R3：一括チェック（S3 ダイアログ流用） | 満たす（コード） | ダイアログの新規作成なし。ボタンからの起動のみ新規 |
| P-R4：検索（ファイル名・フォルダ） | 満たす（コード。保存は P4） | 検索文字列の保存・復元は P4 の範囲 |
| P-R5：階層の展開／折りたたみ＋ディレクトリ単位操作 | 満たす（コード。方式は §40.2 の判断） | ツリーモードへの切替はしない |
| P-R10：フィルタタブ＋H-FILTER との整理 | 満たす（コード。フォルダ側は H-FILTER へ） | フォルダ比較画面側の変更はしない |
| D11 レビューフィルタ UI との関係 | 重複なし（確認済み） | 適用先・次元が異なる。パネル側に確認内容フィルタは新設しない |

### 40.4 検証レポート

- CI 実行 45838875376（head a137693e、PR #36）：結論 success。GoogleTest 715 テスト全パス（58 スイート、`[ FAILED ]` 行なし。新規 Merge20FileListFilter 10 件含む。P1 時点 705 件から＋10）。コンパイルエラー 0 件。成果物：WinMerge-x64.zip 26,439,319 バイト（ID 15128620145、失効 2027-02-24T14:25:32Z）、WinMerge-x64-Setup-x64.exe 39,886,783 バイト（ID 15128665198、失効 2027-02-24T14:25:32Z）。全文ログは `~/workspace/.coord/p2_final_log.txt` に保存（47,571 行）。ログ中の "FAILED" 4 件は翻訳文字列 ID（IDS_FILESAVE_FAILED 等）で、テスト失敗ではない。CI の番兵「Merge20 strings present in the bundled Japanese.po.」も確認。新規 Merge20FileListFilter 10 テストすべて `[ OK ]`。警告は新規ファイルに 1 件だけあった（C4245、検索ラベルの IDC_STATIC 変換。1c23b91f で解消。ログ全体の warning C は 852 件で、大半は既存コード由来）。
- CI の失敗と修正（2 回）：1 回目 45835518981（head d0021594）は `Merge20FileListCtrl.h` の C4430（`GetDisplayedRow` の戻り値 `const Row*` が `struct Row` の定義より前にあり、ネスト型が見えなかった。誤解析で `Row` が int メンバーと解釈され、後続の `struct Row` 定義・`m_allRows`／`m_rows`・Bar.cpp の `Row*` 参照まで連鎖した。cd9239e0 で struct Row を public 先頭へ移動して解消）。2 回目 45837249482（head cd9239e0）は `Merge20FileListBar.h` の C4430（`SetOutputForDirectory` の `DIFFITEM` が Bar.h で未宣言。a137693e で `class DIFFITEM;` の前方宣言を追加して解消）。3 回目 45838875376（head a137693e）で success。C4245 修正（1c23b91f）はこの success の後のコミットであり、本節の記録コミットとあわせた最終 head の CI 実行も別途確認済み（親への報告を参照）。
- .po 監査：ブランチ版 msgid 2157 件 vs master 版 2151 件（＋6。新規 6 件）。【Wiz】125 件 vs master 版 119 件（＋6）。＋6 と ＋6 が一致し、新規文字列 6 件と一致することを確認。ブランチ版・master 版とも API から取得して確認。
- ローカル検証：全変更ファイルの括弧バランス（文字列・コメント除去後に 0）、`resource.h` の値ソートによる ID 重複チェック（1813〜1817 に重複なし。`_APS_NEXT_CONTROL_VALUE` 1813→1818。なお master 版に既存の IDC 値重複 1401〜1403／1430〜1431／1434〜1436 は別ダイアログ間の再利用で、P2 とは無関係）、新規 msgid 6 件のリテラル `_()` 呼び出しの確認（.pot 生成のため。作成時と UpdateResources() の両方にリテラルあり）、`Merge20FileListFilter` の MFC 非依存・ASCII の確認（UnicodeString.h／Merge20Types.h のみ）、vcxproj 4 件の XML パース、`strutils::makelower`／`trim_ws`（UnicodeString.h）の到達性確認、`DIFFITEM::GetDepth()`（ルート直下＝0）・`HasChildren()` の実測確認、`CDirDoc::GetDiffContext()`（非 const 版あり）の確認。`Merge20FileListCtrl`／`Bar` は MFC のためこの VM ではコンパイルできない（CI で確認）。
- 未検証（Windows 実機でのみ確認できる挙動）：§40.5 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 40.5 実機確認チェックリスト（手順付き）

1. **検索欄の表示**：フォルダ比較からファイル比較を開く。パネル上部に検索ラベル＋入力欄が表示されること。単独ファイル比較では検索欄が無効化されていること。
2. **検索の絞り込み**：ファイル名の一部（例：`tax`）を入力すると、パスにその文字を含むファイルだけが表示されること。大文字小文字を区別しないこと（`TAX` でも `tax` を含むファイルが出ること）。フォルダ名で検索すると配下のファイルが表示されること。検索文字列を消すと全行に戻ること。件数行の N が絞り込み後の件数と一致すること。
3. **全展開／全折畳**：サブフォルダを含むフォルダ比較でファイル比較を開く。「すべて折りたたむ」を押すとディレクトリ行だけが残り、ファイル行が消えること。「すべて展開」を押すと全行が戻ること。件数行の N が変わること。
4. **ディレクトリ行の展開／折りたたみ**：ディレクトリ行をダブルクリックすると、そのフォルダ配下だけが折りたたまれる／展開されること。▾／▸ の表示が切り替わること。右クリックメニューの「フォルダを展開」「フォルダを折りたたむ」でも同じ動きになること。フォルダ比較側のツリー表示は変わらないこと（パネル側だけの状態であること）。
5. **一括チェックボタンの起動**：パネルの「一括チェック…」ボタンを押すと、フォルダ比較の右クリックメニューから開くのと同じダイアログ（確認内容 5 状態のチェックボックス・件数付き OK ボタン）が開くこと。ダイアログの件数がパネルの表示分と一致すること。
6. **一括チェックの適用**：ダイアログで「未確認」にチェックして OK すると、表示中の未確認ファイルの出力チェックが ON になること（パネルとフォルダ比較の一覧の両方で ☑ になること）。「表示分すべてチェック」「全解除」も同じダイアログで動くこと。キャンセルでは何も変わらないこと。検索で絞り込んだ状態での一括チェックは、表示中のファイルだけが対象になること（隠れているファイルは変わらないこと）。ただし「全解除」は比較全体が対象になること（S3 と同じ）。
7. **フィルタタブの表示**：パネル上部に対応区分のタブ（すべて＋7 種）が表示されること。「すべて」では全行が表示されること。
8. **フィルタタブの絞り込み**：区分タブ（例：「要マージ（旧欠落）」）を選ぶと、その区分のファイルだけが表示されること。未分類（未開封）のファイルは区分タブでは表示されず、「すべて」で表示されること。ディレクトリ行は、表示される子孫を持つ場合に残ること。件数行の N が絞り込み後の件数と一致すること。
9. **検索とフィルタタブの併用**：検索文字列を入れた状態で区分タブを選ぶと、両方の条件を満たすファイルだけが表示されること。
10. **ディレクトリ単位の操作**：ディレクトリ行を右クリックし、「配下をすべてチェック」を選ぶと、そのフォルダ配下の全ファイル（折りたたまれたサブフォルダ配下を含む）の出力チェックが ON になること。「配下のチェックをすべて外す」で OFF になること。フォルダ比較の一覧側でも同じ結果になること。
11. **回帰なし**：ファイル比較・フォルダ比較が従来どおり動くこと。P1 の列表示・行色・件数表示が変わらないこと。確認内容の変更・出力チェック・CSV 出力・セッション保存／復元・一括分類・専用モード・スナイパー・フォーカスドック・Grep が従来どおり動くこと。

### 40.6 P3 への引き継ぎ

- P2 の範囲外（P3 以降）：パネルからの操作（行クリックでの比較切替・確認内容の変更・出力チェックのトグル）は P3。表示状態・幅・スリム化・検索文字列・折りたたみ状態・選択タブの保存・復元の完成は P4（D11 と連携）。
- 既知の制限：パネルからの出力フラグ変更時、フォルダ比較画面のステータスバーの件数表示が次回更新まで古いままになる（§40.2 参照）。P3 でフォルダ側への通知（または活性化時更新）を検討する。
- H-FILTER 着手時の判断事項：パネルのフィルタタブ（対応区分）と H-FILTER のデッキ（対応区分を含む）の同期、またはデッキのパネル側への寄せ（P-R10 の方向性）。P2 のタブ状態はパネル内のメンバ変数に閉じており、同期が必要になった場合は公開 API を追加する。
- 旧／変／新の内訳 3 列（H-LIST）は未実装のまま（P1 の暫定 1 列のまま）。H-LIST の着手時に `Merge20CsvGetOldChangedNewText` を 3 列分の getter に置き換える（§39.6 参照）。
- 再スキャン後のパネル更新は未整備のまま（§39.6 参照）。P3 で検討する。
- 閉じたフレームの通知登録は残るまま（§39.6 参照）。P3 で解除 API を検討する。
- 実機確認待ち（§40.5 の全項目）。実機で問題が出た場合は本節へ結果を追記すること。

## 41. Panel P3 — 常設ファイル一覧パネル 連動（設計書 §12.7 P3）

- 対応する設計書：§12.3 P-R7（パネルからの比較切替・選択追従）、P-R8（パネルからの確認内容・出力決定）。P1 の残課題（再スキャン後のパネル更新、閉じたフレームの通知登録の解除 API なし）と P2 の既知の制限（フォルダ比較画面のステータスバー件数表示の遅延）も P3 の範囲として解消した。
- ブランチ：`merge20/panel-p3`（base master 58da8968）。PR #37（作成後に番号・URL を追記）。
- コード head：95ade37e（.po まで）。最終 head（本節の記録コミット後）は §41.4 に追記。

### 41.1 変更一覧（1ファイル1コミット。連続コミットの間に sleep 3）

| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | Src/StdAfx.h | 4456f124 | `MSG_MERGE20_FILELIST_OPEN_FILE = WM_USER + 5` を追加（WM_USER + 1〜+4 は使用済みで +5 は未使用。取得済みファイルの grep で確認） |
| 2 | Src/resource.h | c4b0319e | `ID_MERGE20_FILELIST_TOGGLE_OUTPUT 38967` を追加。`_APS_NEXT_COMMAND_VALUE` を 38967 → 38968 に更新。コメント欄に P3 の消費を追記 |
| 3 | Src/DiffContext.h | dff1f1b3 | `RemoveMerge20FileListNotifyCallback(std::uintptr_t key)` の宣言 |
| 4 | Src/DiffContext.cpp | c104aa30 | `RemoveMerge20FileListNotifyCallback` の実装（キー一致エントリを erase、未知キーは no-op） |
| 5 | Testing/GoogleTest/Merge20/Merge20FileListNotify_test.cpp | b50ec52a | Remove の単体テスト 3 件を追加（`RemoveUnregistersOne`、`RemoveUnknownKeyIsNoop`、`RemoveThenReaddFires`） |
| 6 | Src/DirDoc.h | 5047787b | P3 公開 API 5 件の宣言（`Merge20FindItemByRelPath`、`Merge20FindRelPath`、`Merge20MoveToFile`、`Merge20UpdateDirViewItem`、`Merge20UpdateDirViewStatus`） |
| 7 | Src/DirDoc.cpp | 734456a6 | P3 実装本体（上記 5 件＋`Merge20DocMatchesItem`）。`CompareReady()` の末尾に再スキャン完了通知を追加 |
| 8 | Src/DirView.h | 8f643c16 | `Merge20OpenFile` の宣言。`UpdateMerge20StatusCount` を private から public へ移動 |
| 9 | Src/DirView.cpp | 407fd4d4 | `Merge20OpenFile` の実装 |
| 10 | Src/Merge20/Merge20FileListCtrl.h | 8a8d4c32 | `WM_MERGE20_FILELIST_ROW_CLICK`（`WM_USER + 121`）、`Column` enum の public 化、`SelectRowByRelPath`／`GetSelectedRow`／`OnClick` の宣言 |
| 11 | Src/Merge20/Merge20FileListCtrl.cpp | a928f2c8 | `OnClick`、`SelectRowByRelPath`、`GetSelectedRow` の実装。メッセージ・マップに `ON_NOTIFY_REFLECT(NM_CLICK, OnClick)` を追加 |
| 12 | Src/Merge20/Merge20FileListBar.h | 3cbbc7e5 | 行操作 API（`HandleListKey`、`OnRowClick`、`TakePendingOpenRelPath`、`SetCurrentFileRelPath` ほか）とメンバ `m_sCurrentRelPath`／`m_sPendingOpenRelPath` の宣言 |
| 13 | Src/Merge20/Merge20FileListBar.cpp | a0f88ba4 | 行クリック振り分け、キー処理、ファイル行メニュー、行操作ヘルパー、Refresh 末尾での選択追従、一括・ディレクトリ単位操作後のフォルダ側同期 |
| 14 | Src/MergeEditFrm.h | f75725bd | `RefreshFileListPanel()` と `OnMerge20FileListOpenFile` の宣言 |
| 15 | Src/MergeEditFrm.cpp | 1bef93f3 | 開くメッセージの受信、`RefreshFileListPanel()`、既存 4 か所の Refresh() の置き換え、`PreTranslateMessage` へのキー処理、`DestroyWindow()` での登録解除、`ChangeFile` 成功時の追従 |
| 16 | Translations/WinMerge/Japanese.po | 95ade37e | 新規 msgid `"Toggle output"`（msgstr「【Wiz】出力を切り替える」）を 1 件追加 |
| 17 | MERGE20_NOTES.md | （本コミット） | 本節 §41 の記録 |

- 新規コマンド ID は 38967 の 1 件のみ。タスク指示の「コマンド ID 38966 以降」は古く、実際は 38966 は P0 のパネル・トグル `ID_VIEW_MERGE20_FILELIST_PANEL` が使用済みだった。resource.h の全 define を値でソートして空きを確認し、389xx に重複がないことを確認済み（35xxx〜36xxx の重複は master と同一の既存分）。
- 新規 .po 文字列は `"Toggle output"` 1 件のみ。確認内容の 5 表示名は S1 の既存 msgid（"Not reviewed"→未確認、"Done"→済、"On hold"→保留、"Needs manual work"→個別対応要、"Keep as is"→現状維持）をリテラル `_()` 呼び出しで流用。右クリックメニューの確認内容項目 ID は専用バーのレビュー・ボタン ID `ID_MERGE20_DED_REVIEW_TODO`〜`ID_MERGE20_DED_REVIEW_KEEP`（38945〜38949）を流用し、`TPM_RETURNCMD` で受けて `state = id - TODO` とする（P2 がコントロール ID をメニュー ID として流用したのと同じ方式）。`WM_MERGE20_FILELIST_ROW_KEY`（WM_USER + 122）は作らない（フレームの PreTranslateMessage 経由に一本化）。

### 41.2 設計判断（設計書との食い違いと判断理由）

- **行クリックの開き方：「現在の比較を閉じて開く」**（P-R7）。HTML 版の selFile と同じ「その場で切り替える」動き。開くたびに MDI 子ウィンドウを増やすのは「マージしながら出力対象を決める」動線に合わないため。`CDirDoc::MoveToNextFile` と同じ形（現在の merge doc を `CloseNow()` してから `CDirView::Merge20OpenFile` で開く）。`CloseNow()` が false（未保存変更をユーザーがキャンセル）を返した場合は何も開かない（MoveToNextFile は戻り値を無視する既存の挙動だが、P3 では尊重する）。
- **既に開いている場合は重複して開かず、そのフレームを活性化する**（P-R7 の「既に開いている場合はそのファイルに切り替わる」）。照合は `Merge20DocMatchesItem`（全ペインの比較。比較パスが NUL デバイスで doc パスが空の場合も一致とみなす。根拠：`CDiffContext::GetComparePaths` は欠落側に `paths::NATIVE_NULL_DEVICE_NAME` を使い、`CDirView::Open` は欠落側の FileLocation を未設定のままにするため `FileLocation` の既定コンストラクタにより merge doc の `m_filePaths` は空になる。`CMergeDoc::LoadFile` で `m_filePaths[nBuffer] = sFileName` であることも実読で確認済み）。
- **閉じる操作の安全性：バーは PostMessage のみ**。フレームのハンドラ `OnMerge20FileListOpenFile` は `Merge20MoveToFile` を最後の文として呼び、`CloseNow()` がフレームを同期的に破棄するため、その後にフレームのメンバに触れない（DedBar の Prev/Next ボタンが main frame へ PostMessage する前例と、`CFrameWnd::OnClose` が `DestroyWindow()` を最後の文として呼ぶ MFC 標準パターンを根拠とする）。
- **宛先の受け渡し：relPath をバーのメンバに置き、フレームが処理時に解決する**（`TakePendingOpenRelPath()` → `Merge20FindItemByRelPath`）。PostMessage に DIFFITEM ポインタを載せないため、クリックと処理の間に再スキャンが走っても誤った行を開かない。
- **行クリックの振り分け（P-R8）**：出力列→トグル（フォルダ一覧の S2 と同じ）、確認内容列→巡回（Space と同じ）、それ以外→ファイルを開く。ディレクトリ行のシングルクリックは何もしない（折りたたみは P2 どおりダブルクリック）。
- **キー操作の場所：フレームの `PreTranslateMessage` → バー `HandleListKey`**（`WM_MERGE20_FILELIST_ROW_KEY` は作らない）。リスト自体はキーを swallow できないため。フォーカスがリストそのものであることと、Ctrl／Alt／Shift が押されていないこと（bare keys のみ、DirView と同じ条件）を確認してから、1〜5→設定、Space→巡回。処理したら TRUE を返して swallow する。専用モードでもパネルが表示されるため専用モードでも動く（フォーカス条件によりエディタでの入力とは干渉しない）。
- **選択行の追従（P-R7、逆方向）**：フレームに private ヘルパー `RefreshFileListPanel()` を新設。`m_pMergeDoc->m_filePaths` から `CDirDoc::Merge20FindRelPath` で relPath を求め、バーに `SetCurrentFileRelPath(relPath)` してから `Refresh()` する。既存の 4 か所の `m_wndFileListBar.Refresh()` 呼び出し（`OnCreate`、`OnBarCheck`、`OnMDIActivate`、`OnMerge20FileListRefresh`）をすべてこれに置き換えた。これにより、パネル・クリック／Next・Prev／専用バーの Prev・Next で新しいフレームが開いた場合も、パネルの選択行が新しいファイルになる。ファイル・パス・バーからの `ChangeFile` 成功時（`OnCreateClient` の `SetOnFileSelectedCallback` 内）にも `RefreshFileListPanel()` を呼ぶ（同一フレーム内でのファイル切替にも追従する）。
- **relPath の解決（`Merge20FindRelPath`）**：まず D2 と同じ全パス照合 `FindMerge20ItemFromPaths` を試す。片側のみファイルでは doc の欠落側パスが空のため全パス照合が失敗するので、フォールバックとしてコンテキスト全走査で「空でない任意の1ペインが一致する」項目を探す（同一フォルダ比較内では1ペインのパスは1項目を一意に特定する）。なお、`Merge20OpenFileAtLine`（Grep ジャンプ、D7）の照合条件（`!paths[i].empty() && pDoc->GetPath(i)!= paths[i]`）は片側のみファイルでは黙って失敗する（doc パスは空、比較パスは NUL なので不一致になる）。これは D7 の既存の wart であり P3 では直さないが、`Merge20FindRelPath` のフォールバックではこの問題を回避している。
- **パネルの選択は常に比較ウィンドウのファイルと一致させる**（シングルクリックが開く操作であるため）。バーの `Refresh()` の末尾で、格納済みの `m_sCurrentRelPath` が空でなければ `m_wndList.SelectRowByRelPath(m_sCurrentRelPath)` を呼ぶ（行が再構築された後なので、インデックスではなく relPath で追う）。表示されていない行（検索・フィルタタブ・折りたたみで隠れている、または再スキャンで消えた）の場合は、陳腐化した選択を残さないよう選択を解除する（`SetItemState(-1, 0, ...)`）。非表示時の Refresh() は早期 return するので、その場合は選択更新をスキップし、表示時にフレーム経由で再設定される。
- **フォルダ比較画面との同期（P-R8 の (b)、P2 の既知の制限の解消）**：`CDirDoc::Merge20UpdateDirViewItem`（行の再描画＋ステータスバー件数更新）と `Merge20UpdateDirViewStatus`（フォルダ一覧の Invalidate＋件数更新を1回だけ）を新設。単一行の変更（パネルからの確認内容・出力変更）で前者、一括チェック／ディレクトリ単位操作の後に後者を呼ぶ（S3 のフォルダ側の一括処理と同じ「ループ後に1回」の規則）。`CDirView::UpdateMerge20StatusCount` を private から public へ移動（`GetMerge20StatusCountText()` は private のまま）。
- **同期を setter 自体に入れない理由**：`SetMerge20IncludeInOutput`／`ApplyMerge20ReviewState`（DIFFITEM 版）のフォワーダ自体にフォルダ側同期を入れると、一括ループの中で呼ばれてファイル数分の再描画・件数更新になってしまうため。同期はバーの操作単位（単一行→`Merge20UpdateDirViewItem`、一括→`Merge20UpdateDirViewStatus`）で行う。
- **再スキャン後のパネル更新（P1 の残課題）**：`CDirDoc::CompareReady()` の末尾に `m_pCtxt->NotifyMerge20FileListChanged()` を追加（`m_pCtxt != nullptr` でガード）。再スキャン完了（`CDirView::OnUpdateUIMessage` の `EVENT_COMPARE_COMPLETED` → `CompareReady()` → `Redisplay()`）時に全パネルへ通知する。通知は PostMessage（非同期）なので、パネルの Refresh はフォルダ側の Redisplay の後に行われる。初回比較時にはまだパネルが登録されていないため no-op。`SaveModified` の中断経路からも `CompareReady()` が呼ばれるが、その場合も通知は無害。
- **閉じたフレームの通知登録の解除（P1 の残課題）**：`CDiffContext::RemoveMerge20FileListNotifyCallback(std::uintptr_t key)` を追加（宣言＋実装＋単体テスト 3 件）。フレームは `CMergeEditFrame::DestroyWindow()` の先頭で、登録時と同じキー（フレーム HWND を `std::uintptr_t` 化したもの）で解除する。`DestroyWindow()` はフレーム生成失敗の途中でも呼ばれるため、解除は未知キー no-op であることが重要（テストで確認済み）。P0 の単一コールバック（`SetMerge20FileListNotifyCallback`）は変更しない（本番コードからの呼び出しは P1 以降 Add 経由のみ。テストのみが Set を使う）。
- **`CDirView::GetItemIndex` は未発見時に -1 ではなく 0 を返す upstream の癖がある**（`DirView.cpp` の実装を実読して確認。`return 0;`）。既存コードの `nIdx != -1` チェックはそのため実質死んでいる。P3 のコードでは `GetItemIndex` の結果を `GetItemKey(nIdx) == &di` で検証してから使う（`Merge20OpenFile` と `Merge20UpdateDirViewItem` の両方）。既存の -1 判定（`Merge20OpenFileAtLine` ほか）は直さない（upstream の既存挙動への変更は P3 の範囲外）。
- **P1／P2 の残課題のうち P3 の範囲外のもの**：旧／変／新の内訳 3 列（H-LIST）は未実装のまま（P1 の暫定 1 列）。H-FILTER との整理も P3 の範囲外。P4（保存・復元の完成：表示状態・幅・スリム化・検索文字列・折りたたみ状態・選択タブ）は P3 の範囲外であることを明記する。

### 41.3 受け入れ条件の照合表（設計書 §12.7 P3）

| 条件 | 設計書の文言 | 判定 | 根拠 |
|---|---|---|---|
| (a) | 「マージしつつ出力対象を決める動線が使えること」：パネルでファイルを選ぶ→差分を判定する→パネルで確認内容を「済」にする→出力チェックが ON になる、という一連の動きがファイル比較ウィンドウから出ずに完結すること | コード上は満たす（実機未検証） | `OnRowClick`（開く）→`ApplyRowReview`（DONE へ。S2 の出力連動で DONE は出力フラグ ON へ。ただし 2-way の NEW_ONLY ファイルは OFF のまま）→`ToggleRowOutput`。実機での確認は §41.5 の手順 6 |
| (b) | フォルダ比較画面に戻ったときに、パネルでの変更が反映されていること | コード上は満たす（実機未検証） | 同じ CDiffContext のサイドカーを共有するため変更自体は即座に共有される。加えて `Merge20UpdateDirViewItem`／`Merge20UpdateDirViewStatus` でフォルダ行の再描画とステータスバー件数更新を行う。実機での確認は §41.5 の手順 12・13 |
| (c) | 比較ウィンドウでのファイル移動にパネルの選択行が追従すること | コード上は満たす（実機未検証） | `RefreshFileListPanel()`（Next／Prev／専用バーの Prev・Next／ファイル・パス・バーの ChangeFile の全経路で呼ぶ）。実機での確認は §41.5 の手順 4・5 |
| P-R7 | パネルで行をシングルクリックすると、そのファイルの比較がファイル比較ウィンドウで開く（または既に開いている場合はそのファイルに切り替わる）。比較ウィンドウで別のファイルに移動した場合は、パネルの選択行も追従する（双方向の連動） | コード上は満たす（実機未検証） | `OnRowClick`→`OpenRowFile`→`MSG_MERGE20_FILELIST_OPEN_FILE`→`Merge20MoveToFile`（既存フレームの活性化を含む）。逆方向は `RefreshFileListPanel()` |
| P-R8 | パネルの行で確認内容を変更できる（キー 1〜5・Space・右クリックメニュー・確認内容列のクリック）。出力チェックの ON／OFF もパネルのチェックボックスで直接変更できる。変更はフォルダ比較の一覧と即座に同期する | コード上は満たす（実機未検証） | キー（`HandleListKey`）、Space（巡回）、右クリックメニュー（5 状態＋出力トグル）、確認内容列クリック（巡回）、出力列クリック（トグル）。同期は `Merge20UpdateDirViewItem`／`Merge20UpdateDirViewStatus` |

- P1 の残課題「再スキャン後のパネル更新」：`CompareReady()` への通知追加で解消（コード上。実機確認は §41.5 の手順 14）。
- P1 の残課題「閉じたフレームの通知登録の解除 API なし」：`RemoveMerge20FileListNotifyCallback` の追加と `DestroyWindow()` での解除で解消（単体テスト 3 件で検証済み。実機での観察は §41.5 の手順 15）。
- P2 の既知の制限「パネルからの出力フラグ変更時、フォルダ比較画面のステータスバー件数表示が次回更新まで古いまま」：`Merge20UpdateDirViewItem`／`Merge20UpdateDirViewStatus` の追加で解消（コード上。実機確認は §41.5 の手順 12・13）。

### 41.4 検証レポート

- **CI（P4 で補完。以下は検証済みの数値）**：CI 実行 45847947921（head `ad3985fa`、PR #37）：結論 success。GoogleTest 718 テスト全パス（`[ PASSED ] 718 tests.`、`[ FAILED ]` 行なし）。コンパイルエラー 0 件（`error C` 0 件、`Build succeeded`）。成果物：WinMerge-x64.zip 26,443,567 バイト（ID 15132562210、失効 2027-02-24T15:19:35Z）、WinMerge-x64-Setup-x64.exe 39,888,101 バイト（ID 15132801925、失効 2027-02-24T15:19:35Z）。全文ログは `~/workspace/.coord/p3_final_head_log.txt` に保存（47,572 行）。ログ中の "FAILED" 4 件は翻訳文字列 ID（IDS_FILESAVE_FAILED 等）で、テスト失敗ではない。CI の番兵「Merge20 strings present in the bundled Japanese.po.」も確認。PR #37 はマージ済み（closed）。head `ad3985fa` は本節の記録コミットを含む最終 head であり、上記の実行がその最終 head のものである（最終 head が NOTES 記録だけのコミットでも、その CI 実行まで確認すること、という検証ゲートの要求を満たす）。
- **GoogleTest のテスト数**：P2 時点の 715 件から＋3 件で **718 件**（確定。新規は `Merge20FileListNotify` の Remove 3 件のみ。MFC 側の新規コードに単体テストはない）。
- **.po 監査**：ブランチ版は msgid **2158 件**（master 版 2157 件＋1）、【Wiz】**126 件**（master 版 125 件＋1）。増分が新規文字列 1 件（"Toggle output"）と一致することを確認済み。msgstr は「【Wiz】出力を切り替える」（.po ファイルへの追加は exec 経由で行い、全角括弧の欠落がないことを確認済み）。
- **ローカル検証（コード上）**：変更した 15 ファイルの括弧バランス（文字列・コメント除去後の ()、{}、[]）をすべて確認済み。リソース ID の重複チェック（389xx に重複なし。35xxx〜36xxx の重複は master と同一の既存分）。新規 msgid `"Toggle output"` のリテラル `_()` 呼び出し（`Src/Merge20/Merge20FileListBar.cpp:767`）を確認済み。`WM_USER + 5` と `WM_USER + 121` が未使用であることを取得済みファイルの grep で確認済み（`WM_USER + 122` は作らない）。
- **実読で確認済みの事実（実装の根拠）**：`CMergeDoc::CloseNow()` は `PromptAndSaveIfNeeded(true)` の後に `GetParentFrame()->DestroyWindow()` を同期的に呼ぶ。`CDirDoc::m_MergeDocs` は `AddMergeDoc` で追加、`MergeDocClosing` で除去される。`CDirDoc::GetMainView()` は public。`CDirView::GetItemKey(int)` は public（範囲外は ASSERT）。`CDirView::OpenSelection`／`MoveFocus`／`GetFocusedItem` は private のため、パネルからは `Merge20OpenFile`（新規 public メソッド）経由でのみ開く。`CMergeDoc::m_filePaths` は `MergeEditFrm.cpp` から既にアクセスされている public メンバ。`GetItemIndex` の未発見時 0 返却（`DirView.cpp` の `return 0;` を実読）。`Merge20OpenFileAtLine` の片側ファイルでの黙った失敗（D7 の既存の wart。P3 では直さない）。
- **未検証（実機でしか確認できないもの）**：§41.5 の実機確認チェックリストの全項目。「できた」とは言わない。

### 41.5 実機確認チェックリスト（未検証。Windows 実機での確認手順）

1. **行クリックで比較が開く**：フォルダ比較で2ファイル以上を開く。比較ウィンドウ左のパネルで別のファイルの行をシングルクリックする→そのファイルの比較が同じウィンドウに開くこと。現在の比較は閉じられること（MDI 子ウィンドウが増えないこと）。
2. **既に開いているファイルへの切り替え**：2つの比較ウィンドウを開いておく。1つ目のウィンドウのパネルで、2つ目のウィンドウで開いているファイルの行をクリックする→2つ目のウィンドウが活性化されること（重複して開かないこと）。
3. **現在のファイルの行クリック**：パネルで現在開いているファイルの行をクリックする→何も起きないこと（比較が閉じて開き直されることがないこと）。
4. **Next／Prev での選択行の追従**：比較ウィンドウで Next／Prev（ツールバーまたは専用バーの Prev／Next）でファイル移動する→パネルの選択行が移動先のファイルになること。
5. **ファイル・パス・バーでの切替時の追従**：比較ウィンドウのファイル・パス・バーから別のファイルに切り替える→パネルの選択行が切り替えたファイルになること。
6. **受け入れ条件 (a) の一連の動線**：パネルでファイルを選ぶ→差分を判定する（エディタで差分を確認する）→パネルで確認内容を「済」にする（キー 2 または確認内容列クリックの巡回、または右クリックメニューで「済」）→出力チェックが ON になること。ファイル比較ウィンドウから出ずに完結すること。
7. **確認内容の変更（キー 1〜5）**：パネルのリストにフォーカスを当て、キー 1〜5 を押す→確認内容がそれぞれ未確認／済／保留／個別対応要／現状維持になること。フォルダ比較の一覧の確認内容も同時に変わること。検索エディットにフォーカスがある状態で '1' を入力した場合は検索文字列になること（確認内容が変わらないこと）。
8. **Space での循環**：パネルのリストにフォーカスを当て、Space を押す→確認内容が TODO→DONE→HOLD→SPECIAL→KEEP→TODO と巡回すること。
9. **確認内容列クリックでの循環**：パネルで確認内容列（「確認内容」）をシングルクリックする→Space と同じ巡回をすること。
10. **出力列クリックでのトグル**：パネルで出力列（「出力」）をシングルクリックする→出力チェックが ON／OFF すること。フォルダ比較の一覧の出力チェックも同時に変わること。
11. **右クリックメニュー**：パネルのファイル行を右クリックする→確認内容 5 状態（現在の状態にチェック付き）と「出力を切り替える」（ON ならチェック付き）のメニューが出ること。各項目を選択するとその状態／出力になること。ディレクトリ行の右クリックは従来どおりのメニュー（折りたたみ／チェック／解除）が出ること。
12. **フォルダ比較画面への反映**：パネルで確認内容・出力を変更した後、フォルダ比較画面に戻る→該当行の表示（確認内容・出力チェック）が変わっていること。ステータスバーの件数表示（出力対象・未確認件数）が変更直後の値になっていること（次回更新まで古いままにならないこと）。
13. **一括チェック後のフォルダ側ステータスバー件数**：パネルで一括チェックを実行する→フォルダ比較画面のステータスバー件数が変更直後の値になっていること。
14. **再スキャン後のパネル更新**：フォルダ比較で再スキャン（F5 等）を実行する→比較ウィンドウのパネルの一覧が新しい比較結果に更新されること（次の活性化まで古いままにならないこと）。
15. **閉じたフレームの通知**：同じフォルダ比較から複数の比較ウィンドウを開いて閉じてを繰り返す→パネルの表示が壊れないこと（閉じたフレームへの通知が残っていないこと。目視での確認）。
16. **片側ファイル（旧のみ／新のみ）の行クリックと選択追従**：旧のみ／新のみのファイルの行をクリックする→そのファイルの比較が開くこと（片側ファイルでも開けること）。そのファイルを開いた状態でパネルの選択行がそのファイルになっていること。
17. **回帰なし**：専用モード、スナイパー、巡回（未確認のみ）、フォーカス・ドック、ダブルクリックでのフォルダ折りたたみ／展開、検索、フィルタタブ、一括チェック、ディレクトリ単位操作が従来どおり動くこと。

### 41.6 P4 への引き継ぎ（P3 の範囲外）

- **P4（保存・復元の完成）**：表示状態・幅・スリム化・検索文字列・折りたたみ状態・選択タブの保存・復元の完成。P3 で追加した `m_sCurrentRelPath`（比較ウィンドウのファイル）は復元対象ではない（フレームごとに解決する）。`m_sPendingOpenRelPath` は一時的な受け渡し用メンバであり、保存対象ではない。
- **P3 で未実装のまま（P1 の暫定のまま）**：旧／変／新の内訳 3 列（H-LIST）。H-FILTER との整理も未着手。
- **既知の制限（P3 で直さないもの）**：`CDirView::GetItemIndex` の未発見時 0 返却（upstream の癖。P3 の新規コードは `GetItemKey` 検証で回避済み）。`Merge20OpenFileAtLine`（Grep ジャンプ、D7）の片側のみファイルでの黙った失敗（`Merge20FindRelPath` のフォールバックでは回避済み）。`Merge20OpenFile` はフォルダ側フィルタで隠れている行は開けず、Grep ジャンプと同じ msgid「Cannot open the file because it is hidden by the current filter.」のメッセージボックスを出す（既存 msgid の流用で新規文字列なし）。
- **実機確認待ち**：§41.5 の全項目（特に手順 6 の受け入れ条件 (a) の動線と、手順 16 の片側ファイル）。

## 42. Panel P4 — 常設ファイル一覧パネル 保存・復元＋受け入れテスト・文書化（設計書 §12.7 P4）

- （§41.4 の補完）P3 の検証レポート §41.4 の CI 欄が「PR 作成後に追記」のままだった不備を、本節の冒頭で補完した。補完内容は §41.4 の CI 項目（実行 45847947921、718 テスト全パス、成果物 2 件）を参照。以下は P4 本体の記録である。
- 対応する設計書：§12.3 P-R9（表示／非表示の切替と設定の保存・復元）、P-R4（検索文字列の保存・復元は D11＝P4 の対象）、§12.7 P4。P2 から引き継いだ検索文字列・折りたたみ状態・選択タブ、P0 から引き継いだ表示状態・幅、P3 から引き継いだ保存対象外の明記（`m_sCurrentRelPath`／`m_sPendingOpenRelPath` は保存しない）を含む。
- ブランチ：`merge20/panel-p4`（base master 4255f79b）。PR #38（https://github.com/askhim659/winmerge/pull/38）。
- コード head：`b21c6f00`（.po まで。テストの符号比較ワーニング回避を含む）。最終 head（本節の記録コミット後）は §42.4 に追記。

### 42.1 変更一覧（1 ファイル 1 コミット、13 件＋本節の記録コミット。連続コミットの間に sleep 3）

| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | Src/OptionsDef.h | a31bb290 | 新キー 4 件（`OPT_MERGE20_FILELIST_SEARCH`／`TAB`／`COLLAPSED`／`SLIM`）。`OPT_MERGE20_SLIM` を流用しない理由をコメントに明記 |
| 2 | Src/OptionsInit.cpp | bf062d82 | 同 4 キーを初期化（検索 ""・タブ 0（範囲 0〜7）・折りたたみ ""・スリム false） |
| 3 | Src/Merge20/Merge20FileListFilter.h | 54aedd60 | 折りたたみパスの join／split（`Merge20JoinCollapsedPaths`／`Merge20SplitCollapsedPaths`）の宣言。`<set>` の include 追加 |
| 4 | Src/Merge20/Merge20FileListFilter.cpp | dbc57cc7 | 同上の実装（改行区切りの 1 文字列。空ピースは読み飛ばす） |
| 5 | Testing/GoogleTest/Merge20/Merge20FileListFilter_test.cpp | 5ff4dbff | join／split の単体テスト 5 件を追加 |
| 6 | Src/Merge20/Merge20FileListCtrl.h | 2b2ca308 | 折りたたみ集合のアクセサ（`GetCollapsedPaths`／`SetCollapsedPaths`）と `ApplySlim` の宣言。`m_bSlim`／`m_savedColWidths` メンバ追加 |
| 7 | Src/Merge20/Merge20FileListCtrl.cpp | 1bab6f6a | 同上の実装。折りたたみ変更時（`SetCollapsed`／`ExpandAll`／`CollapseAll`）の即保存。スリムは列幅 0 で隠し、解除時に記憶した幅へ復元 |
| 8 | Src/resource.h | d1c996ac | `IDC_MERGE20_FILELIST_SLIM` 1818 を確保。`_APS_NEXT_CONTROL_VALUE` 1818→1819。コメント欄に P4 の消費を追記 |
| 9 | Src/Merge20/Merge20FileListBar.h | d2ff730c | スリムボタン（`m_btnSlim`）・`m_bSlim`・`SetSlim`／`SavePanelState`／`RestorePanelState`／`ApplySlimState`・`OnSlimToggle` の宣言 |
| 10 | Src/Merge20/Merge20FileListBar.cpp | fa3c60d2 | スリムボタンの生成・レイアウト（スリム中は上段を折りたたむ）・保存／復元の実装。タブ変更時の即保存。検索文字列は SavePosition 保存のみ（コメントに理由を明記） |
| 11 | Src/MergeEditFrm.cpp | 3307c611 | OnCreate で `RestorePanelState()`（最初の Refresh の前）、SavePosition で `SavePanelState()` を呼ぶ |
| 12 | Translations/WinMerge/Japanese.po | 4732091d | 新規 msgid `"Slim view"`（msgstr「【Wiz】スリム表示」）を 1 件追加 |
| 13 | Testing/GoogleTest/Merge20/Merge20FileListFilter_test.cpp | b21c6f00 | テストの符号比較ワーニング回避（`ASSERT_EQ(paths.size(), 2u)` → `static_cast<size_t>(2)`） |
| 14 | MERGE20_NOTES.md | （本コミット） | 本節 §42 の記録と §41.4 の補完 |

- 新規コマンド ID・ダイアログ ID なし。スリムボタンはコントロール ID 1818 のみ（P2 が 1813〜1817 を使用。`_APS_NEXT_CONTROL_VALUE` は 1818 だった）。vcxproj の変更なし（新規 .cpp なしのため登録不要）。

### 42.2 設計判断（設計書との食い違いと判断理由）

- **スリム用のキーは新設し、`OPT_MERGE20_SLIM` は流用しない**。設計書 §12.3 P-R9 は `OPT_MERGE20_SLIM`／`OPT_MERGE20_DOCK_HIDDEN` の流用を想定しているが、P0 が `OPT_MERGE20_DOCK_HIDDEN` について確認したのと同じ理由で流用できない：`OPT_MERGE20_SLIM` はフォルダ比較一覧のスリムフラグ（D9）であり、パネルのスリムと共有すると一方の切替が他方の表示を変えてしまう。専用キー `OPT_MERGE20_FILELIST_SLIM`（`Merge20/FileListSlim`、既定 false）を新設した（P0 の `OPT_MERGE20_FILELIST_VISIBLE` と同じ判断）。
- **スリムは手動トグルのみ。専用モード入場時の自動スリム化は実装しない**。設計書 §12.6 は専用モード入場時にパネルを「既定でスリム化表示」とするが、P0 の決定（専用モードはパネルに触れない）が優先する。自動スリム化には入場前の状態の記憶と退場時の復元が必要で、P4 の受け入れ条件（保存・復元）には含まれない。残課題として §42.6 に残す。
- **スリムの実体は「ファイル名＋確認内容の列だけ」＋上段の折りたたみ**。設計書 §12.4 の定義どおり、列はファイル名・確認内容だけを残し、他列は幅 0 で隠す。隠す前の列幅を覚えておき、スリム解除時に復元する（手動で広げた列幅がスリム往復で既定幅に戻らないようにするため）。検索行・フィルタタブ・ボタン行も折りたたむが、件数行は残す（件数はスリム表示でも要るため）。スリムボタン自体はスリム中も表示する（戻る手段のため）。
- **保存のタイミングは項目ごとに分ける**。検索文字列は SavePosition 時（フレーム終了時と、バー非表示後の遅延 pane-size 保存時）のみ。毎キーストロークで保存するとレジストリ書き込みがキー入力ごとに発生するため。一方、タブ・折りたたみ・スリムは離散的なユーザー操作なので変更直後に即保存し、SavePosition でも再度保存する（D11 のドック隠しキーと同じ二重保存の前例）。クラッシュ時、タブ・折りたたみ・スリムは残り、検索文字列だけが失われる可能性がある（制限として §42.6 に記録）。
- **折りたたみ集合はグローバル（OptionsMgr）で、フォルダペアごとではない**。相対パスだけをキーにしているため、別フォルダ比較で同じ相対パスのフォルダが折りたたまれることがある。幅の保存と同じ「最後に閉じたフレームが勝つ」規則であり、陳腐化したパスは表示行に一致しない限り無害である（実機確認 §42.5 項目 5・9 で挙動を確認する）。
- **折りたたみの直列化は改行区切りの 1 文字列**。`Merge20JoinCollapsedPaths`／`Merge20SplitCollapsedPaths` を純粋モジュールに置き、単体テスト 5 件で往復を確認した。区切り文字は改行（Windows のファイル名に改行は実用上含まれない）。空ピースは読み飛ばすため、手編集・途切れた値でも空の誤パスはできない。
- **D11 との連携：重複する保存はない**。D11 の §4.1 (a)／(b) はフォルダ側の表示設定（専用モード・ピン・倍率・未確認のみ巡回・スナイパー・スリム・Grep 履歴・レビューフィルタ）とセッションの表示設定を扱い、パネルの状態は含まない。実読で確認：`Merge20SessionDoc.cpp` の `FileList` 出現は `NotifyMerge20FileListChanged()` の呼び出し 1 件だけで、パネル状態の保存・復元はない。パネル状態はフレーム側の永続設定（OptionsMgr の専用キー）に置き、セッションには入れない（パネルはファイル比較フレームに属し、フォルダ比較のセッションには属さないため）。Grep の検索文字列（`OPT_MERGE20_GREP_HISTORY`）とパネルの検索文字列（`OPT_MERGE20_FILELIST_SEARCH`）は別キーであり、互いに上書きしない。
- **選択タブの復元値は 0〜7 にクランプする**。キーに古い値や手編集の値が入っていても、存在しないタブを選ばないため。
- **復元は OnCreate で LoadState の後・最初の Refresh の前に行う**。表示トグル（P0）と幅（LoadState）の復元と同じ位置づけで、最初のリストから復元済みの状態が見える。バーが非表示のまま始まる単独ファイル比較でも、メンバ状態だけは復元され、表示時の Refresh で反映される（Refresh は非表示時は no-op のため）。
- **新規 UI 文字列は 1 件のみ**。スリムボタンの "Slim view"（→「【Wiz】スリム表示」）。確認内容 5 状態・タブの区分名・ボタン類は既存 msgid の流用で、新規文字列は増やさない。なお "Slim List"（D9 のフォルダ一覧スリム）とは別 msgid である（表示名が似ているが対象が違うため流用しない）。

### 42.3 受け入れ条件の照合表（パネル全体。P0〜P3 の (a)〜(d) および P-R1〜P-R10）

| 条件 | 設計書の文言（要約） | 判定 | 根拠 |
|---|---|---|---|
| P4 (a) | パネルの表示状態・幅・スリム化・検索文字列が保存され、次回も復元されること | 満たす（コード）＋未検証（実機） | 表示状態は P0（`OPT_MERGE20_FILELIST_VISIBLE`、OnBarCheck で即保存）、幅は P0（SaveState／LoadState、160〜600px クランプ）、スリムは P4（専用キー＋トグルボタン）、検索文字列は P4（SavePosition で保存・OnCreate で復元）、折りたたみ・選択タブも P4。実機確認は §42.5 項目 1・2・3・4・5・7 |
| P4 (b) | 検証ゲート (a)〜(d) のすべてについて検証証拠を記録すること | 満たす（文書化） | 本節 §42.3（パネル全体の照合表）・§42.4（CI・GoogleTest・成果物・.po 監査）・§42.5（実機チェックリスト統合版）。コードで満たすものと実機確認待ちのものを区別して記録した。実機証拠はユーザーによる確認待ちであることを明記する |
| P4 (c) | 回帰なし | 満たす（コード）＋未検証（実機） | 既存の比較経路への変更は、フレームの OnCreate／SavePosition への呼び出し追加（各 1 か所）と、Ctrl の折りたたみ変更時の保存追加のみ。CI 緑＋GoogleTest 全パスが回帰なしの証拠。実機確認は §42.5 項目 38 |
| P0 (a) | ファイル比較フレームの左側に空のパネルがドッキング表示されること | 満たす（コード）＋未検証（実機） | §38.3。実機確認は §42.5 項目 10 |
| P0 (b) | フォルダ比較から開いた場合は一覧が表示され、単独ファイル比較の場合は「フォルダ比較がありません」と表示されること | 満たす（コード）＋未検証（実機） | §38.3。実機確認は §42.5 項目 11・12 |
| P0 (c) | 表示／非表示の切替が正しく動くこと | 満たす（コード）＋未検証（実機） | §38.3。P4 で保存・復元が完成。実機確認は §42.5 項目 1 |
| P0 (d) | 既存のファイル比較・フォルダ比較の回帰がないこと | 満たす（コード）＋未検証（実機） | §38.3。実機確認は §42.5 項目 38 |
| P1 (a) | パネルの列の表示値がフォルダ比較の一覧と一致すること | 満たす（コード。旧／変／新は暫定 1 列）＋未検証（実機） | §39.3。実機確認は §42.5 項目 14・15 |
| P1 (b) | 行色・タグ・強調がフォルダ比較と同じであること | 満たす（コード。リスク「高」の赤文字だけパネル独自の追加）＋未検証（実機） | §39.3。実機確認は §42.5 項目 16・17 |
| P1 (c) | 件数表示が正しいこと | 満たす（コード）＋未検証（実機） | §39.3。実機確認は §42.5 項目 18 |
| P1 (d) | 100 ファイル規模での描画が重くないこと | 満たす（コード）＋未検証（実機） | §39.3。実機確認は §42.5 項目 19 |
| P2 (a) | 検索で絞り込みができること | 満たす（コード）＋未検証（実機） | §40.3。保存・復元は P4 で追加（本表 P-R4 行参照）。実機確認は §42.5 項目 22 |
| P2 (b) | 展開／折りたたみが正しく動くこと | 満たす（コード）＋未検証（実機） | §40.3。保存・復元は P4 で追加（本表 P-R5 行参照）。実機確認は §42.5 項目 23・24 |
| P2 (c) | 一括チェックが S3 のダイアログで開き、正しく動くこと | 満たす（コード）＋未検証（実機） | §40.3。実機確認は §42.5 項目 25 |
| P2 (d) | フィルタタブが正しく動くこと | 満たす（コード）＋未検証（実機） | §40.3。保存・復元は P4 で追加（本表 P-R10 行参照）。実機確認は §42.5 項目 26 |
| P3 (a) | 「マージしつつ出力対象を決める動線が使えること」 | コード上は満たす（実機未検証） | §41.3。実機確認は §42.5 項目 30 |
| P3 (b) | フォルダ比較画面に戻ったときに、パネルでの変更が反映されていること | コード上は満たす（実機未検証） | §41.3。実機確認は §42.5 項目 33 |
| P3 (c) | 比較ウィンドウでのファイル移動にパネルの選択行が追従すること | コード上は満たす（実機未検証） | §41.3。実機確認は §42.5 項目 29 |
| P-R1 | 列（選択・ファイル名・対応区分・リスク・旧／変／新・確認内容） | 満たす（コード。旧／変／新は暫定 1 列。H-LIST 未実装） | §39.2／§39.3。内訳 3 列は残課題（§42.6） |
| P-R2 | 行の表示（チェックボックス・要判断タグ・リスク「高」の強調・未確認の表示。行色は S2 と同じ規則） | 満たす（コード） | §39.2。選択列はグリフ表示（操作は P3） |
| P-R3 | 一括チェック（S3 ダイアログ流用） | 満たす（コード） | §40.2 |
| P-R4 | 検索（ファイル名・フォルダ）。検索文字列は設定に保存して次回も復元する | 満たす（コード）＋未検証（実機） | P2 が UI と絞り込み、P4 が保存・復元（SavePosition／OnCreate）。実機は §42.5 項目 3・22 |
| P-R5 | 階層の展開／折りたたみ＋ディレクトリ単位操作 | 満たす（コード。ツリーモードへの切替はしない）＋未検証（実機） | P2 が方式（フラット＋折りたたみ集合）、P4 が折りたたみ状態の保存・復元（即保存＋SavePosition）。実機は §42.5 項目 5・23・24・27 |
| P-R6 | 件数表示（「表示 N 件中 M 件チェック」。S3 と同じ定義）。パネルのヘッダに表示する | 満たす（コード） | §39.2 |
| P-R7 | パネル側の操作とファイル比較の連動（行クリックで比較が開く・選択行の追従） | コード上は満たす（実機未検証） | §41.3。実機は §42.5 項目 28・29 |
| P-R8 | パネルから確認内容・出力を決められる | コード上は満たす（実機未検証） | §41.3。実機は §42.5 項目 30・31・32 |
| P-R9 | 表示／非表示の切替と設定の保存・復元 | 満たす（コード）＋未検証（実機） | 表示状態は P0、幅は P0（SaveState／LoadState）、スリム・検索は P4。設計書の想定した `OPT_MERGE20_SLIM`／`OPT_MERGE20_DOCK_HIDDEN` の流用はしない（§42.2 参照）。実機は §42.5 項目 1・2・6・7 |
| P-R10 | フィルタタブ＋H-FILTER との整理 | 満たす（コード。フォルダ側は H-FILTER へ）＋未検証（実機） | P2 がタブ、P4 が選択タブの保存・復元。実機は §42.5 項目 4・26。H-FILTER との整理は未着手（§42.6） |

### 42.4 検証レポート

- CI 実行 45853872508（head `b21c6f00`、PR #38）：結論 success。GoogleTest 723 テスト全パス（`[ PASSED ] 723 tests.`、`[ FAILED ]` 行なし。新規 Merge20FileListFilter 5 件すべて `[ OK ]`。P3 時点 718 件から＋5）。コンパイルエラー 0 件（`error C` 0 件、`Build succeeded`）。成果物：WinMerge-x64.zip 26,445,822 バイト（ID 15134708516、失効 2027-02-24T15:55:39Z）、WinMerge-x64-Setup-x64.exe 39,894,310 バイト（ID 15134663497、失効 2027-02-24T15:55:39Z）。全文ログは `~/workspace/.coord/p4_final_log.txt` に保存（47,579 行）。ログ中の "FAILED" 4 件は翻訳文字列 ID（IDS_FILESAVE_FAILED 等）で、テスト失敗ではない。CI の番兵「Merge20 strings present in the bundled Japanese.po.」も確認。警告はログ全体で 850 件（P2 時点 852 件と同水準。既存コード由来で、新規・変更ファイルに由来するものはない）。本記録（成功結果の追記）コミット後の CI 実行も別途確認すること。
- .po 監査：ブランチ版 msgid 2159 件 vs master 版 2158 件（＋1。新規 1 件）。【Wiz】127 件 vs master 版 126 件（＋1）。＋1 と ＋1 が一致し、新規文字列 1 件（"Slim view" → 「【Wiz】スリム表示」）と一致することを確認。ブランチ版・master 版とも API から取得して確認。CI の番兵は msgid の存在しか見ないため、件数監査で msgstr の欠落がないことを確認済み。
- ローカル検証：全変更ファイルの括弧バランス、resource.h の値ソートによる ID 重複チェック（1818 に重複なし。`_APS_NEXT_CONTROL_VALUE` 1818→1819。なお master 版に既存の IDC 値重複 1401〜1403／1430〜1431／1434〜1436 は別ダイアログ間の再利用で、P4 とは無関係）、新規 msgid "Slim view" のリテラル `_()` 呼び出し 2 か所（作成時と UpdateResources()）の確認、`GetOptionsMgr()`／`SaveOption`／`GetBool`／`GetInt`／`GetString` の使用前例（Merge20GrepDlg.cpp）の確認、純粋モジュール（Merge20FileListFilter）の MFC 非依存・ASCII の維持（改行リテラルは `L'\n'` のみ使用）、`SetColumnWidth`／`GetColumnWidth`／`GetHeaderCtrl` が CListCtrl の標準 API であることの確認、vcxproj の変更なし（新規 .cpp なしのため登録不要）。`Merge20FileListCtrl`／`Bar` は MFC のためこの VM ではコンパイルできない（CI で確認）。
- 未検証（Windows 実機でのみ確認できる挙動）：§42.5 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 42.5 実機確認チェックリスト（統合版、手順付き。未検証）

P0 §38.5・P1 §39.5・P2 §40.5・P3 §41.5 の全項目に P4 の保存・復元項目を統合したもの。P0〜P3 の各節のチェックリストは本節に統合される（重複する項目はここに一本化する）。実機で問題が出た場合は本節へ結果を追記すること。

1. **表示状態の保存・復元（P0＋P4）**：表示メニュー「【Wiz】ファイル一覧パネル」のチェックを外して終了→再起動→同じフォルダを開くと、パネルが閉じたままであること。チェックを入れて終了→再起動すると、表示されたままであること。バーの ✕ ボタンで閉じた場合も同じこと。
2. **幅の保存・復元とクランプ（P0＋P4）**：パネル幅をドラッグで変えて終了→再起動→同じフォルダを開くと、幅が戻っていること。極端に狭く／広くドラッグして終了→再起動すると、幅が 160〜600px の範囲に戻っていること（極端な幅が復元されないこと）。パネルがファイル表示領域を奪って比較画面が見えなくならないこと。
3. **検索文字列の保存・復元（P4、P-R4）**：検索欄に文字列（例：`tax`）を入れて絞り込んだ状態でファイル比較を閉じ、同じフォルダ比較から再度ファイル比較を開くと、検索欄に同じ文字列が入っており、一覧が絞り込まれたままであること。検索文字列を消して閉じ→開くと、絞り込みなしに戻っていること。
4. **フィルタタブの保存・復元（P4、P-R10）**：対応区分のタブ（例：「要マージ（旧欠落）」）を選んだ状態でファイル比較を閉じ、再度開くと、同じタブが選ばれており、一覧が絞り込まれたままであること。「すべて」に戻して閉じ→開くと、全行表示に戻っていること。
5. **折りたたみ状態の保存・復元（P4、P-R5）**：サブフォルダを含むフォルダ比較で、フォルダを折りたたんだ状態（または「すべて折りたたむ」）でファイル比較を閉じ、再度開くと、同じフォルダが折りたたまれたままであること。「すべて展開」して閉じ→開くと、全行表示に戻っていること。
6. **スリム表示の切替（P4、§12.4）**：パネルの「【Wiz】スリム表示」チェックを入れると、ファイル名と確認内容の列だけが表示され、検索行・フィルタタブ・ボタン行（「一括チェック…」など）が折りたたまれること。件数行は残ること。チェックを外すと、元の 6 列と上段の行が戻ること。スリム化の前後で、手動で広げた列の幅が元に戻ること（スリム解除時に既定幅にリセットされないこと）。
7. **スリム表示の保存・復元（P4）**：スリム表示 ON でファイル比較を閉じ、再度開くと、スリム表示のままであること（ボタンにチェックが入っていること）。OFF でも同様に復元されること。
8. **スリム中の操作（P4）**：スリム表示中に確認内容列をシングルクリックすると確認内容が巡回すること。出力列は隠れているため、出力チェックの切替は確認内容を「済」にすること（S2 の連動で ON になる）か、スリム表示を外すことで行えること。
9. **複数フレームでの保存（P4）**：同じフォルダ比較から 2 つのファイル比較を開き、それぞれで違う検索文字列・タブを設定する。後に閉じた方のフレームの状態が、次に開くファイル比較に復元されること（最後に閉じたフレームが勝つ、という仕様どおりであること）。
10. **パネルの表示（P0）**：実フォルダを比較し、一覧からファイルを開く。ファイル比較ウィンドウの左側にパネルがドッキング表示され、ファイル名の一覧が出ること。パネルが Location Pane の左にあること（左端）。パネル幅はドラッグで変えられること。
11. **一覧の内容（P0）**：パネルの一覧がフォルダ比較の一覧と同じファイル名・同じ順序であること（ディレクトリ行も含む）。
12. **単独ファイル比較の空表示（P0）**：2 ファイルを直接開く（コマンドラインまたはドラッグ＆ドロップ）。パネルは初期非表示であること。表示メニューから「【Wiz】ファイル一覧パネル」を ON にすると、パネルに「フォルダ比較がありません」の 1 行だけが表示され、一覧・操作ができないこと。検索欄・タブ・ボタン・スリムボタンも無効化されていること。
13. **通知による再描画（P0）**：フォルダ比較画面で確認内容を変更（キー 1〜5 など）したとき、開いているファイル比較のパネルがエラーなく表示され続けること。複数ファイルを開いている場合、最後に開いたフレームのパネルが最新になること（P1 で全フレーム同報に改善されたことを項目 21 でも確認する）。
14. **列の表示値の一致（P1）**：パネルの各行について、選択（☑／☐）・ファイル名・対応区分・リスク・確認内容が、フォルダ比較の一覧の同じ行の表示値と一致すること。未開封のファイルは対応区分・リスクが「未計算」、旧／変／新が「未計算」であること。ディレクトリ行はファイル名以外が空であること。
15. **旧／変／新の暫定列（P1）**：分類済みのファイルで、旧／変／新列が「欠落：N行 / 変更：M行 / 追加：K行」の形で表示され、N・M・K がフォルダ比較の一覧の要約列の先頭部分の数値と一致すること。3 列に分かれていないこと（暫定形であること）を了解のうえ確認すること。
16. **行色・タグ（P1）**：確認内容を「個別対応要」（SPECIAL）にした行が薄赤、「保留」（HOLD）にした行が薄黄、「現状維持」（KEEP）にした行が灰色になること。フォルダ比較の一覧の同じ行と同じ色であること。
17. **リスク「高」の強調（P1）**：リスクが「高」の行で、リスク列の文字が赤く表示されること（パネルだけの追加。フォルダ比較の一覧では赤くならないこと）。この強調が不要・不自然に見える場合は、実機確認時に判断すること（外すのは 1 行の変更）。
18. **件数表示（P1）**：件数行が「表示 N 件中 M 件チェック、未確認 K 件」と表示され、N・M・K がフォルダ比較のステータスバーの件数表示と一致すること。出力チェックを付け外しすると M が変わること。確認内容を「済」にすると K が減ること。ディレクトリ行は数えられていないこと。
19. **100 ファイル規模の描画（P1）**：100 ファイル程度のフォルダでファイル比較を開き、パネルの一覧表示・スクロールが重くないこと。パネルを開いたままフォルダ比較側で一括分類を実行し、パネルの更新が 1 回で済んでいること（ちらつき・固まりがないこと）。
20. **通知による再描画（セッション復元）（P1）**：ファイル比較を開いた状態で、セッションを復元する（確認内容・出力・分類が保存されたセッション）。開いているファイル比較のパネルが、復元後の確認内容・出力・分類で更新されること（パネルを閉じ開きし直さなくてもよいこと）。
21. **通知による再描画（複数フレーム同報）（P1）**：同じフォルダ比較から 2 つのファイル比較を開く。フォルダ比較画面で片方のファイルの確認内容を変更したとき、両方のファイル比較のパネルが更新されること。
22. **検索欄の表示と絞り込み（P2）**：パネル上部に検索ラベル＋入力欄が表示されること。ファイル名の一部（例：`tax`）を入力すると、パスにその文字を含むファイルだけが表示されること。大文字小文字を区別しないこと。フォルダ名で検索すると配下のファイルが表示されること。検索文字列を消すと全行に戻ること。件数行の N が絞り込み後の件数と一致すること。
23. **全展開／全折畳（P2）**：サブフォルダを含むフォルダ比較でファイル比較を開く。「すべて折りたたむ」を押すとディレクトリ行だけが残り、ファイル行が消えること。「すべて展開」を押すと全行が戻ること。件数行の N が変わること。
24. **ディレクトリ行の展開／折りたたみ（P2）**：ディレクトリ行をダブルクリックすると、そのフォルダ配下だけが折りたたまれる／展開されること。▾／▸ の表示が切り替わること。右クリックメニューの「フォルダを展開」「フォルダを折りたたむ」でも同じ動きになること。フォルダ比較側のツリー表示は変わらないこと（パネル側だけの状態であること）。
25. **一括チェック（P2）**：パネルの「一括チェック…」ボタンを押すと、フォルダ比較の右クリックメニューから開くのと同じダイアログが開くこと。ダイアログの件数がパネルの表示分と一致すること。ダイアログで「未確認」にチェックして OK すると、表示中の未確認ファイルの出力チェックが ON になること。検索で絞り込んだ状態での一括チェックは、表示中のファイルだけが対象になること（ただし「全解除」は比較全体が対象になること）。
26. **フィルタタブの絞り込み（P2）**：区分タブ（例：「要マージ（旧欠落）」）を選ぶと、その区分のファイルだけが表示されること。未分類（未開封）のファイルは区分タブでは表示されず、「すべて」で表示されること。ディレクトリ行は、表示される子孫を持つ場合に残ること。件数行の N が絞り込み後の件数と一致すること。検索とフィルタタブの併用では、両方の条件を満たすファイルだけが表示されること。
27. **ディレクトリ単位の操作（P2）**：ディレクトリ行を右クリックし、「配下をすべてチェック」を選ぶと、そのフォルダ配下の全ファイル（折りたたまれたサブフォルダ配下を含む）の出力チェックが ON になること。「配下のチェックをすべて外す」で OFF になること。フォルダ比較の一覧側でも同じ結果になること。
28. **行クリックで比較が開く（P3）**：パネルで別のファイルの行をシングルクリックする→そのファイルの比較が同じウィンドウに開くこと。現在の比較は閉じられること（MDI 子ウィンドウが増えないこと）。既に開いているファイルの行をクリックすると、そのウィンドウが活性化されること（重複して開かないこと）。現在開いているファイルの行をクリックすると、何も起きないこと。
29. **Next／Prev での選択行の追従（P3）**：比較ウィンドウで Next／Prev（ツールバーまたは専用バーの Prev／Next）でファイル移動する→パネルの選択行が移動先のファイルになること。ファイル・パス・バーからの切替でも追従すること。
30. **受け入れ条件 (a) の一連の動線（P3）**：パネルでファイルを選ぶ→差分を判定する（エディタで差分を確認する）→パネルで確認内容を「済」にする（キー 2 または確認内容列クリックの巡回、または右クリックメニューで「済」）→出力チェックが ON になること。ファイル比較ウィンドウから出ずに完結すること。
31. **確認内容の変更（キー 1〜5・Space・列クリック）（P3）**：パネルのリストにフォーカスを当て、キー 1〜5 を押す→確認内容がそれぞれ未確認／済／保留／個別対応要／現状維持になること。Space を押す→TODO→DONE→HOLD→SPECIAL→KEEP→TODO と巡回すること。確認内容列のシングルクリックでも同じ巡回をすること。検索エディットにフォーカスがある状態で '1' を入力した場合は検索文字列になること（確認内容が変わらないこと）。
32. **出力列クリックでのトグルと右クリックメニュー（P3）**：パネルで出力列をシングルクリックする→出力チェックが ON／OFF すること。パネルのファイル行を右クリックする→確認内容 5 状態（現在の状態にチェック付き）と「出力を切り替える」（ON ならチェック付き）のメニューが出ること。ディレクトリ行の右クリックは従来どおりのメニュー（折りたたみ／チェック／解除）が出ること。
33. **フォルダ比較画面への反映（P3）**：パネルで確認内容・出力を変更した後、フォルダ比較画面に戻る→該当行の表示（確認内容・出力チェック）が変わっていること。ステータスバーの件数表示が変更直後の値になっていること。一括チェック後のフォルダ側ステータスバー件数も変更直後の値になっていること。
34. **再スキャン後のパネル更新（P3）**：フォルダ比較で再スキャン（F5 等）を実行する→比較ウィンドウのパネルの一覧が新しい比較結果に更新されること（次の活性化まで古いままにならないこと）。
35. **閉じたフレームの通知（P3）**：同じフォルダ比較から複数の比較ウィンドウを開いて閉じてを繰り返す→パネルの表示が壊れないこと（目視での確認）。
36. **片側ファイル（P3）**：旧のみ／新のみのファイルの行をクリックする→そのファイルの比較が開くこと。そのファイルを開いた状態でパネルの選択行がそのファイルになっていること。
37. **専用モードとの独立性（P0）**：パネルを表示したまま専用モードに入ると、パネルは表示されたままであること（5 バーだけが隠れること）。専用モードを出るとパネルの表示状態が入る前と同じであること。パネルを隠したまま専用モードに入って出ても、パネルは隠れたままであること。なお、専用モード入場時にパネルが自動でスリム化されることはない（P4 の範囲外。§42.6 参照）。
38. **回帰なし（P0〜P3＋P4）**：ファイル比較（2 ファイル）・フォルダ比較（2 フォルダ）が従来どおり動くこと。確認内容の変更・出力チェック・一括チェック・CSV 出力・セッション保存／復元・一括分類・専用モード・スナイパー・フォーカスドック・Grep が従来どおり動くこと。

### 42.6 パネル完成時点の残課題一覧

- 旧／変／新の内訳 3 列（H-LIST）は未実装のまま（P1 の暫定 1 列のまま）。H-LIST の着手時に `Merge20CsvGetOldChangedNewText` を 3 列分の getter に置き換え、ヘッダ msgid `"Old/Changed/New"` を廃止する（.po の訳もあわせて整理する）。
- H-FILTER との整理は未着手（P-R10 の方向性：フィルタデッキをパネル側に寄せ、フォルダ比較画面のデッキは残さない方向で検討する）。P4 の選択タブはパネル内のメンバ変数に閉じており、同期が必要になった場合は公開 API を追加する。
- 専用モード入場時のパネル自動スリム化（設計書 §12.6）は未実装。スリムは手動トグルのみ。入場前の状態の記憶と退場時の復元を含めて、専用モード側の着手時に決める。
- 実機確認待ち（§42.5 の全項目。特に P4 の保存・復元項目 1〜9 と P3 の動線項目 30）。
- 複数フレームで最後に閉じたフレームの状態が勝つ（仕様。幅の保存と同じ規則）。
- 検索文字列は SavePosition での保存のみのため、クラッシュ時は失われる可能性がある（タブ・折りたたみ・スリムは変更直後に保存されるため残る）。
- スリム表示中は出力列が隠れるため、出力チェックの切替は確認内容の変更（S2 の連動）かスリム解除による（§42.2 参照）。
- 折りたたみパスの区切り文字（改行）を含むファイル名は理論上分割される（Windows のファイル名に改行は実用上含まれないため、対処しない）。
- 列幅の永続化はない（P0 以来の仕様。バー幅のみ保存される。スリム往復での列幅復元はメモリ内だけのもの）。
- P3 で直さないと記録したもの：`CDirView::GetItemIndex` の未発見時 0 返却（upstream の癖）、`Merge20OpenFileAtLine`（Grep ジャンプ、D7）の片側のみファイルでの黙った失敗。
- P3 の `m_sCurrentRelPath`（比較ウィンドウのファイル）は復元対象ではない（フレームごとに解決する）。`m_sPendingOpenRelPath` は一時的な受け渡し用メンバであり、保存対象ではない（§41.6 参照）。

## 43. H-FILTER — フィルタデッキ（カバレッジ行列 H-11・H-13・H-39）— ブランチ `merge20/h-filter`

### 43.1 変更一覧（1 ファイル 1 コミット、23 件＋本節の記録コミット。連続コミットの間に sleep 3）

| # | コミット | ファイル | 内容 |
|---|---|---|---|
| 1 | `71b59a6` | Src/Merge20/Merge20FilterDeck.h | 純粋モジュール：デッキ状態・可視判定・プリセット・HTML 連動の宣言 |
| 2 | `d29ddec` | Src/Merge20/Merge20FilterDeck.cpp | 同・実装 |
| 3 | `cfa1663` | Testing/GoogleTest/Merge20/Merge20FilterDeck_test.cpp | 単体テスト 10 件 |
| 4 | `434445e` | Src/OptionsDef.h | オプションキー 12 件（存在区分 ×4、対応区分 ×7、バー表示） |
| 5 | `9171cc4` | Src/OptionsInit.cpp | 同・既定値（すべて表示、バー表示） |
| 6 | `93de81c` | Src/DirActions.h | `DirViewFilterSettings` に `show_status[4]`／`show_action[7]` を追加 |
| 7 | `3034d2a` | Src/DirActions.cpp | `IsShowable` にデッキ適用＋フォルダ子スキャンの一般化 |
| 8 | `23d05a5` | Src/Merge20/Merge20Session.h | `Merge20SessionDisplay` に `statusFilter[4]`／`actionFilter[7]` を追加 |
| 9 | `54d2e17` | Src/Merge20/Merge20Session.cpp | セッション JSON にデッキ状態を保存・復元（後方互換） |
| 10 | `81b2174` | Testing/GoogleTest/Merge20/Merge20Session_test.cpp | セッションテスト拡張（往復＋後方互換） |
| 11 | `6f91df2` | Src/DirView.h | `Merge20RefreshFilterDeck`／`Merge20SyncFilterDeckBar` の宣言 |
| 12 | `0c9b803` | Src/DirView.cpp | デッキの再読込・同期、セッション保存・復元への配線 |
| 13 | `4b029de` | Src/Merge20/Merge20FilterDeckBar.h | デッキバー（`CMerge20FilterDeckBar`）の宣言 |
| 14 | `552866c` | Src/Merge20/Merge20FilterDeckBar.cpp | 同・実装（チェックボックス 16 個、プリセット、すべて表示） |
| 15 | `9073893` | Src/DirFrame.h | デッキバーのメンバ・アクセサ・`OnBarCheck` の宣言 |
| 16 | `0d5afa6` | Src/DirFrame.cpp | デッキバーの生成・ドック・表示切替・保存 |
| 17 | `d7f5ff8` | Src/resource.h | リソース ID 割当て（コントロール 1819–1836、コマンド 38968） |
| 18 | `35e84f9` | Src/Merge.rc | フォルダ比較の表示メニューに「Filter Deck」を追加 |
| 19 | `91f0dc2` | Src/Merge.vcxproj | 新規 2 ファイルの登録（純粋モジュールは pch.h グループ、バーは StdAfx.h グループ） |
| 20 | `ce56592` | Src/Merge.vcxproj.filters | 同・フィルタ登録 |
| 21 | `559a3e6` | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | 純粋モジュール＋テストの登録 |
| 22 | `d75bc34` | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同・フィルタ登録 |
| 23 | `47ea67a` | Translations/WinMerge/Japanese.po | 新規文字列 6 件の日本語訳（【Wiz】付き） |
| 24 | — | MERGE20_NOTES.md | 本節の記録コミット |

### 43.2 設計判断（判断理由付き）

**（1）デッキの置き場所：フォルダ比較フレーム（`CDirFrame`）の上部ドックに置く。パネル側に寄せない。** 設計書 §12.3 P-R10 は「パネルができた後は、フィルタデッキはパネル側に寄せ、フォルダ比較画面のフィルタデッキは残さない方向で検討する（H-FILTER の着手時に決める）」としており、着手時の決定は「残す」である。理由は 3 点。(a) デッキが絞り込むのはフォルダ比較の一覧であり、パネルはファイル比較ウィンドウ内にしか存在しない。パネル限定のデッキは、ファイル比較を開いていない主作業（フォルダ比較で出力対象を決める）では使えない。(b) 受け入れ条件の件数表示「表示 N 件中 M 件チェック」は S3 のフォルダ一覧の文言であり、デッキの効果はフォルダ一覧で測るのが筋である。(c) パネルのタブはパネル一覧専用の単一選択フィルタとして P2 で完成済みであり、デッキをパネルに寄せると「単一選択タブ」と「複数選択デッキ」の 2 つの対応区分フィルタが同じ一覧に並ぶことになる。フォルダ比較画面のデッキ＋ファイル比較画面のパネルタブという分担のほうが重なりが小さい。P-R10 の方向性に従わなかった判断であり、理由をここに記録する。

**（2）D11 の「【Wiz】確認内容で表示」サブメニューは残す。デッキの確認内容グループと統合は「状態の統合」で行い、UI は 2 つ残す。** サブメニューとデッキのチェックボックスは同じ 5 つのキー（`OPT_MERGE20_SHOW_REVIEW_*`）を読み書きする。保存される状態は 1 つであり、重複する保存はない。操作口が 2 つあるのは意図的である：サブメニューは一覧内での素早い切替、デッキは 3 種のフィルタを一覧できる常設の概観である。両者が食い違うことがないよう、サブメニューでの切替・セッション復元の直後に `CDirView::Merge20SyncFilterDeckBar()` でデッキのチェックボックスへ状態を押し戻す（`DirView.cpp`：`OnMerge20ShowReview` 末尾、`Merge20SetDisplayFilter` 末尾）。

**（3）パネルの対応区分フィルタタブはパネル内のみとし、デッキと同期しない。** タブは単一選択（「すべて」＋対応区分 7 種）でパネル一覧だけを絞り込む（P2、`OPT_MERGE20_FILELIST_TAB`）。デッキの対応区分グループは複数選択でフォルダ一覧を絞り込む。対象の一覧が違い、選択の型も違うため、同期には「複数選択を単一選択タブにどう写すか」という未定義の写像が必要になる。同期しないことを仕様とし、パネルはデッキに従わない（§43.6 の残課題）。タブとデッキが同じ一覧を二重に絞ることはない（タブはパネル一覧、デッキはフォルダ一覧）ため、実害はない。

**（4）存在区分フィルタは WinMerge 本体の比較結果フィルタとは別物にする（H-11 の記述どおり）。** デッキの存在区分は新規キー（`OPT_MERGE20_SHOW_STATUS_*`）で保持し、本体の `OPT_SHOW_DIFFERENT`／`OPT_SHOW_UNIQUE_*`／`OPT_SHOW_IDENTICAL` には一切書き込まない。`IsShowable` では両方が AND で適用される（本体フィルタで隠れたものはデッキでは復活しないし、その逆もない）。本体フィルタが表示メニューにあるのに対し、デッキは 4 区分を 1 か所に並べたものであり、HTML の `selSt`（複数選択可）に対応する。

**（5）既定値はすべて ON（除外フィルタ）とする。HTML の既定値（同一を隠す）とは違う。** HTML の既定は `selSt` から IDENTICAL を除き、`selAc` から NO_ACTION_IDENTICAL を除く（同一ファイルを隠す）。フォークでは本体の `show_*` フィルタ・D11 の確認内容フィルタと同様にすべて表示を既定とした。理由：既存のフォルダ比較を開いた直後の見え方を変えないこと。同一ファイルを隠したい場合はデッキの「同一」1 クリックで HTML と同じ状態になる。

**（6）未分類ファイルは対応区分の次元を通過する。パネルのタブとは意図的に違う。** S4 の分類はファイルを開いたときにだけ走る（段階 1）ため、比較直後の大半のファイルは未分類である。未分類を対応区分フィルタの対象にすると、対応区分を 1 つでも外した途端に一覧が開封済みファイルだけに崩れる。デッキは除外フィルタ（「隠すもの」を選ぶ）なので、判断がまだのファイルを隠してはならない。一方パネルのタブは包含フィルタ（「これだけ見せる」を選ぶ）なので、未分類はどのタブにも一致しない（P2 の仕様）。同じ「未分類」でもフィルタの型が違うための違いであり、バグではない。プリセット（要対応のみ抽出）でも未分類ファイルは残る。プリセットボタンの動作としては「要対応の 4 区分＋未分類を表示する」になることをここに記録する。

**（7）「要対応のみ抽出」プリセットは HTML の `acPreset` と同一の定義にする。** 存在区分 {変更、旧のみ}、対応区分 {要マージ（旧欠落）、要判断（欠落＋変更）、要判断（変更のみ）、旧のみファイル}。確認内容グループは触らない（HTML の `acPreset` も `selRev` に触らない）。純粋モジュールの `Merge20FilterDeckApplyActionPreset` に切り出し、単体テストで HTML の定義と 1 件ずつ照合した。

**（8）HTML の `tgSt`／`tgAc` の連動は「追加のみ」で移植する。** 存在区分を ON にすると対応する対応区分も ON になり（同一→同一、旧のみ→旧のみファイル、新のみ→新のみファイル。変更には対応する区分が 4 つあるため追加なし）、対応区分を ON にすると対応する存在区分も ON になる（それ以外→変更）。OFF にしても相手は外れない（HTML もそうなっている）。連動は純粋モジュール（`Merge20FilterDeckSyncStatusOn`／`Merge20FilterDeckSyncActionOn`）に切り出し、単体テストで照合した。デッキバーはチェックボックスの変化をコントロールから読み、連動を適用してからキーへ書き戻す。

**（9）HTML の `spOnly`（個別対応要のみ抽出）には専用のコントロールを作らない。** 確認内容グループで未確認・済・保留・現状維持の 4 つを外せば個別対応要のみが残り、同じ結果になる。専用トグルは重複になるため作らない。

**（10）3-way 比較での存在区分は近似とする。** デッキの 4 区分は HTML の 2-way モデルである。3-way では左のみ→旧のみ、右のみ→新のみ、同一→同一とし、中間のみ（2nd only）・欠落系（missing-*）はすべて変更に畳む。差分であるものを「変更」以外に振り分けて隠すことがないよう、畳む先は変更とした。3-way でのデッキの扱いは実機確認項目に入れる（§43.5 項目 11）。

**（11）保存・復元は 3 層に分ける（D11 の方式を踏襲し、重複する保存を作らない）。** (a) デッキの状態（16 個のチェック）はグローバルオプションに変更直後に保存する（本体の `show_*`・D11 の確認内容フィルタと同じ）。(b) フォルダペアごとのセッションには表示設定として保存する（`Merge20SessionDisplay` の `statusFilter[4]`／`actionFilter[7]`。`reviewFilter[5]` と同じ配列方式。H-FILTER 以前のセッションファイルにはキーがなく、その場合は既定値（すべて表示）のまま読めることを単体テストで確認）。セッション復元時は `Merge20SetDisplayFilter` がキーへ書き戻し、デッキバーのチェックボックスへも同期する。(c) バーの表示／非表示は専用キー `OPT_MERGE20_FILTERDECK_VISIBLE`（P0 の方式。`OnBarCheck` で保存）、バーの高さは `Settings-DirFrame` にバーの `SaveState`／`LoadState` で保存する（DedBar の方式）。デッキの状態をセッションとは別に保存する経路はない。

**（12）レイアウトは 2 行固定・高さ 52px とする（DedBar の方式）。** 1 行目＝存在区分＋対応区分＋プリセットボタン、2 行目＝確認内容＋すべて表示ボタン。幅は表示文字列から実測する。2 行は幅 1024px 程度で収まる想定であり、それより狭いウィンドウでは 1 行目の右端がクリップされることがある（既知の制限。バーは表示メニューから隠せる）。高さは `LoadState`／`CalcFixedLayout` で固定し、保存された異常値やドラッグで一覧の領域を奪わない（D1・D2 の教訓）。

**（13）フォルダ行の扱いは D11 の方式を一般化する。** ファイル行には 3 次元すべてを適用する。フォルダ行自体にはデッキを直接適用せず、ツリーモードでは「可視の子が 1 つもないフォルダを隠す」スキャンをデッキ全体に一般化した（D11 の `IsReviewFilterActive` を `IsMerge20FilterActive` に置き換え）。既定（すべて表示）ではスキャンは走らないため、既定のコストは変わらない。非再帰比較のフォルダ行にはデッキの存在区分を適用しない（§43.6 の既知の制限）。再帰比較のフラットモードではフォルダ行自体が一覧に出ないため対象外である。

**（14）HTML の `selRev` の二重の役割（「一括チェックの対象」と「一覧の絞り込み」）のうち、デッキが担うのは一覧の絞り込みのみとする。** 一括チェックの対象選択は S3 の `Merge20BatchCheckDlg` が担っており、ダイアログは「表示中のファイル」を対象とする。デッキで絞った一覧がそのまま一括チェックの対象になるため、動線としてはつながっている。デッキに一括チェックの実行ボタンは置かない（S3 ダイアログと重複するため）。

### 43.3 カバレッジ行列の照合表（H-11・H-13・H-39 を 1 件ずつ確認）

| 条項 | HTML 版の機能 | フォークでの対応 | 判定 |
|---|---|---|---|
| H-11 | 存在区分（`selSt`：MODIFIED／OLD_ONLY／NEW_ONLY／IDENTICAL）の表示・フィルタ。デッキで複数選択可 | 表示名は S0 で実装済み。フィルタは H-FILTER で追加：デッキの存在区分グループ（4 個のチェックボックス、複数選択可）が `IsShowable` で一覧を絞り込む。本体の比較結果フィルタとは別のキーで保持し、AND で適用する（§43.2 (4)）。既定値はすべて表示（HTML の既定とは違う。§43.2 (5)） | 満たす |
| H-13 | 対応区分フィルタ（`selAc`・`tgAc`）＋「⚡要対応のみ抽出」プリセット（`acPreset`：要マージの 4 区分だけ残す） | デッキの対応区分グループ（7 個のチェックボックス、複数選択可）＋プリセットボタン。プリセットの定義は HTML の `acPreset` と同一（§43.2 (7)）。`tgAc` の連動は追加のみで移植（§43.2 (8)）。未分類ファイルは対応区分の次元を通過する（§43.2 (6)）。パネルのタブはパネル内のみで同期しない（§43.2 (3)） | 満たす |
| H-39 | 確認内容フィルタ（`selRev`・`revFilter`／`revFilterPop`・`syncRevFilter`）。一括チェックの対象として選択中の確認内容（複数選択可）。一覧の絞り込みにも使う | 一覧の絞り込みは H-FILTER で追加：デッキの確認内容グループ（5 個のチェックボックス、複数選択可）が D11 のキー（`OPT_MERGE20_SHOW_REVIEW_*`）を読み書きし、`IsShowable` で絞り込む。D11 のサブメニューとは同一状態の 2 つの操作口とし、同期する（§43.2 (2)）。一括チェックの対象選択は S3 ダイアログが担い、デッキで絞った表示中のファイルが対象になる（§43.2 (14)）。HTML のポップアップ UI（`revFilterPop`）は再現せず、デッキのチェックボックス群と S3 ダイアログに分けた（S3 の判断と同じ理由：標準メニューは項目選択で閉じるため、複数選択＋件数確認＋実行と等価にならない） | 満たす |
| P-R10 | フィルタデッキとパネルのフィルタタブの UI の重なりの整理（設計書 §12.3） | 結論：デッキはフォルダ比較画面に残し、パネルタブはパネル内のみとする（§43.2 (1)(3)）。重複する保存はなく、対象の一覧も違う。パネルがデッキに従うことは将来の残課題（§43.6） | 整理済み（方針を記録） |

### 43.4 検証レポート

- ローカル g++ ハーネス：純粋モジュール `Merge20FilterDeck` の主要分岐（既定で全表示、存在区分・対応区分・確認内容の各 OFF、3 次元の AND、プリセット、連動、範囲外、すべて表示）をスタブハーネスで実行し、全合格。単体テスト 10 件と同じ観点を先に通した。
- 単体テスト：新規 `Merge20FilterDeck_test.cpp` 10 件（可視判定、プリセット、連動、範囲外）＋セッションの往復・後方互換の拡張。CI での実行結果は PR の CI 実行で確認する（下記）。
- .po 監査：master 版は msgid 2159 件・【Wiz】 127 件。ブランチ版は msgid 2165 件・【Wiz】 133 件。増分は +6 件ずつで、新規文字列 6 件（Filter Deck、Existence:、Action category:、Review state:、Only files needing action、Show all）と一致する。チェックボックスの表示名は既存 msgid（S0 の存在区分名、S4 の対応区分名、D11 の確認内容名）を再利用しており、追加はない。新規 6 件の msgstr に空欄はない。
- リソース ID 監査：`resource.h` の全 define を値でソートし、コントロール 1819–1836・コマンド 38968 が空きであることを確認してから割り当てた（`_APS_NEXT_*` の指す番地と一致）。`_APS_NEXT_CONTROL_VALUE` は 1837、`_APS_NEXT_COMMAND_VALUE` は 38969 に更新した。
- プロジェクト登録監査：新規 2 ファイルを `Merge.vcxproj`（純粋モジュールは pch.h グループ、バーは StdAfx.h グループの既定エントリ）と `UnitTests.vcxproj`（純粋モジュール＋テスト）の両方に登録し、`.filters` も更新した。4 ファイルとも XML として構文検証済み。
- CI（PR #39、最終コード head `3ae16229`）：実行 45864682922、completed/success。コンパイルエラー 0 件、Build succeeded。GoogleTest は `[  PASSED  ] 733 tests.`（P4 時点 723 件＋新規 10 件）、`[  FAILED  ]` 行なし。新規 `Merge20FilterDeck_*` 10 件とセッション往復・後方互換テストはすべて OK。成果物は API で存在確認済み：`WinMerge-x64.zip`（26,453,029 バイト、artifact ID 15140882013）、`WinMerge-x64-Setup-x64.exe`（39,898,524 バイト、artifact ID 15140642266）、有効期限はどちらも 2027-02-24T17:05:30Z。
- CI 1 回目（実行 45863017438、head `b5743c1`）は failure：`Merge20FilterDeckBar.cpp(488)` で error C2248（`CDirView::Merge20RefreshFilterDeck` が protected のためバーから呼べない）。宣言を public へ移す修正（`3ae16229`）で解決し、2 回目で success になった。
- 作業メモ：作業中に VM のサービス再起動で `/tmp` が消去され、未コミットのセッションテスト編集 1 件を失った（GitHub のコミット済み分は無事で、再取得して編集し直した）。以後、作業ファイルは永続領域（`~/workspace/h-filter-work/`）に置くこと。

### 43.5 実機確認チェックリスト（手順付き。未検証）

Windows 実機でしか確認できない挙動はすべて未検証である。「できた」とは言わない。以下の手順で確認すること。

1. デッキバーの表示：フォルダ比較を開く→上部にデッキバー（2 行）が表示される。「表示」メニューの「Filter Deck」のチェックと連動する。隠す→再表示するとチェックボックスの状態が残る。
2. 存在区分：デッキの「同一」のチェックを外す→同一ファイルが一覧から消える。ステータスバーの「表示 N 件中 M 件チェック」の分母が減る。チェックを戻すと復活する。
3. 対応区分（未分類）：比較直後（大半が未開封）の状態で「旧のみファイル」のチェックを外す→分類済みの旧のみファイルは消えるが、未開封のファイルは残る。ファイルを 1 つ開いて分類済みにしてから外すと、そのファイルも消える。
4. プリセット：「⚡ 要対応のみ抽出」を押す→残るのは変更・旧のみのファイルで、対応区分は要マージの 4 区分のみになる。確認内容のチェックは変わらない。未分類のファイルは残る。
5. すべて表示：プリセットの後に「すべて表示」を押す→全 16 個のチェックが ON に戻り、一覧が元に戻る。
6. 確認内容の同期：デッキの「済」のチェックを外す→済のファイルが消える。右クリックメニューの「【Wiz】確認内容で表示」→「済」を押す→デッキの「済」にもチェックが戻る。逆（デッキ→サブメニューのチェック表示）も確認する。
7. 連動：デッキの「同一」のチェックを外してから ON にする→対応区分の「同一」にもチェックが入る。「旧のみファイル」を ON にする→「旧のみ」にもチェックが入る。OFF にしても相手は外れない。
8. セッション：デッキで絞った状態で「セッションを保存」→ WinMerge を終了→再起動して同じフォルダ比較を開き、セッションを復元する→デッキのチェック状態と一覧の絞り込みが復元される。
9. ツリーモードとフラットモード：ツリーモードでフォルダをたたんだ状態で絞り込む→子がすべて非表示のフォルダ行も消える。フラットモードでも件数が合う。
10. 件数整合：絞り込み前後で「表示 N 件中 M 件チェック」の分母・分子が表示行と一致する。一括チェックダイアログを開き、表示される件数が絞り込み後の表示件数と一致する。
11. 3-way 比較：3 フォルダ比較でデッキが表示され、操作で落ちない。中間のみ・欠落のファイルが「変更」扱いで表示される。
12. 幅：ウィンドウ幅 1024px でデッキの右端（プリセットボタン）まで見える。幅 800px では右端がクリップされる（既知の制限。表示メニューからバーを隠せる）。
13. バーの表示状態の保存：デッキバーを隠して WinMerge を終了→再起動→フォルダ比較を開く→バーが隠れたままになる。表示メニューから表示し直せる。

### 43.6 パネル完成時点の残課題一覧（H-FILTER 終了時点）

- パネルがデッキに従わない（§43.2 (3)）。パネル一覧にデッキ状態を適用する場合は `Merge20ComputeFileListVisibility` の拡張（存在区分・確認内容の次元追加）と、フォルダ比較側からの通知が必要になる。P4 の選択タブはパネル内のメンバ変数に閉じており、同期が必要になった場合は公開 API を追加する。
- 非再帰比較のフォルダ行にデッキの存在区分が適用されない（§43.2 (13)）。非再帰ではフォルダ行が一覧の行でありながら子を持たないため、子スキャンの対象にならない。フォルダ行への直接適用はツリーモードとの整合を別途設計する必要がある。
- ウィンドウ幅 1024px 未満でデッキバー 1 行目の右端がクリップされることがある（§43.2 (12)）。
- 実機確認待ち（§43.5 の全項目。特にプリセット項目 4、同期項目 6、セッション項目 8、件数整合項目 10）。
- H-LIST（旧／変／新の内訳 3 列）は未着手。デッキとは独立しており、H-FILTER の範囲外である。
- D11 のサブメニューとデッキの二重操作口は意図的なもの（同一キー）。将来デッキが十分に定着すればサブメニューの存廃を検討してよい。
- 3-way 比較での存在区分の畳み込み（中間のみ・欠落→変更）は近似である（§43.2 (10)）。実ファイルでの違和感があれば、3-way 専用の区分を別途検討する。

## 44. H-LIST — 旧／変／新の内訳3列（カバレッジ行列 H-27・H-LIST）— ブランチ `merge20/h-list`

### 44.1 変更一覧（1 ファイル 1 コミット、6 件＋本節の記録コミット。連続コミットの間に sleep 3）

| # | コミット | ファイル | 内容 |
|---|---|---|---|
| 1 | `bd230623` | Src/DirViewColItems.h | 新規ラッパー 3 件の宣言（`Merge20CsvGetMissingText`／`Modified`／`Added`）。暫定 `Merge20CsvGetOldChangedNewText` の宣言を削除 |
| 2 | `bec80fee` | Src/DirViewColItems.cpp | 新列 3 件のヘッダ・説明文・getter・ソート・テーブル行（2-way／3-way）、ラッパー 3 件、暫定ラッパーの削除 |
| 3 | `8d96d9d3` | Src/Merge20/Merge20FileListCtrl.h | パネル 8 列化：enum・Row の 3 列化、コメント更新 |
| 4 | `0a74e747` | Src/Merge20/Merge20FileListCtrl.cpp | パネル 8 列の表示・幅・ヘッダ・Refresh・OnGetdispinfo の実装 |
| 5 | `eb811444` | Translations/WinMerge/Japanese.po | 暫定 msgid `"Old/Changed/New"` の廃止、新規列説明文 3 件の追加 |
| 6 | `1e3e726b` | Src/Merge20/Merge20FileListCtrl.cpp | CI 対策：幅配列の `[COL_COUNT]` をリテラルの `[8]` に修正（§44.4 参照） |
| 7 | — | MERGE20_NOTES.md | 本節の記録コミット |

注：作業中に空コミットを 1 件作ってしまった（編集スクリプトの失敗に気づかず `DirViewColItems.cpp` の未変更版をコミットした。コミット `1fb8c46c`）。ブランチは未 PR だったため API の ref 更新（force）で当該コミットを除去し、head を `bd230623` に戻してから作業を続けた。履歴には残っていない。

### 44.2 設計判断（判断理由付き）

- **列ヘッダは CSV の msgid を流用し、【Wiz】は付けない**。3 列のヘッダは `"Missing lines"`／`"Changed lines"`／`"Added lines"`（→ 旧欠落／変更／新追加）。これらは S6 の CSV ヘッダ行（`Merge20CsvHeaderLine`）が使う msgid と同一であり、流用により一覧のヘッダと CSV のヘッダが常に一致する。同じ msgid に【Wiz】を付けると出力 CSV のヘッダ行にも【Wiz】が出てしまうため付けない。列が Merge20 の列であることは regName（`Smissing`／`Smodified`／`Sadded`）と、隣り合う【Wiz】付き列（対応区分・リスク・要約）で分かる。パネル側のヘッダも同じ 3 msgid を流用する（P1 の流用方針どおり）。
- **新規 msgid は列説明文（`COLDESC_*`）の 3 件だけ**。説明文は列選択ダイアログに表示される文言で、W1 が他の `COLDESC_*` の訳文に【Wiz】を付けていない前例（例：`"Action category of the file..."` → 【Wiz】なし）に倣い、素の日本語訳とした。
- **既定では非表示（両テーブルとも既定表示 -1）**。H-27 の記述どおり。理由：3 数は要約列の先頭部分に既に含まれており、一覧は十分に広いため。
- **既存ユーザーへの影響なし（W1 の移行の扱い）**。`MigrateMerge20Columns()` は既定表示の列（`GetColDefaultOrder >= 0`）だけを保存済みレイアウトへ追加するが、新列は既定 -1 で、そもそも移行対象の 5 列の固定リストにも含まれない。よって保存済みレイアウトを持つ既存ユーザーに新列が勝手に表示されることはない。`Merge20/ColumnLayoutVersion` の引き上げも不要（バージョン 1 の移行は新列を見ない）。新規ユーザー（保存済みレイアウトなし）も既定 -1 のため表示されない。列選択ダイアログからは出せる。
- **ソートは数値の昇順、未分類は最後**。S4 の他列（対応区分・リスク・要約）のソートと同じく、未分類（未開封）ファイルは最後に寄せる。既定のソート方向は降順（`defSortUp=false`）で、他の差分数列（`Snsdiffs`／`Snidiffs`／`Sexcluded`）の前例どおり（差分の多い順が先頭）。
- **表示値は要約文・CSV と同じ 3 フィールド**。3 列の getter は `Merge20FileState` の `nMissing`／`nModified`／`nAdded` を読む。CSV（`Merge20CsvExportToFile`）は同じ 3 フィールドを裸の数字で書き、要約文（`Merge20FormatSummary`）は同じ 3 フィールドを `"Missing: %1 lines / Changed: %2 lines / Added: %3 lines"` の書式で先頭に置く。表示値の一致は構造で保証される。書式だけが違う：フォルダ比較の一覧とパネルは `locality::NumToLocaleStr`（ロケールの桁区切り。S4 の要約文と同じ）、CSV は Excel で数値として読めるよう裸の数字（S6 の規則）。桁区切りの有無を除けば数値は一致する。
- **パネルも 3 列化する（P1 の暫定 1 列は廃止）**。P1 §39.6 の引き継ぎどおり、`Merge20CsvGetOldChangedNewText` を 3 列分の getter に置き換え、ヘッダ msgid `"Old/Changed/New"` を廃止した（.po の訳もあわせて削除）。理由：(a) P1 の引き継ぎが置き換えを明記している。(b) 3 つの数値列（各 64px＝計 192px）は暫定 1 列（220px）より狭く、パネル全体は 742px→714px とむしろ狭くなるため、パネルが広がる心配がない。(c) パネルの各列がフォルダ比較の一覧の新列と同一の getter を使うため、表示値の一致が構造で保証される（P1 の方針どおり）。暫定 1 列を残す案は、ヘッダ msgid が他に存在しない（`"Old/Changed/New"` 以外に 1 列用の msgid がない）ため採らなかった。
- **パネルのクリック振り分けは変わらない**。バーは `COL_OUTPUT`（出力トグル）と `COL_REVIEW`（確認内容の循環）だけを特別扱いし、それ以外の列はファイルを開く（`FileListBar.cpp`）。新 3 列は「その他」に落ちるため、暫定 1 列のときと同じくクリックでファイルが開く。`COL_REVIEW` の列番号は 5→7 にずれるが、名前付き enum で参照しているため影響はない。スリム表示（ファイル名＋確認内容のみ）も名前参照のため影響はない。
- **単体テストは新規追加なし**。列 getter は `DirViewColItems.cpp` にあり、UnitTests ビルドに含まれていない。UnitTests へ同ファイルを追加するには `I18n.cpp`・`FilterExpression.cpp` 等の追加リンクが必要になり、S1/S2/S4/S6 の getter（いずれも単体テストなし）に対して過大な変更になるため行わない。表示値の一致は、同じ 3 フィールドを読む構造＋静的整合性チェック（§44.4）＋実機チェックリスト（§44.5）で担保する。分類そのものの単体テスト（`Merge20Classify_test`）と CSV の書式テスト（`Merge20Csv_test`）は既存のまま全パスする。
- **HTML 版との対応**。HTML（`17_v17.15.html`）の定義は `dc`＝削除行（`rt==='del'`）、`mc`＝変更行（`rt==='mod'`）、`acnt`＝追加行（`rt==='add'`）で、要約文は「欠落：`${dc}`行 / 変更：`${mc}`行 / 追加：`${acnt}`行」。フォークの `nMissing`／`nModified`／`nAdded` は S4 の分類で `OP_1STONLY`／`OP_DIFF`／`OP_2NDONLY` を数えたもので、同じ定義である（NOTES §8.2 参照）。HTML の左一覧は旧／変／新を 3 列に分けて表示しており（H-27）、本スライスでフォークも 3 列に揃った。

### 44.3 カバレッジ行列の照合表（H-27・H-LIST を 1 件ずつ確認）

| 条項 | HTML 版の機能 | フォークでの対応 | 判定 |
|---|---|---|---|
| H-27 | 左一覧の列（選択・ファイル名・対応区分・リスク・旧／変／新の行数、確認内容）。行には要判断タグ・リスク「高」・未確認が表示される | 旧／変／新の行数を 3 列に分けた表示を H-LIST で追加（`Smissing`／`Smodified`／`Sadded`）。選択・ファイル名・対応区分・リスク・確認内容は S1/S2/S4 で実装済み。要判断タグ・未確認は確認内容列の表示値そのもの（P1 §39.2 参照）。リスク「高」の強調はフォルダ比較の一覧にはなく、パネルだけの追加（P1 の判断。§44.6 参照） | 満たす（コード）＋未検証（実機。§44.5 項目 1〜4） |
| H-LIST | 旧／変／新の内訳3列の追加。S4 の分類結果は既に `Merge20FileState` にあるため列追加は小さい。既定では非表示・列選択で出せる形にする | 新列 3 件を追加。S4 の分類結果（`nMissing`／`nModified`／`nAdded`）をそのまま表示するため分類の変更はない。既定表示 -1 で列選択から出せる（W1 の移行の扱いに沿い、既存ユーザーには勝手に表示されない）。P1 の暫定 1 列は 3 列に置き換えて廃止 | 満たす（コード）＋未検証（実機。§44.5 項目 1〜4） |

### 44.4 検証レポート

- CI（PR #40、コード head `1e3e726b`）：実行 45872903034（2026-11-26 18:12:18 UTC 更新）：**結論 success**。ジョブ 140092596785（`build`）：completed／success。GoogleTest：`[  PASSED  ] 733 tests.`（H-FILTER 時点と同じ 733 件。新規テストなしのため件数は変わらない）。`[ FAILED ]` 行なし（ログ中の "FAILED" 4 件は翻訳文字列 ID（`IDS_FILESAVE_FAILED` 等）で、テスト失敗ではない）。コンパイルエラー 0 件（`error C` 0 件、`: error` 0 件、`Build succeeded` 1 件）。成果物：`WinMerge-x64.zip`（26,456,588 バイト、ID 15143801909、失効 2027-02-24T18:03:59Z）、`WinMerge-x64-Setup-x64.exe`（39,900,523 バイト、ID 15143627125、失効 2027-02-24T18:03:59Z）。全文ログは `~/workspace/h-list-work/hlist_final_log.txt` に保存（47,610 行）。ジョブログで MergeLang ステップ（`Creating POT file from Merge.rc...`、`Updating PO files from POT file...`、`All PO files updated`）と番兵チェック（`Checking bundled Japanese.po for Merge20 strings...`／`Merge20 strings present in the bundled Japanese.po.`）の通過を確認。
- CI 1 回目（実行 45871488070、head `eb811444`）は failure：`Merge20FileListCtrl.cpp(48)` で error C2065（ファイルスコープの幅配列で `COL_COUNT` が未宣言。`COL_COUNT` は `CMerge20FileListCtrl` の入れ子 enum のため、メンバ関数内では見えるが無名名前空間では見えない）＋ error C2078（初期化子が多すぎる。同じ原因の派生）。幅配列をリテラルの `[8]` にする修正（`1e3e726b`。元の `[6]` と同じ方式）で解決し、2 回目で success になった。教訓：入れ子 enum のメンバはメンバ関数内でしか素で使えない。ファイルスコープの配列にはリテラルを使うこと。
- .po 監査：master 版は msgid 2165 件・【Wiz】 133 件。ブランチ版は msgid 2167 件・【Wiz】 132 件。増分は msgid +2、【Wiz】 -1 で、変更内容（暫定 msgid 1 件の廃止＝【Wiz】 -1、新規説明文 3 件＝msgid +3、【Wiz】なし）と一致する。ブランチ版・master 版とも API から取得して確認。新規 3 件の msgstr に空欄はない。【Wiz】の全角括弧が欠落していないことも件数で確認（133→132 は廃止した 1 件分のみ）。
- 静的整合性チェック（作業用スクリプト。CI ではなくローカルで実行）：(1) 3 getter がそれぞれ正しいフィールドだけを読むこと（`ColMissingGet`→`nMissing`、`ColModifiedGet`→`nModified`、`ColAddedGet`→`nAdded`、他の n フィールドを読まない）。(2) 3 ソートがそれぞれ正しいフィールドで比較すること。(3) CSV 出力が同じ 3 フィールドを書くこと。(4) 要約文の書式 msgid が残っており、同じ 3 フィールドを使うこと。(5) パネルが 3 ラッパーを使い、宣言がヘッダにあること。(6) 暫定の関数・msgid・enum がコードから消えたこと（残る言及は「removed」のコメント 3 か所のみ）。(7) 両テーブルの 3 行がすべて既定表示 -1 であること。(8) `MigrateMerge20Columns` が新列を含まず、既定非表示の列を移行しないこと。(9) 新コードが使う全 msgid がブランチ版 .po に存在すること。(10) パネルの幅配列・右寄せ 3 件・スリム表示の名前参照が一致すること。すべて合格（`~/workspace/h-list-work/hlist_verify.txt`）。
- ローカル検証：全変更ファイルの括弧バランス（`DirViewColItems.cpp`・`FileListCtrl.cpp` とも 0）、`CreateMasterPotFile.js` の対象となるリテラル `_()`／`N_()` の確認（新規 3 ヘッダは既存 msgid の流用、新規 3 説明文はリテラルの `N_()` 呼び出し）、`MigrateMerge20Columns` の実読（既定 -1 の列は追加されない）、`DirView.cpp` の移行呼び出しの実読（バージョン <1 のときのみ実行し、新列は対象外）、リソース ID の変更なし（新規 ID は使っていないため監査不要）、`Merge.vcxproj`／`UnitTests.vcxproj` の変更なし（新規ファイルはないため登録不要）。
- 未検証（Windows 実機でのみ確認できる挙動）：§44.5 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 44.5 実機確認チェックリスト（手順付き。未検証）

1. **3 列の表示**：実フォルダを比較し、フォルダ比較の一覧で「列の表示」を開く。「旧欠落」「変更」「新追加」の 3 列があること。3 列を表示させ、ファイルを 1 つ開いて分類済みにすると、その行の 3 列に数字が表示されること。数字は要約列の先頭部分「欠落：N行 / 変更：M行 / 追加：K行」の N・M・K と一致すること。
2. **既定では非表示**：保存済み列レイアウトのある環境（既存ユーザー）でフォルダ比較を開き直しても、3 列が勝手に表示されないこと。新規環境でも既定では表示されないこと。
3. **表示値の一致（CSV）**：CSV を出力し、同じファイルの「旧欠落」「変更」「新追加」の 3 欄が、一覧の 3 列の数字と一致すること（CSV は桁区切りなし、一覧はロケールの桁区切り付きの場合がある）。
4. **未分類・ディレクトリ行**：未開封のファイルの 3 列が「未計算」であること。ディレクトリ行の 3 列が空であること。
5. **ソート**：3 列のいずれかのヘッダをクリックしてソートし、数字の昇順・降順になること。未分類（未開封）のファイルが最後に寄ること。
6. **パネルの 3 列化**：ファイル比較を開き、左のパネルに旧欠落・変更・新追加の 3 列が表示されること（暫定の「欠落：N行 / 変更：M行 / 追加：K行」1 列ではなくなっていること）。数値はフォルダ比較の一覧の 3 列と一致すること。パネルの幅が以前より広がっていないこと。スリム表示にするとファイル名＋確認内容だけになること。3 列のいずれかをクリックするとそのファイルが開くこと。
7. **回帰なし**：ファイル比較・フォルダ比較が従来どおり動くこと。確認内容の変更・出力チェック・一括チェック・CSV 出力・セッション保存／復元・一括分類・フィルタデッキが従来どおり動くこと。

### 44.6 次への移行

- H-LIST は完了（コード）。残る Wave 4 は H-SIMPLE、S0-F1、H-DOC。
- 実機確認待ち（§44.5 の全項目）。
- リスク「高」の強調はフォルダ比較の一覧にはなくパネルだけの追加である（P1 §39.2・§39.6 の判断。実機確認時のユーザーの判断に委ねる）。H-LIST では一覧側に強調を追加していない。
- 3-way 比較での 3 列の表示は、2-way と同じ getter・同じ分類フィールドを使うため、表示値の定義は同じである。実機確認項目には 2-way のみを置いた。3-way での違和感があれば別途記録すること。

## 45. H-SIMPLE — 巨大ファイル時の簡易比較＋ [簡易] バッジ（カバレッジ行列 H-05・H-28・H-SIMPLE）— ブランチ `merge20/h-simple`

- PR: #41（https://github.com/askhim659/winmerge/pull/41）。最終 head（コード）`0864aa1a72432bb4cacc9d0cf8d10d0767be9cbc`（7 件）。マージは本記録時点では未実施（コーディネーター判断待ち）。
- カバレッジ行列 H-05・H-28・H-SIMPLE、仕様書 §7、設計書 §3.12・§5.2 D10、HTML `17_v17.15.html`（`diffFast`／`lcs`、1049-1072 行）。受け入れ条件：しきい値を超えるファイルで簡易比較になりバッジが表示されること（H-05）、一覧での `[簡易]` バッジ表示が正しいこと（開封後も含む。H-28）。

### 45.1 変更一覧（1 ファイル 1 コミット、7 件＋本節の記録コミット。連続コミットの間に sleep 3）

| # | コミット | ファイル | 内容 |
|---|---|---|---|
| 1 | `ae573e7e` | Src/Merge20/Merge20BulkDiff.h | `Merge20BulkLineReader`＋`Merge20BulkIsSimple` の宣言（`<functional>` 追加） |
| 2 | `4bb56c11` | Src/Merge20/Merge20BulkDiff.cpp | `Merge20BulkIsSimple` の実装。`Merge20BulkDiff` の `useSimple` 判定を同関数に一本化 |
| 3 | `910251b4` | Testing/GoogleTest/Merge20/Merge20BulkDiff_test.cpp | `Merge20BulkIsSimple` の単体テスト 9 件追加（計 21 件） |
| 4 | `afdb9c31` | Src/Merge20/Merge20ClassifyDoc.h | `Merge20ClassifyIsSimpleCompare` の宣言。`Merge20ClassifyFromMergeDoc` に `bool simpleCompare` 引数を追加 |
| 5 | `73c5013d` | Src/Merge20/Merge20ClassifyDoc.cpp | `CountRealLines`（ゴースト行を除く実ライン数）＋`Merge20ClassifyIsSimpleCompare` の実装。`Merge20ClassifyFromMergeDoc` で `state.simpleCompare` に格納 |
| 6 | `4b186761` | Src/MergeDoc.cpp | Rescan 開始時に巨大ファイル判定（`merge20HugeFile`）。巨大ファイルで `AdjustDiffBlocks`・`FlagMovedLines` をスキップ。分類フックへ判定を引き渡し |
| 7 | `0864aa1a` | Src/Merge20/Merge20FileListCtrl.cpp | パネルのファイル名セルに ` [簡易]` バッジを追加（`Refresh()`、表示時のみ） |
| 8 | — | MERGE20_NOTES.md | 本節の記録コミット |

注：コミット 3 のメッセージは「10 new tests」と書いたが、実際は 9 件の追加である（既存 12 件＋新規 9 件＝計 21 件。§45.5 参照）。

### 45.2 D10 との重なり整理（H-05・H-28 の受け入れ条件と 1 件ずつ照合）

D10（PR #32、§36）の実装を master から読んで照合した結果。D10 で満たされている部分と H-SIMPLE で追加した部分を区別する。

| 受け入れ条件の要素 | D10 での状態 | H-SIMPLE での追加／判断 |
|---|---|---|
| しきい値 `n*m > 2,200,000`（HTML `lcs` と同一） | 満たす。`Merge20BulkDiff` がトリミング後の中間部に適用。既定 2200000 の `OPT_MERGE20_SIMPLE_THRESHOLD`（0＝無効） | 判定を `Merge20BulkIsSimple` として切り出し、一括分類パスと開封パスで同じ関数を使うよう一本化（コミット 2）。判定が 2 か所に分散して食い違う余地をなくした |
| 位置合わせ方式（HTML の `lcs` フォールバックと等価） | 満たす（一括分類パス）。中間部がしきい値を超えると位置合わせ（同じ行番号同士）になり `simpleUsed=true` | 開封パスは差分エンジン自体を置き換えない（下記の判断参照）。開封パスで使うのは「しきい値を超えるか」の判定だけで、位置合わせの差分は作らない |
| 一覧に `[簡易]` バッジが表示される | 満たす（一括分類済みのファイル）。`ColSummaryGet` が `simpleCompare` 時に `"[" + _("Simple") + "] "` を表示時に付ける。保存される summary は汚さない | 変更なし（表示は D10 のまま）。ただし下記の (a) により、開封後もバッジが残るようになった |
| (a) 開封後も `[簡易]` バッジが一覧に残る | 満たさない。`Merge20ClassifyFromMergeDoc` は `simpleCompare` を立てず（既定 false）、開封時の Rescan フックが `SetMerge20Classification` で false に上書きするため、開封するとバッジが消える（§36.3 の食い違い） | 満たす（コード）。Rescan 開始時にバッファから巨大ファイル判定を行い（`Merge20ClassifyIsSimpleCompare`）、分類フックへ渡して `state.simpleCompare` に格納する（コミット 5・6）。開封時の再分類でもフラグが維持される |
| (b) ファイル比較を開いたときに巨大ファイルであることが分かる表示 | 満たさない。ファイル比較ウィンドウ側に巨大ファイルを示す表示はなかった（フォルダ一覧の要約列だけ） | 満たす（コード）。比較ウィンドウの常設ファイル一覧パネルで、ファイル名の横に `名前 [簡易]` を表示する（コミット 7。HTML がファイル名の横に `[簡易]` を出すのと同じ位置。パネルに要約列はないためファイル名に付けた）。要約文への「簡易比較」の追加は行わない（下記の判断参照） |
| (c) しきい値を超えるファイルを開いたときの挙動の改善 | 満たさない。開封時は通常の完全な差分計算（diffutils＋位置合わせ＋移動検出）が走る | 満たす（コード、一部トレードオフあり）。巨大ファイルでは類似行の位置合わせ（`AdjustDiffBlocks`）と移動ブロック検出（`FlagMovedLines`）をスキップする（コミット 6）。diffutils 自体の差分計算はそのまま走るため、開封が速くなる度合いは実機での確認が必要（§45.6 項目 4）。開封前のモーダル通知は実装しない（下記の判断参照） |
| 比較エンジン自体を簡易比較に切り替える（仕様書 §7 の読み） | 範囲外（§36.2 の判断）。開封時の差分エンジンは diffutils のまま | 範囲外のまま（下記の判断参照）。H-SIMPLE でも置き換えない |

### 45.3 設計判断（判断理由付き）

- **判定の意味を「分類が簡易比較によるもの」から「ファイルがしきい値を超える巨大ファイルであること」に寄せた**。開封後の分類は完全な diffutils 差分から作られるため、厳密には「簡易比較で分類された」という意味は開封後には成り立たない。だがファイルの行数は開封で変わらないため、「しきい値を超えるファイル」という判定は開封前後で安定する。判定関数を一本化したことで、一括分類パスと開封パスが同じファイルに同じ答えを出す（単体テスト `AgreesWithBulkDiffer` が両者の一致を固定する）。バッジの意味がずれる余地は NOTES に明記する（本節）。セッション JSON の `simpleCompare`（D10）はそのまま流用する。
- **判定はトリミング後の中間部に適用する（HTML と同一）。未トリミングの総行数積は使わない**。5000 行のファイルが中間 10 行だけ違う場合、未トリミングなら 25,000,000＞2,200,000 でバッジが付いてしまうが、HTML の `lcs` は中間 10×10＝100 で簡易比較を使わない。誤ったバッジを出さないため、開封パスでも共通 prefix／suffix を正規化行で除去してから判定する（`Merge20BulkIsSimple`）。正規化は一括分類パスと同じ `NormalizeLine`（空白つぶし・大文字小文字）を使う。単体テスト `HugeFilesWithTinyMiddleDoNotTrigger` がこの場合を固定する。
- **開封パスの判定はバッファから読む（ファイルの再読み込みなし）**。`Merge20ClassifyIsSimpleCompare` は `CDiffTextBuffer` の実ライン数を数え（`GetLineCount()` はゴースト行込みの見かけ行数のため `LF_GHOST` を除く）、共通 prefix／suffix の行だけを `GetBufferLine`（実ライン→見かけライン変換つき）で読む。中間部の行は読まないため、巨大ファイルでも読む行は頭尾の数行だけ（同一ファイルなら全行読むが、それは diffutils も同じオーダー）。Rescan 開始時（ゴースト除去前）と分類フック時（ゴースト追加後）のどちらでも正しく動くよう、実ライン数えと `ComputeApparentLine` 経由の読みに統一した。判定は Rescan で 2 回走る（開始時とフック時。フック時は分類に渡すため）。2 回とも O(行数) のフラグ走査＋頭尾の行読みで、diffutils に比べれば無視できる。
- **比較エンジン自体を簡易比較に切り替えない（D10 の判断を維持）**。理由：(1) WinMerge の差分エンジンは diffutils（`CDiffWrapper`）で、ファイル比較ウィンドウの表示・編集・マージ操作はすべてその差分リストを前提にしている。位置合わせの差分に置き換えると、差分ブロックの分割・移動検出・単語差分・3-way・プラグイン（unpacker／prediffer）など広範囲の前提が崩れ、H-SIMPLE の範囲を大きく超える。(2) ユーザーの要望の中心は「巨大ファイルでも一覧の分類とバッジが正しく出ること」と「開封時の負荷軽減」であり、開封後の差分表示そのものを簡易にすることではない。(3) 一括分類パス（D10）ではすでに位置合わせの差分が分類に使われており、開封前の判断は簡易比較で行われている。仕様書 §7 の「比較エンジン自体を簡易比較に切り替える」読みとは異なる食い違いとして残し、カバレッジ行列の H-05・H-28 の行にもその旨を反映させること（行列の更新はコーディネーターが行う）。
- **要約文への「簡易比較」の追加は行わない（HTML との意図的な違い）**。HTML は要約文の末尾に ` / 簡易比較` を追加するが、フォークは D10 の方式（保存される summary は汚さず、表示時に `[簡易]` を付ける）を維持する。理由：(1) 要約文は CSV・セッション・学習の監査証跡として使われており、表示用の注記を混ぜると後方互換と検索性が落ちる。(2) `[簡易]` バッジが要約列の先頭とパネルのファイル名横の 2 か所に出るため、情報としては同じことが伝わる。(3) D10 の監査で「バッジ自体にはを付けない（仕様書 §7 と HTML のリテラル `"[簡易]"` との一致を優先）」と決めており、同じ方針を踏襲する。
- **開封前のモーダル通知は実装しない**。理由：(1) Rescan は開封時だけでなく再読込・編集後にも走るため、Rescan 内での通知は巨大ファイルの編集中に繰り返し出てしまう。(2) 一覧とパネルのバッジが常時見えるため、開く前に巨大ファイルであることは既に分かる。(3) ユーザーは摩擦を嫌う（短い直接の指示とスクリーンショットで正す人であり、余計なダイアログは仕事の邪魔になる）。代わりに (c) の負荷軽減（後処理のスキップ）を実装した。
- **巨大ファイルでの後処理スキップのトレードオフ**。`AdjustDiffBlocks`（類似行の位置合わせ）をスキップすると、変更ブロック内の類似行が揃わなくなる（オプションを OFF にしたときと同じ表示）。`FlagMovedLines`（移動ブロック検出）をスキップすると、移動マーカーが出なくなる（移動検出を OFF にしたときと同じ表示）。分類カウントは粗いブロックから作られるため、位置合わせありの場合と数行ずれることがある。いずれも「巨大ファイルでは表示の一部を簡略化する代わりに開封の負荷を下げる」という H-SIMPLE の範囲内の判断であり、通常サイズのファイルには一切影響しない（判定が false なら従来どおり）。3-way は対象外（しきい値の定義が 2-way のため、`AdjustDiffBlocks3way` はそのまま）。
- **新規 UI 文字列なし**。パネルのバッジは D10 の msgid `"Simple"`（→「簡易」）を流用し、バッジ自体に【Wiz】は付けない（D10 §36.2 の方針どおり）。`.po` の変更なし、リソース ID の変更なし、新規ファイルなし（vcxproj の変更なし）。
- **片側のみファイルにはバッジは付かない**。一括分類パスでは片側のみファイルは行数カウントのみで分類され `simpleCompare` は false のまま（D10 のワーカー）。開封パスでも片側のバッファは 0 行のため中間部積は 0 になり false。両パスで一致する。

### 45.4 カバレッジ行列の照合表（H-05・H-28・H-SIMPLE を 1 件ずつ確認）

| 条項 | HTML 版の機能 | フォークでの対応 | 判定 |
|---|---|---|---|
| H-05 | 巨大ファイル時の簡易比較＋ `[簡易]` バッジ（`lcs`：`n*m>2200000` で位置合わせの簡易比較に切替え、`simple:true`。一覧に `[簡易]` 表示、要約文に「簡易比較」を追加） | しきい値・位置合わせ方式は HTML と同一（`Merge20BulkDiff`＋`Merge20BulkIsSimple`、既定 2200000）。一覧の要約列に `[簡易]`（D10）、比較ウィンドウのパネルでファイル名横に `[簡易]`（H-SIMPLE）。要約文への「簡易比較」の追加は行わない（§45.3 の判断。保存される要約文は汚さない D10 の方式を維持）。比較エンジン自体の置き換えは範囲外（§45.3 の判断） | 満たす（コード、一部食い違いあり）＋未検証（実機。§45.6 項目 1〜3） |
| H-28 | 一覧の `⊘N`・`[簡易]` バッジ表示（H-08・H-05 と同じものの「一覧での見え方」） | `[簡易]` は D10 の要約列バッジに加え、H-SIMPLE で開封後も残るようになった（Rescan フックへ判定を引き渡し）＋パネルのファイル名横にも表示。`⊘N` は S0-F1 の範囲（本スライスでは扱わない） | 満たす（コード、`[簡易]` の部分）＋未検証（実機。§45.6 項目 1〜3） |
| H-SIMPLE | 巨大ファイル時の簡易比較＋ `[簡易]` バッジ。しきい値 `n*m>2,200,000` は HTML と同じ定義。位置合わせ方式も HTML の `lcs` フォールバックと等価にする | 一括分類パスは D10 で等価（`Merge20BulkDiff`）。開封パスは判定だけを共有し、差分エンジンは diffutils のまま（§45.3 の判断）。位置合わせの差分そのものを開封パスで作ることはしない | 満たす（コード、一部食い違いあり。開封パスのエンジン置き換えは範囲外）＋未検証（実機。§45.6） |

### 45.5 検証レポート

- CI（PR #41、コード head `0864aa1a`）：実行 45877486338（2026-11-26 18:50:10 UTC 更新）：**結論 success**。ジョブ 140108139857（`build`）：completed／success。GoogleTest：`[  PASSED  ] 742 tests.`（H-LIST 時点の 733 件＋新規 9 件。`[ FAILED ]` 行なし。ログ中の "FAILED" 4 件は翻訳文字列 ID（`IDS_FILESAVE_FAILED` 等）で、テスト失敗ではない）。コンパイルエラー 0 件（`error C` 0 件、`: error` 0 件、`Build succeeded` 1 件）。成果物：`WinMerge-x64.zip`（26,457,479 バイト、ID 15145508328、失効 2027-02-24T18:38:02Z）、`WinMerge-x64-Setup-x64.exe`（39,908,242 バイト、ID 15147100832、失効 2027-02-24T18:38:02Z）。全文ログは `~/workspace/h-simple-work/hsimple_final_log.txt` に保存（47,672 行）。ジョブログで MergeLang ステップ（`Creating POT file from Merge.rc...`、`All PO files updated`）と番兵チェック（`Checking bundled Japanese.po for Merge20 strings...`／`Merge20 strings present in the bundled Japanese.po.`）の通過を確認。
- .po 監査：ブランチ版は msgid 2167 件・【Wiz】132 件。master 版も msgid 2167 件・【Wiz】132 件で一致（新規 UI 文字列なしのため増分 0）。ブランチ版・master 版とも API から取得して確認。`\"Simple\"` → 「簡易」は両版に存在する。
- ローカル検証：`Merge20BulkDiff` 純粋モジュールを g++ スタブハーネスで検証し、19 アサーション全パス（`Merge20BulkIsSimple` と `Merge20BulkDiff` の `simpleUsed` の一致を含む。`~/workspace/h-simple-work/harness_is_simple.cpp`）。ハーネス実行中に 1 件の期待値の誤りが見つかり修正した（読み取り失敗時の判定は「不明な行は相違として扱い中間部に残す」ため true が正しい。テストの期待値を false から true に訂正。実装の変更はない）。
- 静的整合性チェック（作業用スクリプト。CI ではなくローカルで実行）：(1) `Merge20ClassifyFromMergeDoc` の唯一の呼び出し元（`MergeDoc.cpp` の Rescan フック）が新シグネチャに更新されていること。(2) 新規 `_()` リテラルはパネルの `"Simple"`（D10 の流用）のみで、新規 msgid はないこと。(3) 新規リソース ID なし（`.rc`／`resource.h` の変更なし）。(4) 新規ファイルなし（vcxproj の変更なし）。(5) 全変更ファイルの括弧バランスが 0 であること（`MergeDoc.cpp` の括弧カウント -1 は master 原本と同じく既存のもので、自分の差分は 4 か所の意図した編集のみであることを diff で確認）。(6) `Merge20BulkIsSimple` のトリミングが `Merge20BulkDiff` のトリミングと同じ正規化・同じ歩行であること（同じ `NormalizeLine` を使う構造＋`AgreesWithBulkDiffer` テスト）。すべて合格。
- 未検証（Windows 実機でのみ確認できる挙動）：§45.6 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 45.6 実機確認チェックリスト（手順付き。未検証）

1. **[簡易] バッジ（一括分類）**：行数×行数が 2,200,000 を超える差分ファイル（例：旧 2000 行・新 2000 行で中間部が大きいもの）を含むフォルダで一括分類を実行し、該当ファイルの要約列の先頭に `[簡易]` が付くこと（D10 §36.6 項目 3 と同じ手順）。
2. **[簡易] バッジ（開封後も残る。H-SIMPLE の主眼）**：項目 1 のファイルを開き、フォルダ比較の一覧に戻ると、要約列の先頭に `[簡易]` が残っていること（D10 では開くと消えていた）。ファイルを閉じて再度開いても残ること。セッションを保存して復元しても残ること。
3. **[簡易] バッジ（パネル）**：項目 1 のファイルを開き、比較ウィンドウ左の常設ファイル一覧パネルで、該当ファイルのファイル名の横に ` [簡易]` が付いていること。パネルがスリム表示のときもファイル名横に付いていること（スリム表示はファイル名＋確認内容だけを表示する）。パネルでファイルを切り替えてもバッジが付いたままであること。
4. **巨大ファイルの開封（負荷軽減）**：項目 1 のファイルを開くとき、ファイル比較ウィンドウが表示されること。開いたファイルで、変更ブロック内の類似行の位置合わせが行われていないこと（類似行が揃わず、変更ブロックが粗く表示される）。移動ブロック検出が OFF のときと同じく、移動マーカーが出ないこと。通常サイズのファイル（しきい値以下）では従来どおり位置合わせ・移動マーカーが出ること。
5. **バッジが付かない場合**：5000 行のファイル同士で中間 10 行だけが違うフォルダで一括分類を実行し、該当ファイルに `[簡易]` が付かないこと（トリミング後の中間部が小さいため）。そのファイルを開いてもバッジが付かないこと。片側のみの巨大ファイル（旧のみ／新のみ）にも `[簡易]` が付かないこと。
6. **しきい値オプション**：Tools＞Options で `Merge20/SimpleThreshold` を 0 にすると（既定 2200000。0＝無効）、一括分類でも開封でも `[簡易]` が付かなくなること。値を小さくする（例：100）と、小さいファイルにも `[簡易]` が付くようになること。変更後は WinMerge を再起動せずに反映されること（オプションは Rescan／一括分類のたびに読む）。
7. **回帰なし**：フォルダ比較・ファイル比較・セッション保存／復元・一括分類・フィルタデッキ・常設ファイル一覧パネルが従来どおり動くこと。巨大ファイルでない通常のファイルを開いたとき、確認内容・リスク・要約が従来と同じであること。

### 45.7 次への移行

- H-SIMPLE は完了（コード）。残る Wave 4 は S0-F1、H-DOC。
- 実機確認待ち（§45.6 の全項目）。特に項目 2（開封後のバッジ）と項目 4（後処理スキップの表示）は H-SIMPLE の主眼のため、実機での確認結果を本節へ追記すること。
- 既知の制限（勝手に削っていない。理由は §45.3 参照）：(1) 比較エンジン自体の簡易比較への置き換えは範囲外（開封後の差分は diffutils の完全な差分）。(2) 要約文への「簡易比較」の追加は行わない（表示時バッジで代替）。(3) 開封前のモーダル通知は実装しない（一覧とパネルのバッジで代替）。(4) 巨大ファイルでは類似行の位置合わせと移動ブロック検出をスキップする（表示の簡略化と引き換えの負荷軽減）。(5) Rescan ごとに巨大ファイル判定が 2 回走る（開始時とフック時。いずれも O(行数) のフラグ走査＋頭尾の行読みで、diffutils に比べれば無視できる）。
- 検証ゲートの注意：CI 緑だけでは不十分。最終 head の実行結論・全文ログの GoogleTest 全パス行・成果物 2 件の API 確認を毎回行うこと。実機挙動は §45.6 のチェックリストでユーザーに確認してもらうこと。

## 46. S0-F1 — ⊘N 真の除外行数表示（カバレッジ行列 S0-F1・H-08・H-28）— ブランチ `merge20/s0-f1`

- PR: #42（https://github.com/askhim659/winmerge/pull/42）。最終 head（コード）`67b8f07007242b6ebb4eb8ffbaf35ef974d0cac0`（24 件）。マージは本記録時点では未実施（コーディネーター判断待ち）。
- カバレッジ行列 S0-F1・H-08・H-28、仕様書 §7、設計書 §3.12、HTML `17_v17.15.html`（`f.ex`、1415 行目・1343 行目・1356 行目・691 行目）。受け入れ条件：Sexcluded 列の値が prediffer で除外された真の行数であること（差分件数ではないこと）、内訳ツールチップが表示されること、既定表示の扱いが決まっていること（W1 の移行の扱いに沿うこと）、100〜1000 ファイル規模での比較時間への影響が記録されていること（計測方法を含む）。

### 46.1 変更一覧（1 ファイル 1 コミット、24 件＋本節の記録コミット。連続コミットの間に sleep 3）

| # | コミット | ファイル | 内容 |
|---|---|---|---|
| 1 | `2ca07271` | Src/Merge20/Merge20ExcludedLines.h | `Merge20CountExcludedLines` の宣言（純粋モジュール、ASCII のみ） |
| 2 | `031a4fe7` | Src/Merge20/Merge20ExcludedLines.cpp | 計数実装（`#include "pch.h"` 先頭、SplitLines＋IsBlank、before 非空白＋after 空白／行なし＝除外） |
| 3 | `ecec8123` | Testing/GoogleTest/Merge20/Merge20ExcludedLines_test.cpp | 単体テスト 12 件（18 アサーション。不変、1 行除外、2 行目バージョン、既に空白、空白化、削除行、追加行、非空白化変更、CRLF、CR のみ、複数行、末尾 EOL なし／末尾改行） |
| 4 | `18faede8` | Src/Merge.vcxproj | Merge20ExcludedLines.h/.cpp を登録（pch.h Use ブロック付き、XML 検証済み） |
| 5 | `10c96c59` | Src/Merge.vcxproj.filters | 同上（Filter Merge20、XML 検証済み） |
| 6 | `1db007fc` | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | 同上（ソース素の ClCompile、テストは pch.h Use ブロック、XML 検証済み） |
| 7 | `6dc88f6a` | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同上（XML 検証済み） |
| 8 | `297e5ddb` | Src/DiffItem.h | `int nexcluded[3]`（-1＝不明）追加、インライン `GetExcludedTotal()`（既知の最大値、不明時は -1）、ctor 初期化 `nexcluded{-1, -1, -1}` |
| 9 | `44932b54` | Src/DiffItem.cpp | `Swap` で `nexcluded[idx1]`／`[idx2]` を交換、`GetEmptyItem` の assert に nexcluded 3 件を追加 |
| 10 | `c1b35e82` | Src/DiffItemList.cpp | `SetDiffCounts` で `di.nexcluded[0/1/2] = -1` にリセット（理由コメント付き） |
| 11 | `8c42e554` | Src/FileTransform.h | `PrediffingInfo::Prediffing` に `int * pExcludedLines = nullptr` を追加（ドキュメントコメント付き） |
| 12 | `2ff20835` | Src/FileTransform.cpp | `Prediffing()` の BUFFER_PREDIFF 分岐で before スナップショット（`GetDataBufferUnicode()`＋`SysStringLen` でコピー）→ `InvokePrediffBuffer` → `ValidateNewBuffer()` 後に `GetNChanged() > 0` かつ before 非空なら `Merge20CountExcludedLines` で加算。フィルタ式分岐と FILE_PREDIFF 分岐には数えない理由のコメントを追加 |
| 13 | `3b4889f4` | Src/DiffFileData.h | `Filepath_Transform` に `int * pExcludedLines = nullptr` を追加 |
| 14 | `58cd2979` | Src/DiffFileData.cpp | 同パラメータを `Prediffing` へ受渡し |
| 15 | `8e209403` | Src/DirViewColItems.h | `bool IsColExcluded(int col) const;` を宣言 |
| 16 | `34bcfae8` | Src/DirViewColItems.cpp | COLDESC_EXCLUDED の msgid を真の行数説明に変更。`ColExcludedGet` を DIFFITEM ベースに書換え（片側のみは空欄、不明／0 は空欄、>0 は ⊘N＋`locality::NumToLocaleStr`）。新規 `ColExcludedSort`（不明は最後）。`IsColExcluded` 実装。2-way 行は offset 0・ソート ColExcludedSort・order 11、3-way 行は order 12。`MigrateMerge20Columns` に `Sexcluded` を追加 |
| 17 | `80616943` | Src/DirView.cpp | `OnChildNotify` の LVN_GETINFOTIPW で Sexcluded セルのツールチップを埋める（他列は現状維持＝ツールチップなし）。`OnInitialUpdate` の移行バージョン `< 1`→`< 2`、保存 1→2 |
| 18 | `963c963d` | Src/OptionsInit.cpp | `InitOption(OPT_MERGE20_COLUMN_LAYOUT_VERSION, 0, 0, 1)` の最大値を 1→2 に変更 |
| 19 | `30437bde` | Src/OptionsDef.h | バージョンキーの説明を更新（0＝移行前、1＝5 列、2＝除外行列も追加） |
| 20 | `aa2a54c4` | BuildTranslations.cmd | 番兵リストに新規 ASCII msgid 3 件を追加（`%1`／`%2`／`%3` は `%%` にエスケープ） |
| 21 | `7c8bd7dd` | Translations/WinMerge/Japanese.po | 新規 3 件の訳を追加、旧 COLDESC_EXCLUDED の項目を削除（W1 の規則：文字列を削除したら .po の項目も同じ変更で消すこと）。msgid 2167 → 2169 件、【Wiz】132 件で変化なし |
| 22 | `53c476d6` | Src/FileTransform.cpp | 修正：`Prediffing` の定義側シグネチャに `int * pExcludedLines` を追加（コミット #12 の漏れ。宣言側のみで定義側が旧 4 引数のままになっており、自己レビューで発見。未コンパイルのままでは C2065 になるところだった） |
| 23 | `3b906ae1` | Src/CompareEngines/FullQuickCompare.cpp | `int nExcluded[3] = { 0, 0, 0 }` を宣言、prediff ループで `&nExcluded[nIndex]` を渡し、末尾で `di.nexcluded[i]` へ格納。比較が途中で失敗したとき（`m_ndiffs == DIFFS_UNKNOWN`）は -1（不明）にする。クイック比較（`DIFFS_UNKNOWN_QUICKCOMPARE`）では prediffer は走っているため計数を残す |
| 24 | `67b8f070` | Src/DirView.cpp | `#include "locality.h"` を追加（コミット #17 のツールチップで使う `locality::NumToLocaleStr` が未宣言だった。CI 初回実行のコンパイルエラー 2 件を受けての修正。§46.4 参照） |

- 注記（到達不能コミット 2 件。PR には含まれない）：(a) `99dea40d` — FileTransform.cpp の編集スクリプトが `if (bWithFile)` の old テキストの複数一致で assert 失敗し、ファイル未変更のまま打たれた空コミット。直後に `commit_on_parent.py`（parent `8c42e554`、force PATCH）で正しい内容の #12 を作り置き換えた。教訓：複数箇所に一致しうる old テキストは周辺の一意な文脈まで含めて指定すること。(b) `19adb64b` — FullQuickCompare.cpp の修正コミット時に、編集対象ファイルを誤って master 版で上書きし、#15 相当の配線（`580d2cd2`）を巻き戻してしまった。直後に `commit_on_parent.py`（parent `53c476d6`、force PATCH）で配線＋ガード付きの正しい内容の #23 を作り置き換えた。教訓：編集前の取得元（ブランチ版か master 版か）を毎回確認すること。`fetch.py` は既定で master を取る。

### 46.2 設計判断（判断理由付き）

1. **⊘N＝側ごとの prediffer 空行化行数の最大値。** 対称 prediffer（通常）では HTML の行数と一致。合計にしない（対称ペアを二重計上するため）。非対称時のずれは NOTES に明記し、ツールチップで側別内訳を示す。
2. **計数：純粋関数 `Merge20CountExcludedLines(std::wstring_view before, std::wstring_view after)`。** before 非空白＋after 空白（または after に行がない）＝除外。行分割は CRLF/LF/CR。空白のみ行は before でも除外に数えない。after が増えた行は数えない。末尾の改行は phantom 行を作らない。
3. **計数場所：`Prediffing()` の BUFFER_PREDIFF 分岐のみ。** FILE_PREDIFF（temp ファイル。stock／Merge20 に該当プラグインなし）とフィルタ式分岐（任意 TransformLine で blanking の契約なし）は数えない（制限として記録）。
4. **既定表示：表示にする（order 11／12）。** 2026-11-26 実機で列自体が見つからなかった一因が既定非表示のため。W1 の扱いに沿い、移行リストに Sexcluded を追加しバージョン 1→2（InitOption max も 2 へ、DirView の `<1`→`<2`・保存 1→2）。バージョン 1 で移行済みのレイアウトは 5 列が表示済みのため、バージョン 2 では Sexcluded だけを追加する（`MigrateMerge20Columns` は表示済みの列を跳ばす）。
5. **ツールチップ：2-way「Excluded lines: old %1, new %2」、3-way「Excluded lines: left %1, middle %2, right %3」。** 不明側は "?"。合計>0 のときのみ。msgstr には【Wiz】を付けない（数値表示のため。S4 要約値の前例。列見出しには既に【Wiz】がある）。`OnChildNotify` は LVN_GETINFOTIP を従来無記入で TRUE 返していた（＝ツールチップなし）が、Sexcluded セルだけを埋める。他列は現状維持＝ツールチップなし。`pInfo->pszText` へのコピーは既存の前例（`FileFiltersDlg::OnInfoTip`）と同じ `_tcsncpy_s(..., _TRUNCATE)`。
6. **ソート：新規 `ColExcludedSort`（不明は最後）。** 表示：片側のみ／不明は空欄、0 は空欄（現状と同じ）、>0 は ⊘N。
7. **`DiffItemList::SetDiffCounts`（ファイル比較からの更新経路）は nexcluded を不明へリセット（再計算しないため）。** `DirActions::SetDiffCounts(di,0,0)`（コピー後）は触らない（コピーは内容を変えず、空行化行数は変わらないため）。
8. **比較が途中で失敗したときは nexcluded を -1（不明）にする。** `exitPrepAndCompare` のエピローグは全経路で走るため、unpack／prediff／OpenFiles の失敗時に側 0 だけ計数済みの部分値を書き込むと、失敗した比較に ⊘N が付いて誤解を招く。`m_ndiffs == DIFFS_UNKNOWN` のとき -1 にする。クイック比較は `DIFFS_UNKNOWN_QUICKCOMPARE`（≠ `DIFFS_UNKNOWN`）で prediffer は走っているため計数を残す。片側のみのファイルも `DIFFS_UNKNOWN` になるが、列の表示側で片側のみは空欄にするため影響はない。
9. **要約文への追加は行わない。** S4 の `Merge20FormatSummary` は既に「除外:N行」を含むが、それは OP_TRIVIAL 行数という別数量であり、prediffer 計数を同じ「除外」で二重表示すると混乱するため。統一にはファイル比較ロード経路への計数引渡しが必要→残課題として §46.6 に記録する。列の ⊘N（prediffer 空行化）と要約文の「除外」（trivial 差分行）は別数量である旨をここに明記する。
10. **性能：計数はメモリ内 BSTR に対する O(n) 走査＋1 回のバッファコピー（prediffer 実行時のみ）。** 追加 I/O・追加 diff パスなし。実測は実機チェックリスト（§46.5 項目 6。Merge20Measure のログ行で base ビルドと比較）。
11. **新規 msgid は 3 件（説明文＋ツールチップ 2 件）。** 説明文の msgstr にも【Wiz】は付けない（S8 メモ説明文の前例）。`.pot` は触らない（CI が .js で再生成し自己修復。English.pot に H-LIST の msgid「Old/Changed/New」がないことから、最近のスライスは .pot を更新していない）。
12. **自己レビューで見つけた不具合 2 件（§46.1 の #22・#23 の注記参照）を修正済み。** いずれも CI に投げる前に発見した。FileTransform.cpp の定義側シグネチャ漏れは、宣言と定義の両方を同じ変更で確認する習慣がなかったことが原因。FullQuickCompare.cpp の巻戻しは、編集対象ファイルの取得元（master／ブランチ）の確認漏れが原因。

### 46.3 カバレッジ行列の照合表（H-08・H-28・S0-F1 を 1 件ずつ確認）

| 条項 | HTML 版の機能 | フォークでの対応 | 判定 |
|---|---|---|---|
| H-08 | ⊘N 除外行数表示（`f.ex`）。一覧に `⊘N`、カーソルを乗せると内訳を表示（`data-tip`）。要約文にも「除外:N行」 | 一覧の ⊘N は prediffer 空行化行数（最大値）に。カーソルを乗せると側別内訳のツールチップを表示。要約文の「除外:N行」は S4 で既に存在するが、OP_TRIVIAL 行数という別数量であり、prediffer 計数との統一は未実施（§46.2 項目 9）。FILE_PREDIFF／フィルタ式は計数対象外、非対称 prediffer では最大値と HTML 行数がずれる | 満たす（コード、一部食い違いあり）＋未検証（実機。§46.5 項目 1〜5） |
| H-28 | 一覧の `⊘N`・`[簡易]` バッジ表示（H-08・H-05 と同じものの「一覧での見え方」） | ⊘N は本スライスで真の行数になり、既定表示＋移行で見つけやすくなった。`[簡易]` は H-SIMPLE の範囲 | 満たす（コード、`⊘N` の部分）＋未検証（実機。§46.5 項目 1〜5） |
| S0-F1 | ⊘N の真の除外行数表示。prediffer 前後バッファ比較による。接続点は `MERGE20_NOTES.md` §8.2 に記録済み、regName `Sexcluded` は流用可 | BUFFER_PREDIFF 分岐で前後バッファ比較（`PrediffingInfo::Prediffing` に out-param を追加し、`FullQuickCompare` から `DIFFITEM::nexcluded` へ引渡し）。内訳ツールチップあり。既定表示（移行 v2）。性能は O(n) 走査＋1 回コピー、実測は実機チェックリスト | 満たす（コード）＋未検証（実機。§46.5 項目 1〜7） |

### 46.4 検証レポート

- CI（PR #42、コード head `67b8f070`）：実行 45885296396（2026-11-26 19:39:45Z 作成、19:52:35Z 更新）：**結論 success**。ジョブ 140134393924（`build`）：completed／success。GoogleTest：`[  PASSED  ] 754 tests.`（master ベースライン 742 件＋新規 12 件。`[ FAILED ]` 行なし。`Merge20ExcludedLines` の 12 件はすべて `[  OK  ]`）。コンパイルエラー 0 件（`: error` 0 件、`error C` 0 件、`error LNK` 0 件、`Build succeeded` 1 件）。成果物：`WinMerge-x64.zip`（26,461,396 バイト、ID 15149342736、失効 2027-02-24T19:39:45Z）、`WinMerge-x64-Setup-x64.exe`（39,907,678 バイト、ID 15149811620、失効 2027-02-24T19:39:45Z）。全文ログは `~/workspace/s0f1-work/s0f1_ci_log2.txt` に保存（47,706 行）。ジョブログで MergeLang ステップ（`Creating POT file from Merge.rc...`、`All PO files updated`）と番兵チェック（`Checking bundled Japanese.po for Merge20 strings...`／`Merge20 strings present in the bundled Japanese.po.`）の通過を確認。なお、初回実行 45884284882（head `3b906ae1`）は **failure** だった：`Src\DirView.cpp(3497)` で `locality::NumToLocaleStr` 未宣言のコンパイルエラー 2 件（`error C2653`／`error C3861`）。`locality.h` の include 漏れ（`DirViewColItems.cpp` の 17 行目と取り違えて「DirView.cpp で include 済み」と誤認していた）。`67b8f070` で include を追加し、再実行で成功した。全文ログは `~/workspace/s0f1-work/s0f1_ci_log.txt` に保存（38,370 行）。
- 成果物 ZIP 内の `Japanese.po` の直接確認：成果物 `WinMerge-x64.zip`（ID 15149342736）をダウンロードし、内側の `wizmerge-2.16.60-ad78371-x64-exe.zip`（670 エントリ、`WinMergeU.exe` あり）から `WizMerge/Languages/Japanese.po` を取り出して監査。新規 msgid 3 件がすべて存在し、msgstr が期待値と完全一致（説明文「prediffer が空行化した行数（除外行）です。最も多い側の行数を表示します。セルにカーソルを乗せると側ごとの内訳が表示されます。」、2-way「除外行数：旧 %1行／新 %2行」、3-way「除外行数：左 %1行／中央 %2行／右 %3行」）。旧 msgid は存在しない。継続行を展開する簡易パーサで確認（長い msgid は出荷時に複数行に折り返されるため、単一行の正規表現では検出できない）。出荷 .po の msgid 件数は 2192 件（ブランチ版 2169 件に対し +23 件。差分は `.pot` 再生成で追加された stock 文字列（`%s`、`Abort`、`Redo` など）で、ブランチ版にあって出荷版にない msgid は 0 件。W1 のマージ方式が意図どおり動いている）。【Wiz】132 件で変化なし。

- .po 監査：ブランチ版は msgid 2169 件・【Wiz】132 件。master 版は msgid 2167 件・【Wiz】132 件。増分は +2 msgid（ツールチップ 2 件。説明文は 1 件置換のため差し引き 0）で、新規文字列と一致。重複 msgid なし。新規 3 件の msgid はコードのリテラルと完全一致し、msgstr（「prediffer が空行化した行数（除外行）です。最も多い側の行数を表示します。セルにカーソルを乗せると側ごとの内訳が表示されます。」「除外行数：旧 %1行／新 %2行」「除外行数：左 %1行／中央 %2行／右 %3行」）がすべて設定されていることを API から取得したブランチ版で確認。`msgfmt` は本 VM に未導入のため、Python による構文・重複チェックで代替した（CI の MergeLang ステップも .po を読むため、CI の成功が追加の裏付けになる）。
- ローカル検証：`Merge20CountExcludedLines` を g++ スタブハーネスで検証し、20 アサーション全パス（GoogleTest の 12 件（18 アサーション）に対応する 18 件＋追加 2 件。ブランチ版の `Merge20ExcludedLines.cpp` を API から取得し、スタブ `pch.h` でコンパイル。`~/workspace/s0f1-work/harness/`）。ハーネス実行中に不具合は見つからなかった。
- 静的整合性チェック（作業用スクリプト。CI ではなくローカルで実行）：(1) `Prediffing`・`Filepath_Transform` の宣言と定義のシグネチャが一致すること（#22 の漏れを受けて再確認）。(2) 新規 `_()`／`N_()` リテラル 3 件がすべて .po に存在すること（上記監査）。(3) 新規リソース ID なし（`.rc`／`resource.h` の変更なし）。(4) `DirViewColItems` の getfnc／sortfnc は `offset`（0）で DIFFITEM 全体を受け取ること（`ColGetTextToDisplay`・ソート経路で確認）。(5) ツールチップの `lParam`→DIFFITEM の導出が `ReflectGetdispinfo` と同一であること。(6) 全変更ファイルの括弧バランスが master と同一であること（FileTransform.cpp の `}` の +1 は master 原本と同じく既存のもので、自分の差分はバランスしている。新規 .cpp／.h は追加ファイルのため対象外）。すべて合格。
- 未検証（Windows 実機でのみ確認できる挙動）：§46.5 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 46.5 実機確認チェックリスト（手順付き。未検証）

1. **⊘N の表示**：除外ワードを含むファイルを含むフォルダでフォルダ比較を実行し、該当ファイルの「無視差分」列に ⊘N（N＝除外ワードに一致した行数）が表示されること。除外ワードを含まないファイルは空欄であること。片側のみのファイルは空欄であること。
2. **⊘N の値の正しさ**：旧・新で除外行数が違うファイル（旧 3 行・新 5 行など）で、⊘5（最大値）が表示されること。同じフォルダを HTML 版（マージ支援20号）で開いたときの ⊘N と一致すること（対称 prediffer の場合）。
3. **内訳ツールチップ**：⊘N のセルにカーソルを乗せると「除外行数：旧 3行／新 5行」のように側別内訳が表示されること。⊘N がないセルにはツールチップが出ないこと。他の列のセルにはツールチップが出ないこと（従来どおり）。
4. **既定表示・移行**：既存の保存済みレイアウト（バージョン 1 のもの）で開いたとき、「無視差分」列が末尾付近に表示されること。列の表示ダイアログで「無視差分」の説明文が「prediffer が空行化した行数（除外行）です。最も多い側の行数を表示します。セルにカーソルを乗せると側ごとの内訳が表示されます。」になっていること（英語の旧文言でないこと）。
5. **ソート**：列ヘッダをクリックして ⊘N でソートできること。不明（空欄）の行が最後に並ぶこと。
6. **比較時間（100〜1000 ファイル規模）**：同じフォルダ・同じ条件で、base ビルド（master）と本ビルドのフォルダ比較時間を比べ、大きな悪化がないこと。手順：(a) Merge20Exclude prediffer が有効な状態で、100〜1000 ファイルを含むフォルダを用意する。(b) base ビルドと本ビルドでそれぞれフォルダ比較を 3 回実行し、Merge20Measure のログ行の比較時間を記録して平均を取る。(c) 本ビルドの平均が base ビルドの平均と大きく変わらないこと（計数はメモリ内 O(n) 走査＋1 回コピーであり、追加 I/O・追加 diff パスはないため、悪化は小さいはず。実測値が大きく悪化する場合はコーディネーターへ報告すること）。
7. **回帰なし**：フォルダ比較・ファイル比較・一括分類・列の表示ダイアログ・セッション保存／復元が従来どおり動くこと。ファイルをファイル比較ウィンドウで編集してフォルダ比較に戻ったとき、そのファイルの ⊘N が空欄（不明）になること。

### 46.6 次への移行

- S0-F1 は完了（コード）。残る Wave 4 は H-DOC。
- 実機確認待ち（§46.5 の全項目）。特に項目 2（HTML 版との ⊘N の一致）と項目 6（比較時間）は S0-F1 の主眼のため、実機での確認結果を本節へ追記すること。
- 既知の制限（勝手に削っていない。理由は §46.2 参照）：(1) FILE_PREDIFF プラグインとフィルタ式分岐は計数対象外。(2) prediffer が空白化ではなく置換で正規化する場合は計数されない。(3) 複数の prediffer が同じ行を空白化するとステップごとに加算され二重計上になりうる。(4) 要約文の「除外:N行」（S4、OP_TRIVIAL 行数）と列の ⊘N（prediffer 空行化行数）は別数量のまま。統一にはファイル比較ロード経路への計数引渡しが必要（フォローアップ）。(5) 比較が途中で失敗した場合は計数を不明（-1）にする（部分計数を表示しない）。(6) IsBlank は半角空白とタブのみを空白とみなす（その他の空白文字を含む行は非空白として扱う。prediffer が空行化する行は "" になるため、通常は影響しない）。
- 検証ゲートの注意：CI 緑だけでは不十分。最終 head の実行結論・全文ログの GoogleTest 全パス行・成果物 2 件の API 確認を毎回行うこと。実機挙動は §46.5 のチェックリストでユーザーに確認してもらうこと。

## 47. H-DOC — 使い方マニュアルの Docs 同梱（カバレッジ行列 H-DOC・H-67）— ブランチ `merge20/h-doc`

- PR: #43（https://github.com/askhim659/winmerge/pull/43）。最終 head（コード）`e94e9c0abe431f49486d188cc29e72bb9f892ee6`（15 件）。マージは本記録時点では未実施（コーディネーター判断待ち）。
- カバレッジ行列 H-DOC・H-67、仕様書 §3、HTML `17_v17.15.html`（`helpMo`、656〜830 行目）。受け入れ条件：ヘルプメニューからマニュアルが開けること（【Wiz】付きのメニュー項目）、マニュアルの内容がフォークの実装と一致していること、マニュアルが成果物（ZIP／Setup exe）に同梱されていること。

### 47.1 変更一覧（1 ファイル 1 コミット、15 件＋本節の記録コミット。連続コミットの間に sleep 3）

| # | コミット | ファイル | 内容 |
|---|---|---|---|
| 1 | `4fb2000d` | Docs/Users/WizMergeManual.html | 新規。日本語マニュアル 17 節（32,089 バイト）。HTML 版 17 節を出発点にフォークの実装に合わせて書き直し。単一 HTML（インライン CSS のみ、外部参照・スクリプト・画像なし）。【Wiz】23 か所 |
| 2 | `869afac2` | BuildArc.cmd | Docs コピー一覧に `Docs\Users\WizMergeManual.html` を追加（ZIP の `WizMerge\Docs` へ） |
| 3 | `e2c22c59` | Installer/InnoSetup/WinMergeX64.is6.iss | [Files] にマニュアル行を追加（`{app}\Docs`、ReleaseNotes.html と同じ Flags・Components） |
| 4 | `a7032d4c` | Installer/InnoSetup/WinMergeARM64.is6.iss | 同上（ARM64） |
| 5 | `38628325` | Installer/InnoSetup/WinMergeX64.iss | 同上（x64、Inno Setup 5 用） |
| 6 | `1dc80dfa` | Installer/InnoSetup/WinMergeX64NonAdmin.iss | 同上（x64 PerUser） |
| 7 | `8e5fbb3d` | Installer/InnoSetup/WinMergeX86.iss | 同上（x86） |
| 8 | `afebe9b4` | Src/Constants.h | `WizManual[] = _T("\\Docs\\WizMergeManual.html")` を追加（`RelNotes` の直後） |
| 9 | `8827b5f1` | Src/resource.h | `ID_MERGE20_USER_MANUAL` = 38969 を追加。割当コメントに H-DOC を追記。`_APS_NEXT_COMMAND_VALUE` 38969→38970 |
| 10 | `c669e8a9` | Src/Merge.rc | 3 つのヘルプメニュー（IDR_MAINFRAME／IDR_DIRDOCTYPE／IDR_MERGEDOCTYPE）に `MENUITEM "&User Manual", ID_MERGE20_USER_MANUAL` を追加（「WizMerge Help」の直後） |
| 11 | `bc8c0b22` | Src/Merge.rc | 修正：メニュー項目の ID 列を隣の項目と揃えた（自己レビュー） |
| 12 | `c45d0821` | Src/MainFrm.h | `afx_msg void OnHelpWizManual();` を宣言（`OnHelpContents` の直後） |
| 13 | `a93d58dc` | Src/MainFrm.cpp | メッセージマップに `ON_COMMAND(ID_MERGE20_USER_MANUAL, OnHelpWizManual)` を追加。ハンドラ `OnHelpWizManual()` を実装（`OnHelpReleasenotes()` と同じ方式：`env::GetProgPath()`＋`WizManual` を `shell::Open` で開く。ファイルがない場合は `AfxMessageBox` で日本語メッセージ） |
| 14 | `dad3ac75` | Translations/WinMerge/Japanese.po | 新規 2 件：`"&User Manual"` → 「【Wiz】使い方マニュアル(&U)」、`"WizMerge user manual (Docs\\WizMergeManual.html) was not found."` → 「WizMerge の使い方マニュアル（Docs\WizMergeManual.html）が見つかりませんでした。」。msgid 2169 → 2171 件、【Wiz】132 → 133 件 |
| 15 | `e94e9c0a` | BuildTranslations.cmd | 番兵リストに `"User Manual"` を追加（`&` はバッチのメタ文字のため、`"&User Manual"` ではなく部分一致トークンで検査） |

### 47.2 設計判断（判断理由付き）

1. **形式は HTML（単一ファイル、インライン CSS のみ）。** WinMerge のヘルプは Docs/Manual サブモジュール（.chm）であり、フォークのマニュアルを .chm に組み込むのは難しい（サブモジュールのビルド手順・HTML Help Workshop が必要）。HTML なら既定ブラウザで開くだけで表示でき、ZIP／Setup への同梱も既存の Docs コピー経路（BuildArc.cmd の一覧、.iss の [Files]）に 1 行追加するだけで済む。外部参照・スクリプト・画像はなし（オフラインで開ける。HTML 版の CDN 依存とは違う）。
2. **同梱場所は `Docs/Users/WizMergeManual.html`。** Stock の `Docs/Users/ReleaseNotes.html`・`ChangeLog.html` と同じ場所・同じ同梱経路。`Docs/Manual` はサブモジュール（.chm の元 XML）のため使わない。アプリ内リソース（.rc に埋め込む案）は、32KB の HTML をリソース化する手間と、将来の更新のしにくさから不採用。
3. **開き方はヘルプメニューの新項目＋ `shell::Open`。** `OnHelpReleasenotes()` と同じ方式（`env::GetProgPath()`＋相対パスを既定ブラウザで開く）。内部ビューアは作らない（ブラウザ表示で十分であり、新規ビューは検証範囲が広がる）。F1（WinMerge Help、.chm）は stock のまま残す。
4. **英語版は作らない。** ユーザーは日本語 UI を使用し、マニュアルの読み手も日本語話者に限られる。英語版を作ると二重管理になり、日本語の統一（W1 の主眼）が崩れる。理由をここに記録する。
5. **内容は HTML 版 17 節を出発点に、フォークの実装に合わせて書き直した。** フォークにない機能は除くか、未実装である旨を明記した：マージ後 ZIP 出力（§13・§17）、強調文字の比較画面表示（§12・§17。マスタの管理だけができる）、サンプル読込（§17）、FORM／XML 比較表示（§17）、監査コメント自動挿入（記述なし）、全体マップの倍率操作（記述なし。位置ビューは移動のみと明記）。フォーク独自の機能（学習、専用モード、スナイパー、パネル、フィルタデッキ、一括分類、セッション、⊘N、[簡易]）はすべて記述した。
6. **メニュー項目の加速キーは U。** Help メニューで未使用の文字（W・E・T・O・G・A は使用済み）。`.rc` ラベルは英語 `"&User Manual"`（W1 の規則：【Wiz】は msgstr にだけ付ける）。日本語表示は「【Wiz】使い方マニュアル(&U)」。
7. **コマンド ID は 38969。** タスク指示の「38968 以降」は、H-FILTER が 38968（デッキバー切替）を既に使用済みのため 38969 に訂正した（resource.h の割当コメントと `_APS_NEXT_COMMAND_VALUE` で確認）。次に使える空きは 38970。
8. **ファイル欠落時は日本語メッセージボックス。** `OnHelpReleasenotes()` には欠落時の処理がないが、マニュアルは H-DOC で新規追加するファイルのため、ZIP／Setup への同梱漏れやビルドディレクトリからの起動で欠落する可能性があり、沈黙の失敗を避けた。メッセージは `_()` リテラル（.po に訳あり）。
9. **番兵トークンは `"User Manual"`（部分一致）。** msgid 本体は `"&User Manual"` だが、`&` はバッチのメタ文字のため FOR 集合に入れられない。`.po` 内の `"User Manual"` は新規 msgid の 1 件だけであることを grep で確認済み（偽陽性なし）。欠落メッセージは番兵に入れない（括弧が FOR 集合を壊すため）。代わりに .po 件数監査（§47.4）でカバーする。
10. **新規 msgid は 2 件。** メニュー項目と欠落メッセージ。欠落メッセージの msgstr に【Wiz】は付けない（エラーメッセージのため。S4 要約値・S0-F1 ツールチップの前例）。`.pot` は触らない（CI が再生成し自己修復。S0-F1 と同じ）。

### 47.3 カバレッジ行列の照合表（H-67・H-DOC を 1 件ずつ確認）

| 条項 | HTML 版の機能 | フォークでの対応 | 判定 |
|---|---|---|---|
| H-67 | 使い方マニュアル 17 節（`openMan`／`helpMo`・「📖 使い方」ボタン）。基本の流れ、フォルダ読込、画面構成、左一覧、フィルタ、差分判定、確認内容、AI要約、フォーカス詳細、専用モード、スナイパー、Grep・強調・除外、出力、保存・復元、学習、キーボード、制限と注意。ツールの挙動と一致することを検証済み（v17.13） | 17 節を `Docs/Users/WizMergeManual.html` として書き直し、ヘルプメニュー（【Wiz】使い方マニュアル）から開けるようにした。各節はフォークの実装に合わせて書き換え済み（§47.2 項目 5）。HTML 版の「📖 使い方」ボタン（ブラウザ内モーダル）は、フォークではヘルプメニューの項目に置き換えた | 満たす（コード）＋未検証（実機。§47.5 項目 1〜4） |
| H-DOC | 使い方マニュアルの Docs 同梱。ヘルプメニューから開ける形にする。内容は本行列と各設計書から生成する | マニュアルを `Docs/Users/` に同梱し、ヘルプメニューから既定ブラウザで開く。ZIP へは BuildArc.cmd、Setup exe へは .iss 5 件で同梱。内容は本行列と各設計書（NOTES §1〜§46）から生成し、フォークにない機能は未実装と明記 | 満たす（コード）＋未検証（実機・成果物。§47.4・§47.5） |

### 47.4 検証レポート

- CI（PR #43、コード head `e94e9c0a`）：実行 45890866054（2026-11-26 20:24:57Z 作成、20:33:44Z 更新）：**結論 success**。ジョブ 140152908669（`build`）：completed／success。GoogleTest：`[  PASSED  ] 754 tests.`（master ベースライン 742 件＋S0-F1 の 12 件。H-DOC は単体テストを追加しないため同数。`[ FAILED ]` 行なし）。コンパイルエラー 0 件（`: error` 0 件、`error C` 0 件、`error LNK` 0 件、`Build succeeded` 1 件）。成果物：`WinMerge-x64.zip`（26,471,329 バイト、ID 15151747699、失効 2027-02-24T20:24:57Z）、`WinMerge-x64-Setup-x64.exe`（39,922,628 バイト、ID 15152600240、失効 2027-02-24T20:24:57Z）。全文ログは `~/workspace/h-doc-work/hdoc_ci_log.txt` に保存（47,702 行）。ジョブログで MergeLang ステップ（`Creating POT file from Merge.rc...`、`All PO files updated`）と番兵チェック（`Checking bundled Japanese.po for Merge20 strings...`／`Merge20 strings present in the bundled Japanese.po.`）の通過を確認。インストーラのログには `WizMergeManual.html` の読込・圧縮行が 2 回（x64 Setup と x64 PerUser Setup）ある。
- 成果物 ZIP 内のマニュアルの直接確認：成果物 `WinMerge-x64.zip`（ID 15151747699）をダウンロードし、内側の `wizmerge-2.16.60-4aacb54-x64-exe.zip`（671 エントリ、`WinMergeU.exe` あり）から `WizMerge/Docs/WizMergeManual.html` を取り出して確認。コミット版と CRLF 正規化後にバイト一致（出荷時は Windows ランナーの git チェックアウトで CRLF 化されており 253 バイト増えている）。出荷 .po の監査：新規 msgid 2 件がすべて存在し、msgstr が期待値と完全一致（「【Wiz】使い方マニュアル(&U)」、「WizMerge の使い方マニュアル（Docs\WizMergeManual.html）が見つかりませんでした。」）。ブランチ版にあって出荷版にない msgid は 0 件。出荷 .po の msgid 件数は 2185 件（ブランチ版 2162 件に対し +23 件。差分は `.pot` 再生成で追加された stock 文字列で、S0-F1 と同じパターン）。【Wiz】133 件で一致。


- .po 監査：ブランチ版は msgid 2171 件・【Wiz】133 件。master 版は msgid 2169 件・【Wiz】132 件。増分は +2 msgid・+1【Wiz】で、新規文字列 2 件（メニュー項目＋欠落メッセージ）と一致。重複 msgid の新規発生なし（master と同一の既存重複 5 件のみ：None・Folder・Binary・Diff・Disabled）。新規 2 件の msgid はコード／.rc のリテラルと完全一致し、msgstr がすべて設定されていることを API から取得したブランチ版で確認。`msgfmt` は本 VM に未導入のため、Python による構文・重複チェックで代替した（CI の MergeLang ステップも .po を読むため、CI の成功が追加の裏付けになる）。
- マニュアル HTML の検証：タグバランスを確認（html/head/body/ol/ul/table/div/li すべて対応）。h2 17 節、目次リンク 31 件。外部参照（http/https）0 件、script タグ 0 件、img タグ 0 件。【Wiz】23 か所。UTF-8 として読めること、32,089 バイトであることを確認。
- 静的整合性チェック（作業用スクリプト。CI ではなくローカルで実行）：(1) コマンド ID 38969 が resource.h で 1 件だけ定義され、他と重複しないこと（全 define を値でソートして確認。38968 は H-FILTER、38970 が次の空き）。(2) メニュー項目が 3 つの Help ポップアップすべてにあること（Merge.rc で 3 件）。(3) `WizManual` 定数が Constants.h にあり、他に同名がないこと。(4) `OnHelpWizManual` の宣言（MainFrm.h）・メッセージマップ（MainFrm.cpp）・定義（MainFrm.cpp）が揃っていること。(5) `Merge.h` が `resource.h` を include しているため MainFrm.cpp で `ID_MERGE20_USER_MANUAL` が見えること。(6) BuildArc.cmd／.iss 5 件の追加行が、それぞれ ReleaseNotes.html 行の直後に 1 件だけあること。(7) 全変更ファイルの改行コードが原本と同じであること（Merge.rc・BuildArc.cmd・Constants.h・MainFrm.h・MainFrm.cpp・BuildTranslations.cmd は LF、resource.h は CRLF、.iss はファイルごとの混在を維持）。すべて合格。
- 未検証（Windows 実機でのみ確認できる挙動）：§47.5 のチェックリスト。実機で確認するまで「できた」とは言わない。

### 47.5 実機確認チェックリスト（手順付き。未検証）

1. **ヘルプメニューからの表示**：WizMerge を起動し、ヘルプメニューを開くと「【Wiz】使い方マニュアル」が表示されること（「WizMerge ヘルプ」の直後）。クリックすると既定ブラウザでマニュアル（17 節）が開くこと。フォルダ比較ウィンドウ・ファイル比較ウィンドウのどちらからでも同じ項目が出ること。
2. **マニュアルの内容の正確さ**：マニュアルの各節（特に §6 差分判定、§10 専用モード、§11 スナイパー、§12 マスタ管理、§15 学習）の記述が、実際の画面・メニュー名・キー操作と一致していること。食い違いがあれば本節へ追記すること。
3. **ZIP の中にマニュアルファイルがあること**：CI 成果物 `WinMerge-x64.zip` を展開し、`WizMerge\Docs\WizMergeManual.html` があること。展開したフォルダで `WinMergeU.exe` を起動し、ヘルプメニューからマニュアルが開けること。
4. **Setup exe の中にマニュアルファイルがあること**：CI 成果物 `WinMerge-x64-Setup-x64.exe` でインストールし、`{app}\Docs\WizMergeManual.html` があること。インストール後にヘルプメニューからマニュアルが開けること。
5. **ファイル欠落時のメッセージ**：`Docs\WizMergeManual.html` を一時的にリネームしてヘルプメニューから開くと、「WizMerge の使い方マニュアル（Docs\WizMergeManual.html）が見つかりませんでした。」と日本語で表示されること（英語のまま・無言の失敗でないこと）。確認後はファイルを戻すこと。
6. **回帰なし**：ヘルプメニューの他の項目（WizMerge ヘルプ、リリースノート、翻訳、設定ログ、GNU ライセンス、WizMerge について）が従来どおり動くこと。

### 47.6 次への移行

- H-DOC は完了（コード）。これで Wave 4（新規設計分）はすべて完了（H-FILTER・H-LIST・H-SIMPLE・S0-F1・H-DOC、常設ファイル一覧パネル P0〜P4）。
- 実機確認待ち（§47.5 の全項目）。特に項目 1（メニューからの表示）と項目 3・4（成果物への同梱）は H-DOC の主眼のため、実機での確認結果を本節へ追記すること。
- 既知の制限（勝手に削っていない。理由は §47.2 参照）：(1) マニュアルは日本語のみ。英語版なし。(2) マニュアルは HTML（.chm には組み込んでいない）。(3) マニュアルの内容の正確さは実機未検証（§47.5 項目 2）。(4) FORM／XML 比較表示・マージ後 ZIP 出力・強調文字の比較画面表示・サンプル読込は、この版には含まれていない（マニュアル §17 に明記）。(5) 欠落メッセージは番兵リストに入っていない（括弧のため）。件数監査でカバーする。
- 検証ゲートの注意：CI 緑だけでは不十分。最終 head の実行結論・全文ログの GoogleTest 全パス行・成果物 2 件の API 確認を毎回行うこと。実機挙動は §47.5 のチェックリストでユーザーに確認してもらうこと。


## §48 H-DOC マージ・Wave 4 完了・Wave 5（FORM比較 F0）起動（2026-11-26 夜）

### 48.1 H-DOC のマージ（PR #43 → master）

- H-DOC（使い方マニュアルの Docs 同梱、ブランチ merge20/h-doc、16コミット・15ファイル・+381/−5）をコーディネーター検証後にマージ。マージコミット **bbc43fb9**（master head）。
- コーディネーター検証済み（マージ前）：PR #43 は open・mergeable=true・mergeable_state=clean、head 64b534b0、base d4581344（S0-F1 マージ済み）。最終 head の CI 実行 45892145474 は completed／success（head_sha=64b534b0 と一致）。全文ログで GoogleTest [ PASSED ] 754 tests.（新規テストなし。S0-F1 時点 754件と同じ）、[ FAILED ] 0件、コンパイルエラー 0件、番兵チェック通過（Merge20 strings present in the bundled Japanese.po.）。成果物（実行 45892145474）：WinMerge-x64.zip 26,471,309バイト（ID 15152940427）、WinMerge-x64-Setup-x64.exe 39,943,921バイト（ID 15152945402）、失効 2027-02-24。コード head e94e9c0a の実行 45890866054 も success＋成果物 2件（ZIP 26,471,329バイト ID 15151747699、Setup 39,922,628バイト ID 15152600240、失効 2027-02-24）。
- インストーラのログには WizMergeManual.html の読込・圧縮行が 2回（x64 Setup と PerUser Setup）あることをコーディネーターも全文ログで確認済み。ZIP への同梱は子エージェントがダウンロードした ZIP から直接確認済み（内側 wizmerge-2.16.60-4aacb54-x64-exe.zip、671エントリ、WizMerge/Docs/WizMergeManual.html は CRLF 正規化後にバイト一致）。
- S0-F1 のマージコミット CI もコーディネーターが API で確認済み：実行 45889090048（head d4581344）completed／success、成果物 2件（WinMerge-x64.zip 26,461,467バイト ID 15151032630、WinMerge-x64-Setup-x64.exe 39,908,695バイト ID 15151037613、失効 2027-02-24）。S0-F1 のマージはこれで完全検証済み。
- H-DOC のマージコミット CI はポーリング中（~/workspace/.coord/poll_hdoc_merge_ci.sh、head bbc43fb9）。完了後に API で成果物を確認し、本節へ追記すること。

### 48.2 Wave 4（新規設計分）の完了

- **Wave 4 は H-DOC のマージで完了。** H-FILTER（PR #39）・H-LIST（PR #40）・H-SIMPLE（PR #41）・S0-F1（PR #42）・H-DOC（PR #43）、および常設ファイル一覧パネル P0〜P4（PR #34〜#38）。
- 未検証（実機のみ）：§47.5 のチェックリスト 6項目（ヘルプメニューからの表示、マニュアルの内容の正確さ、ZIP／Setup への同梱の実機確認、欠落時メッセージ、回帰なし）。実機確認待ち。

### 48.3 Wave 5（FORM/XML比較 F0〜F7）起動

- 設計書：merge20-form-compare-design.md（§10 スライス F0〜F7）。FORM比較は可視化のみ（マージしない。ユーザーの訂正 2026-11-26 09:39 JST）。HTML版より「わかりやすく、高い精度で」改善すること（ユーザーの追加指示）。
- F0（基盤：XMLパーサ＋データモデル＋単体テスト）を子エージェントに委任（ブランチ merge20/form-f0、base master bbc43fb9）。受け入れ条件は設計書§10 F0 (a)〜(d)：正常XMLのパースと不正XMLの失敗、実体参照のデコード、HTML版 parseXmlToMapAndShapes の ground truth（Node 実行）との一致（要素の集合と属性で比較。キー文字列の比較は F1）、回帰なし（新規モジュールのみ）。
- マージ順序：F0→F1→F2→F3→F4→F5→F6→F7。F3・F4・F5 は並行可だがマージは順序どおり。
- ユーザー確認待ち（未回答）：F-7（点滅）、F-10（Merge20.flt 既定フィルタ）、F-15（S4 add-only「中」vs HTML「なし」）、「【Wiz】」→「【WizMerge】」ラベル変更、T-F-1（実フォームXMLサンプル）、T-F-9（名称マスターの保存形式／場所）。
- Wave 2 の S5／S7 は引き続き U-0-2（2-way 自動マージの実機検証待ち）のため保留。

## §49 F0 マージ・Wave 5 F0 完了・F1 起動（2026-11-27）

### 49.1 F0 のマージ（PR #44 → master）

- F0（基盤：XMLパーサ＋データモデル＋単体テスト、ブランチ `merge20/form-f0`、7コミット・7ファイル・追加のみ +1347/-0、base master `94956238`）をコーディネーター検証後にマージ。マージコミット **15173552**。
- コーディネーター検証済み（マージ前）：PR #44 は open・mergeable=true・mergeable_state=clean、head `6e780da8910008fba1f8ab627c475c2c8ff62c64`、base `94956238313b95f233f3eba76bd5c2bfc9333416`。最終 head の CI 実行 45924163470 は completed／success（head_sha=6e780da8 と一致）。全文ログで GoogleTest `[ PASSED ] 763 tests.`（H-DOC 時点 754＋新規 9。`[ FAILED ]` は翻訳文字列 ID（IDS_FILESAVE_FAILED 等）のみでテスト失敗ではない）、コンパイルエラー 0 件（`error C[0-9]`・`error LNK` 0 件、`Build succeeded` 1 件）。成果物（実行 45924163470）：`WinMerge-x64.zip` 26,472,199 バイト（ID 15166555717）、`WinMerge-x64-Setup-x64.exe` 39,928,899 バイト（ID 15166096676）、失効 2027-02-25。全文ログは `~/workspace/.coord/f0_final_log.txt` に保存（47,767 行）。
- マージコミット CI：実行 45925523753 は completed／success（head_sha=15173552 と一致）。成果物：`WinMerge-x64.zip` 26,472,231 バイト、`WinMerge-x64-Setup-x64.exe` 39,927,179 バイト。F0 のマージはこれで完全検証済み。

### 49.2 受け入れ条件の照合（設計書 §10 F0）

| 条件 | 結果 |
|---|---|
| (a) 正常 XML のパースと不正 XML の失敗 | 合格。要素・属性・テキスト・コメント・XML宣言・CDATA を正しくパースし、タグ対応不一致・属性値の引用符の閉じ忘れ・未閉じタグ・裸の `&`／`<`・ルートなし・ルート後のゴミをパース失敗として返す（単体テストで検証） |
| (b) 実体参照のデコード | 合格。`&amp;`・`&lt;`・`&gt;`・`&quot;`・`&apos;`・数値文字参照をデコード。未定義の実体参照はそのまま保持 |
| (c) HTML版 ground truth との一致 | 合格。HTML版 `parseXmlToMapAndShapes` の関数そのものを Node＋jsdom の DOMParser で実行した ground truth と、同じ 12 入力に対するフォークのパーサ出力を突き合わせ、要素の集合・属性・図形の座標すべて一致（不一致 0 件）。ローカル g++ ハーネスでも 59 件のチェック全合格 |
| (d) 回帰なし | 合格。新規モジュールの追加のみ。既存コードの変更はなし（`.vcxproj`／`.filters` は新規ファイルの登録のみ） |

### 49.3 変更一覧（1 ファイル 1 コミット、7 件。連続コミットの間に sleep 3）

| # | コミット | ファイル | 内容 |
|---|---|---|---|
| 1 | `426ca0fa` | Src/Merge20/Merge20FormXml.h | 新規。データモデル（`Merge20FormElement`／`Merge20FormShape`／`Merge20FormDiffRecord`／`Merge20FormFileResult`、`Merge20FormDiffType`／`Merge20FormSeverity`）＋パーサ宣言。座標は double。`isEstimated`・`structPath`・`groupId` を含む（F1 用の受け皿） |
| 2 | `1ec617a5` | Src/Merge20/Merge20FormXml.cpp | 新規。自前 XML パーサ（要素・属性・テキスト・コメント・XML宣言・CDATA。タグ対応・引用符の検査。実体参照のデコード）。図形の抽出規則は HTML版と同じ（x／y のフォールバック、x2／y2 の推定、`line` 判定） |
| 3 | `f7c49fb9` | Testing/GoogleTest/Merge20/Merge20FormXml_test.cpp | 新規。単体テスト 9 件（ParseBasicForm／ParseEntitiesAndNamespaces／ParseFailures／UnknownEntityKeptAsIs／ShapeEstimationAndLines／DuplicateKeysUseGlobalCounter／StructPaths／NonNumericCoordsMakeNoShape／ParseNumber） |
| 4 | `83e1756e` | Src/Merge.vcxproj | 新規 2 ファイルを登録（`Merge20FormXml.cpp` は pch.h グループ） |
| 5 | `719fe5d3` | Src/Merge.vcxproj.filters | 同上 |
| 6 | `d12b2e68` | Testing/GoogleTest/UnitTests/UnitTests.vcxproj | 新規ファイルとテストを登録（S1 の LNK2019 の教訓） |
| 7 | `6e780da8` | Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters | 同上 |

### 49.4 意図的な差異（設計書 §3.3 どおり。PR 説明文にも記載）

- 未定義の実体参照はパース失敗にせずそのまま保持する（DOMParser は致命的エラー）。`&unknown;` を含む入力では HTML版は null、フォークは成功する。
- `xmlns`／`xmlns:*` はキー化から除外する（属性自体は保持）。属性署名キーはソート順（HTML版は文書順）。

### 49.5 未検証（実機）・次への移行

- F0 には UI・永続化・記録フックがないため、実機で確認できる動作はない。実機確認は「CI 成果物の `WinMergeU.exe` が従来どおり起動すること（回帰確認）」のみ。
- `wchar_t` 2 バイト時のサロゲートペア処理（`&#x10000;` 以上）はコード上の対応のみで実機未検証。
- F1 を起動（ブランチ `merge20/form-f1`、base は F0 マージ後の master `15173552`）。受け入れ条件は設計書 §10 F1 (a)〜(d)。
- **パネルバグ（実機報告 2026-11-27 10:49 JST）**：品質修正版ビルド（run 45893505703）で、フォルダをドラッグ＆ドロップで開いた場合にファイル比較ウィンドウのパネルが「フォルダ比較がありません」と表示する。フォルダ比較ウィンドウ自体には一覧が出ているため、P3 の連携（DirDoc の取得／Merge20FindRelPath／FindMerge20ItemFromPaths のフォールバック）の不具合で確定。`workspace/goals/winmerge-fork-with-20-features/hidden_files/panel-bug-2026-11-27.md` に記録。修正を別ブランチ `merge20/panel-dragdrop-fix`（base `15173552`）で F1 と並行して着手する（触るファイルの重なりなし）。GitHub Issue での記録は 410 で作成できなかった（Issue が無効の可能性）。

## §50 品質修正版の .po 欠落：artifact レベルで解消を確認（親エージェント報告、2026-11-27 ~10:25 JST）

- 親エージェントが品質修正版の artifact 自体（run 45893505703、内側 zip `wizmerge-2.16.60-bbc43fb-x64-exe.zip`）から確認：出荷 `WizMerge/Languages/Japanese.po` は **2,069 msgids**（リポジトリ版 2,046 に対し +23）。**欠落 0 件**（以前のビルドでは 111 件欠落）。Merge20 文字列はすべて存在（Review State、Include in Output、Action Category、Risk、Summary、Memo、Learning...、Batch check...、Excluded: %1 lines、Dedicated Compare Mode）。列名は日本語で表示されるはず。
- ZIP 内のトップフォルダは `WizMerge/` になり、リネームが artifact にも到達した。
- 本節は親エージェントの確認結果の記録である（コーディネーター自身の検証ではない）。ユーザーへの実機確認のチェックリスト（`quality-fix-checklist.md`）には引き続き「列名が日本語で表示されること」を残す。

## §51 新調整役の開始・Wave 5 再開（2026-11-27 11:21 JST）

### 51.1 前任の中断
- 前任の調整役（10:15 JST のスケジュール実行が所有、f5eff8b4）は、所有するスケジュール実行の 1 時間制限で 2026-11-27 11:16 JST に途中で打ち切られた。子エージェントも同時に取り消された。
- 中断時点で確定していたこと：F0 はマージ済み（§49）。F1 は設計書の読み込みのみで実装・コミット・PR はゼロ。パネル連携バグは原因をコード実読で特定済み（作成順序のバグ。`CMergeDoc::m_pDirDoc`。ドラッグ＆ドロップ固有ではない）だが実装・ブランチ・PR はゼロ。専用モード不具合は調査のみで外部への書き込みゼロ。
- スケジュール実行の自動リトライ（2 回目、11:17 JST 開始）は親エージェントが止め、runonce ジョブを削除した（重複する調整役の発生を防ぐため）。
- 新調整役はメインのチャットが所有する（1 時間制限なし）。開始時の master HEAD は `54b369d1`（F0 マージ＋NOTES §49.1 の CI 検証済み）。開いている PR は 0 件。並行ブランチ `merge20/panel-dragdrop-fix`／`merge20/form-f1` は作成されていない。

### 51.2 ユーザー方針（2026-11-27 11:00〜11:03 JST。親エージェント経由）
- **開発は計画どおり進めつつ、同時に品質も上げる。** 新機能の追加を止めない。実機で確定した不具合は優先修正として次のビルドに入れる。検証ゲート継続。
- **UI の大幅な見直し**：見た目がほぼ WinMerge のままで視覚的に感動しない。アプリのヘッダに WizMerge を目立つ形で出すこと。根本的に見直されて作られた、ただの WinMerge の名前違いではないものだと視覚的にも証明すること。`hidden_files/ui-redesign-2026-11-27.md`。
- **最高峰のワークフロー（北極星）**：「確認の取りこぼしゼロ、マージの取りこぼしゼロ」。完成の条件は、一連の作業の終わりに「見落としがない」と確信できること。8 本の柱は `hidden_files/best-workflow-2026-11-27.md`。
- 完成予定は 2026-12-02〜12-03（親エージェントがユーザーに伝達済みの見込み）。UI 見直しと 8 本の柱の設計次第で延びる可能性あり。詳細は §51.4。
- ラベルは【Wiz】のまま（【WizMerge】への変更はユーザー未確認のため独断で変えない）。課金が絡む提案はしないこと。

### 51.3 進める順序
1. **優先修正（次のビルドに入れる）**：パネル連携バグ（ブランチ `merge20/panel-linkage-fix`。NOTES §49.5 の仮名 `merge20/panel-dragdrop-fix` は原因がドラッグ＆ドロップ固有ではないため使わない）と専用モード不具合（ブランチ `merge20/ded-mode-fix`。原因特定からやり直す）。いずれも base は master `54b369d1`。
2. **Wave 5（F1〜F7）**：設計書 §10 の順序どおりにマージ。F3・F4・F5 は並行可だがマージは順序どおり。FORM／XML 比較は可視化のみ（マージしない）。F1 は本節と同時に起動済み（ブランチ `merge20/form-f1`、base `54b369d1`）。
3. **UI の大幅な見直し**：MFC の制約内で実現可能な設計をまず行い、スライス化してから実装する。操作性は壊さないこと。
4. **最高峰のワークフロー**：8 本の柱について、すでに計画にあるもの（S1〜S4、D3、D5、D6、S8、学習、パネル、FORM）と未着手のものを切り分け、不足分を追加設計・スライス化する。
5. **残件**：S5（一括承認）、統一「選択ファイルZIP」（v17.16 の (3)）。NOTES・仕様書で状態を確認し、計画に組み込むこと。

### 51.4 見込み（2026-11-27 時点の調整役の見積もり）
- 優先修正 2 件：各 1 PR サイクル。専用モードは原因特定からやり直すため 1〜2 サイクルかかる可能性あり。
- F1〜F7：F1・F2 は各 1 サイクル、F3・F4・F5 は並行で各 1〜2 サイクル、F6・F7 は各 1 サイクル。過去のペースは 1 PR あたり約 50〜60 分＋CI 約 20〜30 分。
- UI 見直しと 8 本の柱：設計スライス化が先行するため、規模が判明するのは優先修正と F1 の進行後。現時点の見込みとして、完全版の完成は **2026-12-02〜12-03 を維持できる見込みだが、UI 見直しと 8 本の柱の追加設計の規模次第で 12-04 まで延びる可能性あり**。優先修正入りの次ビルドは 2026-11-27〜11-28 を目指す。
- 検証ゲート厳守：CI の緑バッジだけでなく成果物の中身まで確認する。実機未検証は明記する。見た目の確認とワークフロー全体の通し確認も検証対象に含める。

## §52 パネル連携バグ修正（PR #45）— マージ・CI 検証済み（2026-11-27）

### 52.1 原因（コード実読で確定）
- `CMergeDoc` のコンストラクタは `m_pDirDoc(nullptr)` で初期化する（`Src/MergeDoc.cpp:183`）。
- `GetMergeDocForDiff`（`Src/MainFrm.cpp`）は `pTemplate->OpenDocumentFile(nullptr, bMakeVisible)` を呼ぶ。MFC はこの中でドキュメントとフレームを生成するため、`CMergeEditFrame::OnCreateClient`／`OnCreate` はすべて `OpenDocumentFile` の内部で同期的に走る。
- `CMergeDoc::SetDirDoc(pDirDoc)` が呼ばれるのは `OpenDocumentFile` が返った後（同関数内、`MainFrm.cpp` 約 2330 行目）。
- したがって `OnCreateClient` の `if (CDirDoc* pDirDoc = ...GetDirDoc())`（`Src/MergeEditFrm.cpp:244`）は**常に偽**で、通知コールバックの登録と `m_wndFileListBar.SetDirDoc(pDirDoc)` は一度も実行されなかった。`RefreshFileListPanel()` もバーの DirDoc を設定しなかったため、バーの `m_pDirDoc` はフレームの生存期間ずっと `nullptr` のままで、`CMerge20FileListBar::Refresh()` は常に「フォルダ比較がありません」の分岐を取った。
- 同じ理由で `OnCreate` の初期表示判定（`GetDirDoc() != nullptr`、約 443 行目）も常に偽で、パネルはトグル ON でも初期非表示になっていた（副次的な不具合として今回同時に修正）。
- 開く経路は `CDirView::Open` → `DoFileOrFolderOpen` → `ShowTextOrTableMergeDoc` → `GetMergeDocForDiff` で一本化されており、フォルダのドラッグ＆ドロップも同じ経路を通る。**ドラッグ＆ドロップ固有ではない**ことを確認済み。

### 52.2 変更一覧（ブランチ `merge20/panel-linkage-fix`、base master `54b369d1`。2 ファイル 2 コミット）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20FileListBar.h` | `a7b4d6b55f` | `GetDirDoc()` アクセサを 1 件追加（`SetDirDoc` の対） |
| 2 | `Src/MergeEditFrm.cpp` | `a2df9cd7a0` | `RefreshFileListPanel()` で、初めて `GetDirDoc()` が非 nullptr になったときにバーの DirDoc を設定し、通知コールバック（HWND キー付き Add、従来と同じラムダ）を登録する。初期表示もここで補う（保存トグル ON の場合のみ表示し、ユーザーが隠したパネルは再表示しない。`Refresh()` は非表示時 no-op のため表示を先に行う）。フォルダ比較が外れた場合はバーの参照を `nullptr` に戻す（ダングリング防止）。`OnCreateClient` の実行されない attach ブロックは削除し、理由をコメントに残した。`DestroyWindow()` のコメントも登録場所に合わせて修正 |

- 新規 UI 文字列なし。`.po`・`.rc`・ワークフロー・`MERGE20_NOTES.md` に変更なし。ローカルクローンは未使用（Git Database API のみ）。

### 52.3 検証レポート（コーディネーター検証済み）
- **PR #45**：open・mergeable=true・mergeable_state=clean、head `a2df9cd7a0`、base master。2 コミット、+60/−20、変更ファイル 2 件。
- **CI（ブランチ head）**：実行 45928929476、completed／success。全文ログ（3,552,066 バイト、ジョブ `build` 140276770352）で GoogleTest `[ PASSED ] 763 tests.`（F0 時点 763 件と一致。テストファイルに変更なし）、`[ FAILED ]` 行なし、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件。全文ログは `~/workspace/.coord/panel45_final_log.txt` に保存。
- **成果物（実行 45928929476）**：`WinMerge-x64.zip` 26,471,878 バイト（ID 15167836618）、`WinMerge-x64-Setup-x64.exe` 39,931,945 バイト（ID 15168300444）。
- **マージ**：マージコミット `a77763be`。マージコミット CI：実行 45930481595、completed／success（head_sha=a77763be と一致）。
- **受け入れ条件の照合**：
  - (a) フォルダ比較から開いたファイル比較ウィンドウのパネルにフォルダ比較の一覧が出ること — コード上は満たす（初回アクティベーション時の `RefreshFileListPanel` で attach され、以降 `Refresh()` が一覧分岐を取る）。**実機未検証**。
  - (b) 単独ファイル比較では従来どおり「フォルダ比較がありません」の空表示であること — コード上は満たす（`GetDirDoc()` が null のままなのでバーの DirDoc は設定されず、空表示分岐のまま）。**実機未検証**。
  - (c) 回帰なし — コード上は満たす（既存の比較経路への変更はなく、attach は `RefreshFileListPanel` 内の追加処理のみ）。**実機未検証**。
- **未検証（実機でしか確認できないもの）**：上記 (a)〜(c) の実機確認（フォルダ比較からのオープン時のパネル表示・一覧内容・選択行の追従、単独ファイル比較の空表示、表示／非表示トグルの保存・復元）、フォルダ側で確認内容・出力を変えたときのパネルのライブ更新、専用モードとの独立性。実機確認チェックリストは §38.5 と §41.5 を流用すること。

### 52.4 次への移行
- 専用モード不具合の修正は別ブランチ `merge20/ded-mode-fix` で進行中（実行 45929416749 は completed／success。head `9dd0ac29`。子の完了報告待ち）。F1（`merge20/form-f1`）も進行中。
- パネル修正入りの次ビルドは、専用モード修正のマージ後に master の最新 CI 成果物から出す（2026-11-27〜11-28 を目指す）。

## §53 専用モード不具合の修正（PR #46）— マージ・CI 検証済み（2026-11-27）

### 53.1 原因（コード実読で確定）
**主因：自動隠しタイマが専用バー（DedBar）を入場約 3 秒後に隠し、復帰手段が目に見えない。**
- `Src/MergeEditFrm.cpp` `EnterDedicatedMode()` はバーを表示するが、直後に `IDT_DEDHIDE`（500ms ポーリング）を開始する。`OnTimer()` はピン固定（`OPT_MERGE20_DED_PINNED`）でない限り、flash 猶予 1.6 秒経過＋カーソル離脱 3 秒で `ShowControlBar(&m_wndDedBar, FALSE, FALSE)` を呼ぶ。通常の入場ではカーソルは表示メニュー付近（バーから離れた位置）にあるため、**入場から約 3〜3.5 秒でバーが消える**。
- 再表示は子ウィンドウ上端 8px のホバーストリップのみで、画面上に何の手がかりもない。D11 で見えるストリップは不採用とされた（§37.2）。バーが消えると専用モードの唯一の UI がなくなり、残る可視の変化は隠れた下段ペイン（Diff Pane）だけで「下段の diff 画面が消えただけで、現状の比較と何も変わっていない」「何をしても何も表示されない」に見える。§24.6.1(c) が D2 ビルドで同じ訴えが出ると予測していたとおり。
- D11 の初回ピン固定（`OPT_MERGE20_DED_PIN_FIRST`、既定 true）は文字どおり最初の 1 回の入場しか守らず、2 回目以降は設計上ピン OFF（自動隠し）に戻る（§37.7 項目 16 の期待どおり）。さらに次の経路でもピンは外れる。
  - ピンボタンの手動 OFF（以後すべての入場が自動隠しになる）。
  - セッション復元：`Src/Merge20/Merge20SessionDoc.cpp:506-507` `ApplySessionStates` が `OPT_MERGE20_DED_PINNED` へ `session.display.dedPinned`（`Merge20Session.h:165` で既定 false。D11 以前のセッションファイルには項目自体がない）を書き戻す。タイマがキーをライブで読むため、**開いている専用モードのフレームのバーも**次のティックで隠れる。
- **実読で除外したもの**：ボタン→コマンド対応（`resource.h` の IDC 1745〜1756 は `Merge20DedBarCommands` の想定順と一致。`DIFFS_ONLY` 1805 は別レンジで処理）、バーの高さ（`LoadState`／`CalcFixedLayout` で 32px に固定）、ボタンの生成（`OnCreate` で 13 ボタン＋ファイル情報ラベルを生成し `LayoutButtons` で配置）、タイマ ID の衝突（`IDT_DEDHIDE`=4 は 2・3 と別。基底クラスに `OnTimer` なし）、出荷 `.po`（品質修正版で欠落 0 件を確認済み §50）。品質修正版ビルド（94956238）から現 master（54b369d1）の間に `MergeEditFrm.cpp`／`Merge20DedBar.*` の変更はなく（compare API で確認）、不具合は master にそのまま存在した。

### 53.2 変更一覧（ブランチ `merge20/ded-mode-fix`、base master `54b369d1`。1 ファイル 1 コミット）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/MergeEditFrm.cpp` | `9dd0ac29ee` | `EnterDedicatedMode()` で入場時に毎回 `OPT_MERGE20_DED_PINNED` を true にする。D11 の `OPT_MERGE20_DED_PIN_FIRST` による 1 回限りの条件をやめた。キー自体は定義を残し（`OptionsDef.h`／`OptionsInit.cpp` 不変）、初回限定ピンに戻せるようにした。ピンボタンはセッション内では従来どおり使える（手動 OFF でそのセッション中は自動隠し）。再入場すればバーは再び表示されるため、ユーザーがバーを見失ったままにならない。新規 UI 文字列なし（`.po`・`.rc` 不変） |

- **意図的な既定の変更（コーディネーター判断）**：D2 受け入れテスト §24.3 項目 8「既定では入場から数秒後にバーが隠れる」と D11 §37.7 項目 16「2 回目以降の入場ではピン OFF」は、この修正で変わる（入場時は常にピン ON）。自動隠し機能そのものは残る（手動ピン OFF＋ホバーストリップは従来どおり）。実機で 2 回報告された「バーが消えてモードが壊れて見える」不具合を直すための変更。D2／D11 の受け入れ項目の文言は、別途の見直しで更新すること。
- **残存する穴（別 PR での検討）**：セッション復元は開いている専用モードのフレームのピンを今も外しうる（§53.1）。その場合は表示メニューから専用モードを出て入り直すとバーは戻る。復元側の対処（開いている専用フレームがある間はピンを書き戻さない等）は別 PR で検討する。

### 53.3 検証レポート（コーディネーター検証済み）
- **PR #46**：open・mergeable=true・mergeable_state=clean、head `9dd0ac29`、base master。1 コミット、+16/−12、変更ファイル 1 件。
- **CI（ブランチ head）**：実行 45929416749、completed／success。全文ログ（3,554,424 バイト、ジョブ `build` 140278311299）で GoogleTest `[ PASSED ] 763 tests.`、`[ FAILED ]` 行なし、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件。ログ中の `error` 12 件はすべてビルドスクリプトの外部ダウンロード失敗を示す `echo ERROR: Failed to download ...` 行（マニュアル等）で、ビルドエラーではない。全文ログは `~/workspace/.coord/ded46_final_log.txt` に保存。`.po` 番兵も通過（`Merge20 strings present in the bundled Japanese.po.`）。
- **成果物（実行 45929416749）**：`WinMerge-x64.zip` 26,472,194 バイト（ID 15168631599）、`WinMerge-x64-Setup-x64.exe` 39,919,332 バイト（ID 15168462265）。いずれも未失効。
- **マージ**：マージコミット `85ceb9f6`。マージコミット CI：実行 45931878653、completed／success（head_sha=85ceb9f6 と一致）。
- **受け入れ条件の照合**：
  - (a) 専用モードに入ると DedBar が表示される — コード上は満たす（入場時に毎回ピン ON。バーが自動で隠れなくなる）。**実機未検証**。
  - (b) 終了／前へ／次へ／確認内容×5／ピン／未確認のみ／スナイパー／倍率が使える — コード上は満たす（配線は従来どおりで変更なし。ボタン→コマンド対応を実読で確認済み）。**実機未検証**。
  - (c) 専用モードを出ると通常表示に戻る — コード上は満たす（`ExitDedicatedMode` 不変。5 バー・最大化状態・入場前倍率を復元）。**実機未検証**。
  - (d) 回帰なし — CI 上は満たす（GoogleTest 763 件全パス、ビルド成功）。**実機未検証**。
- **未検証（実機でしか確認できないもの）**：バーが実際に入場時に表示され続け、ボタンが押せること。ピン OFF 時の自動隠しと上端ホバーでの再表示。終了ボタンでの通常モードへの復帰。入場／退場の繰り返しでの表示崩れの有無。実機確認手順は §24.6.4／§37.7 項目 1・2・16 に準じること。

### 53.4 次への移行
- 優先修正 2 件（パネル連携 PR #45、専用モード PR #46）は両方とも master に入った。次ビルドはこの master の最新 CI 成果物から出す（2026-11-27〜11-28 を目指す）。
- F1（`merge20/form-f1`）は進行中（CI 実行 45931838629 が in_progress。head `1ba719f2`）。

## §54 Wave 5 F1 — 照合・分類（安定キー、移動／改名、ノイズ除去、グルーピング、深刻度）— マージ・CI 検証済み（2026-11-27）

### 54.1 内容（設計書 §10 F1）
- 新規純粋モジュール `Merge20FormDiff.h/.cpp`（MFC 非依存・`_()` なし）と GoogleTest 19 件を追加。既存コードの変更は `.vcxproj`／`.filters` の登録のみ。
- 安定キー（§5.1）：`structPath + tag + name`（無名要素は属性署名）。重複名の順序問題を解消。
- 移動／改名（§5.1）：`Moved`／`Renamed` を 1 件に集約。改名は「同じ位置」条件付き（新旧の矩形が重なること。重ならなければ削除＋追加のまま＝§5.1 テスト4）。同親内の順序入替えのみは無視（T-F-2）。挿入による兄弟インデックスのずれは stage 1.5 で吸収。
- ノイズ除去（§5.2）：0.5 未満の座標ずれは無視（件数を `ignoredNoiseCount` に記録）。0.5 以上 1.0 未満は `minor` グループに分離。`diffVal` は小数第1位まで保持（100 → 112.5 は 12.5。HTML版は 13 に丸める）。
- グルーピング（§5.3）：同一親・同一方向・同一量の座標変更を `G1`… にまとめ、要約文を自動生成（例：「field 5項目が下に12単位移動」）。
- 深刻度（§5.4）・大幅変動（§4.4）：`Merge20ClassifyFormSeverity`／`Merge20ApplyLargeChangeFlag`（HTML版と同じ規則：平均×1.5 かつ >20）。
- `getDetailInterpretation` の移植：座標系・フォント・バージョン・汎用の文言を日本語で生成。HTML版との意図的な差異：要素テキストの変更を `Mod` として検出、無名要素の属性変更は構造パス＋タグで対応付け、傾向に `移動(N)`／`改名(N)` を追加。

### 54.2 変更一覧（ブランチ `merge20/form-f1`、base master `54b369d1`。7 ファイル 7 コミット）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20FormDiff.h` | `ad21b152` | 新規（9,938 バイト） |
| 2 | `Src/Merge20/Merge20FormDiff.cpp` | `f24aa767` | 新規（49,768 バイト） |
| 3 | `Testing/GoogleTest/Merge20/Merge20FormDiff_test.cpp` | `2e5b23d7` | 新規。単体テスト 19 件 |
| 4 | `Src/Merge.vcxproj` | `c1c72ec7` | 新規ファイルの登録 |
| 5 | `Src/Merge.vcxproj.filters` | `3568b69a` | 同上 |
| 6 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj` | `3aad417e` | 新規ファイルとテストの登録 |
| 7 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters` | `1ba719f2` | 同上（ブランチ head） |

- 全ファイル ASCII のみ（日本語リテラルは `\u` エスケープ）。`Merge.vcxproj` は CRLF、`UnitTests.vcxproj` は LF の原本どおり保持し、4 ファイルとも XML として有効、diff は追加行のみであることを確認済み。

### 54.3 検証レポート（コーディネーター検証済み）
- **PR #47**：open・mergeable=true・mergeable_state=clean、head `1ba719f2`、base master。7 コミット、+2059/−0、変更ファイル 7 件。
- **CI（ブランチ head）**：実行 45931838629、completed／success。全文ログ（3,557,181 バイト、ジョブ `build` 140286000849）で GoogleTest `[ PASSED ] 782 tests.`（F0 時点 763 件 → +19 件＝F1 のテスト）、`[ FAILED ]` 行なし、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件。全文ログは `~/workspace/.coord/f1_final_log.txt` に保存。
- **成果物（実行 45931838629）**：`WinMerge-x64.zip` 26,472,146 バイト（ID 15168459403）、`WinMerge-x64-Setup-x64.exe` 39,921,437 バイト。有効期限 2027-02-25。
- **マージ**：マージコミット `5b48ec8e`。マージコミット CI：実行 45933389139、completed／success（head_sha=5b48ec8e と一致）。
- **受け入れ条件の照合（設計書 §10 F1）**：
  - (a) §5.1 テスト1〜4、§5.2 テスト1〜3、§5.3 テスト1〜3 がすべて単体テストで合格 — 合格。GoogleTest 19 件に含まれ、CI で全合格。ローカル g++ ハーネスでも 145 チェック全合格。
  - (b) HTML版 `executeDiff` の ground truth との突き合わせ — 合格。HTML版の関数そのものを Node＋jsdom で verbatim 実行した結果とフォークの出力を 20 の XML 対で比較し、142 チェック、失敗 0 件（移動・改名・ノイズ除去・グルーピング・テキスト検出・無名要素の改善分を除く）。
  - (c) 影響分析の文言（`desc`／`impact`）が日本語で、HTML版の `getDetailInterpretation` と同等 — 合格。上記の突き合わせで確認（数値の丸め差 ±0.5 以内）。
  - (d) 回帰なし — 合格。新規モジュールの追加のみ。GoogleTest 全体が 782 件合格し、既存テストの失敗は 0 件。
- **検証の証拠**：`~/workspace/goals/muse-merge-rebuild-competing-with-gemini-neon/hidden_files/f1-work/` に保存（`f1_ci_log.txt`＝CI 全文ログ、`fork_out.json`／`html_gt.json`＝突き合わせ入出力、`compare_gt.py`、`pairs.json` など）。
- **未検証（実機でしか確認できないもの）**：F1 は純粋モジュールで UI・永続化がないため、実機で確認できる動作はない。実機確認は「CI 成果物の `WinMergeU.exe` が従来どおり起動すること（回帰確認）」のみ。`wchar_t` 2 バイト時のサロゲートペア処理は F0 からの継承で未検証。

### 54.4 次への移行
- F2（フォルダ比較からの起動＋一覧）はブランチ `merge20/form-f2`（base `5b48ec8e`）で進行中。
- UI 見直しの設計と 8 本の柱のギャップ分析も並行で進行中（いずれもコードを書かない設計・分析タスク）。

## §55 UI 見直し U0 — ビジュアル基盤（テーマ定数・共有ヘルパー）— マージ・CI 検証済み（2026-11-27）

### 55.1 内容（UI 見直し設計書 §4 U0）
- 新規 `Src/Merge20/Merge20Theme.h/.cpp`（MFC 依存のため `UnitTests.vcxproj` には登録しない。DedBar と同じ扱い）。WizMerge アクセント定数（設計書 §2.1 の RGB 値。ライト／ダーク両対応）、グラデーション描画・角丸描画・太字フォント生成の共有ヘルパー。ダーク判定は既存の `DarkMode::isEnabled()`／`WinMergeDarkMode` を使う。
- U0 単体では呼び出し元がなく、既存の画面表示に変化はない。
- 実装した API（`namespace Merge20Theme`）：`IsDarkMode()`、`GetAccentColor(bool)`、`GetAccentGlowColor()`、`GetBrandBarStartColor/EndColor/UnderlineColor(bool)`、`GetReviewStateColor(ReviewState, bool)`、`GetRiskHighColor(bool)`、`FillGradient`、`DrawBrandBar(CDC*, CRect, bool)`、`DrawRoundedRect`、`CreateBoldFont(CFont&)`（所有権が明確な `CFont&` 受け取り型。タスクの `CreateBoldFont()` の趣旨に沿い、署名は U0 の裁量で決定）。
- ダークのリスク「高」赤文字 `#FB7185` は、設計書どおり U5 で実機確認する候補値としてコメントに明記。

### 55.2 変更一覧（ブランチ `merge20/ui-u0`、base master `951c1b07`。4 ファイル 4 コミット）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20Theme.h` | `9b261bd6` | 新規（4,432 バイト）。テーマ定数・ヘルパーの宣言 |
| 2 | `Src/Merge20/Merge20Theme.cpp` | `58fa2d2e` | 新規（4,712 バイト）。§2.1 の RGB 値と描画実装 |
| 3 | `Src/Merge.vcxproj` | `6f0c11b6` | 登録（+2 行。Merge20 グループ末尾） |
| 4 | `Src/Merge.vcxproj.filters` | `5114315c` | 登録（+6 行。`<Filter>Merge20</Filter>` 付き。ブランチ head） |

- 1 ファイル 1 コミット、連続コミットの間に sleep 3。両新規ファイルは ASCII のみ・LF。`.vcxproj`／`.filters` は CRLF を保持し、差分は追加行のみ（XML として有効であることを確認済み）。`UnitTests.vcxproj` には登録していない（MFC 依存のため）。
- 新規コマンド ID・オプションキー・UI 文字列なし（`.po`・`.rc` に変更なし）。
- **調査メモ**：`DarkModeLib.h`／`WinMergeDarkMode` の宣言ファイルはフォークのリポジトリツリー内に存在しない（全 6,997 エントリを確認）。`MainFrm.h` が `#include "DarkModeLib.h"` し、`MainFrm.cpp` が `DarkMode::isEnabled()`／`WinMergeDarkMode::GetDarkModeType` を使う既存の経路と同じ include を `Merge20Theme.cpp` で使っており、CI でコンパイル成功・`Merge20Theme.obj` 生成を確認済みのため、U0 としては問題ない。

### 55.3 検証レポート（コーディネーター検証済み）
- **PR #48**：open・mergeable=true・mergeable_state=clean、head `5114315c`、base master。4 コミット、+307/−0、変更ファイル 4 件。
- **CI（ブランチ head）**：実行 45936191008、completed／success。全文ログ（3,558,462 バイト、ジョブ `build` 140299831351）で GoogleTest `[ PASSED ] 782 tests.`（F1 時点の 782 件と同数で変化なし）、`[ FAILED ]` 行なし（ログ中の "FAILED" 4 件は `.po` のコメント行 `#. IDS_FILESAVE_FAILED` 等の誤検出）、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件。`Merge20Theme.cpp` が CL.exe のコマンドラインに含まれ、`/Yu"stdafx.h"` でコンパイルされ、`Merge20Theme.obj` が生成されたことをログで確認。警告 `warning C[0-9]` 850 件はすべて既存の googletest／xdiff 等由来で、Merge20 を含むものは 0 件。番兵「Merge20 strings present in the bundled Japanese.po.」も通過。全文ログは `~/workspace/.coord/u0_final_log.txt` に保存（子の保存した `hidden_files/u0-ci-log-45936191008.txt` のコピー）。
- **成果物（実行 45936191008）**：`WinMerge-x64.zip` 26,471,768 バイト（ID 15170404583）、`WinMerge-x64-Setup-x64.exe` 39,922,791 バイト（ID 15170793375）。いずれも未失効。
- **マージ**：マージコミット `e2bed6dc`。マージコミット CI：実行 45938370585、completed／success（head_sha=e2bed6dc と一致）。
- **受け入れ条件の照合（設計書 §4 U0）**：
  - (a) CI でコンパイルされ、GoogleTest 件数が変わらないこと — 合格（`Merge20Theme.obj` の生成を確認。782 件全パスで件数変化なし）。
  - (b) 定数の RGB 値が §2.1 の表と一致すること — 合格（ブランチ head のコミット済みファイルから RGB リテラルを全抽出して照合。すべて一致）。
  - (c) 既存の画面表示に変化がないこと — コード上は合格、**実機未検証**（U0 は描画を呼ぶコードを一切追加していないため、表示が変わる経路が存在しない）。
- **未検証（実機でしか確認できないもの）**：実機でファイル比較・フォルダ比較を開き、見た目が変わっていないこと。ダークモード切替時の描画（U0 単体では描画を呼ばないため、実機確認は U1・U5 で行う）。`DarkMode::isEnabled()` の実機での true/false。
- **ツール不具合の記録**：`github_api.py` の `pr` ヘルパーは head が `merge20/phase1` に固定されており、他ブランチの PR 作成には使えない（422 になる）。U0 の子は直接 API 呼び出しで PR を作成した。今後のスライスも同じ手順を使うこと。

### 55.4 次への移行
- U1（ブランド帯）はブランチ `merge20/ui-u1`（base `e2bed6dc`）で進行中。
- F2（`merge20/form-f2`）と B1（`merge20/wf-b1`）も進行中（いずれも CI 実行中）。

## §56 最高峰ワークフロー B1 — 差分単位の明示的結論 — B1a・B1b マージ・CI 検証済み（2026-11-27）

### 56.1 内容（ギャップ分析書 §4.2 B1。北極星の第 1 波の最初）
- 差分ごとに結論状態（未判断／旧採用／新採用／両方採用／不採用／現状維持）を持たせる。結果ペインの差分に状態を表示し、D4 フォーカスドックの判定ボタンと結果ペインの右クリックメニューから結論を付けられるようにする。結論を付けたらファイルの確認内容を連動させる（HTML の `setHunk` と同じ規則：不採用以外は出力チェック ON・TODO→DONE、不採用は出力チェック OFF・確認内容は変更なし。ただし 2-way の新のみファイルは S2 の規則どおり ON にしない）。ファイル比較の下部またはステータスバーに「判断済み X／Y 差分」を表示する。セッション（S8 の `diffJudgments`）に結論状態を保存・復元する。学習候補の受け入れも同じ結論として扱い、同じ連動規則に乗せる。
- B1 は 2 PR に分けて実装した（B1a：状態＋表示＋連動、B1b：セッション保存・復元＋学習受け入れの統合）。

### 56.2 B1a — 変更一覧（ブランチ `merge20/wf-b1`、base master `951c1b07`。18 コミット・17 ファイル）
- 内容：結論 6 状態の純粋モジュール `Merge20Conclusion.h/.cpp`、結果ペイン右クリック「差分の結論」サブメニュー、D4 フォーカスドック判定ボタンからの付与、ヘッダの「判断済み X／Y 差分」表示、結果ペインのマージン表示（現状維持=K・不採用=X）、唯一の連動規則 `ApplyMerge20ConclusionLinkage`（不採用以外は出力チェック ON・TODO→DONE、不採用は出力チェック OFF・確認内容は変更なし。2-way 新のみファイルは S2 規則どおり ON にしない）、分割・再結合での結論引き継ぎ、Rescan でクリア、`.po` 新規 7 件。
- **PR #50**：`B1a: per-diff explicit conclusion (state + display + linkage)`。18 コミット、+801/−28、変更ファイル 17 件。

### 56.3 B1a — 検証レポート（コーディネーター検証済み）
- **PR #50**：open・mergeable=true・mergeable_state=clean、head `9e9c8493`、base master。
- **CI（ブランチ head）**：実行 45938220865、completed／success。全文ログ（3,565,258 バイト、ジョブ `build` 140306248995）で GoogleTest `[ PASSED ] 793 tests.`（F1・U0 時点 782 件 → +11 件＝新規 `Merge20Conclusion` テスト全 OK）、`[ FAILED ]` 行なし、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件。`.po` 番兵「Merge20 strings present in the bundled Japanese.po.」も通過。全文ログは `~/workspace/.coord/b1a_final_log.txt` に保存。
- **成果物（実行 45938220865）**：`WinMerge-x64.zip` 26,476,258 バイト、`WinMerge-x64-Setup-x64.exe` 39,923,456 バイト。いずれも未失効。
- **マージ**：マージコミット `e37ed445`。マージコミット CI：実行 45941175196、completed／success（head_sha=e37ed445 と一致）。

### 56.4 B1b — 変更一覧（ブランチ `merge20/wf-b1b`、base 当初 `merge20/wf-b1`→マージ後に master へ付け替え。7 コミット＋CI 起動用空コミット 1 件）
- コミット：`cffb7107` Merge20Types.h（`conclusion` 追加）、`55697733` Merge20Session.cpp（任意フィールド `"conclusion"` の保存・読込、v1 互換・範囲外は差分のみ破棄）、`7ffacd16` Merge20Session_test.cpp（`ConclusionRoundTrip` テスト）、`547ca2a2` MergeDoc.h（`Merge20SetDiffConclusionRestored` 宣言）、`9903fbe2` Merge20SessionDoc.cpp（結論の抽出・復元＋ドック刷新）、`b2e7b8db` MergeDocResultPane.cpp（復元用セッター＋学習受け入れの結論化）、`b13cce12` Merge20FocusDock.cpp（差分未選択時も進捗表示）。
- 学習受け入れの写像：InsertOld／ReplaceWithOld→旧採用、BothOldNew→両方採用、KeepNew→削除ブロックなら不採用、それ以外は新採用。受け入れは結論を格納し B1a と同一の `ApplyMerge20ConclusionLinkage` を呼ぶ。学習記録は既存ループのみ（重複記録なし）。
- 新規 UI 文字列なし（`.po` 変更なし）。
- **スタック PR の扱い**：`.github/workflows/main.yml` は `pull_request: branches: [ master ]` のみを対象とするため、base が `merge20/wf-b1` の #51 では CI が起動しなかった。B1a マージ後に base を master に付け替え（API PATCH）、さらに base 付け替えだけではワークフローが起動しないため、調整役が空コミット `af10121e`（`Trigger CI for B1b (base retargeted to master)`）をブランチに作って CI を起動した。
- **PR #51**：`B1b: session save/restore of diff conclusions + learning acceptance as conclusion`。7 コミット＋空コミット 1 件。

### 56.5 B1b — 検証レポート（コーディネーター検証済み）
- **PR #51**：open・mergeable=true・mergeable_state=clean、head `af10121e`、base master。
- **CI（ブランチ head）**：実行 45941445600、completed／success。全文ログ（3,565,481 バイト、ジョブ `build` 140316449671）で GoogleTest `[ PASSED ] 794 tests.`（B1a 時点 793 件 → +1 件＝新規 `ConclusionRoundTrip`）、`[ FAILED ]` 行なし、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件。全文ログは `~/workspace/.coord/b1b_final_log.txt` に保存。
- **成果物（実行 45941445600）**：`WinMerge-x64.zip` 26,478,985 バイト（ID 15173467401）、`WinMerge-x64-Setup-x64.exe` 39,926,291 バイト（ID 15173357847）。いずれも未失効。
- **マージ**：マージコミット `b13b7ab0`。マージコミット CI：実行 45942672719、completed／success（head_sha=b13b7ab0 と一致）。

### 56.6 受け入れ条件の照合（ギャップ分析書 §4.2 B1）
- (a) 任意の差分に 6 状態を付けられる — 合格（B1a。メニュー＋ドック判定ボタン）。CI 緑。
- (b) 「判断済み X／Y 差分」表示 — 合格（B1a。ドックヘッダ。差分未選択時も B1b で表示）。CI 緑。
- (c) 連動が S2 のファイル単位規則と一致 — 合格（B1a。唯一の連動関数）。CI 緑。
- (d) セッション保存・復元で結論が戻る — 合格（B1b。単体テスト `ConclusionRoundTrip` あり）。CI 緑。
- (e) 学習受け入れも同じ結論・同じ連動規則 — 合格（B1b）。CI 緑。
- **未検証（実機でしか確認できないもの）**：実機での結論付与→確認内容・出力チェックの連動、進捗表示、分割・再結合後の結論。実機でのセッション保存→再起動→復元（結論・進捗の反映）。実機での学習候補の受け入れ（結論マーカー・確認内容・出力チェックの連動）。実機での日本語表示（`.po` は CI のバンドルチェックで確認済み）。

### 56.7 次への移行
- B1 の完了により、北極星の第 1 波は B9（終わりの一覧確認）→ B2（未判断警告ゲート）へ進む。B2 は S7 未着手のため終了時警告だけ先行実装する選択肢がある（ギャップ分析書 §5）。
- F2（`merge20/form-f2`）は CI 失敗が続いており、子エージェントに原因（`Merge20FormCompareDoc.cpp:272,289` の `SetTitle` への `String` 渡し。`.c_str()` が要る）を伝えて修正中。
- U1（`merge20/ui-u1`）は進行中。

## §57 Wave 5 F2 — フォルダ比較からの起動＋一覧（KPI・フィルタ・検索・並び替え・ナビゲータ）— マージ・CI 検証済み（2026-11-27）

### 57.1 内容（設計書 §10 F2）
- `CFormCompareDoc`／`CFormCompareView` の骨格。フォルダ比較の右クリックメニューとメインメニューからの起動（§3.2）。2フォルダの走査（`.xml` だけを対象）。KPIダッシュボード（7つ）、フィルタタブ（5つ）、検索、並び替え（ファイル名順・数値変動の昇順・降順・変更箇所数の降順・深刻度順）、左ナビゲータ、帳票カードの一覧（ヘッダ部分のみ。キャンバス・差分テーブルは F3・F5）。
- 純粋モジュール `Merge20FormList.h/.cpp`（KPI・フィルタ・検索・並び替え）と単体テスト 20 件。

### 57.2 変更一覧（ブランチ `merge20/form-f2`、base master `5b48ec8e`。35 コミット：実装 25＋修正 9＋マージ 1。22 ファイル、+2233/−4）
- 新規 9 ファイル：`Src/Merge20/Merge20FormList.h/.cpp`、`Merge20FormCompareDoc.h/.cpp`、`Merge20FormCompareFrame.h/.cpp`、`Merge20FormCompareView.h/.cpp`、`Testing/GoogleTest/Merge20/Merge20FormList_test.cpp`。
- 変更 13 ファイル：`Src/Merge.h/.cpp`、`DirView.h/.cpp`、`MainFrm.h/.cpp`、`resource.h`、`Merge.rc`、`Merge.vcxproj[.filters]`、`UnitTests.vcxproj[.filters]`、`Translations/WinMerge/Japanese.po`。
- 主なコミット：`d04a448f` FormList 純粋モジュール／`2b9fab58` 同実装／`ff3307fa` 単体テスト 20 件／`e73ef688` `c4bab675` CFormCompareDoc 宣言・実装／`95c89a51` `ac6089c3` CFormCompareFrame 宣言・実装／`60022a8e` `41596145` CFormCompareView 宣言・実装／`0e2b5070` `809637f2` テンプレート登録（CMergeApp）／`70eb4117` `7ab794a2` フォルダ比較右クリックメニュー配線／`1541945e` `a14cc361` メインメニュー単独起動＋言語切替更新／`b5fd7557` `bb1413ff` `UpdateResources` 宣言・実装／`fe797fdb` ID 追加／`3d0c6540` メニュー項目 2 か所／`a82733ca` `.po` 42 件追加／`4aa3d31c` 付与の修正／`4c72db6e` `dba91d3c` Merge.vcxproj 登録／`c393b1f3` `c25adcda` UnitTests.vcxproj 登録。
- **マージコミット `132660f8`**：master `b13b7ab0` をブランチに取り込み、競合 2 件を解消。`ID_MERGE20_FORM_COMPARE` を 38970→**38976** へ移動（B1 が 38970 を取っていたため）、`_APS_NEXT_COMMAND_VALUE` 38976→38978、`_APS_NEXT_CONTROL_VALUE` 1837→1858、台帳コメント更新、`.po` は B1 の 7 件＋F2 の 42 件を両方保持。その他 5 ファイルは自動マージで両方の追加を保持。

### 57.3 CI 失敗の経緯と教訓（3 回失敗→修正→成功）
1. 実行 45938139030：C1010（MFC の 3 ファイルで `pch.h` が見つからない）＋ C2589（`std::min` が windows.h の `min` マクロと衝突）→ `pch.h` 化と `(std::min)` ガードで修正。
2. 実行 45939326103：C2504（`CDocument`／`CView` 未定義。`pch.h` には MFC ヘッダが無いことが判明）→ MFC ファイルはプロジェクト既定の PCH（`StdAfx.h`、vcxproj は自己完結エントリ）に戻し、純粋モジュール `FormList.cpp` のみ `pch.h` グループに残す形で修正。
3. 実行 45940604738：C2664（`SetTitle` に `String` を渡した。2 件のみ）→ 両箇所に `.c_str()` を付けて修正（調整役がログから原因を特定し、子エージェントに送付）。
- **教訓（今後の実装者へ）**：MFC の Doc／Frame／View はプロジェクト既定の PCH（`StdAfx.h`）を使うこと。`pch.h` グループに登録するのは STL のみの純粋モジュールだけ（`pch.h` には MFC ヘッダが無い）。`String` を MFC の `LPCTSTR` 引数に渡すときは `.c_str()` が必須（`SetTitle`、`SetWindowText` 等）。resource.h の ID 割り当て時は台帳コメントの通りに全 define を値順で確認し、`_APS_NEXT_*` も一緒に進めること（S3 の CVT1100 教訓）。B1 と F2 が同じ 38970 を取ったのは、両ブランチが同じベースから同じ「次の空き」を使ったため。並行ブランチでは ID の割り当てを調整役が事前に調整すること。

### 57.4 検証レポート（コーディネーター検証済み）
- **PR #49**：open・mergeable=true・mergeable_state=clean、head `132660f8`、base master。35 コミット、+2233/−4、変更ファイル 22 件。
- **CI（ブランチ head）**：実行 45942948552、completed／success。全文ログ（3,568,846 バイト、ジョブ `build` 140321139203）で GoogleTest `[ PASSED] 814 tests.`（F0 時点 763 ＋ F2 の 20 ＋ その間に master へ入った B1／B1b の 31）、`[ FAILED]` 行なし、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件。全文ログは `~/workspace/.coord/f2_final_log.txt` に保存。
- **成果物（実行 45942948552）**：`WinMerge-x64.zip` 26,562,769 バイト（ID 15174500188。内側 `wizmerge-2.16.60-7fb950a-x64-exe.zip`、SHA256 `10766ea719126a89c5f06abddd50ce41f678d4d8c152260609b4d474fe528f13` は子の確認値）、`WinMerge-x64-Setup-x64.exe` 40,020,579 バイト（ID 15174425338）。いずれも未失効。
- **成果物の `.po` 監査（子の確認値）**：バンドルされた `WizMerge/Languages/Japanese.po`（2,243 msgids）に F2 の 42 件がすべて残っていることを確認（サンプル 10 件＋2 件を個別確認。ブランチ側にあって成果物に無い msgid は 0 件）。番兵「Merge20 strings present in the bundled Japanese.po.」も通過。
- **マージ**：マージコミット `df925a7c`。マージコミット CI：実行 45944878092、completed／success（head_sha=df925a7c と一致）。
- **受け入れ条件の照合（設計書 §10 F2）**：
- (a) フォルダ比較画面から「FORM比較（見るだけ）…」を選ぶと、左右フォルダが引き継がれて FORM比較画面が開き、解析が実行される — コード上は実装済み（右クリックメニュー＋ハンドラ＋ドキュメントテンプレート＋2 フォルダ走査）。**実機未検証**。
- (b) KPI の7つの数値が HTML版の `updateKpiBadges` と同じ定義 — 合格（単体テストで定義を固定し、CI で 20 件すべてパス）。実機での表示確認は未検証。
- (c) フィルタタブ（5つ）・検索・並び替えが正しく動くこと。完全一致のファイルが一覧に出ないこと — 合格（単体テストで固定し、CI でパス。完全一致はドキュメントが結果から除外し、KPI の「完全一致」で数える）。実機での操作確認は未検証。
- (d) 既存のフォルダ比較・ファイル比較の回帰がないこと — 合格（変更はメニュー項目の追加と新規ドキュメントテンプレートのみ。CI の全 814 テストがパス）。実機での操作確認は未検証。
- **未検証（実機でしか確認できないもの）**：実機での起動（フォルダ比較→右クリック、メインメニュー→ツール、フォルダピッカー 2 回）、KPI／フィルタ／検索／並び替えの操作、日本語表示。
- **既知の制限**：大規模フォルダでの解析は UI スレッドで同期実行するため解析中は UI がブロックされる（プログレスバーは今後の課題）。カード一覧は F2 ではレポート形式のヘッダのみ。キャンバス描画は F3、差分テーブルは F5。帳票名は F6 の名称マスタまで「（名称未登録）」。3 フォルダ比較からは外側 2 フォルダのみ引き継ぐ（中間フォルダは使わない）。

### 57.5 次への移行
- F3（ビジュアルプレビュー）はブランチ `merge20/form-f3`（base `df925a7c`）で進行中。F4・F5 は F3 のキャンバスの上に載せるため、F3 のマージ後に起動する。
- U1（`merge20/ui-u1`）は進行中（CI 実行中）。B9（`merge20/wf-b9`）は進行中。

## §58 UI 見直し U1 — WizMerge ブランド帯（アプリ内ヘッダ）— マージ・CI 検証済み（2026-11-27）

### 58.1 内容（UI 見直し設計書 §4 U1。最優先）
- 新規 `Src/Merge20/Merge20BrandBar.h/.cpp`（`CSizingControlBarCF` 派生。`SCBS_EDGETOP | SCBS_EDGEBOTTOM`、高さ 30px 固定。`LoadState`／`CalcFixedLayout` で 30px にクランプする。D1／D2 の教訓：保存値・ドラッグによる肥大化を防ぐ）。
- `OnPaint` でオーナードロー：左端に光るドット（HTML のトップバーと同じシアンの丸）＋太字「WizMerge」＋バージョン（`theApp` のバージョン文字列を流用）＋右端に進捗サマリ（アクティブなフォルダ比較があれば「未確認 N件／済 M件」。S3 の `GetMerge20StatusCountText()` と同じ定義。フォルダ比較がなければ「ファイル比較」等の状態文言または空欄）。ダーク時は §2.1 のダーク配色、ライト時はライト配色。下線 2px はアクセント色。U0 の `Merge20Theme` ヘルパーを使う。
- `CMainFrame::OnCreate` で ReBar（`CreateToolbar()`）より先に生成し、最上部（`AFX_IDW_DOCKBAR_TOP`）へドックする。既存の ReBar・タブバー・ステータスバーの生成順は変えない。
- 新規オプションキー `OPT_MERGE20_BRAND_HEADER`（`Merge20/BrandHeader`、既定 true）。View メニューに「ブランドヘッダ」トグルを追加（既存の `OnBarCheck`／`OnUpdateControlBarMenu` の前例で表示／非表示＋チェック表示。トグルは専用キーに保存）。非表示時は `OPT_REBAR_STATE` 等の既存の保存状態に影響しないこと（ブランド帯の表示状態は専用キーだけで管理する）。
- 右端の進捗サマリは 500ms タイマで更新する。ブランド帯は DedBar と同じく下辺ドラッグが可能だが、高さは 30px にクランプされるためクライアント領域を奪えない。下部ドックへの移動は DedBar と同じ扱い（ドラッグ可、クリックで再ドック）。

### 58.2 変更一覧（ブランチ `merge20/ui-u1`、base 当初 `e2bed6dc`→現 master `b13b7ab0` へリベース→マージコミットで現 master `66c4fee4` を取り込み。12 コミット＋マージ 1 件）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/resource.h` | `e4fa799c` | `ID_VIEW_MERGE20_BRAND_HEADER` **38978** を追加（当初 38976。リベース時に変更。§58.3 を参照）。`_APS_NEXT_COMMAND_VALUE` を 38978→38979 へ更新。予約コメントを更新 |
| 2 | `Src/OptionsDef.h` | `39e43cb1` | `OPT_MERGE20_BRAND_HEADER`（`Merge20/BrandHeader`）を追加 |
| 3 | `Src/OptionsInit.cpp` | `1c359b50` | 既定 true で初期化 |
| 4 | `Src/Merge20/Merge20BrandBar.h` | `0376b9a5` | 新規。`CSizingControlBarCF` 派生、30px 固定 |
| 5 | `Src/Merge20/Merge20BrandBar.cpp` | `bec950e1` | 新規。オーナードロー（光るドット＋太字「WizMerge」＋バージョン＋右端進捗サマリ、500ms タイマで更新） |
| 6 | `Src/DirView.h` | `54c666c2` | `GetMerge20StatusCountText()` を private から public へ移動（実装変更なし） |
| 7 | `Src/MainFrm.h` | `40b66641` | `m_wndBrandBar` メンバ、`OnBarCheck` override 宣言、ヘッダ include |
| 8 | `Src/MainFrm.cpp` | `43834dd0` | `OnCreate` で ReBar より先に生成・最上部へドック、`OnBarCheck`（専用キーへ保存）、`ShowOutputPane`／`SavePosition`／`UpdateResources` への配線 |
| 9 | `Src/Merge.rc` | `d459b905` | 3 つの View メニュー（IDR_MAINFRAME／IDR_DIRDOCTYPE／IDR_MERGEDOCTYPE）に「Brand Header」を「Status Bar」の直後へ追加 |
| 10 | `Translations/WinMerge/Japanese.po` | `e8717784` | 新規 2 件：`Brand Header` → `ブランドヘッダ`、`File Compare` → `ファイル比較`（msgid 2178→2180 件） |
| 11 | `Src/Merge.vcxproj` | `609efa43` | 新規ファイル 2 件を登録 |
| 12 | `Src/Merge.vcxproj.filters` | `11fd97dd` | 新規ファイル 2 件を Merge20 フィルタへ登録（ブランチ head） |
| 13 | （マージ） | `bf11f517` | master `66c4fee4` をブランチにマージ。競合解消は §58.3 のとおり |

### 58.3 リベースとコマンド ID の競合解消（調整役の決定）
- 初版は base `e2bed6dc`（U0 マージコミット）で作成したが、その後に master が進み（PR #50／#51 マージ）、PR が `mergeable: false`（競合）となり CI が起動しなかった。原因は `resource.h`（B1 のコマンド ID）、`Merge.rc`（Diff conclusion メニュー）、`Japanese.po`、`Merge.vcxproj`／`.filters` の競合。
- 現 master `b13b7ab0` へリベースした（ブランチを force-reset して 12 コミットを再適用）。リセット直後に差分が空になり PR が GitHub により自動クローズされたため、再開（reopen）している。再コミット後に `mergeable: true` を確認済み。
- その後さらに master が進み（F2 マージ `df925a7c`）、`ID_VIEW_MERGE20_BRAND_HEADER` に使っていた 38976 が F2 の `ID_MERGE20_FORM_COMPARE` と重複したため、PR は再び `mergeable: false`（dirty）になった。
- **調整役の決定：`ID_VIEW_MERGE20_BRAND_HEADER` は 38978 を使うこと。** 38977 は `IDS_SPLASH_GPLTEXT` のため使わない。`_APS_NEXT_COMMAND_VALUE` を 38978→38979 に更新し、D0 の予約コメントも更新する（38978 は U1 使用済み、次は 38979 から空き、38977 は `IDS_SPLASH_GPLTEXT` のため使用不可、と明記）。
- フォローアップの子が master `66c4fee4` をブランチにマージ（マージコミット `bf11f517`。リベースなし、force-push なし。U1 の 12 コミットは全てそのまま残っている）し、ID を 38976→38978 に変更した。`MainFrm.cpp`／`MainFrm.h`／`DirView.h`／`Merge.rc`／`Merge.vcxproj`／`.filters` は 3-way マージでクリーンマージ（U1 のブランド帯追加＋F2 の FORM 比較追加の両方が残っている）。master 側のみの変更 15 ファイルは master の blob をそのまま採用した。
- **コマンド ID の空きはこれで枯渇した。** `_APS_NEXT_COMMAND_VALUE` は 38979 になったが、38977 は `IDS_SPLASH_GPLTEXT` の値である（`_APS_NEXT_*` はリソースエディタ用のヒント値であり実リソースではないため、ビルド上の重複リソースは生じない。resource.h の予約コメントに明記済み）。**今後コマンド ID が要るスライスは、調整役の判断が必要。** 並行ブランチではコマンド ID の割り当てを調整役が事前に調整すること（§57.3 の教訓と同じ）。

### 58.4 master の `Japanese.po` 末尾の diff3 残骸（F2 マージ時の解消漏れ）
- **master の `Japanese.po` 最終行（11,990 行目）に diff3 の残骸 `||||||| base/Translations_WinMerge_Japanese.po` が残っていた**（調整役が blob API で直接確認済み。F2 マージ時の解消漏れと思われる）。この行は.po として不正な行である。
- U1 のマージでは U1 追加分を付ける際に当該行を除去したため、PR #52 のマージで master 側も直る。
- **教訓**：マージコミットでの競合解消は、解消後のファイル全体を目視・構文確認すること。特に `.po` は末尾まで確認すること。CI の番兵は文字列の存在だけを確認し、.po の構文不正は検出しない（msgfmt による厳密な構文チェックはこの VM には msgfmt がないため未実施。CI の番兵＋ビルド成功が代わりの証拠）。

### 58.5 検証レポート（コーディネーター検証済み）
- **PR #52**：open・mergeable=true・mergeable_state=clean、head `bf11f517`、base master。13 コミット（U1 の 12＋マージ 1）、+447/−7、変更ファイル 12 件。
- **CI（ブランチ head）**：実行 45947297955、completed／success。全文ログ（3,569,274 バイト、ジョブ `build` 140334818447）で GoogleTest `[ PASSED] 814 tests.`（前回の 794 件から +20 件＝F2 の FormList テスト分）、`[ FAILED]` 行なし、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件。番兵「Merge20 strings present in the bundled Japanese.po.」も通過。全文ログは `~/workspace/.coord/u1r_final_log.txt` に保存。
- **成果物（実行 45947297955）**：`WinMerge-x64.zip` 26,565,829 バイト、`WinMerge-x64-Setup-x64.exe` 40,024,452 バイト。いずれも未失効。
- **`.po` の件数監査（子の確認値）**：msgid 数：基底 2,178 → master 2,220（F2 +42 件）→ ブランチ 2,180（U1 +2 件）→ **マージ後 2,222 件（+44 件）**。マージ後の msgid 集合 ＝ 基底 ∪ F2 追加分 ∪ U1 追加分、と完全一致を確認。B1 の 7 件（`Diff conclusion`／`Undecided`／`Adopt old lines`／`Adopt new lines`／`Adopt both`／`Reject this diff`／`Judged %1 of %2 diffs`）は基底から引き継がれ、すべて残っている。F2 の 42 件・U1 の 2 件（`Brand Header`→`ブランドヘッダ`、`File Compare`→`ファイル比較`）もすべて残っている。構造チェック（全行がコメント／msgid／msgstr／継続行／空行であること）も通過。
- **マージ**：マージコミット `6d1767aa`。マージコミット CI：実行 45948536756、completed／success（head_sha=6d1767aa と一致）。
- **受け入れ条件の照合（設計書 §4 U1）**：
- (a) 起動直後に最上部にブランド帯が表示され、「WizMerge」が太字で読める — コード上は満たす。**実機未検証**。
- (b) フォルダ比較を開くと右端に進捗サマリが表示され、確認内容の変更で数が変わる — コード上は満たす（500ms タイマで再読込）。**実機未検証**。
- (c) View メニューで OFF／ON の永続化 — コード上は満たす（専用キーで永続化）。**実機未検証**。
- (d) ツールバー・メニューバー・タブバーの表示／非表示・サイズ変更でもレイアウトが崩れない — コード上は満たす。**実機未検証**。
- (e) 既存のファイル比較・フォルダ比較・専用モードが従来どおり動く — コード上は満たす。**実機未検証**。
- **未検証（実機でしか確認できないもの）**：(a)〜(e) すべて実機未検証。実機確認チェックリストは PR 本文に記載（起動直後の帯表示・ライト／ダーク両テーマ、フォルダ比較での件数表示と「済」による減少、ファイル比較時の「ファイル比較」表示と開始画面での空欄、OFF／ON の再起動後の永続化、ツールバー等の表示切替・サイズ変更、タブのタイトルバー表示 ON／OFF 両方、専用モード中の帯表示、出力ペイン切替時の帯表示状態）。CI の緑バッジ＋成果物確認は「ビルドが通った」ことの証拠であり、「見た目が変わった」ことの証拠ではないため、実機スクリーンショット相当の確認が必要。
- **既知の制限**：ブランド帯は DedBar と同じく下辺ドラッグが可能だが、高さは 30px にクランプされるためクライアント領域を奪えない。下部ドックへの移動は DedBar と同じ扱い（ドラッグ可、クリックで再ドック）。実機でのドラッグ挙動は未検証。

### 58.6 次への移行
- U5（バーのアクセント統一）はブランチ `merge20/ui-u5`（base `6d1767aa`）で進行中。
- F3（`merge20/form-f3`）は CI 失敗が 1 回あり、子エージェントに原因（`Merge20FormCanvasGeom_test.cpp:234` のアサーション構文）を伝えて修正中。
- B9（`merge20/wf-b9`）は進行中（CI 実行中）。

## §59 UI 見直し U5 — バーのアクセント統一（DedBar・ファイル一覧パネル・フォーカスドック）— マージ・CI 検証済み（2026-11-27）

### 59.1 内容（UI 見直し設計書 §4 U5）
- `CMerge20DedBar`：U0 のテーマヘルパーを使い、全 13 ボタンをオーナードロー化する（`BS_OWNERDRAW`＋`DrawItem`。D2 の `LayoutButtons` の配置・固定幅・高さ 32px は維持）。確認内容 5 ボタンを HTML の `.ded-rev` と同じ色付きピルにする（TODO 灰／DONE 緑／HOLD 黄／SPECIAL 赤／KEEP ティール。選択中は塗りつぶし＋白文字、非選択は枠線のみ＋淡色）。終了ボタン（「通常モードへ戻る」）はアクセント色の塗りつぶしにする。ピン／未確認のみ／スナイパー／差分のみは ON 時にアクセント塗りつぶし。バー背景は U0 の `Merge20Theme::DrawBrandBar`（グラデーション＋2px 下線）。`LayoutButtons` の配置・固定幅・高さ 32px・ボタン幅合計は不変。チェック状態はボタン自身が保持するため、フレーム側の setter・コマンド転送・自動隠し・ピンは不変。ファイル情報ラベルは透過背景＋テーマ文字色（残像防止のため `SetFileInfo` でバー再描画を追加）。
- `CMerge20FileListBar`：件数行・検索行・フィルタタブ行の底部にアクセント 2px 下線（各行の子コントロールの下端を 2px 上げ、下線帯を空けた）。フィルタタブを `TCS_OWNERDRAWFIXED`＋`DrawItem` で `.pill.on` 化（選択中だけアクセント枠＋不透明な淡色塗り、非選択はミュート色）。スリム・一括チェックボタンは配置・ID とも不変。
- フォーカスドック（D4）のヘッダストリップにアクセント下線を付ける（`Merge20FocusDock` の描画部のみ。ドックの動作は変えない）。
- テーマ切替時は `WM_SYSCOLORCHANGE`／`WM_SETTINGCHANGE` で再描画。ダークでは白文字／濃色文字を使い分け（ダークのアクセント塗りは濃色文字、ダークの TODO ピル等は明るくして可読性を確保）。
- `CSizingControlBarCF` の基底 `OnPaint` は空（`sizecbar.cpp` で実読確認）なので、DedBar／FileListBar の `OnPaint` 追加で既存の描画を置き換えることはない。

### 59.2 変更一覧（ブランチ `merge20/ui-u5`、base master `6d1767aa`。6 ファイル 6 コミット）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20DedBar.h` | `edad742` | オーナードロー宣言（OnDrawItem／OnPaint／OnCtlColor／テーマ変更時再描画） |
| 2 | `Src/Merge20/Merge20DedBar.cpp` | `cde873e` | ボタンのオーナードロー実装 |
| 3 | `Src/Merge20/Merge20FileListBar.h` | `7f30893` | 下線＋タブのオーナードロー宣言（行 rect 3 件を追加） |
| 4 | `Src/Merge20/Merge20FileListBar.cpp` | `0b479cc` | 下線＋タブのオーナードロー実装 |
| 5 | `Src/Merge20/Merge20FocusDock.h` | `ec019ce` | ヘッダ下線の宣言 |
| 6 | `Src/Merge20/Merge20FocusDock.cpp` | `c6f1c6f` | ヘッダ下線の実装（ブランチ head） |

- 新規コマンド ID・新規 UI 文字列なし（`.po`／`.rc`／`.vcxproj` 不変）。描画のみの変更。
- **任意項目（未実施）**：Merge20 追加ダイアログのタイトル行の帯は見送り。A-12（DedBar 左端のワードマーク）は未実施。D3 のファイル情報ラベル（300px）がすでに左端にあり、ワードマークを追加するとボタン幅の合計が増えて通常幅のウィンドウで終了ボタンがクリップされるため。D3 で解消済みの「ファイル名が見えない」問題でもあるため、追加の価値より回帰リスクが大きいと判断した。必要なら別途検討すること。
- 設計書 §4 U5 の「ボタン幅の合計（D2 時点で約 1010px）は変えない」は遵守した（ファイル情報ラベル 300px を含む総幅は D3 時点のまま）。

### 59.3 検証レポート（コーディネーター検証済み）
- **PR #56**：open・mergeable=true・mergeable_state=clean、head `c6f1c6fc`、base master。6 コミット、+551/−12、変更ファイル 6 件。
- **CI（ブランチ head）**：実行 45950247914、completed／success。全文ログ（3,567,949 バイト、ジョブ `build` 140344167689）で GoogleTest `[ PASSED ] 814 tests.`（ベースラインの U1 マージ時 814 件と一致。テスト件数の変化なし）、`[ FAILED ]` 行なし、コンパイルエラー `error C[0-9]` 0 件、`error LNK*`／`error MSB*` 0 件、`Build succeeded` 1 件。自分の 3 ファイル由来の警告は 0 件。3 ファイルとも CL.exe のコマンドラインに含まれ、`.obj` の生成を確認。`.po` 番兵も通過。全文ログは `~/workspace/.coord/u5_final_log.txt` に保存（子の保存した `hidden_files/u5-ci-log-45950247914.txt` のコピー）。
- **成果物（実行 45950247914）**：`WinMerge-x64.zip` 26,568,616 バイト（ID 15177165287）、`WinMerge-x64-Setup-x64.exe` 40,025,023 バイト（ID 15175849948）。いずれも未失効。
- **マージ**：マージコミット `9da2f5ff`。マージコミット CI：実行 45951783516、completed／success（head_sha=9da2f5ff と一致）。
- **受け入れ条件の照合（設計書 §4 U5）**：
  - (a) DedBar の色付きボタン＋グラデーション＋下線、現在の確認内容ボタンだけ塗りつぶし — コード上は満たす。**実機未検証**。
  - (b) パネルの選択タブがアクセント色で区別できる。件数行・検索行の表示が変わらない — コード上は満たす。**実機未検証**。
  - (c) ライト／ダーク両テーマで文字が読める — コード上は満たす（塗りつぶしピルは白文字で 3:1 以上、ダークのアクセント塗りは濃色文字）。**実機未検証**。
  - (d) クリック・トグル・自動隠し・ピン・倍率表示が D2／D6 のまま（PR #46 の入場時ピン ON を含む） — コード上は満たす（描画のみの変更で、コマンド転送・タイマ・フレーム側は不変）。**実機未検証**。
  - (e) 専用モードを出ると通常モードに戻る。ドック状態の汚染なし（D1 の防衛 (a)〜(c) が生きている） — コード上は満たす（入退場・`LoadState`／`CalcFixedLayout` は不変）。**実機未検証**。
- **未検証（実機でしか確認できないもの）**：(a)〜(e) すべて実機未検証。実機チェックリスト：専用モードに入り DedBar のピル色・グラデーション・下線を確認（ライト／ダーク各 1 枚）。確認内容ボタンが現在の状態だけ塗りつぶしになること。パネルの選択タブと 3 本の下線を確認（ライト／ダーク各 1 枚）。フォーカスドックのヘッダ下線を確認。ピン OFF 時の自動隠しと上端ホバーでの再表示、終了ボタンでの通常モードへの復帰、狭いウィンドウでのクリップ挙動（D2 のまま右端がクリップされること）。

### 59.4 次への移行
- U2（タイトルバーに WizMerge＋進捗を表示）は U0 依存で U1・U5 とは独立。U5 のマージを受けて着手する。
- F3（PR #54）・B9（PR #55）は開いているが、子の最終報告待ちのためまだ扱わない。
## §60 Wave 5 F3 — FORM ビジュアルプレビュー（キャンバス・ズーム・文字描画）— マージ・CI 検証済み（2026-11-27）

### 60.1 内容（設計書 §10 F3）
- `Merge20FormCanvasGeom`（純粋モジュール：座標変換・グリッド判定）＋単体テスト 20 件。TransformOneToOne（XML 100 → 画面 132、往復一致）を含む。
- `Merge20FormCanvas`（`CWnd` 派生・GDI 描画）：原寸 1:1、方眼グリッド（100 単位ごと、500 単位ごとに太線・XML 座標基準に修正済み）、色規則（変更なし＝グレー実線、旧＝赤破線、新＝緑実線）、ズーム（100%／25%／フィット／スライダー 10〜200% クランプ）、ドラッグスクロール＋図形クリック、枠高さ切替（全開／640px 固定）、文字・値の描画（§6 (6)）。フォントが近似表示の場合は「フォントは近似表示です」の注記を出す。
- `fontName` 追加（`Xml.h`／`Xml.cpp`、ShapeFontName テスト 1 件）。
- `FormCompareView` にプレビューペインを配線（`Canvas.cpp` の `SetResult` が同一ポインタを無視しスクロール・選択を保持）。
- リソース：コントロール ID 1858–1865、`_APS_NEXT_CONTROL_VALUE` 1866。コマンド ID 新規なし。`.po` 新規 6 件。

### 60.2 変更一覧（ブランチ `merge20/form-f3`、base master `df925a7c`。24 コミット）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20FormCanvasGeom.h` | `a7c81b90` | 新規・純粋モジュール |
| 2 | `Src/Merge20/Merge20FormCanvasGeom.cpp` | `3e2af0ae` | 新規 |
| 3 | `Testing/GoogleTest/Merge20/Merge20FormCanvasGeom_test.cpp` | `6c053d63` | 新規・20 件 |
| 4–6 | 同上 3 ファイル | `dcd43f0a`／`c79040d8`／`806b0ff8` | グリッド太線判定を XML 座標基準に修正 |
| 7 | `Src/Merge20/Merge20FormCanvas.h` | `5df28a6a` | 新規 |
| 8 | `Src/Merge20/Merge20FormCanvas.cpp` | `cb311b4d` | 新規・GDI 描画 |
| 9–11 | `Src/Merge20/Merge20FormXml.h`／`.cpp`／Xml テスト | `32aebc1c`／`723c2ebf`／`41d9e40f` | `fontName` 追加 |
| 12 | `Src/Merge20/Merge20FormCompareView.h` | `20fa1037` | キャンバス＋ズーム操作 |
| 13 | `Src/Merge20/Merge20FormCanvas.cpp` | `2a9bf1cf` | `SetResult` が同一ポインタを無視しスクロール・選択を保持 |
| 14 | `Src/Merge20/Merge20FormCompareView.cpp` | `0f8d98c3` | プレビューペイン配線 |
| 15 | `Src/resource.h` | `0a763bd0` | コントロール ID 1858–1865、`_APS_NEXT_CONTROL_VALUE` 1866 |
| 16–17 | `Src/Merge.vcxproj`／`.filters` | `0b519678`／`d3f5d708` | 登録 |
| 18–19 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` | `e282a9e0`／`a03286d7` | Geom 登録 |
| 20 | `Translations/WinMerge/Japanese.po` | `47645948` | 新規 msgid 6 件 |
| 21 | テスト | `cb6b9219` | CI 失敗修正：テスト 234 行目、`EXPECT_NEAR` 2 引数→`EXPECT_DOUBLE_EQ` |
| 22 | `Src/Merge20/Merge20FormCanvas.cpp` | `11b61f62` | CI 失敗修正：`CDC::GetTextExtentPoint32` 不存在→`CDC::GetTextExtent` |
| 23 | （マージ） | `a63d8102` | master マージ（`6d1767aa`、競合解消） |
| 24 | `Translations/WinMerge/Japanese.po` | `8e6fe505` | .po 修正：エントリ区切りの空行挿入（3 か所。ブランチ head） |

- CI 失敗と修正（実行 45947903570）：調整役の初報は「2 Error(s) のみ」と誤っていた。ログを自分で確認した結果、実際は **6 Error(s)** だった。テスト側 4 件（C2059／C2181／C2065／C2440、すべて `Merge20FormCanvasGeom_test.cpp:234` の 2 引数 `EXPECT_NEAR` が原因）に加え、`Merge20FormCanvas.cpp(548)` の `error C2039: 'GetTextExtentPoint32W': is not a member of 'CDC'` が 2 件。両方とも修正済み（`cb6b9219`、`11b61f62`）。**教訓：CI 失敗の件数はログの全文で数えること。調整役の初報の件数を鵜呑みにしないこと。** なお修正コミット `11b61f62` には CI 実行が一度も作成されなかった（理由不明）。その後のマージコミット `a63d8102` では実行 45949653613 が作成され success したが、.po 問題が見つかったためさらに修正して最終実行で再確認した。
- master マージ（`a63d8102`）：master が 15 コミット進み、`Merge.vcxproj`／`.filters`／`resource.h`／`Japanese.po` が競合したため master（`6d1767aa`、U1 ブランドヘッダ等を含む）をマージ。F2 と同じ方式で Git Database API によるマージコミットを作成。競合解消：`_APS_NEXT_COMMAND_VALUE` 38979（U1 が 38978 を使用）、`_APS_NEXT_CONTROL_VALUE` 1866（F3 が 1858–1865 を使用）。F3・U1 両方の登録を保持。`diff3` で vcxproj／filters はクリーンマージ、resource.h／.po の競合箇所は手動で両方残す形に解決した。
- **.po 空行欠陥の発見と修正（`8e6fe505`）**：検証ゲートの成果物チェックで判明。出荷 .po では `Font is shown approximately.` の訳文が空になっていた。原因はリポジトリ .po 内で `msgstr` の直後に空行なしで次エントリのコメントが続いていたためで、ビルド時の .po 処理がその訳文を落とす。子のマージ解決スクリプトの `.strip()` が F3 最終エントリと U1 エントリの間の空行を消したのが原因。**同じ欠陥が既存の 2 件（`%1 displayed, %2 checked, %3 unreviewed`、`(No lines)`、D4 由来）にもあった**ため、3 件まとめて空行を挿入した。修正後の成果物で 8 件すべて訳文ありを確認済み。**教訓（全スライス共通）：.po の各エントリ間には必ず空行を入れること。マージ解決スクリプトで `.strip()` を使うときはエントリ間の空行を消さないこと。マージ後は .po を末尾まで確認し、`diff3` マーカーがないことと、空行欠落がないことを確認すること。**

### 60.3 検証レポート（コーディネーター検証済み）
- **PR #54**：open・mergeable=true・mergeable_state=clean、head `8e6fe505`、base master。24 コミット、変更ファイル 16 件。PR 差分の 16 ファイルは F3 のものだけ（マージで取り込んだ master 側の変更は差分に含まれないことを確認済み）。
- **CI（ブランチ head）**：実行 45951495877、completed／success。全文ログ（3,575,130 バイト、ジョブ `build` 140348155237、48,018 行）をコーディネーターが取得・確認（`~/workspace/.coord/f3_final_log.txt`）。GoogleTest `[ PASSED ] 835 tests.`（814→835、新規 21 件＝Geom 20 件＋ShapeFontName 1 件を含む。`[ FAILED ]` なし）、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件、`0 Error(s)`。ログ中の "error" 42 件はすべてビルドスクリプト内の `echo ERROR: Failed to download ...` という文字列そのもの（実際の失敗ではない）。`Checking bundled Japanese.po for Merge20 strings...` → `Merge20 strings present in the bundled Japanese.po.` を確認。
- **成果物（実行 45951495877）**：`WinMerge-x64.zip` 26,580,310 バイト、`WinMerge-x64-Setup-x64.exe` 40,031,440 バイト。いずれも未失効。成果物の .po を直接ダウンロードして確認：出荷 `WizMerge/Languages/Japanese.po` 内で F3 の 6 件すべて日本語訳あり（`Visual preview`→ビジュアルプレビュー、`Select a form to preview it`→帳票を選択するとプレビューを表示します、`Fit`→フィット、`Expand to full height`→枠高さ：全開、`Fixed height (640px)`→枠高さ：640pxに固定、`Font is shown approximately.`→フォントは近似表示です）。既存 2 件の訳文の復旧も確認。
- **マージ**：マージコミット `dee3974f`。マージコミット CI：実行 45953212902、completed／success（head_sha=dee3974f と一致）。全文ログ（3,571,925 バイト、ジョブ `build` 140353622266、47,967 行）をコーディネーターが取得・確認（`~/workspace/.coord/f3_merge_final_log.txt`）。GoogleTest `[ PASSED ] 835 tests.`、`Build succeeded`、`0 Error(s)`、`.po` 番兵通過、`error C[0-9]` 0 件。成果物：`WinMerge-x64.zip` 26,582,263 バイト、`WinMerge-x64-Setup-x64.exe` 40,038,275 バイト。いずれも未失効（expire 2027-02-25）。
- **受け入れ条件の照合（設計書 §10 F3）**：
  - (a) 原寸 1:1 — 単体テスト＋ローカルハーネスで検証済み（TransformOneToOne：XML 100 → 画面 132、往復一致）。描画の目視は**実機未検証**。
  - (b) ズーム 4 種 — 単体テストで検証済み（Scale* 7 件：100%／25%／フィット／スライダー 10〜200% クランプ）。実機操作は未検証。
  - (c) §6 (6) テスト 1〜3 — 切り詰めは単体テストで検証済み。フォント近似時の注記は実装済みで、出荷 .po での訳文ありを確認。文字描画・色分けの目視は**実機未検証**。
  - (d) 回帰なし — 835 件全合格で回帰なし。キャンバスは FormCompareView 内の追加のみで、他ウィンドウへの影響なし。
- **未検証（実機でしか確認できないもの）**：描画の目視（1:1 の位置、グリッド、色分け、文字表示）、ズーム操作（ボタン／スライダー／フィット）、ドラッグスクロール、図形クリック選択、枠高さ切替、実ファイルでの表示。

### 60.4 次への移行
- F4（スポットライト）は F3 マージを受けて着手済み（ブランチ `merge20/form-f4`、base `dee3974f`、ID 事前割り当て：コントロール 1872–1881、コマンド 38980）。ただし初回 CI（実行 45955297401、head `d445db8b`）が failure（`Merge20FormSpotlight_test.cpp` 262〜266 行目の 2 引数 `EXPECT_NEAR` と同じパターン、63 Error(s)）。F4 担当に診断ヒント（F3 と同一パターン、`EXPECT_DOUBLE_EQ` への修正）を送付済み。
- B9（PR #57）は付け替え後の CI 成功（実行 45954401163、head `23be11da`）をコーディネーターが全文ログで検証済み（`Build succeeded`・`0 Error(s)`・GoogleTest `[ PASSED ] 839 tests.`（835＋B9 の 4 件）・`[ FAILED ]` なし・`.po` 番兵通過。全文ログは `~/workspace/.coord/b9_final_log.txt`）。ただし U2 マージ（`cfd4534c`）で再び競合（dirty）になったため、B9 担当に master `cfd4534c` の取り込みと CI 再実行を指示済み。B9 のコントロール ID は付け替え後 1866–1871、コマンド ID 38979。
- U2（PR #58）はマージ済み（マージコミット `cfd4534c`）。マージコミット CI（実行 45956300465）は進行中。検証後に §61 として記録する。
- ID 割り当て状況（2026-11-27 07:10 JST 時点）：コントロール ID は F3 が 1858–1865、B9 が 1866–1871（付け替え後）、F4 が 1872–1881（予約済み）。コマンド ID は B9 が 38979、F4 が 38980（予約済み）。master の `_APS_NEXT_COMMAND_VALUE` は 38979、`_APS_NEXT_CONTROL_VALUE` は 1866（F3 マージ時点。B9 マージ時にそれぞれ 38980／1872 へ進む）。
- 運用の教訓：`/tmp` 配下の作業ファイルは VM により消去されることがある（U2 担当が未コミットの編集 1 件を失った。調整役の `/tmp/gh.py` も消去された）。作業用スクラッチと共用スクリプトは `~/workspace/.coord/` 配下に置くこと（`gh.py` は `~/workspace/.coord/gh.py` に再作成済み）。
## §61 UI 見直し U2 — タイトルバーに WizMerge＋進捗を表示 — マージ・CI 検証済み（2026-11-27）

### 61.1 内容（UI 見直し設計書 §4 U2）
- 新規オプション `OPT_MERGE20_TITLE_PROGRESS`（`Merge20/TitleProgress`、既定 true）。OFF で従来のタイトル書式に戻る。オプション画面への項目追加は設計書どおり本スライスの範囲外のため、OFF の切り替えは設定ファイルで行う。
- `CMainFrame::OnUpdateFrameTitle` を拡張。アクティブな `CDirDoc` がある場合のみタイトルを `"WizMerge - N unreviewed - <文書名>"` 形（日本語表示では `WizMerge - 未確認 N件 - <文書名>`）に書き換える。`bAddToTitle=FALSE` の場合（`OnTimer` のメニュー更新時）でも進捗表示が消えないよう、書き換えは `bAddToTitle` の値によらず行う。
- **N の定義**：S3 と同じ（`GetMerge20StatusCountText()` と同じ定義）。表示中のファイル行のみで、ディレクトリ行と `..` 行は数えず、確認内容が TODO の件数。既存の `Merge20CountDisplayedByReviewState` を流用し、新しい数え方は作っていない。`DirView.h` で同関数を private→public へ移動（実装無変更。U1 が `GetMerge20StatusCountText` で行ったのと同じ前例）。
- **更新の契機**：`CDirView::UpdateMerge20StatusCount()` の末尾からメインフレームの `OnUpdateFrameTitle(TRUE)` を呼ぶ。`UpdateMerge20StatusCount()` は確認内容変更の単一の funnel（Redisplay・キー 1〜5／Space・一括チェック・ディレクトリ単位操作・セッション復元・ファイル一覧パネル経由の `CDirDoc::Merge20UpdateDirViewStatus`）のため、S2 の連動と同じ経路でタイトルが追従する。フレーム側はアクティブな文書から件数を読み直すため、バックグラウンドのタブの更新がタイトルを壊すことはない。
- タイトル書式は画面内部の語彙のため【Wiz】は付けていない（入口の文字列にだけ付ける約束どおり）。
- 新規コマンド ID なし。`.po` 新規 1 件。

### 61.2 変更一覧（ブランチ `merge20/ui-u2`、base master `93035933`。6 ファイル 6 コミット＋マージ 1 件）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/OptionsDef.h` | `7a99d4d6` | `OPT_MERGE20_TITLE_PROGRESS`（`Merge20/TitleProgress`）を追加 |
| 2 | `Src/OptionsInit.cpp` | `6cf4f8ef` | 既定 true で初期化 |
| 3 | `Src/DirView.h` | `36ec022a` | `Merge20CountDisplayedByReviewState` を private→public へ移動（実装無変更） |
| 4 | `Src/MainFrm.cpp` | `54485e2d` | `CMainFrame::OnUpdateFrameTitle` を拡張 |
| 5 | `Src/DirView.cpp` | `5cc6859e` | `UpdateMerge20StatusCount()` の末尾から `OnUpdateFrameTitle(TRUE)` を呼ぶ配線を追加 |
| 6 | `Translations/WinMerge/Japanese.po` | `ccb7fb68` | 新規 1 件：msgid `WizMerge - %1 unreviewed - %2`／msgstr `WizMerge - 未確認 %1件 - %2`（msgid 2222→2223 件） |
| — | （マージ） | `d63e2eb0` | master へのマージコミット（下記「競合の解消」を参照） |

- 差分は +60/−2（マージコミットを除く）。`.github/workflows/*` と `MERGE20_NOTES.md` には触れていない。
- 競合の解消：PR 作成直後、master に F3（PR #54）がマージされ、PR は `mergeable: false（dirty）` になり CI が起動しなかった。競合ファイルは `Translations/WinMerge/Japanese.po` のみ（F3 が 35 行追加、U2 が 5 行追加）。U1 の前例（§58.3）どおり、リベースや force-push はせず、**マージコミット `d63e2eb0`** で解消した（API による 3-way マージ）。`.po` は master 側の内容（2228 件）に U2 の 1 件を再追加したもの（計 2229 件）とし、F3 の 16 ファイルは master 側の blob をそのまま取り込んだ。マージ後、PR は mergeable: true になり CI が起動した。

### 61.3 検証レポート（コーディネーター検証済み）
- **PR #58**：open・mergeable=true・mergeable_state=clean、head `d63e2eb0`、base master `dee3974f`。7 コミット、変更ファイル 6 件（U2 の 6 ファイルのみ）。
- **CI（ブランチ head）**：実行 45953876566、completed／success。全文ログ（3,575,188 バイト、ジョブ `build` 140355740743、48,019 行）をコーディネーターが取得・確認（`~/workspace/.coord/u2/ci_log.txt`）。GoogleTest `[ PASSED ] 835 tests.`（U1 時 814 件からの +21 件は F3 のテスト分。U2 は MFC 依存のため新規テストなし）、`[ FAILED ]` 行なし（ログ中の "FAILED" 4 件はすべて `IDS_FILESAVE_FAILED`／`IDS_MERGE_RESULT_SAVE_FAILED` の翻訳文字列 ID で、W2 §25.4 と同じ既知のもの）、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件。`.pot` 抽出ステップで新規 msgid `WizMerge - %1 unreviewed - %2`（`MainFrm.cpp:3450`）が拾われていることもログで確認。`.po` 番兵も通過。
- **成果物（実行 45953876566）**：`WinMerge-x64.zip` 26,582,648 バイト（ID 15178720721）、`WinMerge-x64-Setup-x64.exe` 40,044,890 バイト（ID 15178551405）。いずれも未失効。成果物内の `Build\Translations\WinMerge\Japanese.po` が同梱されていることもログで確認済み（コネクタでは成果物の中身をダウンロードできないため、`.po` 内の文字列の直接確認はログの番兵＋msgid 抽出による間接確認）。
- **マージ**：マージコミット `cfd4534c`。マージコミット CI：実行 45956300465、completed／success（head_sha=cfd4534c と一致）。全文ログ（3,574,204 バイト、ジョブ `build` 140363439634、48,004 行）をコーディネーターが取得・確認（`~/workspace/.coord/u2_merge_final_log.txt`）。GoogleTest `[ PASSED ] 835 tests.`、`Build succeeded`、`0 Error(s)`、`.po` 番兵通過、`error C[0-9]` 0 件。成果物：`WinMerge-x64.zip` 26,582,607 バイト、`WinMerge-x64-Setup-x64.exe` 40,044,573 バイト。いずれも未失効（expire 2027-02-25）。
- **受け入れ条件の照合（設計書 §4 U2）**：
  - (a) フォルダ比較を開くとタイトルバーに「WizMerge - 未確認 N件 - …」と表示されること（タスクバーにも同じ表示が出ること） — コード上は満たす（アクティブな `CDirDoc` がある場合のみ書き換え。タスクバーはメインウィンドウのタイトルを表示するため同じ表示になる）。**実機未検証**。
  - (b) 確認内容を「済」にするとタイトルの N が減ること — コード上は満たす（`UpdateMerge20StatusCount()` 経由で全ての確認内容変更からタイトルを更新）。**実機未検証**。
  - (c) オプション OFF で従来のタイトルに戻ること — コード上は満たす（`OPT_MERGE20_TITLE_PROGRESS` false で早期 return し、基底のタイトルが残る）。**実機未検証**。
  - (d) 単独ファイル比較・フォルダ比較なしの状態でタイトルが壊れないこと（`"WizMerge"` のみ） — コード上は満たす（`CDirDoc` でない／文書がない場合は基底の結果をそのまま残す）。**実機未検証**。
- **未検証（実機でしか確認できないもの）**：(a)〜(d) はすべて実機未検証。実機チェックリストは PR #58 の説明文に記載済み（フォルダ比較でのタイトル表示と N の一致、キー 1〜5／専用バー／ファイル一覧パネルでの「済」による N の減少、`Merge20/TitleProgress=false` での従来タイトルへの復帰、単独ファイル比較・文書なし状態でのタイトル、MDI 子の最大化／通常表示の両方）。
- 作業中の出来事：作業中に `/tmp` 配下の作業ファイルが VM により消去され、未コミットの MainFrm.cpp 編集 1 件を失った。プッシュ済みの 3 コミットは無事で、MainFrm.cpp を master から再取得して編集を再適用し直した（内容は同一）。以後、作業用スクラッチは `~/workspace/.coord/u2/` 配下に置いている（§60.4 の教訓も参照）。
## §62 最高峰ワークフロー B9 — 終わりの一覧確認・セッション確定ダイアログ — マージ・CI 検証済み（2026-11-27）

### 62.1 内容（ギャップ分析書 §4.2 B9。柱 5「終わりの一覧確認」）
- 純粋モジュール `Src/Merge20/Merge20Summary.h/.cpp`：H-DASH 7 種（総数・差分計・変更・旧のみ・新のみ・同一・個別対応要）＋未判断件数・出力対象件数の集計。定義は S6 CSV と同一（変更＝`isResultDiff`、旧のみ＝`isSideFirstOnly`、新のみ＝`isSideSecondOnly` または 3-way の `isSideThirdOnly`、同一＝`!hasDiff`、個別対応要＝確認内容 SPECIAL、未判断＝TODO、出力対象＝出力チェック ON）。`modified + oldOnly + newOnly + identical == total` が常に成立する。
- 単体テスト `Testing/GoogleTest/Merge20/Merge20Summary_test.cpp`：4 件（空入力、全同一・確認済み、混合 6 ファイルの 9 件数、未判断と警告の連動）。
- ダイアログ `Src/Merge20/Merge20SummaryDlg.h/.cpp`（`CTrDialog`）：6 列リスト（ファイル名・確認内容・対応区分・リスク・出力・要約、値はフォルダ一覧・S6 CSV と同一の表示文字列）、上部 2 行の集計、未判断時の警告行、ボタン 3 つ。［未確認のみ表示］［確定して出力］は対象 0 件のとき無効化（S6 の範囲ダイアログと同じ作り）。［戻る］は IDCANCEL（早期 return で無変更）。
- 呼び出し：フォルダ一覧の右クリックメニュー「【Wiz】終わりの一覧確認…」（msgid `End-of-session review...`）。ダイアログタイトルは「【Wiz】終わりの一覧確認」。
- ボタンの動作（`CDirView::OnMerge20Summary`）：［未確認のみ表示］は `OPT_MERGE20_TODO_ONLY` を ON にして `OpenNextFile()` を呼ぶ（D3 の TODO-only 巡回へ）。［確定して出力］は S7 がない間の暫定として S6 の CSV（範囲は出力対象＝「選択分のみ」）を `Summary_YYYY-MM-DD.csv` に出力する。比較全体を走査するためフィルタで隠れたファイルや折りたたまれたフォルダ配下も含む。ファイル 0 件のときは情報メッセージを出して終了する。
- リソース割り当て：IDD_MERGE20_SUMMARY＝301、IDC_MERGE20_SUMMARY_*＝1866–1871（付け替え後。下記「ID 衝突と付け替え」を参照）、ID_MERGE20_SUMMARY＝38979。`.po` 新規 9 件。列見出しは既存 msgid を流用。
- `UnitTests.vcxproj.filters` の `Merge20Summary.cpp`／`.h` フィルタエントリの補完（ブランチに欠けていた分）も本 PR に含む。

### 62.2 変更一覧（ブランチ `merge20/wf-b9`。16 コミット、14 ファイル）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20Summary.h` | `ca5c83f0` | 集計モジュール（ヘッダ） |
| 2 | `Src/Merge20/Merge20Summary.cpp` | `7ab354ab` | 集計モジュール（実装） |
| 3 | `Testing/GoogleTest/Merge20/Merge20Summary_test.cpp` | `9bdbfed0` | 単体テスト 4 件 |
| 4 | `Src/Merge20/Merge20SummaryDlg.h` | `2e89d07b` | ダイアログ（ヘッダ） |
| 5 | `Src/Merge20/Merge20SummaryDlg.cpp` | `ea24dda8` | ダイアログ（実装） |
| 6 | `Src/resource.h` | `f4696ba1` | リソース ID 割り当て（メッセージは旧番号のまま。下記参照） |
| 7 | `Src/Merge.rc` | `053fa384` | ダイアログテンプレート |
| 8 | `Translations/WinMerge/Japanese.po` | `b9d839ae` | 新規 9 件 |
| 9 | `Src/DirView.h` | `8eac5315` | `OnMerge20Summary` 宣言 |
| 10 | `Src/DirView.cpp` | `b0a7c7d8` | フォルダ一覧への配線（`summaryFiles` への改名を含む） |
| 11 | `Src/Merge.vcxproj` | `7059fd12` | 登録 |
| 12 | `Src/Merge.vcxproj.filters` | `723274e3` | 登録 |
| 13 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj` | `2aa3b8e2` | 登録 |
| 14 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters` | `774df25f` | 登録 |
| — | （マージ） | `23be11da` | master マージ（F3 取り込み・ID 付け替え。下記参照） |
| — | （マージ） | `c9ac72d5` | master マージ（U2 取り込み・競合解消。下記参照。ブランチ head） |

- PR #55 → #57 の置き換え：#55 は CI 成功済み（実行 45947979698：GoogleTest 818 件合格・`[ FAILED ]` 0 件・コンパイルエラー 0 件・`.po` 番兵通過・成果物 WinMerge-x64.zip 26,574,932 バイト／Setup exe 40,043,314 バイト）だったが、その後 master に U1（PR #56）がマージされ、U1 がコマンド 38978（`ID_VIEW_MERGE20_BRAND_HEADER`）を取得したため B9 のコマンドを 38978 → 38979 に変更し、master `6fab8e32` にリベースした。API 経由の force-reset 後に PR #55 の head が追従しなかった（#53 と同じ現象）ため、PR を作り直した。#55 は close した。変更内容は #55 と同一。差分はコマンド ID 38978→38979 のみで、さらに DirView.cpp のローカル変数 `files` を `summaryFiles` に改名した（C4459 警告の解消。CI ログで当該警告が出ていたため）。内容の同一性は各ファイルの差分点検で確認済み。
- **ID 衝突と付け替え（F3 マージ時）**：B9 は当初 IDC_MERGE20_SUMMARY_* に 1858–1863 を使っていたが、F3 が master で同じ 1858–1865 を `IDC_MERGE20_FORM_*` に使ったため衝突した。マージコミット `23be11da`（master `dee3974f` 取り込み）で 1858–1863 → **1866–1871** に付け替え、`_APS_NEXT_CONTROL_VALUE` は 1872 とした。コマンド ID 38979 は衝突しなかった（F3 はコマンド ID を使っていない）。なおコミット `f4696ba1` のメッセージは旧番号（1858-1863）のままだが、内容は付け替え後の 1866–1871 である（メッセージと内容の不一致。無害だが記録しておく）。
- **U2 との競合解消（マージコミット `c9ac72d5`、master `cfd4534c` 取り込み）**：重なった 3 ファイル（`Src/DirView.cpp`／`Src/DirView.h`／`Translations/WinMerge/Japanese.po`）で U2・B9 両方の変更を残存させた。`DirView.cpp` は master に対して純粋な追加のみ（削除 0 行）。`.po` は U2 の 1 件の後に B9 の 9 件を追加。Merge20 系エントリはすべて直前に空行あり（欠落 0 件）。F3 の 6 件・U2 の 1 件・B9 の 9 件すべて存在し、msgid＝msgstr＝2,238 件。`diff3` マーカー行なし。今回の U2 マージでは PR の head が正しく追従し（マージコミット方式）、CI も `synchronize` で即起動した。force-reset 時の head 停滞（#53・#55 の現象）は発生しなかった。

### 62.3 検証レポート（コーディネーター検証済み）
- **PR #57**：open・mergeable=true・mergeable_state=clean、head `c9ac72d5`、base master `cfd4534c`。16 コミット、変更ファイル 14 件（B9 の 14 ファイルのみ）。
- **CI（ブランチ head）**：実行 45956986344、completed／success。全文ログ（3,574,373 バイト、ジョブ `build` 140365643106、48,004 行）をコーディネーターが取得・確認（`~/workspace/.coord/b9_final2_log.txt`）。GoogleTest `[ PASSED ] 839 tests.`（835＋B9 の 4 件。B9 の単体テスト 4 件すべて `[ OK ]`）、`[ FAILED ]` 行なし、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件、`0 Error(s)`。B9 ハンドラの C4459 警告なし（`summaryFiles` への改名が効いている。残る C4459 は F2 他の既存分）。`.po` 番兵も通過。
- **成果物（実行 45956986344）**：`WinMerge-x64.zip` 26,594,226 バイト（ID 15179223918）、`WinMerge-x64-Setup-x64.exe` 40,059,499 バイト（ID 15179452594）。いずれも未失効（expire 2027-02-25）。
- **マージ**：マージコミット `f9fc0f4a`。マージコミット CI：実行 45959073117、completed／success（head_sha=f9fc0f4a と一致）。全文ログ（3,575,768 バイト、ジョブ `build` 140372314878、48,026 行）をコーディネーターが取得・確認（`~/workspace/.coord/b9_merge_final_log.txt`）。GoogleTest `[ PASSED ] 839 tests.`、`Build succeeded`、`0 Error(s)`、`.po` 番兵通過、`error C[0-9]` 0 件。成果物：`WinMerge-x64.zip` 26,594,320 バイト、`WinMerge-x64-Setup-x64.exe` 40,070,913 バイト。いずれも未失効（expire 2027-02-25）。
- **受け入れ条件の照合（ギャップ分析書 §4.2 B9）**：
  - (a) ファイルごとの判断が一文ずつ一覧で見られること — 実装済み（6 列リスト）。**実機未検証**。
  - (b) 集計の 7 種の件数が正しいこと — 実装済み。単体テスト＋g++ ハーネスで検証済み。
  - (c) 未判断があるときは警告が表示されること — 実装済み（警告行＋ボタン無効化）。**実機未検証**。
  - (d) ［未確認のみ表示］で未確認ファイルへ移動できること — 実装済み（D3 巡回へ）。**実機未検証**。
  - (e) ［確定して出力］で S7 の出力が始まること（S7 がない間は CSV 出力へ） — 実装済み（暫定：S6 CSV・出力対象のみ）。**実機未検証**。
  - (f) ［戻る］では何も変わらないこと — 実装済み（IDCANCEL は早期 return）。**実機未検証**。
- **未検証（実機でしか確認できないもの）**：実機での一覧ダイアログの表示（6 列・集計 2 行・警告行の見え方、日本語表示）。実機での［未確認のみ表示］→未確認ファイルのオープン、［確定して出力］→CSV の保存と内容、［戻る］→無変更。実機での 10 ファイル程度の受け入れテスト（ギャップ分析書 §4.2 B9 の検証方法）。

### 62.4 次への移行
- B2（未判断警告ゲート・第1段階）はマージ済み（マージコミット `60a5f113`）。マージコミット CI（実行 45962662832）は進行中。検証後に §63 として記録する。
- F4（スポットライト）は CI 失敗の修正中（`Merge20FormSpotlight_test.cpp` の 2 引数 `EXPECT_NEAR`、63 Error(s)）。F4 担当に診断ヒントを送付済み。完了報告待ち。
- B4（未判断の高リスク残量表示）を次に着手する（B2 の次。ギャップ分析書の推奨順序どおり）。
- ID 割り当て状況（2026-11-27 08:05 JST 時点。master `resource.h` で確認済み）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2` のため使用不可）。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881（予約済み・未使用）、B2 が 1882–1885。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第2段階用に 38981 を予約済み（`resource.h` のコメントに記録）。`_APS_NEXT_RESOURCE_VALUE` 304、`_APS_NEXT_COMMAND_VALUE` 38980、`_APS_NEXT_CONTROL_VALUE` 1886。
## §63 最高峰ワークフロー B2 — 未判断警告ゲート・第1段階（終了時警告）— マージ・CI 検証済み（2026-11-27）

### 63.1 内容（ギャップ分析書 §4.2 B2。柱 1「すべての差分に明示的な結論」の警告ゲート）
- B2 は本来、出力時（S7 統一選択 ZIP、S5 一括承認）と終了時の 3 か所に警告を掛ける設計だが、**S7・S5 はユーザー作業 B0 の結論待ちで未着手のため、今回は (iii) フォルダ比較文書を閉じるときの警告だけを先行実装**した（ギャップ分析書 §5 の 2 段階化の選択肢どおり）。(i)(ii) の出力ゲートは第2段階とし、今回は実装していない。
- フック位置は `CDirDoc::SaveModified()`（閉じる直前に呼ばれ、FALSE を返すと閉じる処理が中止される。セッション自動保存はその後の `OnCloseDocument` で走るため「自動保存の直前」になる）。既存の比較中・長時間比較の確認の後に追加。
- 純粋モジュール `Src/Merge20/Merge20CloseWarn.h/.cpp`：件数集計（`Merge20CloseWarnComputeCounts`／`Merge20CloseWarnHasUndecided`）。集計直前に `Merge20SessionRefreshDiffJudgments` で開いているファイル比較の判定をサイドカーへ反映（セッション自動保存と同じ経路）。
- ダイアログ `Src/Merge20/Merge20CloseWarnDlg.h/.cpp`：未判断の件数（ファイル X 件・差分 Y 件、うち高リスク Z 件）を表示し、ボタンは［未確認のみ表示］（D3 の TODO-only 巡回へ誘導）・［それでも閉じる］（警告を無視して閉じる処理を続行）・［キャンセル］（何も変えずに閉じる処理を中止する）。［進む］ボタンの文言はコンストラクタ引数のため、第2段階では「それでも出力する」等の文言を渡して同じダイアログを再利用できる。
- 文言・操作感は B9 の一覧ダイアログと S5 の件数確認ダイアログと統一（同じ「件数＋対象＋キャンセルで無変更」の形）。
- **件数の定義（二重計上なし）**：未確認ファイル X 件＝確認内容が TODO のファイル数（B9 の undecided と同一）。未判断差分 Y 件＝記録されている差分判定のうち、B1 の結論が「未判断」のものの総数。マージセッションで開いたファイルにだけ記録があるため、未開封ファイルの差分は Y に含めない（そのファイルは X に含まれる）。1 つの差分は最大 1 回だけ数え、ファイルは差分として数えない。うち高リスク Z 件＝上記の未判断のうち、ファイルのリスクが S4 の「高」のもの（TODO かつリスク高のファイル数＋リスク高ファイルに属する未判断差分）。常に Z ≦ X + Y。
- リソース割り当て：IDD_MERGE20_CLOSE_WARN＝303（302 は `IDI_ROTATE2` のため使用不可）、IDC_MERGE20_CLOSE_WARN_*＝1882–1885（1882 件数行、1883 警告行、1884 未確認のみ表示、1885 進むボタン）。`_APS_NEXT_RESOURCE_VALUE` 304、`_APS_NEXT_CONTROL_VALUE` 1886。**コマンド ID は使っていない**（警告は閉じる操作に自動で掛かるためメニュー項目不要）。38981 は第2段階の出力ゲート用に予約として `resource.h` のコメントに記録した。`.po` 新規 4 件。
- 第2段階へのフック：件数集計は純粋モジュール `Merge20CloseWarn` に切り出してあるため、S7・S5 の呼び出し側は同じ関数を呼ぶだけでよい。ダイアログの［進む］ボタンの文言はコンストラクタ引数で差し替え可能。件数・警告・残り 2 ボタンは共通。

### 63.2 変更一覧（ブランチ `merge20/wf-b2`、base master `451691d0`。13 コミット、13 ファイル）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20CloseWarn.h` | `957fd015` | 純粋モジュール・ヘッダ |
| 2 | `Src/Merge20/Merge20CloseWarn.cpp` | `41893e49` | 純粋モジュール |
| 3 | `Src/Merge20/Merge20CloseWarnDlg.h` | `e19fd63b` | 警告ダイアログ・ヘッダ |
| 4 | `Src/Merge20/Merge20CloseWarnDlg.cpp` | `bd7b6d48` | 警告ダイアログ |
| 5 | `Testing/GoogleTest/Merge20/Merge20CloseWarn_test.cpp` | `a6efb5f8` | 単体テスト 7 件 |
| 6 | `Src/resource.h` | `784e819b` | IDD 303、IDC 1882–1885 |
| 7 | `Src/Merge.rc` | `ccde5fab` | ダイアログテンプレート＋レイアウト |
| 8 | `Src/Merge.vcxproj` | `b102ab80` | 登録 |
| 9 | `Src/Merge.vcxproj.filters` | `386e8cd8` | 登録 |
| 10 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj` | `d81a3737` | 登録 |
| 11 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters` | `fd48472a` | 登録 |
| 12 | `Src/DirDoc.cpp` | `872a7626` | `SaveModified` に警告ゲート |
| 13 | `Translations/WinMerge/Japanese.po` | `8b87cdc3` | 訳文 4 件追加（ブランチ head） |

- `.po` の確認：追加した 4 エントリはすべて `msgstr` の直後に空行を入れ、ファイル末尾も空行で終えている。`<<<<<<<` の 2 ヒットは、コンフリクト表示の説明文の中の既存の stock WinMerge 翻訳文字列であることをコーディネーターが直接確認済み（`>>>>>>>`／`|||||||` は 0 件）。`diff3` マーカーの残存なし。

### 63.3 検証レポート（コーディネーター検証済み）
- **PR #60**：open・mergeable=true・mergeable_state=clean、head `8b87cdc3`、base master `451691d0`。13 コミット、変更ファイル 13 件（B2 の 13 ファイルのみ）。
- **CI（ブランチ head）**：実行 45960856740、completed／success。全文ログ（3,578,941 バイト、ジョブ `build` 140378059474、48,069 行）をコーディネーターが取得・確認（`~/workspace/.coord/b2_final_log.txt`）。GoogleTest `[ PASSED ] 846 tests.`（839＋B2 の 7 件。`EmptyInput`／`AllDecided`／`AcceptanceScenario`／`FilesOnlyUndecided`／`DiffsOnlyUndecided`／`DecidingClearsWarning`／`HighRiskNeverExceedsUndecided` すべて `[ OK ]`）、`[ FAILED ]` 0 件、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件、`0 Error(s)`。429 warnings は gtest ヘッダ等の既存分。B2 の新規ファイルからの警告は 0 件。`.po` 番兵も通過。
- **成果物（実行 45960856740）**：`WinMerge-x64.zip` 26,598,578 バイト（ID 15181755124）、`WinMerge-x64-Setup-x64.exe` 40,057,186 バイト（ID 15180753952）。いずれも未失効（expire 2027-02-25）。
- **マージ**：マージコミット `60a5f113`。マージコミット CI：実行 45962662832、completed／success（head_sha=60a5f113 と一致）。全文ログ（3,578,870 バイト、ジョブ `build` 140383900949、48,067 行）をコーディネーターが取得・確認（`~/workspace/.coord/b2_merge_final_log.txt`）。GoogleTest `[ PASSED ] 846 tests.`、`Build succeeded`、`0 Error(s)`、`.po` 番兵通過、`error C[0-9]` 0 件。成果物：`WinMerge-x64.zip` 26,598,021 バイト、`WinMerge-x64-Setup-x64.exe` 40,052,792 バイト。いずれも未失効（expire 2027-02-25）。
- **受け入れ条件の照合（B2 第1段階）**：
  - (a) 未判断 0 で警告なし — `Merge20CloseWarnHasUndecided` が偽ならダイアログを出さない。単体テスト `AllDecided`／`DecidingClearsWarning` で検証。コード上は満たす。
  - (b) 未判断ありで件数付き警告 — 受け入れシナリオ（未確認ファイル 2 件・未判断差分 3 件・うち高リスク 3 件）を単体テスト `AcceptanceScenario` で検証。コード上は満たす。
  - (c) ［未確認のみ表示］で未確認ファイルへ移動 — `OPT_MERGE20_TODO_ONLY` を ON にして `CDirView::OpenNextFile()` を呼ぶ B9 と同一経路。コード上は満たす。**実機での移動は未検証**。
  - (d) ［それでも閉じる］の場合に限り続行 — Proceed のときだけ `SaveModified` が TRUE を返す。コード上は満たす。
  - (e) キャンセルで無変更 — Cancel／ダイアログを閉じた場合は FALSE を返し、判断状態も変えない。コード上は満たす。
  - (f) 回帰なし — 警告は `SaveModified` の末尾（既存の確認の後）に追加したのみで、通常の閉じる操作・セッション保存・復元・B9・D3 巡回には触れていない。CI 全テスト合格で裏付け。**実機での回帰確認は未実施**。
- **未検証（実機でしか確認できないもの）**：ダイアログの表示そのもの（レイアウト・日本語表示・ボタン配置）。実機での受け入れ手順 (1)〜(5)：未確認ファイル 2 件・未判断差分 3 件を残した状態で (1) 閉じようとすると警告が出る、(2) ［未確認のみ表示］で未確認ファイルに移動する、(3) ［それでも閉じる］で閉じられる、(4) 全件 DONE にしてから閉じると警告が出ない、(5) キャンセルで閉じられない。ダイアログ表示中にフォルダ比較ウィンドウを閉じる等の実機フロー。

### 63.4 次への移行
- B4（未判断の高リスク残量表示）はマージ済み（マージコミット `4c359a70`）。マージコミット CI（実行 45967396948）は進行中。検証後に §64 として記録する。
- F4（スポットライト）は CI 失敗の修正中（`Merge20FormSpotlight_test.cpp` の 2 引数 `EXPECT_NEAR`、63 Error(s)）。F4 担当に診断ヒントを送付済み。完了報告待ち。
- B3（リスク順巡回）を次に着手する（B4 の次。ギャップ分析書の推奨順序どおり）。
- ID 割り当て状況（2026-11-27 08:45 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881（予約済み・未使用）、B2 が 1882–1885、B4 が 1886。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第2段階用に 38981 を予約済み、B3 用に 38982 を確保予定。`_APS_NEXT_RESOURCE_VALUE` 304、`_APS_NEXT_COMMAND_VALUE` 38980、`_APS_NEXT_CONTROL_VALUE` 1887。
## §64 最高峰ワークフロー B4 — 未判断の高リスク残量表示 — マージ・CI 検証済み（2026-11-27）

### 64.1 内容（ギャップ分析書 §4.2 B4。柱 1「すべての差分に明示的な結論」の残量表示）
- 「【Wiz】未判断の高リスク：N 件」を常駐表示する。N が 0 になったら行ごと非表示にする（縦幅も取らない。警告表示にはしない）。
- **表示場所：ファイル一覧パネルのヘッダ、件数行の直下。1 か所に統一**（§3.5）。S3／D3 の件数表示は実際にファイル一覧パネルのヘッダ（`CMerge20FileListCtrl::GetCountText()` → `CMerge20FileListBar::m_stCount`）に描かれていることをコードで確認した。ステータスバーには同種の件数表示はないため、パネルヘッダに決定。ステータスバーには置かない。**後続の B3 も同じ場所（パネルヘッダ）に統一すること**（PR 説明文に明記済み）。
- **件数：B2 の Z と完全一致**。`CDirDoc::GetMerge20CloseWarnCounts()` を新設し、収集（判断スタッシュ＋走査＋`Merge20CloseWarnComputeCounts`）を B2 の警告ダイアログと B4 の表示で共有。定義も収集も 1 か所。高リスクの定義は S4 の分類と同一。
- **更新の契機**：既存の通知経路（`NotifyMerge20FileListChanged` → `MSG_MERGE20_FILELIST_REFRESH` → パネル `Refresh()`。レビュー状態・B1 結論の連動が通知を出す）に乗せる。純粋モジュール `Merge20HighRiskRemain` が「表示を触る必要があるか」を判定し、件数が変わらない Refresh ではコントロールに触れない。`SetMerge20DiffJudgments`／`SetMerge20SplitGroups` は通知を出さないため、パネル更新内での判断スタッシュに再通知ループの危険がないことも確認済み。
- 集計が 50ms を超えた場合のみ `Merge20 high-risk remain: X ms for N files` をログに出す（D10 計測ログのパターン）。
- リソース割り当て：コントロール ID は `IDC_MERGE20_FILELIST_HIGHRISK` ＝ 1886 のみ使用し、`_APS_NEXT_CONTROL_VALUE` を 1887 へ進めた（`resource.h` の割り当てコメントも更新）。コマンド ID・ダイアログ ID は不使用。`.po` 新規 1 件（`Undecided high risk: %1` → `【Wiz】未判断の高リスク：%1 件`。msgid 総数 2,242 → 2,243）。
- B2 の `SaveModified` は集計呼び出しだけを置き換え（ダイアログ挙動は同一）。

### 64.2 変更一覧（ブランチ `merge20/wf-b4`、base master `7147e61f`。13 コミット、13 ファイル）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20HighRiskRemain.h` | `d62b321f` | 新規・純粋モジュール |
| 2 | `Src/Merge20/Merge20HighRiskRemain.cpp` | `4eafef65` | 新規 |
| 3 | `Testing/GoogleTest/Merge20/Merge20HighRiskRemain_test.cpp` | `473ed2a1` | 新規・9 件 |
| 4 | `Src/DirDoc.h` | `ef7beb5b` | 共有集計メソッドの宣言 |
| 5 | `Src/DirDoc.cpp` | `30568869` | 共有集計の実装＋`SaveModified` の寄せ |
| 6 | `Src/Merge20/Merge20FileListBar.h` | `6dbb9dd3` | 残量行の宣言 |
| 7 | `Src/Merge20/Merge20FileListBar.cpp` | `1ef23c31` | 残量行の表示・更新・レイアウト |
| 8 | `Src/resource.h` | `d5dd11fd` | IDC 1886 割り当て |
| 9 | `Translations/WinMerge/Japanese.po` | `d0b71b06` | 新訳文 |
| 10 | `Src/Merge.vcxproj` | `fff49330` | 登録 |
| 11 | `Src/Merge.vcxproj.filters` | `91146113` | 登録 |
| 12 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj` | `30366e77` | 登録 |
| 13 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters` | `faaaec59` | 登録（ブランチ head） |

- `.po` の確認：`<<<<<<<` のヒットは stock 翻訳の正当なもの（コンフリクト表示の説明文）のみで、`diff3` マーカーの残存なし。

### 64.3 検証レポート（コーディネーター検証済み）
- **PR #61**：open・mergeable=true・mergeable_state=clean、head `faaaec59`、base master `7147e61f`。13 コミット、変更ファイル 13 件（B4 の 13 ファイルのみ）。
- **CI（ブランチ head）**：実行 45964954783、completed／success。全文ログ（3,580,202 バイト、ジョブ `build` 140391321090、48,080 行）をコーディネーターが取得・確認（`~/workspace/.coord/b4_final_log.txt`）。GoogleTest `[ PASSED ] 855 tests.`（846＋B4 の 9 件。`Merge20HighRiskRemain` 全 9 件合格）、`[ FAILED ]` 0 件（ログ中の "FAILED" 4 ヒットは `.pot` 生成時の `IDS_FILESAVE_FAILED` 等の文字列 ID で、テスト失敗ではない）、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件、`0 Error(s)`。警告 429 件は既存のもののみで、B4 のファイル由来の警告は 0 件。`.po` 番兵も通過。
- **成果物（実行 45964954783）**：`WinMerge-x64.zip` 26,599,055 バイト（ID 15182688891）、`WinMerge-x64-Setup-x64.exe` 40,058,204 バイト（ID 15182504948）。いずれも未失効（expire 2027-02-25）。
- **マージ**：マージコミット `4c359a70`。マージコミット CI：実行 45967396948、completed／success（head_sha=4c359a70 と一致）。全文ログ（3,581,025 バイト、ジョブ `build` 140399269017、48,092 行）をコーディネーターが取得・確認（`~/workspace/.coord/b4_merge_final_log.txt`）。GoogleTest `[ PASSED ] 855 tests.`、`Build succeeded`、`0 Error(s)`、`.po` 番兵通過、`error C[0-9]` 0 件。成果物：`WinMerge-x64.zip` 26,599,031 バイト、`WinMerge-x64-Setup-x64.exe` 40,071,494 バイト。いずれも未失効（expire 2027-02-25）。
- **受け入れ条件の照合（ギャップ分析書 §4.2 B4）**：
  - (a) 未判断の高リスク差分・ファイルの件数が正しく表示されること — B2 と同じ集計を流用。テスト `AcceptanceScenario`（3 件→2 件→0 件）、`DiffsKeepRemainder`（DONE ファイルの未判断差分も数える）で確認。コード上の確認であり、画面表示そのものは**実機未検証**。
  - (b) 判断が進むと件数が減ること — S3 の通知経路に乗り、テスト `CountDecreaseUpdates` で件数変化時の表示更新を確認。B1 結論の連動（TODO→DONE）は `SetMerge20ReviewState` 経由で通知を出すこともコードで確認済み。
  - (c) 0 件のときは警告表示にならないこと — 行ごと非表示＋縦幅ゼロ。テスト `ZeroHides`／`ZeroStaysHidden` で確認。
  - (d) 100〜1000 ファイル規模でも表示更新が重くならないこと — 純粋集計の 1000 件テスト `ScaleThousandFiles` で highRisk 800 を正確に算出。MFC 側は 50ms 超え時のみログを出す仕組み。**実機での重さの目視・ログ確認は未検証**。
  - (e) 回帰なし — 件数行・リスト・B9 ダイアログ・B2 警告・ステータスバーの既存表示に触れていない。B2 の `SaveModified` は集計呼び出しだけを置き換え（ダイアログ挙動は同一）。全 855 テスト合格。
- **未検証（実機でしか確認できないもの）**：パネルヘッダに「【Wiz】未判断の高リスク：N 件」が実際に表示されること。検証手順：リスク高の未確認ファイル 3 件を含むフォルダ比較で (1)「未判断の高リスク：3 件」になること、(2) 1 件を DONE にすると 2 件になること、(3) 全件 DONE にすると行が消えること。100〜1000 ファイル規模で表示更新が重くないこと（遅い場合のみログに `Merge20 high-risk remain:` が出る）。日本語表示（【Wiz】未判断の高リスク：N 件）。

### 64.4 次への移行
- B3（リスク順巡回）はマージ済み（マージコミット `f4cf22bc`）。マージコミット CI は進行中。検証後に §65 として記録する。
- F4（スポットライト）は前任者が途中終了し、後継担当を spawn 済み（根本原因は C2373 の同名衝突で確定。調整役の `EXPECT_NEAR` ヒントは誤りだったことを認め、後継への指示で訂正済み）。後継の完了報告待ち。
- B14（高リスク確認）を次に着手する（B3 の次。ギャップ分析書の推奨順序どおり）。
- ID 割り当て状況（2026-11-27 09:30 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881（予約済み・未使用）、B2 が 1882–1885、B4 が 1886、B3 が 1887–1896（予約。実使用は `IDC_MERGE20_DED_RISK_ORDER` 1888 を含む）。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第2段階用に 38981 を予約済み、B3 が `ID_MERGE20_RISK_ORDER` 38982 を使用。`_APS_NEXT_RESOURCE_VALUE` 304、`_APS_NEXT_COMMAND_VALUE` 38983（B3 マージ後）、`_APS_NEXT_CONTROL_VALUE` 1897（B3 マージ後。B3 の実使用分は要確認）。
## §65 最高峰ワークフロー B3 — リスク順巡回 — マージ・CI 検証済み（2026-11-27）

### 65.1 内容（ギャップ分析書 §4.2 B3。柱 2「リスク順レビュー」）
- (a) フォルダ一覧の巡回をリスク順にする。D3 の TODO-only 巡回に「リスク順」モードを追加し、高リスクの未確認ファイルを先に巡回できるようにする（専用バーの Prev／Next とフォルダ一覧の ↑／↓ に同じ入口を使う。D3 の設計を踏襲）。モードの ON／OFF を切り替えられること。`Merge20BuildPatrolOrder`＋`Merge20FindNextInOrder` で実現。モード OFF 時は `Merge20BuildPatrolOrder` が一覧順を返し、従来のコードパスを通る。
- (b) ファイル内の差分巡回もリスク順にする。差分単位のリスクを S4 の分類から算出する（`Merge20ClassifyDiffRisk`：S4 のファイル単位ルール `Merge20ClassifyDiffLines` を、その差分範囲の行だけに適用。削除のみ（OP_1STONLY）→中、変更（OP_DIFF）→中、追加のみ（OP_2NDONLY）→なし。リスク重要語を含むと 1 段階上がる（削除／変更→高、追加→中）。trivial／差分行なし→なし。インサイト（丸め／SQL／if ガード削除／条件／定数）はリスク点を加算しない。文脈行（OP_NONE）はリスクを変えない。学習状態は入力にしない（リスク軸と学習軸は独立、§3.1）。純粋モジュール `Merge20Patrol` に切り出し、単体テスト可能）。`RiskOrderDiffs`（候補集合は従来と同じ、有意・非フィルタ・TODO-only 時は未解決）。
- (c) リスクフィルタ（高／中／なし）を追加する。置き場所は**パネル側**（パネルヘッダの第2タブ行、対応区分タブの直下）。デッキ側とパネル側の両方に置いて同期を取る設計は避けた（ギャップ分析書 §3.5、P-R10「フィルタデッキをパネル側に寄せる」と一致）。表示場所は B4 と同じくパネルヘッダ周辺に統一。タブは「すべてのリスク／高／中／なし」（巡回順＝高が先）。コンボボックスではなくタブ行にしたのは、P2 実績のオーナードロー・ピルパターンを流用し、コンボのドロップダウン高さによる固定行レイアウトの問題を実機なしで避けるため。未分類ファイルは「すべて」のみ一致（対応区分タブと同じ規則）。フィルタはパネル限定（フォルダ一覧はフィルタしない）。巡回はパネルフィルタと独立。
- モード切替 UI：コマンド `ID_MERGE20_RISK_ORDER`（38982）。View メニュー（フォルダ比較・ファイル比較の両方）と専用バーのトグルボタン（`IDC_MERGE20_DED_RISK_ORDER` 1888、幅 110）。ハンドラは `CMainFrame` と `CMergeEditFrame` の両方に配置（MFC のコマンド経路により一度の呼び出しで片方だけが動く）。専用バーのチェック表示は `UpdateDedBar` → `SetRiskOrder` で同期。
- 巡回規則の詳細：現在位置が巡回対象でない場合のステップ規則を文書化し、D3 とのパリティを単体テストで確認（`FindNextInOrderMatchesFindNextTargetInListOrder` で一覧順における D3 との完全一致を確認）。未分類（未開封）ファイルは最後に寄せる（`Merge20RiskRank::UNCLASSIFIED` = 3。S4 のソート規則と同じ）。学習候補の有無は巡回順に影響させない。
- リソース割り当て：コマンド 38982、コントロール 1887–1889（`IDC_MERGE20_DED_RISK_ORDER` 1888 を含む）。新規オプション `OPT_MERGE20_RISK_ORDER`、`OPT_MERGE20_FILELIST_RISK_FILTER`。`.po` 新規 3 件（`Risk-order patrol`→「【Wiz】リスク順巡回」、`Risk order`→「【Wiz】リスク順」、`All risks`→「すべてのリスク」。「高／中／なし」は既存訳を再利用）。

### 65.2 変更一覧（ブランチ `merge20/wf-b3`、base master `5d060706`。33 コミット、29 ファイル）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/resource.h` | `451353b8` | ID 割り当て（コマンド 38982、コントロール 1887–1889） |
| 2 | `Src/OptionsDef.h` | `8349e035` | `OPT_MERGE20_RISK_ORDER`、`OPT_MERGE20_FILELIST_RISK_FILTER` 追加 |
| 3 | `Src/OptionsInit.cpp` | `c52ad24b` | 既定値の登録 |
| 4–6 | `Src/Merge20/Merge20Patrol.h`／`.cpp`／テスト | `bbc8e59c`／`2ef79e26`／`c619a2ae` | リスク順巡回の API・実装・単体テスト（旧版） |
| 7–9 | `Src/Merge20/Merge20Classify.h`／`.cpp`／テスト | `27ee2af0`／`09d2d8be`／`c650aafa` | `Merge20ClassifyDiffRisk` の宣言・実装・単体テスト |
| 10–11 | `Src/Merge20/Merge20ClassifyDoc.h`／`.cpp` | `adb78238`／`c5da9d83` | `Merge20ClassifyOneDiffRange` の宣言・実装 |
| 12–14 | `Src/Merge20/Merge20FileListFilter.h`／`.cpp`／テスト | `72e40ebd`／`ca37f597`／`e935d97d` | パネル行へのリスク付与・可視性判定・単体テスト |
| 15–16 | `Src/Merge20/Merge20FileListCtrl.h`／`.cpp` | `05e2c4ac`／`f21fd3fe` | `SetRiskFilter` の宣言・配線 |
| 17 | `Src/resource.h` | `06a6ec8f` | リスクフィルタはタブ行にする整理（ID 1887–1888） |
| 18–19 | `Src/Merge20/Merge20FileListBar.h`／`.cpp` | `46e17df6`／`cbae3304` | パネル・リスクタブ行の宣言・実装 |
| 20–22 | `Src/Merge20/Merge20Patrol.h`／`.cpp`／テスト | `5f261e5c`／`8a835340`／`428e3404` | 巡回規則の位置規則版（非対象からのステップ・D3 パリティ） |
| 23–24 | `Src/DirView.h`／`.cpp` | `a3746fb7`／`7fdb1026` | `Merge20BuildPatrolOrder` の宣言・フォルダ巡回 |
| 25 | `Src/MergeDiffNavigation.h` | `cf0ff2f1` | 差分巡回のリスク順 |
| 26–27 | `Src/Merge20/Merge20DedBar.h`／`.cpp` | `27b9f6fc`／`d6ebff5d` | 専用バーのトグルボタン |
| 28–29 | `Src/MergeEditFrm.h`／`.cpp` | `5d62c63b`／`3cb86df6` | トグルハンドラ（ファイル比較側） |
| 30–31 | `Src/MainFrm.h`／`.cpp` | `c47e24fe`／`ca5bcbf5` | トグルハンドラ（メイン側） |
| 32 | `Src/Merge.rc` | `39277ab9` | View メニュー項目（フォルダ比較・ファイル比較） |
| 33 | `Translations/WinMerge/Japanese.po` | `1d8ee4af` | 新規 3 件（ブランチ head） |

- 変更ファイル 29 件はすべて modified（新規ファイルなし。`Merge20Patrol`／`Merge20Classify`／`Merge20FileListFilter` は既存モジュールの拡張）。

### 65.3 検証レポート（コーディネーター検証済み）
- **PR #62**：open・mergeable=true・mergeable_state=clean、head `1d8ee4af`、base master `5d060706`。33 コミット、変更ファイル 29 件。
- **CI（ブランチ head）**：実行 45972056871、completed／success。全文ログ（3,583,880 バイト、ジョブ `build` 140414609553、48,128 行）をコーディネーターが取得・確認（`~/workspace/.coord/b3/b3_final_log.txt`）。GoogleTest `[ PASSED ] 867 tests.`（855＋B3 の 12 件：Patrol 8、Classify 1、FileListFilter 3）、`[ FAILED ]` 0 件、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件、`0 Error(s)`。`.po` 番兵も通過。新 msgid 3 件が CI 内の `.po` ダンプに含まれていることも確認。
- **成果物（実行 45972056871）**：`WinMerge-x64.zip` 26,606,805 バイト（SHA256 `345b0db556a5f9f8f35d5474e23390f1ea51b376c11bbd726efe81e4cae6c202`、ID 15185613263）、`WinMerge-x64-Setup-x64.exe` 40,058,688 バイト（SHA256 `a1e0d1291a094af8c94330ecd196c4dd9f166a73e7b82c8e6d8498949573d23f`、ID 15185742964）。いずれも未失効（expire 2027-02-25）。
- **マージ**：マージコミット `f4cf22bc`。マージコミット CI：実行 45974145511、completed／success（head_sha=f4cf22bc と一致）。全文ログ（3,582,898 バイト、ジョブ `build` 140421576652、48,112 行）をコーディネーターが取得・確認（`~/workspace/.coord/b3_merge_final_log.txt`）。GoogleTest `[ PASSED ] 867 tests.`、`Build succeeded`、`0 Error(s)`、`.po` 番兵通過、`error C[0-9]` 0 件。成果物：`WinMerge-x64.zip` 26,606,805 バイト相当（ID は実行 45974145511 のもの。zip／Setup exe とも未失効）。
- **受け入れ条件の照合（ギャップ分析書 §4.2 B3）**：
  - (a) リスク順モードで巡回すると、高→中→なしの順に未確認ファイルを開くこと — 単体テスト済み。**実機の目視は未検証**。
  - (b) ファイル内の差分巡回も同じ順になること — 差分単位リスクは単体テスト済み。**実機の目視は未検証**。
  - (c) リスクフィルタで「高だけ表示」にできること — 単体テスト済み。**実機の目視は未検証**。
  - (d) リスク順モードを切ると従来の一覧順巡回に戻ること — 単体テスト `FindNextInOrderMatchesFindNextTargetInListOrder` で一覧順における D3 との完全一致を確認。
  - (e) 未分類（未開封）ファイルは最後に寄せること — `Merge20RiskRank::UNCLASSIFIED` = 3。単体テスト済み。
  - (f) 回帰なし — モード OFF 時の経路は変更なし。D3 の TODO-only 巡回・専用バーの Prev／Next・フォルダ一覧の ↑／↓・H-FILTER の既存フィルタ・B4 の残量表示に影響しない設計で、CI の GoogleTest 867 件全合格で確認。
- **既知の軽微な端**：リスク順モード＋TODO-only OFF 時、Next／Prev ボタンの有効化判定は従来のままのため、最後のファイルで現在位置がリスク順の末尾かつ差分順で後続の差分がある場合、ボタンが有効なまま押しても何も起きないことがある（更新ハンドラは分類コストを避けるため意図的に変更していない）。
- **未検証（実機でしか確認できないもの）**：検証方法 (1)〜(4) の目視：リスク順モード ON での Prev／Next（高→中→なし）、ファイル内差分の巡回順、フィルタ「高だけ表示」、モード OFF での一覧順復帰。専用バーのトグルボタンの表示・チェック同期、パネルヘッダのリスクタブ行の表示・レイアウト、View メニュー項目の表示。

### 65.4 次への移行
- F4（スポットライト）は後継担当の修正が完了し、ブランチ CI 成功（実行 45973779109、874 tests 合格）をコーディネーターが全文ログで検証済み。ただし master が B3 マージ＋NOTES §64 まで進んだため、PR #59 は再び mergeable: False／dirty になっている。現在 master `26782ed3` の取り込みと競合解消・CI 再実行のため、後継の後継（フォローアップ担当）を spawn する。
- B14（高リスク確認）は着手済み（ブランチ `merge20/wf-b14`）。完了報告待ち。
- F4 マージ後に F5（構造化差分テーブル＋影響分析）を着手する（F3→F4→F5→F6→F7 の順序どおり）。
- ID 割り当て状況（2026-11-27 09:45 JST 時点。B3 の完了報告で確定）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881（予約済み・使用中。コマンド ID は F4 では未使用）、B2 が 1882–1885、B4 が 1886、B3 が 1887–1889。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第2段階用に 38981 を予約済み、B3 が `ID_MERGE20_RISK_ORDER` 38982 を使用。`_APS_NEXT_RESOURCE_VALUE` 304、`_APS_NEXT_COMMAND_VALUE` 38983、`_APS_NEXT_CONTROL_VALUE` 1890（B3 マージ後の確定値。§64 の「1897」は B3 の実使用が 1887–1889 だったため訂正する）。
## §66 最高峰ワークフロー B14 — 高リスク判断の確認 — マージ・CI 検証済み（2026-11-27）

### 66.0 §65 の訂正 2 件
- B3 マージ CI（実行 45974145511）の成果物サイズ：§65 では「WinMerge-x64.zip 26,606,805 バイト相当」と書いたが、正しくは **WinMerge-x64.zip 26,607,146 バイト、WinMerge-x64-Setup-x64.exe 40,063,506 バイト**（expire 2027-02-25）である。26,606,805 バイトはブランチ CI（実行 45972056871）の方のサイズだった。
- B3 のコントロール ID：§65 では「B3 が 1887–1889」と書いたが、実際の define は **1887–1888 のみ**（`IDC_MERGE20_FILELIST_RISK_TABS` 1887、`IDC_MERGE20_DED_RISK_ORDER` 1888）で、1889 は未使用だった。B3 のコミット `451353b8` のメッセージ「controls 1887-1889」は確保範囲の記述で、実使用は 1887–1888 である。1889 は B14 が正しく取得した。

### 66.1 内容（ギャップ分析書 §4.2 B14。柱 8「キーボード中心の速さと危険操作の慎重さ」の危険側）
- リスク「高」の差分に対して、破壊的な方向の判断（旧採用で新側を捨てる／新採用で旧側を捨てる／不採用）をするときに確認ダイアログを挟む。
- ダイアログ `Merge20HighRiskConfirmDlg`：件数行・明細行（読取専用マルチライン編集欄）・警告行・［実行する］／［キャンセル］。**［キャンセル］が既定ボタン**（Enter の誤押しでも判断が決まらない）。タイトル「【Wiz】高リスクの確認」。件数行「高リスクの差分：N 件」／「高リスクのファイル：<相対パス>」。明細行「差分 N：<S4 差分要約>」「学習候補：<方針>（<完全一致／特徴一致>・<n>件）」「次の状態：済」。警告行「キャンセルすると何も変わりません。」。
- ダイアログの内容：リスク理由（S4 の要約文：欠落・変更・重要語・SQL 等＋影響関数。`Merge20ClassifyOneDiffSummary` を新設。B3 の差分リスク関数と範囲→行の構築を `BuildOneDiffRangeLines` として共有化）と、学習候補がある場合はその内容（L3 の候補文言）を併記する。
- 確認は 1 回の判断につき 1 回だけ出す（同じ差分の連続操作では繰り返さない）。範囲操作は適用前に 1 つのダイアログにまとめ、キャンセル時は 1 件も変わらない。
- 文言・操作感の原型：**S5 の件数確認ダイアログは未実装だった**（U-0-2 待ち、NOTES §51.3 残件）。調整役の指示の前提が誤っていた。B14 担当が正しく見抜き、同じ「件数＋対象＋キャンセルで無変更」の形を持つ **B9 の一覧ダイアログと B2 の終了時警告ダイアログ**を原型として統一した。
- **Space 順送りの扱い（設計決定）：順送りを止めてその差分（ファイル）で確認を出す。スキップしない。** 対象は S2 Space の循環のうち高リスクファイルを「済」にする段（TODO→DONE。出力チェックも ON になる）のみで、他の段（DONE→HOLD 等）は無確認のまま。**スキップを選ばなかった理由**：高リスクファイルを無言で飛ばすと Space が「効かないキー」に見え、巡回フローからそのファイルが隠れてしまうため。止めて確認を出す方が、誤キーを防ぎつつ「ここに危険がある」ことを目に入らせる。キー 1〜5 はガードしない（'2' は DONE の明示選択で、次が何になるか分からないまま押す Space の循環とは誤キー性が違うため）。確認内容列のクリックによる循環も同様にガードしない（セルを狙った明示クリックのため）。この決定と理由は PR 説明文にも明記済み。
- 確認ゲートの唯一の入口：`CMergeDoc::Merge20ConfirmHighRiskDiffs` を新設し、`OnMerge20SetConclusion`（範囲メニュー）と `OnMerge20LearnApply`（学習受け入れ）と D4 フォーカスドックの判定ボタンの 3 入口から呼ぶ。学習候補の受け入れでも、差分が高リスクなら確認が出る（候補があることを理由に確認を省かない。§3.1）。
- Space が判断を決めるのはフォルダ一覧・パネルの確認内容循環のみで、ファイル比較エディタの Space は文字入力、スナイパーの Space はスクロールであることをコードで確認済み。
- リソース割り当て：IDD_MERGE20_HIGHRISK_CONFIRM＝304、IDC_MERGE20_HIGHRISK_*＝1889–1892（1889 件数、1890 明細、1891 警告、1892 実行）。`_APS_NEXT_RESOURCE_VALUE` 305、`_APS_NEXT_CONTROL_VALUE` 1893。コマンド ID は不使用（B2 の警告と同じく判断操作から自動で呼ばれるため）。38980（F4 予約）・38981（B2 第2段階予約）は使っていない。`.po` 新規 9 件（msgid 2,246 → 2,255）。
- B15（ショートカット一覧）について：B14 で新しいショートカットは追加していない（確認ダイアログは既存の判断操作から呼ばれるのみ）。アプリ内のショートカット一覧（B15）はまだ存在しないため（H-DOC のマニュアルのみ）、B15 の更新はない。

### 66.2 変更一覧（ブランチ `merge20/wf-b14`、base master `26782ed3`→`ef524909`。19 コミット、19 ファイル）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20HighRiskConfirm.h` | `0aab80d2` | 純粋モジュール・ヘッダ |
| 2 | `Src/Merge20/Merge20HighRiskConfirm.cpp` | `aafce4e7` | 純粋モジュール |
| 3 | `Testing/GoogleTest/Merge20/Merge20HighRiskConfirm_test.cpp` | `c3624064` | 単体テスト 19 件 |
| 4 | `Src/Merge20/Merge20HighRiskConfirmDlg.h` | `d3bf5c58` | ダイアログ・ヘッダ |
| 5 | `Src/Merge20/Merge20HighRiskConfirmDlg.cpp` | `49a10bc5` | ダイアログ・実装（ファイル用ヘルパー `Merge20ConfirmHighRiskFileReview` を含む） |
| 6 | `Src/resource.h` | `f489b5dc` | IDD 304、IDC 1889–1892 |
| 7 | `Src/Merge.rc` | `0f2ecd89` | ダイアログテンプレート＋レイアウト／マージン登録 |
| 8 | `Translations/WinMerge/Japanese.po` | `e976cb7b` | 新規 9 件 |
| 9 | `Src/Merge.vcxproj` | `20a4e69b` | 登録 |
| 10 | `Src/Merge.vcxproj.filters` | `068020be` | 登録 |
| 11 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj` | `a3d9ed1f` | 登録 |
| 12 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters` | `d1d54c15` | 登録 |
| 13 | `Src/Merge20/Merge20ClassifyDoc.h` | `2a5a7715` | `Merge20ClassifyOneDiffSummary` の宣言 |
| 14 | `Src/Merge20/Merge20ClassifyDoc.cpp` | `e7f120f1` | `Merge20ClassifyOneDiffSummary` の実装 |
| 15 | `Src/MergeDoc.h` | `12c3f858` | 確認ゲートの宣言 |
| 16 | `Src/MergeDocResultPane.cpp` | `2355b220` | `Merge20ConfirmHighRiskDiffs` の実装・配線 |
| 17 | `Src/Merge20/Merge20FocusDock.cpp` | `0b6bc7db` | 判定ボタンに同じゲート |
| 18 | `Src/DirView.cpp` | `b7d55290` | Space 循環ガード（フォルダ一覧） |
| 19 | `Src/Merge20/Merge20FileListBar.cpp` | `9a273587` | Space 循環ガード（パネル。ブランチ head） |

- 作業中に master が 1 コミット進んだが（`ef524909`、MERGE20_NOTES §65 のみ）、B14 の 19 ファイルと競合せず、PR の base は自動で現 master になった。
- 純粋モジュール `Merge20HighRiskConfirm`：破壊的判定（旧採用／新採用／不採用のみ真）、確認要否（リスク高＋破壊的＋未決定）、範囲収集（確認要の差分を順に 1 回ずつ）、Space 循環の確認要否（高リスクかつ TODO→DONE のみ真）。単体テスト 19 件（破壊的判定 6 件、確認要否マトリクス、収集、Space 循環、受け入れシナリオ、学習候補が確認を抑制しないこと）。

### 66.3 検証レポート（コーディネーター検証済み）
- **PR #63**：open・mergeable=true・mergeable_state=clean、head `9a273587`、base master `ef524909`。19 コミット、変更ファイル 19 件（新規 5、変更 14。B14 の 19 ファイルのみ）。
- **CI（ブランチ head）**：実行 45977287468、completed／success。全文ログ（3,585,980 バイト、ジョブ `build` 140432091022、48,138 行）をコーディネーターが取得・確認（`~/workspace/.coord/b14/b14_final_log.txt`）。GoogleTest `[ PASSED ] 886 tests.`（867＋B14 の 19 件。B14 の 19 件すべて `[ OK ]`）、`[ FAILED ]` 0 件、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件、`0 Error(s)`。警告は既存分のみ。B14 の新規ファイル由来の警告は 0 件（DirView.cpp の C4459 2 件は既存の他ハンドラのもので、今回触った箇所ではない）。`.po` 番兵も通過。
- **成果物（実行 45977287468）**：`WinMerge-x64.zip` 26,616,134 バイト（ID 15187933029）、`WinMerge-x64-Setup-x64.exe` 40,067,824 バイト（ID 15188735062）。いずれも未失効（expire 2027-02-25）。
- **マージ**：マージコミット `5700cb16`。マージコミット CI：実行 45978891064、completed／success（head_sha=5700cb16 と一致）。全文ログ（3,587,335 バイト、ジョブ `build` 140437466629、48,159 行）をコーディネーターが取得・確認（`~/workspace/.coord/b14_merge_final_log.txt`）。GoogleTest `[ PASSED ] 886 tests.`、`Build succeeded`、`0 Error(s)`、`.po` 番兵通過、`error C[0-9]` 0 件。成果物：`WinMerge-x64.zip` 26,616,114 バイト、`WinMerge-x64-Setup-x64.exe` 40,079,808 バイト。いずれも未失効（expire 2027-02-25）。
- **受け入れ条件の照合（ギャップ分析書 §4.2 B14）**：
  - (a) 高リスク差分の破壊的な判断で確認が出ること — 3 入口すべてにゲート。ダイアログ表示そのものは**実機未検証**。
  - (b) 確認ダイアログにリスク理由が表示されること — `Merge20ClassifyOneDiffSummary`（S4 差分要約＋影響関数）を明細行に表示。**実機未検証**。
  - (c) キャンセルでは何も変わらないこと — ゲートが偽を返し、呼び出し元は適用前に return する（範囲操作も事前走査のため 1 件も変わらない）。**実機未検証**。
  - (d) リスク高でない差分では確認が出ず、従来どおり速く進めること — 純粋モジュールの素通り（単体テスト済み）。回帰は CI 全合格で確認。
  - (e) 学習候補の受け入れでも、差分が高リスクなら確認が出ること — `OnMerge20LearnApply` に同じゲート。候補は要否に影響しない（単体テスト済み）。**実機未検証**。
  - (f) Space の順送りで高リスク差分を誤って決めてしまわないこと — TODO→DONE の段だけ確認を挟む（単体テスト済み）。**実機未検証**。
  - (g) 回帰なし — B1 の結論付与・S2 のキー操作・学習候補の受け入れ・D3 巡回に触れていない（ゲートは確認要の場合にだけ割り込み、それ以外は既存経路をそのまま通る）。CI 全合格で確認。**実機での回帰確認は未実施**。
- **未検証（実機でしか確認できないもの）**：確認ダイアログの表示そのもの（レイアウト・日本語表示・ボタン配置・既定ボタンがキャンセルであること）。実機での受け入れ手順 (1)〜(5)：リスク高の差分（削除・SQL・税・丸めを含むサンプル）で旧採用を選ぶと確認が出る、キャンセルすると状態が変わらない、リスクなしの差分では確認が出ない、学習候補のある高リスク差分を受け入れると確認が出る、Space の順送りで高リスク差分を誤って決められない（フォルダ一覧・パネルで TODO→DONE の段だけ確認が出る）。

### 66.4 次への移行
- F4（スポットライト）はマージ済み（マージコミット `bd15fa55`）。マージコミット CI は進行中。検証後に §67 として記録する。
- B7（出力前最終検証ゲート・第1段階）は着手済み（ブランチ `merge20/wf-b7`。子が「B7 (1/2)」として PR を分ける方針で進行中）。完了報告待ち。
- F5（構造化差分テーブル＋影響分析）を次に着手する（F3→F4→F5→F6→F7 の順序どおり）。
- ID 割り当て状況（2026-11-27 10:40 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888（§65 の「1887–1889」は訂正。1889 は B14 が取得）、B14 が 1889–1892。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第2段階用に 38981 を予約済み、B3 が `ID_MERGE20_RISK_ORDER` 38982 を使用。`_APS_NEXT_RESOURCE_VALUE` 305、`_APS_NEXT_COMMAND_VALUE` 38983、`_APS_NEXT_CONTROL_VALUE` 1893。
## §67 Wave 5 F4 — FORM スポットライト（前後横並び・インスペクタバナー・巡回・解除）— マージ・CI 検証済み（2026-11-27）

### 67.1 内容（設計書 §10 F4）
- スポットライトの選択状態（カードごとに保持。`m_spotlightByFile`）。キャンバスの強調描画：選択した差分箇所へ自動スクロールし、周囲を暗転（AlphaBlend 0.32 のヴェール＋窓）、旧（赤破線）→新（緑実線）の前後変化を矢印＋差分コールアウトで強調する。
- インスペクタバナー（`LayoutSpotlightBanner`：旧／新ラベル＋150px 幅の横並びプレビュー `CMerge20FormSpotPreview`。旧の図形だけを描いた小キャンバスと、新の図形だけを描いた小キャンバスを横に並べる。スポットライト選択があるときだけ表示）。前／次での巡回ボタン、解除ボタン、「変更箇所へ移動」ボタン（スポットライト選択があればその図形へ、なければ最初の変更図形へスクロール）。
- 関数：`focusDiffRecord`／`stepSpotlight`／`clearSpotlight`／`scrollToShapeKey`／`scrollToChangedArea`／`updateSpotlightBanner`（設計書 §2.7）。公開フック `CFormCompareView::FocusDiffRecord(int)` を用意し、構造化差分テーブル（F5）が後から呼ぶ形にした。
- 純粋モジュール `Merge20FormSpotlight`（選択・巡回・窓・スクロール・矢印・コールアウト）＋単体テスト 19 件。
- リソース割り当て：コントロール ID 1872–1881。コマンド ID は未使用。`.po` 新規 10 件。`BuildTranslations.cmd` に番兵チェック用の 8 msgid を追加（"Previous change" "Next change" "Clear spotlight" "Go to changed area" "Spotlight" "Changed attributes" "Change details" "No shape"）。"Old (before)"／"New (after)" が番兵に含まれていないのは意図的で、ファイル内のコメントに「括弧が FOR セットを壊すため載せない」と理由が書かれている（.po 側の 10 件には両方とも訳つきで存在）。

### 67.2 変更一覧（ブランチ `merge20/form-f4`。28 コミット、16 ファイル）
| # | ファイル | コミット | 内容 |
|---|---|---|---|
| 1 | `Src/Merge20/Merge20FormSpotlight.h` | `1f64037b` | 純粋モジュール（ヘッダ） |
| 2 | `Src/Merge20/Merge20FormSpotlight.cpp` | `02bc5d3e` | 純粋モジュール（実装） |
| 3 | `Testing/GoogleTest/Merge20/Merge20FormSpotlight_test.cpp` | `35003d5a` | 単体テスト 19 件 |
| 4 | `Src/Merge20/Merge20FormSpotPreview.h` | `6ab3da66` | 横並びプレビュー（ヘッダ） |
| 5 | `Src/Merge20/Merge20FormSpotPreview.cpp` | `35c2d58c` | 横並びプレビュー（実装） |
| 6 | `Src/Merge20/Merge20FormCanvas.h` | `eec40882` | スポットライト API の宣言 |
| 7 | `Src/Merge20/Merge20FormCanvas.cpp` | `4416818e` | キャンバスのスポットライト描画 |
| 8 | `Src/Merge20/Merge20FormCompareView.h` | `3e6f69c9` | バナー・公開フックの宣言 |
| 9 | `Src/Merge20/Merge20FormCompareView.cpp` | `6b7e3d3c` | バナーの実装 |
| 10 | `Src/resource.h` | `c8ab72a7` | コントロール ID 1872–1881 |
| 11 | `Translations/WinMerge/Japanese.po` | `ef04b9ad` | "Change details" の末尾スペース除去 |
| 12 | `Translations/WinMerge/Japanese.po` | `743b3968` | "Changed attributes" の末尾スペース除去 |
| 13 | `Translations/WinMerge/Japanese.po` | `d99301ae` | 新規 10 件 |
| 14 | `BuildTranslations.cmd` | `de364f48` | 番兵チェックへの 8 msgid 追加 |
| 15 | `Src/Merge.vcxproj` | `efe011a6` | 登録（`msimg32.lib` を含む） |
| 16 | `Src/Merge.vcxproj.filters` | `a5d530d0` | 登録 |
| 17 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj` | `1fae926b` | 登録 |
| 18 | `Testing/GoogleTest/UnitTests/UnitTests.vcxproj.filters` | `d445db8b` | 登録 |
| 19 | `Src/Merge20/Merge20FormSpotlight.h` | `c3e07cb7` | 関数宣言の改名（C2373 修正） |
| 20 | `Src/Merge20/Merge20FormSpotlight.cpp` | `bb47b9f5` | 関数定義の改名 |
| 21 | `Src/Merge20/Merge20FormCanvas.cpp` | `47e9b344` | 呼び出し箇所の改名 |
| 22 | テスト | `af194f5b` | 呼び出し箇所の改名＋cos/sin 30° のローカル変数化 |
| 23 | （空コミット） | `d71c074f` | CI 再実行の発火試行（効果なし） |
| 24 | （マージ） | `b6b322a9` | master `5d06070` のマージ（競合解消） |
| 25 | `Src/Merge20/Merge20FormCanvas.cpp` | `d112ce75` | CRect LONG の int キャスト（C2672 修正） |
| 26 | `Src/Merge20/Merge20FormCompareView.cpp` | `c04677f7` | ΔW/ΔH 行の先頭を `String(...)` に（C2110 修正） |
| 27 | （マージ） | `49acb1e7` | master `ef524909` のマージ（B3 との競合解消） |
| 28 | （マージ） | `282275df` | master `5700cb16` のマージ（B14 との競合解消。ブランチ head） |

- CI 失敗と修正（実行 45955297401、63 Error(s)）：調整役の初報のヒント（2 引数 `EXPECT_NEAR` が原因）は**誤り**だった。前任者の `EXPECT_NEAR` はすべて 3 引数だった。真の根本原因は 2 つあった：
  1. **同名衝突（C2373）**：構造体 `Merge20FormSpotlightArrowHead` と同名のファクトリ関数 `Merge20FormSpotlightArrowHead(...)` の衝突。`Merge20FormSpotlight.cpp(168,1): error C2373: 'Merge20FormSpotlightArrowHead': redefinition; different type modifiers` が決定的な証拠。関数名を `Merge20FormSpotlightMakeArrowHead` に改名して解消（`c3e07cb7`／`bb47b9f5`／`47e9b344`／`af194f5b`）。
  2. **独立したエラー（当初は連鎖と誤認）**：C2672 が 24 行、Canvas 965／971／973 と CompareView 1081（C2110）が前回から存在する独立したエラーだった。マージ後 CI（実行 45971953400、failure）で 5 件として表面化し、修正した。C2672：`DrawSpotlightCallout` で `CRect` の LONG メンバ（`.right/.left/.top`、`.cx`）と `int` を `std::max/min` に渡し、型推論が曖昧になっていた → `static_cast<int>` で統一（`d112ce75`）。C2110：`coordLine += _T("  ΔW:") + (dw>=0 ? _T("+") : _T("")) + …` でポインタ同士の加算になっていた（`+=` の右辺がポインタから始まるため）→ 先頭を `String(...)` に（`c04677f7`）。
  - **教訓（調整役の誤診の記録）**：CI 失敗の原因は、ログの全文を読んで特定すること。エラー件数の多さから「すべて 1 件の連鎖」と決めつけないこと。`EXPECT_NEAR` 2 引数説は F3 では正しかったが、F4 では誤りだった。同じエラーパターンでも原因が違うことがある。
- 前任者の途中終了：前任者（子エージェント 3be9c080）は診断までで修正コミットを積めずに終了した（途中経過報告で完了）。後継（da96efc6）が改名・独立エラーの修正・master マージまで行い、フォローアップ（502278ed）が 2 回の master マージ（B3・B14 との競合解消）と CI 再実行を行った。3 人のリレーで完了した。
- CI が発火しなかった出来事：修正 4 コミット後、約 30 分待っても CI が起動しなかった（空コミット・close／reopen でも不発）。原因は **PR #59 が master とコンフリクト（`mergeable: false`、`dirty`）しており、pull_request 用の merge ref が作れないため実行が作られない状態**だった。F3 と同じ方式で master をマージしたところ（`b6b322a9`）、直後に実行 45971953400 が起動した。**教訓：CI が起動しないときは PR の mergeable 状態を確認すること。コンフリクトしていると CI は作られない。**
- マージ 1 回目（`49acb1e7`、master `ef524909` 取り込み）：重なったのは `resource.h` と `Japanese.po` の 2 ファイルのみ。`resource.h` は F4 の 1872–1881 を B2 アンカー（1882）の前に挿入し、B3 の 1887–1888・コマンド 38982 と `_APS_NEXT_*`（304／38983／1889）を維持。`.po` は F4 の 10 件を「File Compare」アンカー直後に挿入し、B3 の 3 件も残存。msgid 数 2243 → 2256。**指示書の訂正**：指示書は「B3 が 1887–1889 を使用、`_APS_NEXT_CONTROL_VALUE` は 1890」とあったが、実コードでは B3 は 1887–1888 のみで master の値は 1889 だった。1890 にすると「1889 は使用済み」という虚偽の申告になるため、master 値 1889 を採用した（§66 の訂正と一致）。
- マージ 2 回目（`282275df`、master `5700cb16` 取り込み）：重なったのは 6 ファイル（`resource.h`、`Japanese.po`、`Src/Merge.vcxproj`／`.filters`、`UnitTests.vcxproj`／`.filters`）。B14 のダイアログ 304・コントロール 1889–1892 と F4 の 1872–1881 は重なりなし。`_APS_NEXT_*` は 305／38983／1893 に更新。UnitTests プロジェクトファイルは master の LF 改行のまま解消したため、PR 差分の改行 churn（従来 +1138/−1131）が消え、現在は +7/−0・+9/−0 のみ。PR 全体は +1798/−31（16 ファイルちょうど）。

### 67.3 検証レポート（コーディネーター検証済み）
- **PR #59**：open・mergeable=true・mergeable_state=clean、head `282275df`、base master `5700cb16`。28 コミット、変更ファイル 16 件（F4 の 16 ファイルのみ）。
- **CI（ブランチ head）**：実行 45980412083、completed／success。全文ログ（3,593,646 バイト、ジョブ `build` 140442575743、48,233 行）をコーディネーターが取得・確認（`~/workspace/.coord/f4_final2_log.txt`）。GoogleTest `[ PASSED ] 905 tests.`（886＋F4 の 19 件。Spotlight 19 件全 OK、B14 HighRiskConfirm 19 件も含まれて合格）、`[ FAILED ]` 0 件、コンパイルエラー `error C[0-9]` 0 件、`Build succeeded` 1 件、`0 Error(s)`。`.po` 番兵も通過。
- **成果物（実行 45980412083）**：`WinMerge-x64.zip` 26,637,168 バイト（ID 15188754047）、`WinMerge-x64-Setup-x64.exe` 40,096,153 バイト（ID 15188933852）。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **マージ**：マージコミット `bd15fa55`。マージコミット CI：実行 45983131061、completed／success（head_sha=bd15fa55 と一致）。全文ログ（3,593,040 バイト、ジョブ `build` 140451830140、48,223 行）をコーディネーターが取得・確認（`~/workspace/.coord/f4_merge_final_log.txt`）。GoogleTest `[ PASSED ] 905 tests.`、`Build succeeded`、`0 Error(s)`、`.po` 番兵通過、`error C[0-9]` 0 件。成果物：`WinMerge-x64.zip` 26,637,687 バイト、`WinMerge-x64-Setup-x64.exe` 40,102,252 バイト。いずれも未失効（expire 2027-02-25）。
- **受け入れ条件の照合（設計書 §10 F4）**：
  - (a) 変更一覧の行またはキャンバス上の図形をクリックすると、キャンバスがその場所へ自動スクロールし、周囲が暗転して旧（赤）→新（緑）の前後変化が矢印＋差分コールアウトで強調されること — コードで確認。`OnLButtonUp` → `OnCanvasShapeClicked` → `FocusDiffRecord` → `SetSpotlight`／`ScrollToShapeKey`／`DrawSpotlight`（暗転 AlphaBlend 0.32、窓、旧／新再描画、矢印、コールアウト）。選択・窓・スクロール量・矢印・コールアウトの単体テスト 19 件が合格。**描画の目視とスクロールの中心合わせは実機未検証**。
  - (b) インスペクタバナーに旧／新の横並びプレビューが表示されること（§6 (4) のテスト3） — コードで確認（`LayoutSpotlightBanner`：旧／新ラベル＋150px 幅プレビュー、`CMerge20FormSpotPreview`）。**目視は実機未検証**。
  - (c) 前／次での巡回が `hasVisual` のあるレコードだけを対象に循環すること。解除で全体表示に戻ること — 純粋モジュール＋テスト（`StepForward/BackwardSkipsNonVisualAndWraps`、`ClearReturnsToWholeView` など）で合格。カードごとの選択状態保持（`m_spotlightByFile`）もコードで確認。**操作の実機確認は未検証**。
  - (d) 回帰なし — 全 905 件合格。
- **未検証（実機でしか確認できないもの）**：描画（暗転・矢印・コールアウト・バナー・旧／新プレビュー）の目視、スクロールの中心合わせ、前／次巡回と解除と「変更箇所へ移動」の操作、カード切替時の選択状態保持。

### 67.4 次への移行
- B7（出力前最終検証ゲート）は PR 1（エンジン）をマージ済み（マージコミット `d787cae7`）。PR 2（ダイアログ＋ゲート）のファイルは `~/workspace/.coord/b7/work/pr2/` に準備済みで、PR 1 のマージ後に同じブランチ `merge20/wf-b7` へ積む方針。PR 2 の担当を spawn する。
- F5（構造化差分テーブル＋影響分析）は着手済み（ブランチ `merge20/form-f5`）。完了報告待ち。
- ID 割り当て状況（2026-11-27 11:10 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 用に 305 を確保済み。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 用に 1893–1897 を確保済み、F5 用に 1903–1912 を確保済み（1898–1902 は予備）。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第2段階用に 38981 を予約済み、B3 が 38982 を使用、B7 用に 38983 を確保済み、F5 用に 38984 を確保済み。`_APS_NEXT_RESOURCE_VALUE` 305、`_APS_NEXT_COMMAND_VALUE` 38983、`_APS_NEXT_CONTROL_VALUE` 1893（F4 マージ時点）。

## 68. B7 (1/2) — 出力時最終検証エンジン（PR #64）

### 68.1 概要
- B7（出力前の最終検証ゲート）の PR 1：検証エンジン `Merge20Verify`（純粋モジュール）＋単体テスト 33 件＋チェックごとのオプションキー。ダイアログ＋ゲート本体（`SaveMergeResult` フック）は PR 2（§70 で記録予定）。
- 4 つのチェック：(i) 再差分（Hirschberg LCS、共有 intern map）、(ii) 行数／エンコーディング（厳密 UTF-8）、(iii) 構文健全性（括弧照合のみ）、(iv) 整合性（B1 の結論と出力の一致）。100 ファイル × 500 行 = 102 ms。
- オプションキー `Merge20/VerifyRediff`、`VerifyLineCount`、`VerifySyntax`、`VerifyConsistency`（すべて既定 ON）。設定画面への追加は未実施。
- stock の保存（`DoSave`）はゲート対象外（OLD／NEW ペインでありマージ出力ではない）。

### 68.2 変更内容（PR #64、11 コミット、9 ファイル、+1884/−4）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20Verify.h` | エンジン宣言 |
| `Src/Merge20/Merge20Verify.cpp` | エンジン実装（4 チェック） |
| `Testing/GoogleTest/Merge20/Merge20Verify_test.cpp` | 単体テスト 33 件 |
| `Src/OptionsDef.h` | オプションキー 4 件 |
| `Src/OptionsInit.cpp` | オプション既定値（ON） |
| `Src/Merge.vcxproj`／`.filters` | 登録 |
| `Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` | 登録 |

### 68.3 検証レポート（コーディネーター検証済み）
- **PR #64**：head `8a015289`（CI 失敗の修正後）、base master。ブランチ CI：実行 45985139460、completed／success。GoogleTest 938 件全合格（905＋33）。
- CI 失敗と修正（実行 45982188009、GoogleTest 10 件失敗）：(1) intern map を新旧で別々に作っていたため再差分が不一致、(2) Conflict／Unresolved を must-equal から除外していなかった、(3) 大文字小文字のみのテストデータ "b" vs "B" が不正 — いずれも修正済み。
- **マージ**：マージコミット `d787cae7`。マージコミット CI：実行 45987411912、completed／success。全文ログ（3,598,601 バイト、ジョブ `build` 140466464425）をコーディネーターが取得・確認（`~/workspace/.coord/b7/b7_pr1_merge_log.txt`）。GoogleTest `[ PASSED ] 938 tests.`、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`。
- **成果物（実行 45987411912）**：`WinMerge-x64.zip` 26,638,306 バイト、`WinMerge-x64-Setup-x64.exe` 40,100,577 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **未検証（実機でしか確認できないもの）**：エンジンは純粋モジュールであり UI がないため、実機での動作確認は PR 2（ダイアログ＋ゲート）と合わせて行う。

### 68.4 次への移行
- B7 PR 2（ダイアログ＋ゲート、IDD 305、IDC 1893–1897、`.po` 27 件）は PR #66 として作成済み。ブランチ `merge20/wf-b7`、head `e9d116f9`、CI 実行 45988382245 が進行中。担当からの完了報告を待って検証・マージする。
- F5（構造化差分テーブル＋影響分析）は PR #65 としてマージ済み（マージコミット `05052799`）。マージ CI 実行 45989040682 が進行中。検証後に §69 として記録する。
- F6（名称マスター管理）を次に着手する。IDD 306 を使用すること（305 は B7 PR2 が予約済み）。コントロール ID は 1905 以降（F5 が 1903–1904 を使用。1893–1902 は B7 PR2 用に空けること）。コマンド ID が必要な場合は 38985 以降（38983 は B7、38984 は F5 が予約済み・未使用）。`_APS_NEXT_*` は master の現在値を読んで進めること。

## 69. Wave 5 F5 — FORM 構造化差分テーブル＋影響分析（PR #65）

### 69.1 概要
- FORM 比較の差分テーブル（HTML 版と同じ 4 列：種別／対象要素／属性／旧値→新値／変更内容・影響分析）と影響分析（重なり・はみ出し・フォント波及・用紙外へのはみ出し）。グループ化された変更は 1 行に集約し、既定で折りたたみ。行クリックでスポットライト（F4 の `FocusDiffRecord` へ配線）。
- **実装中に発見した F3/F4 の連携バグを F5 で修正**：設計書 §4.1 では形状キー（`Merge20FormShape::key`）は安定キーと定義されているが、F1 は F0 の HTML キー（`field[name='a']`）のままにしており、レコード・`changedShapeKeys`・キャンバス・スポットライトは安定キー（`/form/row[1]/field[1]|field|a`）で検索していた。そのため形状検索が常に miss し、キャンバスの変更強調・形状クリック・スポットライトのスクロールと旧／新プレビューが働かない状態だった。`Merge20FormDiff.cpp` に安定キーへの付け替え（旧／新を別マップで対応付け）を追加し、テストで固定した。F5 の受け入れ条件 (a)（行クリック→スポットライト）を成立させる前提の修正である。

### 69.2 変更内容（PR #65、17 コミット、16 ファイル、+1901/−11）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20FormImpact.h/.cpp` | 影響分析（重なり・はみ出し・フォント波及・用紙外）。新側の図形集合に対する幾何判定。推定値の図形（`isEstimated`）は全チェック対象外 |
| `Src/Merge20/Merge20FormTable.h/.cpp` | 差分テーブルの表示行を構築（グループ 1 行集約・既定折りたたみ、座標は小数第 1 位まで表示） |
| `Src/Merge20/Merge20FormDiff.cpp` | 形状キーの安定キー化（上記の連携バグ修正） |
| `Testing/GoogleTest/Merge20/Merge20FormImpact_test.cpp` | 単体テスト 13 件 |
| `Testing/GoogleTest/Merge20/Merge20FormTable_test.cpp` | 単体テスト 9 件 |
| `Src/Merge20/Merge20FormCompareDoc.cpp` | `RunAnalysis` から `Merge20ApplyFormImpact` を呼ぶ（`Merge20ApplyLargeChangeFlag` の後。上書き防止） |
| `Src/Merge20/Merge20FormCompareView.h/.cpp` | プレビュー画面の下に差分テーブル（ラベル＋リスト、200px）を追加。行クリック→スポットライト、グループ行の展開／折りたたみ、スポットライトからの選択行の追従、ファイルごとの展開状態の保持 |
| `Src/resource.h` | コントロール ID 1903–1904（`IDC_MERGE20_FORM_TABLE_LABEL`／`IDC_MERGE20_FORM_TABLE`）。`_APS_NEXT_CONTROL_VALUE` を 1905 へ |
| `Translations/WinMerge/Japanese.po` | 新規 msgid 6 件（日本語訳付き。各 msgstr 直後に空行あり） |
| `Src/Merge.vcxproj`／`.filters`、`Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` | 登録 |

- 影響分析の判定規則（設計書 §6 (5) に基づく。詳細はヘッダーに明記）：
  - 重なり：旧側で重なっていなかった 2 図形が新側で重なった場合に警告。重なり面積／小さい方の図形の面積が 10% 以上で「重なり」（警告文に 2 項目名を含む）、10% 未満は「軽微な重なり」の注記。罫線（`isLine`）同士は対象外、罫線と項目の重なりは警告する。
  - はみ出し：テキスト推定幅（半角 `point×0.6`、全角 `point×1.0`）が枠幅を超えた場合に警告。旧側でもはみ出していたものは警告しない。「〜可能性あり」「推定のため近似です」と明記。
  - フォント波及：`point` が増えた要素についてテキスト幅の増加量を推定し、最も近い隣接図形までのすき間（矩形間距離）が増加量より小さい場合に警告（「可能性あり」「推定による近似判定です」）。
  - 用紙外へのはみ出し：用紙サイズが判明している場合は用紙、なければキャンバス原寸を超えた場合に警告。**注意**：F1 の原寸は「最大座標＋160」のため、キャンバス基準では原理的に発火しない。用紙サイズは現状不明（T-F-4）のため、実働する経路は用紙サイズ判明時のみ。
  - 深刻度：重なり／はみ出し／用紙外がある場合に `LayoutBreaking` へ引き上げる（フォント波及・軽微な重なりは引き上げない）。ファイル追加（旧側なし）の場合は比較対象がないため警告しない。

### 69.3 検証レポート（コーディネーター検証済み）
- **PR #65**：head `b625d59f`、base master `aeb12db5`（マージ時点で master は B7 PR1＋§67 まで進んでいたが、mergeable=clean のためそのままマージ）。CI：実行 45987159867、completed／success。全文ログ（48,251 行）を F5 担当が取得・確認（`~/workspace/.coord/f5/f5_run45987159867.log`）。GoogleTest 927 件中 927 件合格（ベースライン 905 件＋新規 22 件＝Impact 13 件＋Table 9 件）、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`（警告 429 件は既存のもので、F5 ファイルに起因する警告なし）。`.po` 番兵も通過。
- **マージ**：マージコミット `05052799`。マージコミット CI：実行 45989040682、completed／success。全文ログ（3,604,200 バイト、ジョブ `build` 140471883924）をコーディネーターが取得・確認（`~/workspace/.coord/f5/f5_merge_log.txt`）。GoogleTest `[ PASSED ] 960 tests.`（938＋22）、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`。
- **成果物（実行 45989040682）**：`WinMerge-x64.zip` 26,654,080 バイト、`WinMerge-x64-Setup-x64.exe` 40,127,355 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **受け入れ条件の照合（設計書 §10 F5）**：
  - (a) 列が HTML 版と同じ・行クリックでスポットライト — 列構成は純粋モジュールのテストで固定。行クリック→`FocusDiffRecord` の配線はコードで実装済み。**実機でのクリック操作そのものは未検証**。形状キーの修正により、スポットライトのスクロール・旧／新プレビューが初めて機能するはず。
  - (b) §6 (5) テスト 1〜4 合格 — GoogleTest で合格（`OverlapDetected`／`TextOverflowDetected`／`FontRippleDetected`／`NoFalsePositivesOnCleanChange`＋`LineLineOverlapIgnored`）。はみ出し・フォント波及の警告文は「可能性あり」表現で、近似である旨も文中に明記。
  - (c) グループ化の 1 行表示＋展開（§5.3 テスト 1〜3） — GoogleTest で合格（`RowMoveGrouped`／`TwoRowsMoveSeparately`／`GroupSummaryText`）。微小グループの既定折りたたみと `+12.5単位` の小数表示もテスト済み。
  - (d) 回帰なし — 938 件→960 件、全合格。既存テストの失敗なし。
- **未検証（実機でしか確認できないもの）**：差分テーブルがプレビュー画面の下に 4 列で表示されること。グループ行のクリックで展開／折りたたみ、微小グループが既定で折りたたまれていること。レコード行のクリックでスポットライトが移動すること（キャンバスのスクロール、バナーの旧／新プレビュー）。FILE_ADD 行のクリックではスポットライトが動かないこと。重なり警告が第 4 列に出て 2 項目名を含むこと、深刻度がレイアウト破壊になること。ラベルの「微小差 N件を無視」表示。**重点：キャンバスで変更図形が緑／赤破線で強調されること（形状キー修正前は強調が働かなかったはずのため、目視での変化確認をお願いしたい）**。

### 69.4 次への移行
- F6（名称マスター管理）を着手済み（ブランチ `merge20/form-f6`）。IDD 306、コントロール 1905 以降、コマンドが必要なら 38985 以降。完了報告待ち。
- B7 PR2（ダイアログ＋ゲート）は PR #66 として作成済みだが、CI 実行 45988382245 が failure（3 Error(s)）。`Merge20VerifyGate.cpp(151)` の C2440（`CMergeResultTextBuffer *` → `const CDiffTextBuffer *` への変換不可）と `Merge20VerifyDlg.cpp(107)` の C2653／C3861（`ucr::toTString` が存在しない）。担当に修正を指示済み。全文ログは `~/workspace/.coord/b7/b7_pr2_ci_fail_log.txt`。
- ID 割り当て状況（2026-11-27 11:27 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 用に 305 を確保済み、F6 用に 306 を確保済み。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 用に 1893–1897 を確保済み、F5 が 1903–1904 を使用（1898–1902 は予備）、F6 用に 1905 以降を確保済み。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第 2 段階用に 38981 を予約済み、B3 が 38982 を使用、B7 用に 38983 を確保済み、F5 用に 38984 を確保済み（未使用）、F6 用に 38985 以降を確保済み。`_APS_NEXT_RESOURCE_VALUE` 306（F6 の予約分を含む見込み。次回 master 読取時に確認すること）、`_APS_NEXT_COMMAND_VALUE` 38983、`_APS_NEXT_CONTROL_VALUE` 1905（F5 マージ時点）。

## 70. B7 (2/2) — 出力時最終検証ゲート＋問題一覧ダイアログ（PR #66）

### 70.1 概要
- B7（出力前の最終検証ゲート）の PR 2：ダイアログ＋ゲート本体。`SaveMergeResult` の冒頭にフックし、§68 の検証エンジンで問題があれば出力を停止して問題一覧ダイアログを表示する。問題がなければダイアログを出さずに素通りする。
- ゲート API：`Merge20VerifyBeforeOutput(CMergeDoc& doc)` — 問題なし→`true`（ダイアログなし）、問題あり→問題一覧ダイアログ（Close→`false`、Output anyway→`true`、Jump→移動して `false`）。非対話実行ではダイアログを出さずログに記録して `false`。
- S7 用の下位 API：`Merge20VerifyCheckMergeDocFile` → `Merge20VerifyCollectRows` → `Merge20VerifyShowIssuesAndDecide` → `Merge20VerifyJumpToIssue`。同じエンジン・同じダイアログを複数ファイルで使うためのもの（S7 自体は未着手のため呼び出し元なし）。
- フック位置：`SaveMergeResult` 冒頭（結果バッファ初期化チェック直後、未解決プロンプトの前）。二重プロンプトにならない順序。
- stock の保存（`DoSave`）にはゲートを付けない：`DoSave` は比較ペイン（OLD／NEW）の書き込みでマージ出力ではなく、ゲートすると正当な OLD／NEW 編集を阻み、stock の保存・終了経路（バックアップ・読み取り専用・`SaveModified`）に割り込むリスクがあるため。ゲート対象はマージ出力の漏斗 `SaveMergeResult` のみ。
- 項目別 ON／OFF：オプションキー `Merge20/VerifyRediff`・`VerifyLineCount`・`VerifySyntax`・`VerifyConsistency`（すべて既定 ON）。設定画面への追加は未実施で、INI／レジストリで直接切り替える。

### 70.2 変更内容（PR #66、15 コミット、11 ファイル、+1196/−4）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20VerifyDlg.h/.cpp` | 問題一覧ダイアログ（IDD 305、520×340） |
| `Src/Merge20/Merge20VerifyGate.h/.cpp` | ゲート本体（`Merge20VerifyBeforeOutput`、Jump、S7 用下位 API） |
| `Src/Merge.rc` | `IDD_MERGE20_VERIFY` 追加 |
| `Src/resource.h` | ダイアログ 305・コントロール 1893–1897 割り当て。`_APS_NEXT_*` は 306／1898 へ（F5 マージ後の master 値 1905 を維持。台帳に B7・F5 両方の割り当てを併記） |
| `Src/MergeDoc.h` | `Merge20GetNewPane()` 宣言 |
| `Src/MergeDocResultPane.cpp` | `SaveMergeResult` へゲートをフック＋`Merge20GetNewPane()` 実装 |
| `Translations/WinMerge/Japanese.po` | 新規 msgid 27 件（日本語訳付き。各 msgstr 直後に空行あり） |
| `Src/Merge.vcxproj`／`.filters` | Dlg／Gate 登録（既定 PCH） |

- **準備ファイルのリベース**：PR2 の準備ファイルは F4 スポットライト・マージ前の master を基に作られており、`Merge.vcxproj`（`msimg32.lib` 追加＋F4 ファイル 4 件）、`.filters`（F4 ファイル 4 件）、`Japanese.po`（F4 エントリ 10 件）が欠けていた。そのままコミットすると F4 を巻き戻すため、3 件だけ現 master（`4049229e`）へリベースした（`~/workspace/.coord/b7/rebase_pr2.py`）。リベース後の `.po` は 2292 msgid（2265＋27）で F4 の 10 件も含むことを確認済み。
- **`LF_GHOST` スキップの修正あり**：`Merge20VerifyGate.cpp` の `appendBufferLines`（197 行目）と `CountRealLines`（75 行目）でゴースト行をスキップし、`BuildExpandedResultText`（保存内容）とのミラーが一致する。

### 70.3 検証レポート（コーディネーター検証済み）
- **PR #66**：head `1acab176`、base master `05052799`（F5 マージ済み）。15 コミット、変更ファイル 11 件（B7 の 11 ファイルのみ）。
- CI 失敗と修正（実行 45988382245、3 Error(s)）：原因は 2 つとも準備ファイルの欠落した include だった。
  1. `Merge20VerifyGate.cpp(151)` C2440：`CMergeResultTextBuffer` は `CDiffTextBuffer` の派生クラスだが（`MergeResultTextBuffer.h:22`）、`MergeDoc.h:37` は前方宣言のみで、Gate.cpp の翻訳単位では継承関係が見えていなかった → `#include "MergeResultTextBuffer.h"` を追加（コミット `fcf02fe0`）。
  2. `Merge20VerifyDlg.cpp(107)` C2653／C3861：`ucr::toTString` 自体は正しい（`Src/Common/unicoder.h:107–108` で宣言され、`MergeDoc.cpp` でも使用）。誤りは include の欠落だけで、別名への置き換えは不要だった → `#include "unicoder.h"` を追加（コミット `71b74e57`）。
  - **教訓**：調整役の初報は「`ucr::toTString` は存在しない」と断定したが誤りだった。コンパイルエラーの原因は「識別子が存在しない」ことと「include が欠けている」ことの両方があり得る。実コードを読んで確認すること。
  - 警告 C4459（Gate.cpp の `files` がグローバル宣言を隠す）は警告のみで、ビルド失敗の原因ではないため触っていない。
- F5 マージとの競合解消：修正後に master が進み PR #66 は mergeable=false になったため、3-way マージコミット `1acab176`（親：`fcf02fe0`＋`05052799`）を作成。重なった 4 ファイル（`Merge.vcxproj`、`.filters`、`resource.h`、`Japanese.po`）を base `4049229e`／master／branch の 3 者で `diff3 -m`。`resource.h` のみ台帳まわりで 3 箇所の競合があり、両方を残す形で解消（`_APS_NEXT_CONTROL_VALUE` は master 側の 1905（最大値）、台帳の割り当て行は B7・F5 両方の情報を併記、`_APS_NEXT_RESOURCE_VALUE` は 306 のまま）。マージ後の検証：vcxproj／.filters に B7 と F5 の両方の登録あり、`.po` は 2298 msgid（2265＋B7 27＋F5 6）、重複は stock 由来の既存分のみ、msgstr 直後の空行漏れ 0、diff3 マーカーなし。
- **CI（ブランチ head）**：実行 45989997164、completed／success。全文ログ（3,605,853 バイト）を担当が取得・確認（`~/workspace/.coord/b7/pr2_build_success.log`）。コーディネーターも同ログの要所を確認：`Build succeeded`、`0 Error(s)`（警告 430 件は既存パターン）、GoogleTest `[ PASSED ] 960 tests.`、`[ FAILED ]` 0 件、`Merge20 verify: 100 files x 500 lines, 107 ms total, 0 issues`（第 1 段階の 102 ms と同オーダー）、`.po` 番兵通過。成果物：`WinMerge-x64.zip` 26,695,696 バイト、`WinMerge-x64-Setup-x64.exe` 40,147,826 バイト、expire 2027-02-25。
- **マージ**：マージコミット `968b3b90`。マージコミット CI：実行 45992046764、completed／success。全文ログ（3,605,610 バイト、ジョブ `build` 140482031521）をコーディネーターが取得・確認（`~/workspace/.coord/b7/b7_pr2_merge_log.txt`）。GoogleTest `[ PASSED ] 960 tests.`、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`、`Merge20 verify: 100 files x 500 lines, 95 ms total, 0 issues`。
- **成果物（実行 45992046764）**：`WinMerge-x64.zip` 26,695,717 バイト、`WinMerge-x64-Setup-x64.exe` 40,150,348 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **受け入れ条件の照合（B7 の (b)〜(f)）**：
  - (b) 問題あり→出力停止＋一覧表示 — `SaveMergeResult` 冒頭の `Merge20VerifyBeforeOutput` が問題時に `false` を返し、問題一覧ダイアログを表示。**実機での停止動作は未検証**。
  - (c) 一覧から該当ファイル・差分へ移動 — `Merge20VerifyJumpToIssue`（差分つきはその差分の結果セグメントへ、差分なしは結果行へ）。**実機未検証**。
  - (d) 問題なしは素通り — 問題なしならダイアログを出さず `true` を返す。エンジン計測は CI ログで 107 ms（100 ファイル×500 行）を確認。
  - (e) 項目別 ON／OFF — オプションキー 4 件（すべて既定 ON）。設定画面への追加は未実施で、INI／レジストリで直接切り替える。
  - (f) 回帰なし — フックはゲート判定のみで通常の保存・マージ・B1 結論付与の経路を変更しない。GoogleTest 960 件全合格で確認。
  - (a) 4 項目の検証は第 1 段階で実装済み。本段階ではゲート経由で呼ばれる（`Merge20VerifyBeforeOutput` → `Merge20VerifyRunFile`）。
- **未検証（実機でしか確認できないもの）**：ダイアログの実機表示（レイアウト、列幅、日本語表示）。ゲートの実機動作：問題あり→出力停止・一覧表示・Jump・Output anyway、問題なし→素通りして保存されること。実ファイル（実マージ結果）での 4 項目の検出精度。S7 複数ファイル集約の下位 API（S7 未着手のため呼び出し元がなく、実機では未使用の経路）。

### 70.4 次への移行
- B7 はこれで完了（エンジン＋ゲート）。次は **B8**（検証レポートの保存。B7 に依存）に進む。B8 の担当を spawn する。
- F6（名称マスター管理）は着手済み（ブランチ `merge20/form-f6`、PR の CI が進行中とみられる）。完了報告待ち。
- ID 割り当て状況（2026-11-27 11:40 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 が 305 を使用、F6 用に 306 を確保済み。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 が 1893–1897 を使用（1898–1902 は予備）、F5 が 1903–1904 を使用、F6 用に 1905 以降を確保済み。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第 2 段階用に 38981 を予約済み、B3 が 38982 を使用、B7 用に 38983 を確保済み、F5 用に 38984 を確保済み（未使用）、F6 用に 38985 以降を確保済み。`_APS_NEXT_RESOURCE_VALUE` 306、`_APS_NEXT_COMMAND_VALUE` 38983、`_APS_NEXT_CONTROL_VALUE` 1905（B7 PR2 マージ時点）。

## 71. B8 — 検証レポートの保存（PR #68）

### 71.1 概要
- B7 の検証結果レポートを、出力物と一緒に保存する。レポートには検証日時・対象ファイル数・各項目の結果・問題の一覧（あった場合）を含める。形式は S6 の CSV と同じ方針（BOM 付き UTF-8、Excel で読めること）。ファイル名は `VerifyReport_YYYY-MM-DD.csv`。
- 保存場所：マージ出力と**同じフォルダ**（証拠が出力と一緒に移動する）。同名が既にあれば `VerifyReport_YYYY-MM-DD_HHMMSS.csv`、さらにあれば `_2`・`_3`…（最大 999）。既存レポートを上書きしない。
- SaveMergeResult 経路：保存成功時に出力ファイルと同じフォルダへレポートを保存。ゲートが既に実行した検証の結果を使い、再検証はしない。レポート書き込み失敗時は警告のみで、保存自体は失敗しない。「それでも出力する」で進めた場合も問題一覧付きでレポートを出す。検証されなかった保存ではレポートを出さない（マージ結果未構築の場合。エラーではない）。
- 単独出力：フォルダ比較の右クリックメニューに「検証レポートを出力…」を追加（S6 の直後）。対象は「このフォルダ比較に属する、開いているファイル比較」（OLD 側が左ルート配下・NEW 側が右ルート配下）。保存先はユーザーが選ぶ。対象 0 件のときは案内メッセージを出す。開いているが未構築のファイルは「未検証」行としてレポートに残す。
- B11 の監査ログとは別物である（検証レポート＝出力の正しさの証拠、監査ログ＝判断の履歴）。本スライスは監査ログに触れていない。

### 71.2 変更内容（PR #68、18 コミット、17 ファイル、+1175/−94）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20VerifyReport.h/.cpp` | レポート構造体と CSV 生成（純粋モジュール）。S6 と同じ規則（テキスト常時ダブルクォート、`"` は `""`、件数は素の数字、CRLF）。表示文字列は呼び出し側からラベルで渡すため ASCII のみ |
| `Src/Merge20/Merge20VerifyReportExport.h/.cpp` | MFC アダプタ。ラベル合成（`_()`）、ゲート結果→レポート変換、S6 と同じ書き込み（`UniStdioFile::OpenCreateUtf8`＋`WriteBom`）、既定名・衝突回避付きパス生成、SaveMergeResult 用フック |
| `Src/MergeDocResultPane.cpp` | SaveMergeResult 経路でのレポート保存フック |
| `Src/DirView.cpp`／`DirView.h` | 右クリックメニュー「検証レポートを出力…」（S6 の直後） |
| `Src/Merge20/Merge20VerifyGate.h/.cpp` | ゲートの拡張。検査項目名・問題の場所・詳細を合成する共有ヘルパーを新設し、B7 ダイアログは委譲するだけにした（文言の変更なし、ダイアログとレポートで食い違わない）。`Merge20VerifyFileResult` に `verified` を追加し、未構築ファイルも NEW 側ファイル名付きで返す。`Merge20VerifyBeforeOutput` に任意の out 引数を追加（呼び出し元は SaveMergeResult のみ） |
| `Src/Merge20/Merge20VerifyDlg.cpp` | 共有ヘルパーへの委譲（−88 行。文言の変更なし） |
| `Src/resource.h` | コマンド `ID_MERGE20_VERIFY_REPORT` 38983。`_APS_NEXT_COMMAND_VALUE` を 38984 へ |
| `Translations/WinMerge/Japanese.po` | 新規 msgid 20 件（付き。各 msgstr 直後に空行あり）。B7 の検査項目名・問題詳細は流用 |
| `Src/Merge.vcxproj`／`.filters`、`Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` | 登録 |
| `Testing/GoogleTest/Merge20/Merge20VerifyReport_test.cpp` | 単体テスト 5 件 |

- **ID 割り当ての訂正**：B7 は 38983 を「確保済み」と台帳に記録していたが、実際にはコマンド ID を使っておらず（PR #66 の差分にコマンド ID はない）、master の `_APS_NEXT_COMMAND_VALUE` は 38983 のままだった。F6（PR #67）もコマンド ID を使っておらず、F6 ブランチの `_APS_NEXT_COMMAND_VALUE` も 38983 のままであることを実読で確認したうえで、B8 が 38983 を採用した。**教訓：台帳の「確保済み」は「使用済み」とは限らない。ID を取るときは master の `_APS_NEXT_*` の現在値と、並行ブランチの実際の使用状況を両方確認すること。** なお F5 の予約 38984（未使用）は引き続き未使用のままである。

### 71.3 検証レポート（コーディネーター検証済み）
- **PR #68**：head `7d5b09b7`、base master `a3856905`。18 コミット、変更ファイル 17 件（B8 の 17 ファイルのみ）。
- 1 回目の CI（実行 45996991249）も成功していたが、そのログで新規警告 C4459 が 1 件だけ出ていたため、ローカル変数名を `files`→`verifyFiles` に変える修正コミットを積み、CI を再実行して警告が消えたことを確認済み（残る同クラス警告は B7 以前からの既存分のみ）。
- **CI（ブランチ head）**：実行 45998767118、completed／success。全文ログ（3,607,775 バイト）を担当が取得・確認（`~/workspace/.coord/b8/ci2-full-log.txt`）。コーディネーターも同ログの要所を確認：`Build succeeded`、`0 Error(s)`、GoogleTest `[ PASSED] 965 tests.`（960＋新規 5 件）、`[ FAILED]` 0 件。`.po` 番兵通過。成果物：`WinMerge-x64.zip` 26,712,881 バイト、`WinMerge-x64-Setup-x64.exe` 40,161,984 バイト、expire 2027-02-25。
- **マージ**：マージコミット `10251cb1`。マージコミット CI：実行 46000837671、completed／success。全文ログ（3,607,019 バイト、ジョブ `build` 140511759644）をコーディネーターが取得・確認（`~/workspace/.coord/b8/b8_merge_log.txt`）。GoogleTest `[ PASSED ] 965 tests.`、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`、`Merge20 verify: 100 files x 500 lines, 81 ms total, 0 issues`。
- **成果物（実行 46000837671）**：`WinMerge-x64.zip` 26,712,940 バイト、`WinMerge-x64-Setup-x64.exe` 40,176,397 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **受け入れ条件の照合（gap 分析 B8）**：
- (a) 出力時に検証レポートが一緒に出ること — SaveMergeResult 経路で満たす。**S7-ZIP への同梱は S7 待ち**（S7 未実装のため）。S7 ができたら `Merge20VerifyReportFromFileResults`／`Merge20VerifyReportWriteFile` を S7 側から呼んで同梱する設計にしてある。
- (b) 検証日時・対象件数・各項目の結果が含まれること — 集計（検証日時・対象ファイル数・合格／不合格・問題総数）＋ファイル×4 項目のテーブル＋問題一覧。単体テストで完全一致を確認。
- (c) Excel で文字化けなく開けること — BOM 付き UTF-8。CI ではバイト列のテストで確認（`Utf8Bytes`：CRLF、日本語の UTF-8 バイト、BOM）。**Excel での目視は実機未検証**。
- (d) 検証レポートだけを単独で出力できること — 右クリックメニューから出力できる。**ただし開いているファイル比較だけが対象**（開いていないファイルを検証するにはヘッドレス比較が必要だが、その仕組みはコードベースに存在しないため作っていない）。メニューからの操作は実機未検証。
- **未検証（実機でしか確認できないもの）**：正常なマージ保存で出力フォルダに `VerifyReport_YYYY-MM-DD.csv` ができること。Excel で開いて日本語が文字化けしないこと。同日 2 回目の保存で時刻付きファイル名になり、1 本目が上書きされないこと。「それでも出力する」の場合に問題一覧がレポートに残ること。右クリックメニューからの単独出力（0 件時の案内含む）。B7 ダイアログの表示・ジャンプ・［それでも出力する］が従来どおりであること。

### 71.4 次への移行
- B8 はこれで完了。次は **B10**（判断の取消／やり直し）に進む。B10 の担当を spawn する。
- F6（名称マスター管理）は PR #67 として作成済みだが、CI 実行 45991352517（head `e967f29c`）が failure で、担当が修正中（head は `02732761` まで進んでいる）。完了報告待ち。
- ID 割り当て状況（2026-11-27 12:40 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 が 305 を使用、F6 用に 306 を確保済み。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 が 1893–1897 を使用（1898–1902 は予備）、F5 が 1903–1904 を使用、F6 用に 1905–1909 を確保済み（F6 の実読による）。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第 2 段階用に 38981 を予約済み、B3 が 38982 を使用、B8 が 38983 を使用（B7 の予約は未使用だったため B8 が採用。§71.2 参照）、F5 用に 38984 を予約済み（未使用）。`_APS_NEXT_RESOURCE_VALUE` 306、`_APS_NEXT_COMMAND_VALUE` 38984、`_APS_NEXT_CONTROL_VALUE` 1905（B8 マージ時点。F6 マージ時にコントロール ID の台帳を確認すること）。

## 72. Wave 5 F6 — FORM 名称マスター管理（PR #67）

### 72.1 概要
- FORM 比較の名称マスター（帳票 ID→帳票名）の管理。モーダルでの編集・保存・即時反映、初期リストへの復元、名称解決（HTML 版の `getFormName` と同じ規則）。
- 保存先：学習データと同じ方式（INI 時は `<progpath>\Merge20FormMaster\`、それ以外は `%APPDATA%\WinMerge\Merge20FormMaster\`）。ファイル名は `Merge20FormMaster.txt`（HTML 版と同じタブ区切りテキスト形式。`ID\t帳票名` の行形式）。UTF-8＋BOM、原子書き込み。
- 上部バーにボタン「名称マスター（%1種）」を追加。`CFormCompareDoc::RefreshFormNames()` で保存後の即時反映を行う。
- 複数ウィンドウでの名称マスター同時編集の競合は警告しない（第 1 版の制限）。

### 72.2 変更内容（PR #67、24 コミット、16 ファイル、+1313/−6）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20FormMaster.h/.cpp` | 純粋モジュール（`ParseText`／`ToText`／`GetDefault`／`Resolve`／`CountUniqueIds`） |
| `Src/Merge20/Merge20FormMasterDlg.h/.cpp` | 編集モーダル（コード生成コントロール、件数バッジ、保存＝即時反映、初期リスト復元＋確認ダイアログ） |
| `Src/Merge20/Merge20FormCompareDoc.h/.cpp` | `RefreshFormNames()` |
| `Src/Merge20/Merge20FormCompareView.h/.cpp` | 上部バーに「名称マスター（%1種）」ボタンを追加 |
| `Src/Merge.rc` | `IDD_MERGE20_FORM_MASTER` 追加 |
| `Src/resource.h` | ダイアログ 306、コントロール 1905–1909。`_APS_NEXT_*` は 308／38984／1910 へ |
| `Translations/WinMerge/Japanese.po` | 新規 msgid 11 件（日本語訳付き。各 msgstr 直後に空行あり） |
| `Src/Merge.vcxproj`／`.filters`、`Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` | 登録 |
| `Testing/GoogleTest/Merge20/Merge20FormMaster_test.cpp` | 単体テスト 13 件 |

- **初期データの検証**：付録 A と原文 HTML `defaultMasterData` が完全一致することを確認済み（171 行・142 の一意 ID・15 件の複数名称 ID）。テストで 171／142、スポットチェック、≤3 件「 ／ 」結合、≥4 件「先頭の名称 ほか全 N 種」（ZEB302R001 の 10 名称含む）を検証。
- **編集内容の検証**：空行は無視。タブのない行は警告して保存を中止（行番号付き）。重複する ID＋名称の行は保存時に除去して正規化（同一 ID の異なる名称は重複ではない）。
- **名称解決の規則**：ファイル名から拡張子を除いたベース名で完全一致、なければ ID を含むかで調べる。未登録は「（名称未登録）」。同一 ID に複数名称がある 15 件は 3 件までは「 ／ 」で結合、4 件以上は「先頭の名称 ほか全 N 種」。

### 72.3 検証レポート（コーディネーター検証済み）
- **PR #67**：head `b4a12a52`（マージコミット）、base master `10251cb1`（B8 マージ）。24 コミット、変更ファイル 16 件（F6 の 16 ファイルのみ）。
- CI 失敗と修正（実行 45991352517、failure）：`Merge20FormMasterDlg.cpp(177)` の `AfxMessageBox(_(...))` で `String`→`LPCTSTR` の `.c_str()` 漏れ（C2665）→ 修正コミット `79b4c87c`。
- master マージ（2 回）：作業中に master が 2 回進み、PR がコンフリクトしたため master をマージして解消。
- 1 回目：B7（`a3856905`）との衝突。`diff3` で解決。F6 は IDD 306／コントロール 1905–1909／`_APS_NEXT_*` 308／1910 を維持、B7 は IDD 305／1893–1897 を維持。コミット `02732761`。CI 実行 46000427241、success（973 tests 合格）。
- 2 回目：B8（PR #68、`10251cb1`）との衝突。`resource.h` で `_APS_NEXT_*` のみ 1 件の実コンフリクト（F6 の 308／1910 と B8 のコマンド 38984 を両方採用）。`Merge.vcxproj`／`.filters`、`UnitTests.vcxproj`／`.filters`、`Japanese.po` は `diff3` で自動解決。コミット `b4a12a52`。CI 実行 46002366021 が最終の success（978 tests＝973＋B8 の 5 件）。
- マージ検証：6 ファイルすべて、master 側との差分＝F6 の変更のみ、ブランチ側との差分＝B7／B8 の変更のみであることを確認。`MERGE20_NOTES.md` は master のものをそのまま採用（未編集）。
- **CRLF の教訓**：コミットしたブランチ版ファイルが CRLF を失っていた（Python テキストモード読み書き）。`resource.h`／`vcxproj`／`.filters` はマージ時に CRLF へ復元済み。2 回目のマージでは 3 者とも CRLF が揃っていたため問題なし。**教訓：API 経由でファイルを読み書きするときはバイナリモードを使うこと。テキストモードの読み書きは改行を壊す。**
- ブランチ履歴に空コミットが 2 件ある（`3750d1f8` View.h、`f77db584` View.cpp。失敗した編集スクリプトを未検証でコミットしたもの）。無害のためそのままマージした（PR 本文に記載済み）。
- **CI（ブランチ head）**：実行 46002366021、completed／success。全文ログ（3,610,665 バイト）を担当が取得・確認（`~/workspace/.coord/f6/ci-46002366021.log`）。コーディネーターも同ログの要所を確認：`Build succeeded`、`0 Error(s)`、GoogleTest `[ PASSED] 978 tests.`（965＋F6 の 13 件）、`[ FAILED]` 0 件。`.po` 番兵通過。成果物：`WinMerge-x64.zip` 26,735,957 バイト、`WinMerge-x64-Setup-x64.exe` 40,178,504 バイト、expire 2027-02-25。
- **マージ**：マージコミット `18f7c718`。マージコミット CI：実行 46004437421、completed／success。全文ログ（3,610,500 バイト、ジョブ `build` 140523930793）をコーディネーターが取得・確認（`~/workspace/.coord/f6/f6_merge_log.txt`）。GoogleTest `[ PASSED ] 978 tests.`、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`、`Merge20 verify: 100 files x 500 lines, 97 ms total, 0 issues`。
- **成果物（実行 46004437421）**：`WinMerge-x64.zip` 26,736,790 バイト、`WinMerge-x64-Setup-x64.exe` 40,185,862 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **受け入れ条件の照合（設計書 §10 F6）**：
- (a) 付録 A の初期データ（171 行・142 の一意 ID）が正しく読み込まれ、帳票名が表示されること。同一 ID に複数名称がある 15 件について、3 件までは「 ／ 」で結合し、4 件以上は「先頭の名称 ほか全 N 種」と表示されること — GoogleTest 13 件で検証済み。**実機での表示確認は未検証**。
- (b) モーダルでの編集・保存・即時反映が正しく動くこと。タブのない行は警告して保存を中止すること。重複行は保存時に除去されること — 単体テストで検証済み。**モーダル操作・即時反映の目視は実機未検証**。
- (c) 「初期リストに戻す」で付録 A の初期データに戻ること（確認ダイアログ付き） — ロジックはテスト済み。**ボタン操作の実機確認は未検証**。
- (d) 回帰なし。名称マスターの保存が他の設定に影響しないこと — 全 978 tests 合格。名称マスターの保存は独立ファイル（`Merge20FormMaster.txt`）で他設定に影響しない設計。
- **未検証（実機でしか確認できないもの）**：モーダルの操作、保存後の即時反映の目視、リセットボタンの実機パス、件数バッジの表示。

### 72.4 次への移行
- F6 はこれで完了。Wave 5 は F7（CSV 出力＋受け入れテスト・文書化）を残すのみ。F7 の担当を spawn する。
- B10 PR1（判断の取消／やり直し）は着手済み（ブランチ `merge20/wf-b10`）。完了報告待ち。
- ID 割り当て状況（2026-11-27 13:05 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 が 305 を使用、F6 が 306 を使用（307 は `IDR_LOGO`）。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 が 1893–1897 を使用（1898–1902 は予備）、F5 が 1903–1904 を使用、F6 が 1905–1909 を使用。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第 2 段階用に 38981 を予約済み、B3 が 38982 を使用、B8 が 38983 を使用、F5 用に 38984 を予約済み（未使用）。`_APS_NEXT_RESOURCE_VALUE` 308、`_APS_NEXT_COMMAND_VALUE` 38984、`_APS_NEXT_CONTROL_VALUE` 1910（F6 マージ時点）。

## 73. B10 PR1 — 判断の取消／やり直し（Undo 基盤＋基本 3 種）（PR #69）

### 73.1 概要
- B10（判断の取消／やり直し）の PR1：Undo エンジン（純粋モジュール）＋基本 3 種の Undo。PR2（D5 分割・再結合、学習候補の受け入れ／却下の Undo、セッション保存・復元との相互作用の残り）は別途。
- Undo 対象（3 種）：(1) 確認内容の変更（キー 1〜5・Space・パネル循環。S2 連動の出力フラグも前後で一緒に記録）、(2) 出力チェックの切替、(3) 差分単位の結論（B1。結論＋連動する確認内容・出力フラグを記録。差分同一性 op／begin／end で陳腐化を検出）。
- 履歴はフォルダ比較単位（`CDiffContext` が保持）。履歴の深さの上限は **100 件**（超えた古いものから消える）。no-op は積まない。新規 push で redo を消す。
- Ctrl+Z／Ctrl+Shift+Z／Ctrl+Y で戻る／進む。両方の右クリックメニューに「判断の取り消し／やり直し」。
- **設計上の決定**：ファイル比較では Ctrl+Z はテキスト編集の Undo のまま（判断 Undo はメニューのみ）。Ctrl+Z がテキスト編集と衝突するため。文言で別系統であることを明記。フォルダ一覧では Ctrl+Z／Ctrl+Shift+Z／Ctrl+Y が判断 Undo として効く。
- 一括チェックは記録しない（`bRecordUndo=false`）。学習不変は構造で保証（Undo 経路は `Merge20SetDiffConclusionRestored`＋生 setter＋`ResultChooseSources` のみ）。
- **セッション復元は履歴を消す**（`ApplySessionStates` 開始時にクリア、保存では消さない）。マニュアル §17 と PR 本文に明記。
- B15：ショートカット一覧はマニュアル（`Docs/Users/WizMergeManual.html`）§16 の表にのみ存在したため、そこに Ctrl+Z／Ctrl+Shift+Z／Ctrl+Y の 2 行を追加（アプリ内一覧はなし）。§17 に制限事項も追加。

### 73.2 変更内容（PR #69、30 コミット、22 ファイル、+1206/−21）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20Undo.h/.cpp` | Undo エンジン（純粋モジュール。スタック・上限 100 件・no-op 除外・redo 消去） |
| `Testing/GoogleTest/Merge20/Merge20Undo_test.cpp` | 単体テスト 10 件 |
| `Src/DiffContext.h/.cpp` | 履歴の保持（フォルダ比較単位） |
| `Src/DirDoc.h/.cpp` | Undo／Redo の実行・メニュー・Ctrl+Z 配線 |
| `Src/DirView.h/.cpp` | 右クリックメニュー「判断の取り消し／やり直し」 |
| `Src/MergeDoc.h/.cpp`、`Src/MergeDocResultPane.cpp` | B1 結論の Undo 対応（結論＋連動フラグの記録・復元、陳腐化の検出） |
| `Src/Merge20/Merge20FileListBar.cpp` | パネル循環の Undo 記録 |
| `Src/Merge20/Merge20SessionDoc.cpp` | セッション復元時の履歴クリア |
| `Docs/Users/WizMergeManual.html` | §16 ショートカット一覧に Ctrl+Z／Ctrl+Shift+Z／Ctrl+Y を追加、§17 に制限事項を追加 |
| `Src/Merge.rc` | メニュー項目追加 |
| `Src/resource.h` | コマンド `ID_MERGE20_UNDO_JUDGMENT` 38984、`ID_MERGE20_REDO_JUDGMENT` 38985。`_APS_NEXT_COMMAND_VALUE` を 38986 へ |
| `Translations/WinMerge/Japanese.po` | 新規 msgid（付き） |
| `Src/Merge.vcxproj`／`.filters`、`Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` | 登録 |

- **ID 割り当ての経緯**：38984 は F5 の予約（未使用）だったが、F5 は既にマージ済みで実際には使っていないため、B10 PR1 が採用した（B8 が B7 の予約 38983 を採用したのと同じ経緯。§71.2 の教訓どおり、master と F6 ブランチの両方で空きを確認済み）。これで F5 の予約 38984 は消費済みとなった。
- 検証中に見つけた 2 件の不具合を修正：`GetNextDiffPosition` が `const DIFFITEM&` を返すための const 修正（DirDoc.cpp）、テストの `_T` 用に `<tchar.h>` を明示 include。`.po` 検査の過剰な assert も修正（`<<<<<<<` は既存の正規文言、複数行 msgstr の継続行を許容）。

### 73.3 検証レポート（コーディネーター検証済み）
- **PR #69**：head `7326c1e9`（マージコミット）、base master `1d83fc34`（F6 マージ＋NOTES §72）。30 コミット、変更ファイル 22 件（B10 PR1 の 22 ファイルのみ）。
- F6 マージによるコンフリクト解消：PR 作成直後、作業中に F6（PR #67）が master（`1d83fc34`）へマージされ、PR は mergeable=false（dirty）で CI 未発火。競合した 7 ファイル（Merge.rc、resource.h、Merge.vcxproj／.filters、UnitTests.vcxproj／.filters、Japanese.po）を新 master 版に載せ直して 7 コミット。さらに resource.h の `_APS_NEXT_*` 3 行が隣接しているため 3-way マージが衝突と判定したので、Git Database API で master を取り込むマージコミット `7326c1e9` を作成（master 側のみ変更の 10 ファイル＝NOTES・FORM 比較／名称マスター関連を master 版で取り込み）。その後 mergeable=True／clean となり CI が発火。**教訓：`_APS_NEXT_*` の 3 行は隣接しているため、並行ブランチのマージで衝突しやすい。ID の台帳行と `_APS_NEXT_*` 行を同時に触るときは注意すること。**
- **CI（ブランチ head）**：実行 46009557803、completed／success。全文ログ（3,610,582 バイト）を担当が取得・確認（`~/workspace/.coord/b10/ci-full-log.txt`）。コーディネーターも同ログの要所を確認：`Build succeeded`、`0 Error(s)`、GoogleTest `[ PASSED] 988 tests.`（978＋新規 10 件）、`[ FAILED]` 0 件。`.po` 番兵通過。成果物：`WinMerge-x64.zip` 26,744,878 バイト、`WinMerge-x64-Setup-x64.exe` 40,205,300 バイト、expire 2027-02-25。
- **マージ**：マージコミット `25fbc2b2`。マージコミット CI：実行 46011203073、completed／success。全文ログ（3,612,502 バイト、ジョブ `build` 140546864238）をコーディネーターが取得・確認（`~/workspace/.coord/b10/b10_pr1_merge_log.txt`）。GoogleTest `[ PASSED ] 988 tests.`、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`、`Merge20 verify: 100 files x 500 lines, 108 ms total, 0 issues`。
- **成果物（実行 46011203073）**：`WinMerge-x64.zip` 26,744,911 バイト、`WinMerge-x64-Setup-x64.exe` 40,190,897 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **受け入れ条件の照合（gap 分析 B10 のうち PR1 の範囲）**：
- (a) 状態変更・出力切替・差分結論のそれぞれが Undo／Redo できること — 実装済み（単体テストはスタック側、操作は実機チェックリストで確認）。
- (b) Undo しても学習データが変わらないこと — 構造上保証＋単体テスト `UndoDoesNotRecordLearning` 通過。
- (c) Undo の上限を超えた古い操作は消えること（クラッシュしないこと） — `DepthCapDropsOldestEntries`／`CapKeepsNewestEntries` 通過、クラッシュなし。
- (d) セッション保存・復元後も Undo 履歴が残っているか、残らない場合はその旨が明記されていること — **残らない**。復元時に消去し、マニュアル §17・PR 本文に明記。
- (e) 分割・再結合の Undo が結果ペインとセッションの両方に反映されること — **PR1 対象外**。PR 本文に明記（分割・再結合後は結論 Undo が Stale になる場合あり）。
- **未検証（実機でしか確認できないもの）**：Ctrl+Z 操作、メニュー表示・グレー表示、日本語表示、100 件超過時の挙動、セッション復元後の履歴消去、ファイル未オープン時の案内、一括チェックが Undo されないこと。PR 本文の実機チェックリスト 10 項目どおり。
- **PR2 への引き継ぎ**：D5 分割・再結合の Undo、学習候補の受け入れ／却下の Undo、セッション保存・復元との相互作用の残り。既知の制限：結論を Undecided へ Undo しても、その結論で作られた学習候補は復活しない（再スキャン／オプション変更時に再計算）。

### 73.4 次への移行
- B10 PR2（D5 分割・再結合、学習候補の受け入れ／却下の Undo）を次に spawn する。
- F7（CSV 出力＋受け入れテスト・文書化）は着手済み（ブランチ `merge20/form-f7`）。完了報告待ち。
- ID 割り当て状況（2026-11-27 13:55 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 が 305 を使用、F6 が 306 を使用（307 は `IDR_LOGO`）。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 が 1893–1897 を使用（1898–1902 は予備）、F5 が 1903–1904 を使用、F6 が 1905–1909 を使用。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第 2 段階用に 38981 を予約済み、B3 が 38982 を使用、B8 が 38983 を使用、B10 PR1 が 38984–38985 を使用（38984 は F5 の予約を消費）。`_APS_NEXT_RESOURCE_VALUE` 308、`_APS_NEXT_COMMAND_VALUE` 38986、`_APS_NEXT_CONTROL_VALUE` 1910（B10 PR1 マージ時点）。

## 74. Wave 5 F7 — FORM CSV 出力＋受け入れテスト・文書化（PR #70）

### 74.1 概要
- FORM 比較の CSV／Excel 出力（HTML 版の `exportToExcel` と同じ 2 部構成）＋ F0〜F6 の総合確認としての受け入れテストと文書化。Wave 5（F0〜F7）の最後のスライス。
- 第 1 部：帳票サマリ一覧 9 列（HTML 版 7 列＋「深刻度」・「一行要約」）。第 2 部：変更箇所別 詳細比較・影響分析一覧 14 列（HTML 版 12 列＋「グループ」・「微小差」。「移動／改名」の種別は表示語彙「移動」／「改名」）。
- 引用符規則は S6 の `Merge20Csv` と同じ（テキストは常時ダブルクォート、`"` は `""` にエスケープする）。BOM 付き UTF-8。出力日時を先頭行に入れる。
- 対象は変更のあるファイルのみ（HTML 版と同じ。完全一致のファイルは含まない）。フィルタで絞り込んだ結果だけを出力するのか、変更のあるファイル全体を出力するのかは、出力ダイアログで選べる（S6 の範囲選択ダイアログと同じ方式）。
- 起動：F2 の画面の上部バーにある CSV 出力ボタンから出力する。
- **実ファイルでの受け入れテストは未実施**：ユーザーの実際のフォーム定義 XML が手元にないため、合成テストデータでの受け入れテストのみ実施。1000 ファイル規模の計測は記録済み（下記）。実ファイルでの受け入れテストは「ユーザーの実ファイル待ち」。

### 74.2 変更内容（PR #70、18 コミット、16 ファイル、+1199/−10）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20FormCsv.h/.cpp` | 2 部構成の CSV 行組み立て（純粋モジュール。S6 と同じ引用符規則） |
| `Src/Merge20/Merge20FormCsvExport.h/.cpp` | BOM 付き UTF-8 書き出し |
| `Src/Merge20/Merge20FormCsvExportDlg.h/.cpp` | 範囲選択ダイアログ（IDD 308） |
| `Src/Merge20/Merge20FormCompareView.h/.cpp` | 上部バーに CSV 出力ボタン（IDC 1910）を追加 |
| `Src/Merge.rc` | `IDD_MERGE20_FORM_CSV_EXPORT` 追加 |
| `Src/resource.h` | ダイアログ 308、コントロール 1910–1912。`_APS_NEXT_*` は 309／38986／1913 へ |
| `Translations/WinMerge/Japanese.po` | 新規 msgid 6 件（日本語訳付き。各 msgstr 直後に空行あり） |
| `Src/Merge.vcxproj`／`.filters`、`Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` | 登録 |
| `Testing/GoogleTest/Merge20/Merge20FormCsv_test.cpp` | 単体テスト 8 件（2 部構成・列数・移動／改名の語彙・グループ／微小列・エスケープ・範囲選択・F0〜F6 総合確認・1000 ファイル計測） |

- グループ列はグループ要約文（単独の変更は空欄）、微小差列は §5.2 の微小グループ所属の変更に「微小」。ラベルはのまま。
- コマンド ID は不使用。台帳で F5 が予約としていた 38984 は master の B10 が実際に取得済み（`ID_MERGE20_UNDO_JUDGMENT` 38984、`ID_MERGE20_REDO_JUDGMENT` 38985、`_APS_NEXT_COMMAND_VALUE` 38986）。

### 74.3 検証レポート（コーディネーター検証済み）
- **PR #70**：head `c1b8c3fd`（マージコミット）、base master `61104532`（B10 PR1 マージ＋NOTES §73）。18 コミット、変更ファイル 16 件（F7 の 16 ファイルのみ）。
- master マージ：ブランチ作成後に master が 32 コミット進み（B10 判断 undo／redo）、PR が一時 mergeable=false（dirty）になった。Git Database API のみで master をブランチへマージして解消（マージコミット `c1b8c3fd`、parents [F7 head `2916c6ee`, master `61104532`]）。競合した 7 ファイル（resource.h、Merge.rc、Merge.vcxproj／.filters、UnitTests.vcxproj／.filters、Japanese.po）は手動で両方の変更を統合し、検証済み（統合版と master との差分は F7 の変更のみ、統合版と F7 ブランチとの差分は B10 の変更のみ。Merge.rc の空行 1 行の配置ずれも修正済み）。統合版の XML 4 ファイルはパース検証済み、`.po` は重複 msgid なし（検出された重複は master・branch 両方に既存の文脈違いのもので、マージ起因ではない）。
- **CI（ブランチ head）**：実行 46016786597、completed／success。全文ログ（3,616,055 バイト）を担当が取得・確認（`~/workspace/.coord/f7/ci-46016786597.log`）。コーディネーターも同ログの要所を確認：`Build succeeded`、`0 Error(s)`、GoogleTest `[ PASSED] 996 tests.`（988＋新規 8 件。`Merge20Form*` 計 131 テスト OK）、`[ FAILED]` 0 件。`.po` 番兵通過（マージ後は master の B10 分を含め 2345 msgid）。成果物：`WinMerge-x64.zip` 26,755,669 バイト、`WinMerge-x64-Setup-x64.exe` 40,194,081 バイト、expire 2027-02-25。
- **マージ**：マージコミット `85b5c843`。マージコミット CI：実行 46019359349、completed／success。全文ログ（3,614,866 バイト、ジョブ `build` 140574660286）をコーディネーターが取得・確認（`~/workspace/.coord/f7/f7_merge_log.txt`）。GoogleTest `[ PASSED ] 996 tests.`、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`、`Merge20 verify: 100 files x 500 lines, 90 ms total, 0 issues`。
- **成果物（実行 46019359349）**：`WinMerge-x64.zip` 26,754,837 バイト、`WinMerge-x64-Setup-x64.exe` 40,196,291 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **受け入れ条件の照合（設計書 §10 F7）**：
- (a) CSV の 2 部構成・列構成が §9 どおりであること。BOM 付き UTF-8 で Excel で開けること — CI 上は合格（2 部構成・列数・BOM・範囲選択の単体テスト）。**Excel で開けること自体の目視は実機未検証**。
- (b) 実ファイル（ユーザーの実際のフォーム定義 XML）での受け入れテストが合格すること。1000 ファイル規模での解析時間の計測を記録すること — **実ファイルでの受け入れテストは未実施（実ファイル待ち）**。合成テストデータでの受け入れテストのみ実施。**1000 ファイル規模の計測は記録済み：1000 ファイル×20 項目の解析＋CSV 出力が 346 ms**（マージ前実行では 325 ms。上限 120,000 ms に対し十分余裕あり）。
- (c) 検証ゲート (a)〜(d) のすべてについて、検証証拠（単体テスト結果・実機確認のチェックリスト）を記録すること。検証できない部分は「未検証」と明示すること — 単体テスト結果は全文ログで確認済み。実機確認のチェックリストは PR 本文に記載済み。実機でしか確認できない部分は「実機未検証」と明記。
- (d) 回帰なし — GoogleTest 996 件全合格（B10 の 10 件を含む）、`[ FAILED]` 0 件。
- **未検証（実機でしか確認できないもの）**：CSV 出力ボタンの表示、範囲ダイアログの件数表示、Excel での文字化けなし・2 部構成の目視、移動／改名／グループ／微小列の画面との一致、未解析時のメッセージ、**実ファイルでの受け入れテスト**、既存機能の回帰確認。実機確認手順は PR 本文に整備済み。
- **Wave 5 の完了**：F0〜F7 がすべてマージ済みとなり、FORM／XML 比較（可視化のみ）は一通り揃った。残る実ファイルでの受け入れテストはユーザーの実ファイル待ち。

### 74.4 次への移行
- B10 PR2（D5 分割・再結合、学習候補の受け入れ／却下の Undo）は着手済み（ブランチ `merge20/wf-b10-pr2`）。完了報告待ち。
- Wave 5 完了後の次の波は、B11（監査ログ）→ B12 → B13 → B5 → B6 の順（gap 分析の推奨順序）。UI は U3／U6（アートワークが先に必要）→ U4 → U7 → U8。
- ID 割り当て状況（2026-11-27 14:50 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 が 305 を使用、F6 が 306 を使用（307 は `IDR_LOGO`）、F7 が 308 を使用。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 が 1893–1897 を使用（1898–1902 は予備）、F5 が 1903–1904 を使用、F6 が 1905–1909 を使用、F7 が 1910–1912 を使用。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第 2 段階用に 38981 を予約済み、B3 が 38982 を使用、B8 が 38983 を使用、B10 PR1 が 38984–38985 を使用。`_APS_NEXT_RESOURCE_VALUE` 309、`_APS_NEXT_COMMAND_VALUE` 38986、`_APS_NEXT_CONTROL_VALUE` 1913（F7 マージ時点）。

## 75. B10 PR2 — 分割・再結合＋学習候補の受け入れ／却下の Undo（PR #71）

### 75.1 概要
- B10（判断の取消／やり直し）の PR2：D5 の分割・再結合の Undo、学習候補の受け入れ／却下の Undo、セッション保存・復元との相互作用の残り。B10 はこれで完了。
- **分割・再結合の Undo**：`Merge20SplitDiff`／`Merge20RegroupDiff` に `bRecordUndo`（既定 false）を追加し、メニュー経路だけが記録。適用は同じ関数を記録なしで呼ぶため結果ペインは手操作と同一に再構築される。グループは同一性（親範囲／先頭行）で探し直し、格納した添字は使わない。適用前に検証し、行を判断し直していたら Stale としてエントリを捨てる（再結合は先頭行の判断しか残さないため、行ごとの判断を黙って捨てない）。手編集のある結果も Stale。
- **学習候補の受け入れ／却下の Undo**：受け入れは差分ごとに結論・確認内容・出力フラグ・採用ペインを記録。取り消しは差分を未決定表示に戻し、受け入れ前の結論・ファイル状態を戻し、候補を再計算して再表示する。やり直しは格納した採用ペインで再適用する。却下の取り消しは却下集合から同一性を外して候補を再計算する（新規 `Merge20LearnUndismissCandidate`）。**どちらの方向も学習を記録せず、学習 JSON を書き換えない**（PR1 の §3.1 保証を PR2 の対象にも適用）。
- **PR1 の既知の制限を解消**：結論を Undecided へ Undo しても学習候補が復活しない件は、この PR で解消。`Merge20ApplyConclusionUndo` で取り消し先が Undecided のときに候補を再計算する（分割・再結合が既に走らせているのと同じ再計算で、学習データは触らない）。
- **セッション保存・復元**：PR1 の決定を継承し、**復元で履歴は全て消える**（分割・学習のエントリも同じスタック）。復元で再適用された分割は記録されない。分割の Undo は結果ペインとセッションの両方に反映される（保存時に開いている比較から分割状態を読み取る既存経路のため）。却下の集合はセッションに保存されない（既存仕様）ため、復元後に却下済み候補が再表示されうる。以上をマニュアル §17 に明記。

### 75.2 変更内容（PR #71、13 コミット、12 ファイル、+827/−43）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20Split.h/.cpp` | `bRecordUndo` の追加（メニュー経路だけが記録） |
| `Src/Merge20/Merge20Undo.h/.cpp` | 分割・再結合、学習候補の受け入れ／却下の Undo エントリ。`Merge20ApplyConclusionUndo` での候補再計算 |
| `Src/Merge20/Merge20LearnDoc.h/.cpp` | `Merge20LearnUndismissCandidate`（却下の取り消し） |
| `Src/MergeDoc.h`、`Src/MergeDocResultPane.cpp` | 分割・再結合／学習受け入れの Undo 適用 |
| `Src/DirDoc.cpp` | Undo 経路の配線 |
| `Docs/Users/WizMergeManual.html` | §17 にセッション保存・復元後の扱いを明記 |
| `Testing/GoogleTest/Merge20/Merge20Undo_test.cpp` | 単体テスト追加（5 件） |
| `Testing/GoogleTest/Merge20/Merge20Split_test.cpp` | 単体テスト追加（2 件） |

- **ID 割り当て**：新規 ID は**なし**。Undo／Redo は PR1 の 38984／38985 をそのまま使用。`_APS_NEXT_COMMAND_VALUE` は 38986 のまま。新規 `_()` 文言もないため **`.po` の変更なし**。

### 75.3 検証レポート（コーディネーター検証済み）
- **PR #71**：head `e6001c26`、base master `61104532`（B10 PR1 マージ＋NOTES §73。F7 マージ後の master `9696812d` よりは古いが、mergeable=True／clean のためそのままマージ）。13 コミット、変更ファイル 12 件（B10 PR2 の 12 ファイルのみ）。
- **CI（ブランチ head）**：実行 46018315051、completed／success。全文ログ（3,611,828 バイト）を担当が取得・確認（`~/workspace/.coord/b10/pr2/ci-full-log.txt`）。コーディネーターも同ログの要所を確認：`Build succeeded`、`0 Error(s)`、GoogleTest `[ PASSED ] 995 tests.`（988＋新規 7 件）、`[ FAILED ]` 0 件。新規テスト 7 件は全て `[ OK ]` をログで確認（Undo 5 件：分割・再結合の変化判定、学習エントリの変化判定、5 種の順序、学習の同一性、学習不変の往復＋Split 2 件：親範囲照合、グループ等価）。成果物：`WinMerge-x64.zip` 26,747,864 バイト、`WinMerge-x64-Setup-x64.exe` 40,195,039 バイト、expire 2027-02-25。
- **マージ**：マージコミット `b31a28d4`。マージコミット CI：実行 46021849129、completed／success。全文ログ（3,615,866 バイト、ジョブ `build` 140583187120）をコーディネーターが取得・確認（`~/workspace/.coord/b10/pr2/b10_pr2_merge_log.txt`）。GoogleTest `[ PASSED ] 1003 tests.`（995＋F7 の 8 件。PR #71 のブランチ CI 時点では F7 は未マージだったため）、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`、`Merge20 verify: 100 files x 500 lines, 70 ms total, 0 issues`。
- **成果物（実行 46021849129）**：`WinMerge-x64.zip` 26,759,600 バイト、`WinMerge-x64-Setup-x64.exe` 40,195,620 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **受け入れ条件の照合（gap 分析 B10 のうち PR2 の範囲）**：
  - (e) 分割・再結合の Undo が結果ペインとセッションの両方に反映されること — 満たす（コード＋単体テスト。実機は未検証）。
  - 学習候補の受け入れ／却下の Undo／Redo、Undo しても学習データが変わらないこと — 満たす（コード＋単体テスト `LearnUndoRedoCycleDoesNotRecordLearning`）。
  - セッション保存・復元後の扱いの明記 — 満たす（PR 本文＋マニュアル §17）。
- **未検証（実機でしか確認できないもの）**：分割／再結合の Undo 操作、学習受け入れ／却下の Undo 操作、セッション保存→復元後の分割状態、行を判断し直した後の分割 Undo の拒否、学習 JSON 件数の不変、結論 Undo 時の候補再表示。PR 本文の実機チェックリスト 12 項目どおり。
- **B10 の完了**：PR1（基本 3 種）＋ PR2（分割・再結合、学習候補）で B10 は完了。残る実機確認はチェックリストどおり。

### 75.4 次への移行
- 次は **B11**（監査ログ）に進む。B11 の担当を spawn する。
- ID 割り当て状況（2026-11-27 15:00 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 が 305 を使用、F6 が 306 を使用（307 は `IDR_LOGO`）、F7 が 308 を使用。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 が 1893–1897 を使用（1898–1902 は予備）、F5 が 1903–1904 を使用、F6 が 1905–1909 を使用、F7 が 1910–1912 を使用。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第 2 段階用に 38981 を予約済み、B3 が 38982 を使用、B8 が 38983 を使用、B10 PR1 が 38984–38985 を使用。`_APS_NEXT_RESOURCE_VALUE` 309、`_APS_NEXT_COMMAND_VALUE` 38986、`_APS_NEXT_CONTROL_VALUE` 1913（B10 PR2 マージ時点）。

## 76. B11 — 監査ログ（PR #72）

### 76.1 概要
- B11（監査ログ）：判断の履歴を追記式のログに残す。B8 の検証レポート（出力の正しさの証拠）とは別物（監査ログは判断の履歴）。B10 の Undo 履歴（取り消しのための履歴）とも別物。
- 1 件あたり「日時・ファイル（相対パス）・操作の種類（確認内容変更／出力切替／差分結論／一括チェック／学習候補の受け入れ・却下／分割・再結合）・旧値→新値・操作者 ID（学習の出典 ID `u-<GUID>` を流用）」。旧値／新値は表示文言ではなく安定したコードで保存し、表示時に日本語へ変換。
- **形式：JSON Lines**（`Merge20Audit_<フォルダペアのハッシュ>.json`、セッションと同じ場所・同じハッシュ）。追記式で全体の読み直し・書き直しが不要、1 行 1 件で書き込み途中の破損が最後の 1 行だけに留まる。CSV は表示・出力用に生成。
- フォルダ一覧の右クリックメニューから監査ログの一覧表示と CSV 出力ができる。一覧表示は古い順（時系列）。
- **Undo は記録しない**（Undo は新しい判断ではない。Undo 経路は生の setter を通るためフックにかからず、既存の記録も消えない）。
- **セッションの保存・復元は記録しない**（学習と同じく、復元は判断操作ではないため）。例外は S8 変更検出による TODO への復帰のみ（過去の判断を破棄するため。保存時すでに TODO なら変化なしで記録しない）。
- 一括チェックはファイルごとに 1 行（kind BatchCheck）。追記失敗でも判断は成功する。
- **S8 のセッション形式には触れていない**。学習 JSON とも別ファイルで、ダイアログ内に別物である旨を明記。マニュアル §18 を追加（§14・§15 から相互参照）。

### 76.2 変更内容（PR #72、29 コミット、22 ファイル、+2213/−10）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20Audit.h/.cpp` | 監査ログの純粋モジュール（エントリ・JSON Lines 追記／読み込み・CSV 生成） |
| `Src/Merge20/Merge20AuditDoc.h/.cpp` | MFC アダプタ（保存場所の決定・追記・一覧・CSV 出力） |
| `Src/Merge20/Merge20AuditDlg.h/.cpp` | 一覧表示ダイアログ（IDD 309、古い順に表示） |
| `Src/DirDoc.cpp` | 確認内容・出力切替のログ記録（転送メソッド `ApplyMerge20ReviewState`／`SetMerge20IncludeInOutput` へ移設。下記参照） |
| `Src/DirView.h/.cpp` | 右クリックメニュー「監査ログ…」「監査ログを CSV 出力…」 |
| `Src/Merge20/Merge20FileListBar.cpp` | パネル経路の記録 |
| `Src/Merge20/Merge20SessionDoc.h/.cpp` | S8 変更検出による TODO 復帰の記録 |
| `Src/MergeDocResultPane.cpp` | 差分結論・学習候補・分割・再結合の記録 |
| `Docs/Users/WizMergeManual.html` | §18 を追加（§14・§15 から相互参照） |
| `Src/Merge.rc` | `IDD_MERGE20_AUDIT` 追加 |
| `Src/resource.h` | ダイアログ 309、コントロール 1913–1916、コマンド 38986–38987。`_APS_NEXT_*` は 311／38988／1917 へ（310 は `IDR_MARGIN_ICONS_PNG` 使用中のため 309→311） |
| `Translations/WinMerge/Japanese.po` | 新規 msgid 29 件（メニューとダイアログ題名には接頭辞） |
| `Src/Merge.vcxproj`／`.filters`、`Testing/GoogleTest/UnitTests/UnitTests.vcxproj`／`.filters` | 登録 |
| `Testing/GoogleTest/Merge20/Merge20Audit_test.cpp` | 単体テスト 12 件 |

- **CI 失敗と修正（2 回）**：
1. 実行 46031437993：92 エラー。`Merge20Audit.h` が `Merge20DiffConclusion`（`Merge20Conclusion.h`）の include を欠いていた。`Merge20Undo.h` と同じ include を追加して修正。
2. 実行 46032749889：LNK2019 が 2 件。`CDiffContext` は GoogleTest にもコンパイルされるが、テスト側には MFC アダプタ（`Merge20AuditDoc.cpp` 等）がないため、アダプタを呼ぶフックはリンクできない。修正として、確認内容・出力切替のログを `CDirDoc` の転送メソッド（`ApplyMerge20ReviewState`／`SetMerge20IncludeInOutput`）へ移した。すべてのユーザー操作（フォルダ一覧キー、出力列クリック、パネル、専用バー／フォーカスドックの `PathContext` 版）は `CDirDoc` を通り `bRecordUndo=true` になるため、記録範囲は変わらない。バッチ（false）、結論連動（コンテキスト直接・false）、Undo（生の setter）、セッション復元（`SetMerge20SessionState` 直接）はいずれも `CDirDoc` を通らず、記録されないことを確認済み。
- **教訓：`DiffContext.cpp` からは純粋モジュールの関数しか呼べない。** MFC アダプタを呼ぶフックは `CDirDoc` 側に置くこと。F2 の教訓（MFC には既定 PCH）と合わせて、純粋モジュールと MFC アダプタの境界を意識すること。

### 76.3 検証レポート（コーディネーター検証済み）
- **PR #72**：head `156688cb`、base master `33616888`（B10 PR2 マージ＋NOTES §75）。29 コミット、変更ファイル 22 件（B11 の 22 ファイルのみ）。
- **CI（ブランチ head）**：実行 46035353752、completed／success。全文ログ（3,621,206 バイト）を担当が取得・確認（`~/workspace/.coord/b11/ci-46035353752-full.log`）。コーディネーターも同ログの要所を確認：`Build succeeded`、`0 Error(s)`、GoogleTest `[ PASSED] 1015 tests.`（1003＋新規 12 件）、`[ FAILED]` 0 件。`.po` 番兵通過（`<<<<<<<` 2 件は既存のヘルプ文言内の説明文で、混入ではないことを確認済み）。成果物：`WinMerge-x64.zip` 26,788,890 バイト、`WinMerge-x64-Setup-x64.exe` 40,220,808 バイト、expire 2027-02-25。
- **マージ**：マージコミット `0bb25526`。マージコミット CI：実行 46037613631、completed／success。全文ログ（3,619,591 バイト、ジョブ `build` 140636897206）をコーディネーターが取得・確認（`~/workspace/.coord/b11/b11_merge_log.txt`）。GoogleTest `[ PASSED ] 1015 tests.`、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`、`Merge20 verify: 100 files x 500 lines, 90 ms total, 0 issues`。
- **成果物（実行 46037613631）**：`WinMerge-x64.zip` 26,788,781 バイト、`WinMerge-x64-Setup-x64.exe` 40,235,524 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **受け入れ条件の照合（gap 分析 B11）**：
- (a) 判断操作のたびにログが 1 行増えること — フックは Undo の記録箇所と同じ操作の発生箇所。テスト `AppendAndLoadKeepsOrder` で確認。**実機での行数確認は未検証**。
- (b) ログに日時・旧値→新値・操作者 ID が含まれること — エントリ必須項目。テストで往復確認済み。
- (c) ログの一覧表示で時系列に辿れること — ダイアログは古い順に表示。**実機未検証**。
- (d) CSV 出力が Excel で読めること — バイト列はテスト `CsvFileStartsWithBom`・`CsvTextIsCrlfJoinedAndQuoted` で確認。**Excel での目視は実機未検証**。
- (e) セッションの保存・復元が監査ログに記録されないこと — 復元は `ctxt.SetMerge20SessionState` を直接呼ぶためフックを通らない。**実機未検証**（復元後の行数の不変）。
- **未検証（実機でしか確認できないもの）**：一覧表示の操作、判断ごとの行数増加、セッション保存・復元での行数不変、ファイル変更時の TODO 復帰の記録、Excel での目視、日本語表示、Undo で行数が増えない・消えないこと。

### 76.4 次への移行
- 次は **B12**（再開位置の完全復元）に進む。B12 の担当を spawn する。
- ID 割り当て状況（2026-11-27 16:55 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 が 305 を使用、F6 が 306 を使用（307 は `IDR_LOGO`）、F7 が 308 を使用、B11 が 309 を使用（310 は `IDR_MARGIN_ICONS_PNG`）。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 が 1893–1897 を使用（1898–1902 は予備）、F5 が 1903–1904 を使用、F6 が 1905–1909 を使用、F7 が 1910–1912 を使用、B11 が 1913–1916 を使用。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第 2 段階用に 38981 を予約済み、B3 が 38982 を使用、B8 が 38983 を使用、B10 PR1 が 38984–38985 を使用、B11 が 38986–38987 を使用。`_APS_NEXT_RESOURCE_VALUE` 311、`_APS_NEXT_COMMAND_VALUE` 38988、`_APS_NEXT_CONTROL_VALUE` 1917（B11 マージ時点）。

## 77. B12 — 再開位置の完全復元（PR #73）

### 77.1 概要
- B12（再開位置の完全復元）：セッションに再開位置を追加し、復元時に同じファイル比較・同じ差分・同じスクロール位置に戻す。NOTES §53.4 の専用モードのピンの穴を塞ぐ。パネルの検索文字列を変更直後に保存する。
- (a) 再開位置：`Merge20SessionDisplay` に `resumeRelPath`／`resumeDiff`／`resumeTopLine` を追加（D11 パターンの後方互換パース）。保存時に `CDirDoc::Merge20GetLastOpenMergeDoc()`（アクティブなフレームのファイル比較、なければ最後に開いたもの）から相対パス・現在差分・先頭行（`GetTopLine()`）を記録。復元時に `Merge20SessionResumeLastFileCompare()` で開き直し、`SelectDiff`→`ScrollToSubLine(GetSubLineIndex(...))` で位置を戻す（live の件数でクランプ）。呼び出しは復元提案・手動復元・遅延復元の 3 経路すべて。変更されて TODO に戻ったファイルは再開しない（差分番号・行番号がずれるため）。「選択ファイル」は既存の `focusedPath`／`topIndex` で満たすため追加なし。
- (b) ピンの穴：`CMergeEditFrame` に専用モードのフレーム数カウンタを追加（`SetDedicatedMode` が唯一の入口、専用モードのまま破棄される場合はデストラクタで帳尻合わせ）。復元時に専用モードのフレームが開いている間は `OPT_MERGE20_DED_PINNED` を書き戻さない。**NOTES §53.4 の穴はこれで塞がれた。**
- (c) 検索文字列の即時保存：`OnSearchChanged()` で `OPT_MERGE20_FILELIST_SEARCH` をその場で保存（タブ・折りたたみ・スリムと同じ扱い）。`SavePanelState()` でも引き続き保存（二重保存）。
- (d) 自動スリム化の決定：**実装しない**。P0 の決定（専用モードはパネルに触れない）が優先し、入場前の状態の記憶と退場時の復元が必要になるため。専用モード側の着手時に決める（PR 本文と本節に記録。NOTES §42.6 の記載のまま）。
- **復元提案ダイアログにチェックボックスは置かない**。代わりに提案文に「前回開いていたファイル比較「%1」も開きます。」の 1 行を追加し、開いてほしくない場合は復元全体を断れるようにした（新規 msgid 1 件、日本語訳付き。マニュアル §14 にも再開位置の 1 行を追加）。

### 77.2 変更内容（PR #73、14 コミット、14 ファイル、+354/−18）
| ファイル | 内容 |
|---|---|
| `Src/Merge20/Merge20Session.h/.cpp` | `Merge20SessionDisplay` に `resumeRelPath`／`resumeDiff`／`resumeTopLine` を追加（後方互換パース） |
| `Src/Merge20/Merge20SessionDoc.h/.cpp` | 再開位置の保存・復元（`Merge20SessionResumeLastFileCompare`、復元レポート表示の「後」に呼ぶ。開いたファイル比較がレポートを隠さないため。遅延復元では pending 適用後の `OnUpdateUIMessage` から呼ぶ） |
| `Src/DirDoc.h/.cpp` | `Merge20GetLastOpenMergeDoc()` |
| `Src/DirView.h/.cpp` | 復元提案文に再開ファイルの 1 行を追加 |
| `Src/MergeEditFrm.h/.cpp` | 専用モードのフレーム数カウンタ（`SetDedicatedMode` が唯一の入口） |
| `Src/Merge20/Merge20FileListBar.cpp` | 検索文字列の変更直後の保存 |
| `Docs/Users/WizMergeManual.html` | §14 に再開位置の 1 行を追加 |
| `Testing/GoogleTest/Merge20/Merge20Session_test.cpp` | 単体テスト 1 件（`Merge20Session.ResumeFieldsBackwardCompat`） |
| `Translations/WinMerge/Japanese.po` | 新規 msgid 1 件（日本語訳付き） |

- **ID 割り当て**：新規コマンド ID・ダイアログ ID・コントロール ID はなし（チェックボックス・ショートカットを追加していないため）。`_APS_NEXT_*` は 311／38988／1917 のまま。
- **注意点**：専用モードのカウンタは `SetDedicatedMode` 経由のみで増減する（`EnterDedicatedMode` の直接呼び出しは `SetDedicatedMode` 内のみ、`ExitDedicatedMode` の直接呼び出しは `DestroyWindow` 内のみでデストラクタが補正する）。将来、専用モードの入場／退場の入口を増やす場合はカウンタの網羅性を再確認すること。
- `.po` の `<<<<<<<` ヒット 2 件は master にもともとある未解決コンフリクト説明文中の正規文言で、混入ではないことを確認済み。

### 77.3 検証レポート（コーディネーター検証済み）
- **PR #73**：head `fc2c82ba`、base master `c9d4a360`（B11 マージ＋NOTES §76）。14 コミット、変更ファイル 14 件（B12 の 14 ファイルのみ）。1 PR に収まったため分割なし。
- **CI（ブランチ head）**：実行 46043923529、completed／success。全文ログ（3,621,000 バイト）を担当が取得・確認（`~/workspace/.coord/b12/ci-46043923529-full.log`）。コーディネーターも同ログの要所を確認：`Build succeeded`、`0 Error(s)`、GoogleTest `[ PASSED ] 1016 tests.`（1015＋新規 1 件）、`[ FAILED ]` 0 件。`.po` 番兵通過。成果物：`WinMerge-x64.zip` 26,791,774 バイト、`WinMerge-x64-Setup-x64.exe` 40,219,996 バイト、expire 2027-02-25。
- **マージ**：マージコミット `38c012cc`。マージコミット CI：実行 46045789482、completed／success。全文ログ（3,620,255 バイト、ジョブ `build` 140664600000）をコーディネーターが取得・確認（`~/workspace/.coord/b12/b12_merge_log.txt`）。GoogleTest `[ PASSED ] 1016 tests.`、`[ FAILED ]` 0 件、`Build succeeded`、`0 Error(s)`、`Merge20 verify: 100 files x 500 lines, 72 ms total, 0 issues`。
- **成果物（実行 46045789482）**：`WinMerge-x64.zip` 26,791,616 バイト、`WinMerge-x64-Setup-x64.exe` 40,220,258 バイト。いずれも未失効（expire 2027-02-25）。※成果物の SHA256 は API では取得できないため未確認。
- **受け入れ条件の照合（gap 分析 B12）**：
  - (a) 閉じる前に開いていたファイル比較が、再開時に同じ差分・同じスクロール位置で開くこと — コード＋単体テストで固定。**実機未検証**。
  - (b) 専用モードを開いたままセッションを復元しても、専用バーが隠れないこと — コードで固定。**実機未検証**。
  - (c) パネルの検索文字列を入れて即座に閉じても、再開時に残っていること — コードで固定。**実機未検証**。
  - (d) (a) の復元に失敗したときはフォルダ一覧だけが復元され、エラーで止まらないこと — 開けない場合（フィルタで隠れている、オープン失敗、項目なし、変更されて再開対象外）は無言でフォルダ一覧だけが残る。**実機未検証**。
  - S8 の既存の受け入れ条件（保存時と異なるフォルダ組で復元しようとすると断られること）は変更なし。**実機未検証**。
- **未検証（実機でしか確認できないもの）**：5 個目の差分での再開、専用モードを開いたままの復元、検索文字列の即座の保存、異なるフォルダ組での復元拒否、変更された再開対象ファイルでのフォルダ一覧のみの復元、ファイル比較を開いていない状態での復元、提案メッセージの行と「いいえ」での無変更。PR 本文の実機チェックリスト 7 項目どおり。
- **B13 への引き継ぎ**：B13（再開サマリ表示）は本 PR の提案文の 1 行（ファイル名のみ）を土台にできる。件数サマリは未実装のまま。

### 77.4 次への移行
- 次は **B13**（再開サマリ表示）に進む。B13 の担当を spawn する。
- ID 割り当て状況（2026-11-27 17:55 JST 時点）：IDD は B9 が 301、B2 が 303（302 は `IDI_ROTATE2`）、B14 が 304、B7 PR2 が 305 を使用、F6 が 306 を使用（307 は `IDR_LOGO`）、F7 が 308 を使用、B11 が 309 を使用（310 は `IDR_MARGIN_ICONS_PNG`）。コントロール ID は F3 が 1858–1865、B9 が 1866–1871、F4 が 1872–1881、B2 が 1882–1885、B4 が 1886、B3 が 1887–1888、B14 が 1889–1892、B7 PR2 が 1893–1897 を使用（1898–1902 は予備）、F5 が 1903–1904 を使用、F6 が 1905–1909 を使用、F7 が 1910–1912 を使用、B11 が 1913–1916 を使用。コマンド ID は B9 が 38979、F4 が 38980（予約済み・未使用）、B2 第 2 段階用に 38981 を予約済み、B3 が 38982 を使用、B8 が 38983 を使用、B10 PR1 が 38984–38985 を使用、B11 が 38986–38987 を使用。`_APS_NEXT_RESOURCE_VALUE` 311、`_APS_NEXT_COMMAND_VALUE` 38988、`_APS_NEXT_CONTROL_VALUE` 1917（B12 マージ時点）。
