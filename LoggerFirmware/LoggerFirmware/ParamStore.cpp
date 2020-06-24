/*! \file ParamStore.cpp
 *  \brief Implementation of a non-volatile memory parameter store for the logger.
 *
 * In order to provide abstracted storage of configuration parameters, this code generates a
 * factory for the particular hardware implementation, and a sub-class implementation for
 * known hardware.
 *
 * Copyright (c) 2020, University of New Hampshire, Center for Coastal and Ocean Mapping
 * & NOAA-UNH Joint Hydrographic Center.  All Rights Reserved.
 */

#include "ParamStore.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)

#include "FS.h"
#include "SPIFFS.h"

/// \class SPIFSParamStore
/// \brief Implement a key-value pair object in flash storage on the SPIFFS module in the ESP32
class SPIFSParamStore : public ParamStore {
public:
    /// Default constructor for the sub-class, which brings up the SPIFFS system, which causes the
    /// file-system part of the flash memory to be formatted if it hasn't been already.  This may take a
    /// little extra time when this is first called.  Since this is likely to be when the system is brought up
    /// in manufacturing, it shouldn't be too much of a problem.
    
    SPIFSParamStore(void)
    {
        if (!SPIFFS.begin(true)) {
            // "true" here forces a format of the FFS if it isn't already
            // formatted (which will cause it to initially fail).
            Serial.println("ERR: SPIFFS mount failed.");
        }
        size_t filesystem_size = SPIFFS.totalBytes();
        size_t used_size = SPIFFS.usedBytes();
        Serial.println(String("INFO: SPI FFS total ") + filesystem_size + "B, used " + used_size + "B");
    }
    
    /// Empty default destructor to allow for sub-classing if required.
    
    virtual ~SPIFSParamStore(void)
    {
    }
    
private:
    /// Set a key-value pair by constructing a file with the name of the key in the SPIFFS file-system
    /// space, and writing the value into it.
    ///
    /// \param key      Recognition name for the value to store.
    /// \param value    Data to write for the key.
    /// \return True if the file was created and written successfully, otherwise false.
    
    bool set_key(String const& key, String const& value)
    {
        fs::File f = SPIFFS.open(String("/") + key + ".par", FILE_WRITE);
        if (!f) {
            Serial.println("ERR: failed to write key to filesystem.");
            return false;
        }
        f.print(value);
        f.close();
        return true;
    }
    
    /// Get the value of a key-value pair by looking for the file corresponding to the key name in the
    /// SPIFFS file-system, and reading the contents.
    ///
    /// \param key  Recognition name for the value to retrieve
    /// \param value    Reference for where to store the value retrieved.
    /// \return True if the value was successfully retrieved, otherwise false.
    
    bool get_key(String const& key, String& value)
    {
        fs::File f = SPIFFS.open(String("/") + key + ".par", FILE_READ);
        if (!f) {
            Serial.print("ERR: failed to find key \"");
            Serial.print(key);
            Serial.println("\" in filesystem.");
            value = "";
            return false;
        }
        value = f.readString();
        f.close();
        return true;
    }
};

#endif

#if defined(__SAM3X8E__)

/// \class BLEParamStore
/// \brief Implement a key-value pair object in the Adafruit BLE module NVM
class BLEParamStore : public ParamStore {
public:
    /// Default constructor for storage of key-value parameters in the BLE module NVM.
    /// This doesn't have anything to do, since it is assumed that the BLE module will already
    /// have been instantiated before this call is made.
    
    BLEParamStore(void)
    {
    }
    
    /// Empty default destructor to allow for sub-classing if required.
    
    virtual ~BLEParamStore(void)
    {
    }
    
private:
    const int max_nvm_string_length = 28; ///< Maximum length of any string written to the NVM memory on the module

    /// Match a known-string key-name with the placement location in memory for the value
    /// associated.  This recognises a limited number of key names, but the string has to be
    /// exact: no partial matching is done.
    ///
    /// \param key  Name to convert to a number
    /// \return Placement location for the key, or -1 if the key name is not recognised.
    
    int match_key(String const& key)
    {
        int value = -1;
        if (key == "idstring")
            value = 0;
        else if (key == "adname")
            value = 1;
        else if (key == "ssid")
            value = 2;
        else if (key == "password")
            value = 3;
        else if (key == "ipaddress")
            value = 4;
        return value;
    }
    
    /// Set a key-value pair in the BLE module NVRAM.  This attempts to match the key name in the list of known parameter
    /// keys, and sets the value if possible.  Note that the \a max_nvm_string_length constant sets the maximum length for
    /// any value to be set; anything over this limit is simply ignored.
    ///
    /// \param key  Name of the key to set
    /// \param value    Value to set for the string name
    /// \return True if the key was successfully set, otherwise false.
    
    bool set_key(String const& key, String const& value)
    {
        int refnum = match_key(key);
        if (refnum < 0) {
            Serial.println("ERR: key not known.");
            return false;
        }
        String write_str;
        if (value.length() < max_nvm_string_length)
            write_str = value;
        else
            write_str = value.substring(0, max_nvm_string_length);
        int address = (sizeof(int) + max_nvm_string_length) * refnum;
        ble.writeNVM(address, write_str.length());
        ble.writeNVM(address + 4, (uint8_t const*)(write_str.c_str()), write_str.length());
        return true;
    }
    
    /// Retrieve the value associated with a given key from the BLE module NVRAM.  This attempts to match the key name
    /// in the list of known parameter keys, and retrieves the value if possible.  Note that the value string is limited to a fixed
    /// maximum length (set by the \a max_nvm_string_length) and therefore may not be what you set in some cases.
    ///
    /// \param key  Recognition name for the value to retrieve
    /// \param value    Reference for where to store the value retrieved.
    /// \return True if the value was retrieved correctly, otherwise false.
    bool get_key(String const& key, String& value)
    {
        int refnum = match_key(key);
        if (refnum < 0) {
            Serial.println("ERR: key not known.");
            return false;
        }
        int address = (sizeof(int) + max_nvm_string_length) * refnum;
        int32_t length;
        uint8_t buffer[max_nvm_string_length + 1];
        ble.readNVM(address, &length);
        ble.readNVM(address + 4, buffer, length);
        buffer[length] = '\0';
        
        value = String((const char*)buffer);
        return true;
    }
};

#endif

/// Base class call-through to the sub-class implementation to set the given key-value pair.
///
/// \param key  Name of the key to set
/// \param value    Value to set for the string name
/// \return True if the key was successfully set, otherwise false.

bool ParamStore::SetKey(String const& key, String const& value)
{
    return set_key(key, value);
}

/// Base class call-through to the sub-class implementation to get the given key-value pair.
///
/// \param key  Recognition name for the value to retrieve
/// \param value    Reference for where to store the value retrieved.
/// \return True if the value was retrieved correctly, otherwise false.

bool ParamStore::GetKey(String const& key, String& value)
{
    return get_key(key, value);
}

/// Factory method to generate the appropriate implementation of the ParamStore object for the
/// current hardware module.  The sub-class is up-cast to the base object on return.
///
/// \return Up-cast pointer to the ParamStore implementation for the hardware.

ParamStore *ParamStoreFactory::Create(void)
{
    ParamStore *obj;
    
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
    obj = new SPIFSParamStore();
#endif
#if defined(__SAM3X8E__)
    obj = new BLEParamStore();
#endif
    
    return obj;
}
