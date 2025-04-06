#pragma once
#include "Field.h"
#include "Solution.h"
#include <Siv3D.hpp>
#include <random>

class SimulatedAnnealingAlgorithm {
private:
	Field m_initialField;
	double m_initialTemperature;
	double m_coolingRate;
	int32 m_maxIterations;
	double m_minTemperature;
	std::mt19937 m_rng;

public:
	SimulatedAnnealingAlgorithm(const Field& field,
							   double initialTemperature = 100.0,
							   double coolingRate = 0.99,
							   int32 maxIterations = 1000000,
							   double minTemperature = 0.1)
		: m_initialField(field)
		, m_initialTemperature(initialTemperature)
		, m_coolingRate(coolingRate)
		, m_maxIterations(maxIterations)
		, m_minTemperature(minTemperature)
		, m_rng(std::random_device{}()) {
	}

	Solution run();
};
