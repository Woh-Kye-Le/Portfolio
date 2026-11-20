/* !
@file    	    CfgSerializer.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2025
@brief
    Implementation of CFGSerializer for reading, writing, and managing
    configuration files in INI/CFG format. Supports key-based and sequential
    access to float, int, bool, and string values, as well as section management
    and error tracking.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#ifndef CFGSERIALIZER_HPP
#define CFGSERIALIZER_HPP

// Engine Serialization Headers
#include "engine/serializer/ISerializer.hpp"

// Standard Library Headers
#include <fstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine::Serializer
{
    // Reads/writes simple INI-style files ("key = value", with [section] headers). We use it for game.cfg.
    class CFGSerializer : public ISerializer
    {
    private:
        std::fstream file;
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> configData;
        std::string currentSection = "global";
        std::vector<std::string> sequentialKeys;
        std::stack<std::string> sectionStack;
        std::string error_message;
        std::string fileName;
        size_t currentSequentialIndex = 0;
        bool good_state = true;

        // ---------------------- Helper functions ----------------------

        /*!***********************************************************************
        @brief          Trims whitespace from both ends of a string
        @param          str                     Input string
        @return         Trimmed string
        *************************************************************************/
        static std::string trim(const std::string& str);

        /*!***********************************************************************
        @brief          Converts a string to lowercase
        @param          str                     Input string
        @return         Lowercase string
        *************************************************************************/
        static std::string toLower(const std::string& str);

        /*!***********************************************************************
        @brief          Parses the currently opened file into the internal map
        @return         None
        *************************************************************************/
        void parseFile();

        /*!***********************************************************************
        @brief          Retrieves the value associated with a key in the current section
        @param          key                     Key to lookup
        @return         Value as string
        *************************************************************************/
        std::string getValue(const std::string& key) const;

        /*!***********************************************************************
        @brief          Sets an error message and marks serializer as invalid
        @param          msg                     Error message
        @return         None
        *************************************************************************/
        void setError(const std::string& msg);

    public:
        /*!***********************************************************************
        @brief          Default constructor
        @return         None
        *************************************************************************/
        CFGSerializer() = default;

        /*!***********************************************************************
        @brief          Default destructor
        @return         None
        *************************************************************************/
        ~CFGSerializer() override = default;

        /*!***********************************************************************
        @brief          Retrieves the last error message
        @return         Error message string
        *************************************************************************/
        const std::string& getError() const;

        /*!***********************************************************************
        @brief          Creates a new CFG file for writing
        @param          filename                Name of the file to create
        @return         None
        *************************************************************************/
        void NewFile(const std::string& filename);

        /*!***********************************************************************
        @brief          Loads an existing CFG file for reading and writing
        @param          filename                Name of the file to load
        @return         None
        *************************************************************************/
        void LoadFile(const std::string& filename);

        /*!***********************************************************************
        @brief          Writes a key/value pair to the current section
        @param          key                     Key to write
        @param          value                   Value to write
        @return         None
        *************************************************************************/
        void WriteValue(const std::string& key, const std::string& value);

        // ---------------------- Key-based Reading ----------------------

        /*!***********************************************************************
        @brief          Reads a float value associated with a key
        @param          key                     Key to read
        @param          f                       Reference to store the float
        @return         None
        *************************************************************************/
        void ReadFloat(const std::string& key, float& f) override;

        /*!***********************************************************************
        @brief          Reads an integer value associated with a key
        @param          key                     Key to read
        @param          i                       Reference to store the integer
        @return         None
        *************************************************************************/
        void ReadInt(const std::string& key, int& i) override;

        /*!***********************************************************************
        @brief          Reads a boolean value associated with a key
        @param          key                     Key to read
        @param          b                       Reference to store the boolean
        @return         None
        *************************************************************************/
        void ReadBool(const std::string& key, bool& b) override;

        /*!***********************************************************************
        @brief          Reads a string value associated with a key
        @param          key                     Key to read
        @param          s                       Reference to store the string
        @return         None
        *************************************************************************/
        void ReadString(const std::string& key, std::string& s) override;

        // ---------------------- Key-based Writing ----------------------

        /*!***********************************************************************
        @brief          Writes a float value associated with a key
        @param          key                     Key to write
        @param          f                       Float value to write
        @return         None
        *************************************************************************/
        void WriteFloat(const std::string& key, float f) override;

        /*!***********************************************************************
        @brief          Writes an integer value associated with a key
        @param          key                     Key to write
        @param          i                       Integer value to write
        @return         None
        *************************************************************************/
        void WriteInt(const std::string& key, int i) override;

        /*!***********************************************************************
        @brief          Writes a boolean value associated with a key
        @param          key                     Key to write
        @param          b                       Boolean value to write
        @return         None
        *************************************************************************/
        void WriteBool(const std::string& key, bool b) override;

        /*!***********************************************************************
        @brief          Writes a string value associated with a key
        @param          key                     Key to write
        @param          s                       String value to write
        @return         None
        *************************************************************************/
        void WriteString(const std::string& key, const std::string& s) override;

        // ---------------------- Sequential Reading ----------------------

        /*!***********************************************************************
        @brief          Reads the next float value sequentially
        @param          f                       Reference to store the float
        @return         None
        *************************************************************************/
        void ReadFloat(float& f) override;

        /*!***********************************************************************
        @brief          Reads the next integer value sequentially
        @param          i                       Reference to store the integer
        @return         None
        *************************************************************************/
        void ReadInt(int& i) override;

        /*!***********************************************************************
        @brief          Reads the next boolean value sequentially
        @param          b                       Reference to store the boolean
        @return         None
        *************************************************************************/
        void ReadBool(bool& b) override;

        /*!***********************************************************************
        @brief          Reads the next string value sequentially
        @param          s                       Reference to store the string
        @return         None
        *************************************************************************/
        void ReadString(std::string& s) override;

        // ---------------------- Sequential Writing ----------------------

        /*!***********************************************************************
        @brief          Writes a float value sequentially
        @param          f                       Float value to write
        @return         None
        *************************************************************************/
        void WriteFloat(float f) override;

        /*!***********************************************************************
        @brief          Writes an integer value sequentially
        @param          i                       Integer value to write
        @return         None
        *************************************************************************/
        void WriteInt(int i) override;

        /*!***********************************************************************
        @brief          Writes a boolean value sequentially
        @param          b                       Boolean value to write
        @return         None
        *************************************************************************/
        void WriteBool(bool b) override;

        /*!***********************************************************************
        @brief          Writes a string value sequentially
        @param          s                       String value to write
        @return         None
        *************************************************************************/
        void WriteString(const std::string& s) override;

        // ---------------------- CFG-specific Methods ----------------------

        /*!***********************************************************************
        @brief          Enters a named section, creating it if it doesn't exist
        @param          section                 Name of the section
        @return         None
        *************************************************************************/
        void EnterSection(const std::string& section);

        /*!***********************************************************************
        @brief          Leaves the current section, returning to previous section
        @return         None
        *************************************************************************/
        void LeaveSection();

        /*!***********************************************************************
        @brief          Resets current section to the global section
        @return         None
        *************************************************************************/
        void ResetToGlobal();

        /*!***********************************************************************
        @brief          Checks whether a section exists
        @param          section                 Name of the section
        @return         True if the section exists, false otherwise
        *************************************************************************/
        bool HasSection(const std::string& section) const;

        /*!***********************************************************************
        @brief          Checks whether a key exists in the current section
        @param          key                     Key to check
        @return         True if the key exists, false otherwise
        *************************************************************************/
        bool HasKey(const std::string& key) const;

        /*!***********************************************************************
        @brief          Returns a list of all section names
        @return         Vector of section names
        *************************************************************************/
        std::vector<std::string> GetSections() const;

        /*!***********************************************************************
        @brief          Returns a list of keys in a section (current section if empty)
        @param          section                 Optional section name
        @return         Vector of keys
        *************************************************************************/
        std::vector<std::string> GetKeysInSection(const std::string& section = "") const;
    };
}

#endif /* CFGSERIALIZER_HPP */
