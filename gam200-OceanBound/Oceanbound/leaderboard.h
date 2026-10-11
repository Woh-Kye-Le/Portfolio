/* Start Header ************************************************************************/
/*!
\file       Leaderboard.hpp
\project    Ocean Bound
\author     Jethro Sung
\date       April 06, 2025
\brief      This header defines the Leaderboard class and related structures for managing and displaying
			player leaderboard data.

			The Leaderboard system provides functionality for reading from and writing to a file to persist
			player names and completion times. It supports sorting entries based on performance and rendering
			the top records on the screen.

			Functions include:
			- Reading leaderboard data from a file
			- Sorting leaderboard entries by completion time
			- Rendering a leaderboard UI popup and entries on screen
			- Ensuring the leaderboard data file exists and is accessible

			This system is intended to provide post-game feedback and motivation for players to improve
			their performance in future attempts.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef LEADERBOARD_HPP
#define LEADERBOARD_HPP

#include <string>
#include <vector>
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"

struct LeaderboardEntry {
	std::string name;
	float timer = 0.f;
};

class Leaderboard {
public:
	/*
	 So basically we wanna read fn from a file
	*/
	static std::vector<LeaderboardEntry> readFromFile(const std::string& filename);

	/*
		And now we wanna sort said entries
	*/
	static void sortEntries(std::vector<LeaderboardEntry>& entries);

	static void displayEntries(const std::vector<LeaderboardEntry>& entries, s8 font, float x, float y, float lineSpacing = 0.5f, int topN = 5);

	static void displayLeaderboardPopup(s8 font, AEGfxVertexList* pMesh, AEGfxVertexList* closeMesh, bool* leaderboardPopup);
	static void ensureFileExists(const std::string& filename);
};

#endif // !LEADERBOARD_HPP
