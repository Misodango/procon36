#include "Field.h"
#include <Siv3D.hpp> // Siv3Dの機能を使用
#include <chrono>    // For seeding RNG
#include <cmath>     // For Euclidean distance calculations
#include <bitset>
#include <execution>
#include <numeric>
#include <thread>
#include <future>


// Define static members for Zobrist Hashing
std::vector<std::vector<std::vector<uint64_t>>> Field::zobristTable;
bool Field::zobristTableInitialized = false;
std::mt19937_64 Field::rng(std::chrono::steady_clock::now().time_since_epoch().count()); // Seed RNG

// Named constants for scoring weights in evaluateState
namespace {
	static const float ScoreFactorPairCount = 100.0f;
	static const float ScoreFactorConsecutivePairs = 50.0f;
	// Consider adjusting ScoreFactorFormedPairBonus to change the importance of pair formation
	static const float ScoreFactorFormedPairBonus = 5.0f; // Used for already adjacent pairs and in rotateAndGetDiff
	static const float ScoreFactorUnformedPairBase = 20.0f;
	static const float ScoreFactorUnformedPairSameRowColBonus = 5.0f;
	static const float ScoreFactorUnformedPairDiagonalBonus = 2.0f;
	static const float ScoreFactorUnformedPairEstRotPenalty = 0.5f;
	static const float ScoreFactor2x2Pattern = 2.0f;
	static const float ManhattanPenaltyFactorUnpaired = 0.5f;
	// Consider adjusting ScoreFactorEuclideanDistance to change the influence of distance optimization
	static const float ScoreFactorEuclideanDistance = 0.1f; // New scoring factor

	// Helper function to find positions of an entity
	std::vector<std::pair<int32, int32>> findEntityPositions(int32 entityValue, const Grid<int32>& entities, int32 fieldSize) {
		std::vector<std::pair<int32, int32>> positions;
		if (entityValue == 0) return positions; // Skip blank entity

		for (int32 y = 0; y < fieldSize; ++y) {
			for (int32 x = 0; x < fieldSize; ++x) {
				if (entities[y][x] == entityValue) {
					positions.push_back({ x, y });
					if (positions.size() == 2) return positions; // Found both, no need to search further
				}
			}
		}
		return positions; // Should ideally always find 2 for valid entities, or 0 if not on board (e.g. during diff)
	}
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
	float beforeScoreFormedPairs = 0; // Score from newly formed/broken adjacent pairs

	const int32 checkStartX = std::max(0, x - 1);
	const int32 checkStartY = std::max(0, y - 1);
	const int32 checkEndX = std::min(size, x + n + 1);
	const int32 checkEndY = std::min(size, y + n + 1);

	std::unordered_set<int32> uniqueEntitiesInAffectedArea;
	for (int32 cy = checkStartY; cy < checkEndY; ++cy) {
		for (int32 cx = checkStartX; cx < checkEndX; ++cx) {
			if (entities[cy][cx] != 0) { // Assuming 0 is blank/irrelevant for pairing
				uniqueEntitiesInAffectedArea.insert(entities[cy][cx]);
			}
		}
	}
	// Also include entities within the rotation box itself, as their relative positions change
	for (int32 r_y = 0; r_y < n; ++r_y) {
		for (int32 r_x = 0; r_x < n; ++r_x) {
			if (entities[y + r_y][x + r_x] != 0) {
				uniqueEntitiesInAffectedArea.insert(entities[y + r_y][x + r_x]);
			}
		}
	}


	Array<bool> seenBefore(this->entityCount + 1, false); // Max entity value + 1
	for (int32 cy = checkStartY; cy < checkEndY; ++cy) {
		for (int32 cx = checkStartX; cx < checkEndX; ++cx) {
			int32 currentEntity = entities[cy][cx];
			if (currentEntity != 0 && !seenBefore[currentEntity] && isPair(cx, cy)) {
				beforePairs++;
				beforeScoreFormedPairs += ScoreFactorFormedPairBonus;
				seenBefore[currentEntity] = true;
			}
		}
	}

	float sumEuclideanDistBefore = 0.0f;
	for (int32 entity : uniqueEntitiesInAffectedArea) {
		auto positions = findEntityPositions(entity, this->entities, this->size);
		if (positions.size() == 2) {
			float dx = static_cast<float>(positions[0].first - positions[1].first);
			float dy = static_cast<float>(positions[0].second - positions[1].second);
			sumEuclideanDistBefore += std::sqrt(dx * dx + dy * dy);
		}
	}

	// Perform the actual rotation
	rotate(x, y, n);

	// Count pairs and score after rotation in the same area
	int32 afterPairs = 0;
	float afterScoreFormedPairs = 0;
	Array<bool> seenAfter(this->entityCount + 1, false); // Max entity value + 1
	for (int32 cy = checkStartY; cy < checkEndY; ++cy) {
		for (int32 cx = checkStartX; cx < checkEndX; ++cx) {
			int32 currentEntity = entities[cy][cx];
			if (currentEntity != 0 && !seenAfter[currentEntity] && isPair(cx, cy)) {
				afterPairs++;
				afterScoreFormedPairs += ScoreFactorFormedPairBonus;
				seenAfter[currentEntity] = true;
			}
		}
	}

	float sumEuclideanDistAfter = 0.0f;
	for (int32 entity : uniqueEntitiesInAffectedArea) {
		auto positions = findEntityPositions(entity, this->entities, this->size);
		if (positions.size() == 2) {
			float dx = static_cast<float>(positions[0].first - positions[1].first);
			float dy = static_cast<float>(positions[0].second - positions[1].second);
			sumEuclideanDistAfter += std::sqrt(dx * dx + dy * dy);
		}
	}

	// Calculate differences
	int32 pairDiffCount = afterPairs - beforePairs;
	float formedPairScoreDiff = afterScoreFormedPairs - beforeScoreFormedPairs;
	float euclideanScoreDiff = (sumEuclideanDistBefore - sumEuclideanDistAfter) * ScoreFactorEuclideanDistance;

	// Return the difference in pair count and total score difference
	return { pairDiffCount, formedPairScoreDiff + euclideanScoreDiff };
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

	int32 pairCount = countPairs();
	score += pairCount * ScoreFactorPairCount;

	return score;
}


/*
* @brief 指定エンティティの両方の位置を取得
* @param entity エンティティ値
* @return 位置のペア（見つからない場合は空のベクター）
*/
std::vector<std::pair<int32, int32>> Field::getEntityPositions(int32 entity) const {
	std::vector<std::pair<int32, int32>> positions;

	for (int32 y = 0; y < size; ++y) {
		for (int32 x = 0; x < size; ++x) {
			if (entities[y][x] == entity) {
				positions.push_back({ x, y });
				if (positions.size() == 2) break;
			}
		}
		if (positions.size() == 2) break;
	}

	return positions;
}

/*
* @brief 指定エンティティがペアを形成しているかチェック
* @param entity エンティティ値
* @return ペアを形成している場合true
*/
bool Field::isEntityPaired(int32 entity) const {
	auto positions = getEntityPositions(entity);
	if (positions.size() != 2) return false;

	auto [x1, y1] = positions[0];
	auto [x2, y2] = positions[1];

	return abs(x1 - x2) + abs(y1 - y2) == 1;
}

/*
* @brief 指定エンティティのユークリッド距離を計算
* @param entity エンティティ値
* @return ユークリッド距離（ペアが見つからない場合は-1）
*/
float Field::getEntityEuclideanDistance(int32 entity) const {
	auto positions = getEntityPositions(entity);
	if (positions.size() != 2) return -1.0f;

	auto [x1, y1] = positions[0];
	auto [x2, y2] = positions[1];

	float dx = static_cast<float>(x1 - x2);
	float dy = static_cast<float>(y1 - y2);

	return std::sqrt(dx * dx + dy * dy);
}

/*
* @brief 全エンティティのユークリッド距離の合計を計算
* @return 全エンティティのユークリッド距離の合計
*/
float Field::getTotalEuclideanDistance() const {
	float total = 0.0f;

	for (int32 entity = 1; entity <= entityCount; ++entity) {
		float distance = getEntityEuclideanDistance(entity);
		if (distance >= 0) {
			total += distance;
		}
	}

	return total;
}

/*
* @brief 指定位置から最も近い同じエンティティまでの距離を計算
* @param x X座標
* @param y Y座標
* @return 最も近い同じエンティティまでのユークリッド距離
*/
float Field::getNearestSameEntityDistance(int32 x, int32 y) const {
	int32 targetEntity = entities[y][x];
	if (targetEntity == 0) return -1.0f;

	float minDistance = std::numeric_limits<float>::max();
	bool found = false;

	for (int32 cy = 0; cy < size; ++cy) {
		for (int32 cx = 0; cx < size; ++cx) {
			if (cx == x && cy == y) continue; // 自分自身は除外

			if (entities[cy][cx] == targetEntity) {
				float dx = static_cast<float>(x - cx);
				float dy = static_cast<float>(y - cy);
				float distance = std::sqrt(dx * dx + dy * dy);

				if (distance < minDistance) {
					minDistance = distance;
					found = true;
				}
			}
		}
	}

	return found ? minDistance : -1.0f;
}

/*
* @brief 回転操作が有効かどうかを判定
* @param x X座標
* @param y Y座標
* @param n 回転サイズ
* @return 有効な回転操作の場合true
*/
bool Field::isValidRotation(int32 x, int32 y, int32 n) const {
	// 範囲チェック
	if (x < 0 || y < 0 || x + n > size || y + n > size) {
		return false;
	}

	// サイズチェック
	if (n <= 1) {
		return false;
	}

	return true;
}


// ヘルパー関数の最適化版
/*
* @brief 高速化されたエンティティ位置検索
* @param entityValue エンティティ値
* @param entities エンティティグリッド
* @param fieldSize フィールドサイズ
* @return 位置のペア
*/
inline std::vector<std::pair<int32, int32>> findEntityPositionsFast(
	int32 entityValue, const Grid<int32>& entities, int32 fieldSize) {

	std::vector<std::pair<int32, int32>> positions;
	positions.reserve(2);  // 常に2個なので予約

	if (entityValue == 0) return positions;

	// キャッシュフレンドリーな行優先アクセス
	for (int32 y = 0; y < fieldSize; ++y) {
		const int32* row = &entities[y][0];  // 行の先頭ポインタを取得
		for (int32 x = 0; x < fieldSize; ++x) {
			if (row[x] == entityValue) {
				positions.emplace_back(x, y);
				if (positions.size() == 2) return positions;
			}
		}
	}
	return positions;
}

// 高速化ヘルパー関数
inline float fastSqrt(float x) {
	return std::sqrt(x); // コンパイラの最適化に任せる
}

inline int32 manhattan(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
	return std::abs(x1 - x2) + std::abs(y1 - y2);
}

inline float euclidean(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
	float dx = static_cast<float>(x1 - x2);
	float dy = static_cast<float>(y1 - y2);
	return fastSqrt(dx * dx + dy * dy);
}


namespace {
	static const float EntropyWeight = -10.0f;  // エントロピーが高いほどペナルティ
	static const float PositionalEntropyWeight = -5.0f;
	static const float ClusteringBonusWeight = 15.0f;
	static const float LocalOrderWeight = 8.0f;
}

/*
* @brief Shannon エントロピーを計算
* @param frequencies 各要素の出現頻度のマップ
* @param totalCount 総数
* @return float エントロピー値
*/
float Field::calculateShannonEntropy(const std::map<int32, int32>& frequencies, int32 totalCount) const {
	if (totalCount == 0) return 0.0f;

	float entropy = 0.0f;
	for (const auto& [entity, count] : frequencies) {
		if (count > 0) {
			float probability = static_cast<float>(count) / totalCount;
			entropy -= probability * std::log2f(probability);
		}
	}
	entropy /= std::log2f(totalCount); // 正規化
	return entropy;
}

/*
* @brief 局所的な位置エントロピーを計算
* @param windowSize 評価ウィンドウのサイズ（例：3x3）
* @return float 平均位置エントロピー
*/
float Field::calculatePositionalEntropy(int32 windowSize) const {
	if (windowSize < 2) windowSize = 2;

	float totalEntropy = 0.0f;
	int32 windowCount = 0;

	// 各位置を中心とするウィンドウでエントロピーを計算
	for (int32 y = 0; y <= size - windowSize; ++y) {
		for (int32 x = 0; x <= size - windowSize; ++x) {
			std::map<int32, int32> localFreqs;
			int32 totalInWindow = 0;

			// ウィンドウ内の要素を集計
			for (int32 dy = 0; dy < windowSize; ++dy) {
				for (int32 dx = 0; dx < windowSize; ++dx) {
					int32 entity = entities[y + dy][x + dx];
					localFreqs[entity]++;
					totalInWindow++;
				}
			}

			totalEntropy += calculateShannonEntropy(localFreqs, totalInWindow);
			windowCount++;
		}
	}

	return windowCount > 0 ? totalEntropy / windowCount : 0.0f;
}

/*
* @brief クラスタリング係数を計算（同じ要素の密集度）
* @return float クラスタリング係数（0.0-1.0）
*/
float Field::calculateClusteringCoefficient() const {
	int32 totalSameNeighbors = 0;
	int32 totalPossibleNeighbors = 0;

	const int32 dx[4] = { 1, 0, -1, 0 };
	const int32 dy[4] = { 0, 1, 0, -1 };

	for (int32 y = 0; y < size; ++y) {
		for (int32 x = 0; x < size; ++x) {
			int32 currentEntity = entities[y][x];
			int32 validNeighbors = 0;
			int32 sameNeighbors = 0;

			for (int32 dir = 0; dir < 4; ++dir) {
				int32 nx = x + dx[dir];
				int32 ny = y + dy[dir];

				if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
					validNeighbors++;
					if (entities[ny][nx] == currentEntity) {
						sameNeighbors++;
					}
				}
			}

			totalSameNeighbors += sameNeighbors;
			totalPossibleNeighbors += validNeighbors;
		}
	}

	return totalPossibleNeighbors > 0 ?
		static_cast<float>(totalSameNeighbors) / totalPossibleNeighbors : 0.0f;
}

/*
* @brief 局所的な秩序度を計算
* @return float 秩序度スコア
*/
float Field::calculateLocalOrder() const {
	float orderScore = 0.0f;

	// 水平方向の連続性
	for (int32 y = 0; y < size; ++y) {
		int32 consecutiveCount = 1;
		for (int32 x = 1; x < size; ++x) {
			if (entities[y][x] == entities[y][x - 1]) {
				consecutiveCount++;
			}
			else {
				if (consecutiveCount >= 2) {
					orderScore += consecutiveCount * consecutiveCount; // 二乗で重み付け
				}
				consecutiveCount = 1;
			}
		}
		if (consecutiveCount >= 2) {
			orderScore += consecutiveCount * consecutiveCount;
		}
	}

	// 垂直方向の連続性
	for (int32 x = 0; x < size; ++x) {
		int32 consecutiveCount = 1;
		for (int32 y = 1; y < size; ++y) {
			if (entities[y][x] == entities[y - 1][x]) {
				consecutiveCount++;
			}
			else {
				if (consecutiveCount >= 2) {
					orderScore += consecutiveCount * consecutiveCount;
				}
				consecutiveCount = 1;
			}
		}
		if (consecutiveCount >= 2) {
			orderScore += consecutiveCount * consecutiveCount;
		}
	}

	return orderScore;
}

/*
* @brief 全体的なエントロピー評価値を計算
* @return float エントロピーベースの評価値
*/
float Field::calculateEntropyScore() const {
	float entropyScore = 0.0f;

	// 1. 全体のエンティティ分布エントロピー
	std::map<int32, int32> globalFreqs;
	for (int32 y = 0; y < size; ++y) {
		for (int32 x = 0; x < size; ++x) {
			globalFreqs[entities[y][x]]++;
		}
	}
	float globalEntropy = calculateShannonEntropy(globalFreqs, size * size);
	entropyScore += globalEntropy * EntropyWeight;

	// 2. 局所的な位置エントロピー（4x4ウィンドウ）
	float positionalEntropy = calculatePositionalEntropy(4);
	entropyScore += positionalEntropy * PositionalEntropyWeight;

	// 3. クラスタリング係数（高いほど良い）
	float clustering = calculateClusteringCoefficient();
	entropyScore += clustering * ClusteringBonusWeight;

	// 4. 局所的な秩序度（連続する同じ要素）
	float localOrder = calculateLocalOrder();
	entropyScore += localOrder * LocalOrderWeight;

	return entropyScore;
}

/*
* @brief 修正されたevaluateState関数（エントロピー項を追加）
* @return float 評価値（高いほど良い状態）
*/
float Field::evaluateStateWithEntropy() const {
	// 既存の評価関数の結果を取得
	float baseScore = evaluateState();

	// エントロピーベースの評価を追加
	float entropyScore = calculateEntropyScore();

	// 最終的なスコア
	return baseScore + entropyScore;
}

/*
* @brief Calculate Positional Entropy for a local region
* @param r_start_x Starting X coordinate of the region
* @param r_start_y Starting Y coordinate of the region
* @param r_size Size of the region
* @return float Positional entropy for the region
*/
float Field::calculateLocalPositionalEntropy(int r_start_x, int r_start_y, int r_size) const {
	if (entityCount == 0) return 0.0f;
	std::map<int32, int32> counts;
	int totalCellsInRegion = 0;
	for (int i = 0; i < r_size; ++i) {
		for (int j = 0; j < r_size; ++j) {
			int x = r_start_x + i;
			int y = r_start_y + j;
			if (x < size && y < size) { // Ensure within bounds
				counts[entities[y][x]]++;
				totalCellsInRegion++;
			}
		}
	}

	if (totalCellsInRegion == 0) return 0.0f;

	float entropy = 0.0f;
	for (auto const& [entity, count] : counts) {
		if (count > 0) {
			float p = static_cast<float>(count) / totalCellsInRegion;
			entropy -= p * std::log2(p);
		}
	}
	return entropy;
}

/*
* @brief Calculate Clustering Coefficient for a local region
* @param r_start_x Starting X coordinate of the region
* @param r_start_y Starting Y coordinate of the region
* @param r_size Size of the region
* @return float Clustering coefficient for the region
*/
float Field::calculateLocalClusteringCoefficient(int r_start_x, int r_start_y, int r_size) const {
	if (entityCount == 0) return 0.0f;
	float totalClusteringCoefficient = 0.0f;
	int cellsInRegion = 0;

	for (int i = 0; i < r_size; ++i) {
		for (int j = 0; j < r_size; ++j) {
			int x = r_start_x + i;
			int y = r_start_y + j;

			if (x >= size || y >= size) continue; // Skip if center is out of bounds
			cellsInRegion++;

			int32 currentEntity = entities[y][x];
			if (currentEntity == 0) continue; // Skip empty cells or boundary markers

			int sameTypeNeighbors = 0;
			int totalNeighbors = 0;

			for (int dx = -1; dx <= 1; ++dx) {
				for (int dy = -1; dy <= 1; ++dy) {
					if (dx == 0 && dy == 0) continue;
					int nx = x + dx;
					int ny = y + dy;

					if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
						totalNeighbors++;
						if (entities[ny][nx] == currentEntity) {
							sameTypeNeighbors++;
						}
					}
				}
			}
			if (totalNeighbors > 0) {
				totalClusteringCoefficient += static_cast<float>(sameTypeNeighbors) / totalNeighbors;
			}
		}
	}
	return cellsInRegion > 0 ? totalClusteringCoefficient / cellsInRegion : 0.0f;
}

/*
* @brief Calculate Local Order for a local region
* @param r_start_x Starting X coordinate of the region
* @param r_start_y Starting Y coordinate of the region
* @param r_size Size of the region
* @return float Local order score for the region
*/
float Field::calculateLocalLocalOrder(int r_start_x, int r_start_y, int r_size) const {
	int pairedCells = 0;
	int totalCellsInRegion = 0;
	for (int i = 0; i < r_size; ++i) {
		for (int j = 0; j < r_size; ++j) {
			int x = r_start_x + i;
			int y = r_start_y + j;
			if (x < size && y < size) { // Ensure within bounds
				totalCellsInRegion++;
				if (isPair(x, y)) {
					pairedCells++;
				}
			}
		}
	}
	return totalCellsInRegion > 0 ? static_cast<float>(pairedCells) / totalCellsInRegion : 0.0f;
}

/*
* @brief Calculate Pair Completion for a local region
* @param r_start_x Starting X coordinate of the region
* @param r_start_y Starting Y coordinate of the region
* @param r_size Size of the region
* @return float Pair completion score for the region
*/
float Field::calculateLocalPairCompletion(int r_start_x, int r_start_y, int r_size) const {
	int pairsInRegion = 0;
	int possiblePairsInRegion = 0; // Max pairs if all cells in region were part of a pair within the region

	std::vector<std::vector<bool>> visited(r_size, std::vector<bool>(r_size, false));

	for (int i = 0; i < r_size; ++i) {
		for (int j = 0; j < r_size; ++j) {
			int x = r_start_x + i;
			int y = r_start_y + j;

			if (x >= size || y >= size || entities[y][x] == 0) continue;
			
			possiblePairsInRegion++; // Each non-empty cell could potentially form one end of a pair

			if (visited[i][j]) continue;

			// Check right neighbor (within the local r_size x r_size region)
			if (j + 1 < r_size) { // Internal horizontal edge
				int nx_local = i;
				int ny_local = j + 1;
				int nx_global = r_start_x + nx_local;
				int ny_global = r_start_y + ny_local;

				if (nx_global < size && ny_global < size && entities[y][x] == entities[ny_global][nx_global] && entities[y][x] != 0) {
					pairsInRegion++;
					visited[i][j] = true;
					visited[nx_local][ny_local] = true; 
					continue; 
				}
			}
			// Check bottom neighbor (within the local r_size x r_size region)
			if (i + 1 < r_size) { // Internal vertical edge
				int nx_local = i + 1;
				int ny_local = j;
				int nx_global = r_start_x + nx_local;
				int ny_global = r_start_y + ny_local;
				if (nx_global < size && ny_global < size && entities[y][x] == entities[ny_global][nx_global] && entities[y][x] != 0) {
					pairsInRegion++;
					visited[i][j] = true;
					visited[nx_local][ny_local] = true;
				}
			}
		}
	}
	// possiblePairsInRegion is divided by 2 because each pair involves two cells.
	return (possiblePairsInRegion > 0) ? static_cast<float>(pairsInRegion) / (possiblePairsInRegion / 2.0f) : 0.0f;
}

/*
* @brief Calculate Edge Smoothness for a local region
* @param r_start_x Starting X coordinate of the region
* @param r_start_y Starting Y coordinate of the region
* @param r_size Size of the region
* @return float Edge smoothness score for the region
*/
float Field::calculateLocalEdgeSmoothness(int r_start_x, int r_start_y, int r_size) const {
	int smoothEdges = 0;
	int totalEdges = 0;

	for (int i = 0; i < r_size; ++i) {
		for (int j = 0; j < r_size; ++j) {
			int x = r_start_x + i;
			int y = r_start_y + j;

			if (x >= size || y >= size) continue;

			// Check horizontal edge (with cell to the right)
			if (j + 1 < r_size) { // Internal horizontal edge
				int nx = x;
				int ny = r_start_y + j + 1;
				if (ny < size) {
					totalEdges++;
					if (entities[y][x] == entities[ny][nx] || entities[y][x] == 0 || entities[ny][nx] == 0) {
						smoothEdges++;
					}
				}
			} else if (y + 1 < size) { // Boundary horizontal edge (with cell outside subgrid but inside main grid)
				 totalEdges++;
				 if (entities[y][x] == entities[y+1][x] || entities[y][x] == 0 || entities[y+1][x] == 0) {
					smoothEdges++;
				 }
			}


			// Check vertical edge (with cell below)
			if (i + 1 < r_size) { // Internal vertical edge
				int nx = r_start_x + i + 1;
				int ny = y;
				if (nx < size) {
					totalEdges++;
					if (entities[y][x] == entities[ny][nx] || entities[y][x] == 0 || entities[ny][nx] == 0) {
						smoothEdges++;
					}
				}
			} else if (x + 1 < size) { // Boundary vertical edge
				totalEdges++;
				if (entities[y][x] == entities[y][x+1] || entities[y][x] == 0 || entities[y][x+1] == 0) {
					smoothEdges++;
				}
			}
		}
	}
	return totalEdges > 0 ? static_cast<float>(smoothEdges) / totalEdges : 0.0f;
}

/*
* @brief Calculate entropy difference for a rotation operation
* @param op_x X coordinate of the operation
* @param op_y Y coordinate of the operation
* @param op_size Size of the operation
* @return float Entropy difference due to the rotation
*/
float Field::calculateEntropyDiffForRotation(int op_x, int op_y, int op_size) const {
	// Weights (should be consistent with evaluateStateWithEntropy)
	float w_h = -0.1f; 
	float w_c = 0.2f;  
	float w_lo = 0.4f; 
	float w_pc = 0.5f; 
	float w_es = 0.3f; 

	// 1. Calculate sum of local scores for the region BEFORE rotation
	float old_local_h = calculateLocalPositionalEntropy(op_x, op_y, op_size);
	float old_local_c = calculateLocalClusteringCoefficient(op_x, op_y, op_size);
	float old_local_lo = calculateLocalLocalOrder(op_x, op_y, op_size);
	float old_local_pc = calculateLocalPairCompletion(op_x, op_y, op_size);
	float old_local_es = calculateLocalEdgeSmoothness(op_x, op_y, op_size);
	
	float old_local_score_sum = w_h * old_local_h + w_c * old_local_c + w_lo * old_local_lo + w_pc * old_local_pc + w_es * old_local_es;

	// 2. Create a temporary field, apply rotation, and calculate sum of local scores AFTER rotation
	Field tempField = *this;
	tempField.rotate(op_x, op_y, op_size);

	float new_local_h = tempField.calculateLocalPositionalEntropy(op_x, op_y, op_size);
	float new_local_c = tempField.calculateLocalClusteringCoefficient(op_x, op_y, op_size);
	float new_local_lo = tempField.calculateLocalLocalOrder(op_x, op_y, op_size);
	float new_local_pc = tempField.calculateLocalPairCompletion(op_x, op_y, op_size);
	float new_local_es = tempField.calculateLocalEdgeSmoothness(op_x, op_y, op_size);

	float new_local_score_sum = w_h * new_local_h + w_c * new_local_c + w_lo * new_local_lo + w_pc * new_local_pc + w_es * new_local_es;

	// 3. The difference is the change in score due to the rotation in that local area.
	return new_local_score_sum - old_local_score_sum;
}
