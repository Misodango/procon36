#include "Field.h"
#include <Siv3D.hpp> // Siv3Dの機能を使用

/*
*  @brief フィールドのコンストラクタ
*  @param size フィールドのサイズ
*  @return 0埋めされたフィールド
*/

Field::Field(int32 size) : size(size), entityCount(size* size / 2 - 1), entities(size, size, 0) {}

/*
*  @brief フィールドのコピーコンストラクタ
*  @param other コピー元のフィールド
*  @return コピーされたフィールド
*/

Field::Field(const Field& other) : size(other.size), entityCount(other.entityCount), entities(other.entities) {}

/*
*  @brief フィールドのコピー代入演算子
* @param other コピー元のフィールド
* @return コピーされたフィールド
*/

Field& Field::operator=(const Field& other) {
	if (this != &other) {
		size = other.size;
		entityCount = other.entityCount;
		entities = other.entities;
	}
	return *this;
}


/*
*  @brief ランダムにフィールドを生成する
*  @param size フィールドのサイズ
*  @return ランダム生成されたフィールド
*/

Field Field::random(int32 size) {
	Field field(size);
	// エンティティは1つにつき2回出現する
	Array<int32> candidates((field.entityCount + 1) * 2);
	for (int32 i : step(field.entityCount + 1)) {
		candidates[i * 2] = candidates[i * 2 + 1] = i;
	}
	candidates.shuffle();
	for (int32 x : step(size)) {
		for (int32 y : step(size)) {
			field.entities[x][y] = candidates[x * field.size + y];
		}
	}
	return field;
}

/*
*  @brief JSONからフィールドを生成する
*  @param json JSONオブジェクト
*  @return フィールド
*/

Field Field::fromJSON(const JSON& json) {
	const int32 size = json[U"problem"][U"field"][U"size"].get<int32>();
	const JSONArrayView entitiesArray = json[U"problem"][U"field"][U"entities"].arrayView();
	Field field(size);
	for (int y = 0; y < size; ++y) {
		const JSONArrayView entities = entitiesArray[y].arrayView();
		for (int x = 0; x < size; ++x) {
			field.entities[y][x] = entities[x].get<int32>();
		}
	}
	return field;
}

/*
*  @brief ファイルからフィールドを生成する
*  @param path ファイルのパス
*  @return フィールド
*/

Field Field::fromPath(const FilePath& path) {

	const JSON json = JSON::Load(path);
	if (json) {
		return Field::fromJSON(json);
	}
	else {
		Print << U"JSON ファイルの読み込みに失敗しました";
	}
	return Field(0);
}

/*
* @ brief 任意の座標に導きを適用
* @ param x x座標 y y座標 n サイズ
* @ return void
*/

void Field::rotate(int32 x, int32 y, int32 n) {
	// 範囲外チェック
	if (x < 0 || y < 0 || x + n > size || y + n > size) {
		return;
	}

	// n <= 1の回転は無効
	if (n <= 1) {
		return;
	}

	Grid<int32> temp(n, n);
	// 回転前の値を一時的な配列にコピー
	for (int32 i = 0; i < n; ++i) {
		for (int32 j = 0; j < n; ++j) {
			temp[i][j] = entities[y + i][x + j];
		}
	}

	// 90度時計回りに回転させて元の配列にコピー
	for (int32 i = 0; i < n; ++i) {
		for (int32 j = 0; j < n; ++j) {
			entities[y + j][x + n - 1 - i] = temp[i][j];
		}
	}
}

/*
*  @brief フィールドを描画する
*/

void Field::draw() const {
	static Array<Color> colors;
	static const Font font(20);

	// Initialize colors only once
	if (colors.isEmpty()) {
		colors.resize(entityCount + 1);

		// Golden ratio approach for better distribution
		const double goldenRatioConjugate = 0.618033988749895;
		double h = 0.5; // Starting hue

		for (int i = 0; i <= entityCount; ++i) {
			h = fmod(h + goldenRatioConjugate, 1.0);
			colors[i] = ColorF(HSV(h * 360.0, 0.7, 0.95));
		}
	}

	for (int32 y : step(size)) {
		for (int32 x : step(size)) {

			Rect(x * 50, y * 50, 50, 50).draw(colors[entities[y][x]]);
			font(copysign(entities[y][x], 1)).drawAt(x * 50 + 25, y * 50 + 25, Palette::Black);
		}
	}

	// フィールドの枠を描画
	Rect(0, 0, size * 50, size * 50).drawFrame(2, Palette::Black);

}


/*
* @brief フィールドのペアを数える
* @return int32
*/

int32 Field::countPairs() const {
	const int32 dx[4] = { 1, 0, -1, 0 };
	const int32 dy[4] = { 0, 1, 0, -1 };
	int32 res = 0;
	Array<bool> seen(size * size / 2, false);
	for (int32 i : step(size)) {
		for (int32 j : step(size)) {
			int32 cur = entities[i][j];
			if (seen[cur]) {
				continue;
			}
			for (int32 k : step(4)) {
				int32 y = i + dy[k];
				int32 x = j + dx[k];
				if (x < 0 || size <= x || y < 0 || size <= y) {
					continue;
				}
				int32 nxt = entities[y][x];
				if (nxt != cur) continue;
				seen[cur] = true;
				res++;
				break;
			}

		}
	}
	return res;
}

/*
* @brief フィールドのサイズを取得
* @return int32
*/

int32 Field::getSize() const {
	return size;
}

/*
* @brief ペアのマスかどうかを判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPair(int32 x, int32 y) const {
	const int32 dx[4] = { 1, 0, -1, 0 };
	const int32 dy[4] = { 0, 1, 0, -1 };
	int32 cur = entities[y][x];
	for (int32 k : step(4)) {
		int32 ny = y + dy[k];
		int32 nx = x + dx[k];
		if (nx < 0 || size <= nx || ny < 0 || size <= ny) {
			continue;
		}
		int32 nxt = entities[ny][nx];
		if (nxt != cur) continue;
		return true;
	}
	return false;
}


/*
* @brief 左上から連続のペアを数える
* @return int32
*/

int32 Field::countPairsFromTopLeftHorizontal() const {
	const int32 dx[4] = { 1, 0, -1, 0 };
	const int32 dy[4] = { 0, 1, 0, -1 };
	int32 res = 0;
	Array<bool> seen(size * size / 2, false);
	for (int32 i : step(size)) {
		for (int32 j : step(size)) {
			int32 cur = entities[i][j];
			if (seen[cur]) {
				continue;
			}
			if (!isPair(i, j)) {
				return res;
			}
			res++;
			seen[cur] = true;
		}
	}
	return res;
}

/*
* @brief 左上から連続のペアを数える
* @return int32
*/

int32 Field::countPairsFromTopLeftVertical() const {
	const int32 dx[4] = { 1, 0, -1, 0 };
	const int32 dy[4] = { 0, 1, 0, -1 };
	int32 res = 0;
	Array<bool> seen(size * size / 2, false);
	for (int32 i : step(size)) {
		for (int32 j : step(size)) {
			int32 cur = entities[j][i];
			if (seen[cur]) {
				continue;
			}
			if (!isPair(j, i)) {
				return res;
			}
			res++;
			seen[cur] = true;
		}
	}
	return res;
}

/*
* @brief 合法手を取得
* @return Array<Solution>
*/
Array<Solution> Field::getLegalMoves() const {
	Array<Solution> legalMoves;
	for (int32 y : step(size)) {
		for (int32 x : step(size)) {
			int32 range = size - Max(x, y);
			for (int32 n : step(2, range + 1)) {
				;
			}
		}
	}
	return legalMoves;
}

/*
* @brief 終了判定
* @return bool
*/

bool Field::isFinished() const {
	return countPairs() == size * size / 2;
}

/*
* @brief 右にペアがあるか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::hasPairRight(int32 x, int32 y) const {
	return (x + 1 < size && entities[y][x] == entities[y][x + 1]);
}

/*
* @brief 左にペアがあるか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::hasPairLeft(int32 x, int32 y) const {
	return (x - 1 >= 0 && entities[y][x] == entities[y][x - 1]);
}

/*
* @brief 上にペアがあるか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::hasPairUp(int32 x, int32 y) const {
	return (y - 1 >= 0 && entities[y][x] == entities[y - 1][x]);
}

/*
* @brief 下にペアがあるか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::hasPairDown(int32 x, int32 y) const {
	return (y + 1 < size && entities[y][x] == entities[y + 1][x]);
}

/*
* @brief 自信が右のペアか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPairRight(int32 x, int32 y) const {
	return (0 <= x - 1 && entities[y][x] == entities[y][x - 1]);
}

/*
* @brief 自信が左のペアか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPairLeft(int32 x, int32 y) const {
	return (x + 1 < size && entities[y][x] == entities[y][x + 1]);
}

/*
* @brief 自信が上のペアか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPairUp(int32 x, int32 y) const {
	return (0 <= y - 1 && entities[y][x] == entities[y - 1][x]);
}

/*
* @brief 自信が下のペアか判定
* @param x x座標
* @param y y座標
* @return bool
*/

bool Field::isPairDown(int32 x, int32 y) const {
	return (y + 1 < size && entities[y][x] == entities[y + 1][x]);
}

