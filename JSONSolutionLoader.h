#pragma once
#include <Siv3D.hpp>
#include "Field.h"
#include "Solution.h"

class JSONSolutionLoader {
public:
	struct LoadResult {
		bool success = false;
		String errorMessage;
		Field field;
		Solution solution;
		bool hasField = false;
	};

	// ファイルパスからJSONを読み込み、解答とフィールド（あれば）を取得
	static LoadResult loadFromFile(const FilePath& filePath);
	
	// JSONオブジェクトから解答とフィールド（あれば）を取得
	static LoadResult loadFromJSON(const JSON& json);
	
	// JSONの構造を検証
	static bool validateSolutionJSON(const JSON& json, String& errorMessage);
	
	// JSONからフィールドデータを抽出（オプション）
	static Optional<Field> extractField(const JSON& json);

private:
	// フィールドデータの検証
	static bool validateFieldData(const JSON& fieldJson);
};
