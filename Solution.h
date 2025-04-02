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
		// 他のアルゴリズムの種類を追加
	};

	Type type;
	Array<Operation> ops;

	JSON toJSON() const;

	static String ToString(Type type);
};
