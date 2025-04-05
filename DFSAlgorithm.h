#pragma once
#include "Field.h"
#include "Solution.h"

class DFSAlgorithm
{
public:
	// DFSAlgorithmのコンストラクタ
	DFSAlgorithm(const Field& field);

	// DFSAlgorithmの実行
	Solution run();

private:
	// フィールド
	Field m_field;
};
