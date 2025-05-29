# include <Siv3D.hpp>
# include "Field.h"
# include "Algorithm.h"
# include "PuzzleVisualizer.h"

/*
* # procon36
*
* 熊本高専熊本キャンパス
*
* ゲーム全体の管理
*
* 0515 ひろし参上！
*/

// Helper function to convert Field to JSON
JSON FieldToJSON(const Field& field) {
	JSON json;
	json[U"size"] = field.getSize();
	
	for (int32 y = 0; y < field.getSize(); ++y) {
		Array<int32> row;
		for (int32 x = 0; x < field.getSize(); ++x) {
			row.push_back(field.entities[y][x]);
		}
		json[U"entities"].push_back(row);
	}
	return json;
}

// Helper function to convert Solution to JSON
JSON SolutionToJSON(const Solution& solution) {
	JSON json;
	json[U"num_operations"] = static_cast<int32>(solution.ops.size());
	Array<int32> flatOpsArray;
	for (const auto& op : solution.ops) {
		flatOpsArray.push_back(op.x);
		flatOpsArray.push_back(op.y);
		flatOpsArray.push_back(op.n);
	}
	json[U"ops"] = flatOpsArray;
	return json;
}

void Main()
{
	// 背景の色を設定する
	Scene::SetBackground(ColorF{ 0.6, 0.8, 0.7 });

	// フルスクリーン
	// Window::SetFullscreen(true); // ML data generation doesn't need fullscreen

	const int32 num_samples_to_generate = 10000; // Number of data samples to generate
	const FilePath output_directory = U"ml_data";

	if (not FileSystem::IsDirectory(output_directory)) {
		FileSystem::CreateDirectories(output_directory);
	}

	for (int32 i = 0; i < num_samples_to_generate; ++i) {
		int32 board_size = Random<int32>(3, 6) * 2; // Random board size between 6 and 10
		Field field = Field::random(board_size);

		Algorithm algorithm(field);
		// Use a longer timeout for the async task itself, 
		// the internal beam search timeout will handle the 300s limit.
		AsyncTask<Solution> solutionTask = algorithm.runAsync(Solution::Type::IterativeBeamSearch);

		Print << U"Generating sample " << (i + 1) << U"/" << num_samples_to_generate << U" with size " << board_size;

		// Wait for the task to complete.
		// The BeamSearchAlgorithm will internally handle its 300s timeout.
		while (not solutionTask.isReady()) {
			System::Update(); // Keep the system responsive
			if (not System::Update()) return; // Exit if the app is closed
		}

		Solution solution = solutionTask.get();

		JSON inputJson = FieldToJSON(field);
		JSON outputJson = SolutionToJSON(solution);

		JSON combinedJson;
		combinedJson[U"input"] = inputJson;
		combinedJson[U"output"] = outputJson;

		// Generate a unique filename
		const String timestamp = DateTime::Now().format(U"yyyyMMdd_HHmmss_fff");
		const FilePath filePath = FileSystem::FullPath(output_directory + U"/ml_data_" + timestamp + U".json");

		// Save to file
		if (combinedJson.save(filePath)) {
			Print << U"Saved: " << filePath;
		} else {
			Print << U"Error: Could not save file " << filePath;
		}
		
	}

	Print << U"Finished generating " << num_samples_to_generate << U" samples.";

	// Keep window open for a bit to see the final messages or remove for pure CLI.
	while (System::Update()) {
		if (KeyX.down()) break; // Exit on X key press
	}
}
