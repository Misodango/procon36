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
	static constexpr double INTERNAL_TIMEOUT_SECONDS = 300.0;

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

	struct NextStateCandidate {
		Field field;
		std::vector<Operation> operationsToReach; // この盤面に至る手順
		float estimatedScore;
		int32 stepCount; // 何手で到達したか
	};

	// IDA*-like iterative deepening structures
	struct OperationScore {
		int32 x, y, size;
		int32 pairDiff;
		float scoreDiff;
		
		bool operator<(const OperationScore& other) const {
			// Higher pairDiff and scoreDiff are better
			if (pairDiff != other.pairDiff) return pairDiff > other.pairDiff;
			return scoreDiff > other.scoreDiff;
		}
	};

	// IDA*-like candidate generation with threshold
	std::vector<NextStateCandidate> generateNextStatesIDA(
		const BeamState& current,
		int32 maxCandidates,
		float scoreThreshold
	);

public:
	BeamSearchAlgorithm(const Field& field, int32 beamWidth = 100, int32 maxDepth = 500);

	std::vector<NextStateCandidate> generateNextStates(const BeamState& current, int32 maxMultiSteps, int32 topKSingle, int32 topKMulti);

	Solution run();
};
