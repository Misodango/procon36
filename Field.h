#pragma once

#include <Siv3D.hpp> // 必要なヘッダファイルをインクルード
#include "Solution.h"

class Field {
public:
	// フィールドのサイズ
	int32 size;

	// エンティティの数
	int32 entityCount;

	// エンティティの値を保持する2次元配列
	Grid<int32> entities;

	// 描画用の色
	static Array<Color> colors;

	// コンストラクタでフィールドを初期化
	Field(int32 size);

	Field(int32 size, bool isComplete, int32 seed = -1);

	// コピーコンストラクタ
	Field(const Field& other);

	// コピー代入演算子
	Field& operator=(const Field& other);

	// ランダムにフィールドを生成する関数
	static Field random(int32 size);

	static Field fromJSON(const JSON& json);

	static Field fromPath(const FilePath& path);

	// 指定した範囲を右方向に90度回転させる関数
	void rotate(int32 x, int32 y, int32 n);

	// 指定した範囲を左方向に90度回転させる関数
	void rotateReverse(int32 x, int32 y, int32 n);

	// フィールドを描画する関数
	void draw() const;

	// フィールドのペアを数える
	int32 countPairs() const;

	// 左上から連続のペアを数える
	int32 countPairsFromTopLeftHorizontal() const;

	// 左上から連続のペアを数える
	int32 countPairsFromTopLeftVertical() const;

	// 差分更新
	std::pair<int32, float> rotateAndGetDiff(int32 x, int32 y, int32 n);

	// フィールドのサイズを取得
	int32 getSize() const;

	// 合法手を取得
	Array<Solution> getLegalMoves() const;

	// ペアのマスかどうかを判定
	bool isPair(int32 x, int32 y) const;

	// 右にペアがあるか判定
	bool hasPairRight(int32 x, int32 y) const;

	// 左にペアがあるか判定
	bool hasPairLeft(int32 x, int32 y) const;

	// 下にペアがあるか判定
	bool hasPairDown(int32 x, int32 y) const;

	// 上にペアがあるか判定
	bool hasPairUp(int32 x, int32 y) const;

	// 自信が右のペアか判定
	bool isPairRight(int32 x, int32 y) const;

	// 自信が左のペアか判定
	bool isPairLeft(int32 x, int32 y) const;

	// 自信が下のペアか判定
	bool isPairDown(int32 x, int32 y) const;

	// 自信が上のペアか判定
	bool isPairUp(int32 x, int32 y) const;

	// 終了判定
	bool isFinished() const;

	// フィールドの評価値を計算する（ビームサーチ用）
	float evaluateState() const;
	float evaluateStateWithEntropy() const;
	float calculateEntropyDiffForRotation(int op_x, int op_y, int op_size) const;

	// Zobrist Hashing related members
	static std::vector<std::vector<std::vector<uint64_t>>> zobristTable;
	static bool zobristTableInitialized;
	static std::mt19937_64 rng; // For random number generation

	// Zobrist Hashing initialization
	static void initializeZobristTable(int32 maxSize, int32 maxEntityValuePlusOne);

	// Standard hash (previous implementation)
	size_t computeStdHash() const;

	// フィールドをハッシュ値に変換 (Now Zobrist hash)
	size_t computeHash() const;

	// エンティティ情報取得メソッド
	std::vector<std::pair<int32, int32>> getEntityPositions(int32 entity) const;
	bool isEntityPaired(int32 entity) const;
	float getEntityEuclideanDistance(int32 entity) const;
	float getTotalEuclideanDistance() const;
	float getNearestSameEntityDistance(int32 x, int32 y) const;

	bool isValidRotation(int32 x, int32 y, int32 n) const;

	float calculateShannonEntropy(const std::map<int32, int32>& frequencies, int32 totalCount) const;
	float calculatePositionalEntropy(int32 windowSize = 3) const;
	float calculateClusteringCoefficient() const;
	float calculateLocalOrder() const;

	float calculateEntropyScore() const;

	Field() = default;

	// 学習データ生成
	// ランダムに揃った盤面
	Array<Field> generateRandomCompleteFieldsAll();

private:
	// Helper methods for calculateEntropyDiffForRotation
	float calculateLocalPositionalEntropy(int r_start_x, int r_start_y, int r_size) const;
	float calculateLocalClusteringCoefficient(int r_start_x, int r_start_y, int r_size) const;
	float calculateLocalLocalOrder(int r_start_x, int r_start_y, int r_size) const;
	float calculateLocalPairCompletion(int r_start_x, int r_start_y, int r_size) const;
	float calculateLocalEdgeSmoothness(int r_start_x, int r_start_y, int r_size) const;
};
