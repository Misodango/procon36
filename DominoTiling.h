#pragma once

class DominoTiling {
private:
	int32 m_n;
	Array<Grid<int32>>  m_allPatterns;
	std::map<int32, int32> m_memo;

	int32 dp(int32 col, int32 mask);

	int32 fill(int32 col, int32 mask, int32 pos, int32 nextMask);

	void generatePatterns(int32 col, int32 mask, Grid<int32>& pattern);

	void fillPattern(int32 col, int32 mask, int32 pos, int32 nextMask, Grid<int32>& pattern);

public:

	DominoTiling(int32 n);

	int32 countTilings(int32 n);

	void generateAllPatterns();

	Grid<int32> getPattern(int32 index) const;

	int32 getPatternCount() const;

	// Returns a specified number of generated patterns.
	// If count is less than or equal to 0, an empty array is returned.
	// If count is greater than the total number of stored patterns, all available patterns are returned.
	Array<Grid<int32>> getManyPatterns(int32 count) const;

	// メモリ使用量が多すぎるので注意
	Array<Grid<int32>> getAllPatterns() const;
};
