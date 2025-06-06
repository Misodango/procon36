#include "ReplicaExchangeMonteCarlo.h"
#include <Siv3D.hpp>

ReplicaExchangeMonteCarlo::ReplicaExchangeMonteCarlo(const Field& field, int32 numReplicas, int32 maxSteps)
	: m_maxSteps(maxSteps), m_exchangeInterval(100) {

	// 温度スケジュール
	double minTemp = 0.1;
	double maxTemp = 10.0;
	// Avoid division by zero if numReplicas is 1
	double tempRatio = (numReplicas <= 1) ? 1.0 : std::pow(minTemp / maxTemp, 1.0 / (numReplicas - 1));

	m_tempelatures.resize(numReplicas);
	m_replicas.reserve(numReplicas);

	const double stepPenaltyFactor = 0.1; // Define step penalty factor

	for (int32 i = 0; i < numReplicas; ++i) {
		double temperature = (numReplicas <= 1) ? maxTemp : maxTemp * std::pow(tempRatio, i);
		m_tempelatures[i] = temperature;
		m_replicas.emplace_back(field, temperature, i * 1000 + static_cast<uint32_t>(time(0)));

		// Initialize replica energy
		if (!m_replicas.empty()) {
			Replica& currentReplica = m_replicas.back();
			double potentialEnergy = -currentReplica.field.evaluateStateWithEntropy();
			double initialStepPenalty = currentReplica.solution.ops.size() * stepPenaltyFactor;
			currentReplica.energy = potentialEnergy + initialStepPenalty;
		}
	}
}

Solution ReplicaExchangeMonteCarlo::run() {

	std::vector<std::future<void>> futures;

	for (int32 step = 0; step < m_maxSteps; step++) {
		Print << step;
		// 並列処理
		futures.clear();
		for (auto& replica : m_replicas) {
			futures.emplace_back(std::async(std::launch::async,
				[&replica, this]() { mcStep(replica); }));
		}

		// 全レプリカの処理完了を待機
		for (auto& future : futures) {
			future.wait();
		}

		// レプリカ間の交換
		if (step % m_exchangeInterval == 0) {
			performExchange();
		}

		// 早期終了チェック
		if (m_replicas[0].field.isFinished()) {
			return m_replicas[0].solution;
		}
	}

	// 最適解を返す
	auto bestReplica = std::min_element(m_replicas.begin(), m_replicas.end(),
		[](const Replica& a, const Replica& b) { return a.energy < b.energy; });

	return bestReplica->solution;
}

void ReplicaExchangeMonteCarlo::mcStep(Replica& replica) {
	auto operation = generateHeuristicOperation(replica.field, replica.rng);

	if (!operation.has_value()) {
		return; // 無効な操作はスキップ
	}

	// Create a new field state by applying the operation
	Field newField = replica.field;
	newField.rotateAndGetDiff(operation->x, operation->y, operation->n);

	// Calculate the potential energy of the new state using evaluateStateWithEntropy
	double potentialEnergyOfNewState = -newField.evaluateStateWithEntropy();

	// Define the step penalty factor
	const double stepPenaltyFactor = 0.1; 

	// Calculate the step penalty for the new state (if accepted, one more operation)
	double stepPenaltyOfNewState = (replica.solution.ops.size() + 1) * stepPenaltyFactor;

	// Calculate the total energy of the proposed new state
	double totalEnergyOfNewState = potentialEnergyOfNewState + stepPenaltyOfNewState;

	// Calculate the change in energy for the Metropolis criterion
	double deltaEnergy = totalEnergyOfNewState - replica.energy;

	// Metropolis acceptance criterion
	double acceptanceProbability;
	if (deltaEnergy <= 0) {
		acceptanceProbability = 1.0;
	} else {
		if (replica.temperature < 1e-9) {
			acceptanceProbability = 0.0; 
		} else {
			acceptanceProbability = std::exp(-deltaEnergy / replica.temperature);
		}
	}

	std::uniform_real_distribution<double> dist(0.0, 1.0);
	if (dist(replica.rng) < acceptanceProbability) {
		replica.field = newField;
		replica.energy = totalEnergyOfNewState;
		replica.solution.add(*operation);
	}
}

std::optional<Operation> ReplicaExchangeMonteCarlo::generateHeuristicOperation(const Field& field, std::mt19937& rng) {
	int32 fieldSize = field.getSize();
	std::vector<Operation> candidates;

	std::uniform_int_distribution<int32> sizeDist(2, fieldSize - 1);
	std::uniform_int_distribution<int32> posDist(0, fieldSize - 1);

	// 候補を複数収集
	for (int32 attempt = 0; attempt < 100; ++attempt) {
		int32 size = sizeDist(rng);
		int32 x = posDist(rng);
		int32 y = posDist(rng);

		if (x + size > fieldSize || y + size > fieldSize) continue;
		if (field.isPairRight(x, y)) continue;

		// 使われていない場所を含むか確認
		bool hasUnpaired = false;
		for (int32 i = 0; i < size && !hasUnpaired; ++i) {
			for (int32 j = 0; j < size && !hasUnpaired; ++j) {
				if (!field.isPair(x + i, y + j)) {
					hasUnpaired = true;
				}
			}
		}

		if (hasUnpaired) {
			candidates.emplace_back(Operation{ x, y, size });
		}
	}
	
	if (candidates.empty()) return std::nullopt;

	// 評価関数で最良の操作を選ぶ
	double bestScore = std::numeric_limits<double>::infinity();
	std::optional<Operation> bestOp;
	for (const auto& op : candidates) {
		Field temp = field;
		temp.rotateAndGetDiff(op.x, op.y, op.n);
		double score = temp.evaluateStateWithEntropy(); // or + step penalty

		if (score < bestScore) {
			bestScore = score;
			bestOp = op;
		}
	}
	return bestOp;
}


void ReplicaExchangeMonteCarlo::performExchange() {
	int32 numReplicas = m_replicas.size();
	std::uniform_int_distribution<int32> dist(0, numReplicas - 1);
	for (int32 i = 0; i < numReplicas - 1; ++i) {
		double delta1 = 1.0 / m_replicas[i].temperature;
		double delta2 = 1.0 / m_replicas[i + 1].temperature;

		double deltaEnergy = m_replicas[i + 1].energy - m_replicas[i].energy;
		double exchangeProbability = std::exp(deltaEnergy * (delta1 - delta2));

		std::uniform_real_distribution<double> probDist(0.0, 1.0);
		if (probDist(m_replicas[i].rng) < exchangeProbability) {
			std::swap(m_replicas[i], m_replicas[i + 1]);
			std::swap(m_replicas[i].solution, m_replicas[i + 1].solution);
			std::swap(m_replicas[i].energy, m_replicas[i + 1].energy);
		}
	}
}
