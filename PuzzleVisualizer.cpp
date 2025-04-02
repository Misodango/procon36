#include "PuzzleVisualizer.h"
#include "Field.h"
#include "Solution.h"

/*
* @brief PuzzleVisualizerのコンストラクタ
* @param initialField 初期フィールド
* @param solution 解答
* @return PuzzleVisualizer
*/

PuzzleVisualizer::PuzzleVisualizer(const Field& initialField, const Solution& solution) :
	m_initialField(initialField),
	m_currentField(initialField),
	m_solution(solution),
	m_font(24)
{
	// ウィンドウサイズの設定
	m_windowWidth = 800;
	m_windowHeight = 600;
	Scene::Resize(m_windowWidth, m_windowHeight);

	// フィールド表示領域とコントロール領域の設定
	m_fieldRect = Rect(0, 0, m_windowWidth, m_windowHeight - 200);
	m_controlRect = Rect(0, m_fieldRect.h, m_windowWidth, 200);

	// スライダーの設定
	// m_slider = Slider(Rect(m_controlRect.x + 100, m_controlRect.y + 20, m_controlRect.w - 200, 30), 0, static_cast<double>(m_solution.getOperationCount()));

	// ボタンの設定
	m_playButton = Rect(m_controlRect.x + 10, m_controlRect.y + 20, 50, 30);
	m_pauseButton = m_playButton;
	m_resetButton = Rect(m_controlRect.x + m_controlRect.w - 60, m_controlRect.y + 20, 50, 30);

	m_nextButton = Rect(m_controlRect.x + 70, m_controlRect.y + 20, 30, 30);
	m_prevButton = Rect(m_controlRect.x + 100, m_controlRect.y - 10, 30, 30); // スライダーの右にある

	m_speedUpButton = Rect(m_controlRect.x + m_controlRect.w - 120, m_controlRect.y + 20, 50, 30);
	m_speedDownButton = Rect(m_controlRect.x + m_controlRect.w - 180, m_controlRect.y + 20, 50, 30);

	// 操作の取得
	for (int i = 0; i < solution.getOperationCount(); i++) {
		int x, y, size;
		solution.getOperation(i, x, y, size);
		m_operations.push_back({ x, y, size });
	}
}

/*
* @brief PuzzleVisualizerの実行
*/

void PuzzleVisualizer::run()
{
	while (System::Update())
	{
		// 入力処理
		handleInput();

		// 状態更新
		update();

		// フィールドの描画
		m_currentField.draw();

		// 現在の操作を視覚化（部分グリッドのハイライト）
		if (m_currentStep > 0 && m_currentStep <= m_operations.size()) {
			const Operation& op = m_operations[m_currentStep - 1];
			int cellSize = Min(m_fieldRect.w, m_fieldRect.h) / m_initialField.getSize();
			Rect(op.x * cellSize, op.y * cellSize, op.n * cellSize, op.n * cellSize)
				.drawFrame(4, 0, ColorF(1.0, 0.0, 0.0, 0.5));
		}

		// コントロールの描画
		drawControls();
	}
}

/*
* @brief PuzzleVisualizerの描画
*/

void PuzzleVisualizer::drawControls()
{
	// コントロールパネルの背景
	m_controlRect.draw(ColorF(0.2, 0.2, 0.2, 0.8));

	// スライダー
	SimpleGUI::Slider(U"Step:{}/{}"_fmt(m_currentStep, m_operations.size()),
		m_sliderValue, m_currentStep, m_operations.size(),
		Vec2(m_controlRect.x + 100, m_controlRect.y + 20), m_controlRect.w - 200
	);

	// 再生・一時停止ボタン
	if (m_isPlaying) {
		m_pauseButton.drawFrame(2, 0, ColorF(1.0));
		Rect(m_pauseButton.x + 10, m_pauseButton.y + 10, 8, m_pauseButton.h - 20).draw(ColorF(1.0));
		Rect(m_pauseButton.x + m_pauseButton.w - 18, m_pauseButton.y + 10, 8, m_pauseButton.h - 20).draw(ColorF(1.0));
	}
	else {
		m_playButton.drawFrame(2, 0, ColorF(1.0));
		Triangle(Vec2(m_playButton.x + 15, m_playButton.y + 10),
				 Vec2(m_playButton.x + 15, m_playButton.y + m_playButton.h - 10),
				 Vec2(m_playButton.x + m_playButton.w - 15, m_playButton.y + m_playButton.h / 2)).draw(ColorF(1.0));
	}

	// リセットボタン
	m_resetButton.drawFrame(2, 0, ColorF(1.0));
	Line(m_resetButton.x + 12, m_resetButton.y + 12, m_resetButton.x + m_resetButton.w - 12, m_resetButton.y + m_resetButton.h - 12).draw(2, ColorF(1.0));
	Line(m_resetButton.x + 12, m_resetButton.y + m_resetButton.h - 12, m_resetButton.x + m_resetButton.w - 12, m_resetButton.y + 12).draw(2, ColorF(1.0));

	// 次へ・前へボタン
	m_nextButton.drawFrame(2, 0, ColorF(1.0));
	Triangle(Vec2(m_nextButton.x + 15, m_nextButton.y + 10),
			 Vec2(m_nextButton.x + 15, m_nextButton.y + m_nextButton.h - 10),
			 Vec2(m_nextButton.x + m_nextButton.w - 15, m_nextButton.y + m_nextButton.h / 2)).draw(ColorF(1.0));

	m_prevButton.drawFrame(2, 0, ColorF(1.0));
	Triangle(Vec2(m_prevButton.x + m_prevButton.w - 15, m_prevButton.y + 10),
			 Vec2(m_prevButton.x + m_prevButton.w - 15, m_prevButton.y + m_prevButton.h - 10),
			 Vec2(m_prevButton.x + 15, m_prevButton.y + m_prevButton.h / 2)).draw(ColorF(1.0));

	// 速度調節ボタン
	m_speedUpButton.drawFrame(2, 0, ColorF(1.0));
	m_font(U">>").drawAt(m_speedUpButton.center(), ColorF(1.0));

	m_speedDownButton.drawFrame(2, 0, ColorF(1.0));
	m_font(U"<<").drawAt(m_speedDownButton.center(), ColorF(1.0));

	// 情報表示
	m_font(U"Step: {}/{}"_fmt(m_currentStep, m_operations.size())).draw(m_controlRect.x + 10, m_controlRect.y + 80, ColorF(1.0));
	m_font(U"Speed: x{:.1f}"_fmt(m_playSpeed)).draw(m_controlRect.x + 10, m_controlRect.y + 110, ColorF(1.0));

	// ペア数
	int32_t pairs = m_currentField.countPairs();
	int32_t totalPairs = (m_initialField.getSize() * m_initialField.getSize()) / 2 - 1;
	m_font(U"Pairs: {}/{}"_fmt(pairs, totalPairs)).draw(m_controlRect.x + 10, m_controlRect.y + 140, ColorF(1.0));
}

/*
* @brief PuzzleVisualizerの入力処理
*/

void PuzzleVisualizer::handleInput()
{
	// スライダーの値が変更された場合
	if (m_slider) {
		m_currentStep = static_cast<int32>(m_sliderValue);
		updateFieldToCurrentStep();
	}

	// 再生・一時停止ボタン
	if (m_isPlaying ? m_pauseButton.leftClicked() : m_playButton.leftClicked()) {
		m_isPlaying = !m_isPlaying;
	}

	// リセットボタン
	if (m_resetButton.leftClicked()) {
		m_currentStep = 0;
		m_isPlaying = false;
		updateFieldToCurrentStep();
		m_sliderValue = 0;
	}

	// 次へボタン
	if (m_nextButton.leftClicked() && m_currentStep < m_operations.size()) {
		m_currentStep++;
		updateFieldToCurrentStep();
		m_sliderValue = m_currentStep;
	}

	// 前へボタン
	if (m_prevButton.leftClicked() && m_currentStep > 0) {
		m_currentStep--;
		updateFieldToCurrentStep();
		m_sliderValue = m_currentStep;
	}

	// 速度調節ボタン
	if (m_speedUpButton.leftClicked()) {
		m_playSpeed = Min(m_playSpeed * 1.5, 8.0);
	}

	if (m_speedDownButton.leftClicked()) {
		m_playSpeed = Max(m_playSpeed / 1.5, 0.25);
	}
}

/*
* @brief 現在のステップにフィールドを更新
*/

void PuzzleVisualizer::updateFieldToCurrentStep()
{
	// 初期状態から現在のステップまで操作を適用
	m_currentField = m_initialField;
	for (int i = 0; i < m_currentStep; i++) {
		const Operation& op = m_operations[i];
		m_currentField.rotate(op.x, op.y, op.n);
	}
}

/*
* @brief PuzzleVisualizerの更新処理
*/

void PuzzleVisualizer::update()
{
	// 再生中なら自動的に次のステップへ
	if (m_isPlaying && m_currentStep < m_operations.size()) {
		int currentTime = static_cast<int>(Scene::Time() * 1000);
		if (currentTime - m_lastUpdateTime > 1000 / m_playSpeed) {
			m_currentStep++;
			updateFieldToCurrentStep();
			m_sliderValue = m_currentStep;
			m_lastUpdateTime = currentTime;

			// 最後のステップに達したら停止
			if (m_currentStep >= m_operations.size()) {
				m_isPlaying = false;
			}
		}
	}
}
