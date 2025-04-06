#pragma once
#include "Field.h"
#include "Solution.h"
#include <queue>
#include <unordered_set>
#include <functional>

class BeamSearchAlgorithm {
private:
	Field m_field;
	int32 m_beamWidth;  // ビーム幅
	int32 m_maxDepth;   // 最大探索深さ

	// 状態を評価値とともに保持する構造体
	struct BeamState {
		Field field;
		Solution solution;
		float score;
		int32 depth;

		// 比較演算子（スコアの高い順にソート）
		bool operator<(const BeamState& other) const {
			return score < other.score;
		}
	};

public:
	BeamSearchAlgorithm(const Field& field, int32 beamWidth = 1000, int32 maxDepth = 100)
		: m_field(field), m_beamWidth(beamWidth), m_maxDepth(maxDepth) {
	}

	Solution run();
};
