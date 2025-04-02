#include "Solution.h"


JSON Solution::toJSON() const
{
	JSON json;
	json[U"type"] = ToString(type);
	Array<JSON> opsJson;
	for (const auto& op : ops)
	{
		JSON opJson;
		opJson[U"x"] = op.x;
		opJson[U"y"] = op.y;
		opJson[U"n"] = op.n;
		opsJson.push_back(opJson);
	}
	json[U"ops"] = opsJson;
	return json;
}

static String ToString(Solution::Type type)
{
	switch (type)
	{
	case Solution::Type::Greedy:
		return U"Greedy";
		// 他のアルゴリズムの種類を追加
	default:
		return U"Unknown";
	}
}
