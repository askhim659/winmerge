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

## 12. Phase 2 S4（対応区分・リスク・ルールベース要約）— ブランチ `merge20/phase2-s4`

（注：§10（S2）・§11（S3）はこのファイルには未記録。進捗は
`merge20-complete-plan.md` の進捗ログ参照。）

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
