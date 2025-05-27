#include "Field.h"
#include <Siv3D.hpp> // Siv3Dの機能を使用
#include <chrono>    // For seeding RNG
#include <cmath>     // For Euclidean distance calculations

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

/*
* @brief キャッシュフレンドリーな実装
* @param x X座標
* @param y Y座標
* @param n 回転サイズ
* @return ペア数の差分とスコアの差分
*/
std::pair<int32, float> Field::rotateAndGetDiffCacheFriendly(int32 x, int32 y, int32 n) {
	// 小さい固定サイズの構造体を使用
	struct CompactEntityState {
		int32 entity;
		int16_t x1, y1, x2, y2;  // 座標を16bitに圧縮
		float euclideanDist;
		bool isPaired;

		CompactEntityState() : entity(0), euclideanDist(0.0f), isPaired(false) {}
	};

	// スタックアロケーションで高速化
	std::array<CompactEntityState, 64> beforeStates;  // 通常64個以下で十分
	int32 stateCount = 0;

	// 影響を受けるエンティティの状態を記録
	std::unordered_set<int32> processed;

	// 回転領域 + 境界を一度にスキャン
	const int32 scanStartX = std::max(0, x - 1);
	const int32 scanStartY = std::max(0, y - 1);
	const int32 scanEndX = Min(size, x + n + 1);
	const int32 scanEndY = Min(size, y + n + 1);

	for (int32 cy = scanStartY; cy < scanEndY; ++cy) {
		for (int32 cx = scanStartX; cx < scanEndX; ++cx) {
			int32 entity = entities[cy][cx];
			if (entity != 0 && processed.find(entity) == processed.end()) {
				processed.insert(entity);

				auto positions = findEntityPositions(entity, entities, size);
				if (positions.size() == 2 && stateCount < 64) {
					CompactEntityState& state = beforeStates[stateCount++];
					state.entity = entity;
					state.x1 = static_cast<int16_t>(positions[0].first);
					state.y1 = static_cast<int16_t>(positions[0].second);
					state.x2 = static_cast<int16_t>(positions[1].first);
					state.y2 = static_cast<int16_t>(positions[1].second);
					state.isPaired = (abs(state.x1 - state.x2) + abs(state.y1 - state.y2) == 1);

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

	for (int32 i = 0; i < stateCount; ++i) {
		const CompactEntityState& beforeState = beforeStates[i];
		auto positions = findEntityPositions(beforeState.entity, entities, size);

		if (positions.size() == 2) {
			bool isNowPaired = (abs(positions[0].first - positions[1].first) +
							  abs(positions[0].second - positions[1].second) == 1);

			float dx = static_cast<float>(positions[0].first - positions[1].first);
			float dy = static_cast<float>(positions[0].second - positions[1].second);
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
