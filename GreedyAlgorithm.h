#pragma once
#include "Field.h"
#include "Solution.h"

class GreedyAlgorithm
{
public:
	// GreedyAlgorithmのコンストラクタ
	GreedyAlgorithm(const Field& field);

	// GreedyAlgorithmの実行
	Solution run();

	// フィールド全体のペアの数をカウント
	int32 countPairs() const;

private:
	// フィールド
	Field m_field;
};
