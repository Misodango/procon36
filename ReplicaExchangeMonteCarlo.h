#pragma once
#include "Field.h"
#include "Solution.h"

class ReplicaExchangeMonteCarlo {
public:

	struct Replica {
		Field field;
		Solution solution;
		double temperature;
		double energy;
		std::mt19937 rng;

		Replica(const Field& field, double temperature, int32 seed)
			: field(field), temperature(temperature), rng(seed) {
			energy = -field.evaluateState();
		}
	};

	void mcStep(Replica& replica);
	std::optional<Operation> generateHeuristicOperation(const Field& field, std::mt19937& rng);
	void performExchange();

public:

	ReplicaExchangeMonteCarlo(const Field& initialField, int32 numReplicas = 8, int32 maxSteps = 300);

	std::vector<Replica> m_replicas;
	std::vector<double> m_tempelatures;
	int32 m_maxSteps;
	int32 m_exchangeInterval;

	Solution run();
};
