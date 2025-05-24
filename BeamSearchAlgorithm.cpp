#include "BeamSearchAlgorithm.h"

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
	currentBeam.push({ m_field, Solution(), m_field.evaluateState(), 0 });

	// 訪問済み状態を記録するハッシュセット
	std::unordered_set<size_t> visited;
	visited.insert(m_field.computeHash()); // Will now use Zobrist hash

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

			// Define a structure for operations
			struct Operation {
				int x, y, size;
				int pairDiff;
				float scoreDiff;
				bool isPromising;

				// Sort order: promising first, then by scoreDiff (desc), then by pairDiff (desc)
				bool operator<(const Operation& other) const {
					if (isPromising != other.isPromising) {
						return isPromising > other.isPromising; // true (promising) comes before false
					}
					if (scoreDiff != other.scoreDiff) {
						return scoreDiff > other.scoreDiff;
					}
					return pairDiff > other.pairDiff;
				}
			};

			std::vector<Operation> operations;

			// Generate and evaluate all possible rotation operations
			// Iterate from largest possible size down to 2
			for (int32 size = fieldSize - 1; size >= 2; --size) {
				for (int32 x = 0; x <= fieldSize - size; ++x) {
					for (int32 y = 0; y <= fieldSize - size; ++y) {
						if (current.field.isPair(x, y)) continue; // Skip if top-left is already part of a pair

						Field tempField = current.field;
						auto [pairDiff, scoreDiff] = tempField.rotateAndGetDiff(x, y, size);

						bool hasUnpaired = false;
						for (int i = 0; i < size; ++i) {
							for (int j = 0; j < size; ++j) {
								if (!current.field.isPair(x + i, y + j)) {
									hasUnpaired = true;
									break;
								}
							}
							if (hasUnpaired) break;
						}

						bool isPromising = (pairDiff > 0 || scoreDiff > 0.0f) && hasUnpaired;
						operations.push_back({ x, y, size, pairDiff, scoreDiff, isPromising });
					}
				}
			}

			// Sort operations
			std::sort(operations.begin(), operations.end());

			// Process sorted operations
			for (const auto& op : operations) {
				Field nextField = current.field;
				nextField.rotateAndGetDiff(op.x, op.y, op.size); // Apply rotation

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
	Print << U"not finished {}ms"_fmt(stopwatch.ms());
	return bestSolution;
}
