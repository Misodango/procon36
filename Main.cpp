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

static const URL DEFAULT_HTTP_URL = U"192.168.3.33:3000";

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
		flatOpsArray.push_back(op.n); // Assuming 'n' is the size member in Operation struct
	}
	json[U"ops"] = flatOpsArray;
	return json;
}

enum class AppMode {
	SelectMode,
	InitializingGameplay,
	Gameplay,
	Solving,
	ShowingSolution,
	InitializingDataCollection,
	DataCollecting,
	InitializingHTTPGame,
	HTTPGameLoading,
};

// Forward declaration for CollectData
void CollectData(int32 num_samples_to_generate, const FilePath& output_directory);

void Main()
{
	// ウィンドウサイズを大きく設定
	Window::Resize(1200, 900);
	
	// ウィンドウスタイルをサイズ変更可能に設定
	Window::SetStyle(WindowStyle::Sizable);
	
	// シーンサイズもウィンドウサイズに合わせて設定
	Scene::Resize(1200, 900);
	
	Scene::SetBackground(ColorF{ 0.6, 0.8, 0.7 });
	Window::SetTitle(U"Puzzle Game & Data Collector");

	AppMode currentMode = AppMode::SelectMode;

	Optional<Field> currentField;
	Optional<Algorithm> algorithmInstance;
	Optional<PuzzleVisualizer> gameVisualizerInstance;
	Optional<AsyncTask<Solution>> solutionTask;
	Optional<Solution> solvedSolution;
	

	// HTTPの状態管理
	Optional<AsyncTask<Field>> httpFieldTask;
	Optional<AsyncTask<bool>> submissionTask;
	TextEditState customHTTPURL{ DEFAULT_HTTP_URL }; // デフォルトURL
	bool httpLoadingFailed = false;
	bool submissionInProgress = false;
	bool submissionSuccess = false;
	String httpErrorMessage;
	String submissionMessage;

	int32 selectedBoardSize = 8; // Default for gameplay
	double boardSizeSlider = static_cast<double>(selectedBoardSize);

	// Data collection settings
	const int32 num_data_samples = 10000; // Can be adjusted
	const FilePath data_output_dir = U"ml_data";


	while (System::Update())
	{
		// Mode selection buttons (conditionally displayed)
		if (currentMode == AppMode::SelectMode) {
			SimpleGUI::Headline(U"Select Mode", Vec2{ 20, 20 });
			if (SimpleGUI::Button(U"Gameplay Mode", Vec2{ 20, 60 }, 200)) {
				currentMode = AppMode::InitializingGameplay;
				// Reset states from other modes
				solutionTask.reset();
				solvedSolution.reset();
				currentField.reset();
				algorithmInstance.reset();
				gameVisualizerInstance.reset();
				httpFieldTask.reset();
				httpLoadingFailed = false;
			}
			if (SimpleGUI::Button(U"HTTP GET Mode", Vec2{ 20, 110 }, 200)) {
				currentMode = AppMode::InitializingHTTPGame;
				// Reset states
				solutionTask.reset();
				solvedSolution.reset();
				currentField.reset();
				algorithmInstance.reset();
				gameVisualizerInstance.reset();
				httpFieldTask.reset();
				httpLoadingFailed = false;
			}
			if (SimpleGUI::Button(U"Collect Data Mode", Vec2{ 20, 160 }, 200)) {
				currentMode = AppMode::InitializingDataCollection;
				solutionTask.reset();
				solvedSolution.reset();
				currentField.reset();
				algorithmInstance.reset();
				gameVisualizerInstance.reset();
				httpFieldTask.reset();
				httpLoadingFailed = false;
			}
		} else {
			if (SimpleGUI::Button(U"Back to Mode Select", Vec2{ Scene::Width() - 220, 20 }, 200)) {
				currentMode = AppMode::SelectMode;
				solutionTask.reset();
				solvedSolution.reset();
				currentField.reset();
				algorithmInstance.reset();
				gameVisualizerInstance.reset();
				httpFieldTask.reset();
				httpLoadingFailed = false;
			}
		}

		switch (currentMode)
		{
		case AppMode::InitializingHTTPGame:
		{
			SimpleGUI::Headline(U"HTTP Game Setup", Vec2{ 20, 100 });
			SimpleGUI::TextBox(customHTTPURL, Vec2{ 20, 140 }, 400);

			if(httpLoadingFailed) {
				SimpleGUI::Headline(httpErrorMessage, Vec2{ 20, 180 }, 400);
			}

			if (SimpleGUI::Button(U"Load Game from HTTP", Vec2{ 20, 200 })) {
				// 非同期でHTTPフィールドを取得
				httpFieldTask = Async([]() -> Field {
					return Field::fromHTTP(DEFAULT_HTTP_URL);
				});
				httpLoadingFailed = false;
				httpErrorMessage.clear();
				currentMode = AppMode::HTTPGameLoading;
			}

			if (SimpleGUI::Button(U"Use Custom URL", Vec2{ 20, 250 })) {
				if (!customHTTPURL.text.isEmpty()) {
					httpFieldTask = Async([customHTTPURL]() -> Field {
						return Field::fromHTTP(customHTTPURL.text);
					});
					httpLoadingFailed = false;
					httpErrorMessage.clear();
					currentMode = AppMode::HTTPGameLoading;
				}
			}

			if (submissionTask && submissionTask->isReady()) {
				submissionSuccess = submissionTask->get();
				submissionInProgress = false;
				submissionMessage = submissionSuccess ?
					U"Solution submitted successfully!" :
					U"Failed to submit solution. Check console for details.";
				submissionTask.reset();
			}

			if (submissionInProgress) {
				SimpleGUI::Headline(U"Submitting solution...", Vec2{ 20, Scene::Height() - 200 });
			}
			if (!submissionMessage.isEmpty()) {
				Color messageColor;
				if (submissionSuccess) {
					messageColor = Palette::Green;
				}
				else {
					messageColor = Palette::Red;
				}
				SimpleGUI::Headline(submissionMessage, Vec2{ 20, Scene::Height() - 200 }, 400);
			}
			break;
		}
		case AppMode::HTTPGameLoading:
		{
			SimpleGUI::Headline(U"Loading Game from HTTP...", Vec2{ 20, 100 });
			SimpleGUI::Headline(U"Please wait while the game is being downloaded.", Vec2{ 20, 140 });

			// 簡単なローディングアニメーション
			const double t = Scene::Time();
			const int32 dots = static_cast<int32>(t * 2) % 4;
			String loadingText = U"Loading";
			for (int32 i = 0; i < dots; ++i) {
				loadingText += U".";
			}
			SimpleGUI::Headline(loadingText, Vec2{ 20, 300 });

			if (httpFieldTask && httpFieldTask->isReady()) {
				Field loadedField = httpFieldTask->get();

				if (loadedField.getSize() > 0) {
					// 正常にロードされた場合
					currentField = loadedField;
					algorithmInstance.emplace(*currentField);
					solvedSolution.reset();
					solutionTask.reset();
					gameVisualizerInstance.emplace(*currentField, Solution{});
					currentMode = AppMode::Gameplay;
					Print << U"HTTPゲームを正常に開始しました";
				}
				else {
					// ロードに失敗した場合
					httpLoadingFailed = true;
					httpErrorMessage = U"Failed to load game from HTTP. Check URL and network connection.";
					currentMode = AppMode::InitializingHTTPGame;
				}
				httpFieldTask.reset();
			}

			// キャンセルボタン
			if (SimpleGUI::Button(U"Cancel", Vec2{ 20, 210 })) {
				httpFieldTask.reset();
				currentMode = AppMode::InitializingHTTPGame;
			}
			break;
		}
		case AppMode::InitializingGameplay:
		{
			SimpleGUI::Headline(U"Gameplay Setup", Vec2{ 20, 100 });
			SimpleGUI::Slider(U"Board Size (Even): {:.0f}"_fmt(boardSizeSlider), boardSizeSlider, 6.0, 24.0, Vec2{ 20, 140 }, 180, 100);
			selectedBoardSize = static_cast<int32>(boardSizeSlider);
			if (selectedBoardSize % 2 != 0) {
				selectedBoardSize = Max(6, selectedBoardSize - 1);
				boardSizeSlider = selectedBoardSize;
			}

			if (SimpleGUI::Button(U"Start Game", Vec2{ 20, 200 })) {
				currentField = Field::random(selectedBoardSize);
				algorithmInstance.emplace(*currentField);
				solvedSolution.reset();
				solutionTask.reset();
				gameVisualizerInstance.emplace(*currentField, Solution{});
				currentMode = AppMode::Gameplay;
			}
			break;
		}
		case AppMode::Gameplay:
		{
			if (not currentField) { // Should not happen if logic is correct
				currentMode = AppMode::InitializingGameplay;
				break;
			}

			SimpleGUI::Headline(U"Gameplay", Vec2{ 20, 60 });
			if (gameVisualizerInstance) {
				gameVisualizerInstance->draw(); // Draw the current board state
			}

			bool canSolve = algorithmInstance.has_value() && !solutionTask.has_value() && !solvedSolution.has_value();
			if (SimpleGUI::Button(U"Solve Puzzle", Vec2{ 20, Scene::Height() - 100 }, 180, canSolve)) {
				solutionTask = algorithmInstance->runAsync(Solution::Type::IterativeBeamSearch);
				currentMode = AppMode::Solving;
			}

			if (solvedSolution) {
				if (SimpleGUI::Button(U"Show Solution Steps", Vec2{ 20, Scene::Height() - 150 }, 220)) {
					// Ensure visualizer has the solved solution
					gameVisualizerInstance.emplace(*currentField, *solvedSolution);
					currentMode = AppMode::ShowingSolution;
				}
			}
			
			if (SimpleGUI::Button(U"New Game (Setup)", Vec2{ Scene::Width() - 220, Scene::Height() - 60 }, 200)) {
				currentMode = AppMode::InitializingGameplay;
				currentField.reset();
				algorithmInstance.reset();
				gameVisualizerInstance.reset();
				solutionTask.reset();
				solvedSolution.reset();
				submissionTask.reset();
				submissionInProgress = false;
				submissionSuccess = false;
				submissionMessage.clear();
			}

			if (solvedSolution && !submissionInProgress) {
				if (SimpleGUI::Button(U"Submit Solution", Vec2{ 250, Scene::Height() - 150 }, 180)) {
					submissionTask = Async([solvedSolution]() -> bool {
						return Field::submitSolution(*solvedSolution);
					});
					submissionInProgress = true;
					submissionSuccess = false;
					submissionMessage.clear();
				}
			}
			break;
		}
		case AppMode::Solving:
		{
			SimpleGUI::Headline(U"Solving...", Vec2{ 20, 60 });
			if (gameVisualizerInstance) {
				gameVisualizerInstance->draw(); // Keep drawing the board
			}
			if (solutionTask && solutionTask->isReady()) {
				solvedSolution = solutionTask->get();
				solutionTask.reset();
				if (solvedSolution->ops.empty()) { // Solution was not found (e.g. timeout in algorithm)
					Print << U"Solver finished, but no solution was found (timeout or no possible solution).";
					solvedSolution.reset(); // Indicate no solution
				} else {
					Print << U"Solution found with " << solvedSolution->ops.size() << U" operations.";
					// Update visualizer with the actual solution for drawing if needed, or wait for "Show Solution"
					gameVisualizerInstance.emplace(*currentField, *solvedSolution);
				}
				currentMode = AppMode::Gameplay; // Go back to gameplay to offer "Show Solution"
			} else if (solutionTask) {
				// Optionally, add a cancel button here
				// if (SimpleGUI::Button(U"Cancel Solve", Vec2{ 20, Scene::Height() - 50 })) {
				//    solutionTask.reset(); // This might not immediately stop the thread, depends on AsyncTask impl.
				//    currentMode = AppMode::Gameplay;
				// }
			} else { // Should not happen
				currentMode = AppMode::Gameplay;
			}
			break;
		}
		case AppMode::ShowingSolution:
		{
			SimpleGUI::Headline(U"Showing Solution", Vec2{ 20, 60 });
			if (gameVisualizerInstance && solvedSolution && currentField) {
				// Assuming PuzzleVisualizer::run() is a blocking call that handles its own loop and drawing.
				// And it was already updated with the solved solution.
				gameVisualizerInstance->run();
			}
			// After run() finishes, or if it couldn't run:
			currentMode = AppMode::Gameplay;
			// Optionally, reset visualizer to not show solution automatically next time in Gameplay
			if(currentField) gameVisualizerInstance.emplace(*currentField, Solution{});
			// Keep solvedSolution so user can view again, or reset if one-time view
			// solvedSolution.reset(); 
			break;
		}
		case AppMode::InitializingDataCollection:
		{
			// This mode immediately transitions to DataCollecting
			// Ensures that CollectData is called only once per mode switch.
			currentMode = AppMode::DataCollecting;
			// Call CollectData in a way that doesn't block the main thread if it's very long,
			// or make it clear it's a blocking operation.
			// For now, direct call:
			Print << U"Starting data collection...";
			CollectData(num_data_samples, data_output_dir);
			Print << U"Data collection finished.";
			// Automatically go back to select mode or gameplay setup
			currentMode = AppMode::SelectMode; 
			break;
		}
		case AppMode::DataCollecting:
		{
			// This state is mostly a placeholder if CollectData was async.
			// Since CollectData is called synchronously in InitializingDataCollection and then transitions,
			// this state might not be strictly necessary with the current synchronous call.
			// If CollectData were async, this mode would show progress.
			SimpleGUI::Headline(U"Data Collection in Progress...", Vec2{20, 60});
			// ... (display progress if CollectData was async) ...
			break;
		}
		case AppMode::SelectMode:
			// UI handled at the top of the loop
			break;
		}
	}
}


void CollectData(int32 num_samples_to_generate, const FilePath& output_directory)
{
	// This function contains the logic previously in Main for data generation.
	if (not FileSystem::IsDirectory(output_directory)) {
		FileSystem::CreateDirectories(output_directory);
	}

	for (int32 i = 0; i < num_samples_to_generate; ++i) {
		if (!System::Update()) return; // Allow exiting during long data collection

		int32 board_size = Random<int32>(3, 6) * 2; // Random board size: 6, 8, 10, 12
		Field field = Field::random(board_size);

		Algorithm algorithm(field);
		AsyncTask<Solution> dataSolutionTask = algorithm.runAsync(Solution::Type::IterativeBeamSearch);

		Print << U"Generating ML sample " << (i + 1) << U"/" << num_samples_to_generate << U" with size " << board_size;

		while (not dataSolutionTask.isReady()) {
			System::Update(); 
			if (not System::Update()) return; 
		}

		Solution solution = dataSolutionTask.get();

		JSON inputJson = FieldToJSON(field);
		JSON outputJson = SolutionToJSON(solution);

		JSON combinedJson;
		combinedJson[U"input"] = inputJson;
		combinedJson[U"output"] = outputJson;

		const String timestamp = DateTime::Now().format(U"yyyyMMdd_HHmmss_fff");
		const FilePath filePath = FileSystem::FullPath(output_directory + U"/ml_data_" + timestamp + U".json");

		if (combinedJson.save(filePath)) {
			Print << U"Saved: " << filePath;
		}
		else {
			Print << U"Error: Could not save file " << filePath;
		}
		// Add a small delay or yield to prevent UI freeze if many files are saved quickly,
		// though System::Update() in the loop helps.
		// System::Sleep(1ms); 
	}
	// No final System::Update() loop here, function returns after generation.
}
