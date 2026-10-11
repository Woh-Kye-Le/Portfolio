/* Start Header ************************************************************************/
/*!
\file       leaderboard.cpp
\project    Ocean Bound
\author     Jethro Sung
\date       April, 06, 2025
\brief      This file manages the leaderboard system by reading, sorting, and displaying player scores from a file.
			It includes logic to handle file I/O operations, rank entries based on completion times and alphabetical order, 
			and render a graphical leaderboard popup with interactive closing functionality.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "leaderboard.h"
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <iostream>
#include <cmath>
#include "utils.hpp"

std::vector<LeaderboardEntry> Leaderboard::readFromFile(const std::string& filename) {
	ensureFileExists(filename);
	std::vector<LeaderboardEntry> entries;
	std::ifstream ifs(filename);
	if (!ifs)
	{
		std::cerr << "Error: Cannot open leaderboard file: " << filename << "\n";
		return entries;
	}
	std::string line;
	while (std::getline(ifs, line))
	{
		std::istringstream iss(line);
		LeaderboardEntry entry;
		if (!(iss >> entry.name >> entry.timer))
		{
			continue;
		}
		entries.push_back(entry);
	}
	std::cout << "Total entries read: " << entries.size() << "\n";
	return entries;
}

void Leaderboard::ensureFileExists(const std::string& filename) {
	std::ifstream infile(filename);
	if (!infile.good()) { // File doesn't exist or cannot be read
		std::ofstream outfile(filename);
		outfile.close();
	}
}

// So basically we wanna compare the timer, and if timer same, then by alphabet
static bool compareLeaderBoardEntries(const LeaderboardEntry& a, const LeaderboardEntry& b)
{
	if (fabs(a.timer - b.timer) < 0.01f)
	{
		return a.name < b.name;
	}
	return a.timer < b.timer;
}

void Leaderboard::sortEntries(std::vector<LeaderboardEntry>& entries) {
	std::sort(entries.begin(), entries.end(), compareLeaderBoardEntries);
}

void Leaderboard::displayEntries(const std::vector<LeaderboardEntry>& entries, s8 font, float x, float y, float lineSpacing, int topN)
{
	int count = (std::min)(topN, static_cast<int>(entries.size()));
	char buffer[128];
	float maxAllowedWidth = 0.25; //popup size - margin
	float baseCharwidth = 0.04f;
	int longestLength = 0;
	for (int i = 0; i < count; i++)
	{
		std::string line = std::to_string(i + 1) + ". " + entries[i].name + " - " + std::to_string(static_cast<int>(entries[i].timer));
		if (line.length() > static_cast<size_t>(longestLength))
		{
			longestLength = static_cast<int>(line.length());
		}
	}

	float scale = maxAllowedWidth / (longestLength * baseCharwidth);

	AEGfxPrint(font, "Leaderboard", -0.15f, 0.37f, 0.5f, 0.f, 0.f, 0.f, 1.f);

	for (int i = 0; i < count; i++)
	{
		int totalSeconds = static_cast<int>(entries[i].timer);
		int minutes = totalSeconds / 60;
		int seconds = totalSeconds % 60;
		sprintf_s(buffer, sizeof(buffer), "%d. %s - %02d:%02d", i + 1, entries[i].name.c_str(), minutes, seconds);
		std::cout << "Entry: " << entries[i].name.c_str() << "- " << minutes << ":" << seconds << "\n";
		AEGfxPrint(font, buffer, -0.22f, 0.3f - i * 0.075f, scale, 1.f, 1.f, 1.f, 1.f);
		//AEGfxPrint(font, buffer, 0.5, 0.5, 0.6f, 1.f, 1.f, 1.f, 1.f);
	}
}

void Leaderboard::displayLeaderboardPopup(s8 font, AEGfxVertexList* pMesh, AEGfxVertexList* closeMesh, bool* leaderboardPopup) {
	std::vector<LeaderboardEntry> entries = Leaderboard::readFromFile("leaderboard.txt");
	Leaderboard::sortEntries(entries);

	float popupX = -0.8f, popupY = 0.8f, popupWidth = 1.6f, popupHeight = 1.2f;
	/*AEGfxSetBackgroundColor(0.2f, 0.2f, 0.2f);
	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxSetColorToMultiply(1.f, 1.f, 1.f, 1.f);*/
	// Here, draw a rectangle; the exact API depends on your engine.
	// For example:
	// drawRectangle(popupX, popupY, popupWidth, popupHeight, 0.f, 0.f, 0.f, 0.8f);
	// (Replace drawRectangle with your actual function.)

	mesh_Draw(0.0f, 400.0f, 400.0f, 0.0f, 0.0f, pMesh);
	mesh_Draw(0.f, 30.f, 30.f, 170.f, 170.f, closeMesh);
	AEGfxPrint(font, "X", 0.2f, 0.35f, 0.5f, 1.f, 1.f, 1.f, 1.f);

	// Now display the leaderboard text on top.
	// Adjust the starting coordinates as needed.
	float textStartX = popupX + 0.1f;
	float textStartY = popupY - 0.1f;
	Leaderboard::displayEntries(entries, font, textStartX, textStartY, 0.06f, 10);

	if (checkCursorCollisionWithRectangle(970.f, 280.f, 30.f, 30.0f)) {
		if (AEInputCheckTriggered(AEVK_LBUTTON)) {
			*leaderboardPopup = false;
		}
	}
}