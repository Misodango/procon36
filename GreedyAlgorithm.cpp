#include "GreedyAlgorithm.h"

/*
* @brief GreedyAlgorithmのコンストラクタ
* @param field フィールド
* @return GreedyAlgorithm
*/

GreedyAlgorithm::GreedyAlgorithm(const Field& field) : m_field(field) {}


/*
* @brief GreedyAlgorithmの実行
* @return Solution
*/

Solution GreedyAlgorithm::run()
{
	Solution solution;
	solution.type = Solution::Type::Greedy; // アルゴリズムの種類を設定
	// アルゴリズムの実行ロジックをここに追加
	// 例として、フィールドの一部を回転させる操作を追加
	solution.ops.push_back({ 0, 0, 2 });
	solution.ops.push_back({ 2, 2, 2 });
	return solution;
}
