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
	Field currentField = m_field;

	// 現在のペア数
	int32_t currentPairCount = currentField.countPairs();

	// 改善が見られなくなるまで繰り返す
	bool improved = true;
	while (improved) {
		improved = false;

		// 最良の操作を格納する変数
		int bestX = -1;
		int bestY = -1;
		int bestSize = -1;
		int bestGain = -1;

		// フィールドのサイズを取得
		int fieldSize = currentField.size;

		// 考えられるすべての「導き」操作を試す
		for (int size = 2; size <= fieldSize; size++) {
			// size×sizeの部分グリッドが収まる範囲でループ
			for (int x = 0; x <= fieldSize - size; x++) {
				for (int y = 0; y <= fieldSize - size; y++) {
					// 現在のフィールドをコピー
					Field tempField = currentField;

					// 部分グリッドを回転
					tempField.rotate(x, y, size);

					// 回転後のペア数を計算
					int32_t newPairCount = tempField.countPairs();

					// ペアの増加量を計算
					int gain = newPairCount - currentPairCount;

					// より良い操作が見つかった場合、情報を更新
					if (gain > bestGain) {
						bestGain = gain;
						bestX = x;
						bestY = y;
						bestSize = size;
						improved = true;
					}
				}
			}
		}

		// 改善する操作が見つかった場合、実行して解に追加
		if (improved) {
			currentField.rotate(bestX, bestY, bestSize);
			currentPairCount += bestGain;

			// 操作を解に追加
			solution.add({ bestX, bestY, bestSize });

			// すべてのペアが隣接していれば終了
			if (currentPairCount == (fieldSize * fieldSize) / 2 - 1) {
				break;
			}
		}
	}

	return solution;
}
