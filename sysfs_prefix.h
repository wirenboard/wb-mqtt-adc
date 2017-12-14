inline std::string GetSysfsPrefix()
{
    const char* prefix = getenv("WB_SYSFS_PREFIX");
    return prefix ? prefix : "/sys";
}