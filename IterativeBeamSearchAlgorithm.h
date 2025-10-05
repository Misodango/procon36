#pragma once
#include "Field.h"
#include "Solution.h"
#include "BeamSearchAlgorithm.h" // Include for BeamSearchAlgorithm usage

class IterativeBeamSearchAlgorithm {
private:
	Field m_initialField;
	int32 m_beamWidthPerStep;
	int32 m_depthPerStep;
	int32 m_maxTotalSteps;

	Field m_currentField;
	Solution m_accumulatedSolution;

public:
	IterativeBeamSearchAlgorithm(const Field& initialField, int32 beamWidthPerStep = 500, int32 dethPerStep = 200 , int32 maxTotalSteps = 300);
	Solution run();
};
