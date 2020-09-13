#include "file_utils.h"

#include <cstring>
#include <dirent.h>
#include <iomanip>

TNoDirError::TNoDirError(const std::string& msg) : std::runtime_error(msg) {}

bool TryOpen(const std::vector<std::string>& fnames, std::ifstream& file)
{
    for (auto& fname : fnames) {
        file.open(fname);
        if (file.is_open()) {
            return true;
        }
        file.clear();
    }
    return false;
}

void WriteToFile(const std::string& fileName, const std::string& value)
{
    std::ofstream f;
    OpenWithException(f, fileName);
    f << value;
}

std::string IterateDir(const std::string&                      dirName,
                       const std::string&                      pattern,
                       std::function<bool(const std::string&)> fn)
{
    DIR* dir = opendir(dirName.c_str());

    if (dir == NULL) {
        throw TNoDirError("Can't open directory: " + dirName);
    }

    dirent*                                 ent;
    auto                                    closeFn = [](DIR* d) { closedir(d); };
    std::unique_ptr<DIR, decltype(closeFn)> dirPtr(dir, closeFn);
    while ((ent = readdir(dirPtr.get())) != NULL) {
        if (!std::strstr(ent->d_name, pattern.c_str())) {
            continue;
        }
        std::string d(dirName + "/" + std::string(ent->d_name));
        if (fn(d)) {
            return d;
        }
    }
    return std::string();
}
