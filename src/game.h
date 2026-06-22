#pragma once

#include <string>

// High-level state of a single play session.
enum class GameState
{
	Ready,    // waiting on the start screen
	Playing,  // round in progress, countdown running
	GameOver  // round finished, showing the score
};

// Owns the round logic (countdown, score, best score) and draws the ImGui HUD
// and start/restart screens. Rendering-agnostic: it only issues ImGui calls.
class Game
{
  public:
	explicit Game(float roundSeconds = 45.0f, std::string scoreFilePath = "highscore.txt");

	// Advances the countdown while playing; flips to GameOver when time runs out.
	void update(float deltaTimeSeconds);

	// Emits the ImGui HUD and the start / restart screens.
	void buildUi(float windowWidth, float windowHeight);

	// Counts a crate the player just shot (ignored unless a round is running).
	void registerCrateHit(int count = 1);

	// (Re)starts a round: resets the score and timer and enters Playing.
	void start();

	// Returns true exactly once after a round has (re)started, so the caller can
	// reset the world (tank, bullets, crates) in sync.
	[[nodiscard]] bool consumeJustStarted();

	[[nodiscard]] bool      isPlaying() const { return state == GameState::Playing; }
	[[nodiscard]] GameState getState() const { return state; }
	[[nodiscard]] int       getScore() const { return score; }
	[[nodiscard]] int       getBestScore() const { return bestScore; }
	[[nodiscard]] float     getTimeRemaining() const { return timeRemaining; }

  private:
	void loadBestScore();
	void saveBestScore() const;

	GameState   state            = GameState::Ready;
	float       roundSeconds     = 45.0f;
	float       timeRemaining    = 45.0f;
	int         score            = 0;
	int         bestScore        = 0;
	bool        justStarted      = false;
	std::string scoreFilePath;
};
