#include "GreedyAlgorithm.h"

/*
* @brief GreedyAlgorithmのコンストラクタ
* @param field フィールド
* @return GreedyAlgorithm
*/

GreedyAlgorithm::GreedyAlgorithm(const Field& field) : m_field(field) {}


/*
* @brief GreedyAlgorithmの実行
* @return Solution
*/

Solution GreedyAlgorithm::run()
{
	Solution solution;



	return solution;
}

/*
* @brief フィールド全体のペアの数をカウント
* @return int32
*/

int32 GreedyAlgorithm::countPairs() const
{
	const int32 dx[4] = { 1, 0, -1, 0 };
	const int32 dy[4] = { 0, 1, 0, -1 };

	int32 count = 0;
	Array<bool> seen(m_field.size * m_field.size / 2, false);
	for (size_t i = 0; i < m_field.size; i++)
	{
		for (size_t j = 0; j < m_field.size; j++)
		{
			if (seen[m_field.entities[i][j]])
			{
				continue;
			}
			for (size_t k = 0; k < 4; k++)
			{
				int32 x = i + dx[k];
				int32 y = j + dy[k];
				if (x < 0 || y < 0 || x >= m_field.size || y >= m_field.size)
				{
					continue;
				}
				if (m_field.entities[i][j] == m_field.entities[x][y])
				{
					count++;
				}
			}
			if (count == 2) {
				seen[m_field.entities[i][j]] = true;
			}
		}

	}
	return count;
}
