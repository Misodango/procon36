# include <Siv3D.hpp>
# include "Field.h"
# include "Algorithm.h"
# include "PuzzleVisualizer.h"


/*
* # procon36
*
* 熊本高専熊本キャンパス
*
* ゲーム全体の管理
*
*/

void Main()
{
	// 背景の色を設定する
	Scene::SetBackground(ColorF{ 0.6, 0.8, 0.7 });

	// フルスクリーン
	Window::SetFullscreen(true);

	// JSON読み込み
	// JSON ファイルのパス
	const FilePath path = FileSystem::FullPath(U"input.json");
	// JSON パーサーの生成
	// Field field = Field::fromPath(path);
	Field field = Field::random(24);
	bool isField = true;

	// solve
	Algorithm algorithm(field);
	AsyncTask<Solution> solutionTask;

	while (System::Update()) {

		if (SimpleGUI::Button(U"Solve", Vec2(1000, 1000), unspecified, (not solutionTask.isValid()))) {
			solutionTask = algorithm.runAsync(Solution::Type::DivideAndConquerBeamSearch);
		}

		if (solutionTask.isReady()) {
			PuzzleVisualizer visualizer(field, solutionTask.get());
			visualizer.run();
		}
		else {
			static PuzzleVisualizer visualizer(field, Solution());
			visualizer.draw();
		}
	}
}
