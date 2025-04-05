#pragma once
#include "Field.h"
#include "Solution.h"

class BFSAlgorithm
{
public:
	// DFSAlgorithmのコンストラクタ
	BFSAlgorithm(const Field& field);

	// DFSAlgorithmの実行
	Solution run();

private:
	// フィールド
	Field m_field;
};
