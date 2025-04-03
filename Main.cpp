# include <Siv3D.hpp> // Siv3D v0.6.15
# include "NetworkManager.h"
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
	// Solution solution = algorithm.run(Solution::Type::Greedy);
	Solution solution;
	PuzzleVisualizer visualizer(field, solution);
	visualizer.run();
	while (System::Update())
	{

	}


}
