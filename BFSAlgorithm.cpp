#include "BFSAlgorithm.h"
#include "Field.h"
#include "Solution.h"

/*
* @brief BFSAlgorithmのコンストラクタ
* @param field フィールド
* @return BFSAlgorithm
*/

BFSAlgorithm::BFSAlgorithm(const Field& field) : m_field(field) {}


/*
* @brief BFSAlgorithmの実行
* @return Solution
*/

Solution BFSAlgorithm::run() 
{
	Field currentField = m_field;
	int fieldSize = currentField.size;
	Solution emptySolution;

	struct State {
		Field field;
		Solution solution;
	};

	std::queue<State> queue;
	queue.push({ currentField, emptySolution });

	// 訪問済み状態を記録（オプション、メモリ使用量と探索効率のトレードオフ）
	std::set<size_t> visited;
	visited.insert(currentField.computeHash());

	// 時間計測
	Stopwatch stopwatch;
	stopwatch.start();

	while (!queue.empty()) {
		State state = queue.front();
		queue.pop();

		// 完了状態をチェック
		if (state.field.isFinished()) {
			Print << U"solved! elapsed:{}ms"_fmt(stopwatch.ms());
			return state.solution; // 最初に見つけた解が最短解
		}

		// 考えられるすべての「導き」操作を試す
		for (int size = 2; size < fieldSize; size++) {
			for (int x = 0; x <= fieldSize - size; x++) {
				for (int y = 0; y <= fieldSize - size; y++) {
					// 現在のフィールドをコピー
					Field tempField = state.field;
					if (tempField.isPair(x, y)) continue;

					// 部分グリッドを回転
					tempField.rotate(x, y, size);

					// 状態の文字列表現
					size_t fieldHash = tempField.computeHash();

					// 未訪問の状態のみキューに追加
					if (visited.find(fieldHash) == visited.end()) {
						visited.insert(fieldHash);

						// 新しい状態をキューに追加
						Solution newSolution = state.solution;
						newSolution.add({ x, y, size });
						queue.push({ tempField, newSolution });
					}
				}
			}
		}
	}

	// 解が見つからなかった場合は空の解を返す
	return emptySolution;
}
