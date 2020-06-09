#pragma once

#include <functional>
#include <vector>
#include <fstream>

bool TryOpen(const std::vector<std::string>& fnames, std::ifstream& file);


template<class T> void OpenWithException(T& f, const std::string& fileName)
{
    f.open(fileName);
    if (!f.is_open()) {
        throw std::runtime_error("Can't open file:" + fileName);
    }
}

void WriteToFile(const std::string& fileName, const std::string& value);

/**
 * @brief Exception class thrown on open directory failure.
 */
class TNoDirError: public std::runtime_error
{
    public:
        TNoDirError(const std::string& msg);
};

std::string IterateDir(const std::string& dirName, const std::string& pattern, std::function<bool (const std::string& )> fn);
