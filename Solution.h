#pragma once
#include <Siv3D.hpp> // Siv3Dの機能を使用

struct Operation
{
	int32 x;
	int32 y;
	int32 n;
};

struct Solution
{
	enum class Type
	{
		Greedy,
		BFS,
		BeamSearch,
		SimulatedAnnealing,
		DivideAndConquerBeamSearch,
		// 他のアルゴリズムの種類を追加
	};

	Type type;
	Array<Operation> ops;

	Solution();
	Solution(Array<Operation>);

	// 操作を追加
	void add(Operation operation);

	// 別の解とマージ
	void merge(const Solution& other);

	// 操作にアクセス
	const Operation& operator[](int32 index) const;

	// 全ての操作を取得
	const Array<Operation>& getOperations() const;

	// 操作の数を取得
	int32 getOperationCount() const;

	// 操作を取得
	void getOperation(int32 index, int32& x, int32& y, int32& size) const;

	// 解答用JSONから変換
	static Solution fromJSON(const JSON& json);
};
