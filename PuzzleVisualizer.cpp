#include "PuzzleVisualizer.h"

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
	m_operations(solution.ops.size()),
	m_font(30),
	m_slider(false),
	m_windowWidth(800),
	m_windowHeight(600),
	m_fieldRect(50, 50, 500, 500),
	m_controlRect(50, 550, 500, 50),
	m_playButton(600, 50, 50, 50),
	m_pauseButton(660, 50, 50, 50),
	m_resetButton(720, 50, 50, 50),
	m_nextButton(600, 110, 50, 50),
	m_prevButton(660, 110, 50, 50),
	m_speedUpButton(720, 110, 50, 50),
	m_speedDownButton(780, 110, 50, 50)
{
	for (int i = 0; i < solution.ops.size(); i++) {
		m_operations[i] = { solution.ops[i].x, solution.ops[i].y, solution.ops[i].n };
	}
}

/*
* @brief PuzzleVisualizerの実行
*/

void PuzzleVisualizer::run()
{
	while (System::Update())
	{
		handleInput();
		update();
	}
}

/*
* @brief PuzzleVisualizerの描画
*/

void PuzzleVisualizer::drawControls()
{
	// フィールドの描画
	m_currentField.draw();
	// フィールドの枠
	m_fieldRect.drawFrame(1, Palette::Black);
	// ボタンの描画
	m_playButton.drawFrame(1, Palette::Black);
	m_pauseButton.drawFrame(1, Palette::Black);
	m_resetButton.drawFrame(1, Palette::Black);
	m_nextButton.drawFrame(1, Palette::Black);
	m_prevButton.drawFrame(1, Palette::Black);
	m_speedUpButton.drawFrame(1, Palette::Black);
	m_speedDownButton.drawFrame(1, Palette::Black);
	// ボタンのテキスト
	m_font(U"Play").drawAt(m_playButton.center(), Palette::Black);
	m_font(U"Pause").drawAt(m_pauseButton.center(), Palette::Black);
	m_font(U"Reset").drawAt(m_resetButton.center(), Palette::Black);
	m_font(U"Next").drawAt(m_nextButton.center(), Palette::Black);
	m_font(U"Prev").drawAt(m_prevButton.center(), Palette::Black);
	m_font(U"Speed Up").drawAt(m_speedUpButton.center(), Palette::Black);
	m_font(U"Speed Down").drawAt(m_speedDownButton.center(), Palette::Black);
	// スライダーの描画
	if (m_slider) {
		Rect(50, 550, 500, 50).drawFrame(1, Palette::Black);
	}
}

/*
* @brief PuzzleVisualizerの入力処理
*/

void PuzzleVisualizer::handleInput()
{
	// マウスの座標
	const Point mousePos = Cursor::Pos();
	// マウスの左クリック
	const bool leftClicked = MouseL.down();
	// マウスの右クリック
	const bool rightClicked = MouseR.down();
	// マウスの左クリックが押されたとき
	if (leftClicked) {
		// プレイボタンが押されたとき
		if (m_playButton.leftClicked()) {
			m_isPlaying = true;
		}
		// ポーズボタンが押されたとき
		if (m_pauseButton.leftClicked()) {
			m_isPlaying = false;
		}
		// リセットボタンが押されたとき
		if (m_resetButton.leftClicked()) {
			m_currentStep = 0;
			m_currentField = m_initialField;
		}
		// 次へボタンが押されたとき
		if (m_nextButton.leftClicked()) {
			m_currentStep++;
			if (m_currentStep >= m_operations.size()) {
				m_currentStep = m_operations.size() - 1;
			}
			updateFieldToCurrentStep();
		}
		// 前へボタンが押されたとき
		if (m_prevButton.leftClicked()) {
			m_currentStep--;
			if (m_currentStep < 0) {
				m_currentStep = 0;
			}
			updateFieldToCurrentStep();
		}
		// スピードアップボタンが押されたとき
		if (m_speedUpButton.leftClicked()) {
			m_playSpeed *= 2;
		}
		// スピードダウンボタンが押されたとき
		if (m_speedDownButton.leftClicked()) {
			m_playSpeed /= 2;
		}
	}
}

/*
* @brief PuzzleVisualizerの更新処理
*/

void PuzzleVisualizer::update()
{
	// フィールドの描画
	drawControls();
	// プレイ中の場合
	if (m_isPlaying) {
		// 経過時間
		const int currentTime = Time::GetMillisec();
		// 一定時間経過した場合
		if (currentTime - m_lastUpdateTime > 1000 / m_playSpeed) {
			m_lastUpdateTime = currentTime;
			m_currentStep++;
			if (m_currentStep >= m_operations.size()) {
				m_currentStep = m_operations.size() - 1;
				m_isPlaying = false;
			}
			updateFieldToCurrentStep();
		}
	}
}

/*
* @brief 現在のステップにフィールドを更新
*/

void PuzzleVisualizer::updateFieldToCurrentStep()
{
	m_currentField = m_initialField;
	for (int i = 0; i <= m_currentStep; i++) {
		m_currentField.rotate(m_operations[i].x, m_operations[i].y, m_operations[i].n);
	}
}
