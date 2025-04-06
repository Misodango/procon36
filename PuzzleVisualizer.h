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
	AsyncTask<Solution> m_asyncSolution;
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
	bool m_showNumbers;
	bool m_colorTile;

	// ボタン
	Rect m_playButton, m_pauseButton, m_resetButton;
	Rect m_nextButton, m_prevButton;
	Rect m_speedUpButton, m_speedDownButton;
	Rect m_showNumbersButton;
	Rect m_colorTileButton;
	Rect m_closeButton;

	// フォント
	Font m_font;

	// フィールドの描画
	void drawControls();

	// スライダーの値を取得
	void handleInput();

	// 現在のステップにフィールドを更新
	void updateFieldToCurrentStep();

	// フィールドの更新
	void update();

	// フィールドの描画
	void drawField() const;

public:
	PuzzleVisualizer(const Field& initialField, const Solution& solution);

	PuzzleVisualizer(const Field& initialField, const AsyncTask<Solution>& asyncSolution);

	void run();

	// 描画のみ（更新などなし）
	void draw() const;
};
