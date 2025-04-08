#include "DivideAndConquerBeamSearch.h"

DivideAndConquerBeamSearch::DivideAndConquerBeamSearch(const Field& field, int32 beamWidth, int32 maxDepth, int32 timeout, int32 attempts)
	: m_originalField(field), m_beamWidth(beamWidth), m_maxDepth(maxDepth), m_timeout(timeout), m_attempts(attempts) {
}

Solution DivideAndConquerBeamSearch::run() {
	Stopwatch stopwatch;
	stopwatch.start();

	Solution bestSolution;
	Field field = m_originalField;

	// 初回のビームサーチを実行
	BeamSearchAlgorithm beamSearch(field, m_beamWidth, m_maxDepth);
	Solution solution = beamSearch.run();

	// 解を適用
	for (const auto& op : solution.ops) {
		field.rotate(op.x, op.y, op.n);
	}

	bestSolution = solution;

	// フィールドが未完成の場合
	while (!field.isFinished()) {
		// 盤面全体を回転
		field.rotate(0, 0, field.getSize());
		bestSolution.add({ 0, 0, field.getSize() }); // 全体回転の操作を追加

		// 再度ビームサーチを実行
		BeamSearchAlgorithm beamSearchAfterRotation(field, m_beamWidth, m_maxDepth);
		Solution solutionAfterRotation = beamSearchAfterRotation.run();

		// 新たな解を適用
		for (const auto& op : solutionAfterRotation.ops) {
			field.rotate(op.x, op.y, op.n);
		}

		// 暫定解に新たな操作を追加
		bestSolution.ops.insert(bestSolution.ops.end(), solutionAfterRotation.ops.begin(), solutionAfterRotation.ops.end());
	}

	Print << U"Solved in {}ms"_fmt(stopwatch.ms());
	return bestSolution;
}
