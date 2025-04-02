#include "Algorithm.h"
#include "GreedyAlgorithm.h"

/*
* @brief Algorithmのコンストラクタ
* @param field フィールド
* @return Algorithm
*/

Algorithm::Algorithm(const Field& field) : m_field(field) {}



Solution Algorithm::run(const Solution::Type type)
{
	Solution solution;

	switch (type)
	{
	case Solution::Type::Greedy:
	{
		GreedyAlgorithm greedyAlgorithm(m_field);
		solution = greedyAlgorithm.run();
	}
	default:
		break;
	}
	
	return solution;
}
