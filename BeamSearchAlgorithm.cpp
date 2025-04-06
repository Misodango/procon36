#include "BeamSearchAlgorithm.h"

Solution BeamSearchAlgorithm::run() {
	// 初期状態
	std::priority_queue<BeamState> currentBeam;
	currentBeam.push({ m_field, Solution(), m_field.evaluateState(), 0 });

	// 訪問済み状態を記録するハッシュセット
	std::unordered_set<size_t> visited;
	visited.insert(m_field.computeHash());

	Solution bestSolution;
	float bestScore = -std::numeric_limits<float>::infinity();
	int fieldSize = m_field.getSize();

	// Stopwatch
	Stopwatch stopwatch;
	stopwatch.start();
	// 各深さでビームサーチを実行
	for (int32 depth = 0; depth < m_maxDepth; ++depth) {
		std::priority_queue<BeamState> nextBeam;
		int32 statesExamined = 0;

		// 現在のビームの各状態を展開
		while (!currentBeam.empty() && statesExamined < m_beamWidth) {
			BeamState current = currentBeam.top();
			currentBeam.pop();
			statesExamined++;

			// 完了状態なら解を更新
			if (current.field.isFinished()) {
				Print << U"solved ! :{}ms"_fmt(stopwatch.ms());
				return current.solution;  // 最短解を見つけたので即座に返す
			}

			// 現在の最良解より良い場合は更新
			if (current.score > bestScore) {
				bestScore = current.score;
				bestSolution = current.solution;
			}

			// 全ての可能な操作を試す
			for (int32 size = 2; size < fieldSize; ++size) {
				for (int32 x = 0; x <= fieldSize - size; ++x) {
					for (int32 y = 0; y <= fieldSize - size; ++y) {
						// 既にペアになっている場所は回転しない
						if (current.field.isPair(x, y)) continue;

						// フィールドをコピーして回転
						Field nextField = current.field;
						
						auto [pairDiff, scoreDiff] = nextField.rotateAndGetDiff(x, y, size);

						int32 newPairCount = current.field.countPairs() + pairDiff;
						float newScore = current.score + scoreDiff;

						// ハッシュ値を計算
						size_t hash = nextField.computeHash();

						// 未訪問の状態のみ追加
						if (visited.find(hash) == visited.end()) {
							visited.insert(hash);

							// 新しい解を作成
							Solution nextSolution = current.solution;
							nextSolution.add({ x, y, size });

							// 状態を評価
							// float score = nextField.evaluateState();

							// 次のビームに追加
							nextBeam.push({ nextField, nextSolution, newScore, depth + 1 });
						}
					}
				}
			}
		}

		// ビームが空になったら終了
		if (nextBeam.empty()) {
			break;
		}

		// 次の深さに進む
		currentBeam = nextBeam;

		// ビーム幅を制限
		std::priority_queue<BeamState> limitedBeam;
		int32 count = 0;
		while (!currentBeam.empty() && count < m_beamWidth) {
			limitedBeam.push(currentBeam.top());
			currentBeam.pop();
			count++;
		}
		currentBeam = limitedBeam;
	}
	Print << U"not finished {}ms"_fmt(stopwatch.ms());
	return bestSolution;
}
