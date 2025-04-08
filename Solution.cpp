#include "Solution.h"

Solution::Solution() :ops() {}

/*
* @brief Solutionのコンストラクタ
* @param ops 操作の配列
* @return Solution
*/

Solution::Solution(Array<Operation> ops) : ops(ops) {}


/*
* @brief 操作を追加
* @param operation 操作
* @return void
*/

void Solution::add(Operation operation) {
	ops.emplace_back(operation);
}

/*
* @brief 別の解とマージ
* @param other 別の解
* @return void
*/

void Solution::merge(const Solution& other) {
	ops.insert(ops.end(), other.ops.begin(), other.ops.end());
}

/*
* @brief 操作にアクセス
* @param index インデックス
* @return 操作
*/

const Operation& Solution::operator[](int32 index) const {
	return ops[index];
}

/*
* @brief 操作の数を取得
* @return 操作の数
*/

int32 Solution::getOperationCount() const {
	return ops.size();
}

/*
* @brief 操作を取得
* @param index インデックス
* @param x x座標
* @param y y座標
* @param size サイズ
* @return void
*/

void Solution::getOperation(int32 index, int32& x, int32& y, int32& size) const {
	x = ops[index].x;
	y = ops[index].y;
	size = ops[index].n;
}

/*
* @brief JSONから解答を生成する
* @param json JSONオブジェクト
* @return 解答
*/
Solution Solution::fromJSON(const JSON& json) {
	Solution solution;
	
	return solution;
}

