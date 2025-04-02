# include <Siv3D.hpp> // Siv3D v0.6.15
# include "NetworkManager.h"
# include "Field.h"

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
	
	while (System::Update())
	{
		// フィールドを描画
		field.draw();
	}
	

}
