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

    TConfig loadFromJSON(const string& fileName, const Value& schema)
    {
        TConfig config;

        Value configJson(Parse(fileName));

        Validate(configJson, schema);

        Get(configJson, "device_name", config.DeviceName);
        Get(configJson, "debug", config.EnableDebugMessages);

        const auto& ch = configJson["iio_channels"];
        for_each(ch.begin(), ch.end(), [&](const Value& v) { LoadChannel(v, config.Channels); });

        return config;
    }

    void removeDeviceNameRequirement(Value& schema)
    {
        Value newArray = arrayValue;
        for (auto& v : schema["required"]) {
            if (v.asString() != "device_name") {
                newArray.append(v);
            }
        }
        schema["required"] = newArray;
    }
} // namespace

TConfig LoadConfig(const string& mainConfigFile,
                   const string& optionalConfigFile,
                   const string& systemConfigDir,
                   const string& schemaFile)
{
    Value schema             = Parse(schemaFile);
    Value noDeviceNameSchema = schema;
    removeDeviceNameRequirement(noDeviceNameSchema);

    if (!optionalConfigFile.empty())
        return loadFromJSON(optionalConfigFile, schema);
    TConfig cfg;
    try {
        IterateDir(systemConfigDir, ".conf", [&](const string& f) {
            Append(loadFromJSON(f, noDeviceNameSchema), cfg);
            return false;
        });
    } catch (const TNoDirError&) {
    }
    Append(loadFromJSON(mainConfigFile, schema), cfg);
    return cfg;
}

TBadConfigError::TBadConfigError(const string& msg) : runtime_error(msg) {}