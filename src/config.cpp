#include "config.h"
#include <algorithm>
#include <fstream>
#include <wblib/utils.h>

#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>

#include "file_utils.h"

using namespace Json;
using namespace valijson;
using namespace std;

namespace
{
    bool Get(const Value& item, const string& key, string& value)
    {
        if (!item.isMember(key)) {
            return false;
        }
        Value v = item[key];
        if (!v.isString()) {
            throw TBadConfigError(key + " is not a string value");
        }
        value = v.asString();
        return true;
    }

    bool Get(const Value& item, const string& key, double& value)
    {
        if (!item.isMember(key)) {
            return false;
        }
        Value v = item[key];
        if (!v.isDouble()) {
            throw TBadConfigError(key + " is not a float value");
        }
        value = v.asDouble();
        return true;
    }

    bool Get(const Value& item, const string& key, uint32_t& value)
    {
        if (!item.isMember(key)) {
            return false;
        }
        Value v = item[key];
        if (!v.isUInt()) {
            throw TBadConfigError(key + " is not an unsigned integer value");
        }
        value = v.asUInt();
        return true;
    }

    bool Get(const Value& item, const string& key, bool& value)
    {
        if (!item.isMember(key)) {
            return false;
        }
        Value v = item[key];
        if (!v.isBool()) {
            throw TBadConfigError(key + " is not a boolean value");
        }
        value = v.asBool();
        return true;
    }

    void LoadChannel(const Value& item, vector<TADCChannelSettings>& channels)
    {
        TADCChannelSettings channel;
        Get(item, "id", channel.Id);
        Get(item, "averaging_window", channel.ReaderCfg.AveragingWindow);
        Get(item, "max_voltage", channel.ReaderCfg.MaxScaledVoltage);
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

    void ValidateJson(const Value& root, const Value& schema_js)
    {
        adapters::JsonCppAdapter doc(root);
        adapters::JsonCppAdapter schema_doc(schema_js);

        SchemaParser parser(SchemaParser::kDraft4);
        Schema       schema;
        parser.populateSchema(schema_doc, schema);
        Validator         validator(Validator::kStrongTypes);
        ValidationResults results;
        if (!validator.validate(schema, doc, &results)) {
            stringstream err_oss;
            err_oss << "Validation failed." << endl;
            ValidationResults::Error error;
            int                      error_num = 1;
            while (results.popError(error)) {
                err_oss << "Error " << error_num << endl << "  context: ";
                for (const auto& er : error.context) {
                    err_oss << er;
                }
                err_oss << endl << "  desc: " << error.description << endl;
                ++error_num;
            }
            throw TBadConfigError(err_oss.str());
        }
    }

    Value ParseJson(const string& fileName)
    {
        ifstream file;
        OpenWithException(file, fileName);

        Value  root;
        Reader reader;

        // Report failures and their locations in the document.
        if (!reader.parse(file, root, false)) {
            throw TBadConfigError("Failed to parse JSON " + fileName + ":" +
                                  reader.getFormattedErrorMessages());
        }
        if (!root.isObject()) {
            throw TBadConfigError("Bad JSON " + fileName + ": the root is not an object");
        }
        return root;
    }

    TConfig loadFromJSON(const string& fileName, const Value& shema)
    {
        TConfig config;

        Value configJson(ParseJson(fileName));

        ValidateJson(configJson, shema);

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
    Value shema             = ParseJson(shemaFile);
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