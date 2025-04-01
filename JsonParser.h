#pragma once
#include "Field.h"
/*
* JSON パーサー
*
* @param path JSON ファイルのパス
* @return JSON Field オブジェクト
*/

class JsonParser
{
public:
	JsonParser();
	~JsonParser();

	Field parseJson(const FilePath& path);

private:

};


