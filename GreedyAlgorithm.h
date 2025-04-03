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

	// 先読みDFS
	Solution runDFS(int32 depth);

private:
	// フィールド
	Field m_field;
};
