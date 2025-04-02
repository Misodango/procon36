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

private:
	// フィールド
	Field m_field;
};
