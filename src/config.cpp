#include "config.h"
#include <algorithm>
#include <fstream>
#include <wblib/utils.h>
#include <wblib/json_utils.h>

#include "file_utils.h"

using namespace Json;
using namespace WBMQTT::JSON;
using namespace std;

namespace
{
    void LoadChannel(const Value& item, vector<TADCChannelSettings>& channels)
    {
        TADCChannelSettings channel;
        Get(item, "id", channel.Id);
        Get(item, "averaging_window", channel.ReaderCfg.AveragingWindow);
        if (Get(item, "max_voltage", channel.ReaderCfg.MaxScaledVoltage))
            channel.ReaderCfg.MaxScaledVoltage *= 1000;
        Get(item, "voltage_multiplier", channel.ReaderCfg.VoltageMultiplier);
        Get(item, "readings_number", channel.ReaderCfg.ReadingsNumber);
        Get(item, "decimal_places", channel.ReaderCfg.DecimalPlaces);
        Get(item, "scale", channel.ReaderCfg.DesiredScale);
        Get(item, "match_iio", channel.MatchIIO);

        Value v = item["channel_number"];
        if (v.isInt()) {
            channel.ReaderCfg.ChannelNumber = "voltage" + to_string(v.asInt());
        } else {
            channel.ReaderCfg.ChannelNumber = v.asString();
        }
        channels.push_back(channel);
    }

    void Append(const TConfig& src, TConfig& dst)
    {
        dst.DeviceName          = src.DeviceName;
        dst.EnableDebugMessages = src.EnableDebugMessages;

        for (const auto& v : src.Channels) {
            auto el = find_if(dst.Channels.begin(), dst.Channels.end(), [&](auto& c) {
                return c.Id == v.Id;
            });
            if (el == dst.Channels.end()) {
                dst.Channels.push_back(v);
            } else {
                *el = v;
            }
        }
    }

    TConfig loadFromJSON(const string& fileName, const Value& shema)
    {
        TConfig config;

        Value configJson(Parse(fileName));

        Validate(configJson, shema);

        Get(configJson, "device_name", config.DeviceName);
        Get(configJson, "debug", config.EnableDebugMessages);

        const auto& ch = configJson["iio_channels"];
        for_each(ch.begin(), ch.end(), [&](const Value& v) { LoadChannel(v, config.Channels); });

        return config;
    }

    void removeDeviceNameRequirement(Value& shema)
    {
        Value newArray = arrayValue;
        for (auto& v : shema["required"]) {
            if (v.asString() != "device_name") {
                newArray.append(v);
            }
        }
        shema["required"] = newArray;
    }
} // namespace

TConfig LoadConfig(const string& mainConfigFile,
                   const string& optionalConfigFile,
                   const string& shemaFile)
{
    Value shema             = Parse(shemaFile);
    Value noDeviceNameShema = shema;
    removeDeviceNameRequirement(noDeviceNameShema);

    if (!optionalConfigFile.empty())
        return loadFromJSON(optionalConfigFile, shema);
    TConfig cfg;
    try {
        IterateDir(mainConfigFile + ".d", ".conf", [&](const string& f) {
            Append(loadFromJSON(f, noDeviceNameShema), cfg);
            return false;
        });
    } catch (const TNoDirError&) {
    }
    Append(loadFromJSON(mainConfigFile, shema), cfg);
    return cfg;
}

TBadConfigError::TBadConfigError(const string& msg) : runtime_error(msg) {}