#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace std;

struct SolverStopwatch {
	chrono::steady_clock::time_point t0;
	SolverStopwatch() { reset(); }
	void reset() { t0 = chrono::steady_clock::now(); }
	double ms() const {
		return chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
	}
};

int map_size = 24;
const int savestates = 1000;

vector<vector<int>> maps;
vector<vector<int>> operations;
vector<vector<vector<int>>> best_operations;
vector<vector<vector<int>>> best_maps;
vector<vector<vector<int>>> current_best_operations;
vector<vector<vector<int>>> current_best_maps;

void reset_solver_state(int size) {
	map_size = size;
	maps.assign(map_size, vector<int>(map_size, 0));
	operations.clear();
	best_operations.clear();
	best_maps.clear();
	current_best_operations.clear();
	current_best_maps.clear();
}

int score(const vector<vector<int>>& ops, const vector<vector<int>>& mps) {
	(void)mps;
	return static_cast<int>(ops.size());
}

void save_beststate() {
	vector<pair<int, int>> idx;
	idx.reserve(current_best_operations.size());
	for (int i = 0; i < static_cast<int>(current_best_operations.size()); i++) {
		idx.push_back({ score(current_best_operations[i], current_best_maps[i]), i });
	}
	sort(idx.begin(), idx.end());
	best_operations.clear();
	best_maps.clear();
	for (int i = 0; i < min(savestates, static_cast<int>(idx.size())); i++) {
		best_operations.push_back(current_best_operations[idx[i].second]);
		best_maps.push_back(current_best_maps[idx[i].second]);
	}
	current_best_operations.clear();
	current_best_maps.clear();
}

void save_curstate() {
	current_best_operations.push_back(operations);
	current_best_maps.push_back(maps);
	if (current_best_operations.size() > savestates * 2) {
		vector<pair<int, int>> idx;
		idx.reserve(current_best_operations.size());
		for (int i = 0; i < static_cast<int>(current_best_operations.size()); i++) {
			idx.push_back({ score(current_best_operations[i], current_best_maps[i]), i });
		}
		sort(idx.begin(), idx.end());
		vector<vector<vector<int>>> temp_ops = current_best_operations;
		vector<vector<vector<int>>> temp_maps = current_best_maps;
		current_best_operations.clear();
		current_best_maps.clear();
		for (int i = 0; i < min(savestates, static_cast<int>(idx.size())); i++) {
			current_best_operations.push_back(temp_ops[idx[i].second]);
			current_best_maps.push_back(temp_maps[idx[i].second]);
		}
	}
}

void generate_maps(mt19937& rng) {
	vector<int> perm(map_size * map_size);
	iota(perm.begin(), perm.end(), 0);
	shuffle(perm.begin(), perm.end(), rng);
	for (int i = 0; i < map_size; i++) {
		for (int j = 0; j < map_size; j++) {
			maps[i][j] = perm[i * map_size + j] / 2 + 1;
		}
	}
}

void print_board(const vector<vector<int>>& board) {
	for (int i = 0; i < map_size; i++) {
		for (int j = 0; j < map_size; j++) {
			cout << setw(3) << board[i][j] << (j + 1 == map_size ? '\n' : ' ');
		}
	}
}

void rotate_map(int x, int y, int r) {
	if (x < 0 || y < 0 || x + r > map_size || y + r > map_size || r < 2) {
		return;
	}
	vector<vector<int>> temp(r, vector<int>(r));
	for (int i = 0; i < r; i++) {
		for (int j = 0; j < r; j++) {
			temp[j][r - 1 - i] = maps[x + i][y + j];
		}
	}
	for (int i = 0; i < r; i++) {
		for (int j = 0; j < r; j++) {
			maps[x + i][y + j] = temp[i][j];
		}
	}
	operations.push_back({ x, y, r });
}

pair<int, int> find_num(int num, int skip_x, int skip_y) {
	for (int i = 0; i < map_size; i++) {
		for (int j = 0; j < map_size; j++) {
			if (i == skip_x && j == skip_y) continue;
			if (maps[i][j] == num) {
				return { i, j };
			}
		}
	}
	return { -1, -1 };
}

bool rotate_board(vector<vector<int>>& board, int x, int y, int r) {
	int n = static_cast<int>(board.size());
	if (x < 0 || y < 0 || x + r > n || y + r > n || r < 2) return false;
	vector<vector<int>> temp(r, vector<int>(r));
	for (int i = 0; i < r; i++) {
		for (int j = 0; j < r; j++) {
			temp[j][r - 1 - i] = board[x + i][y + j];
		}
	}
	for (int i = 0; i < r; i++) {
		for (int j = 0; j < r; j++) {
			board[x + i][y + j] = temp[i][j];
		}
	}
	return true;
}

int count_pairs(const vector<vector<int>>& board) {
	int n = static_cast<int>(board.size());
	if (n == 0) return 0;
	int total = (n * n) / 2;
	vector<char> seen(total + 1, 0);
	int cnt = 0;
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			int v = board[i][j];
			if (v <= 0 || v > total || seen[v]) continue;
			bool adj = false;
			if (i > 0 && board[i - 1][j] == v) adj = true;
			else if (i + 1 < n && board[i + 1][j] == v) adj = true;
			else if (j > 0 && board[i][j - 1] == v) adj = true;
			else if (j + 1 < n && board[i][j + 1] == v) adj = true;
			if (adj) {
				seen[v] = 1;
				cnt++;
			}
		}
	}
	return cnt;
}

bool validate_board(const vector<vector<int>>& board, string& err) {
	int n = static_cast<int>(board.size());
	if (n == 0) {
		err = "empty board";
		return false;
	}
	int total = (n * n) / 2;
	vector<int> freq(total + 1, 0);
	vector<array<int, 4>> pos(total + 1, { -1, -1, -1, -1 });
	for (int i = 0; i < n; i++) {
		if (static_cast<int>(board[i].size()) != n) {
			err = "non-square board";
			return false;
		}
		for (int j = 0; j < n; j++) {
			int v = board[i][j];
			if (v < 1 || v > total) {
				err = "entity out of range";
				return false;
			}
			if (++freq[v] > 2) {
				err = "entity count mismatch";
				return false;
			}
			auto& p = pos[v];
			if (p[0] == -1) {
				p[0] = i;
				p[1] = j;
			}
			else {
				p[2] = i;
				p[3] = j;
			}
		}
	}
	for (int v = 1; v <= total; v++) {
		if (freq[v] != 2) {
			err = "entity count mismatch";
			return false;
		}
		auto& p = pos[v];
		int dist = abs(p[0] - p[2]) + abs(p[1] - p[3]);
		if (dist != 1) {
			err = "entity not adjacent";
			return false;
		}
	}
	err.clear();
	return true;
}

bool validate_solution(const vector<vector<int>>& start,
					   const vector<vector<int>>& final_board,
					   const vector<vector<int>>& ops,
					   string& err) {
	vector<vector<int>> cur = start;
	for (const auto& op : ops) {
		if (op.size() != 3) {
			err = "malformed operation";
			return false;
		}
		if (!rotate_board(cur, op[0], op[1], op[2])) {
			err = "invalid rotation";
			return false;
		}
	}
	if (cur != final_board) {
		err = "final board mismatch";
		return false;
	}
	return validate_board(final_board, err);
}

void run_solver_core() {
	save_curstate();
	save_beststate();
	//愚直をベースにビームサーチみたいなものをする
	//逆L字にそろえる
	for (int i = 0; i < map_size - 2; i += 2) {
		//下側
		int right_size = map_size - i;
		for (int k = map_size - 1 - i; k > map_size - 1 - i - 2; k--) {
			for (int j = map_size - 1 - i; j >= 1; j -= 2) {

				for (int idx = 0; idx < best_operations.size(); idx++) {
					operations = best_operations[idx];
					maps = best_maps[idx];

					vector<vector<int>> canbe_target; //一手で入れられるもの、{target, opx, opy, opr}
					for (int x = k; x >= max(0, k - j); x--) {
						canbe_target.push_back({ maps[x][j], x, j - (k - x), k - x + 1 });
					}

					for (auto ct : canbe_target) {
						maps = best_maps[idx];
						operations = best_operations[idx];
						int target = ct[0];
						if (ct[3] != 1) {
							rotate_map(ct[1], ct[2], ct[3]);
						}

						auto [x, y] = find_num(target, k, j);

						if (x == k && y == j - 1) {
							continue;
						}

						if (x == k) {
							if (y > j) {
								continue;
							}

							if (x + 1 >= j - y) {
								for (int l = 0; l < 3; l++) {
									rotate_map(k - (j - y) + 1, y, j - y);
								}
							}
							else {
								for (int l = 0; l < 2; l++) {
									rotate_map(0, y, x + 1);
								}

								x = find_num(target, k, j).first;
								y = find_num(target, k, j).second;

								while (y < j - k) {
									rotate_map(0, y, k + 1);
									y += k;
								}

								if (y != j - 1) {
									rotate_map(0, y, j - y);
								}

								rotate_map(0, j - (k + 1), k + 1);
							}
						}
						else if (y == j) {

						nexx:;

							if (x > k) {
								continue;
							}

							while (k - x - 1 >= j) {
								rotate_map(x, 0, j + 1);
								x += j;
							}

							if (k != x + 1) {
								rotate_map(x, j - (k - x - 1), k - x);
							}

							rotate_map(k - 1, j - 1, 2);

						}
						else if (x < k && y < j) {

							if (k - x - 1 <= j - y) {
								if (k - 1 < j - y) {
									if (x != 0) {
										rotate_map(0, y, x + 1);
									}

									x = find_num(target, k, j).first;
									y = find_num(target, k, j).second;

									int desti = j - (k - 1);
									while (desti - y >= k - 1) {
										rotate_map(0, y, k);
										y += k - 1;
									}

									if (desti != y) {
										rotate_map(0, y, desti - y + 1);
									}

									x = find_num(target, k, j).first;
									y = find_num(target, k, j).second;

									for (int l = 0; l < 2; l++) {
										rotate_map(0, y, k);
									}

									rotate_map(k - 1, j - 1, 2);

								}
								else {

									if (k - x - 1 != j - y) {
										rotate_map(k - 1 - (j - y), y, x - (k - (j - y)) + 2);
									}

									x = find_num(target, k, j).first;
									y = find_num(target, k, j).second;

									for (int l = 0; l < 2; l++) {
										rotate_map(x, y, k - x);
									}

									rotate_map(k - 1, j - 1, 2);

								}

							}
							else {

								rotate_map(x, y, j - y + 1);
								goto nexx;

							}

						}
						else {
							if (x > k || y < j) {
								continue;
							}

							//x < k && y > j
							if (k - x - 1 < y - j) {

								if (x != k - 1) {
									rotate_map(x, y - (k - x - 1), k - x);
								}

								x = find_num(target, k, j).first;
								y = find_num(target, k, j).second;

								while (y - j >= k - 1) {
									rotate_map(0, y - (k - 1), k);
									x = find_num(target, k, j).first;
									y = find_num(target, k, j).second;
								}

								if (y != j) {
									rotate_map(k - 1 - (y - j), j, y - j + 1);
								}

								rotate_map(k - 1, j - 1, 2);

							}
							else {

								if (right_size - j - 1 < k - x - 1) {

									if (y != right_size - 1) {
										rotate_map(x, y, right_size - y);
									}

									x = find_num(target, k, j).first;
									y = find_num(target, k, j).second;

									rotate_map(x, y - (k - x - 1), k - x);

									rotate_map(k - (right_size - j), j, right_size - j);

									rotate_map(k - 1, j - 1, 2);

								}
								else {

									rotate_map(x, y, j + (k - x - 1) - y + 1);

									for (int l = 0; l < 2; l++) {
										rotate_map(x, j, k - x);
									}

									rotate_map(k - 1, j - 1, 2);
								}
							}
						}
						save_curstate();
					}
				}
				save_beststate();
			}
		}

		//右側
		for (int k = map_size - 1 - i; k > map_size - 1 - i - 2; k--) {
			for (int j = 0; j < map_size - 2 - i; j += 2) {

				for (int idx = 0; idx < best_operations.size(); idx++) {
					operations = best_operations[idx];
					maps = best_maps[idx];
					vector<vector<int>> canbe_target; //一手で入れられるもの、{target, opx, opy, opr}
					for (int y = k; y >= max(0, k - (map_size - 2 - i - j) + 1); y--) {
						canbe_target.push_back({ maps[j][y], j, y, k - y + 1 });
					}
					for (auto ct : canbe_target) {
						maps = best_maps[idx];
						operations = best_operations[idx];
						int target = ct[0];
						if (ct[3] != 1) {
							rotate_map(ct[1], ct[2], ct[3]);
						}

						int target2 = maps[j][k];
						auto [x, y2] = find_num(target2, j, k);

						if (x == j + 1 && y2 == k) {
							continue;
						}

						if (y2 == k) {

							if (x < j) continue;

							for (int l = 0; l < 3; l++) {
								rotate_map(j + 1, k + 1 - (x - j), x - j);
							}

						}
						else if (x == j) {

						nexxx2:;

							if (y2 > k) continue;
							while (k - y2 - 1 >= map_size - i - 2 - j) {
								rotate_map(x, y2, map_size - i - 2 - j);
								y2 += map_size - i - j - 3;
							}

							if (y2 != k - 1) {
								rotate_map(x, y2, k - y2);
							}
							rotate_map(j, k - 1, 2);

						}
						else if (x > j && y2 < k) {

							if (x - j - 1 >= k - y2) {

								if (x - j - 1 != k - y2) {
									int dest = y2 - (k - (x - j - 1));
									rotate_map(x - dest, y2 - dest, dest + 1);
								}
								x = find_num(target2, j, k).first;
								y2 = find_num(target2, j, k).second;

								for (int l = 0; l < 2; l++) {
									rotate_map(j + 1, y2, x - j);
								}

							}
							else {

								if (x - j == k - y2) {
									rotate_map(x - 1, y2 - 1, 2);
									x = find_num(target2, j, k).first;
									y2 = find_num(target2, j, k).second;
								}

								rotate_map(j, y2, x - j + 1);
								x = find_num(target2, j, k).first;
								y2 = find_num(target2, j, k).second;
								goto nexxx2;

							}

						}
						else {

							//x < j && y2 < k
							if (x > j || y2 > k) continue;
							if (k - y2 - 1 <= j - x) {

								if (k - y2 - 1 != j - x) {
									int dest = j - x - (k - y2 - 1);
									rotate_map(x, y2 - dest, dest + 1);
								}

								x = find_num(target2, j, k).first;
								y2 = find_num(target2, j, k).second;
								for (int l = 0; l < 2; l++) {
									rotate_map(x, y2, k - y2);
								}
								rotate_map(j, k - 1, 2);

							}
							else {

								if (y2 < k - 1 - j) {
									rotate_map(0, y2, x + 1);
									x = find_num(target2, j, k).first;
									y2 = find_num(target2, j, k).second;
									int to = k - 1 - j;
									rotate_map(0, y2, to - y2 + 1);
								}
								x = find_num(target2, j, k).first;
								y2 = find_num(target2, j, k).second;

								if (k - y2 - 1 != j - x) {
									int dest = x - (j - (k - y2 - 1));
									rotate_map(x - dest, y2, dest + 1);
								}

								x = find_num(target2, j, k).first;
								y2 = find_num(target2, j, k).second;

								for (int l = 0; l < 2; l++) {
									rotate_map(x, y2, k - y2);
								}
								rotate_map(j, k - 1, 2);
							}
						}
						save_curstate();
					}
				}
				save_beststate();
			}
		}
	}

	for (int idx = 0; idx < best_operations.size(); idx++) {
		operations = best_operations[idx];
		maps = best_maps[idx];

		if (maps[0][0] == maps[1][1]) {
			rotate_map(2, 2, 2);
			for (int k = 0; k < 3; k++) {
				rotate_map(1, 0, 3);
			}
			rotate_map(2, 0, 2);
			for (int k = 0; k < 2; k++) {
				rotate_map(0, 1, 2);
			}
			for (int k = 0; k < 2; k++) {
				rotate_map(1, 0, 2);
			}
		}
		save_curstate();
	}
	save_beststate();

	if (!best_operations.empty()) {
		operations = best_operations[0];
		maps = best_maps[0];
	}

}

struct RunMetrics {
	int size = 0;
	int totalPairs = 0;
	int finalPairs = 0;
	size_t moveCount = 0;
	double elapsedMs = 0.0;
	bool valid = false;
	bool solved = false;
	string error;
};

struct SolveOutput {
	RunMetrics metrics;
	vector<vector<int>> initialBoard;
	vector<vector<int>> finalBoard;
	vector<vector<int>> ops;
	optional<uint64_t> seedUsed;
	bool jsonWritten = false;
};

struct RunConfig {
	bool quiet = false;
	bool printInitial = true;
	bool printFinal = true;
	bool printStats = true;
	bool printOps = false;
	bool writeJson = true;
	bool skipJsonOnFailure = true;
	optional<string> jsonPathOverride;
	string jsonDir = "datas";
};

static string format_ms(double ms) {
	ostringstream oss;
	oss << fixed << setprecision(2) << ms;
	return oss.str();
}

static bool write_solution_json(const string& path,
								const vector<vector<int>>& initial,
								const vector<vector<int>>& final_board,
								const vector<vector<int>>& ops,
								int size) {
	ofstream ofs(path);
	if (!ofs) return false;
	auto dump = [&](const vector<vector<int>>& board) {
		ofs << "[\n";
		for (int i = 0; i < size; i++) {
			ofs << "        [";
			for (int j = 0; j < size; j++) {
				ofs << board[i][j];
				if (j + 1 != size) ofs << ", ";
			}
			ofs << "]" << (i + 1 != size ? "," : "") << "\n";
		}
		ofs << "      ]";
		};
	ofs << "{\n  \"problem\": {\n    \"field\": {\n      \"size\": " << size << ",\n      \"entities\": ";
	dump(initial);
	ofs << "\n    }\n  },\n  \"solution\": {\n    \"ops\": [\n";
	for (size_t i = 0; i < ops.size(); i++) {
		const auto& op = ops[i];
		ofs << "      {\"x\": " << op[1] << ", \"y\": " << op[0] << ", \"n\": " << op[2] << "}";
		if (i + 1 != ops.size()) ofs << ",";
		ofs << "\n";
	}
	ofs << "    ],\n    \"final\": {\n      \"field\": {\n        \"size\": " << size << ",\n        \"entities\": ";
	dump(final_board);
	ofs << "\n      }\n    }\n  }\n}\n";
	return true;
}

static uint64_t mix_seed(uint64_t x) {
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccdULL;
	x ^= x >> 33;
	x *= 0xc4ceb9fe1a85ec53ULL;
	x ^= x >> 33;
	return x;
}

static uint64_t generate_seed() {
	random_device rd;
	return (static_cast<uint64_t>(rd()) << 32) ^ rd();
}

SolveOutput solve_once(int size, optional<uint64_t> boardSeed, const RunConfig& cfg) {
	SolveOutput out;
	RunMetrics metrics;
	metrics.size = size;
	metrics.totalPairs = (size * size) / 2;

	reset_solver_state(size);
	uint64_t seed = boardSeed.value_or(generate_seed());
	out.seedUsed = seed;
	mt19937 rng(seed);
	generate_maps(rng);
	out.initialBoard = maps;

	if (!cfg.quiet && cfg.printInitial) {
		cout << "Initial field (size " << size << "):\n";
		print_board(out.initialBoard);
		cout << "\n";
	}

	SolverStopwatch timer;
	run_solver_core();
	metrics.elapsedMs = timer.ms();

	out.finalBoard = maps;
	out.ops = operations;
	metrics.moveCount = out.ops.size();
	metrics.finalPairs = count_pairs(out.finalBoard);

	string validationError;
	metrics.valid = validate_solution(out.initialBoard, out.finalBoard, out.ops, validationError);
	if (!metrics.valid) {
		metrics.error = validationError;
	}
	metrics.solved = metrics.valid && metrics.finalPairs == metrics.totalPairs;

	if (cfg.writeJson && (metrics.valid || !cfg.skipJsonOnFailure)) {
		string path = cfg.jsonPathOverride.value_or(
			cfg.jsonDir + "/sabatya_" + to_string(size) + "x" + to_string(size) + "_solution.json"
		);
		out.jsonWritten = write_solution_json(path, out.initialBoard, out.finalBoard, out.ops, size);
		if (!out.jsonWritten) {
			cerr << "[warn] failed to write JSON: " << path << "\n";
		}
	}

	if (!cfg.quiet && cfg.printStats) {
		cout << "Moves: " << metrics.moveCount << "\n";
		cout << "Pairs: " << metrics.finalPairs << "/" << metrics.totalPairs << "\n";
		cout << "Elapsed: " << format_ms(metrics.elapsedMs) << " ms\n";
		cout << "Status: " << (metrics.solved ? "solved" : metrics.valid ? "incomplete" : "invalid") << "\n";
	}
	if (!cfg.quiet && cfg.printFinal) {
		cout << "\nFinal field:\n";
		print_board(out.finalBoard);
		cout << "\n";
	}
	if (!cfg.quiet && cfg.printOps) {
		cout << "Operations:\n";
		for (const auto& op : out.ops) {
			cout << op[0] << ' ' << op[1] << ' ' << op[2] << '\n';
		}
	}

	out.metrics = metrics;
	return out;
}

struct TestCaseResult {
	int size = 0;
	bool seeded = false;
	optional<uint64_t> seed;
	RunMetrics metrics;
};

struct TestSummary {
	vector<TestCaseResult> cases;
	double totalSeededMs = 0.0;
	double totalRandomMs = 0.0;
	size_t totalSeededMoves = 0;
	size_t totalRandomMoves = 0;
	size_t seededCases = 0;
	size_t randomCases = 0;
	bool allPassed = true;
};

static vector<int> parse_board_sizes(const string& csv) {
	vector<int> result;
	string token;
	stringstream ss(csv);
	while (getline(ss, token, ',')) {
		if (token.empty()) continue;
		int v = stoi(token);
		if (v < 6) v = 6;
		if (v % 2) v++;
		result.push_back(v);
	}
	sort(result.begin(), result.end());
	result.erase(unique(result.begin(), result.end()), result.end());
	return result;
}

class SolverTester {
public:
	SolverTester(RunConfig baseCfg, uint64_t seedBase, vector<int> sizes)
		: cfg(move(baseCfg)), seedBase(seedBase), sizes(move(sizes)) {
	}

	TestSummary run(bool verbose = true) const {
		TestSummary summary;
		for (int size : sizes) {
			uint64_t deterministicSeed = mix_seed(seedBase ^ static_cast<uint64_t>(size));
			SolveOutput seeded = solve_once(size, deterministicSeed, cfg);
			summary.cases.push_back({ size, true, deterministicSeed, seeded.metrics });
			summary.seededCases++;
			summary.totalSeededMs += seeded.metrics.elapsedMs;
			summary.totalSeededMoves += seeded.metrics.moveCount;
			if (verbose) log_case(summary.cases.back());
			if (!seeded.metrics.solved) {
				summary.allPassed = false;
				report_failure("deterministic", seeded);
				break;
			}

			SolveOutput randomRun = solve_once(size, nullopt, cfg);
			summary.cases.push_back({ size, false, randomRun.seedUsed, randomRun.metrics });
			summary.randomCases++;
			summary.totalRandomMs += randomRun.metrics.elapsedMs;
			summary.totalRandomMoves += randomRun.metrics.moveCount;
			if (verbose) log_case(summary.cases.back());
			if (!randomRun.metrics.solved) {
				summary.allPassed = false;
				report_failure("random", randomRun);
				break;
			}
		}
		return summary;
	}

	void print_summary(const TestSummary& summary) const {
		size_t totalCases = summary.seededCases + summary.randomCases;
		if (totalCases == 0) {
			cout << "[TEST] no cases executed\n";
			return;
		}
		cout << "[TEST] Summary: cases=" << totalCases
			<< " seeded=" << summary.seededCases
			<< " moves=" << summary.totalSeededMoves
			<< " total=" << format_ms(summary.totalSeededMs) << "ms"
			<< " random=" << summary.randomCases
			<< " moves=" << summary.totalRandomMoves
			<< " total=" << format_ms(summary.totalRandomMs) << "ms"
			<< " pass=" << (summary.allPassed ? "yes" : "no") << "\n";
	}

private:
	RunConfig cfg;
	uint64_t seedBase;
	vector<int> sizes;

	void log_case(const TestCaseResult& res) const {
		cout << "[TEST] size=" << res.size
			<< (res.seeded ? " seeded" : " random")
			<< " seed=" << (res.seed ? to_string(*res.seed) : "-")
			<< " moves=" << res.metrics.moveCount
			<< " pairs=" << res.metrics.finalPairs << "/" << res.metrics.totalPairs
			<< " time=" << format_ms(res.metrics.elapsedMs) << "ms"
			<< " status=" << (res.metrics.solved ? "OK" : res.metrics.valid ? "INCOMPLETE" : "INVALID")
			<< "\n";
	}

	void report_failure(const char* label, const SolveOutput& out) const {
		cerr << "[TEST][FAIL] " << label << " run failed ("
			<< out.metrics.finalPairs << "/" << out.metrics.totalPairs
			<< " pairs). Seed=" << (out.seedUsed ? to_string(*out.seedUsed) : "-") << "\n";
		cerr << "Final board:\n";
		print_board(out.finalBoard);
		cerr << "\n";
	}
};

int main(int argc, char** argv) {
	ios::sync_with_stdio(false);
	cin.tie(nullptr);

	int size = 24;
	optional<uint64_t> seed;
	bool runTests = false;
	bool quiet = false;
	bool printOps = false;
	bool noJson = false;
	bool keepJsonOnFailure = false;
	string jsonDir = "datas";
	optional<string> jsonPathOverride;
	optional<uint64_t> testSeedBase;
	vector<int> testSizes;
	vector<string> trailing;

	for (int i = 1; i < argc; i++) {
		string arg = argv[i];
		auto need_value = [&](const string& opt) -> string {
			if (i + 1 >= argc) throw runtime_error("missing value for " + opt);
			return argv[++i];
			};
		if (arg == "--size") {
			size = stoi(need_value(arg));
		}
		else if (arg == "--seed") {
			seed = strtoull(need_value(arg).c_str(), nullptr, 10);
		}
		else if (arg == "--json-dir") {
			jsonDir = need_value(arg);
		}
		else if (arg == "--json-path") {
			jsonPathOverride = need_value(arg);
		}
		else if (arg == "--no-json") {
			noJson = true;
		}
		else if (arg == "--keep-json-on-failure") {
			keepJsonOnFailure = true;
		}
		else if (arg == "--quiet") {
			quiet = true;
		}
		else if (arg == "--print-ops") {
			printOps = true;
		}
		else if (arg == "--test") {
			runTests = true;
		}
		else if (arg == "--test-sizes") {
			testSizes = parse_board_sizes(need_value(arg));
		}
		else if (arg == "--test-seed-base") {
			testSeedBase = strtoull(need_value(arg).c_str(), nullptr, 10);
		}
		else {
			trailing.push_back(arg);
		}
	}

	if (!runTests && !seed.has_value() && trailing.size() == 1) {
		size = stoi(trailing[0]);
	}

	if (size < 6) size = 6;
	if (size % 2) size++;

	if (runTests) {
		if (testSizes.empty()) {
			for (int n = 8; n <= 24; n += 2) testSizes.push_back(n);
		}
		RunConfig testCfg;
		testCfg.quiet = true;
		testCfg.printInitial = false;
		testCfg.printFinal = false;
		testCfg.printStats = false;
		testCfg.printOps = false;
		testCfg.writeJson = false;
		testCfg.skipJsonOnFailure = true;
		uint64_t base = testSeedBase.value_or(seed.value_or(0xC0FFEE123456789ULL));
		SolverTester tester(testCfg, base, testSizes);
		TestSummary summary = tester.run(true);
		tester.print_summary(summary);
		return summary.allPassed ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	RunConfig cfg;
	cfg.quiet = quiet;
	cfg.printInitial = !quiet;
	cfg.printFinal = !quiet;
	cfg.printStats = !quiet;
	cfg.printOps = printOps;
	cfg.writeJson = !noJson;
	cfg.skipJsonOnFailure = !keepJsonOnFailure;
	cfg.jsonDir = jsonDir;
	cfg.jsonPathOverride = jsonPathOverride;

	SolveOutput result = solve_once(size, seed, cfg);
	if (!result.metrics.valid) {
		cerr << "[error] validation failed: " << result.metrics.error << "\n";
		return EXIT_FAILURE;
	}
	if (!result.metrics.solved) {
		cerr << "[error] board not fully solved (" << result.metrics.finalPairs
			<< '/' << result.metrics.totalPairs << " pairs)\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
