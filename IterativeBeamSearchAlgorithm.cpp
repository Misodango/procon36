#include "IterativeBeamSearchAlgorithm.h"
#include "BeamSearchAlgorithm.h"

IterativeBeamSearchAlgorithm::IterativeBeamSearchAlgorithm(const Field& initialField, int32 beamWidthPerStep, int32 depthPerStep, int32 maxTotalSteps)
	: m_initialField(initialField),
	m_beamWidthPerStep(beamWidthPerStep),
	m_depthPerStep(depthPerStep),
	m_maxTotalSteps(maxTotalSteps),
	m_currentField(initialField) {
	// Ensure Zobrist table is initialized if not already,
	// as BeamSearchAlgorithm relies on it.
	// This might be redundant if BeamSearchAlgorithm's constructor always handles it,
	// but it's safe to ensure.
	if (!Field::zobristTableInitialized) {
		Field::initializeZobristTable(m_initialField.getSize(), m_initialField.entityCount + 1);
	}
}

Solution IterativeBeamSearchAlgorithm::run() {
	m_currentField = m_initialField;
	m_accumulatedSolution = Solution();

	Stopwatch totalStopwatch;
	totalStopwatch.start();

	for (int32 step = 0; step < m_maxTotalSteps; ++step) {
		Print << U"Iterative Step: {}"_fmt(step + 1);

		BeamSearchAlgorithm beamStep(m_currentField, m_beamWidthPerStep, m_depthPerStep);
		Solution stepSolution = beamStep.run();

		if (stepSolution.ops.isEmpty()) {
			Print << U"IterativeBeamSearch: Beam search returned no operations at step {}."_fmt(step + 1);
			break; // Stuck, no further moves found by the beam search step
		}

		// Apply the operations from stepSolution to m_currentField
		// and add them to m_accumulatedSolution
		for (const auto& op : stepSolution.ops) {
			// Call rotateAndGetDiffOptimized to apply the rotation.
			// The returned pair (pairDiff, scoreDiff) is not strictly needed here,
			// as the solution 'op' is already determined by the BeamSearchAlgorithm.
			// However, using this method ensures the field state is updated consistently
			// with how operations are evaluated during the search.
			// (void)m_currentField.rotateAndGetDiffOptimized(op.x, op.y, op.n); 
			(void)m_currentField.rotate(op.x, op.y, op.n);
			m_accumulatedSolution.add(op);
		}

		Print << U"Iterative Step {} completed. Current score: {}"_fmt(step + 1, m_currentField.evaluateState());


		if (m_currentField.isFinished()) {
			Print << U"IterativeBeamSearch: Puzzle solved in {} steps, total time: {}ms"_fmt(step + 1, totalStopwatch.ms());
			break;
		}
	}

	if (!m_currentField.isFinished()) {
		Print << U"IterativeBeamSearch: Max total steps reached or search stuck. Puzzle not solved. Total time: {}ms"_fmt(totalStopwatch.ms());
	}

	return m_accumulatedSolution;
}
