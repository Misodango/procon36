#include "Field.h"
#include <Siv3D.hpp> // Siv3Dの機能を使用
#include <chrono>    // For seeding RNG

// Define static members for Zobrist Hashing
std::vector<std::vector<std::vector<uint64_t>>> Field::zobristTable;
bool Field::zobristTableInitialized = false;
std::mt19937_64 Field::rng(std::chrono::steady_clock::now().time_since_epoch().count()); // Seed RNG

// Named constants for scoring weights in evaluateState
namespace {
	static const float ScoreFactorPairCount = 100.0f;
	static const float ScoreFactorConsecutivePairs = 50.0f;
	static const float ScoreFactorFormedPairBonus = 5.0f; // Used for already adjacent pairs and in rotateAndGetDiff
	static const float ScoreFactorUnformedPairBase = 20.0f;
	static const float ScoreFactorUnformedPairSameRowColBonus = 5.0f;
	static const float ScoreFactorUnformedPairDiagonalBonus = 2.0f;
	static const float ScoreFactorUnformedPairEstRotPenalty = 0.5f;
	static const float ScoreFactor2x2Pattern = 2.0f;
	static const float ManhattanPenaltyFactorUnpaired = 0.5f;
}

/*
*  @brief フィールドのコンストラクタ
*  @param size フィールドのサイズ
*  @return 0埋めされたフィールド
*/

Field::Field(int32 size) : size(size), entityCount(size* size / 2 - 1), entities(size, size, 0) {}

/*
*  @brief フィールドのコピーコンストラクタ
*  @param other コピー元のフィールド
*  @return コピーされたフィールド
*/

Field::Field(const Field& other) : size(other.size), entityCount(other.entityCount), entities(other.entities) {}

/*
*  @brief フィールドのコピー代入演算子
* @param other コピー元のフィールド
* @return コピーされたフィールド
*/

Field& Field::operator=(const Field& other) {
	if (this != &other) {
		size = other.size;
		entityCount = other.entityCount;
		entities = other.entities;
	}
	return *this;
}


/*
*  @brief ランダムにフィールドを生成する
*  @param size フィールドのサイズ
*  @return ランダム生成されたフィールド
*/

Field Field::random(int32 size) {
	Field field(size);
	// エンティティは1つにつき2回出現する
	Array<int32> candidates((field.entityCount + 1) * 2);
	for (int32 i : step(field.entityCount + 1)) {
		candidates[i * 2] = candidates[i * 2 + 1] = i;
	}
	candidates.shuffle();
	for (int32 x : step(size)) {
		for (int32 y : step(size)) {
			field.entities[x][y] = candidates[x * field.size + y];
		}
	}
	return field;
}

/*
*  @brief JSONからフィールドを生成する
*  @param json JSONオブジェクト
*  @return フィールド
*/

Field Field::fromJSON(const JSON& json) {
	const int32 size = json[U"problem"][U"field"][U"size"].get<int32>();
	const JSONArrayView entitiesArray = json[U"problem"][U"field"][U"entities"].arrayView();
	Field field(size);
	for (int y = 0; y < size; ++y) {
		const JSONArrayView entities = entitiesArray[y].arrayView();
		for (int x = 0; x < size; ++x) {
			field.entities[y][x] = entities[x].get<int32>();
		}
	}
	return field;
}

/*
*  @brief ファイルからフィールドを生成する
*  @param path ファイルのパス
*  @return フィールド
*/

Field Field::fromPath(const FilePath& path) {

	const JSON json = JSON::Load(path);
	if (json) {
		return Field::fromJSON(json);
	}
	else {
		Print << U"JSON ファイルの読み込みに失敗しました";
	}
	return Field(0);
}

/*
* @ brief 任意の座標に導きを適用
* @ param x x座標 y y座標 n サイズ
* @ return void
*/

void Field::rotate(int32 x, int32 y, int32 n) {
	// 範囲外チェック
	if (x < 0 || y < 0 || x + n > size || y + n > size) {
		return;
	}

	// n <= 1の回転は無効
	if (n <= 1) {
		return;
	}

	Grid<int32> temp(n, n);
	// 回転前の値を一時的な配列にコピー
	for (int32 i = 0; i < n; ++i) {
		for (int32 j = 0; j < n; ++j) {
			temp[i][j] = entities[y + i][x + j];
		}
	}

	// 90度時計回りに回転させて元の配列にコピー
	for (int32 i = 0; i < n; ++i) {
		for (int32 j = 0; j < n; ++j) {
			entities[y + j][x + n - 1 - i] = temp[i][j];
		}
	}
}

/*
*  @brief フィールドを描画する
*/

void Field::draw() const {
	static Array<Color> colors;
	static const Font font(20);

	// Initialize colors only once
	if (colors.isEmpty()) {
		colors.resize(entityCount + 1);

		// Golden ratio approach for better distribution
		const double goldenRatioConjugate = 0.618033988749895;
		double h = 0.5; // Starting hue

		for (int i = 0; i <= entityCount; ++i) {
			h = fmod(h + goldenRatioConjugate, 1.0);
			colors[i] = ColorF(HSV(h * 360.0, 0.7, 0.95));
		}
	}

	for (int32 y : step(size)) {
		for (int32 x : step(size)) {

			Rect(x * 50, y * 50, 50, 50).draw(colors[entities[y][x]]);
			font(copysign(entities[y][x], 1)).drawAt(x * 50 + 25, y * 50 + 25, Palette::Black);
		}
	}

	// フィールドの枠を描画
	Rect(0, 0, size * 50, size * 50).drawFrame(2, Palette::Black);

}


/*
* @brief フィールドのペアを数える
* @return int32
*/

int32 Field::countPairs() const {
	const int32 dx[4] = { 1, 0, -1, 0 };
	const int32 dy[4] = { 0, 1, 0, -1 };
	int32 res = 0;
	Array<bool> seen(size * size / 2, false);
	for (int32 i : step(size)) {
		for (int32 j : step(size)) {
			int32 cur = entities[i][j];
			if (seen[cur]) {
				continue;
			}
			for (int32 k : step(4)) {
				int32 y = i + dy[k];
				int32 x = j + dx[k];
				if (x < 0 || size <= x || y < 0 || size <= y) {
					continue;
				}
				int32 nxt = entities[y][x];
				if (nxt != cur) continue;
				seen[cur] = true;
				res++;
				break;
			}

		}
	}
	return res;
}

/*
* @brief フィールドのサイズを取得
* @return int32
*/

int32 Field::getSize() const {
	return size;
}

/*
* @brief ペアのマスかどうかを判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPair(int32 x, int32 y) const {
	const int32 entity = entities[y][x];

	if (x + 1 < size && entities[y][x + 1] == entity) return true;

	if (y + 1 < size && entities[y + 1][x] == entity) return true;

	if (x - 1 >= 0 && entities[y][x - 1] == entity) return true;

	if (y - 1 >= 0 && entities[y - 1][x] == entity) return true;

	return false;
}


/*
* @brief 左上から連続のペアを数える
* @return int32
*/

int32 Field::countPairsFromTopLeftHorizontal() const {
	const int32 dx[4] = { 1, 0, -1, 0 };
	const int32 dy[4] = { 0, 1, 0, -1 };
	int32 res = 0;
	Array<bool> seen(size * size / 2, false);
	for (int32 i : step(size)) {
		for (int32 j : step(size)) {
			int32 cur = entities[i][j];
			if (seen[cur]) {
				continue;
			}
			if (!isPair(i, j)) {
				return res;
			}
			res++;
			seen[cur] = true;
		}
	}
	return res;
}

/*
* @brief 左上から連続のペアを数える
* @return int32
*/

int32 Field::countPairsFromTopLeftVertical() const {
	const int32 dx[4] = { 1, 0, -1, 0 };
	const int32 dy[4] = { 0, 1, 0, -1 };
	int32 res = 0;
	Array<bool> seen(size * size / 2, false);
	for (int32 i : step(size)) {
		for (int32 j : step(size)) {
			int32 cur = entities[j][i];
			if (seen[cur]) {
				continue;
			}
			if (!isPair(j, i)) {
				return res;
			}
			res++;
			seen[cur] = true;
		}
	}
	return res;
}

/*
* @brief 差分更新
* @param x x座標
* @param y y座標
* @param n サイズ
*/

std::pair<int32, float> Field::rotateAndGetDiff(int32 x, int32 y, int32 n) {
	// 回転前の適用領域と周囲のペアをカウント
	int32 beforePairs = 0;
	float beforeScore = 0;

	// The area affected is the rotation area plus a 1-cell border around it
	const int32 checkStartX = std::max(0, x - 1);
	const int32 checkStartY = std::max(0, y - 1);
	const int32 checkEndX = std::min(size, x + n + 1);
	const int32 checkEndY = std::min(size, y + n + 1);
	Array<bool> seen(size * size / 2, false);
	// Count pairs before rotation
	for (int32 cy = checkStartY; cy < checkEndY; ++cy) {
		for (int32 cx = checkStartX; cx < checkEndX; ++cx) {
			if (!seen[entities[cy][cx]] && isPair(cx, cy)) {
				beforePairs++;
				beforeScore += ScoreFactorFormedPairBonus;  // Use named constant
				seen[entities[cy][cx]] = true;
			}
		}
	}

	// Perform the actual rotation
	rotate(x, y, n);

	// seenのリセット
	seen.fill(false);

	// Count pairs after rotation in the same area
	int32 afterPairs = 0;
	float afterScore = 0;
	for (int32 cy = checkStartY; cy < checkEndY; ++cy) {
		for (int32 cx = checkStartX; cx < checkEndX; ++cx) {
			if (!seen[entities[cy][cx]] && isPair(cx, cy)) {
				afterPairs++;
				afterScore += ScoreFactorFormedPairBonus; // Use named constant
				seen[entities[cy][cx]] = true;
			}
		}
	}

	// Return the difference
	return { afterPairs - beforePairs, afterScore - beforeScore };
}

/*
* @brief 合法手を取得
* @return Array<Solution>
*/
Array<Solution> Field::getLegalMoves() const {
	Array<Solution> legalMoves;
	for (int32 y : step(size)) {
		for (int32 x : step(size)) {
			int32 range = size - Max(x, y);
			for (int32 n : step(2, range + 1)) {
				;
			}
		}
	}
	return legalMoves;
}

/*
* @brief 終了判定
* @return bool
*/

bool Field::isFinished() const {
	return countPairs() == size * size / 2;
}

/*
* @brief 右にペアがあるか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::hasPairRight(int32 x, int32 y) const {
	return (x + 1 < size && entities[y][x] == entities[y][x + 1]);
}

/*
* @brief 左にペアがあるか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::hasPairLeft(int32 x, int32 y) const {
	return (x - 1 >= 0 && entities[y][x] == entities[y][x - 1]);
}

/*
* @brief 上にペアがあるか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::hasPairUp(int32 x, int32 y) const {
	return (y - 1 >= 0 && entities[y][x] == entities[y - 1][x]);
}

/*
* @brief 下にペアがあるか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::hasPairDown(int32 x, int32 y) const {
	return (y + 1 < size && entities[y][x] == entities[y + 1][x]);
}

/*
* @brief 自信が右のペアか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPairRight(int32 x, int32 y) const {
	return (0 <= x - 1 && entities[y][x] == entities[y][x - 1]);
}

/*
* @brief 自信が左のペアか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPairLeft(int32 x, int32 y) const {
	return (x + 1 < size && entities[y][x] == entities[y][x + 1]);
}

/*
* @brief 自信が上のペアか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPairUp(int32 x, int32 y) const {
	return (0 <= y - 1 && entities[y][x] == entities[y - 1][x]);
}

/*
* @brief 自信が下のペアか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPairDown(int32 x, int32 y) const {
	return (y + 1 < size && entities[y][x] == entities[y + 1][x]);
}

/*
* @brief Zobristテーブルを初期化する
* @param maxSize フィールドの最大サイズ
* @param maxEntityValuePlusOne エンティティの最大値 + 1
*/
void Field::initializeZobristTable(int32 maxSize, int32 maxEntityValuePlusOne) {
	if (zobristTableInitialized) return;

	zobristTable.resize(maxSize);
	for (int32 i = 0; i < maxSize; ++i) {
		zobristTable[i].resize(maxSize);
		for (int32 j = 0; j < maxSize; ++j) {
			zobristTable[i][j].resize(maxEntityValuePlusOne);
			for (int32 k = 0; k < maxEntityValuePlusOne; ++k) {
				zobristTable[i][j][k] = std::uniform_int_distribution<uint64_t>()(rng);
			}
		}
	}
	zobristTableInitialized = true;
}

/*
* @brief フィールドをZobristハッシュ値に変換
* @return size_t
*/
size_t Field::computeHash() const {
	if (!zobristTableInitialized) {
		// Fallback or error, though initializeZobristTable should be called by algorithm constructor
		// For safety, one might call initialize here or throw an error.
		// Let's assume it's initialized. Or, for robustness:
		// Field::initializeZobristTable(this->size, this->entityCount + 1); // Potentially problematic if called concurrently or with different max sizes
		// Better to ensure it's called once at the start.
	}
	uint64_t currentHash = 0;
	for (int32 y = 0; y < size; ++y) {
		for (int32 x = 0; x < size; ++x) {
			// Ensure entities[y][x] is a valid index for zobristTable's last dimension
			// And that y, x are within the table bounds (although current use implies this->size <= maxSize used in init)
			if (y < zobristTable.size() && x < zobristTable[y].size() &&
				entities[y][x] >= 0 && static_cast<size_t>(entities[y][x]) < zobristTable[y][x].size()) {
				currentHash ^= zobristTable[y][x][entities[y][x]];
			}
			else {
				// Handle error or use a default hash for unexpected entity values/sizes
				// This case should ideally not be reached if initialization and field construction are correct.
				// As a fallback, could XOR with a fixed value or entity value itself, but this breaks Zobrist properties.
				// For now, let's assume entity values and sizes are always valid.
			}
		}
	}
	return static_cast<size_t>(currentHash);
}

/*
* @brief フィールドを標準ハッシュ値に変換 (旧computeHash)
* @return size_t
*/
size_t Field::computeStdHash() const {
	size_t hash = 0;
	for (int32 y : step(size)) {
		for (int32 x : step(size)) {
			hash ^= std::hash<int32>()(entities[y][x]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		}
	}
	return hash;
}

/*
* @brief フィールドの評価値を計算する（ビームサーチ用）
* @return float 評価値（高いほど良い状態）
*/

float Field::evaluateState() const {

	float score = 0.0f;

	// 1. ペアの数を基本スコアとする（最も重要）
	int32 pairCount = countPairs();
	score += pairCount * ScoreFactorPairCount;

	// 2. 左上からの連続ペアを高く評価
	int32 consecutivePairsH = countPairsFromTopLeftHorizontal();
	int32 consecutivePairsV = countPairsFromTopLeftVertical();
	score += std::max(consecutivePairsH, consecutivePairsV) * ScoreFactorConsecutivePairs;

	// 3. 各エンティティについて、ペアとなるもう一方への「回転距離」を評価
	// 同じ数字の位置を記録する一時的なマップ
	std::unordered_map<int32, std::vector<std::pair<int32, int32>>> entityPositions;

	// 各エンティティの位置を記録
	for (int32 y = 0; y < size; ++y) {
		for (int32 x = 0; x < size; ++x) {
			int32 entity = entities[y][x];
			if (entity > 0) { // 0は空白なので無視
				entityPositions[entity].push_back({ x, y });
			}
		}
	}

	// 各エンティティペアの「回転距離」を評価
	for (const auto& [entity, positions] : entityPositions) {
		// 同じ数字が2つあるはず（ペアになる条件）
		if (positions.size() == 2) {
			auto [x1, y1] = positions[0];
			auto [x2, y2] = positions[1];

			// 既にペアになっている場合はスコア加算（隣接している）
			if (abs(x1 - x2) + abs(y1 - y2) == 1) {
				score += ScoreFactorFormedPairBonus;
				continue;
			}

			// 回転距離の計算：
			// 1. マンハッタン距離の逆数をベースにする
			int32 manhattanDist = abs(x1 - x2) + abs(y1 - y2);

			// 2. 同じ行または同じ列にある場合は少し加点（1回の回転で近づける可能性が高い）
			bool sameRow = (y1 == y2);
			bool sameCol = (x1 == x2);

			// 3. 対角線上にある場合も評価（L字の動きが必要）
			bool diagonal = abs(x1 - x2) == abs(y1 - y2);

			// マンハッタン距離が小さいほど良い（大きいほど悪い）
			float distanceScore = ScoreFactorUnformedPairBase / (manhattanDist + 1.0f);

			// 同じ行/列ボーナス
			if (sameRow || sameCol) {
				distanceScore += ScoreFactorUnformedPairSameRowColBonus;
			}

			// 対角線ボーナス（やや難しいがまだ扱いやすい）
			if (diagonal) {
				distanceScore += ScoreFactorUnformedPairDiagonalBonus;
			}

			// 必要な回転回数の推定（より大きなグリッドサイズは動かせる範囲が広いが、精密さは低い）
			// マンハッタン距離を最小のグリッドサイズ（2）で割った値が必要回転回数の下限
			int32 estimatedRotations = (manhattanDist + 1) / 2;
			distanceScore -= estimatedRotations * ScoreFactorUnformedPairEstRotPenalty;

			score += distanceScore;
		}
	}

	// Calculate Manhattan distance penalty for unpaired pieces
	float totalManhattanDistanceUnpaired = 0.0f;
	for (const auto& [entity, positions] : entityPositions) {
		if (positions.size() == 2) {
			auto [x1, y1] = positions[0];
			auto [x2, y2] = positions[1];
			int32 manhattanDist = std::abs(x1 - x2) + std::abs(y1 - y2);
			if (manhattanDist > 1) { // Not adjacent (already paired items are handled by manhattanDist == 1 in the loop above)
				totalManhattanDistanceUnpaired += static_cast<float>(manhattanDist);
			}
		}
	}

	// Apply Manhattan Distance Penalty using the named constant
	score -= totalManhattanDistanceUnpaired * ManhattanPenaltyFactorUnpaired;

	// 4. ペアになる可能性が高い配置パターンを評価
	for (int32 y = 0; y < size - 1; ++y) {
		for (int32 x = 0; x < size - 1; ++x) {
			// 2x2の範囲内で同じ数字がある場合（回転しやすい）
			std::unordered_set<int32> entitiesIn2x2;
			for (int32 dy = 0; dy < 2; ++dy) {
				for (int32 dx = 0; dx < 2; ++dx) {
					entitiesIn2x2.insert(entities[y + dy][x + dx]);
				}
			}

			// 2x2内の重複を評価（同じ数字が複数ある＝ペアになりやすい）
			if (entitiesIn2x2.size() < 4) {
				score += (4 - entitiesIn2x2.size()) * ScoreFactor2x2Pattern;
			}
		}
	}

	return score;
}

