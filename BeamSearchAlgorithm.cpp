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



std::vector<BeamSearchAlgorithm::NextStateCandidate> BeamSearchAlgorithm::generateNextStates(
	const BeamState& current,
	int32 maxMultiSteps,  // 最大2手先まで
	int32 topKSingle,   // 単手候補数
	int32 topKMulti      // 複数手候補数
) {
	std::vector<NextStateCandidate> candidates;
	const int32 fieldSize = current.field.getSize();
	
	// Generate work items for parallelization
	std::vector<WorkItem> work_items;
	work_items.reserve(fieldSize * fieldSize * fieldSize);
	
	for (int32 s_loop = fieldSize - 1; s_loop >= 2; --s_loop) {
		for (int32 x_loop = 0; x_loop <= fieldSize - s_loop; ++x_loop) {
			for (int32 y_loop = 0; y_loop <= fieldSize - s_loop; ++y_loop) {
				work_items.push_back({ s_loop, x_loop, y_loop });
			}
		}
	}
	
	if (!work_items.empty()) {
		std::vector<std::future<std::vector<NextStateCandidate>>> futures;
		
		const size_t num_threads_to_use = std::max(1u, std::thread::hardware_concurrency());
		size_t items_per_thread = (work_items.size() + num_threads_to_use - 1) / num_threads_to_use;
		if (items_per_thread == 0 && !work_items.empty()) items_per_thread = 1;
		
		const Field& current_field_const_ref = current.field;
		
		for (size_t i = 0; i < num_threads_to_use; ++i) {
			size_t start_idx = i * items_per_thread;
			size_t end_idx = std::min(work_items.size(), (i + 1) * items_per_thread);
			
			if (start_idx >= end_idx) continue;
			
			futures.emplace_back(std::async(std::launch::async,
				[&work_items, start_idx, end_idx, &current_field_const_ref]() {
					std::vector<NextStateCandidate> local_candidates;
					local_candidates.reserve(end_idx - start_idx);
					
					for (size_t item_idx = start_idx; item_idx < end_idx; ++item_idx) {
						const auto& item = work_items[item_idx];
						const int32 s = item.s;
						const int32 x = item.x;
						const int32 y = item.y;
						
						Field next = current_field_const_ref;
						auto [pairDiff, scoreDiff] = next.rotateAndGetDiff(x, y, s);
						
						local_candidates.push_back({
							next,
							{{x, y, s}},
							pairDiff * 100.0f + scoreDiff,
							1
						});
					}
					return local_candidates;
				}));
		}
		
		candidates.clear();
		candidates.reserve(work_items.size());
		
		for (auto& fut : futures) {
			std::vector<NextStateCandidate> thread_candidates = fut.get();
			candidates.insert(candidates.end(),
				std::make_move_iterator(thread_candidates.begin()),
				std::make_move_iterator(thread_candidates.end()));
		}
	}
	
	// Sort and limit candidates
	std::sort(candidates.begin(), candidates.end(),
		[](const NextStateCandidate& a, const NextStateCandidate& b) {
			return a.estimatedScore > b.estimatedScore;
		});
	
	if(candidates.size() > static_cast<size_t>(topKSingle)) {
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

			// Generate next state candidates using the new method
			operationGenerationStopwatch.reset();
			operationGenerationStopwatch.start();
			
			std::vector<NextStateCandidate> candidates = generateNextStates(current, 2, 200, 50);

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
