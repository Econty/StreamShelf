#include "component_manager.hpp"
#include <iomanip>
namespace ss {
std::vector<int> componentVersion(const std::string &s) {
    std::smatch m;
    if (!std::regex_search(s, m, std::regex(R"((\d{1,4})\.(\d{1,3})(?:\.(\d{1,3}))?)"))) return {};
    return {std::stoi(m[1]), std::stoi(m[2]), m[3].matched ? std::stoi(m[3]) : 0};
}
bool componentNewer(const std::string &latest, const std::string &installed) {
    auto a=componentVersion(latest), b=componentVersion(installed);
    return !a.empty() && (b.empty() || a>b);
}
bool componentFilePresent(const fs::path &p) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    return GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &data) &&
        !(data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
}
std::wstring terminalComponentCommand(const std::wstring &command, const fs::path &root) {
    const auto start = command.find_first_not_of(L" \t");
    if (start == std::wstring::npos) return command;
    size_t end = start; std::wstring name;
    if (command[start] == L'"') {
        end = command.find(L'"', start + 1); if (end == std::wstring::npos) return command;
        name = command.substr(start + 1, end - start - 1); ++end;
    } else {
        end = command.find_first_of(L" \t&|<>()", start); if (end == std::wstring::npos) end = command.size();
        name = command.substr(start, end - start);
    }
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });
    if (name == L"yt-dlp" || name == L"ffmpeg" || name == L"ffprobe" || name == L"deno") name += L".exe";
    if (name != L"yt-dlp.exe" && name != L"ffmpeg.exe" && name != L"ffprobe.exe" && name != L"deno.exe") return command;
    auto path = root / name;
    if (!componentFilePresent(path)) throw std::runtime_error("Компонент отсутствует в папке " + utf8(root.wstring()) + ": " + utf8(name));
    return command.substr(0,start) + quoteWide(path.wstring()) + command.substr(end);
}
static std::string readVersion(const fs::path &p, std::atomic_bool &stop) {
    if (!componentFilePresent(p)) return {};
    std::atomic_bool cancel{false}, done{false};
    std::thread watchdog([&] { for(int i=0;i<100 && !done && !stop;i++) Sleep(100); if(!done) cancel=true; });
    try {
        auto name = p.filename().wstring();
        bool ff = name.find(L"ffmpeg") != std::wstring::npos || name.find(L"ffprobe") != std::wstring::npos;
        auto r=runProcess(p,{ff?L"-version":L"--version"},nullptr,cancel,true);
        done=true; watchdog.join();
        if (r.code || cancel || stop) return {};
        auto v=componentVersion(r.output);
        if (v.empty()) return {};
        return std::to_string(v[0])+"."+std::to_string(v[1])+"."+std::to_string(v[2]);
    } catch (...) { done=true; watchdog.join(); return {}; }
}
static const wchar_t *componentFiles[] = {L"yt-dlp.exe", L"ffmpeg.exe", L"ffprobe.exe", L"deno.exe"};
std::vector<uint64_t> componentFingerprint(const fs::path &root) {
    std::vector<uint64_t> result;
    for (auto name : componentFiles) {
        WIN32_FILE_ATTRIBUTE_DATA data{};
        if (!GetFileAttributesExW((root / name).c_str(), GetFileExInfoStandard, &data) || (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
            result.insert(result.end(), {0,0,0}); continue;
        }
        result.push_back(1);
        result.push_back((uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow);
        result.push_back((uint64_t(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime);
    }
    return result;
}
std::vector<Component> refreshInstalledComponents(const fs::path &root, std::vector<Component> v, std::atomic_bool &stop) {
    if (v.size() != 4) v = {{"yt-dlp"},{"FFmpeg"},{"FFprobe"},{"Deno"}};
    for (size_t i=0;i<v.size();i++) {
        auto &c=v[i]; c.installed=readVersion(root/componentFiles[i],stop);
        c.update=!c.latest.empty() && (c.installed.empty() || componentNewer(c.latest,c.installed));
        if(c.latest.empty()) c.status=c.installed.empty()?"Не установлен · выпуск не проверен":"Установлен · выпуск не проверен";
        else c.status=c.installed.empty()?"Не установлен":c.update?"Доступно обновление":"Актуальная версия";
    }
    if (!v[1].latest.empty() && ((!v[1].installed.empty() && componentNewer(v[1].installed,v[1].latest)) ||
        (!v[2].installed.empty() && componentNewer(v[2].installed,v[2].latest)))) {
        v[1].update=v[2].update=false;
        v[1].status=v[2].status="Установленная версия новее доступной; обновление пары отменено";
    }
    return v;
}
static std::pair<std::string,std::string> releaseAsset(const Json &j,const std::string &name,const std::string &sum) {
    std::string u,h;
    if (!j.contains("assets") || !j["assets"].is_array()) throw std::runtime_error("Нет списка файлов выпуска");
    for (const auto &a:j["assets"]) {
        if (str(a,"name")==name) u=str(a,"browser_download_url");
        if (str(a,"name")==sum) h=str(a,"browser_download_url");
    }
    if (u.empty() || h.empty()) throw std::runtime_error("Файлы выпуска не найдены");
    return {u,h};
}
static std::string latestTagFromPage(const std::string &owner, const std::string &repo) {
    const auto page = httpGet("https://github.com/" + owner + "/" + repo + "/releases/latest", 8 * 1024 * 1024);
    const auto prefix = "/" + owner + "/" + repo + "/releases/tag/";
    std::smatch match;
    const std::regex pattern("<meta\\s+property=\"og:url\"\\s+content=\"" + prefix + "(v?[0-9]+\\.[0-9]+\\.[0-9]+)\"");
    if (!std::regex_search(page, match, pattern)) throw std::runtime_error("Не удалось определить версию выпуска");
    return match.str(1);
}
std::vector<Component> scanComponents(const fs::path &root, std::atomic_bool &stop, Log trace) {
    std::vector<Component> v={{"yt-dlp"},{"FFmpeg"},{"FFprobe"},{"Deno"}};
    if (trace) {
        trace("Проверка компонентов только в папке: " + utf8(root.wstring()));
        for (size_t i=0;i<v.size();i++) {
            auto path=root/componentFiles[i];
            if (componentFilePresent(path)) trace("> \"" + utf8(path.wstring()) + "\" " + (i==1 || i==2 ? "-version" : "--version"));
            else trace("Файл отсутствует: " + utf8(path.wstring()));
        }
    }
    v=refreshInstalledComponents(root,std::move(v),stop);
    if (trace) for (const auto &c:v) trace(c.name + ": " + (c.installed.empty() ? "не установлен или не запускается" : c.installed));
    auto assign=[&](size_t i,const std::string &latest,const std::string &url,const std::string &sum) {
        if (componentVersion(latest).empty()) throw std::runtime_error("Неизвестный формат версии");
        v[i].latest=latest; v[i].url=url; v[i].checksumUrl=sum;
        v[i].update=v[i].installed.empty() || componentNewer(latest,v[i].installed);
        v[i].status=v[i].installed.empty()?"Не установлен":v[i].update?"Доступно обновление":"Актуальная версия";
    };
    try {
        if (trace) trace("GET https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest");
        auto j=Json::parse(httpGet("https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest",4*1024*1024));
        auto a=releaseAsset(j,"yt-dlp.exe","SHA2-256SUMS");
        if (a.first.rfind("https://github.com/yt-dlp/yt-dlp/releases/",0)!=0 || a.second.rfind("https://github.com/yt-dlp/yt-dlp/releases/",0)!=0) throw std::runtime_error("Неверный источник yt-dlp");
        assign(0,str(j,"tag_name"),a.first,a.second);
    } catch(const std::exception &) {
        try { if (trace) trace("API недоступен; проверяю страницу выпуска yt-dlp на GitHub"); const auto tag=latestTagFromPage("yt-dlp","yt-dlp"); const auto base="https://github.com/yt-dlp/yt-dlp/releases/download/"+tag+"/"; assign(0,tag,base+"yt-dlp.exe",base+"SHA2-256SUMS"); }
        catch(const std::exception &) { v[0].status="Не удалось проверить выпуск"; }
    }
    try {
        if (trace) trace("GET https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip.ver");
        auto version=httpGet("https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip.ver",1024);
        auto ver=componentVersion(version); if(ver.empty()) throw std::runtime_error("Версия FFmpeg недоступна");
        auto text=std::to_string(ver[0])+"."+std::to_string(ver[1])+"."+std::to_string(ver[2]);
        for(size_t i:{size_t(1),size_t(2)}) assign(i,text,"https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip","https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip.sha256");
        if ((!v[1].installed.empty() && componentNewer(v[1].installed, text)) || (!v[2].installed.empty() && componentNewer(v[2].installed, text))) {
            v[1].update = v[2].update = false;
            v[1].status = v[2].status = "Установленная версия новее доступной; обновление пары отменено";
        }
    } catch(const std::exception &) { v[1].status=v[2].status="Не удалось проверить выпуск"; }
    try {
        if (trace) trace("GET https://api.github.com/repos/denoland/deno/releases/latest");
        auto j=Json::parse(httpGet("https://api.github.com/repos/denoland/deno/releases/latest",4*1024*1024));
        auto a=releaseAsset(j,"deno-x86_64-pc-windows-msvc.zip","deno-x86_64-pc-windows-msvc.zip.sha256sum");
        if(a.first.rfind("https://github.com/denoland/deno/releases/",0)!=0 || a.second.rfind("https://github.com/denoland/deno/releases/",0)!=0) throw std::runtime_error("Неверный источник Deno");
        assign(3,str(j,"tag_name"),a.first,a.second);
    } catch(const std::exception &) {
        try { if (trace) trace("API недоступен; проверяю страницу выпуска Deno на GitHub"); const auto tag=latestTagFromPage("denoland","deno"); const auto base="https://github.com/denoland/deno/releases/download/"+tag+"/deno-x86_64-pc-windows-msvc.zip"; assign(3,tag,base,base+".sha256sum"); }
        catch(const std::exception &) { v[3].status="Не удалось проверить выпуск"; }
    }
    v = refreshInstalledComponents(root,std::move(v),stop);
    if (trace) for (const auto &c:v) trace(c.name + ": установлена " + (c.installed.empty()?"нет":c.installed) + ", доступна " + (c.latest.empty()?"неизвестно":c.latest) + "; " + c.status);
    return v;
}
static std::string expectedHash(const std::string &text,const std::string &file) {
    std::istringstream in(text); std::string line; std::regex hex(R"(([0-9a-fA-F]{64}))"); std::smatch m;
    while(std::getline(in,line)) {
        if (!file.empty() && line.find(file)==std::string::npos) continue;
        if(std::regex_search(line,m,hex)) { auto s=m.str(1); std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));}); return s; }
    }
    throw std::runtime_error("Контрольная сумма не найдена");
}
static fs::path systemTar() {
    wchar_t path[MAX_PATH]{}; if(!GetSystemDirectoryW(path,MAX_PATH)) throw std::runtime_error("Нет системного каталога");
    auto p=fs::path(path)/L"tar.exe"; if(!fs::is_regular_file(p)) throw std::runtime_error("Для распаковки требуется системный tar.exe"); return p;
}
static void replaceTools(const fs::path &root,const fs::path &stage,const std::vector<std::wstring> &names) {
    // Prepare every backup first. If a replacement fails, restore earlier files.
    std::vector<bool> existed; for(const auto &n:names) { bool e=fs::exists(root/n); existed.push_back(e); if(e) fs::copy_file(root/n,stage/(n+L".backup"),fs::copy_options::overwrite_existing); }
    size_t changed=0;
    try { for(const auto &n:names) { if(!MoveFileExW((stage/n).c_str(),(root/n).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Не удалось заменить "+utf8(n)); changed++; } }
    catch (...) { for(size_t i=0;i<changed;i++) { if(existed[i]) { if(!MoveFileExW((stage/(names[i]+L".backup")).c_str(),(root/names[i]).c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Не удалось восстановить прежний компонент: "+utf8(names[i])); } else { std::error_code ec; fs::remove(root/names[i],ec); } } throw; }
}
static void installGroup(const fs::path &root,const Component &c,const std::vector<std::wstring> &names,std::atomic_bool &stop,Log log, ComponentProgressCallback progress) {
    auto stage=root/(L"component-stage-"+std::to_wstring(GetCurrentProcessId())+L"-"+wide(c.name));
    if(fs::exists(stage)) throw std::runtime_error("Временная папка уже существует: "+utf8(stage.wstring()));
    fs::create_directories(stage);
    struct Cleanup { fs::path p; bool keep=false; ~Cleanup() { if (!keep) { std::error_code ec; fs::remove_all(p,ec); } } } cleanup{stage};
    if (progress) progress({c.name, ComponentPhase::Downloading, 0, 0});
    if (log) log("Получаю контрольную сумму: " + c.checksumUrl);
    std::string sum=expectedHash(httpGet(c.checksumUrl,1024*1024),c.name=="yt-dlp"?"yt-dlp.exe":"");
    auto archive=stage/(c.name=="yt-dlp"?L"yt-dlp.exe":L"download.zip");
    if (log) log("Загрузка " + c.name + " " + c.latest + "…");
    httpDownload(c.url, archive, stop, [](double) {}, Json::object(), "",
        [&](uint64_t received, uint64_t total) { if (progress) progress({c.name, ComponentPhase::Downloading, received, total}); });
    if(stop) throw std::runtime_error("Загрузка отменена");
    if (progress) progress({c.name, ComponentPhase::Verifying, 0, 0});
    if(sha256(archive)!=sum) throw std::runtime_error("Контрольная сумма не совпала: "+c.name);
    if (log) log("SHA-256 проверен: " + c.name);
    if(c.name!="yt-dlp") {
        if (progress) progress({c.name, ComponentPhase::Extracting, 0, 0});
        auto tar=systemTar(); auto listing=runProcess(tar,{L"-tf",archive.wstring()},nullptr,stop,true);
        if(listing.code) throw std::runtime_error("Не удалось прочитать архив "+c.name);
        std::istringstream input(listing.output); std::string line; std::map<std::wstring,std::wstring> selected;
        while(std::getline(input,line)) { if(!line.empty()&&line.back()=='\r')line.pop_back();
            for(const auto &name:names) {
                std::string escaped=utf8(name); std::regex safe(c.name=="Deno"?"^deno\\.exe$":"^ffmpeg-[0-9][0-9.]*-essentials_build/bin/"+(escaped=="ffmpeg.exe"?std::string("ffmpeg\\.exe"):std::string("ffprobe\\.exe"))+"$");
                if(std::regex_match(line,safe)) { if(selected.count(name)) throw std::runtime_error("Повторяющийся файл в архиве"); selected[name]=wide(line); }
            }
        }
        if(selected.size()!=names.size()) throw std::runtime_error("В архиве отсутствуют ожидаемые файлы");
        for(const auto &n:names) {
            auto extract=runProcess(tar,{L"-xf",archive.wstring(),L"-C",stage.wstring(),selected[n]},nullptr,stop,true);
            if(extract.code) throw std::runtime_error("Не удалось распаковать "+utf8(n));
            auto extracted=stage/fs::path(selected[n]);
            if(!fs::is_regular_file(extracted)) throw std::runtime_error("Распакованный файл отсутствует");
            if(extracted!=stage/n) fs::rename(extracted,stage/n);
        }
    }
    for(const auto &n:names) { auto version=readVersion(stage/n,stop); if(version.empty() || componentVersion(version)!=componentVersion(c.latest)) throw std::runtime_error("Скачанный компонент не запускается или его версия не совпадает: "+utf8(n)); }
    if (progress) progress({c.name, ComponentPhase::Installing, 0, 0});
    try { replaceTools(root,stage,names); }
    catch (const std::exception &e) {
        if (std::string(e.what()).find("Не удалось восстановить") != std::string::npos) {
            cleanup.keep = true;
            throw std::runtime_error(std::string(e.what()) + ". Резервные файлы сохранены в " + utf8(stage.wstring()));
        }
        throw;
    }
    if(c.name=="yt-dlp") writeFile(root/L"version.txt",c.latest);
    if(log) log("Установлен "+c.name+" "+c.latest+" в "+utf8(root.wstring()));
}
void installComponents(const fs::path &root,const std::vector<Component> &items,std::atomic_bool &stop,Log log, ComponentProgressCallback progress) {
    if(items.size()!=4) throw std::runtime_error("Сначала проверьте компоненты");
    fs::create_directories(root);
    std::string errors;
    auto attempt=[&](const Component &item,const std::vector<std::wstring> &names) {
        if (stop) { errors += "Загрузка отменена. "; return; }
        try { installGroup(root,item,names,stop,log,progress); }
        catch (const std::exception &e) { errors += item.name + ": " + e.what() + "\n"; }
    };
    if(items[0].update) attempt(items[0],{L"yt-dlp.exe"});
    if(items[1].update || items[2].update) attempt(items[1],{L"ffmpeg.exe",L"ffprobe.exe"});
    if(items[3].update) attempt(items[3],{L"deno.exe"});
    if (!errors.empty()) throw std::runtime_error(errors);
}
}
