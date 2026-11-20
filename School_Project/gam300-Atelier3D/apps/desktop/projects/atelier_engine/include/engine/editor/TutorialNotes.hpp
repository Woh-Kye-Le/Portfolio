/* !
@file    	    TutorialNotes.hpp
@author  	    sanni.go@digipen.edu [100%]
@date    		30/09/2026
@brief
    Declares the tutorial notes shown on the editor's Modes page and their
    save/load functions.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Engine::Editor
{
    // One thing to do in a lesson. check says when it is done (LessonGoals.hpp); it is ticked off by itself.
    struct LessonStep
    {
        std::string text;
        std::string check;

        bool operator==(const LessonStep&) const = default;
    };

    // One card on the editor's Modes page: a name and its notes
    struct Tutorial
    {
        std::string name;
        std::string code;
        std::vector<LessonStep> steps; // none: only the notes in code, as a teacher's own lessons have

        bool operator==(const Tutorial&) const = default;
    };

    // A group of tutorials on the Modes page; nextIndex numbers the next "+ Add", done counts the lessons finished in
    // order (the learner's progress)
    struct TutorialMode
    {
        std::string name;
        std::vector<Tutorial> tutorials;
        int nextIndex = 1;
        int done = 0;

        bool operator==(const TutorialMode&) const = default;
    };

    // A game's tutorial notes (data/tutorials.editor.modes), JSON read and written with Serializer::JSONSerializer:
    //   { "version": 1,
    //     "modes": [ { "name": "Mode 1", "next_index": 2, "done": 0,
    //                  "tutorials": [ { "name": "Tutorial 1", "code": "...",
    //                                   "steps": [ { "text": "...", "check": "..." } ] } ] } ] }
    // Reading is forgiving: a missing or mistyped field keeps its default.
    class TutorialNotes
    {
    public:
        // Bump this when the layout changes in a way older builds cannot read
        static constexpr int kFormatVersion = 1;

        // The three empty modes a game starts with
        static std::vector<TutorialMode> Defaults();
        // Whether modes are still those three, as saved for a game no one wrote any notes in
        static bool IsUntouched(const std::vector<TutorialMode>& modes);
        // Whether the learner has got as far as a lesson: every track before it finished, and every lesson before it
        // in its own track. Lessons already done stay open, to go over them.
        static bool IsUnlocked(const std::vector<TutorialMode>& modes, int mode, int lesson);

        static bool Save(const std::vector<TutorialMode>& modes, const std::filesystem::path& path, std::string& error);
        // Leaves modes unchanged and returns false (with error) if the file is missing, is not valid JSON, is not laid
        // out as above or has an unsupported version
        static bool Load(std::vector<TutorialMode>& modes, const std::filesystem::path& path, std::string& error);
    };
}
