#include "config.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <numeric>
#include <wblib/json_utils.h>
#include <wblib/utils.h>

#include "file_utils.h"

using namespace Json;
using namespace WBMQTT::JSON;
using namespace std;

namespace
{
    const string ProtectedProperties[] = {"match_iio",
                                          "channel_number",
                                          "mqtt_type",
                                          "voltage_multiplier",
                                          "max_voltage"};

    void LoadChannel(const Value& item, vector<TADCChannelSettings>& channels)
    {
        TADCChannelSettings channel;

        Get(item, "id", channel.Id);
        Get(item, "averaging_window", channel.ReaderCfg.AveragingWindow);
        if (Get(item, "max_voltage", channel.ReaderCfg.MaxScaledVoltage))
            channel.ReaderCfg.MaxScaledVoltage *= 1000;
        Get(item, "voltage_multiplier", channel.ReaderCfg.VoltageMultiplier);
        Get(item, "decimal_places", channel.ReaderCfg.DecimalPlaces);
        Get(item, "scale", channel.ReaderCfg.DesiredScale);
        Get(item, "poll_interval", channel.ReaderCfg.PollInterval);
        Get(item, "delay_between_measurements", channel.ReaderCfg.DelayBetweenMeasurements);
        Get(item, "match_iio", channel.MatchIIO);

        Value v = item["channel_number"];
        if (v.isInt()) {
            channel.ReaderCfg.ChannelNumber = "voltage" + to_string(v.asInt());
        } else {
            channel.ReaderCfg.ChannelNumber = v.asString();
        }
        channels.push_back(channel);
    }

    /*! This function recalculates delay between measurements if it is too large for
        selected poll interval. If so, measurements will be evenly distributed in interval
    */
    void MaybeFixDelayBetweenMeasurements(TConfig& cfg, WBMQTT::TLogger* log)
    {
        for (auto& ch: cfg.Channels) {
            auto calcMeasureDelay = ch.ReaderCfg.DelayBetweenMeasurements * ch.ReaderCfg.AveragingWindow;
            if (calcMeasureDelay > ch.ReaderCfg.PollInterval) {
                if (log) {
                    log->Log() << ch.Id << ": averaging delay doesn't fit in poll_interval, "
                               << "adjusting delay_between_measurements";
                }

                ch.ReaderCfg.DelayBetweenMeasurements = ch.ReaderCfg.PollInterval / ch.ReaderCfg.AveragingWindow;
            }
        }
    }

    Value Load(const string& filePath, const Value& schema)
    {
        auto json = Parse(filePath);
        Validate(json, schema);
        return json;
    }

    TConfig LoadFromJSON(const Value& configJson)
    {
        TConfig config;

        Get(configJson, "device_name", config.DeviceName);
        Get(configJson, "debug", config.EnableDebugMessages);
        Get(configJson, "max_unchanged_interval", config.MaxUnchangedInterval);

        const auto& ch = configJson["iio_channels"];
        for_each(ch.begin(), ch.end(), [&](const Value& v) { LoadChannel(v, config.Channels); });

        return config;
    }

    Value RemoveDeviceNameRequirement(const Value& schema)
    {
        Value newArray = arrayValue;
        for (auto& v: schema["required"]) {
            if (v.asString() != "device_name") {
                newArray.append(v);
            }
        }
        Value res(schema);
        res["required"] = newArray;
        return res;
    }
} // namespace

TConfig LoadConfig(const string& mainConfigFile,
                   const string& optionalConfigFile,
                   const string& systemConfigDir,
                   const string& schemaFile,
                   WBMQTT::TLogger* infoLogger,
                   WBMQTT::TLogger* warnLogger)
{
    auto schema = Parse(schemaFile);

    if (!optionalConfigFile.empty()) {
        return LoadFromJSON(Load(optionalConfigFile, schema));
    }

    auto noDeviceNameSchema = RemoveDeviceNameRequirement(schema);

    TMergeParams mergeParams;
    mergeParams.LogPrefix = "[config] ";
    mergeParams.InfoLogger = infoLogger;
    mergeParams.WarnLogger = warnLogger;
    mergeParams.MergeArraysOn["/iio_channels"] = "id";

    Json::Value resultingConfig;
    resultingConfig["iio_channels"] = Json::Value(Json::arrayValue);

    try {
        IterateDirByPattern(systemConfigDir, ".conf", [&](const string& f) {
            Merge(resultingConfig, Load(f, noDeviceNameSchema), mergeParams);
            return false;
        });
    } catch (const TNoDirError&) {
    }

    for (const auto& pr: ProtectedProperties) {
        mergeParams.ProtectedParameters.insert("/iio_channels/" + pr);
    }
    Merge(resultingConfig, Load(mainConfigFile, schema), mergeParams);
    auto cfg = LoadFromJSON(resultingConfig);
    MaybeFixDelayBetweenMeasurements(cfg, infoLogger);
    return cfg;
}

void MakeJsonForConfed(const string& configFile, const string& systemConfigsDir, const string& schemaFile)
{
    auto schema = Parse(schemaFile);
    auto noDeviceNameSchema = RemoveDeviceNameRequirement(schema);
    auto config = Load(configFile, schema);
    unordered_map<string, Value> configuredChannels;
    for (const auto& ch: config["iio_channels"]) {
        configuredChannels.emplace(ch["id"].asString(), ch);
    }
    Value newChannels(arrayValue);
    try {
        IterateDirByPattern(systemConfigsDir, ".conf", [&](const string& f) {
            auto cfg = Load(f, noDeviceNameSchema);
            for (const auto& ch: cfg["iio_channels"]) {
                auto name = ch["id"].asString();
                auto it = configuredChannels.find(name);
                if (it != configuredChannels.end()) {
                    newChannels.append(it->second);
                    configuredChannels.erase(name);
                } else {
                    Value v;
                    v["id"] = ch["id"];
                    v["decimal_places"] = ch["decimal_places"];
                    newChannels.append(v);
                }
            }
            return false;
        });
    } catch (const TNoDirError&) {
    }
    for (const auto& ch: config["iio_channels"]) {
        auto it = configuredChannels.find(ch["id"].asString());
        if (it != configuredChannels.end()) {
            newChannels.append(ch);
        }
    }
    config["iio_channels"].swap(newChannels);
    MakeWriter("", "None")->write(config, &cout);
}

void MakeConfigFromConfed(const string& systemConfigsDir, const string& schemaFile)
{
    auto noDeviceNameSchema = RemoveDeviceNameRequirement(Parse(schemaFile));
    unordered_set<string> systemChannels;
    try {
        IterateDirByPattern(systemConfigsDir, ".conf", [&](const string& f) {
            auto cfg = Load(f, noDeviceNameSchema);
            for (const auto& ch: cfg["iio_channels"]) {
                systemChannels.insert(ch["id"].asString());
            }
            return false;
        });
    } catch (const TNoDirError&) {
    }

    Value config;
    CharReaderBuilder readerBuilder;
    String errs;

    if (!parseFromStream(readerBuilder, cin, &config, &errs)) {
        throw runtime_error("Failed to parse JSON:" + errs);
    }

    Value newChannels(arrayValue);
    for (auto& ch: config["iio_channels"]) {
        auto it = systemChannels.find(ch["id"].asString());
        if (it != systemChannels.end()) {
            for (const auto& pr: ProtectedProperties) {
                ch.removeMember(pr);
            }
            if (ch.size() > 1) {
                newChannels.append(ch);
            }
        } else {
            newChannels.append(ch);
        }
    }
    config["iio_channels"].swap(newChannels);
    MakeWriter("  ", "None")->write(config, &cout);
}

void MakeSchemaForConfed(const string& systemConfigsDir, const string& schemaFile, const string& schemaForConfedFile)
{
    auto schema = Parse(schemaFile);
    auto noDeviceNameSchema = RemoveDeviceNameRequirement(schema);
    unordered_set<string> systemChannels;
    try {
        IterateDirByPattern(systemConfigsDir, ".conf", [&](const string& f) {
            auto cfg = Load(f, noDeviceNameSchema);
            for (const auto& ch: cfg["iio_channels"]) {
                systemChannels.insert(ch["id"].asString());
            }
            return false;
        });
    } catch (const TNoDirError&) {
    }
    auto& notNode = schema["definitions"]["custom_channel"]["allOf"][1]["not"]["properties"]["id"]["enum"];
    auto& sysIdsNode = schema["definitions"]["system_channel"]["allOf"][1]["properties"]["id"]["enum"];
    for (const auto& id: systemChannels) {
        notNode.append(id);
        sysIdsNode.append(id);
    }
    ofstream f(schemaForConfedFile);
    MakeWriter("    ", "None")->write(schema, &f);
}