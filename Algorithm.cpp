#include "Algorithm.h"
#include "GreedyAlgorithm.h"
#include "DFSAlgorithm.h"
#include "Solution.h"
#include "Field.h"

/*
* @brief Algorithmのコンストラクタ
* @param field フィールド
* @return Algorithm
*/

Algorithm::Algorithm(const Field& field) : m_field(field) {}

/*
* @brief Algorithmの実行
* @param type 解法の種類
* @return Solution
*/

Solution Algorithm::run(const Solution::Type type)
{
	Solution solution;

	switch (type)
	{
	case Solution::Type::Greedy:
	{
		GreedyAlgorithm greedyAlgorithm(m_field);
		solution = greedyAlgorithm.run();
		break;
	}
	case Solution::Type::DFS:
	{
		DFSAlgorithm dfsAlgorithm(m_field);
		solution = dfsAlgorithm.run();
		break;
	}
	default:
		break;
	}
	
	return solution;
}
