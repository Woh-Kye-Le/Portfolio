/* !
@file    	    ISerializer.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2025
@brief
    Provides default implementations of sequential read/write methods that
    delegate to key-based versions using an empty string as the default key,
    allowing primitive types to be read from or written to the current node
    without specifying a key explicitly.

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

// Engine Serialization Headers
#include "engine/serializer/ISerializer.hpp"

namespace Engine::Serializer
{
    // ----- Key-based reading -----

    /*!***********************************************************************
    @brief Reads a float value from the current node using default key
    @param f Reference to store the float value
    *************************************************************************/
    void ISerializer::ReadFloat(float& f)
    {
        ReadFloat("", f);
    }

    /*!***********************************************************************
    @brief Reads an integer value from the current node using default key
    @param i Reference to store the integer value
    *************************************************************************/
    void ISerializer::ReadInt(int& i)
    {
        ReadInt("", i);
    }

    /*!***********************************************************************
    @brief Reads a boolean value from the current node using default key
    @param b Reference to store the boolean value
    *************************************************************************/
    void ISerializer::ReadBool(bool& b)
    {
        ReadBool("", b);
    }

    /*!***********************************************************************
    @brief Reads a string value from the current node using default key
    @param s Reference to store the string value
    *************************************************************************/
    void ISerializer::ReadString(std::string& s)
    {
        ReadString("", s);
    }

    // ----- Key-based writing -----

    /*!***********************************************************************
    @brief Writes a float value to the current node using default key
    @param f Float value to write
    *************************************************************************/
    void ISerializer::WriteFloat(float f)
    {
        WriteFloat("", f);
    }

    /*!***********************************************************************
    @brief Writes an integer value to the current node using default key
    @param i Integer value to write
    *************************************************************************/
    void ISerializer::WriteInt(int i)
    {
        WriteInt("", i);
    }

    /*!***********************************************************************
    @brief Writes a boolean value to the current node using default key
    @param b Boolean value to write
    *************************************************************************/
    void ISerializer::WriteBool(bool b)
    {
        WriteBool("", b);
    }

    /*!***********************************************************************
    @brief Writes a string value to the current node using default key
    @param s String value to write
    *************************************************************************/
    void ISerializer::WriteString(const std::string& s)
    {
        WriteString("", s);
    }
}
