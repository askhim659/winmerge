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

設計書 `merge20-phase2-design.md` §7 の S2 に従う（§3.2 のうち連動ルール・
キー操作・出力チェック列・行色）。C++ の変更はこの Linux VM では
コンパイルできないため、CI でのビルド＋成果物確認が検証になる。

### 10.1 変更内容

- `Src/Merge20/Merge20FileStates.h`／`.cpp`：`HasState`（エントリの有無）、
  `GetIncludeInOutput`／`SetIncludeInOutput`（出力チェック旗）を追加。
- `Src/DiffContext.h`／`.cpp`：`GetMerge20IncludeInOutput(const DIFFITEM&)`／
  `SetMerge20IncludeInOutput(const DIFFITEM&, bool)`、
  `SetMerge20ReviewState(const DIFFITEM&, ReviewState)`（初回エントリ作成時に
  出力チェック初期値を存在区分から決めて記録する）、
  `ApplyMerge20ReviewState(const DIFFITEM&, ReviewState)`（連動ルール付き設定）
  を追加。初期値は設計書 §3.2 どおり：変更・旧のみ＝ON、新のみ・同一＝OFF。
  連動ルールは HTML の `setRev`／`cycRev` と同一：DONE→ON（ただし 2-way の
  新のみファイルは ON にしない）、HOLD／KEEP→OFF、TODO／SPECIAL→変更なし。
- `Src/DirDoc.h`／`.cpp`：上記のフォワーダ
  （`Get/SetMerge20IncludeInOutput`、`ApplyMerge20ReviewState`）を追加。
  `SetMerge20ReviewState` は DIFFITEM 版を使うように変更（初期値の記録のため）。
- `Src/DirViewColItems.cpp`：新列「出力」（regName `Soutput`、英語
  `Include in Output`）。表示は ☑（U+2611）／☐（U+2610）の文字で、
  フォルダ行は空欄。ソートは未チェック＜チェック。`f_cols`（既定表示順 7、
  確認内容列の次）と `f_cols3`（既定表示順 8）の両方に追加し、表の末尾に配置
  （S1 と同じ理由：`CompareFilesIfFilesAreLarge` のハードコード列番号）。
  `IsColOutputCheck` ヘルパーを `DirViewColItems.h`／`.cpp` に追加。
- `Src/DirView.cpp`：(1) `PreTranslateMessage` にキー 1〜5（未確認／済／保留／
  個別対応要／現状維持）と Space 順送り（TODO→DONE→HOLD→SPECIAL→KEEP→TODO）
  を追加。フォルダ比較一覧内のみで有効（`!IsLabelEdit()` かつフォーカス行あり）。
  修飾子付き（Ctrl／Alt／Shift）は除外し、加速子との競合を避ける。エディタ内では
  有効にしない（設計書 §1.3 #3）。(2) `OnClick` に出力列セルのクリック切替を
  追加（`SubItemHitTest` で列を判定し、`IsColOutputCheck` の列だけ切替える。
  フォルダ行と「..」行は対象外。仮想リストのため行の再描画で反映）。
  (3) `GetColors` に確認内容ごとの行色を追加：SPECIAL＝薄赤背景
  RGB(255,200,200)、HOLD＝薄黄背景 RGB(255,255,180)、KEEP＝灰背景
  RGB(230,230,230)、TODO／DONE＝従来色。背景だけを変え、差分状態の文字色は
  そのまま残す（両立）。固定色で、テーマ対応は後回し（設計書 §3.2）。
- `Translations/WinMerge/Japanese.po`：`Include in Output`→`出力`、列説明文の訳
  を追加（S0・S1 と同じ msgid 参照の方針）。
- 新規 `.cpp` ファイルはないため `Merge.vcxproj`／`UnitTests.vcxproj` の変更は
  不要（S1 の LNK2019 の教訓に該当しない）。

### 10.2 設計からの変更点（重要）：出力チェック列は文字表示＋クリック切替

- 設計書 §3.2 の推奨案 (a)（ステートイメージリストによる自前チェック描画）は
  採用しなかった。実コードで確認したところ、リストビュー項目のステートイメージ
  は 1 行に 1 つで第 1 列にしか表示されず、しかもツリーモードでは既に展開／折畳み
  アイコン（state image 1／2）に使われている（`DirView.cpp: ReflectGetdispinfo`）。
  したがってサブ項目列にステートイメージでチェックボックスを描くことはできない。
- 代わりに設計書のフォールバック案 (b)（☑／☐ の文字表示）を採用し、クリックでの
  手動 ON／OFF 切替も実装した（`CDirView::OnClick`）。値は他の列と同じく
  `LVN_GETDISPINFO`（`ReflectGetdispinfo` → `ColGetTextToDisplay`）で動的供給
  されるため、仮想リスト構造（T-2-4）との整合性は保たれている。
- 設計書とのその他の食い違いはない。確認内容キー 1〜5・Space の有効範囲は
  設計書 §1.3 #3 の結論どおりフォルダ比較一覧のみ（専用モードは Phase 4）。

### 10.3 API でのコミット時の注意（S2 で発生）

- `commit_file.py` を短い間隔で連続実行すると、2 件目が 422
  "Update is not a fast forward" で失敗することがある（1 件目の ref 更新直後に
  GET した head が古い値を返す。GitHub 側の結果整合性の遅延とみられる）。
  失敗したコミット自体は作成済みだが ref が進んでいないため、数秒待って同じ
  `commit_file.py` を再実行すればよい（同じ内容の blob が再利用され、新しい
  commit が現在の head の子として作られる）。孤立したコミットは無害。
  以後の教訓：コミット間に数秒の間隔を置くこと。

### 10.4 S2 で未実施のこと（S3 以降）

- 一括チェック・件数表示（S3）、対応区分・リスク・要約（S4）、一括承認（S5）、
  CSV（S6）、ZIP（S7）、セッション保存（S8）には着手していない。
- U-0-2（2-way 自動マージの挙動確認）は Windows 実機での確認が必要なため、
  このスライスでは扱っていない（S5 着手前）。

### 10.5 Windows 実機での確認手順（S2 受け入れテスト）

1. CI 成果物 ZIP の `WinMergeU.exe` で 2 フォルダ比較を開く（変更・旧のみ・
   新のみ・同一の 4 種を含むこと）。
2. 一覧に「出力」列が表示され、初期値が「変更・旧のみ＝☑、新のみ・同一＝☐」
   であること。フォルダ行は空欄であること。
3. フォーカス行でキー 2（済）を押すと確認内容が「✓済」になり、出力が ☑ に
   なること。ただし新のみファイルでキー 2 を押しても出力は ☐ のままであること。
4. キー 3（保留）／キー 5（現状維持）を押すと出力が ☐ になること。
   キー 4（個別対応要）／キー 1（未確認）では出力が変わらないこと。
5. Space を繰り返し押すと確認内容が未確認→済→保留→個別対応要→現状維持→
   未確認と順送りされ、連動ルールが 3・4 と同じになること。
6. 出力列のセルをクリックすると ☑／☐ が手動で切り替わること。確認内容は
   変わらないこと。フォルダ行の出力セルをクリックしても何も変わらないこと。
7. 行色：個別対応要の行が薄赤、保留の行が薄黄、現状維持の行が灰色の背景に
   なること。済・未確認の行は従来の差分色のままであること。
8. 列ヘッダの「出力」でソートできること。列選択ダイアログで「出力」列を
   非表示／再表示できること。3 フォルダ比較でも同じ列が出ること。
9. 再スキャン（F5）後も確認内容と出力チェックが変わらないこと。
10. ファイル比較エディタを開き、数字キー 1〜5 と Space を押すと文字が入力
    されること（確認内容が変わらないこと）。

### 10.6 CI 結果と修正（PR #7）

- PR #7（`merge20/phase2-s2`、12 コミット＋修正 2 コミット）。CI 実行 45600362144 は
  failure：`Src/DirView.cpp(1428,13): error C2039: 'isDirectory': is not a member
  of 'DIFFITEM'`。原因は `isDirectory()` が `DIFFITEM` ではなく `DIFFCODE` の
  メンバであることの見誤り（`DiffItem.h:88` の `isDirectory()` は `struct DIFFCODE`
  内の定義。既存コードは `di.diffcode.isDirectory()` と書いている）。
  同じ誤りが `DirViewColItems.cpp` の出力列 getter／sorter にもあったため、
  3 箇所すべてを `di.diffcode.isDirectory()`（sorter は `ldi`／`rdi`）に修正。
  以後の教訓：`DIFFITEM` の述語は `diffcode.` 経由（`isSideFirstOnly`／
  `isResultSame` 等も同じ）。`DiffItem.h` の grep 結果は struct 名を確認すること。
- 修正コミットごとに CI が走り、1 件目の修正だけを含む実行 45602942908 は
  failure（DirView.cpp の修正がまだ入っていないため当然）、2 件とも含む実行
  45602987787 は success。成果物 2 件（いずれも 2027-02-23 失効）：
  `WinMerge-x64.zip`（26,039,876 バイト、ID 15016932460）、
  `WinMerge-x64-Setup-x64.exe`（39,517,186 バイト、ID 15017521005）。

## 11. Phase 2 S3（一括チェック＋件数表示）— ブランチ `merge20/phase2-s3`

設計書 `merge20-phase2-design.md` §7 の S3 に従う（§3.2 のうち一括チェック・
ディレクトリ単位の全チェック／全解除・件数表示）。C++ の変更はこの Linux VM では
コンパイルできないため、CI でのビルド＋成果物確認が検証になる。

### 11.1 変更内容

- `Src/Merge20/Merge20BatchCheckDlg.h`／`.cpp`（新規）：一括チェックダイアログ
  （`CTrDialog` 派生）。確認内容 5 状態のチェックボックス（HTML v17.5 の
  ポップアップと同じ複数選択）、対象件数の表示（OK ボタンのラベル自体が
  「選択分を一括チェック（N件）」になり、件数行にも「N件のファイルが対象です」と
  出る）、「表示分すべてチェック」「全解除」ボタンを同じダイアログに置く。
  何も選択していない間は OK が無効（空実行できない）。キャンセルでは何も変わらない。
  戻り値は `Action`（`Selected`／`CheckAllDisplayed`／`ClearAll`／`None`）。
- `Src/resource.h`：`IDD_MERGE20_BATCH_CHECK`（292）、
  `IDC_MERGE20_BATCH_TODO`〜`KEEP`（1719〜1723、連続値で `ON_CONTROL_RANGE` 用）、
  `IDC_MERGE20_BATCH_COUNT`（1724）、`IDC_MERGE20_BATCH_CHECK_ALL`（1725）、
  `IDC_MERGE20_BATCH_CLEAR_ALL`（1726）、コマンド `ID_MERGE20_BATCH_CHECK`（38931）、
  `ID_MERGE20_DIR_CHECK_ALL`（38932）、`ID_MERGE20_DIR_UNCHECK_ALL`（38933）を追加。
  `_APS_NEXT_*` も更新（resource 293、command 38934、control 1727）。
- `Src/Merge.rc`：`IDD_MERGE20_BATCH_CHECK` のダイアログテンプレートを追加
  （300×148、チェックボックス 5 個＋件数行＋ボタン 4 個。英語リテラルで、
  実行時に `CTrDialog::OnInitDialog` → `I18n::TranslateDialog` で翻訳される）
  と、レイアウト／マージン欄への登録（`AFX_DIALOG_LAYOUT` と `, DIALOG` 欄）。
- `Src/DirView.h`／`.cpp`：
  (1) 右クリックメニュー（`ListContextMenu`）に「一括チェック…」を常時追加し、
  ディレクトリ行が選択されている場合のみ「配下をすべてチェック」「配下の
  チェックをすべて外す」を追加（既存の `bDirSelected` 判定を流用。プラグイン設定
  サブメニューと同じ `AppendMenu` 方式）。コマンドはメッセージマップに登録
  （`ON_COMMAND`／`ON_UPDATE_COMMAND_UI`）。
  (2) `OnMerge20BatchCheck`：表示中のファイルを確認内容別に数えてダイアログを開き、
  承認後のみ実行する。「選択分」は表示中のファイルのうち選択した確認内容のものだけを
  ON にする（HTML の `batchChkSel` はフィルタ後の `filtFiles` を対象にするのと同じ）。
  「表示分すべてチェック」は表示中の全ファイルを ON にする。「全解除」は比較全体の
  全ファイルを OFF にする（HTML の `batchChk(false)` が全ファイルを対象にするのと同じ。
  フィルタで隠れたファイルや折りたたまれたフォルダ配下も含む）。
  (3) ディレクトリ単位の操作（`Merge20SetOutputForDirectory`）：`DIFFITEM` の
  `GetFirstChild()`／`GetFwdSiblingLink()` による再帰走査で、配下の全ファイルの
  出力チェックを入れる／外す。表示一覧ではなくツリーを歩くため、折りたたまれた
  サブフォルダ配下も含まれる。ディレクトリ行自体には出力フラグがない。
  (4) 件数表示：`GetMerge20StatusCountText()`（「表示 N 件中 M 件チェック」。
  分母は表示中のファイル行数、分子はそのうち出力チェック ON の件数。ディレクトリ行と
  「..」行は数えない）と `UpdateMerge20StatusCount()` を追加。`Redisplay()` の末尾
  （表示行の再構築直後）、出力列のクリック切替（`OnClick`）、キー 1〜5・Space
  （`PreTranslateMessage`）、一括チェック／ディレクトリ単位操作の直後に呼ぶ。
  (5) `Merge20SetOutputForAllFiles`：`CDiffContext`（`DiffItemList`）の
  `GetFirstDiffPosition()`／`GetNextDiffPosition()` による深さ優先走査で比較全体の
  全ファイルを対象にする（「全解除」用）。
- `Src/Merge.vcxproj`／`Src/Merge.vcxproj.filters`：新規ダイアログ 2 ファイルを
  `Merge20` フィルタに登録（`.cpp` は `DiffContext.cpp` と同じ PCH 設定）。
  `UnitTests.vcxproj` への登録は不要（ダイアログは `DirView.cpp` からだけ使われ、
  UnitTests は `DirView.cpp` をコンパイルしない。S1 の LNK2019 の教訓に該当しない）。
- `Translations/WinMerge/Japanese.po`：新規文字列 11 件の訳を追加（ダイアログの
  タイトル・説明・ボタン、右クリックメニュー 3 件、ステータスバー件数表示。
  S0〜S2 と同じ msgid 参照の方針。チェックボックスの 5 状態名は S1 の訳を流用）。

### 11.2 T-2-2 の解消：件数表示のステータスバー更新経路

- 設計書 §5 の T-2-2（表示場所の特定）は解消した。フォルダ比較のステータスバーは
  `CDirFrame::m_wndStatusBar`（`CStatusBar`。設計書の推測にあった
  `BasicFlatStatusBar` ではない）。ペイン 0 は伸縮するメッセージペインで、
  書き込み口は `CDirFrame::SetStatus(const tchar_t*)`（`DirFrame.cpp:151`、
  `m_wndStatusBar.SetPaneText(0, szStatus)`）だけである。
- ペイン 0 の唯一の書き手は `CDirView::OnTimer(TIMER_ID_STATUSBAR_UPDATE)`
  （比較完了後に 150ms、選択変更時に 100ms のワンショットタイマ。従来は
  「経過時間」または「N items selected」を表示していた）。選択が変わるたびに
  件数表示が上書きで消えてしまうため、`OnTimer` のメッセージに件数テキストを
  「`選択表示 | 表示 N 件中 M 件チェック`」の形で追加し、かつ `Redisplay()` と
  すべての Merge20 状態変更直後に `UpdateMerge20StatusCount()` で直接書き込む
  二重の経路にした。これで件数表示はフィルタ・チェック操作に追従する。
- HTML の `cntLbl`（`表示:N / 選択:M`）との違い：仕様書 §3（3/4）の文言どおり
  分母＝表示中の件数、分子＝そのうちチェック済みの件数とした（HTML の分子は
  全ファイル中のチェック済み件数だったが、仕様書を優先した）。

### 11.3 設計からの変更点・判断事項

- 一括チェックの UI は、設計書 §3.2 の「右クリックメニュー体系に倣うポップアップ」
  をダイアログとして実装した。メニュー方式では、複数選択のたびにメニューが閉じて
  開き直しになる（標準メニューは項目選択で閉じる）ため、HTML のポップアップ
  （チェックボックスを複数付けたまま件数を確認して実行）と等価にならない。
  ダイアログは既存の `ArchiveDlg`／`ComparisonResultFilterDlg` と同じ作り
  （`CTrDialog`＋`Merge.rc` テンプレート＋`resource.h` の ID）で、件数の事前表示と
  確認後の実行（OK／キャンセル）をそのまま満たす。右クリックメニューからの起動は
  設計書の「既存の右クリックメニュー体系に倣う」を満たす。
- 「全解除」の範囲は比較全体（HTML の `batchChk(false)` と同じ）とし、
  「表示分すべてチェック」「選択分を一括チェック」の範囲は表示中のファイルとした。
  仕様書 §4(b)・§3（3/4）は範囲を明記していないため、HTML の挙動を正本とした。
  この違いはダイアログの説明文と本記録に明記してある。
- 件数表示の分母は「表示中のファイル行数」とした（ディレクトリ行は出力フラグを
  持たないため数えない）。フィルタで隠れたファイルは分母・分子のどちらにも入らない。

### 11.4 S3 で未実施のこと（S4 以降）

- 対応区分・リスク・要約（S4）、一括承認（S5）、CSV（S6）、ZIP（S7）、
  セッション保存（S8）には着手していない。
- U-0-2（2-way 自動マージの挙動確認）は Windows 実機での確認が必要なため、
  このスライスでは扱っていない（S5 着手前）。

### 11.5 Windows 実機での確認手順（S3 受け入れテスト）

1. CI 成果物 ZIP の `WinMergeU.exe` で 2 フォルダ比較を開く（100 ファイル規模、
   サブフォルダを含むこと。確認内容の 5 状態が混ざるようにキー 1〜5 で事前に
   付けておく）。
2. フォルダ比較ウィンドウ下部のステータスバーに「表示 N 件中 M 件チェック」が
   常時表示されること。N は一覧のファイル行数と一致し、M は ☑ の数と一致すること。
   （選択行数を変えても件数表示が消えないこと。「経過時間」表示の直後にも
   件数表示が付いていること。）
3. 出力列のセルをクリックして ☑／☐ を切り替えると、ステータスバーの M が
   すぐに変わること。キー 2（済）／3（保留）／5（現状維持）でも M が連動して
   変わること。
4. 比較結果フィルタ（例：「異なるファイルのみ」）や表示フィルタで表示件数を絞ると、
   N が絞り込み後の件数になり、M も表示中のチェック済み件数になること。
5. 行を右クリックし「一括チェック…」を選ぶ。ダイアログで確認内容を複数
   （例：済＋個別対応要）にチェックすると、OK ボタンのラベルが
   「選択分を一括チェック（N件）」になり、件数行にも同じ N 件が表示されること。
   何も選択していない間は OK が押せないこと。キャンセルでは何も変わらないこと。
   OK を押すと、該当するファイルだけが ☑ になり、他は変わらないこと。
   ステータスバーの M が増えていること。
6. 同じダイアログで「表示分すべてチェック」を押すと、表示中の全ファイルが ☑ に
   なること。「全解除」を押すと、フィルタで隠れているファイルも含め全ファイルが
   ☐ になること（隠れたファイルはフィルタを外して確認する）。
7. ディレクトリ行を右クリックすると「配下をすべてチェック」「配下のチェックを
   すべて外す」が出ること（ファイル行だけの選択では出ないこと）。折りたたんだ
   サブフォルダの配下ファイルも含めて ☑／☐ が変わること（フォルダを展開して確認する）。
8. 再スキャン（F5）後も件数表示と出力チェックが変わらないこと。
   3 フォルダ比較でも同じ操作ができること。

### 11.6 CI 結果と修正（PR #8）

- PR #8（`merge20/phase2-s3`、10 コミット＋修正 2 コミット）。CI 実行 45609234086 は
  failure：`Src/Merge20/Merge20BatchCheckDlg.cpp(92,1): error C1010: unexpected end
  of file while looking for precompiled header`。原因は、ダイアログの `.cpp` が
  `#include "StdAfx.h"` なのに `Merge.vcxproj` への登録を `Merge20FileStates.cpp` と
  同じ `pch.h` グループ（`WinMergeU2.pch`）で書いたこと。`pch.h` は STL のみで MFC を
  含まないため、MFC を使うダイアログは `StdAfx.h` グループでコンパイルしなければならない。
  以後の教訓：新規 `.cpp` の PCH グループは、使うヘッダで決める。MFC（`CTrDialog` 等）
  を使うものは `ArchiveDlg.cpp` と同じ素のエントリ（既定の `StdAfx.h` グループ）にし、
  `pch.h` グループは MFC を使わないものだけにする。`Merge20FileStates.cpp` が
  `pch.h` なのは MFC を使わないからである。
- 修正後の実行 45611123171 は failure：`CVTRES : fatal error CVT1100: duplicate
  resource. type:DIALOG, name:292`。原因は、ダイアログ ID に 292 を使ったが 292 は
  すでに `IDD_ARCHIVE` だったこと。`resource.h` の IDD 一覧は値順に並んでおらず、
  末尾だけ見て「最大値 291」と判断したのが誤りだった（`IDD_ARCHIVE` 292 は一覧の
  途中にあった）。ID 293 に変更して修正。以後の教訓：リソース ID の空き確認は
  一覧の末尾ではなく全 define を値でソートして確認すること（IDD／IDC／コマンド ID
  のすべて）。IDC 1719〜1726 とコマンド 38931〜38933 は全 define 対象で空きを確認済み。
- 3 回目の実行 45613020758 は success。成果物 2 件（いずれも 2027-02-23 失効）：
  `WinMerge-x64.zip`（26,047,171 バイト、ID 15022205819）、
  `WinMerge-x64-Setup-x64.exe`（39,530,112 バイト、ID 15022012096）。
