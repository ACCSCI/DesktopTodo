#include "todo/windows.hpp"
#include <fstream>

namespace todo::win {
std::uintptr_t data_identity(const std::filesystem::path& path) {
    std::uint64_t hash=1469598103934665603ULL;
    for(wchar_t c:path.wstring()){hash^=std::uint64_t(towlower(c));hash*=1099511628211ULL;}
    return std::uintptr_t(hash|1);
}
std::filesystem::path read_storage_directory(const std::filesystem::path& base,const std::filesystem::path& settings) {
    if(!std::filesystem::exists(settings))return base;
    std::ifstream file(settings,std::ios::binary);if(!file)throw std::runtime_error("Cannot read storage settings");
    std::string line;
    while(std::getline(file,line))if(line.starts_with("directory=")) {
        line.erase(0,10);if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()||line.size()>32768)throw std::runtime_error("Invalid storage directory");
        std::filesystem::path path(wide(line));
        return std::filesystem::weakly_canonical(path.is_absolute()?path:base/path);
    }
    throw std::runtime_error("Missing storage directory in settings");
}
void write_storage_directory(const std::filesystem::path& base,const std::filesystem::path& settings,const std::filesystem::path& directory) {
    auto absolute=std::filesystem::weakly_canonical(directory);
    auto relative=absolute.lexically_relative(std::filesystem::weakly_canonical(base));
    auto value=!relative.empty()&&*relative.begin()!=L".."?relative:absolute;
    auto text=path_utf8(value);if(text.find_first_of("\r\n")!=std::string::npos)throw std::invalid_argument("Invalid storage directory");
    auto temporary=settings;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId());
    {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file<<"[Storage]\ndirectory="<<text<<'\n';file.flush();
        if(!file)throw std::runtime_error("Cannot save storage settings");}
    if(!MoveFileExW(temporary.c_str(),settings.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        auto error=GetLastError();std::error_code ignored;std::filesystem::remove(temporary,ignored);
        throw std::runtime_error(last_error("Save storage settings",error));
    }
}
}
