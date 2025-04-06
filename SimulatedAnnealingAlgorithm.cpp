#include "SimulatedAnnealingAlgorithm.h"
#include "Field.h"

 Solution SimulatedAnnealingAlgorithm::run(){
		// Initialize
		Field currentField = m_initialField;
		Field bestField = currentField;
		Solution currentSolution;
		Solution bestSolution;

		double temperature = m_initialTemperature;
		float currentScore = currentField.evaluateState();
		float bestScore = currentScore;

		std::uniform_real_distribution<double> dist(0.0, 1.0);
		const int32 fieldSize = currentField.getSize();

		// For tracking progress
		Stopwatch stopwatch;
		stopwatch.start();
		Print << U"Starting simulated annealing...";

		// Store visited states to avoid repeating
		std::unordered_set<size_t> visited;
		visited.insert(currentField.computeHash());

		// Main simulated annealing loop
		for (int32 iteration = 0; iteration < m_maxIterations && temperature > m_minTemperature; ++iteration) {
			// Generate a random move (x, y, size)
			std::uniform_int_distribution<int32> sizeDist(2, fieldSize - 1);
			int32 size = sizeDist(m_rng);

			std::uniform_int_distribution<int32> posDist(0, fieldSize - size);
			int32 x = posDist(m_rng);
			int32 y = posDist(m_rng);

			// Skip if this position is already part of a pair
			if (currentField.isPair(x, y)) {
				continue;
			}

			// Make a copy of the current field
			Field nextField = currentField;

			// Apply the rotation and get the score difference
			auto [pairDiff, scoreDiff] = nextField.rotateAndGetDiff(x, y, size);
			float nextScore = currentScore + scoreDiff;

			// Calculate acceptance probability
			size_t nextHash = nextField.computeHash();
			bool alreadyVisited = (visited.find(nextHash) != visited.end());

			// Always accept better moves, probabilistically accept worse moves
			bool acceptMove = false;

			if (!alreadyVisited) {
				if (nextScore > currentScore) {
					acceptMove = true;
				}
				else {
					// Calculate acceptance probability based on score difference and temperature
					double delta = nextScore - currentScore;
					double acceptanceProbability = exp(delta / temperature);
					acceptMove = (dist(m_rng) < acceptanceProbability);
				}
			}

			// Apply the move if accepted
			if (acceptMove) {
				currentField = nextField;
				currentScore = nextScore;
				currentSolution.add({ x, y, size });
				visited.insert(nextHash);

				// Update best solution if current is better
				if (currentScore > bestScore) {
					bestScore = currentScore;
					bestField = currentField;
					bestSolution = currentSolution;

					// Print progress
					if (iteration % 100 == 0) {
						Print << U"Iteration {}: New best score: {}, Pairs: {}"_fmt(
							iteration, bestScore, bestField.countPairs());
					}
				}

				// Check if we've solved the puzzle
				if (currentField.isFinished()) {
					Print << U"Solved in {}ms at iteration {}!"_fmt(stopwatch.ms(), iteration);
					return currentSolution;
				}
			}

			// Cool down temperature
			temperature *= m_coolingRate;

			// Optional: periodically display progress
			if (iteration % 1000 == 0) {
				Print << U"Iteration {}: Best score = {}, Current temp = {}, Pairs: {}/{}"_fmt(
					iteration, bestScore, temperature, bestField.countPairs(), fieldSize * fieldSize / 2);
			}
		}

		Print << U"Simulated annealing completed in {}ms. Best pairs: {}/{}"_fmt(
			stopwatch.ms(), bestField.countPairs(), fieldSize * fieldSize / 2);

		return bestSolution;
	}
