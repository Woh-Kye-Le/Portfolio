/* !
@file               Process.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares RunProcess, which runs another program, waits for it and returns
    what it printed. The editor uses it to run the asset tool when a model is
    imported.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Standard Library Headers
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace Engine::Platform
{
    /*!***********************************************************************
    @brief          What a finished program left behind.
    *************************************************************************/
    struct ProcessResult
    {
        bool started = false;  // false: the program could not be started (missing, not executable)
        bool timedOut = false; // true: it ran too long and was killed
        int exitCode = -1;     // its exit code; -1 when it did not start, was killed or crashed
        std::string output;    // everything it printed, standard output and standard error together
    };

    /*!***********************************************************************
    @brief          Runs a program with the given arguments and waits for it, without opening a console window.
                    Nothing is read from the keyboard: the program sees an empty input.
    @param          program       Path of the executable.
    @param          arguments     The arguments, one per entry (no quoting needed, spaces are fine).
    @param          timeout       How long to wait before the program is killed.
    @return         The outcome; started is false when the program could not be launched.
    *************************************************************************/
    ProcessResult RunProcess(const std::filesystem::path& program, const std::vector<std::filesystem::path>& arguments,
                             std::chrono::seconds timeout = std::chrono::seconds(120));
}
