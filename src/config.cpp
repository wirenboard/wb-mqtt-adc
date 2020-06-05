#include "config.h"
#include <fstream>
#include <algorithm>
#include <jsoncpp/json/json.h>
#include <wblib/utils.h>

namespace {

    bool get(const Json::Value&  item, const char* key, std::string& value) 
    {
        if (!item.isMember(key))
            return false;
        Json::Value v = item[key];
        if(!v.isString())
            throw std::runtime_error(std::string(key) + " is not a string value");
        value = v.asString();
        return true;
    }

    bool get(const Json::Value&  item, const char* key, float& value) 
    {
        if (!item.isMember(key))
            return false;
        Json::Value v = item[key];
        if(!v.isDouble())
            throw std::runtime_error(std::string(key) + " is not a float value");
        value = v.asFloat();
        return true;
    }

    bool get(const Json::Value&  item, const char* key, uint32_t& value) 
    {
        if (!item.isMember(key))
            return false;
        Json::Value v = item[key];
        if(!v.isUInt())
            throw std::runtime_error(std::string(key) + " is not an unsigned integer value");
        value = v.asUInt();
        return true;
    }

    bool get(const Json::Value&  item, const char* key, bool& value) 
    {
        if (!item.isMember(key))
            return false;
        Json::Value v = item[key];
        if(!v.isBool())
            throw std::runtime_error(std::string(key) + " is not a boolean value");
        value = v.asBool();
        return true;
    }

    void LoadChannel(const Json::Value& item, std::map<std::string, TADCChannel>& channels)
    {
        std::string id;
        if(!get(item, "id", id)){
            throw std::runtime_error("id field is missing in the config");
        }

        TADCChannel channel;
        get(item, "averaging_window", channel.AveragingWindow);
        if (channel.AveragingWindow == 0) {
            throw std::runtime_error("zero averaging window is specified in the config");
        }

        if (get(item, "max_voltage", channel.MaxVoltage)) {
            channel.MaxVoltage *= 1000;
        }

        get(item, "voltage_multiplier", channel.VoltageMultiplier);
        get(item, "readings_number", channel.ReadingsCount);
        get(item, "decimal_places", channel.DecimalPlaces);
        get(item, "scale", channel.Scale);

        if (!item.isMember("channel_number")) {
            throw std::runtime_error("channel_number field is missing in the config");
        }

        Json::Value v = item["channel_number"];
        channel.ChannelNumber = (v.isInt() ? ("voltage" + std::to_string(v.asInt())) : v.asString());

        channels[id] = channel;
    }

    void append(const TConfig& src, TConfig& dst) 
    {
        dst.DeviceName = src.DeviceName;
        dst.Debug = src.Debug;

        for(const auto& v: src.Channels) {
            dst.Channels[v.first] = v.second;
        }
    }

    TConfig loadFromJSON(const std::string& fileName) 
    {
        TConfig config;

        std::ifstream file(fileName);
        if (!file.is_open()) {
            throw std::runtime_error(std::string("Can't open file: ") + fileName);
        }

        Json::Value root;
        Json::Reader reader;

        // Report failures and their locations in the document.
        if(!reader.parse(file, root, false))
            throw std::runtime_error(std::string("Failed to parse config JSON: ") + reader.getFormattedErrorMessages());
        if (!root.isObject())
            throw std::runtime_error("Bad config file (the root is not an object)");

        if(!get(root, "device_name", config.DeviceName))
            throw std::runtime_error("Device name is not specified in config file");

        get(root, "debug", config.Debug);

        const auto& array = root["iio_channels"];

        std::for_each(array.begin(), array.end(), [&](const Json::Value& v) {LoadChannel(v, config.Channels);});
        return config;
    }
}

TConfig loadConfig(const std::string& mainConfigFile, const std::string& optionalConfigFile)
{
    if(!optionalConfigFile.empty())
        return loadFromJSON(optionalConfigFile);
    return loadFromJSON(mainConfigFile);
}
