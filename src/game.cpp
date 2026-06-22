#include "game.h"

#include <algorithm>
#include <fstream>

#include <imgui.h>

Game::Game(float roundSeconds, std::string scoreFilePath)
    : roundSeconds(roundSeconds),
      timeRemaining(roundSeconds),
      scoreFilePath(std::move(scoreFilePath))
{
	loadBestScore();
}

void Game::start()
{
	score         = 0;
	timeRemaining = roundSeconds;
	state         = GameState::Playing;
	justStarted   = true;
}

bool Game::consumeJustStarted()
{
	const bool started = justStarted;
	justStarted        = false;
	return started;
}

void Game::registerCrateHit(int count)
{
	if (state == GameState::Playing)
	{
		score += count;
	}
}

void Game::update(float deltaTimeSeconds)
{
	if (state != GameState::Playing)
	{
		return;
	}

	timeRemaining -= deltaTimeSeconds;
	if (timeRemaining <= 0.0f)
	{
		timeRemaining = 0.0f;
		state         = GameState::GameOver;

		if (score > bestScore)
		{
			bestScore = score;
			saveBestScore();
		}
	}
}

void Game::buildUi(float windowWidth, float windowHeight)
{
	// --- Persistent status bar (top-left) -------------------------------------
	{
		ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.35f);
		const ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		                                  ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
		                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
		                                  ImGuiWindowFlags_NoNav;
		ImGui::Begin("##hud", nullptr, hudFlags);
		if (state == GameState::Playing)
		{
			ImGui::Text("Time   : %4.1f s", timeRemaining);
		}
		else
		{
			ImGui::Text("Time   :  --");
		}
		ImGui::Text("Crates : %d", score);
		ImGui::Text("Best   : %d", bestScore);
		ImGui::End();
	}

	// --- Start / Game-over screen (centered) ----------------------------------
	if (state != GameState::Playing)
	{
		ImGui::SetNextWindowPos(ImVec2(windowWidth * 0.5f, windowHeight * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		const ImGuiWindowFlags menuFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
		                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;
		ImGui::Begin("##menu", nullptr, menuFlags);

		if (state == GameState::Ready)
		{
			ImGui::Text("TANK CRATE SHOOTER");
			ImGui::Separator();
			ImGui::Text("Drive with the arrow keys, shoot with Space.");
			ImGui::Text("Destroy as many crates as you can in %.0f seconds!", roundSeconds);
			ImGui::Spacing();
			if (ImGui::Button("Start  (Enter)", ImVec2(220.0f, 44.0f)))
			{
				start();
			}
		}
		else // GameState::GameOver
		{
			ImGui::Text("TIME'S UP!");
			ImGui::Separator();
			ImGui::Text("You destroyed %d crates.", score);
			if (score >= bestScore && score > 0)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "New best score!");
			}
			else
			{
				ImGui::Text("Best: %d", bestScore);
			}
			ImGui::Spacing();
			if (ImGui::Button("Restart  (Enter)", ImVec2(220.0f, 44.0f)))
			{
				start();
			}
		}

		ImGui::End();
	}
}

void Game::loadBestScore()
{
	std::ifstream file(scoreFilePath);
	if (file.is_open())
	{
		int value = 0;
		if (file >> value && value > 0)
		{
			bestScore = value;
		}
	}
}

void Game::saveBestScore() const
{
	std::ofstream file(scoreFilePath, std::ios::trunc);
	if (file.is_open())
	{
		file << bestScore << '\n';
	}
}
