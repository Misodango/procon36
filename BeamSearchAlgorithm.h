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
	int32 m_maxDepth;   // 最大探索深さ(N*N/2くらいが理想)

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
	// Constructor declaration (definition will be in .cpp)
	// maxDepth : 24x24/2 = 288手程度で揃えたいので最大300
	BeamSearchAlgorithm(const Field& field, int32 beamWidth = 100, int32 maxDepth = 300);

	Solution run();
};
