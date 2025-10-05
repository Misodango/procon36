#include "BeamSearchAlgorithm.h"
#include <vector>
#include <future>
#include <thread>
#include <algorithm> // For std::min, std::max, std::sort
#include <iterator>  // For std::make_move_iterator

// Constructor definition
BeamSearchAlgorithm::BeamSearchAlgorithm(const Field& field, int32 beamWidth, int32 maxDepth)
	: m_field(field), m_beamWidth(beamWidth), m_maxDepth(maxDepth) {
	if (!Field::zobristTableInitialized) {
		Field::initializeZobristTable(m_field.getSize(), m_field.entityCount + 1);
	}
}

struct WorkItem { int32 s, x, y; };

struct MultiOperationWorkItem {
	std::vector<std::tuple<int32, int32, int32>> operations; // (x, y, size)の組み合わせ
	float estimatedScore;

	bool operator<(const MultiOperationWorkItem& other) const {
		return estimatedScore > other.estimatedScore;
	}
};

// IDA*-like iterative deepening candidate generation
std::vector<BeamSearchAlgorithm::NextStateCandidate> BeamSearchAlgorithm::generateNextStatesIDA(
	const BeamState& current,
	int32 maxCandidates,
	float scoreThreshold
) {
	std::vector<NextStateCandidate> candidates;
	const int32 fieldSize = current.field.getSize();

	// Priority queue to incrementally explore operations
	std::vector<OperationScore> operationScores;
	operationScores.reserve(fieldSize * fieldSize * fieldSize / 8); // Rough estimate

	// Phase 1: Quick evaluation of all operations (no field copy)
	// Start from larger rotations as they tend to have more impact
	for (int32 s_loop = fieldSize - 1; s_loop >= 2; --s_loop) {
		for (int32 x_loop = 0; x_loop <= fieldSize - s_loop; ++x_loop) {
			for (int32 y_loop = 0; y_loop <= fieldSize - s_loop; ++y_loop) {
				if (current.field.isPairRight(x_loop, y_loop) || current.field.isPairDown(x_loop, y_loop)) continue;
				// Quick heuristic: estimate without full rotation
				// Use a lightweight check first
				Field tempField = current.field;
				auto [pairDiff, scoreDiff] = tempField.rotateAndGetDiff(x_loop, y_loop, s_loop);

				// Only keep promising operations
				if (pairDiff > 0 || scoreDiff > scoreThreshold) {
					operationScores.push_back({ x_loop, y_loop, s_loop, pairDiff, scoreDiff });
				}
			}
		}
	}

	// Phase 2: Sort by quality and take top-K
	std::sort(operationScores.begin(), operationScores.end());

	// Phase 3: Generate actual candidates only for top operations
	int32 candidateLimit = std::min(maxCandidates, static_cast<int32>(operationScores.size()));
	candidates.reserve(candidateLimit);

	// Parallelize the final candidate generation
	std::vector<std::future<NextStateCandidate>> futures;
	const size_t num_threads = std::max(1u, std::thread::hardware_concurrency());

	for (int32 i = 0; i < candidateLimit; ++i) {
		const auto& opScore = operationScores[i];

		// Create candidates in parallel batches
		if (i % num_threads == 0 && i + num_threads < candidateLimit) {
			// Batch processing
			futures.clear();
			int32 batchEnd = std::min(candidateLimit, i + static_cast<int32>(num_threads));

			for (int32 j = i; j < batchEnd; ++j) {
				const auto& op = operationScores[j];
				futures.emplace_back(std::async(std::launch::async,
					[&current, op]() {
						Field nextField = current.field;
						auto [pairDiff, scoreDiff] = nextField.rotateAndGetDiff(op.x, op.y, op.size);

						return NextStateCandidate{
							nextField,
							{{op.x, op.y, op.size}},
							pairDiff * 100.0f + scoreDiff,
							1
						};
					}));
			}

			for (auto& fut : futures) {
				candidates.push_back(fut.get());
			}

			i = batchEnd - 1; // Adjust loop counter
		}
		else {
			// Single operation
			Field nextField = current.field;
			auto [pairDiff, scoreDiff] = nextField.rotateAndGetDiff(opScore.x, opScore.y, opScore.size);

			candidates.push_back({
				nextField,
				{{opScore.x, opScore.y, opScore.size}},
				pairDiff * 100.0f + scoreDiff,
				1
			});
		}
	}

	return candidates;
}

std::vector<BeamSearchAlgorithm::NextStateCandidate> BeamSearchAlgorithm::generateNextStates(
	const BeamState& current,
	int32 maxMultiSteps,  // 最大2手先まで
	int32 topKSingle,   // 単手候補数
	int32 topKMulti      // 複数手候補数
) {
	// Use IDA*-like approach with dynamic threshold
	// Start with aggressive pruning, then relax if needed
	std::vector<NextStateCandidate> candidates;

	// Adaptive threshold based on current score
	float initialThreshold = -5.0f; // Allow slightly negative moves for exploration

	candidates = generateNextStatesIDA(current, topKSingle, initialThreshold);

	// If we got too few candidates, relax the threshold
	if (candidates.size() < static_cast<size_t>(topKSingle / 2)) {
		candidates = generateNextStatesIDA(current, topKSingle, initialThreshold - 10.0f);
	}

	// Sort by estimated score
	std::sort(candidates.begin(), candidates.end(),
		[](const NextStateCandidate& a, const NextStateCandidate& b) {
			return a.estimatedScore > b.estimatedScore;
		});

	// Limit to requested size
	if (candidates.size() > static_cast<size_t>(topKSingle)) {
		candidates.resize(topKSingle);
	}

	return candidates;
}

Solution BeamSearchAlgorithm::run() {
	// 初期状態
	std::priority_queue<BeamState> currentBeam;
	currentBeam.push({ m_field, Solution(), m_field.evaluateStateWithEntropy(), 0 });

	// 訪問済み状態を記録するハッシュセット
	std::unordered_set<size_t> visited;
	visited.insert(m_field.computeHash()); // Will now use Zobrist hash

	Solution bestSolution;
	float bestScore = -std::numeric_limits<float>::infinity();
	int fieldSize = m_field.getSize();

	// Stopwatch
	Stopwatch stopwatch;
	stopwatch.start();
	Stopwatch operationGenerationStopwatch; // For profiling operation generation
	Stopwatch beamManagementStopwatch;    // For profiling beam management

	// 各深さでビームサーチを実行
	for (int32 depth = 0; depth < m_maxDepth; ++depth) {
		if (stopwatch.sF() >= INTERNAL_TIMEOUT_SECONDS) {
			// Print << U"Beam search internal timeout of {}s reached at depth {}."_fmt(INTERNAL_TIMEOUT_SECONDS, depth);
			break;
		}
		std::priority_queue<BeamState> nextBeam;
		int32 statesExamined = 0;

		// 現在のビームの各状態を展開
		while (!currentBeam.empty() && statesExamined < m_beamWidth) {
			if (stopwatch.sF() >= INTERNAL_TIMEOUT_SECONDS) {
				Print << U"Beam search internal timeout of {}s reached during state expansion at depth {}."_fmt(INTERNAL_TIMEOUT_SECONDS, depth);
				goto timeout_exit_label; // Using goto to break out of nested loops, ensure label is defined after loops.
			}
			BeamState current = currentBeam.top();
			currentBeam.pop();
			statesExamined++;

			// 完了状態なら解を更新
			if (current.field.isFinished()) {
				// Print << U"solved ! :{}ms in {}steps"_fmt(stopwatch.ms(), current.solution.ops.size());
				return current.solution;  // 最短解を見つけたので即座に返す
			}

			// 現在の最良解より良い場合は更新
			if (current.score > bestScore) {
				bestScore = current.score;
				bestSolution = current.solution;
			}

			// Generate next state candidates using IDA*-like method
			operationGenerationStopwatch.reset();
			operationGenerationStopwatch.start();

			// Adaptive candidate count based on depth
			int32 candidatesPerState = std::max(50, 200 - depth * 5);
			std::vector<NextStateCandidate> candidates = generateNextStates(current, 2, candidatesPerState, 50);

			// Process each candidate
			for (const auto& candidate : candidates) {
				size_t hash = candidate.field.computeHash();

				if (visited.find(hash) == visited.end()) {
					visited.insert(hash);

					Solution nextSolution = current.solution;
					// Add all operations from the candidate
					for (const auto& op : candidate.operationsToReach) {
						nextSolution.add(op);
					}

					nextBeam.push({ candidate.field, nextSolution, current.score + candidate.estimatedScore, depth + 1 });
				}
			}
		}

		// ビームが空になったら終了
		if (nextBeam.empty()) {
			break;
		}

		// 次の深さに進む
		currentBeam = nextBeam;

		beamManagementStopwatch.reset();
		beamManagementStopwatch.start();

		// ビーム幅を制限し、多様性を促進 (Limit beam width and promote diversity)
		std::priority_queue<BeamState> newBeamQueue; // Renamed to avoid confusion with newBeam in other contexts
		std::unordered_set<size_t> hashesInNewBeamQueue;
		while (!currentBeam.empty() && hashesInNewBeamQueue.size() < static_cast<size_t>(m_beamWidth)) {
			BeamState state = currentBeam.top();
			currentBeam.pop();
			size_t hash = state.field.computeHash(); // Will now use Zobrist hash
			if (hashesInNewBeamQueue.find(hash) == hashesInNewBeamQueue.end()) {
				newBeamQueue.push(state);
				hashesInNewBeamQueue.insert(hash);
			}
		}
		currentBeam = newBeamQueue;
	}

timeout_exit_label:; // Label for goto, placed after the main loops

	if (not bestSolution.ops.empty() && stopwatch.sF() >= INTERNAL_TIMEOUT_SECONDS) {
		Print << U"Timeout: Returning best solution found so far after {}ms."_fmt(stopwatch.ms());
	}
	else if (bestSolution.ops.empty() && stopwatch.sF() >= INTERNAL_TIMEOUT_SECONDS) {
		Print << U"Timeout: No solution found within {}ms."_fmt(stopwatch.ms());
	}
	else if (m_field.isFinished()) {
		// This case should be handled by the early exit when a solution is found.
		// If reached, it means a finished state was achieved but not returned immediately.
		// Print << U"Finished (but not caught earlier): {}ms"_fmt(stopwatch.ms());
	}
	else {
		// Print << U"Not finished (max depth or empty beam): {}ms"_fmt(stopwatch.ms());
	}
	return bestSolution;
}
