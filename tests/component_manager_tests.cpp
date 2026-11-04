#include "component_manager.hpp"
#include <iostream>
using namespace ss;
int main(int argc,char **argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") { std::cout << "2026.10.26\n"; return 0; }
    if (argc == 5 && std::string(argv[1]) == "--progress") {
        try {
            std::atomic_bool stop{false};
            fs::path root=wide(argv[2]); std::string base=argv[3];
            const bool known=std::string(argv[4]) == "known";
            std::vector<Component> items={{"yt-dlp"},{"FFmpeg"},{"FFprobe"},{"Deno"}};
            items[0].latest="2026.10.26"; items[0].url=base+"/payload";
            items[0].checksumUrl=base+"/checksum"; items[0].update=true;
            bool bytes=false, verified=false, installed=false;
            installComponents(root,items,stop,{},[&](const ComponentProgress &p) {
                if (p.name!="yt-dlp") throw std::runtime_error("wrong component");
                if (p.phase==ComponentPhase::Downloading && p.received) {
                    bytes=true;
                    if ((p.total!=0)!=known) throw std::runtime_error("incorrect content length");
                    if (known && (componentProgressPercent(p)<0 || componentProgressPercent(p)>100)) throw std::runtime_error("invalid percentage");
                    if (!known && componentProgressPercent(p)!=-1) throw std::runtime_error("unknown size displayed as percentage");
                }
                verified |= p.phase==ComponentPhase::Verifying;
                installed |= p.phase==ComponentPhase::Installing;
            });
            if (!bytes || !verified || !installed || !fs::exists(root/L"yt-dlp.exe")) throw std::runtime_error("missing progress phase or output");
            std::cout<<"Progress integration passed: "<<argv[4]<<"\n"; return 0;
        } catch(const std::exception &e) { std::cerr<<e.what()<<"\n"; return 6; }
    }
    if (argc > 2 && std::string(argv[1]) == "--live") {
        std::atomic_bool stop{false}; fs::path root=wide(argv[2]);
        try {
            std::vector<std::string> trace;
            auto v=scanComponents(root,stop,[&](const std::string &line) { trace.push_back(line); std::cout << "LOG " << line << "\n"; });
            bool hasRoot=false, hasHttp=false;
            for (const auto &line:trace) { if (line.find(utf8(root.wstring())) != std::string::npos) hasRoot=true; if (line.find("GET https://") != std::string::npos) hasHttp=true; }
            if (!hasRoot || !hasHttp) throw std::runtime_error("component scan did not record its local path and network checks");
            for (const auto &c:v) std::cout<<c.name<<" installed="<<c.installed<<" latest="<<c.latest<<" update="<<c.update<<" status="<<c.status<<"\n";
            if (argc>3 && std::string(argv[3])=="--failure") {
                auto before=sha256(root/L"yt-dlp.exe"); v[0].update=true; v[0].checksumUrl="not-a-valid-url";
                bool failed=false;
                try { installComponents(root,v,stop,nullptr); } catch (const std::exception &) { failed=true; }
                if (!failed || sha256(root/L"yt-dlp.exe")!=before) return 5;
                std::cout<<"PASS failed update leaves the installed binary intact\n";
            }
            if (argc>3 && std::string(argv[3])=="--install") {
                installComponents(root,v,stop,[](const std::string &s){std::cout<<s<<std::endl;});
                auto after=scanComponents(root,stop);
                for(const auto &c:after) { std::cout<<"AFTER "<<c.name<<" installed="<<c.installed<<" latest="<<c.latest<<" update="<<c.update<<"\n"; if(c.installed.empty() || c.update) return 4; }
            }
            return 0;
        } catch (const std::exception &e) { std::cerr<<e.what()<<std::endl; return 3; }
    }
    if (componentProgressPercent({"test",ComponentPhase::Downloading,50,100}) != 50 ||
        componentProgressPercent({"test",ComponentPhase::Downloading,50,0}) != -1 ||
        componentProgressPercent({"test",ComponentPhase::Downloading,200,100}) != 100 ||
        componentProgressPercent({"test",ComponentPhase::Verifying,50,100}) != -1) return 7;
    if(componentVersion("deno 2.9.8 (release)")!=std::vector<int>({2,9,8}) ||
       componentVersion("ffmpeg version 9.0.2-essentials")!=std::vector<int>({9,0,2}) ||
       componentVersion("2026.10.26")!=std::vector<int>({2026,10,26}) ||
       !componentVersion("not installed").empty() ||
       !componentNewer("2.9.8", "2.9.7") || componentNewer("2.9.8","2.9.8") ||
       componentNewer("2.9.8","3.0.0") || !componentNewer("9.0.2", "")) return 1;
    std::atomic_bool stop{false};
    const auto root = fs::temp_directory_path() / (L"StreamShelf-component-regression-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    fs::create_directory(root);
    auto clean = [&] { std::error_code ec; fs::remove(root/L"yt-dlp.exe",ec); fs::remove(root,ec); };
    try {
        auto before = componentFingerprint(root);
        fs::copy_file(executable(), root/L"yt-dlp.exe");
        auto after = componentFingerprint(root);
        auto resolved = terminalComponentCommand(L"yt-dlp --version",root);
        if (resolved.find((root/L"yt-dlp.exe").wstring()) == std::wstring::npos || resolved.find(L"--version") == std::wstring::npos) throw std::runtime_error("terminal did not select the component directory");
        if (before == after) throw std::runtime_error("new file not detected");
        std::vector<Component> c={{"yt-dlp"},{"FFmpeg"},{"FFprobe"},{"Deno"}}; c[0].latest="2026.10.26";
        c = refreshInstalledComponents(root,c,stop);
        if (c[0].installed != "2026.10.26" || c[0].update) throw std::runtime_error("installed version not detected");
        fs::remove(root/L"yt-dlp.exe");
        if (componentFingerprint(root) == after) throw std::runtime_error("file removal not detected");
        bool rejected=false; try { terminalComponentCommand(L"yt-dlp --version",root); } catch (const std::exception &) { rejected=true; }
        if (!rejected) throw std::runtime_error("missing component could fall back to PATH");
        c = refreshInstalledComponents(root,c,stop);
        if (!c[0].installed.empty() || !c[0].update || c[0].latest!="2026.10.26") throw std::runtime_error("removed file left stale installed status");
        clean();
    } catch (const std::exception &e) { clean(); std::cerr<<e.what()<<"\n"; return 2; }
    std::cout<<"Version, installation, and removal cases passed\n"; return 0;
}
