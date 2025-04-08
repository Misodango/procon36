#include "Algorithm.h"
#include "Solution.h"
#include "Field.h"

// 使用するアルゴリズムのヘッダファイル
#include "GreedyAlgorithm.h"
#include "BFSAlgorithm.h"
#include "BeamSearchAlgorithm.h"
#include "SimulatedAnnealingAlgorithm.h"
#include "DivideAndConquerBeamSearch.h"


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
	case Solution::Type::BFS:
	{
		BFSAlgorithm bfsAlgorithm(m_field);
		solution = bfsAlgorithm.run();
		break;
	}
	case Solution::Type::BeamSearch:
	{
		BeamSearchAlgorithm beamSearchAlgorithm(m_field);
		solution = beamSearchAlgorithm.run();
		break;
	}
	case Solution::Type::SimulatedAnnealing:
	{
		SimulatedAnnealingAlgorithm simulatedAnnealingAlgorithm(m_field);
		solution = simulatedAnnealingAlgorithm.run();
		break;
	}
	case Solution::Type::DivideAndConquerBeamSearch:
	{
		DivideAndConquerBeamSearch divideAndConquerBeamSearch(m_field);
		solution = divideAndConquerBeamSearch.run();
		break;
	}
	default:
		break;
	}
	
	return solution;
}


/*
* @brief Algorithmの非同期実行
* @param type 解法の種類
* @return AsyncTask<Solution>
*/

AsyncTask<Solution> Algorithm::runAsync(const Solution::Type type)
{
	return AsyncTask<Solution>([this, type]() {
		return run(type);
	});
}
