#include "JsonParser.h"
/*
* JSON パーサー
*
* @param path JSON ファイルのパス
* @return JSON Field オブジェクト
*/

JsonParser::JsonParser()
{

}

JsonParser::~JsonParser()
{

}

Field JsonParser::parseJson(const FilePath& path) {
	const JSON json = JSON::Load(path);

	if (json) {
		return Field::fromJSON(json);
	}
	else {
		Print << U"JSON ファイルの読み込みに失敗しました";
	}
	return Field(0);

}
