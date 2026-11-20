/* !
@file    	    TutorialNotes.cpp
@author  	    sanni.go@digipen.edu [100%]
@date    		30/09/2026
@brief
    Implements saving and loading the Modes page tutorial notes as versioned
    JSON with JSONSerializer.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/editor/TutorialNotes.hpp"

#include "engine/serializer/JsonSerializer.hpp"

#include <exception>
#include <format>

namespace Engine::Editor
{
    // What the Modes page shows the first time (three empty modes, numbering starts at 1)
    std::vector<TutorialMode> TutorialNotes::Defaults()
    {
        return {{"Mode 1", {}, 1}, {"Mode 2", {}, 1}, {"Mode 3", {}, 1}};
    }

    bool TutorialNotes::IsUntouched(const std::vector<TutorialMode>& modes)
    {
        return modes == Defaults();
    }

    bool TutorialNotes::IsUnlocked(const std::vector<TutorialMode>& modes, int mode, int lesson)
    {
        if (mode < 0 || mode >= static_cast<int>(modes.size()) || lesson < 0)
        {
            return false;
        }
        for (int i = 0; i < mode; ++i)
        {
            const TutorialMode& before = modes[static_cast<std::size_t>(i)];
            if (before.done < static_cast<int>(before.tutorials.size()))
            {
                return false;
            }
        }
        return lesson <= modes[static_cast<std::size_t>(mode)].done;
    }

    bool TutorialNotes::Save(const std::vector<TutorialMode>& modes, const std::filesystem::path& path,
                             std::string& error)
    {
        try
        {
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);

            // { "version": 1, "modes": [ { "name", "next_index", "done", "tutorials": [ { "name", "code" } ] } ] }
            Serializer::JSONSerializer json;
            json.ParseJSON("{}");
            json.WriteInt("version", kFormatVersion);
            json.CreateArray("modes");
            json.EnterArray("modes");
            for (std::size_t m = 0; m < modes.size(); ++m)
            {
                const TutorialMode& mode = modes[m];
                json.CreateObject();
                json.EnterArrayElement(m);
                json.WriteString("name", mode.name);
                json.WriteInt("next_index", mode.nextIndex);
                json.WriteInt("done", mode.done);
                json.CreateArray("tutorials");
                json.EnterArray("tutorials");
                for (std::size_t t = 0; t < mode.tutorials.size(); ++t)
                {
                    json.CreateObject();
                    json.EnterArrayElement(t);
                    json.WriteString("name", mode.tutorials[t].name);
                    json.WriteString("code", mode.tutorials[t].code);
                    json.CreateArray("steps");
                    json.EnterArray("steps");
                    for (std::size_t i = 0; i < mode.tutorials[t].steps.size(); ++i)
                    {
                        json.CreateObject();
                        json.EnterArrayElement(i);
                        json.WriteString("text", mode.tutorials[t].steps[i].text);
                        json.WriteString("check", mode.tutorials[t].steps[i].check);
                        json.LeaveObject();
                    }
                    json.LeaveArray();
                    json.LeaveObject();
                }
                json.LeaveArray();
                json.LeaveObject();
            }
            json.SaveFile(path.string());
            return true;
        }
        catch (const std::exception& e)
        {
            error = "Could not write " + path.string() + ": " + e.what();
            return false;
        }
    }

    // false + error message if the file is missing/broken; modes is only changed when it worked
    bool TutorialNotes::Load(std::vector<TutorialMode>& modes, const std::filesystem::path& path, std::string& error)
    {
        const auto fail = [&](const std::string& why)
        {
            error = "Could not read " + path.filename().string() + ": " + why;
            return false;
        };

        Serializer::JSONSerializer json;
        try
        {
            json.LoadFile(path.string());
        }
        catch (const std::exception& e)
        {
            return fail(e.what()); // missing file or invalid JSON
        }

        if (!json.GetCurrentNode().is_object() || !json.HasArray("modes"))
        {
            return fail("expected { \"version\": ..., \"modes\": [ ... ] }");
        }
        int version = 0;
        if (!json.TryReadInt("version", version) || version < 1 || version > kFormatVersion)
        {
            return fail(std::format("unsupported format version (this build reads version 1 to {})", kFormatVersion));
        }

        // Fill a separate list and only swap it in at the end, so a bad file never leaves modes half loaded.
        // Entries that aren't objects are just skipped.
        std::vector<TutorialMode> loaded;
        json.EnterArray("modes");
        const std::size_t modeCount = json.GetCurrentNode().size();
        for (std::size_t m = 0; m < modeCount; ++m)
        {
            json.EnterArrayElement(m);
            if (json.GetCurrentNode().is_object())
            {
                TutorialMode& mode = loaded.emplace_back();
                json.TryReadString("name", mode.name);
                json.TryReadInt("next_index", mode.nextIndex);
                json.TryReadInt("done", mode.done); // older files have none: no lessons done yet
                if (json.HasArray("tutorials"))
                {
                    json.EnterArray("tutorials");
                    const std::size_t tutorialCount = json.GetCurrentNode().size();
                    for (std::size_t t = 0; t < tutorialCount; ++t)
                    {
                        json.EnterArrayElement(t);
                        if (json.GetCurrentNode().is_object())
                        {
                            Tutorial& tutorial = mode.tutorials.emplace_back();
                            json.TryReadString("name", tutorial.name);
                            json.TryReadString("code", tutorial.code);
                            if (json.HasArray("steps"))
                            {
                                json.EnterArray("steps");
                                const std::size_t stepCount = json.GetCurrentNode().size();
                                for (std::size_t i = 0; i < stepCount; ++i)
                                {
                                    json.EnterArrayElement(i);
                                    if (json.GetCurrentNode().is_object())
                                    {
                                        LessonStep& step = tutorial.steps.emplace_back();
                                        json.TryReadString("text", step.text);
                                        json.TryReadString("check", step.check);
                                    }
                                    json.LeaveObject();
                                }
                                json.LeaveArray();
                            }
                        }
                        json.LeaveObject();
                    }
                    json.LeaveArray();
                }
            }
            json.LeaveObject();
        }
        modes = std::move(loaded);
        return true;
    }
}
