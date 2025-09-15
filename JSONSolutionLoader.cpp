#include "JSONSolutionLoader.h"

JSONSolutionLoader::LoadResult JSONSolutionLoader::loadFromFile(const FilePath& filePath) {
	LoadResult result;

	// ファイルの存在確認
	if (!FileSystem::Exists(filePath)) {
		result.errorMessage = U"File does not exist: " + filePath;
		return result;
	}

	// JSONファイルの読み込み
	JSON json = JSON::Load(filePath);
	if (!json) {
		result.errorMessage = U"Failed to parse JSON file: " + filePath;
		return result;
	}

	return loadFromJSON(json);
}

JSONSolutionLoader::LoadResult JSONSolutionLoader::loadFromJSON(const JSON& json) {
	LoadResult result;

	// JSONの構造を検証
	if (!validateSolutionJSON(json, result.errorMessage)) {
		return result;
	}

	// 解答データの読み込み
	result.solution = Solution::fromJSON(json);
	if (result.solution.isEmpty()) {
		result.errorMessage = U"Failed to load solution data from JSON";
		return result;
	}

	// フィールドデータの読み込み（オプション）
	Optional<Field> fieldOpt = extractField(json);
	if (fieldOpt.has_value()) {
		result.field = fieldOpt.value();
		result.hasField = true;
	}

	result.success = true;
	return result;
}

bool JSONSolutionLoader::validateSolutionJSON(const JSON& json, String& errorMessage) {
	// 基本的な構造チェック
	if (!json.isObject()) {
		errorMessage = U"JSON root must be an object";
		return false;
	}

	// 必須フィールド "ops" の存在確認
	if (!json.hasElement(U"ops")) {
		errorMessage = U"JSON must contain 'ops' field";
		return false;
	}

	const JSON& opsArray = json[U"ops"];
	if (!opsArray.isArray()) {
		errorMessage = U"'ops' field must be an array";
		return false;
	}

	// 各操作の検証
	for (size_t i = 0; i < opsArray.size(); ++i) {
		const JSON& op = opsArray[i];

		if (!op.isObject()) {
			errorMessage = U"Operation at index {} must be an object"_fmt(i);
			return false;
		}

		// 必須フィールドの確認
		Array<String> requiredFields = { U"x", U"y", U"n" };
		for (const String& field : requiredFields) {
			if (!op.hasElement(field)) {
				errorMessage = U"Operation at index {} missing required field: {}"_fmt(i, field);
				return false;
			}

			if (!op[field].isNumber()) {
				errorMessage = U"Operation at index {} field '{}' must be a number"_fmt(i, field);
				return false;
			}
		}
	}

	return true;
}

Optional<Field> JSONSolutionLoader::extractField(const JSON& json) {
	// フィールドデータが含まれているかチェック
	if (!json.hasElement(U"field") && !json.hasElement(U"board") && !json.hasElement(U"initial")) {
		return none;
	}

	// 複数の可能なフィールド名をチェック
	Array<String> fieldNames = { U"field", U"board", U"initial", U"initialField" };

	for (const String& fieldName : fieldNames) {
		if (json.hasElement(fieldName)) {
			const JSON& fieldJson = json[fieldName];

			if (validateFieldData(fieldJson)) {
				try {
					// フィールドデータからFieldオブジェクトを作成
					// ここでは仮の実装。実際のFieldクラスのfromJSON実装に依存
					return Field::fromJSON(fieldJson);
				}
				catch (const std::exception& e) {
					Print << U"Error creating field from JSON: {}"_fmt(Unicode::FromUTF8(e.what()));
				}
			}
		}
	}

	return none;
}

bool JSONSolutionLoader::validateFieldData(const JSON& fieldJson) {
	if (!fieldJson.isObject()) {
		return false;
	}

	// 基本的なフィールド構造の検証
	// 実際の検証ロジックはFieldクラスの構造に依存
	return fieldJson.hasElement(U"size") || fieldJson.hasElement(U"entities") || fieldJson.hasElement(U"data");
}
