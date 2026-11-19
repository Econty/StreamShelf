#include "platform.hpp"
#include <iostream>
#include <shellapi.h>

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) return 2;
    try {
        std::atomic_bool cancel{false};
        const auto headers =
            ss::Json{{"User-Agent", "StreamShelf-Test-Agent"},
                     {"Referer", "https://page.example/watch"},
                     {"Authorization", "Bearer synthetic-test-secret"},
                     {"Origin", "https://page.example"}};
        if(argc>=3) {
            const std::string cookies="127.0.0.1\tFALSE\t/\tFALSE\t0\tsession\tsynthetic-test-cookie\n";
            auto body=ss::httpPreviewPrefix(argv[1],cancel,headers,cookies,argc==4?32*1024*1024:4);
            if(argc==3) { if(body!="test") return 4; }
            else {
                const auto base=ss::fs::temp_directory_path()/(L"StreamShelf-PreviewFetch-"+std::to_wstring(GetCurrentProcessId()));
                const auto input=base/L"prefix.bin",output=base/L"frame.png";
                struct Cleanup {ss::fs::path input,output,base;~Cleanup(){std::error_code ec;ss::fs::remove(input,ec);ss::fs::remove(output,ec);ss::fs::remove(base,ec);}} cleanup{input,output,base};
                ss::writeFile(input,body);
                int wideCount=0;
                auto wideArgs=CommandLineToArgvW(GetCommandLineW(),&wideCount);
                struct WideArgsCleanup {wchar_t **args;~WideArgsCleanup(){LocalFree(args);}} wideCleanup{wideArgs};
                if(!wideArgs || wideCount!=argc)return 6;
                auto decoded=ss::runProcess(wideArgs[3],{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",
                    L"-protocol_whitelist",L"file,pipe,crypto",L"-format_whitelist",L"mov,matroska,webm,avi,mpegts,mpeg,mpegvideo,mp3,ogg,flac,aac,wav,asf",
                    L"-i",input.wstring(),L"-frames:v",L"1",L"-an",L"-vf",L"scale=480:270:force_original_aspect_ratio=decrease",L"-y",output.wstring()},nullptr,cancel,true);
                if(decoded.code || !ss::fs::is_regular_file(output) || ss::readFile(output).rfind("\x89PNG\r\n\x1a\n",0)!=0)return 5;
            }
        } else {
            auto body=ss::httpPreview(argv[1],cancel,headers,"");
            if(body!="OK") return 3;
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
