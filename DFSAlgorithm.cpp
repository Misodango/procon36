#include "DFSAlgorithm.h"
#include "Field.h"
#include "Solution.h"

/*
* @brief DFSAlgorithmのコンストラクタ
* @param field フィールド
* @return DFSAlgorithm
*/

DFSAlgorithm::DFSAlgorithm(const Field& field) : m_field(field) {}


/*
* @brief DFSAlgorithmの実行
* @return Solution
*/

Solution DFSAlgorithm::run()
{
	int32 depth = 5; // 深さを指定
	Solution bestSolution;
	int bestPairCount = -1;
	Field currentField = m_field;
	int fieldSize = currentField.size;

	// スタックを使用してDFSを実装
	struct State {
		Field field;
		Solution solution;
		int depth;
	};

	std::stack<State> stack;
	stack.push({ currentField, bestSolution, 0 });

	while (!stack.empty()) {
		State state = stack.top();
		stack.pop();

		// 現在のペア数を計算
		int currentPairCount = state.field.countPairs();

		// 最善の解を更新
		if (currentPairCount > bestPairCount) {
			bestPairCount = currentPairCount;
			bestSolution = state.solution;
		}

		// 深さが指定された値に達した場合、次の状態を探索
		if (state.depth > depth) {
			continue;
		}

		// 考えられるすべての「導き」操作を試す
		for (int size = 2; size <= fieldSize; size++) {
			for (int x = 0; x <= fieldSize - size; x++) {
				for (int y = 0; y <= fieldSize - size; y++) {
					// 現在のフィールドをコピー
					Field tempField = state.field;
					if (tempField.isPair(x, y)) continue;
					// 部分グリッドを回転
					tempField.rotate(x, y, size);

					// 新しい状態をスタックに追加
					Solution newSolution = state.solution;
					newSolution.add({ x, y, size });
					stack.push({ tempField, newSolution, state.depth + 1 });
				}
			}
		}
	}

	return bestSolution;
}

