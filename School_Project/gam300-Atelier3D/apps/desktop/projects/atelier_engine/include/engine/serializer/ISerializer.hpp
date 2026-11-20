/* !
@file    	    ISerializer.hpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2025
@brief
    Provides default implementations of sequential read/write methods that
    delegate to key-based versions using an empty string as the default key,
    allowing primitive types to be read from or written to the current node
    without specifying a key explicitly.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#ifndef ISERIALIZER_HPP
#define ISERIALIZER_HPP

// Standard Library Headers
#include <string>

namespace Engine::Serializer
{
    // Common interface for our file formats (CFGSerializer and JSONSerializer), so code that reads/writes
    // values doesn't have to care which format it is
    class ISerializer
    {
    public:
        // ----- Key-based reading -----

        /*!***********************************************************************
        @brief Reads a float value by key
        @param key Key of the value to read
        @param f Reference to store the float value
        *************************************************************************/
        virtual void ReadFloat(const std::string& key, float& f) = 0;

        /*!***********************************************************************
        @brief Reads an integer value by key
        @param key Key of the value to read
        @param i Reference to store the integer value
        *************************************************************************/
        virtual void ReadInt(const std::string& key, int& i) = 0;

        /*!***********************************************************************
        @brief Reads a boolean value by key
        @param key Key of the value to read
        @param b Reference to store the boolean value
        *************************************************************************/
        virtual void ReadBool(const std::string& key, bool& b) = 0;

        /*!***********************************************************************
        @brief Reads a string value by key
        @param key Key of the value to read
        @param s Reference to store the string value
        *************************************************************************/
        virtual void ReadString(const std::string& key, std::string& s) = 0;

        // ----- Key-based writing -----

        /*!***********************************************************************
        @brief Writes a float value by key
        @param key Key under which to write the value
        @param f Float value to write
        *************************************************************************/
        virtual void WriteFloat(const std::string& key, float f) = 0;

        /*!***********************************************************************
        @brief Writes an integer value by key
        @param key Key under which to write the value
        @param i Integer value to write
        *************************************************************************/
        virtual void WriteInt(const std::string& key, int i) = 0;

        /*!***********************************************************************
        @brief Writes a boolean value by key
        @param key Key under which to write the value
        @param b Boolean value to write
        *************************************************************************/
        virtual void WriteBool(const std::string& key, bool b) = 0;

        /*!***********************************************************************
        @brief Writes a string value by key
        @param key Key under which to write the value
        @param s String value to write
        *************************************************************************/
        virtual void WriteString(const std::string& key, const std::string& s) = 0;

        // ----- Sequential reading/writing -----

        /*!***********************************************************************
        @brief Reads the next float from the current node (default key)
        @param f Reference to store the float value
        *************************************************************************/
        virtual void ReadFloat(float& f);

        /*!***********************************************************************
        @brief Reads the next integer from the current node (default key)
        @param i Reference to store the integer value
        *************************************************************************/
        virtual void ReadInt(int& i);

        /*!***********************************************************************
        @brief Reads the next boolean from the current node (default key)
        @param b Reference to store the boolean value
        *************************************************************************/
        virtual void ReadBool(bool& b);

        /*!***********************************************************************
        @brief Reads the next string from the current node (default key)
        @param s Reference to store the string value
        *************************************************************************/
        virtual void ReadString(std::string& s);

        /*!***********************************************************************
        @brief Writes a float to the current node (default key)
        @param f Float value to write
        *************************************************************************/
        virtual void WriteFloat(float f);

        /*!***********************************************************************
        @brief Writes an integer to the current node (default key)
        @param i Integer value to write
        *************************************************************************/
        virtual void WriteInt(int i);

        /*!***********************************************************************
        @brief Writes a boolean to the current node (default key)
        @param b Boolean value to write
        *************************************************************************/
        virtual void WriteBool(bool b);

        /*!***********************************************************************
        @brief Writes a string to the current node (default key)
        @param s String value to write
        *************************************************************************/
        virtual void WriteString(const std::string& s);

        /*!***********************************************************************
        @brief Virtual destructor
        *************************************************************************/
        virtual ~ISerializer() = default;
    };
}

#endif /* ISERIALIZER_HPP */
