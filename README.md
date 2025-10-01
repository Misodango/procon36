# パズルゲーム仕様

## データ形式

### 入力 (input)

- `entities`: 初期盤面を表す2次元配列（size × size のグリッド）
- `size`: グリッドのサイズ

### 出力 (output)

- `num_operations`: 操作回数
- `ops`: 操作列を1次元配列で表現（実際は(x, y, s)のタプルが連続して格納）

## ゲームルール

### 初期盤面

- size × size のグリッドに数字が配置される
- 数字の範囲：[0, size²)
- 各数字はちょうど2回ずつ現れる（重複なし）

### 操作

操作列`ops`は3つずつ組になっており、各組は以下を表す：

- `(x, y, s)`: 座標(x, y)を左上角とする s×s の正方形領域を90度時計回りに回転

例：`ops = [0, 3, 3, 0, 1, 4, ...]` の場合

- 1番目の操作：(0, 3, 3) → 座標(0,3)から3×3領域を90度時計回りに回転
- 2番目の操作：(0, 1, 4) → 座標(0,1)から4×4領域を90度時計回りに回転

### ゴール条件

最終盤面において、すべての数字が4近傍（上下左右）で隣り合う**ペア**を形成していること。

注意：ゴール盤面は一意ではなく、ペア条件を満たす配置であれば正解となる。

## データ例
{
  "input": {
    "entities": [
      [17, 31, 0, 10, 3, 7, 2, 23],
      [8, 25, 15, 22, 31, 19, 5, 6],
      // ... 8×8グリッド
    ],
    "size": 8
  },
  "output": {
    "num_operations": 31,
    "ops": [0, 3, 3, 0, 1, 4, 1, 3, 5, ...] // 31×3 = 93個の要素
  }
}
この例では：

- 8×8のグリッド（64マス）
- 数字0-31が各2回ずつ配置
- 31回の回転操作で解決

# ビームサーチアルゴリズム修正ガイド

このドキュメントは、プロジェクトに含まれるビームサーチ系のアルゴリズム (`BeamSearchAlgorithm`, `IterativeBeamSearchAlgorithm`) の仕様を理解し、修正を加えるためのガイドです。

## 1. アルゴリズムの概要

このプロジェクトには、主に2つのビームサーチ実装が存在します。

-   **`BeamSearchAlgorithm`**:
    指定された状態から、設定されたビーム幅（`beamWidth`）と最大探索深さ（`maxDepth`）に基づいて一度の探索を実行します。最も評価値の高い状態への手順（`Solution`）を返します。

-   **`IterativeBeamSearchAlgorithm`**:
    `BeamSearchAlgorithm`を短いステップで繰り返し実行する戦略です。各ステップで得られた最良の手順を適用して状態を更新し、その新しい状態から再度ビームサーチを行います。これにより、単一の深い探索では見つけにくい解に到達できる可能性があります。

## 2. 関連ファイル

修正の対象となる主要なファイルは以下の通りです。

-   `BeamSearchAlgorithm.h`: `BeamSearchAlgorithm` クラスの宣言。
-   `BeamSearchAlgorithm.cpp`: `BeamSearchAlgorithm` クラスの主要な実装。**探索ロジックの大部分はここにあります。**
-   `IterativeBeamSearchAlgorithm.h`: `IterativeBeamSearchAlgorithm` クラスの宣言。
-   `IterativeBeamSearchAlgorithm.cpp`: `IterativeBeamSearchAlgorithm` クラスの実装。
-   `Field.h`, `Field.cpp`: 盤面の状態を管理するクラス。状態の評価関数や操作（回転）の適用などが実装されています。

## 3. 主要なクラスと構造体

### `BeamSearchAlgorithm`
-   **役割**: コアとなるビームサーチ処理を実装します。
-   **主要メンバ**:
    -   `m_beamWidth`: 探索の幅。各ステップで保持する候補状態の最大数。
    -   `m_maxDepth`: 探索の最大深さ。
-   **主要メソッド**:
    -   `run()`: ビームサーチを実行し、解を返します。

### `IterativeBeamSearchAlgorithm`
-   **役割**: `BeamSearchAlgorithm` を繰り返し呼び出し、段階的に解を探索します。
-   **主要メンバ**:
    -   `m_beamWidthPerStep`: 各ステップで実行するビームサーチのビーム幅。
    -   `m_depthPerStep`: 各ステップで実行するビームサーチの探索深さ。
    -   `m_maxTotalSteps`: 繰り返しの最大ステップ数。

### `Field`
-   **役割**: パズルの盤面状態を表現し、操作や評価を行います。
-   **主要メソッド**:
    -   `rotate(x, y, size)`: 指定された領域を回転させます。
    -   `evaluateStateWithEntropy()`: 盤面全体の評価値を計算します。
    -   `rotateAndGetDiff(x, y, size)`: 回転操作による評価値の差分を効率的に計算します。
    -   `computeHash()`: Zobrist Hashing を用いて盤面のハッシュ値を計算し、訪問済み状態の高速な検出を可能にします。

### `BeamState` (in `BeamSearchAlgorithm.cpp`)
-   **役割**: 探索中の各候補状態を保持します。優先度付きキュー（`std::priority_queue`）で管理されます。
-   **メンバ**:
    -   `field`: 盤面状態。
    -   `solution`: その状態に至るまでの手順。
    -   `score`: 評価値。
    -   `depth`: 探索深さ。

### `Operation` (in `BeamSearchAlgorithm.cpp`)
-   **役割**: 可能な一手（回転操作）を表現します。
-   **メンバ**:
    -   `x, y, size`: 操作の座標とサイズ。
    -   `scoreDiff`, `pairDiff`, `entropyDiff`: 操作によって得られる評価値やその他の指標の差分。
    -   `isPromising`: 有望な手かどうかを示すフラグ。
-   **ソート順**: この構造体の `<` 演算子は、どの手を優先的に試すかを決定します。現在の実装では `isPromising` > `scoreDiff` > `entropyDiff` の順で評価されます。

## 4. 修正ガイド

### 探索パラメータの調整
-   **`BeamSearchAlgorithm`**:
    -   コンストラクタのデフォルト引数 (`beamWidth`, `maxDepth`) を変更します。
    -   `BeamSearchAlgorithm` を呼び出している箇所で直接値を指定します。
-   **`IterativeBeamSearchAlgorithm`**:
    -   コンストラクタのデフォルト引数 (`beamWidthPerStep`, `depthPerStep`, `maxTotalSteps`) を変更します。

**トレードオフ**:
-   `beamWidth` を増やすと、解を見つける可能性は高まりますが、計算量とメモリ使用量が増加します。
-   `maxDepth` を増やすと、より長い手順の解を見つけられますが、探索時間が長くなります。

### 状態評価ロジックの変更
-   **盤面全体の評価**: `Field::evaluateStateWithEntropy()` を修正します。
-   **操作の差分評価**: `Field::rotateAndGetDiff()` を修正します。これは手の生成時に使われるため、パフォーマンスへの影響が大きいです。
-   **手の優先順位付け**: `BeamSearchAlgorithm.cpp` 内の `Operation` 構造体の `<` 演算子を修正することで、どの手を優先的に探索するかを変更できます。

### 操作（手）生成ロジックの変更
-   手の候補は `BeamSearchAlgorithm::run()` 内の `for (int32 s_loop ...)` ループで生成されます。
-   このループ内で、`std::async` を用いて並列処理を行っています。
-   `if (pairDiff < keep_min_pair_diff) continue;` のような条件文を変更・追加することで、有望でない手を早期に枝刈りし、探索を効率化できます。

### パフォーマンスに関する考慮事項
-   **Zobrist Hashing**: `visited` セットによる訪問済み状態のチェックは、`Field::computeHash()` によって高速化されています。
-   **並列処理**: 手の生成は `std::async` を使って複数のスレッドで並列実行されています。スレッド数を調整する場合は `num_threads_to_use` を確認してください。
-   **差分評価**: `rotateAndGetDiff` のように、盤面全体を再評価するのではなく差分を計算することで、パフォーマンスを向上させています。評価関数の変更時には、同様に差分計算が可能か検討することが推奨されます。