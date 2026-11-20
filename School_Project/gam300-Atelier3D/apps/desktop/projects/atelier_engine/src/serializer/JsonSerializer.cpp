/* !
@file    	    JsonSerializer.cpp
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

#include "engine/serializer/JsonSerializer.hpp"

// Standard Library Headers
#include <charconv>
#include <fstream>
#include <stdexcept>

namespace Engine::Serializer
{

    // ---------------------- Helper functions ----------------------

    namespace
    {
        // JSON numbers are doubles: a float stored as is comes out as e.g. 0.3499999940395355 for 0.35f. Store the
        // shortest decimal that reads back as the same float instead, so files stay readable and still load exactly.
        double ReadableNumber(float f)
        {
            char text[32];
            const auto written = std::to_chars(text, text + sizeof(text), f);
            double value = f;
            if (written.ec == std::errc())
            {
                std::from_chars(text, written.ptr, value);
            }
            return value;
        }
    }

    /*!***********************************************************************
    @brief          Sets the error state with a message
    @param          msg                     Error message
    @return         None
    *************************************************************************/
    void JSONSerializer::setError(const std::string& msg)
    {
        good_state = false;  // Mark state as bad
        error_message = msg; // Store error message
    }

    /*!***********************************************************************
    @brief          Checks if sequential read has exceeded array bounds
    @return         None
    *************************************************************************/
    void JSONSerializer::checkArrayBounds()
    {
        if (nodeStack.empty())
        {
            setError("No current node available");
            return;
        }
        auto& current = *nodeStack.top();
        if (!current.is_array())
        {
            setError("Current node is not an array");
            return;
        }
        if (currentIndex >= current.size())
        {
            eof_reached = true;
            setError("Sequential read beyond array bounds");
        }
    }

    /*!***********************************************************************
    @brief          Returns the current error message
    @return         Error message string
    *************************************************************************/
    std::string JSONSerializer::getError() const
    {
        return error_message;
    }

    /*!***********************************************************************
    @brief          Creates a new JSON object and writes it to a file
    @param          filename                Name of the file to create
    @return         None
    *************************************************************************/
    void JSONSerializer::NewFile(const std::string& filename)
    {
        data = json::object(); // Initialize root as an empty JSON object
        while (!nodeStack.empty())
        {
            nodeStack.pop(); // Clear any existing node stack
        }
        nodeStack.push(&data); // Push pointer to root object

        std::ofstream out(filename); // Open output file stream
        if (!out.is_open())
        {
            throw std::runtime_error("Failed to create file: " + filename);
        }
        out << data.dump(2); // Write JSON with indentation of 2 spaces
    }

    /*!***********************************************************************
    @brief          Loads a JSON file into the serializer
    @param          filename                Name of the file to load
    @return         None
    *************************************************************************/
    void JSONSerializer::LoadFile(const std::string& filename)
    {
        std::ifstream f(filename); // Open input file stream
        if (!f.is_open())
        {
            throw std::runtime_error("Failed to open: " + filename);
        }

        data = json::parse(f); // Parse JSON from file
        while (!nodeStack.empty())
        {
            nodeStack.pop(); // Clear existing node stack
        }
        nodeStack.push(&data); // Push pointer to root object
    }

    /*!***********************************************************************
    @brief          Saves the current JSON data to a file
    @param          filename                Name of the file to save to
    @return         None
    *************************************************************************/
    void JSONSerializer::SaveFile(const std::string& filename)
    {
        std::ofstream out(filename); // Open output file stream
        if (!out.is_open())
        {
            throw std::runtime_error("Failed to open file for writing: " + filename);
        }
        out << data.dump(2); // Write JSON with indentation of 2 spaces
    }

    /*!***********************************************************************
    @brief          Parses a JSON string into the serializer
    @param          jsonStr                 JSON string to parse
    @return         None
    *************************************************************************/
    void JSONSerializer::ParseJSON(const std::string& jsonStr)
    {
        data = json::parse(jsonStr); // Parse JSON from string
        while (!nodeStack.empty())
        {
            nodeStack.pop(); // Clear node stack
        }
        nodeStack.push(&data); // Push pointer to root
    }

    /*!***********************************************************************
    @brief          Creates a new JSON object in the current node
    @param          key                     Optional key for object (empty for array element)
    @return         None
    *************************************************************************/
    void JSONSerializer::CreateObject(const std::string& key)
    {
        if (nodeStack.empty())
        {
            return; // Nothing to do if stack is empty
        }
        auto& current = *nodeStack.top(); // Reference to current node

        if (key.empty())
        {
            if (current.is_array())
            {
                current.push_back(json::object()); // Add object to array
            }
        }
        else
        {
            if (current.is_object())
            {
                current[key] = json::object(); // Add object under key
            }
        }
    }

    /*!***********************************************************************
    @brief          Creates a new JSON array in the current node
    @param          key                     Optional key for array (empty for array element)
    @return         None
    *************************************************************************/
    void JSONSerializer::CreateArray(const std::string& key = "")
    {
        if (nodeStack.empty())
        {
            return; // Nothing to do if stack is empty
        }
        auto& current = *nodeStack.top(); // Reference to current node

        if (key.empty())
        {
            if (current.is_array())
            {
                current.push_back(json::array()); // Add array to array
            }
        }
        else
        {
            if (current.is_object())
            {
                current[key] = json::array(); // Add array under key
            }
        }
    }

    /*!***********************************************************************
    @brief          Removes a JSON object under the given key
    @param          key                     Key of the object to remove
    @return         None
    *************************************************************************/
    void JSONSerializer::RemoveObject(const std::string& key)
    {
        if (nodeStack.empty())
        {
            return;
        }
        auto& current = *nodeStack.top();

        if (current.is_object() && current.contains(key) && current[key].is_object())
        {
            current.erase(key); // Remove object from current node
        }
        else
        {
            throw std::runtime_error("Key not found or not an object");
        }
    }

    /*!***********************************************************************
    @brief          Removes a JSON array under the given key
    @param          key                     Key of the array to remove
    @return         None
    *************************************************************************/
    void JSONSerializer::RemoveArray(const std::string& key)
    {
        if (nodeStack.empty())
        {
            return;
        }
        auto& current = *nodeStack.top();

        if (current.is_object() && current.contains(key) && current[key].is_array())
        {
            current.erase(key); // Remove array from current node
        }
        else
        {
            throw std::runtime_error("Key not found or not an array");
        }
    }

    // ---------------------- Key-based Reading ----------------------

    /*!***********************************************************************
    @brief          Reads a float value from the current object using a key
    @param          key                     Key of the value to read
    @param          f                       Reference to store the float
    @return         None
    *************************************************************************/
    void JSONSerializer::ReadFloat(const std::string& key, float& f)
    {
        f = (*nodeStack.top()).value(key, 0.0f); // Retrieve value or default to 0.0f
    }

    /*!***********************************************************************
    @brief          Reads an integer value from the current object using a key
    @param          key                     Key of the value to read
    @param          i                       Reference to store the int
    @return         None
    *************************************************************************/
    void JSONSerializer::ReadInt(const std::string& key, int& i)
    {
        i = (*nodeStack.top()).value(key, 0); // Retrieve value or default to 0
    }

    /*!***********************************************************************
    @brief          Reads a boolean value from the current object using a key
    @param          key                     Key of the value to read
    @param          b                       Reference to store the bool
    @return         None
    *************************************************************************/
    void JSONSerializer::ReadBool(const std::string& key, bool& b)
    {
        b = (*nodeStack.top()).value(key, false); // Retrieve value or default to false
    }

    /*!***********************************************************************
    @brief          Reads a string value from the current object using a key
    @param          key                     Key of the value to read
    @param          s                       Reference to store the string
    @return         None
    *************************************************************************/
    void JSONSerializer::ReadString(const std::string& key, std::string& s)
    {
        s = (*nodeStack.top()).value(key, ""); // Retrieve value or default to empty string
    }

    // ---------------------- Key-based Writing ----------------------

    /*!***********************************************************************
    @brief          Writes a float value to the current object using a key
    @param          key                     Key of the value to write
    @param          f                       Float value to write
    @return         None
    *************************************************************************/
    void JSONSerializer::WriteFloat(const std::string& key, float f)
    {
        if (!nodeStack.empty() && (*nodeStack.top()).is_object())
        {
            (*nodeStack.top())[key] = ReadableNumber(f);
        }
    }

    /*!***********************************************************************
    @brief          Writes an integer value to the current object using a key
    @param          key                     Key of the value to write
    @param          i                       Integer value to write
    @return         None
    *************************************************************************/
    void JSONSerializer::WriteInt(const std::string& key, int i)
    {
        if (!nodeStack.empty() && (*nodeStack.top()).is_object())
        {
            (*nodeStack.top())[key] = i;
        }
    }

    /*!***********************************************************************
    @brief          Writes a boolean value to the current object using a key
    @param          key                     Key of the value to write
    @param          b                       Boolean value to write
    @return         None
    *************************************************************************/
    void JSONSerializer::WriteBool(const std::string& key, bool b)
    {
        if (!nodeStack.empty() && (*nodeStack.top()).is_object())
        {
            (*nodeStack.top())[key] = b;
        }
    }

    /*!***********************************************************************
    @brief          Writes a string value to the current object using a key
    @param          key                     Key of the value to write
    @param          s                       String value to write
    @return         None
    *************************************************************************/
    void JSONSerializer::WriteString(const std::string& key, const std::string& s)
    {
        if (!nodeStack.empty() && (*nodeStack.top()).is_object())
        {
            (*nodeStack.top())[key] = s;
        }
    }

    // --------------- Sequential Reading (for Arrays) ---------------

    /*!***********************************************************************
    @brief          Reads the next float value from the current array
    @param          f                       Reference to store the float
    @return         None
    *************************************************************************/
    void JSONSerializer::ReadFloat(float& f)
    {
        auto& current = *nodeStack.top();
        f = (current.is_array() && currentIndex < current.size()) ? current[currentIndex++].get<float>() : 0.0f;
    }

    /*!***********************************************************************
    @brief          Reads the next integer value from the current array
    @param          i                       Reference to store the integer
    @return         None
    *************************************************************************/
    void JSONSerializer::ReadInt(int& i)
    {
        auto& current = *nodeStack.top();
        i = (current.is_array() && currentIndex < current.size()) ? current[currentIndex++].get<int>() : 0;
    }

    /*!***********************************************************************
    @brief          Reads the next boolean value from the current array
    @param          b                       Reference to store the boolean
    @return         None
    *************************************************************************/
    void JSONSerializer::ReadBool(bool& b)
    {
        auto& current = *nodeStack.top();
        b = (current.is_array() && currentIndex < current.size()) ? current[currentIndex++].get<bool>() : false;
    }

    /*!***********************************************************************
    @brief          Reads the next string value from the current array
    @param          s                       Reference to store the string
    @return         None
    *************************************************************************/
    void JSONSerializer::ReadString(std::string& s)
    {
        auto& current = *nodeStack.top();
        s = (current.is_array() && currentIndex < current.size()) ? current[currentIndex++].get<std::string>() : "";
    }

    // --------------- Sequential Writing (for Arrays) ---------------

    /*!***********************************************************************
    @brief          Writes a float value to the current array
    @param          f                       Float value to write
    @return         None
    *************************************************************************/
    void JSONSerializer::WriteFloat(float f)
    {
        auto& current = *nodeStack.top();
        if (current.is_array())
        {
            current.push_back(ReadableNumber(f)); // Append to array
        }
        else
        {
            current = json::array({ReadableNumber(f)}); // Convert non-array to array with value
        }
    }

    /*!***********************************************************************
    @brief          Writes an integer value to the current array
    @param          i                       Integer value to write
    @return         None
    *************************************************************************/
    void JSONSerializer::WriteInt(int i)
    {
        auto& current = *nodeStack.top();
        if (current.is_array())
        {
            current.push_back(i);
        }
        else
        {
            current = json::array({i});
        }
    }

    /*!***********************************************************************
    @brief          Writes a boolean value to the current array
    @param          b                       Boolean value to write
    @return         None
    *************************************************************************/
    void JSONSerializer::WriteBool(bool b)
    {
        auto& current = *nodeStack.top();
        if (current.is_array())
        {
            current.push_back(b);
        }
        else
        {
            current = json::array({b});
        }
    }

    /*!***********************************************************************
    @brief          Writes a string value to the current array
    @param          s                       String value to write
    @return         None
    *************************************************************************/
    void JSONSerializer::WriteString(const std::string& s)
    {
        auto& current = *nodeStack.top();
        if (current.is_array())
        {
            current.push_back(s);
        }
        else
        {
            current = json::array({s});
        }
    }

    // ---------------------- JSON-specific Methods ----------------------

    /*!***********************************************************************
    @brief          Enters a JSON object under the given key
    @param          key                     Key of the object to enter
    @return         None
    *************************************************************************/
    void JSONSerializer::EnterObject(const std::string& key)
    {
        auto& current = *nodeStack.top(); // Reference to current node
        if (current.contains(key) && current[key].is_object())
        {
            nodeStack.push(&current[key]); // Push pointer to child object
        }
        currentIndex = 0; // Reset sequential read index
    }

    /*!***********************************************************************
    @brief          Leaves the current object and returns to its parent node
    @return         None
    *************************************************************************/
    void JSONSerializer::LeaveObject()
    {
        if (nodeStack.size() > 1)
        {                     // Do not pop root node
            nodeStack.pop();  // Remove current object from stack
            currentIndex = 0; // Reset sequential read index
        }
    }

    /*!***********************************************************************
    @brief          Enters a JSON array under the given key
    @param          key                     Key of the array to enter
    @return         None
    *************************************************************************/
    void JSONSerializer::EnterArray(const std::string& key)
    {
        auto& current = *nodeStack.top(); // Reference to current node
        if (current.contains(key) && current[key].is_array())
        {
            nodeStack.push(&current[key]); // Push pointer to child array
        }
        currentIndex = 0; // Reset sequential read index
    }

    /*!***********************************************************************
    @brief          Enters a specific element in the current array
    @param          index                   Index of the array element to enter
    @return         None
    *************************************************************************/
    void JSONSerializer::EnterArrayElement(size_t index)
    {
        auto& current = *nodeStack.top(); // Reference to current node
        if (current.is_array() && index < current.size())
        {
            nodeStack.push(&current[index]); // Push pointer to specific element
        }
        else
        {
            throw std::runtime_error("Cannot enter array element: invalid index or not an array");
        }
        currentIndex = 0; // Reset sequential read index
    }

    /*!***********************************************************************
    @brief          Leaves the current array and returns to its parent node
    @return         None
    *************************************************************************/
    void JSONSerializer::LeaveArray()
    {
        if (nodeStack.size() > 1)
        {                     // Do not pop root node
            nodeStack.pop();  // Remove current array from stack
            currentIndex = 0; // Reset sequential read index
        }
    }

    /*!***********************************************************************
    @brief          Resets the current node pointer to the root node
    @return         None
    *************************************************************************/
    void JSONSerializer::ResetToRoot()
    {
        while (nodeStack.size() > 1)
        {
            nodeStack.pop(); // Pop all nodes except root
        }
        currentIndex = 0; // Reset sequential read index
    }

    /*!***********************************************************************
    @brief          Returns a reference to the current node
    @return         Reference to the current JSON node
    *************************************************************************/
    const json& JSONSerializer::GetCurrentNode() const
    {
        return *nodeStack.top(); // Return reference to top of stack
    }

    /*!***********************************************************************
    @brief          Sets the current node to a new JSON node
    @param          node                    JSON node to set as current
    @return         None
    *************************************************************************/
    void JSONSerializer::SetCurrentNode(const json& node)
    {
        if (!nodeStack.empty())
        {
            nodeStack.pop(); // Remove current node
        }
        data = node;           // Replace root data
        nodeStack.push(&data); // Push pointer to new root
        currentIndex = 0;      // Reset sequential read index
    }

    /*!***********************************************************************
    @brief          Retrieves all keys of the current object node
    @return         Vector of key strings
    *************************************************************************/
    std::vector<std::string> JSONSerializer::GetObjectKeys()
    {
        std::vector<std::string> keys;
        if (nodeStack.empty())
        {
            setError("No current node available");
            return keys; // Stack empty, return empty vector
        }

        auto& current = *nodeStack.top();
        if (!current.is_object())
        {
            setError("Current node is not an object");
            return keys; // Not an object, return empty vector
        }

        for (auto it = current.begin(); it != current.end(); ++it)
        {
            keys.push_back(it.key()); // Collect all keys
        }

        return keys;
    }

    /*!***********************************************************************
    @brief          Checks if the current object node contains a specific key
    @param          key                     Key to check for
    @return         True if key exists, false otherwise
    *************************************************************************/
    bool JSONSerializer::HasKey(const std::string& key)
    {
        if (nodeStack.empty())
        {
            setError("No current node available");
            return false; // Stack empty return false
        }

        auto& current = *nodeStack.top();
        if (!current.is_object())
        {
            setError("Current node is not an object");
            return false; // Not an object return false
        }

        return current.contains(key); // Return true if key exists
    }

    namespace
    {
        // The value under key in node if node is an object that has it, otherwise nullptr
        const json* Find(const std::stack<json*>& nodeStack, const std::string& key)
        {
            if (nodeStack.empty() || !nodeStack.top()->is_object())
            {
                return nullptr;
            }
            const auto it = nodeStack.top()->find(key);
            return it == nodeStack.top()->end() ? nullptr : &*it;
        }
    }

    /*!***********************************************************************
    @brief          Checks if the current object node holds a JSON object under a key
    @param          key                     Key to check for
    @return         True if the key exists and holds an object
    *************************************************************************/
    bool JSONSerializer::HasObject(const std::string& key) const
    {
        const json* value = Find(nodeStack, key);
        return value && value->is_object();
    }

    /*!***********************************************************************
    @brief          Checks if the current object node holds a JSON array under a key
    @param          key                     Key to check for
    @return         True if the key exists and holds an array
    *************************************************************************/
    bool JSONSerializer::HasArray(const std::string& key) const
    {
        const json* value = Find(nodeStack, key);
        return value && value->is_array();
    }

    /*!***********************************************************************
    @brief          Reads a number by key if the current object node has one there
    @param          key                     Key of the value to read
    @param          value                   Receives the value; unchanged if it is missing or not a number
    @return         True if the value was read
    *************************************************************************/
    bool JSONSerializer::TryReadFloat(const std::string& key, float& value) const
    {
        const json* found = Find(nodeStack, key);
        if (!found || !found->is_number())
        {
            return false;
        }
        value = found->get<float>();
        return true;
    }

    /*!***********************************************************************
    @brief          Reads a whole number by key if the current object node has one there
    @param          key                     Key of the value to read
    @param          value                   Receives the value; unchanged if it is missing or not a whole number
    @return         True if the value was read
    *************************************************************************/
    bool JSONSerializer::TryReadInt(const std::string& key, int& value) const
    {
        const json* found = Find(nodeStack, key);
        if (!found || !found->is_number_integer())
        {
            return false;
        }
        value = found->get<int>();
        return true;
    }

    /*!***********************************************************************
    @brief          Reads true / false by key if the current object node has it there
    @param          key                     Key of the value to read
    @param          value                   Receives the value; unchanged if it is missing or not true / false
    @return         True if the value was read
    *************************************************************************/
    bool JSONSerializer::TryReadBool(const std::string& key, bool& value) const
    {
        const json* found = Find(nodeStack, key);
        if (!found || !found->is_boolean())
        {
            return false;
        }
        value = found->get<bool>();
        return true;
    }

    /*!***********************************************************************
    @brief          Reads text by key if the current object node has it there
    @param          key                     Key of the value to read
    @param          value                   Receives the value; unchanged if it is missing or not text
    @return         True if the value was read
    *************************************************************************/
    bool JSONSerializer::TryReadString(const std::string& key, std::string& value) const
    {
        const json* found = Find(nodeStack, key);
        if (!found || !found->is_string())
        {
            return false;
        }
        value = found->get<std::string>();
        return true;
    }

    /*!***********************************************************************
    @brief          Returns the number of keys in the current object node
    @return         Number of keys, or 0 if node is not an object or stack is empty
    *************************************************************************/
    size_t JSONSerializer::GetObjectSize() const
    {
        if (nodeStack.empty())
        {
            return 0; // Stack empty return size 0
        }
        auto& current = *nodeStack.top();
        if (!current.is_object())
        {
            return 0; // Not an object return size 0
        }
        return current.size(); // Return number of keys
    }

    /*!***********************************************************************
    @brief          Returns the type of key in the current object node
    @return         Type of key, or NONE if the type is not valid
    *************************************************************************/
    JSONSerializer::DataType JSONSerializer::GetDataType(const std::string& key)
    {
        if (nodeStack.empty())
        {
            return DataType::NONE;
        }
        auto& current = *nodeStack.top();
        if (!current.is_object())
        {
            return DataType::NONE;
        }
        if (!current.contains(key))
        {
            return DataType::NONE;
        }

        const auto& val = current.at(key);
        switch (val.type())
        {
        case json::value_t::number_integer:
        case json::value_t::number_unsigned: return DataType::INT; break;
        case json::value_t::number_float: return DataType::FLOAT; break;
        case json::value_t::boolean: return DataType::BOOL; break;
        case json::value_t::string: return DataType::STRING; break;
        default: return DataType::NONE; break;
        }
    }

}
