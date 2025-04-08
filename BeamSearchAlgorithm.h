#pragma once
#include "Field.h"
#include "Solution.h"
#include <queue>
#include <unordered_set>
#include <functional>

struct BeamState {
	Field field;
	Solution solution;
	float score;
	int32 depth;

	// スコアの高い順にソート
	bool operator<(const BeamState& other) const {
		return score < other.score;
	}
};


class BeamSearchAlgorithm {
protected:
	Field m_field;
	int32 m_beamWidth;  // ビーム幅
	int32 m_maxDepth;   // 最大探索深さ(N*N/2くらいが理想)

	// サブクラスでオーバーライド可能なゴール判定
	virtual bool isGoalState(const Field& field) const {
		return field.isFinished();
	}

	// サブクラスでオーバーライド可能なゴール判定
	virtual Array<Solution> getLegalMoves(const Field& field) const {
		Array<Solution> moves;
		int32 fieldSize = field.getSize();

		for (int32 size = 2; size < fieldSize; ++size) {
			for (int32 y = 0; y <= fieldSize - size; ++y) {
				for (int32 x = 0; x <= fieldSize - size; ++x) {
					// 既にペアになっている場所は回転しない
					if (field.isPair(x, y)) continue;

					Solution sol;
					sol.add({ x, y, size });
					moves << sol;
				}
			}
		}

		return moves;
	}

public:
	BeamSearchAlgorithm(const Field& field, int32 beamWidth = 1000, int32 maxDepth = 20)
		: m_field(field), m_beamWidth(beamWidth), m_maxDepth(maxDepth) {
	}

	virtual Solution run();
};
