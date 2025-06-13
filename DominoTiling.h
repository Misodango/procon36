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
};
