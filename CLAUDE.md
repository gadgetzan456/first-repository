# このリポジトリについて

このリポジトリには独立した2つの静的HTMLゲームが入っている。どちらもビルド不要、外部JSライブラリなし(Google FontsのCSSのみ読み込み)、単一ファイル完結。

- `index.html` — 「組子めいろ」。指でなぞって解く格子迷路パズル。
- `shiren-like.html` — 「迷宮五十階」。不思議のダンジョン系(シレン風)のターン制ローグライク。**このセッションでの作業対象はほぼ常にこちら。**

`README.md`は人間(GitHub閲覧者)向けの説明で、このファイル(`CLAUDE.md`)がClaude向けの作業メモ。READMEの文面を直接読み込みに使う必要はないが、ユーザー向けの仕様説明として機能・挙動と食い違いがないよう、ゲームの挙動を変えたときはREADMEも一緒に更新すること(内部リファクタのみで挙動が変わらない場合は更新不要)。

## shiren-like.html の構造

1つの`<script>(function(){ "use strict"; ... })();`の中に全部入っている(グローバル汚染なし)。主なセクション(上から順):

- `ITEM_TYPES` / `ENEMY_TYPES` / `BOSS_TABLE` / `SPELL_TYPES`(妖術師の呪文)などのデータテーブル
- ダンジョン生成(`populateFloor` / `populateMonsterHouse` / 部屋・通路生成)
- インベントリ操作(`addItemToInventory` / `useInventorySlot` / `equipInventorySlot` / `dropInventorySlot`)
- 戦闘(`playerAttackEnemy` / `enemyAttackPlayer` / `applyEnemyDamage` / `computeDamage`)
- 杖・呪文(`castWand` / `applyWandEffect` / `applySpellToPlayer`)
- ターン進行(`doTurn` — 空腹/毒/自然回復/状態異常のカウントダウンなどをここで一括処理)
- 描画(`renderAll` / `renderStage` / `renderHud` / `renderEquip` / `renderInventory`)
- 入力(`keydown`/`keyup`ハンドラ、持ち物簡易選択メニューの開閉)
- セーブ/ロード(`localStorage`、`saveGame` / `loadSavedGame`)

## ゲーム状態(`G`)

`G`はゲーム1プレイ分の状態を持つオブジェクト(`newRunState()`で生成、`startNewGame`/`continueGame`で差し替え)。主なフィールド:

- `G.player` — `{ x, y, hp, maxHp, hunger, maxHunger, level, exp, atk, def, inventory: [...], sleepTurns, paralyzed, confusedTurns, hasteTurns, weakenTurns, megaEvolveTurns, ironWallTurns, curseTurns, poison, ... }`
- `G.floor` / `G.turn` / `G.grid` / `G.rooms` / `G.roomId` / `G.enemies` / `G.floorItems` / `G.log` / `G.scrollIdentity` / `G.gameOver`

インベントリの1スロットは`{ key, name, cat, sub, value, sym, color, desc, count, equipped? }`。`makeInventoryEntry(def, count)`で作る。**装備中の武器/盾/投げ物も`G.player.inventory`の中に入っており、独立フィールド(`G.player.weapon`のようなもの)は存在しない。** 該当スロットに`equipped: true`が立っているかどうかで区別する(各カテゴリにつき同時に装備できるのは1つまで)。現在装備中のスロットを取得するには`findEquippedSlot(cat)`(`cat`は`"weapon"`/`"shield"`/`"throwable"`)を使うこと — `G.player.weapon`のような直接参照は存在しないので書かない。

## 現時点で確立している仕様・設計方針

- **持ち物枠は10個(`INV_CAP = 10`)。** 装備中の武器/盾/投げ物もこの10枠のうちに含まれる(装備専用の別枠は無い)。ユーザーが枠数について「大事にしたい」と言ったことがあるので、変更するときは必ず確認を取ること。
- **装備の付け替えは、対象スロットの`equipped`フラグを付け替えるだけ**(`equipInventorySlot`/`equipThrowableSlot`)。持ち物からの出し入れ(容量チェック・床に置く処理)は発生しない。装備解除は`unequipInventorySlot`(持ち物には残ったまま`equipped: false`になる)。`useInventorySlot`は対象スロットが武器/盾/投げ物のとき、`slot.equipped`を見て装備/解除のどちらかへ自動でディスパッチする。拾った瞬間の自動装備は`addItemToInventory(def, qty, {autoEquip: true})`のオプションで行う(該当カテゴリに何も装備していない場合のみ)。
- **呪いの巻物は武器/盾の装備解除・付け替え・(装備中の状態での)捨てるの3つすべてをブロックする**(`equipInventorySlot`/`unequipInventorySlot`/`dropInventorySlot`それぞれの入口でcurseTurnsをチェック)。投げ物は呪いの対象外。ブロックされた装備/解除の試みはターンを消費する(`doTurn()`を呼ぶ)が、捨てるのブロックはターンを消費しない(「捨てる」操作自体がそもそもターン消費なしのルールのため)。
- **持ち物欄(サイドバー・Aキーメニューどちらも)では、装備中のアイテムに「E」マーク(`.equip-badge`)が付く。** アクションラベルは武器/盾/投げ物なら装備中は「外す」・未装備なら「装備」または「装備する」、それ以外のカテゴリは常に「使う」。
- **投げ物(矢・石)と杖以外のアイテムは1枠につき1個まで(スタックしない)。** `addItemToInventory`の`stackable`判定は`def.cat === "throwable" || def.cat === "wand"`のみtrue。薬草・おにぎり・巻物などは同じ種類を複数持っていても別々の枠に並ぶ(count は常に1)。杖の`count`は「残り使用回数」という別概念なのでスタック対象に残している。
- **アイテムの使用・装備はターン消費あり。** `useInventorySlot`(薬草・おにぎり・巻物・杖の使用、武器/盾/投げ物の装備 すべてこの関数からディスパッチされる)は関数の入口で`turnBusy`と`handlePlayerIncapacitated()`をチェックし、効果適用後に`doTurn()`を呼ぶ。例外は巻物「階段の巻物」(`descend()`を直接呼び、階段を直接踏んで降りる場合と同様にdoTurnを呼ばない)。「捨てる」(`dropInventorySlot`)はターンを消費しない。
- **クリア条件はボス撃破ではなく単純に地下50階(`MAX_FLOOR`)で階段を踏むこと。** `descend()`内の`if (G.floor >= MAX_FLOOR) { triggerClear(); return; }`がそれ。ボス(10階ごと)は倒さなくてもクリアでき、倒すと良いアイテムを確定ドロップするだけの「強い雑魚敵」という位置づけ。
- **巻物は種類ごとに未識別状態を持つ**(`G.scrollIdentity`、`assignScrollIdentities()`で新規ゲーム開始時に色名をシャッフル割り当て)。表示には`scrollDisplayName(key)`/`scrollDisplayDesc(key)`を使うこと(`slot.name`を直接使わない)。
- **キー配置**: 矢印キー=移動、Z=攻撃/階段を降りる、S=射撃(投げ物)、左シフト+矢印=斜め移動コンボ、q/e/c=固定斜め移動、A=持ち物簡易選択メニューを開く、メニュー中は矢印上下=選択・Z=使用確定・X=キャンセルして閉じる。
- 状態異常の管理: `isIncapacitated(e)`(睡眠・金縛り・モンスターハウス待機)、`clearParalysis(entity)`(ダメージ/命中ミスで金縛り解除)、`handlePlayerIncapacitated()`(プレイヤーの行動可否をまとめて判定、行動できない場合は代わりの処理をしてtrueを返す)。

## 開発ワークフロー(このセッションで確立した進め方)

1. `shiren-like.html`を直接編集。
2. 動作確認用に、`init();\n})();`の直前へ`window.__dbg = {...}`を挿入した一時ファイル(`debug-*.html`)をPythonのstring-replaceで生成し、内部関数・状態を露出させる。
3. `NODE_PATH=/opt/node22/lib/node_modules node <script>.js`でPlaywright(headless Chromium, `executablePath: '/opt/pw-browsers/chromium'`)を使い、対象の変更点をピンポイントでテスト。
4. 変更の種類を問わず、最後に**フルリグレッション**を流す: 1〜50階を`enterFloor(d)`で生成してエラーが出ないことを確認 + 全入力キーを使った数百回のランダム操作でエラーが出ないことを確認。
5. **`debug-*.html`は必ず削除してからコミットする**(リポジトリに残さない)。
6. 挙動がユーザーから見て変わる場合は`README.md`も更新(内部リファクタのみなら更新不要、と明記してコミットメッセージに書く)。
7. `git add -A && git commit`。コミットメッセージは日本語、「何を直したか」「何を意図的に見送ったか(理由付き)」「テスト内容」を書く。末尾に以下の帰属フッターを必ず付ける:
   ```
   Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
   Claude-Session: https://claude.ai/code/session_012Bn7WFJWMzResn4evKoDKY
   ```
   (セッションが変わったら`Claude-Session`のURLは新しいセッションのものに差し替える。)
8. `git push -u origin claude/shiren-like-game-dev-w8s9em`(このブランチがPRの追跡ブランチ)。
9. Artifactを再公開: `Artifact`ツールに`url: "https://claude.ai/code/artifact/dce95331-88c9-4c20-b0b4-ce682fa160d5"`と`file_path: "/home/user/first-repository/shiren-like.html"`を渡す(favicon省略で既存アイコン維持)。
10. ユーザーへ日本語で、変更点・テスト結果・意図的に見送った点を簡潔に報告する。

## その他の注意点

- リクエストはすべて日本語で来る。返答も日本語。
- ユーザーは細かいバランス調整・UI仕様変更を頻繁に依頼してくる。過去の会話で確立した設計判断(上記「現時点で確立している仕様・設計方針」)と矛盾する変更を頼まれたら、変更後の状態が一貫しているか確認する(例: スタック不可ルールを変えるなら、満腹の巻物の実装や表示ロジックとの整合性を確認する)。
- 本質的にあいまいな依頼(例:「ボスとかはいらない」)は、実装を変える前に何を望んでいるか確認する。すでに現在の実装が要望を満たしている場合もある。
- ゲームは完全にクライアントサイド(ブラウザ内)で完結しており、プレイ自体はClaudeのAPI/クレジットを消費しない。
