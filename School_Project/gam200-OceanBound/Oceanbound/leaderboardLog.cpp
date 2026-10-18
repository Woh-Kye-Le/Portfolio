/* Start Header ************************************************************************/
/*!
\file       leaderboardLog.cpp
\project    Ocean Bound
\author     Jethro Sung
\date       April, 06, 2025
\brief      This file provides functionality to log new player entries into the leaderboard file. It appends the player's name and completion time to a specified file, ensuring persistent storage of game results.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "LeaderboardLog.h"
#include <fstream>
#include <iostream>
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"


void logLeaderboardEntry(const std::string& filename, const std::string& playerName, float timer)
{
    // Open file in append mode.
    std::ofstream outfile(filename, std::ios::app);
    if (!outfile.is_open()) {
        std::cerr << "Error: Cannot open leaderboard file: " << filename << "\n";
        return;
    }
    // Write entry in the format: "Name timer"
    outfile << playerName << " " << timer << "\n";
    outfile.close();
}
