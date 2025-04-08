#pragma once

#include "BeamSearchAlgorithm.h"
#include "Field.h"
#include "Solution.h"

class DivideAndConquerBeamSearch {
private:
	Field m_originalField;
	int32 m_beamWidth;
	int32 m_maxDepth;
	int32 m_timeout; // タイムアウト (ミリ秒)
	int32 m_attempts; // ビームサーチの試行回数

public:
	DivideAndConquerBeamSearch(const Field& field, int32 beamWidth = 100, int32 maxDepth = 10, int32 timeout = 300000, int32 attempts = 5);

	// メインの実行関数
	Solution run();
};
