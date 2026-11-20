/* !
@file    	    JsonSerializer.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2025
@brief
    Implements the JSONSerializer class, providing functionality to create,
    load, save, parse, and manipulate JSON data in memory. The class supports
    reading and writing primitive types (int, float, bool, string) both by key
    in objects and sequentially in arrays, as well as navigating nested objects
    and arrays with a stack-based approach. It also maintains error states and
    supports inspection of current nodes, keys, and array bounds.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#ifndef JSONSERIALIZER_HPP
#define JSONSERIALIZER_HPP

// Engine Serialization Headers
#include "engine/serializer/ISerializer.hpp"

// Standard Library Headers
#include <cstdint>
#include <stack>
#include <string>
#include <vector>

// Third Party Headers
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace Engine::Serializer
{
    /*!***********************************************************************
    @brief JSONSerializer implements ISerializer to handle JSON data using nlohmann::json.
           Supports creation, loading, saving, parsing, reading/writing primitive types,
           and navigating nested objects/arrays with stack-based tracking.
    *************************************************************************/
    class JSONSerializer : public ISerializer
    {
        json data;                   // Root JSON object
        std::stack<json*> nodeStack; // Stack to handle nested objects/arrays
        size_t currentIndex = 0;     // Current index for sequential array access

        bool good_state = true;    // Serializer state flag
        std::string error_message; // Stores last error message
        bool eof_reached = false;  // Sequential read end-of-array flag

        // ---------------------- Helper functions ----------------------

        /*!***********************************************************************
        @brief Sets an error state with a message
        @param msg Error message to store
        *************************************************************************/
        void setError(const std::string& msg);

        /*!***********************************************************************
        @brief Checks if sequential read has exceeded array bounds
        *************************************************************************/
        void checkArrayBounds();

    public:
        /*!***********************************************************************
        @enum DataType
        @brief Represents the type of data currently stored or being accessed in a JSON node.
        *************************************************************************/
        enum DataType : std::uint8_t
        {
            NONE,
            FLOAT,
            INT,
            BOOL,
            STRING
        };

        /*!***********************************************************************
        @brief Default constructor
        *************************************************************************/
        JSONSerializer() = default;

        /*!***********************************************************************
        @brief Default destructor
        *************************************************************************/
        ~JSONSerializer() override = default;

        /*!***********************************************************************
        @brief Returns the current error message
        @return Current error message string
        *************************************************************************/
        std::string getError() const;

        /*!***********************************************************************
        @brief Creates a new empty JSON object and writes it to a file
        @param filename Name of the file to create
        *************************************************************************/
        void NewFile(const std::string& filename);

        /*!***********************************************************************
        @brief Loads JSON data from a file into the serializer
        @param filename Name of the file to load
        *************************************************************************/
        void LoadFile(const std::string& filename);

        /*!***********************************************************************
        @brief Saves the current JSON data to a file
        @param filename Name of the file to save
        *************************************************************************/
        void SaveFile(const std::string& filename);

        /*!***********************************************************************
        @brief Parses a JSON string and loads it into the serializer
        @param json JSON string to parse
        *************************************************************************/
        void ParseJSON(const std::string& json);

        /*!***********************************************************************
        @brief Creates a JSON object at the current node
        @param key Optional key for the object (empty for array element)
        *************************************************************************/
        void CreateObject(const std::string& key = "");

        /*!***********************************************************************
        @brief Creates a JSON array at the current node
        @param key Optional key for the array (empty for array element)
        *************************************************************************/
        void CreateArray(const std::string& key);

        /*!***********************************************************************
        @brief Removes a JSON object under the given key
        @param key Key of the object to remove
        *************************************************************************/
        void RemoveObject(const std::string& key);

        /*!***********************************************************************
        @brief Removes a JSON array under the given key
        @param key Key of the array to remove
        *************************************************************************/
        void RemoveArray(const std::string& key);

        // ---------------------- Key-based Reading ----------------------

        /*!***********************************************************************
        @brief Reads a float value by key from the current object
        @param key Key of the value to read
        @param f Reference to store the float value
        *************************************************************************/
        void ReadFloat(const std::string& key, float& f) override;

        /*!***********************************************************************
        @brief Reads an integer value by key from the current object
        @param key Key of the value to read
        @param i Reference to store the integer value
        *************************************************************************/
        void ReadInt(const std::string& key, int& i) override;

        /*!***********************************************************************
        @brief Reads a boolean value by key from the current object
        @param key Key of the value to read
        @param b Reference to store the boolean value
        *************************************************************************/
        void ReadBool(const std::string& key, bool& b) override;

        /*!***********************************************************************
        @brief Reads a string value by key from the current object
        @param key Key of the value to read
        @param s Reference to store the string value
        *************************************************************************/
        void ReadString(const std::string& key, std::string& s) override;

        // ---------------------- Key-based Writing ----------------------

        /*!***********************************************************************
        @brief Writes a float value by key to the current object
        @param key Key under which to write the value
        @param f Float value to write
        *************************************************************************/
        void WriteFloat(const std::string& key, float f) override;

        /*!***********************************************************************
        @brief Writes an integer value by key to the current object
        @param key Key under which to write the value
        @param i Integer value to write
        *************************************************************************/
        void WriteInt(const std::string& key, int i) override;

        /*!***********************************************************************
        @brief Writes a boolean value by key to the current object
        @param key Key under which to write the value
        @param b Boolean value to write
        *************************************************************************/
        void WriteBool(const std::string& key, bool b) override;

        /*!***********************************************************************
        @brief Writes a string value by key to the current object
        @param key Key under which to write the value
        @param s String value to write
        *************************************************************************/
        void WriteString(const std::string& key, const std::string& s) override;

        // --------------- Sequential Reading (for Arrays) ---------------

        /*!***********************************************************************
        @brief Reads the next float from the current array sequentially
        @param f Reference to store the float value
        *************************************************************************/
        void ReadFloat(float& f) override;

        /*!***********************************************************************
        @brief Reads the next integer from the current array sequentially
        @param i Reference to store the integer value
        *************************************************************************/
        void ReadInt(int& i) override;

        /*!***********************************************************************
        @brief Reads the next boolean from the current array sequentially
        @param b Reference to store the boolean value
        *************************************************************************/
        void ReadBool(bool& b) override;

        /*!***********************************************************************
        @brief Reads the next string from the current array sequentially
        @param s Reference to store the string value
        *************************************************************************/
        void ReadString(std::string& s) override;

        // --------------- Sequential Writing (for Arrays) ---------------

        /*!***********************************************************************
        @brief Appends a float value to the current array
        @param f Float value to append
        *************************************************************************/
        void WriteFloat(float f) override;

        /*!***********************************************************************
        @brief Appends an integer value to the current array
        @param i Integer value to append
        *************************************************************************/
        void WriteInt(int i) override;

        /*!***********************************************************************
        @brief Appends a boolean value to the current array
        @param b Boolean value to append
        *************************************************************************/
        void WriteBool(bool b) override;

        /*!***********************************************************************
        @brief Appends a string value to the current array
        @param s String value to append
        *************************************************************************/
        void WriteString(const std::string& s) override;

        // ---------------------- JSON-specific Methods ----------------------

        /*!***********************************************************************
        @brief Enters a child JSON object by key and sets it as current node
        @param key Key of the object to enter
        *************************************************************************/
        void EnterObject(const std::string& key);

        /*!***********************************************************************
        @brief Leaves the current object and returns to its parent
        *************************************************************************/
        void LeaveObject();

        /*!***********************************************************************
        @brief Enters a child JSON array by key and sets it as current node
        @param key Key of the array to enter
        *************************************************************************/
        void EnterArray(const std::string& key);

        /*!***********************************************************************
        @brief Enters a specific element of the current array
        @param index Index of the array element to enter
        *************************************************************************/
        void EnterArrayElement(size_t index);

        /*!***********************************************************************
        @brief Leaves the current array and returns to its parent
        *************************************************************************/
        void LeaveArray();

        /*!***********************************************************************
        @brief Resets the current node to the root node
        *************************************************************************/
        void ResetToRoot();

        /*!***********************************************************************
        @brief  Returns a reference to the current JSON node
        @return Reference to the current node
        *************************************************************************/
        const json& GetCurrentNode() const;

        /*!***********************************************************************
        @brief Sets the current node to a new JSON node
        @param node JSON node to set as current
        *************************************************************************/
        void SetCurrentNode(const json& node);

        /*!***********************************************************************
        @brief  Retrieves all keys from the current object node
        @return Vector of key strings
        *************************************************************************/
        std::vector<std::string> GetObjectKeys();

        /*!***********************************************************************
        @brief  Checks if a specific key exists in the current object node
        @param  key Key to check for
        @return True if key exists, false otherwise
        *************************************************************************/
        bool HasKey(const std::string& key);

        /*!***********************************************************************
        @brief  Checks if the current object node has a JSON object / array under a key
        @param  key Key to check for
        @return True if the key exists and holds an object (HasObject) or an array (HasArray)
        *************************************************************************/
        bool HasObject(const std::string& key) const;
        bool HasArray(const std::string& key) const;

        /*!***********************************************************************
        @brief  Reads a value by key only if the current object node has it with the right type
                (a number for TryReadFloat / TryReadInt, true / false for TryReadBool, text for
                TryReadString). Unlike ReadFloat etc., a missing or mistyped value neither throws
                nor overwrites the argument, so it keeps its default.
        @param  key Key of the value to read
        @param  value Reference that receives the value when it is there
        @return True if the value was read, false if it was missing or had another type
        *************************************************************************/
        bool TryReadFloat(const std::string& key, float& value) const;
        bool TryReadInt(const std::string& key, int& value) const;
        bool TryReadBool(const std::string& key, bool& value) const;
        bool TryReadString(const std::string& key, std::string& value) const;

        /*!***********************************************************************
        @brief  Returns the number of keys in the current object node
        @return Number of keys, or 0 if node is invalid
        *************************************************************************/
        size_t GetObjectSize() const;

        /*!***********************************************************************
        @brief  Returns the type of key in the current object node
        @return Type of key, or NONE if the type is not valid
        *************************************************************************/
        DataType GetDataType(const std::string& key);
    };
}

#endif /* JSONSERIALIZER_HPP */
