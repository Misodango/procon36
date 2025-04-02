#pragma once

class Field {
public:
	// フィールドのサイズ
	const int32 size;

	// エンティティの数
	const int32 entityCount;

	// エンティティの値を保持する2次元配列
	Grid<int32> entities;

	// 描画用の色
	static Array<Color> colors;

	// コンストラクタでフィールドを初期化
	Field(int32 size);

	// ランダムにフィールドを生成する関数
	static Field random(int32 size);

	static Field fromJSON(const JSON& json);

	static Field fromPath(const FilePath& path);

	// 指定した範囲を右方向に90度回転させる関数
	void rotate(int32 x, int32 y, int32 n);

	// フィールドを描画する関数
	void draw() const;

	// フィールドのペアを数える
	int32 countPairs() const;
};
