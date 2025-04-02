#pragma once
#include "Field.h"
#include "Solution.h"

class Algorithm
{
public:
	// Algorithmのコンストラクタ
	Algorithm(const Field& field);

	// Algorithmの実行
	Solution run(const Solution::Type type);

private:
	Field m_field;
};
