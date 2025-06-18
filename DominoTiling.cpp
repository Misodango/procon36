#include "stdafx.h"
#include "DominoTiling.h"

DominoTiling::DominoTiling(int32 n) : m_n(n)
{
	if (m_n % 2) {
		return;
	}
	generateAllPatterns();
}

int32 DominoTiling::countTilings(int32 n) {
	if (n % 2) return 0;
	m_memo.clear();
	return dp(0, 0);
}

int32 DominoTiling::dp(int32 col, int32 mask)
{
	if (col == m_n) {
		return mask == 0 ? 1 : 0;
	}
	int32 key = col * (1 << m_n) + mask;
	if (m_memo.count(key)) { // Check if already memoized
		return m_memo[key];
	}
	int32 result = fill(col, mask, 0, 0);
	m_memo[key] = result;
	return result;
}

int32 DominoTiling::fill(int32 col, int32 mask, int32 pos, int32 nextMask) {
	if (pos == m_n) {
		return dp(col + 1, nextMask);
	}

	if ((mask >> pos) & 1) {
		return fill(col, mask, pos + 1, nextMask);
	}

	int32 result = 0;

	// Try placing a horizontal domino: (pos, col) to (pos, col+1)
	// This occupies cell (pos, col).
	// It makes cell (pos, col+1) covered from column col, so nextMask gets (1 << pos).
	// We then move to consider cell (pos+1, col).
	if (col < m_n) { // Ensure we are not in the last column to place a horizontal domino that extends to col+1
	    result += fill(col, mask, pos + 1, nextMask | (1 << pos));
	}

	// Try placing a vertical domino: (pos, col) to (pos+1, col)
	// This occupies cells (pos, col) and (pos+1, col).
	// It does not affect nextMask.
	// We then move to consider cell (pos+2, col).
	// Requires (pos+1 < m_n) and cell (pos+1, col) is not already covered by a domino from col-1.
	if (pos + 1 < m_n && !((mask >> (pos + 1)) & 1)) {
		result += fill(col, mask, pos + 2, nextMask);
	}
	return result;
}

void DominoTiling::generateAllPatterns() {
	m_allPatterns.clear();
	Grid<int32> initialPattern(m_n, m_n, -1);
	generatePatterns(0, 0, initialPattern);
}

void DominoTiling::generatePatterns(int32 col, int32 mask, Grid<int32>& pattern) {
	if (col == m_n) {
		if (mask == 0) {
			m_allPatterns.emplace_back(pattern);
		}
		return;
	}
	fillPattern(col, mask, 0, 0, pattern);
}

void DominoTiling::fillPattern(int32 col, int32 mask, int32 pos, int32 nextMask, Grid<int32>& pattern) {
	if (pos == m_n) { // Corrected assignment to comparison
		generatePatterns(col + 1, nextMask, pattern);
		return;
	}

	if ((mask >> pos) & 1) {
		fillPattern(col, mask, pos + 1, nextMask, pattern);
		return;
	}

	// 横のドミノ (Horizontal domino)
	// Places a domino at (pos, col) and (pos, col+1)
	if (col + 1 < m_n && pattern[pos][col] == -1 && pattern[pos][col + 1] == -1) {
		int32 tileId = static_cast<int32>(m_allPatterns.size() * 1000 + pos * m_n + col); // Unique enough ID for this pattern generation
		pattern[pos][col] = tileId;
		pattern[pos][col + 1] = tileId;
		// Cell (pos,col) is now filled. For the next state in this column, (pos,col) is occupied.
		// This domino extends to (pos,col+1), so nextMask for column col+1 gets (1<<pos).
		fillPattern(col, mask | (1 << pos), pos + 1, nextMask | (1 << pos), pattern);
		pattern[pos][col] = -1; // Backtrack
		pattern[pos][col + 1] = -1; // Backtrack
	}

	// 縦のドミノ (Vertical domino)
	// Places a domino at (pos, col) and (pos+1, col)
	if (pos + 1 < m_n && !((mask >> (pos + 1)) & 1) &&
		pattern[pos][col] == -1 && pattern[pos + 1][col] == -1) {
		int32 tileId = static_cast<int32>(m_allPatterns.size() * 1000 + pos * m_n + col + m_n * m_n); // Unique enough ID
		pattern[pos][col] = tileId;
		pattern[pos + 1][col] = tileId;
		// Cells (pos,col) and (pos+1,col) are now filled.
		// For the next state in this column, these are occupied.
		// This domino does not extend to col+1, so nextMask is not affected by this placement itself.
		fillPattern(col, mask | (1 << pos) | (1 << (pos + 1)), pos + 2, nextMask, pattern);
		pattern[pos][col] = -1; // Backtrack
		pattern[pos + 1][col] = -1; // Backtrack
	}
}

Grid<int32> DominoTiling::getPattern(int32 index) const {
	if (index >= static_cast<int32>(m_allPatterns.size())) {
		return Grid<int32>();
	}

	if (index == -1) {
		index = Random(m_allPatterns.size());
	}

	const Grid<int32>& pattern = m_allPatterns[index];

	Grid<int32> numberedPattern(m_n, m_n);
	std::map<int32, int32> tileToNumber;
	int32 numberCounter = 0;

	for (int32 i = 0; i < m_n; i++) {
		for (int32 j = 0; j < m_n; j++) {
			int32 tileId = pattern[i][j];
			if (!tileToNumber.count(tileId)) {
				tileToNumber[tileId] = numberCounter++;
			}
		}
	}

	for (int32 i = 0; i < m_n; i++) {
		for (int32 j = 0; j < m_n; j++) {
			int32 tileId = pattern[i][j];
			numberedPattern[i][j] = tileToNumber[tileId];
		}
	}

	return numberedPattern;
}

int32 DominoTiling::getPatternCount() const {
	return static_cast<int32>(m_allPatterns.size());
}

Array<Grid<int32>> DominoTiling::getManyPatterns(int32 count) const {
	if (count <= 0) {
		return Array<Grid<int32>>();
	}

	Array<Grid<int32>> result;
	int32 numToCopy = std::min(count, static_cast<int32>(m_allPatterns.size()));

	for (int32 i = 0; i < numToCopy; ++i) {
		result.emplace_back(getPattern(i)); // Use getPattern to get the numbered version
	}
	return result;
}

// メモリ使用量が多すぎるので注意
Array<Grid<int32>> DominoTiling::getAllPatterns() const {
	return m_allPatterns;
}
