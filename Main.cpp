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

	// JSON読み込み
	// JSON ファイルのパス
	const FilePath path = FileSystem::FullPath(U"input.json");
	// JSON パーサーの生成
	Field field = Field::fromPath(path);
	bool isField = true;

	// solve
	Algorithm algorithm(field);
	Solution solution = algorithm.run(Solution::Type::Greedy);

	int32 stepCount = 0;
	int32 maxStep = solution.ops.size();
	while (System::Update())
	{
		// フィールドを描画
		field.draw();

		const auto [x, y, n] = solution.ops[stepCount];
		field.rotate(x, y, n);
		if (stepCount >= maxStep) stepCount = 0;
		if (field.countPairs() == field.size * field.size / 2) {
			// ペアの数が最大になったら停止
			Print << U"Finish!";
			isField = true;
		}
	}


}
