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
		// As per Field::random, entities are 0 to entityCount.
		// So, maxEntityValuePlusOne is m_field.entityCount + 1.
		// The maxSize for the Zobrist table should be based on m_field.getSize().
		Field::initializeZobristTable(m_field.getSize(), m_field.entityCount + 1);
	}
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
			Print << U"Beam search internal timeout of {}s reached at depth {}."_fmt(INTERNAL_TIMEOUT_SECONDS, depth);
			break;
		}
		std::priority_queue<BeamState> nextBeam;
		int32 statesExamined = 0;

		// 現在のビームの各状態を展開
		while (!currentBeam.empty() && statesExamined < m_beamWidth) {
			if (stopwatch.sF() >= INTERNAL_TIMEOUT_SECONDS) {
				Print << U"Beam search internal timeout of {}s reached during state expansion at depth {}."_fmt(INTERNAL_TIMEOUT_SECONDS, depth);
				timeoutOccurred = true; // Set flag to indicate timeout
				break; // Break out of the inner loop
			}
			BeamState current = currentBeam.top();
			currentBeam.pop();
			statesExamined++;

			// 完了状態なら解を更新
			if (current.field.isFinished()) {
				Print << U"solved ! :{}ms in {}steps"_fmt(stopwatch.ms(), current.solution.ops.size());
				return current.solution;  // 最短解を見つけたので即座に返す
			}

			// 現在の最良解より良い場合は更新
			if (current.score > bestScore) {
				bestScore = current.score;
				bestSolution = current.solution;
			}

			// Define a structure for operations
			struct Operation {
				int x, y, size;
				int pairDiff;
				float scoreDiff;
				bool isPromising;
				float entropyDiff; // new field

				// Sort order: promising first, then by scoreDiff (desc), then by entropyDiff (asc), then by pairDiff (desc)
				bool operator<(const Operation& other) const {
					if (isPromising != other.isPromising) {
						return isPromising > other.isPromising; // true (promising) comes before false
					}
					if (scoreDiff != other.scoreDiff) {
						return scoreDiff > other.scoreDiff;
					}
					if (entropyDiff != other.entropyDiff) {
						return entropyDiff < other.entropyDiff; // prefer lower entropy
					}
					return pairDiff > other.pairDiff;
				}
			};

			std::vector<Operation> operations;
			operationGenerationStopwatch.reset(); // Reset and start for this state's operations
			operationGenerationStopwatch.start();

			// --- Start of Parallelized Operation Generation ---
			struct WorkItem { int32 s, x, y; };
			std::vector<WorkItem> work_items;
			// Estimate max possible operations to reserve space, can be refined
			work_items.reserve(fieldSize * fieldSize * fieldSize); 

			for (int32 s_loop = fieldSize - 1; s_loop >= 2; --s_loop) {
				for (int32 x_loop = 0; x_loop <= fieldSize - s_loop; ++x_loop) {
					for (int32 y_loop = 0; y_loop <= fieldSize - s_loop; ++y_loop) {
						work_items.push_back({s_loop, x_loop, y_loop});
					}
				}
			}

			if (not work_items.empty()) {
				std::vector<std::future<std::vector<Operation>>> futures;
				const size_t num_threads_to_use = std::max(1u, std::thread::hardware_concurrency());
				// Ensure items_per_thread is at least 1 if work_items.size() < num_threads_to_use
				size_t items_per_thread = (work_items.size() + num_threads_to_use - 1) / num_threads_to_use;
				if (items_per_thread == 0 && !work_items.empty()) items_per_thread = 1;


				const Field& current_field_const_ref = current.field; // Capture current.field by const reference

				for (size_t i = 0; i < num_threads_to_use; ++i) {
					size_t start_idx = i * items_per_thread;
					size_t end_idx = std::min(work_items.size(), (i + 1) * items_per_thread);

					if (start_idx >= end_idx) continue; // No work for this thread

					futures.emplace_back(std::async(std::launch::async,
						[&work_items, start_idx, end_idx, &current_field_const_ref]() {
						std::vector<Operation> local_thread_operations;
						// Estimate based on chunk size, can be refined
						local_thread_operations.reserve(end_idx - start_idx); 

						for (size_t item_idx = start_idx; item_idx < end_idx; ++item_idx) {
							const auto& item = work_items[item_idx];
							const int32 s = item.s;
							const int32 x = item.x;
							const int32 y = item.y;

							// Original loop's core logic
							if (current_field_const_ref.isPairRight(x, y)) continue;

							Field tempFieldForDiff = current_field_const_ref; // Copy for rotateAndGetDiff
							auto [pairDiff, scoreDiff] = tempFieldForDiff.rotateAndGetDiff(x, y, s);

							// Calculate entropy diff using the new differential method
							// This is called on the state *before* the rotation.
							float entropyDiff = current_field_const_ref.calculateEntropyDiffForRotation(x, y, s);

							bool hasUnpaired = false;
							for (int r_i = 0; r_i < s; ++r_i) {
								for (int r_j = 0; r_j < s; ++r_j) {
									if (!current_field_const_ref.isPair(x + r_i, y + r_j)) {
										hasUnpaired = true;
										break;
									}
								}
								if (hasUnpaired) break;
							}
							
							bool isPromising = (pairDiff > 0 || scoreDiff > 0.0f) && hasUnpaired;
							local_thread_operations.push_back({ x, y, s, pairDiff, scoreDiff, isPromising, entropyDiff });
						}
						return local_thread_operations;
					}));
				}

				operations.clear(); // Ensure it's empty before collecting results
				operations.reserve(work_items.size()); // Reserve space based on total work items

				for (auto& fut : futures) {
					std::vector<Operation> thread_ops = fut.get();
					operations.insert(operations.end(),
						std::make_move_iterator(thread_ops.begin()),
						std::make_move_iterator(thread_ops.end()));
				}
			}
			// --- End of Parallelized Operation Generation ---
			
			// Sort operations
			std::sort(operations.begin(), operations.end());

			// Process sorted operations
			for (const auto& op : operations) {
				Field nextField = current.field;
				nextField.rotate(op.x, op.y, op.size);
				float newScore = current.score + op.scoreDiff;

				size_t hash = nextField.computeHash(); // Will now use Zobrist hash

				if (visited.find(hash) == visited.end()) {
					visited.insert(hash);

					Solution nextSolution = current.solution;
					nextSolution.add({ op.x, op.y, op.size });

					nextBeam.push({ nextField, nextSolution, newScore, depth + 1 });
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
	} else if (bestSolution.ops.empty() && stopwatch.sF() >= INTERNAL_TIMEOUT_SECONDS) {
		Print << U"Timeout: No solution found within {}ms."_fmt(stopwatch.ms());
	} else if (m_field.isFinished()) {
		// This case should be handled by the early exit when a solution is found.
		// If reached, it means a finished state was achieved but not returned immediately.
		Print << U"Finished (but not caught earlier): {}ms"_fmt(stopwatch.ms());
	}
	else {
		Print << U"Not finished (max depth or empty beam): {}ms"_fmt(stopwatch.ms());
	}
	return bestSolution;
}
