/* Start Header ************************************************************************/
/*!
\file       LeaderboardLog.hpp
\project    Ocean Bound
\author     Jethro Sung
\date       April 06, 2025
\brief      This header provides functionality for logging individual player leaderboard entries.

            The log system appends a player's name and corresponding time to a persistent
            leaderboard file, enabling long-term record keeping and post-game evaluation.

            This functionality complements the main Leaderboard system by allowing real-time
            updates to the leaderboard without requiring full reinitialization or file overwrite.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents
without the prior written consent of DigiPen Institute of
Technology is prohibited.
*/
/* End Header **************************************************************************/
#ifndef LEADERBOARD_LOG_HPP
#define LEADERBOARD_LOG_HPP

#include <string>
#include <crtdbg.h> // To check for memory leaks
#include "AEEngine.h"


void logLeaderboardEntry(const std::string& filename, const std::string& playerName, float timer);

#endif // LEADERBOARD_LOG_HPP
#pragma once
