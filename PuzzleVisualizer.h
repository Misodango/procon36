#pragma once
#include <Siv3D.hpp>
#include "Field.h"
#include "GreedyAlgorithm.h"
#include "Solution.h"


class PuzzleVisualizer {
private:
	// フィールド関連
	Field m_initialField;
	Field m_currentField;
	Solution m_solution;
	Array<Operation> m_operations;
	int32 m_currentStep = 0;
	bool m_isPlaying = false;
	double m_playSpeed = 1.0;
	int32 m_lastUpdateTime = 0;

	// UI関連
	Rect m_fieldRect;
	Rect m_controlRect;
	bool m_slider;
	int32 m_windowWidth, m_windowHeight;
	double m_sliderValue;

	// ボタン
	Rect m_playButton, m_pauseButton, m_resetButton;
	Rect m_nextButton, m_prevButton;
	Rect m_speedUpButton, m_speedDownButton;

	// フォント
	Font m_font;

	void drawControls();

	void handleInput();

	void updateFieldToCurrentStep();

	void update();

public:
	PuzzleVisualizer(const Field& initialField, const Solution& solution);

	void run();
};
