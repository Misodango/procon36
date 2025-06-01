#include "Field.h"
#include <Siv3D.hpp> // Siv3Dの機能を使用
#include <chrono>    // For seeding RNG
#include <cmath>     // For Euclidean distance calculations
#include <bitset>

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
	for (int32 y_coord = 0; y_coord < size; ++y_coord) {
		for (int32 x_coord = 0; x_coord < size; ++x_coord) {
			int32 entity = entities[y_coord][x_coord];
			if (entity > 0) { // 0は空白なので無視
				entityPositions[entity].push_back({ x_coord, y_coord });
			}
		}
	}

	float sumEuclideanDistUnformed = 0.0f;

	// 各エンティティペアの「回転距離」を評価
	for (const auto& [entity, positions_vec] : entityPositions) {
		// 同じ数字が2つあるはず（ペアになる条件）
		if (positions_vec.size() == 2) {
			auto [x1, y1] = positions_vec[0];
			auto [x2, y2] = positions_vec[1];

			// 既にペアになっている場合はスコア加算（隣接している）
			if (abs(x1 - x2) + abs(y1 - y2) == 1) {
				score += ScoreFactorFormedPairBonus;
				// For already formed pairs, Euclidean distance is 1.0, no penalty.
				continue;
			}

			// Calculate Euclidean distance for unformed pairs
			float dx_f = static_cast<float>(x1 - x2);
			float dy_f = static_cast<float>(y1 - y2);
			sumEuclideanDistUnformed += std::sqrt(dx_f * dx_f + dy_f * dy_f);


			// 回転距離の計算：
			// 1. マンハッタン距離の逆数をベースにする
			int32 manhattanDist = abs(x1 - x2) + abs(y1 - y2);

			// マンハッタン距離が小さいほど良い（大きいほど悪い）
			float distanceScore = ScoreFactorUnformedPairBase / static_cast<float>(manhattanDist + 1);
			score += distanceScore;

			// 同じ行または列にある場合はボーナス
			if (x1 == x2 || y1 == y2) {
				score += ScoreFactorUnformedPairSameRowColBonus;
			}
			// 対角線上にある場合は小さなボーナス
			else if (abs(x1 - x2) == abs(y1 - y2)) {
				score += ScoreFactorUnformedPairDiagonalBonus;
			}

			// 推定回転数に基づくペナルティ
			// より複雑な位置関係は回転数が多くなる傾向があるため
			int32 estimatedRotations = std::max(abs(x1 - x2), abs(y1 - y2));
			score -= estimatedRotations * ScoreFactorUnformedPairEstRotPenalty;
		}
	}

	// 4. ユークリッド距離による評価 - 距離が短いほど良い
	score -= sumEuclideanDistUnformed * ScoreFactorEuclideanDistance;

	// 5. 2x2パターンの評価（局所的な完成度を見る）
	int32 count2x2Patterns = 0;
	for (int32 y = 0; y <= size - 2; ++y) {
		for (int32 x = 0; x <= size - 2; ++x) {
			// 2x2領域内でペアが形成されているかチェック
			int32 pairsIn2x2 = 0;
			std::unordered_set<int32> seen;
			for (int32 dy = 0; dy < 2; ++dy) {
				for (int32 dx = 0; dx < 2; ++dx) {
					int32 entity = entities[y + dy][x + dx];
					if (entity > 0 && seen.find(entity) == seen.end() && isPair(x + dx, y + dy)) {
						pairsIn2x2++;
						seen.insert(entity);
					}
				}
			}
			if (pairsIn2x2 >= 2) { // 2x2領域内に2つ以上のペアがある場合
				count2x2Patterns++;
			}
		}
	}
	score += count2x2Patterns * ScoreFactor2x2Pattern;

	// 6. 未ペアエンティティのマンハッタン距離ペナルティ
	for (const auto& [entity, positions_vec] : entityPositions) {
		if (positions_vec.size() == 2) {
			auto [x1, y1] = positions_vec[0];
			auto [x2, y2] = positions_vec[1];

			// 未ペアの場合のみペナルティを適用
			if (abs(x1 - x2) + abs(y1 - y2) != 1) {
				int32 manhattanDist = abs(x1 - x2) + abs(y1 - y2);
				score -= manhattanDist * ManhattanPenaltyFactorUnpaired;
			}
		}
	}

	return score;
}

// 追加のヘルパーメソッドの実装

/*
* @brief 指定した領域内の隣接ペア数を効率的に計算
* @param startX 開始X座標
* @param startY 開始Y座標
* @param endX 終了X座標
* @param endY 終了Y座標
* @return 領域内の隣接ペア数
*/
int32 Field::countPairsInRegion(int32 startX, int32 startY, int32 endX, int32 endY) const {
	int32 pairCount = 0;
	std::unordered_set<int32> seen;

	for (int32 y = startY; y < endY; ++y) {
		for (int32 x = startX; x < endX; ++x) {
			int32 entity = entities[y][x];
			if (entity > 0 && seen.find(entity) == seen.end() && isPair(x, y)) {
				pairCount++;
				seen.insert(entity);
			}
		}
	}

	return pairCount;
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

/*
* @brief 最適化された差分計算（影響を受けるエンティティのみを処理）
* @param x X座標
* @param y Y座標
* @param n 回転サイズ
* @return ペア数の差分とスコアの差分
*/
std::pair<int32, float> Field::rotateAndGetDiffOptimized(int32 x, int32 y, int32 n) {
	// 影響を受ける可能性のあるエンティティを特定
	std::unordered_set<int32> affectedEntities;

	// 回転領域 + 1セル周囲の範囲
	const int32 checkStartX = std::max(0, x - 1);
	const int32 checkStartY = std::max(0, y - 1);
	const int32 checkEndX = std::min(size, x + n + 1);
	const int32 checkEndY = std::min(size, y + n + 1);

	// 影響範囲内のエンティティを収集
	for (int32 cy = checkStartY; cy < checkEndY; ++cy) {
		for (int32 cx = checkStartX; cx < checkEndX; ++cx) {
			affectedEntities.insert(entities[cy][cx]);
		}
	}

	// 回転前の状態を記録
	int32 beforePairs = 0;
	float beforeScore = 0.0f;
	float beforeEuclideanSum = 0.0f;
	Array<bool> checkedEntities;
	for (int32 entity : affectedEntities) {
		if (!checkedEntities[entity] && isEntityPaired(entity)) {
			beforePairs++;
			beforeScore += ScoreFactorFormedPairBonus;
			checkedEntities[entity] = true; // このエンティティは既にチェック済み
		}
		float distance = getEntityEuclideanDistance(entity);
		if (distance >= 0) {
			beforeEuclideanSum += distance;
		}
	}

	// 回転実行
	rotate(x, y, n);

	// 回転後の状態を計算
	int32 afterPairs = 0;
	float afterScore = 0.0f;
	float afterEuclideanSum = 0.0f;

	for (int32 entity : affectedEntities) {
		if (isEntityPaired(entity)) {
			afterPairs++;
			afterScore += ScoreFactorFormedPairBonus;
		}
		float distance = getEntityEuclideanDistance(entity);
		if (distance >= 0) {
			afterEuclideanSum += distance;
		}
	}

	// 差分を計算
	int32 pairDiff = afterPairs - beforePairs;
	float scoreDiff = (afterScore - beforeScore) +
		(beforeEuclideanSum - afterEuclideanSum) * ScoreFactorEuclideanDistance;

	return { pairDiff, scoreDiff };
}

/*
* @brief 回転操作の事前評価（実際に回転せずに効果を予測）
* @param x X座標
* @param y Y座標
* @param n 回転サイズ
* @return 予測されるペア数の差分とスコアの差分
*/
std::pair<int32, float> Field::previewRotationEffect(int32 x, int32 y, int32 n) const {
	if (!isValidRotation(x, y, n)) {
		return { 0, 0.0f };
	}

	// 一時的なフィールドを作成して回転をシミュレート
	Field tempField = *this;
	return tempField.rotateAndGetDiffOptimized(x, y, n);
}

/*
* @brief 最も効果的な回転操作を見つける
* @param maxRotationSize 最大回転サイズ
* @return 最も効果的な回転操作の情報 (x, y, n, 予想スコア差分)
*/
std::tuple<int32, int32, int32, float> Field::findBestRotation(int32 maxRotationSize) const {
	if (maxRotationSize <= 0) maxRotationSize = size;

	int32 bestX = -1, bestY = -1, bestN = -1;
	float bestScore = std::numeric_limits<float>::lowest();

	for (int32 cy = 0; cy <= size - 2; ++cy) {
		for (int32 cx = 0; cx <= size - 2; ++cx) {
			int32 maxN = std::min({ maxRotationSize, size - cx, size - cy });
			for (int32 n = 2; n <= maxN; ++n) {
				auto [pairDiff, scoreDiff] = previewRotationEffect(cx, cy, n);
				if (scoreDiff > bestScore) {
					bestScore = scoreDiff;
					bestX = cx;
					bestY = cy;
					bestN = n;
				}
			}
		}
	}

	return { bestX, bestY, bestN, bestScore };
}

/*
* @brief デバッグ用：フィールドの状態を出力
*/
void Field::debugPrint() const {
	Print << U"Field size: " << size;
	Print << U"Entity count: " << entityCount;
	Print << U"Current pairs: " << countPairs();
	Print << U"Total Euclidean distance: " << getTotalEuclideanDistance();
	Print << U"Evaluation score: " << evaluateState();

	// エンティティ別の状態を表示
	for (int32 entity = 1; entity <= entityCount; ++entity) {
		auto positions = getEntityPositions(entity);
		if (positions.size() == 2) {
			bool paired = isEntityPaired(entity);
			float distance = getEntityEuclideanDistance(entity);
			Print << U"Entity " << entity << U": "
				<< U"(" << positions[0].first << U"," << positions[0].second << U") - "
				<< U"(" << positions[1].first << U"," << positions[1].second << U") "
				<< U"Paired: " << (paired ? U"Yes" : U"No")
				<< U" Distance: " << distance;
		}
	}
}

///*
//* @brief キャッシュフレンドリーな実装
//* @param x X座標
//* @param y Y座標
//* @param n 回転サイズ
//* @return ペア数の差分とスコアの差分
//*/
//std::pair<int32, float> Field::rotateAndGetDiffCacheFriendly(int32 x, int32 y, int32 n) {
//	// 小さい固定サイズの構造体を使用
//	struct CompactEntityState {
//		int32 entity;
//		int16_t x1, y1, x2, y2;  // 座標を16bitに圧縮
//		float euclideanDist;
//		bool isPaired;
//
//		CompactEntityState() : entity(0), euclideanDist(0.0f), isPaired(false) {}
//	};
//
//	// スタックアロケーションで高速化
//	std::array<CompactEntityState, 64> beforeStates;  // 通常64個以下で十分
//	int32 stateCount = 0;
//
//	// 影響を受けるエンティティの状態を記録
//	std::unordered_set<int32> processed;
//
//	// 回転領域 + 境界を一度にスキャン
//	const int32 scanStartX = std::max(0, x - 1);
//	const int32 scanStartY = std::max(0, y - 1);
//	const int32 scanEndX = Min(size, x + n + 1);
//	const int32 scanEndY = Min(size, y + n + 1);
//
//	for (int32 cy = scanStartY; cy < scanEndY; ++cy) {
//		for (int32 cx = scanStartX; cx < scanEndX; ++cx) {
//			int32 entity = entities[cy][cx];
//			if (entity != 0 && processed.find(entity) == processed.end()) {
//				processed.insert(entity);
//
//				auto positions = findEntityPositions(entity, entities, size);
//				if (positions.size() == 2 && stateCount < 64) {
//					CompactEntityState& state = beforeStates[stateCount++];
//					state.entity = entity;
//					state.x1 = static_cast<int16_t>(positions[0].first);
//					state.y1 = static_cast<int16_t>(positions[0].second);
//					state.x2 = static_cast<int16_t>(positions[1].first);
//					state.y2 = static_cast<int16_t>(positions[1].second);
//					state.isPaired = (abs(state.x1 - state.x2) + abs(state.y1 - state.y2) == 1);
//
//					float dx = static_cast<float>(state.x1 - state.x2);
//					float dy = static_cast<float>(state.y1 - state.y2);
//					state.euclideanDist = std::sqrt(dx * dx + dy * dy);
//				}
//			}
//		}
//	}
//
//	// 回転実行
//	rotate(x, y, n);
//
//	// 差分計算
//	int32 pairDiff = 0;
//	float euclideanDiff = 0.0f;
//	float formedPairScoreDiff = 0.0f;
//
//	for (int32 i = 0; i < stateCount; ++i) {
//		const CompactEntityState& beforeState = beforeStates[i];
//		auto positions = findEntityPositions(beforeState.entity, entities, size);
//
//		if (positions.size() == 2) {
//			bool isNowPaired = (abs(positions[0].first - positions[1].first) +
//							  abs(positions[0].second - positions[1].second) == 1);
//
//			float dx = static_cast<float>(positions[0].first - positions[1].first);
//			float dy = static_cast<float>(positions[0].second - positions[1].second);
//			float newEuclideanDist = std::sqrt(dx * dx + dy * dy);
//
//			// 変化のみを計算
//			if (isNowPaired != beforeState.isPaired) {
//				pairDiff += isNowPaired ? 1 : -1;
//				formedPairScoreDiff += isNowPaired ? ScoreFactorFormedPairBonus : -ScoreFactorFormedPairBonus;
//			}
//
//			euclideanDiff += (beforeState.euclideanDist - newEuclideanDist);
//		}
//	}
//
//	float totalScoreDiff = formedPairScoreDiff + euclideanDiff * ScoreFactorEuclideanDistance;
//	return { pairDiff, totalScoreDiff };
//}

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


// 最適化されたrotateAndGetDiff関数群の実装

#include <array>
#include <bit>

// SIMD最適化用のエンティティ状態構造体
struct alignas(32) OptimizedEntityState {
	int32 entity;
	int16_t x1, y1, x2, y2;  // 座標（16bit圧縮）
	float euclideanDist;
	uint8_t isPaired;
	uint8_t padding[7];      // アライメント調整

	OptimizedEntityState() : entity(0), euclideanDist(0.0f), isPaired(0) {}
};

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

/*
* @brief 超高速版差分計算（SIMD最適化）
* @param x X座標
* @param y Y座標
* @param n 回転サイズ
* @return ペア数の差分とスコアの差分
*/
std::pair<int32, float> Field::rotateAndGetDiffUltraFast(int32 x, int32 y, int32 n) {
	// 早期リターンのチェック
	if (!isValidRotation(x, y, n)) {
		return { 0, 0.0f };
	}

	// 影響範囲の事前計算（境界チェック込み）
	const int32 scanStartX = std::max(0, x - 1);
	const int32 scanStartY = std::max(0, y - 1);
	const int32 scanEndX = std::min(size, x + n + 1);
	const int32 scanEndY = std::min(size, y + n + 1);

	// === Phase 1: 影響を受けるエンティティを特定 ===
	std::unordered_set<int32> affectedEntities;

	// 回転領域とその周囲のエンティティを収集
	for (int32 cy = scanStartY; cy < scanEndY; ++cy) {
		const int32* row = &entities[cy][0];
		for (int32 cx = scanStartX; cx < scanEndX; ++cx) {
			if (row[cx] != 0) {
				affectedEntities.insert(row[cx]);
			}
		}
	}

	// === Phase 2: 回転前の状態を記録 ===
	int32 beforePairs = 0;
	float beforeScoreFormedPairs = 0.0f;
	float sumEuclideanDistBefore = 0.0f;

	std::bitset<256> processed;
	std::array<OptimizedEntityState, 64> beforeStates;
	int32 stateCount = 0;

	for (int32 entity : affectedEntities) {
		if (entity <= 0 || entity >= 256) continue; // 安全チェック

		std::vector<std::pair<int32, int32>> positions;
		positions.reserve(2);

		// エンティティの位置を効率的に検索
		for (int32 findY = 0; findY < size && positions.size() < 2; ++findY) {
			const int32* findRow = &entities[findY][0];
			for (int32 findX = 0; findX < size && positions.size() < 2; ++findX) {
				if (findRow[findX] == entity) {
					positions.emplace_back(findX, findY);
				}
			}
		}

		if (positions.size() == 2 && stateCount < 64) {
			OptimizedEntityState& state = beforeStates[stateCount++];
			state.entity = entity;
			state.x1 = static_cast<int16_t>(positions[0].first);
			state.y1 = static_cast<int16_t>(positions[0].second);
			state.x2 = static_cast<int16_t>(positions[1].first);
			state.y2 = static_cast<int16_t>(positions[1].second);

			// マンハッタン距離が1の場合、ペアとみなす
			int32 manhattanDist = std::abs(state.x1 - state.x2) + std::abs(state.y1 - state.y2);
			state.isPaired = (manhattanDist == 1) ? 1 : 0;

			// ペアの場合、スコアを加算
			if (state.isPaired) {
				beforePairs++;
				beforeScoreFormedPairs += ScoreFactorFormedPairBonus;
			}

			// ユークリッド距離を計算
			float dx = static_cast<float>(state.x1 - state.x2);
			float dy = static_cast<float>(state.y1 - state.y2);
			state.euclideanDist = std::sqrt(dx * dx + dy * dy);
			sumEuclideanDistBefore += state.euclideanDist;
		}
	}

	// === Phase 3: 回転実行 ===
	rotate(x, y, n);

	// === Phase 4: 回転後の状態を計算 ===
	int32 afterPairs = 0;
	float afterScoreFormedPairs = 0.0f;
	float sumEuclideanDistAfter = 0.0f;

	for (int32 i = 0; i < stateCount; ++i) {
		const OptimizedEntityState& beforeState = beforeStates[i];

		// 回転後の位置を再検索
		std::vector<std::pair<int32, int32>> newPositions;
		newPositions.reserve(2);

		for (int32 findY = 0; findY < size && newPositions.size() < 2; ++findY) {
			const int32* findRow = &entities[findY][0];
			for (int32 findX = 0; findX < size && newPositions.size() < 2; ++findX) {
				if (findRow[findX] == beforeState.entity) {
					newPositions.emplace_back(findX, findY);
				}
			}
		}

		if (newPositions.size() == 2) {
			// マンハッタン距離を計算
			int32 manhattanDist = std::abs(newPositions[0].first - newPositions[1].first) +
				std::abs(newPositions[0].second - newPositions[1].second);
			bool isNowPaired = (manhattanDist == 1);

			// ペアの場合、スコアを加算
			if (isNowPaired) {
				afterPairs++;
				afterScoreFormedPairs += ScoreFactorFormedPairBonus;
			}

			// ユークリッド距離を計算
			float dx = static_cast<float>(newPositions[0].first - newPositions[1].first);
			float dy = static_cast<float>(newPositions[0].second - newPositions[1].second);
			float newEuclideanDist = std::sqrt(dx * dx + dy * dy);
			sumEuclideanDistAfter += newEuclideanDist;
		}
	}

	// === Phase 5: 差分を計算 ===
	int32 pairDiff = afterPairs - beforePairs;
	float formedPairScoreDiff = afterScoreFormedPairs - beforeScoreFormedPairs;
	float euclideanScoreDiff = (sumEuclideanDistBefore - sumEuclideanDistAfter) * ScoreFactorEuclideanDistance;

	return { pairDiff, formedPairScoreDiff + euclideanScoreDiff };
}

// キャッシュ管理クラス
class RotationCache {
private:
	struct CacheKey {
		uint64_t fieldHash;
		int16_t x, y, n;

		bool operator==(const CacheKey& other) const {
			return fieldHash == other.fieldHash && x == other.x && y == other.y && n == other.n;
		}
	};

	struct CacheKeyHash {
		size_t operator()(const CacheKey& key) const {
			return key.fieldHash ^ (static_cast<size_t>(key.x) << 16) ^
				(static_cast<size_t>(key.y) << 8) ^ static_cast<size_t>(key.n);
		}
	};

	std::unordered_map<CacheKey, std::pair<int32, float>, CacheKeyHash> cache;
	static constexpr size_t MAX_CACHE_SIZE = 10000;

public:
	bool tryGet(uint64_t fieldHash, int32 x, int32 y, int32 n, std::pair<int32, float>& result) {
		CacheKey key{ fieldHash, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(n) };
		auto it = cache.find(key);
		if (it != cache.end()) {
			result = it->second;
			return true;
		}
		return false;
	}

	void store(uint64_t fieldHash, int32 x, int32 y, int32 n, const std::pair<int32, float>& result) {
		if (cache.size() >= MAX_CACHE_SIZE) {
			cache.clear(); // 単純なキャッシュクリア（必要に応じてLRU実装も可）
		}
		CacheKey key{ fieldHash, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(n) };
		cache[key] = result;
	}
};

/*
* @brief キャッシュ付き高速版差分計算
* @param x X座標
* @param y Y座標
* @param n 回転サイズ
* @return ペア数の差分とスコアの差分
*/
std::pair<int32, float> Field::rotateAndGetDiffCached(int32 x, int32 y, int32 n) {
	static RotationCache cache;

	uint64_t currentHash = computeHash();
	std::pair<int32, float> cachedResult;

	if (cache.tryGet(currentHash, x, y, n, cachedResult)) {
		// キャッシュヒット - 実際に回転を実行
		rotate(x, y, n);
		return cachedResult;
	}

	// キャッシュミス - 計算して保存
	std::pair<int32, float> result = rotateAndGetDiffUltraFast(x, y, n);
	cache.store(currentHash, x, y, n, result);

	return result;
}

/*
* @brief バッチ処理版（複数の回転を一度に評価）
* @param rotations 評価する回転操作のリスト
* @return 各回転の差分スコアのリスト
*/
std::vector<std::pair<int32, float>> Field::evaluateMultipleRotations(
	const std::vector<std::tuple<int32, int32, int32>>& rotations) {

	std::vector<std::pair<int32, float>> results;
	results.reserve(rotations.size());

	// 元の状態を保存
	Field originalField = *this;

	for (const auto& [x, y, n] : rotations) {
		// 元の状態に復元
		*this = originalField;

		// 評価実行
		results.push_back(rotateAndGetDiffUltraFast(x, y, n));
	}

	// 最後に元の状態に戻す
	*this = originalField;

	return results;
}

/*
* @brief キャッシュフレンドリーな実装の改良版
* @param x X座標
* @param y Y座標
* @param n 回転サイズ
* @return ペア数の差分とスコアの差分
*/
std::pair<int32, float> Field::rotateAndGetDiffCacheFriendly(int32 x, int32 y, int32 n) {
	// 境界チェック - 早期リターン
	if (!isValidRotation(x, y, n)) {
		return { 0, 0.0f };
	}

	// 影響範囲を計算
	const int32 scanStartX = std::max(0, x - 1);
	const int32 scanStartY = std::max(0, y - 1);
	const int32 scanEndX = std::min(size, x + n + 1);
	const int32 scanEndY = std::min(size, y + n + 1);

	// 小さい固定サイズの構造体をスタック上に確保
	struct CompactEntityState {
		int32 entity;
		int16_t x1, y1, x2, y2;
		float euclideanDist;
		bool isPaired;
	};

	// スタックアロケーションで高速化
	std::array<CompactEntityState, 64> beforeStates;
	int32 stateCount = 0;

	// 影響を受けるエンティティを追跡
	std::bitset<256> processed; // 高速ビットセット

	// 回転前の状態を収集 - キャッシュフレンドリーなスキャン
	for (int32 cy = scanStartY; cy < scanEndY; ++cy) {
		const int32* row = &entities[cy][0]; // 行キャッシュ
		for (int32 cx = scanStartX; cx < scanEndX; ++cx) {
			int32 entity = row[cx];

			if (entity > 0 && !processed[entity] && stateCount < 64) {
				processed[entity] = true;

				// 効率的な位置検索
				std::vector<std::pair<int32, int32>> positions;
				positions.reserve(2);

				// 高速位置検索
				for (int32 ey = 0; ey < size && positions.size() < 2; ++ey) {
					const int32* entityRow = &entities[ey][0];
					for (int32 ex = 0; ex < size && positions.size() < 2; ++ex) {
						if (entityRow[ex] == entity) {
							positions.emplace_back(ex, ey);
						}
					}
				}

				if (positions.size() == 2) {
					CompactEntityState& state = beforeStates[stateCount++];
					state.entity = entity;
					state.x1 = static_cast<int16_t>(positions[0].first);
					state.y1 = static_cast<int16_t>(positions[0].second);
					state.x2 = static_cast<int16_t>(positions[1].first);
					state.y2 = static_cast<int16_t>(positions[1].second);

					// マンハッタン距離=1がペア
					state.isPaired = (std::abs(state.x1 - state.x2) + std::abs(state.y1 - state.y2) == 1);

					// ユークリッド距離計算
					float dx = static_cast<float>(state.x1 - state.x2);
					float dy = static_cast<float>(state.y1 - state.y2);
					state.euclideanDist = std::sqrt(dx * dx + dy * dy);
				}
			}
		}
	}

	// 回転実行
	rotate(x, y, n);

	// 差分計算
	int32 pairDiff = 0;
	float euclideanDiff = 0.0f;
	float formedPairScoreDiff = 0.0f;

	// 最適化されたループ - メモリアクセスパターンを意識
	for (int32 i = 0; i < stateCount; ++i) {
		const CompactEntityState& beforeState = beforeStates[i];

		// 回転後の位置を再検索
		std::vector<std::pair<int32, int32>> newPositions;
		newPositions.reserve(2);

		for (int32 ey = 0; ey < size && newPositions.size() < 2; ++ey) {
			const int32* entityRow = &entities[ey][0];
			for (int32 ex = 0; ex < size && newPositions.size() < 2; ++ex) {
				if (entityRow[ex] == beforeState.entity) {
					newPositions.emplace_back(ex, ey);
				}
			}
		}

		if (newPositions.size() == 2) {
			bool isNowPaired = (std::abs(newPositions[0].first - newPositions[1].first) +
							  std::abs(newPositions[0].second - newPositions[1].second) == 1);

			float dx = static_cast<float>(newPositions[0].first - newPositions[1].first);
			float dy = static_cast<float>(newPositions[0].second - newPositions[1].second);
			float newEuclideanDist = std::sqrt(dx * dx + dy * dy);

			// 変化のみを計算
			if (isNowPaired != beforeState.isPaired) {
				pairDiff += isNowPaired ? 1 : -1;
				formedPairScoreDiff += isNowPaired ? ScoreFactorFormedPairBonus : -ScoreFactorFormedPairBonus;
			}

			euclideanDiff += (beforeState.euclideanDist - newEuclideanDist);
		}
	}

	float totalScoreDiff = formedPairScoreDiff + euclideanDiff * ScoreFactorEuclideanDistance;
	return { pairDiff, totalScoreDiff };
}

/*
* @brief プロファイリング用のベンチマーク関数
* @param iterations ベンチマーク反復回数
*/
void Field::benchmarkRotateDiff(int32 iterations) {
	auto start = std::chrono::high_resolution_clock::now();

	Field originalField = *this;

	for (int32 i = 0; i < iterations; ++i) {
		// ランダムな回転をテスト
		int32 x = rand() % (size - 1);
		int32 y = rand() % (size - 1);
		int32 n = 2 + rand() % (size - std::max(x, y) - 1);

		rotateAndGetDiffUltraFast(x, y, n);

		// フィールドをリセット
		*this = originalField;
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	Print << U"Benchmark: " << iterations << U" iterations in "
		<< duration.count() << U" microseconds";
	Print << U"Average: " << (duration.count() / iterations) << U" microseconds per call";
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

	// 2. 局所的な位置エントロピー（3x3ウィンドウ）
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
