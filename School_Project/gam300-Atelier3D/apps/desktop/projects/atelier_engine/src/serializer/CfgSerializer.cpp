/* !
@file    	    CfgSerializer.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2025
@brief
    Implementation of CFGSerializer for reading, writing, and managing
    configuration files in INI/CFG format. Supports key-based and sequential
    access to float, int, bool, and string values, as well as section management
    and error tracking.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Serialization Headers
#include "engine/serializer/CfgSerializer.hpp"

// Standard Library Headers
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace Engine::Serializer
{
    // ---------------------- Helper functions ----------------------

    /*!***********************************************************************
    @brief          Trims whitespace from both ends of a string
    @param          str                     Input string
    @return         Trimmed string
    *************************************************************************/
    std::string CFGSerializer::trim(const std::string& str)
    {
        size_t start = str.find_first_not_of(" \t\r\n");
        size_t end = str.find_last_not_of(" \t\r\n");
        if (start == std::string::npos || end == std::string::npos)
        {
            return "";
        }
        return str.substr(start, end - start + 1);
    }

    /*!***********************************************************************
    @brief          Converts a string to lowercase
    @param          str                     Input string
    @return         Lowercase string
    *************************************************************************/
    std::string CFGSerializer::toLower(const std::string& str)
    {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](char c)
                       {
                           unsigned char uc = static_cast<unsigned char>(c);
                           char lc = static_cast<char>(std::tolower(uc));
                           return lc;
                       });
        return result;
    }

    /*!***********************************************************************
    @brief          Parses the currently opened file into the internal map
    @return         None
    *************************************************************************/
    void CFGSerializer::parseFile()
    {
        if (!file.is_open())
        {
            return;
        }

        std::string line;
        while (std::getline(file, line))
        {
            line = trim(line);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#' || line[0] == ';')
            {
                continue;
            }

            // Check for section header
            if (line[0] == '[' && line.back() == ']')
            {
                currentSection = toLower(trim(line.substr(1, line.size() - 2)));
                continue;
            }

            // Parse key-value pair
            size_t equalsPos = line.find('=');
            if (equalsPos != std::string::npos)
            {
                std::string key = toLower(trim(line.substr(0, equalsPos)));
                std::string value = trim(line.substr(equalsPos + 1));

                // Remove quotes if present
                if (value.size() >= 2 &&
                    ((value[0] == '"' && value.back() == '"') || (value[0] == '\'' && value.back() == '\'')))
                {
                    value = value.substr(1, value.size() - 2);
                }

                configData[currentSection][key] = value;
                sequentialKeys.push_back(currentSection + "." + key);
            }
        }

        file.clear();
        file.seekg(0);
    }

    /*!***********************************************************************
    @brief          Retrieves the value associated with a key in the current section
    @param          key                     Key to lookup
    @return         Value as string (empty if not found)
    *************************************************************************/
    std::string CFGSerializer::getValue(const std::string& key) const
    {
        std::string lowerKey = toLower(key);
        size_t dotPos = lowerKey.find('.');

        std::string section, actualKey;
        if (dotPos != std::string::npos)
        {
            section = lowerKey.substr(0, dotPos);
            actualKey = lowerKey.substr(dotPos + 1);
        }
        else
        {
            section = currentSection;
            actualKey = lowerKey;
        }

        auto sectionIt = configData.find(section);
        if (sectionIt != configData.end())
        {
            auto keyIt = sectionIt->second.find(actualKey);
            if (keyIt != sectionIt->second.end())
            {
                return keyIt->second;
            }
        }

        return "";
    }

    /*!***********************************************************************
    @brief          Sets the error message and marks the serializer as invalid
    @param          msg                     Error message
    @return         None
    *************************************************************************/
    void CFGSerializer::setError(const std::string& msg)
    {
        good_state = false;
        error_message = msg;
    }

    /*!***********************************************************************
    @brief          Retrieves the last error message from the serializer
    @return         Error message string
    *************************************************************************/
    const std::string& CFGSerializer::getError() const
    {
        return error_message;
    }

    /*!***********************************************************************
    @brief          Creates a new CFG file, clearing existing data. Starts with
                    a default global section. Overwrites file if it exists.
    @param          filename                Name of the file to create
    @return         None
    *************************************************************************/
    void CFGSerializer::NewFile(const std::string& filename)
    {
        fileName = filename;
        configData.clear();
        sequentialKeys.clear();
        currentSequentialIndex = 0;
        currentSection = "global";
        while (!sectionStack.empty())
        {
            sectionStack.pop();
        }
        sectionStack.emplace("global");

        if (file.is_open())
        {
            file.close();
        }
        file.open(fileName, std::ios::out | std::ios::trunc);
        if (file.is_open())
        {
            file << "[global]\n\n"; // start with a default global section
            file.close();
        }

        // reopen for reading/writing
        file.open(fileName, std::ios::in | std::ios::out);
    }

    /*!***********************************************************************
    @brief          Loads an existing CFG file into memory. Parses contents
                    into internal data structures.
    @param          filename                Name of the file to load
    @return         None
    *************************************************************************/
    void CFGSerializer::LoadFile(const std::string& filename)
    {
        fileName = filename;
        configData.clear();
        sequentialKeys.clear();
        currentSequentialIndex = 0;
        currentSection = "global";
        while (!sectionStack.empty())
        {
            sectionStack.pop();
        }
        sectionStack.emplace("global");

        if (file.is_open())
        {
            file.close();
        }
        file.open(fileName, std::ios::in);
        if (file.is_open())
        {
            parseFile(); // parse contents into configData
            file.close();
        }
        currentSection = "global"; // parseFile() leaves the last [section] of the file as current

        // reopen for reading/writing
        file.open(fileName, std::ios::in | std::ios::out);
    }

    /*!***********************************************************************
    @brief          Writes a key-value pair to the internal config map and
                    updates the file on disk
    @param          key                     Key to write
    @param          value                   Value to write
    @return         None
    *************************************************************************/
    void CFGSerializer::WriteValue(const std::string& key, const std::string& value)
    {
        if (key.empty())
        {
            return;
        }

        std::string lowerKey = toLower(key);
        size_t dotPos = lowerKey.find('.');

        std::string section, actualKey;
        if (dotPos != std::string::npos)
        {
            section = lowerKey.substr(0, dotPos);
            actualKey = lowerKey.substr(dotPos + 1);
        }
        else
        {
            section = currentSection;
            actualKey = lowerKey;
        }

        configData[section][actualKey] = value;

        std::string fullKey = section + "." + actualKey;
        if (std::find(sequentialKeys.begin(), sequentialKeys.end(), fullKey) == sequentialKeys.end())
        {
            sequentialKeys.push_back(fullKey);
        }

        // Rewrite file
        if (file.is_open())
        {
            file.close();
        }
        file.open(fileName, std::ios::out | std::ios::trunc);
        if (!file.is_open())
        {
            return;
        }

        for (const auto& secPair : configData)
        {
            file << "[" << secPair.first << "]\n";
            for (const auto& kv : secPair.second)
            {
                file << kv.first << " = " << kv.second << "\n";
            }
            file << "\n";
        }

        file.close();
        file.open(fileName, std::ios::in); // reopen for reading
    }

    // ---------------------- Key-based Reading ----------------------

    /*!***********************************************************************
    @brief          Reads a float value associated with a key
    @param          key                     Key to read
    @param          f                       Reference to store the float
    @return         None
    *************************************************************************/
    void CFGSerializer::ReadFloat(const std::string& key, float& f)
    {
        std::string value = getValue(key);
        if (!value.empty())
        {
            try
            {
                f = std::stof(value);
            }
            catch (...)
            {
                f = 0.0f;
            }
        }
        else
        {
            f = 0.0f;
        }
    }

    /*!***********************************************************************
    @brief          Reads an integer value associated with a key
    @param          key                     Key to read
    @param          i                       Reference to store the integer
    @return         None
    *************************************************************************/
    void CFGSerializer::ReadInt(const std::string& key, int& i)
    {
        std::string value = getValue(key);
        if (!value.empty())
        {
            try
            {
                i = std::stoi(value);
            }
            catch (...)
            {
                i = 0;
            }
        }
        else
        {
            i = 0;
        }
    }

    /*!***********************************************************************
    @brief          Reads a boolean value associated with a key
    @param          key                     Key to read
    @param          b                       Reference to store the boolean
    @return         None
    *************************************************************************/
    void CFGSerializer::ReadBool(const std::string& key, bool& b)
    {
        std::string value = getValue(key);
        if (!value.empty())
        {
            std::string lowerValue = toLower(value);

            if (lowerValue == "true" || lowerValue == "yes" || lowerValue == "on" || lowerValue == "enable" ||
                lowerValue == "1")
            {
                b = true;
            }
            else if (lowerValue == "false" || lowerValue == "no" || lowerValue == "off" || lowerValue == "disable" ||
                     lowerValue == "0")
            {
                b = false;
            }
            else
            {
                try
                {
                    int num = std::stoi(value);
                    b = (num != 0);
                }
                catch (...)
                {
                    b = false;
                }
            }
        }
        else
        {
            b = false;
        }
    }

    /*!***********************************************************************
    @brief          Reads a string value associated with a key
    @param          key                     Key to read
    @param          s                       Reference to store the string
    @return         None
    *************************************************************************/
    void CFGSerializer::ReadString(const std::string& key, std::string& s)
    {
        s = getValue(key);
    }

    // ---------------------- Key-based Writing ----------------------

    /*!***********************************************************************
    @brief          Writes a float value associated with a key
    @param          key                     Key to write
    @param          f                       Float value to write
    @return         None
    *************************************************************************/
    void CFGSerializer::WriteFloat(const std::string& key, float f)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << f; // keep decimal like 25.0
        WriteValue(key, oss.str());
    }

    /*!***********************************************************************
    @brief          Writes an integer value associated with a key
    @param          key                     Key to write
    @param          i                       Integer value to write
    @return         None
    *************************************************************************/
    void CFGSerializer::WriteInt(const std::string& key, int i)
    {
        WriteValue(key, std::to_string(i));
    }

    /*!***********************************************************************
    @brief          Writes a boolean value associated with a key
    @param          key                     Key to write
    @param          b                       Boolean value to write
    @return         None
    *************************************************************************/
    void CFGSerializer::WriteBool(const std::string& key, bool b)
    {
        WriteValue(key, b ? "true" : "false");
    }

    /*!***********************************************************************
    @brief          Writes a string value associated with a key
    @param          key                     Key to write
    @param          s                       String value to write
    @return         None
    *************************************************************************/
    void CFGSerializer::WriteString(const std::string& key, const std::string& s)
    {
        WriteValue(key, s);
    }

    // ---------------------- Sequential Reading ---------------------

    /*!***********************************************************************
    @brief          Reads the next float value sequentially
    @param          f                       Reference to store the float
    @return         None
    *************************************************************************/
    void CFGSerializer::ReadFloat(float& f)
    {
        if (currentSequentialIndex < sequentialKeys.size())
        {
            ReadFloat(sequentialKeys[currentSequentialIndex++], f);
        }
        else
        {
            f = 0.0f;
        }
    }

    /*!***********************************************************************
    @brief          Reads the next integer value sequentially
    @param          i                       Reference to store the integer
    @return         None
    *************************************************************************/
    void CFGSerializer::ReadInt(int& i)
    {
        if (currentSequentialIndex < sequentialKeys.size())
        {
            ReadInt(sequentialKeys[currentSequentialIndex++], i);
        }
        else
        {
            i = 0;
        }
    }

    /*!***********************************************************************
    @brief          Reads the next boolean value sequentially
    @param          b                       Reference to store the boolean
    @return         None
    *************************************************************************/
    void CFGSerializer::ReadBool(bool& b)
    {
        if (currentSequentialIndex < sequentialKeys.size())
        {
            ReadBool(sequentialKeys[currentSequentialIndex++], b);
        }
        else
        {
            b = false;
        }
    }

    /*!***********************************************************************
    @brief          Reads the next string value sequentially
    @param          s                       Reference to store the string
    @return         None
    *************************************************************************/
    void CFGSerializer::ReadString(std::string& s)
    {
        if (currentSequentialIndex < sequentialKeys.size())
        {
            ReadString(sequentialKeys[currentSequentialIndex++], s);
        }
        else
        {
            s = "";
        }
    }

    // ---------------------- Sequential Writing ----------------------

    /*!***********************************************************************
    @brief          Writes a float value sequentially
    @param          f                       Float value to write
    @return         None
    *************************************************************************/
    void CFGSerializer::WriteFloat(float f)
    {
        if (currentSequentialIndex < sequentialKeys.size())
        {
            WriteFloat(sequentialKeys[currentSequentialIndex++], f);
        }
    }

    /*!***********************************************************************
    @brief          Writes an integer value sequentially
    @param          i                       Integer value to write
    @return         None
    *************************************************************************/
    void CFGSerializer::WriteInt(int i)
    {
        if (currentSequentialIndex < sequentialKeys.size())
        {
            WriteInt(sequentialKeys[currentSequentialIndex++], i);
        }
    }

    /*!***********************************************************************
    @brief          Writes a boolean value sequentially
    @param          b                       Boolean value to write
    @return         None
    *************************************************************************/
    void CFGSerializer::WriteBool(bool b)
    {
        if (currentSequentialIndex < sequentialKeys.size())
        {
            WriteBool(sequentialKeys[currentSequentialIndex++], b);
        }
    }

    /*!***********************************************************************
    @brief          Writes a string value sequentially
    @param          s                       String value to write
    @return         None
    *************************************************************************/
    void CFGSerializer::WriteString(const std::string& s)
    {
        if (currentSequentialIndex < sequentialKeys.size())
        {
            WriteString(sequentialKeys[currentSequentialIndex++], s);
        }
    }

    // ---------------------- CFG-specific Methods ----------------------

    /*!***********************************************************************
    @brief          Enters a named section, creating it if it exists.
    @param          section                 Name of the section
    @return         None
    *************************************************************************/
    void CFGSerializer::EnterSection(const std::string& section)
    {
        std::string lowerSection = toLower(section);
        if (configData.find(lowerSection) != configData.end())
        {
            sectionStack.push(currentSection);
            currentSection = lowerSection;
            currentSequentialIndex = 0;
        }
    }

    /*!***********************************************************************
    @brief          Leaves the current section and returns to the previous one
    @return         None
    *************************************************************************/
    void CFGSerializer::LeaveSection()
    {
        if (sectionStack.size() > 1)
        {
            currentSection = sectionStack.top();
            sectionStack.pop();
            currentSequentialIndex = 0;
        }
    }

    /*!***********************************************************************
    @brief          Resets current section to the global section
    @return         None
    *************************************************************************/
    void CFGSerializer::ResetToGlobal()
    {
        while (sectionStack.size() > 1)
        {
            sectionStack.pop();
        }
        currentSection = "global";
        currentSequentialIndex = 0;
    }

    /*!***********************************************************************
    @brief          Checks whether a section exists
    @param          section                 Section name
    @return         True if section exists, false otherwise
    *************************************************************************/
    bool CFGSerializer::HasSection(const std::string& section) const
    {
        return configData.find(toLower(section)) != configData.end();
    }

    /*!***********************************************************************
    @brief          Checks whether a key exists in the current section
    @param          key                     Key name
    @return         True if key exists, false otherwise
    *************************************************************************/
    bool CFGSerializer::HasKey(const std::string& key) const
    {
        return !getValue(key).empty();
    }

    /*!***********************************************************************
    @brief          Returns a list of all section names
    @return         Vector of section names
    *************************************************************************/
    std::vector<std::string> CFGSerializer::GetSections() const
    {
        std::vector<std::string> sections;
        sections.reserve(configData.size());
        std::transform(configData.begin(), configData.end(), std::back_inserter(sections),
                       [](const auto& pair) { return pair.first; });
        return sections;
    }

    /*!***********************************************************************
    @brief          Returns a list of keys in a section (current section if empty)
    @param          section                 Optional section name
    @return         Vector of keys
    *************************************************************************/
    std::vector<std::string> CFGSerializer::GetKeysInSection(const std::string& section) const
    {
        std::vector<std::string> keys;
        std::string targetSection = section.empty() ? currentSection : toLower(section);

        auto it = configData.find(targetSection);
        if (it != configData.end())
        {
            keys.reserve(it->second.size());
            std::transform(it->second.begin(), it->second.end(), std::back_inserter(keys),
                           [](const auto& keyPair) { return keyPair.first; });
        }
        return keys;
    }
}
