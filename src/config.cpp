#include "config.h"
#include <fstream>
#include <algorithm>
#include <jsoncpp/json/json.h>
#include <wblib/utils.h>

#include "file_utils.h"

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

    bool get(const Json::Value&  item, const char* key, double& value) 
    {
        if (!item.isMember(key))
            return false;
        Json::Value v = item[key];
        if(!v.isDouble())
            throw std::runtime_error(std::string(key) + " is not a float value");
        value = v.asDouble();
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

    void LoadChannel(const Json::Value& item, std::vector<TADCChannelSettings>& channels)
    {
        TADCChannelSettings channel;
        if(!get(item, "id", channel.Id)){
            throw std::runtime_error("id field is missing in the config");
        }

        get(item, "averaging_window", channel.ReaderCfg.AveragingWindow);
        if (channel.ReaderCfg.AveragingWindow == 0) {
            throw std::runtime_error("zero averaging window is specified in the config");
        }

        get(item, "max_voltage", channel.ReaderCfg.MaxScaledVoltage);
        get(item, "voltage_multiplier", channel.ReaderCfg.VoltageMultiplier);
        get(item, "readings_number", channel.ReaderCfg.ReadingsNumber);
        get(item, "decimal_places", channel.ReaderCfg.DecimalPlaces);
        get(item, "scale", channel.ReaderCfg.Scale);

        if (!item.isMember("channel_number")) {
            throw std::runtime_error("channel_number field is missing in the config");
        }

        Json::Value v = item["channel_number"];
        channel.ReaderCfg.ChannelNumber = (v.isInt() ? ("voltage" + std::to_string(v.asInt())) : v.asString());

        channels.push_back(channel);
    }

    void append(const TConfig& src, TConfig& dst) 
    {
        dst.DeviceName = src.DeviceName;
        dst.Debug = src.Debug;

        for(const auto& v: src.Channels) {
            auto el = std::find_if(dst.Channels.begin(), dst.Channels.end(), [&](auto& c){return c.Id == v.Id;});
            if(el == dst.Channels.end()){
                dst.Channels.push_back(v);
            } else {
                *el = v;
            }
        }
    }

    TConfig loadFromJSON(const std::string& fileName) 
    {
        TConfig config;

        std::ifstream file;
        OpenWithException(file, fileName);

        Json::Value root;
        Json::Reader reader;

        // Report failures and their locations in the document.
        if(!reader.parse(file, root, false))
            throw std::runtime_error(std::string("Failed to parse config ") + fileName + ":" + reader.getFormattedErrorMessages());
        if (!root.isObject())
            throw std::runtime_error("Bad config " + fileName +": the root is not an object");

        if(!get(root, "device_name", config.DeviceName))
            throw std::runtime_error("Device name is not specified in config " + fileName);

        get(root, "debug", config.Debug);

        const auto& array = root["iio_channels"];

        std::for_each(array.begin(), array.end(), [&](const Json::Value& v) {LoadChannel(v, config.Channels);});
        return config;
    }
}

TConfig LoadConfig(const std::string& mainConfigFile, const std::string& optionalConfigFile)
{
    if(!optionalConfigFile.empty())
        return loadFromJSON(optionalConfigFile);
    TConfig cfg;
    try {   
        IterateDir(mainConfigFile+".d", ".conf", [&](const std::string& f){ append(loadFromJSON(f), cfg); return false; });
    }
    catch(const TNoDirError&) {}
    append(loadFromJSON(mainConfigFile), cfg);
    return cfg;
}
