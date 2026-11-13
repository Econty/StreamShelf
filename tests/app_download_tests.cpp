#include "app.hpp"
#include "pipe_io.hpp"
#include <future>
#include <iostream>
using namespace ss;

struct DownloadAccess { using type = void (App::*)(const std::shared_ptr<Job> &); friend type get(DownloadAccess); };
struct DownloadLoopAccess { using type = void (App::*)(); friend type get(DownloadLoopAccess); };
struct ReadyAccess { using type = bool App::*; friend type get(ReadyAccess); };
struct StoppingAccess { using type = std::atomic_bool App::*; friend type get(StoppingAccess); };
template<class Tag, typename Tag::type member> struct Access { friend typename Tag::type get(Tag) { return member; } };
template struct Access<DownloadAccess, &App::download>;
template struct Access<DownloadLoopAccess, &App::downloadLoop>;
template struct Access<ReadyAccess, &App::ready>;
template struct Access<StoppingAccess, &App::stopping>;

int main(int argc, char **argv) {
    if (argc != 2 && !(argc == 3 && (std::string(argv[1]) == "--credential-download" ||
                                     std::string(argv[1]) == "--filenames"))) return 2;
    try {
        auto root = dataRoot();
        if (root.parent_path() != executable().parent_path() || root.filename() != L"preview-data")
            throw std::runtime_error("App download test must use isolated preview data");
        if (std::string(argv[1]) == "--filenames") {
            const std::string base=argv[2];
            const auto destination=root/L"name-output";
            fs::create_directories(destination);
            {
                App app;
                app.*get(ReadyAccess{})=true;
                const Json format={{"format_id","137"},{"format_note","1080p"},
                                   {"url",base+"/first"},{"vcodec","h264"},{"acodec","aac"}};
                auto first=parseAnalysis(Json{{"title","First video"},{"formats",Json::array({format})}},base+"/page-a");
                auto second=parseAnalysis(Json{{"title","Second video"},{"formats",Json::array({format})}},base+"/page-b");
                auto firstItem=first.items.front(), secondItem=second.items.front();
                // Exercise the real direct downloader and final move with a local
                // inert HTTP fixture, while retaining the parsed format's title.
                for(auto item:{&firstItem,&secondItem}) {
                    item->format=false;item->category="Изображения";item->ext="jpg";item->selected=true;
                }
                firstItem.url=base+"/first";secondItem.url=base+"/second";
                app.analysis=second;app.title=second.title;
                app.media={firstItem,secondItem};app.destination=utf8(destination.wstring());
                app.enqueue();
                if(app.jobs.size()!=2 || app.jobs[0]->title!="First video · 1080p" || app.jobs[1]->title!="Second video · 1080p")
                    throw std::runtime_error("Queued titles did not retain their original source pages");
                std::thread worker([&] { (app.*get(DownloadLoopAccess{}))(); });
                struct JoinWorker {
                    App &app;std::thread &worker;
                    ~JoinWorker() { app.*get(StoppingAccess{})=true;if(worker.joinable())worker.join(); }
                } join{app,worker};
                const auto deadline=GetTickCount64()+10000;
                while(GetTickCount64()<deadline && !(app.jobs[0]->done && app.jobs[1]->done)) { app.drain();Sleep(10); }
                app.drain();
                if(!app.jobs[0]->done || !app.jobs[1]->done || app.jobs[0]->state!="Готово" || app.jobs[1]->state!="Готово")
                    throw std::runtime_error("Local filename fixture did not complete both downloads");
                if(!fs::is_regular_file(destination/L"First video · 1080p.jpg") ||
                   !fs::is_regular_file(destination/L"Second video · 1080p.jpg"))
                    throw std::runtime_error("Output names do not contain their source page titles");
            }
            {
                App restored;
                if(restored.jobs.size()!=2 || restored.jobs[0]->item.sourceTitle!="First video" ||
                   restored.jobs[1]->item.sourceTitle!="Second video")
                    throw std::runtime_error("Source page titles did not survive queue persistence");
            }
            std::cout << "Each page title survives queueing, saving, and output naming\n";
            return 0;
        }
        if (std::string(argv[1]) == "--credential-download") {
            App app;
            app.*get(ReadyAccess{}) = true;
            const std::string target=argv[2];
            const std::string page="https://example.test/watch";
            const auto pipeName=L"\\\\.\\pipe\\StreamShelf-credential-regression-"+
                std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
            Handle server(CreateNamedPipeW(pipeName.c_str(), PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS, 1, 4096, 4096, 0, nullptr));
            if(server.h==INVALID_HANDLE_VALUE)throw std::runtime_error("Could not create credential fixture pipe");
            Handle client(CreateFileW(pipeName.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,
                OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
            if(client.h==INVALID_HANDLE_VALUE)throw std::runtime_error("Could not open credential fixture pipe");
            std::atomic_bool cancel{false};
            if(!pipeio::connect(server,cancel))throw std::runtime_error("Could not connect credential fixture pipe");
            auto session=std::make_shared<Session>();
            session->pipe=server;
            session->scopedCredentialsSupported=true;
            session->updateTab({{"tabId",7},{"url",page},{"storeId","store-A"},{"sourceId",uint64_t(1)}});
            const auto source=session->source();
            auto reply = [&](bool partitioned) {
                uint32_t size=0;
                if(!pipeio::transfer(client,&size,4,false,3000) || !size || size>=MaxFrame)
                    throw std::runtime_error("Missing credential request frame");
                std::string body(size,'\0');
                if(!pipeio::transfer(client,body.data(),size,false,3000))
                    throw std::runtime_error("Incomplete credential request frame");
                auto request=Json::parse(body);
                if(str(request,"type")!="credentials" || str(request,"url")!=target)
                    throw std::runtime_error("Unexpected credential request");
                session->acceptCredentialReply({{"type","credentialsResult"},{"requestId",str(request,"requestId")},
                    {"url",target},{"tabId",7},{"sourceId",uint64_t(1)},{"storeId","store-A"},
                    {"headers",Json::object()},
                    {"cookies",Json::array({Json{{"domain","example.test"},{"path","/"},
                        {"name","session"},{"value","synthetic"},{"secure",false},{"httpOnly",false},
                        {"partitioned",partitioned}}})}});
            };
            auto valid=std::async(std::launch::async,[&] { return app.credentials(session,target,cancel,&source); });
            reply(false);
            if(valid.get().first.find("synthetic")==std::string::npos)
                throw std::runtime_error("Valid browser credentials were not accepted");
            auto job=std::make_shared<Job>();
            job->folder=utf8((root/L"credential-download").wstring());
            job->item.category="Видео";job->item.ext="mp4";job->item.url=target;job->page=page;
            job->session=session;job->browserSourceRequired=true;job->browserSource=source;
            auto blocked=std::async(std::launch::async,[&] {
                try { (app.*get(DownloadAccess{}))(job); }
                catch(const BrowserSourceChanged &) { return true; }
                catch(const std::exception &) { return false; }
                return false;
            });
            reply(true);
            if(!blocked.get())throw std::runtime_error("Partitioned cookie did not fail closed");
            if(fs::exists(root/L"credential-download"/L"media.mp4.part"))
                throw std::runtime_error("A rejected browser snapshot started a download");
            std::cout << "Partitioned browser cookies fail closed before network I/O\n";
            return 0;
        }
        if (std::string(argv[1]) == "--preview-cache") {
            App app;
            std::vector<Media> visible(80);
            std::vector<std::shared_ptr<Preview>> seen(visible.size());
            for (size_t i=0;i<visible.size();++i) {
                visible[i].key="visible-"+std::to_string(i);
                visible[i].category="Изображения";
                visible[i].thumbnail=utf8((root/(L"absent-preview-"+std::to_wstring(i)+L".png")).wstring());
            }
            bool complete=false;
            const auto deadline=GetTickCount64()+10000;
            while(GetTickCount64()<deadline) {
                app.beginPreviewFrame();
                for(size_t i=0;i<visible.size();++i) {
                    auto current=app.requestPreview(visible[i]);
                    if(!current)continue;
                    if(seen[i] && seen[i]->id!=current->id)
                        throw std::runtime_error("A still-visible preview was evicted and re-requested");
                    seen[i]=current;
                }
                Sleep(2);
                app.drain();
                complete=std::all_of(seen.begin(),seen.end(),[](const auto &preview){return preview && preview->done;});
                if(complete)break;
            }
            if(!complete)throw std::runtime_error("Visible previews starved before receiving a slot");
            // A shared thumbnail can only share work for the same source.
            auto firstSession=std::make_shared<Session>(), secondSession=std::make_shared<Session>();
            BrowserSource source{7,1,"https://example.test/watch","store",uint64_t(1)};
            Media first=visible[0], same=first, other=first;
            first.key="same-source-first";same.key="same-source-second";other.key="different-session";
            app.mediaSessions[first.key]=app.mediaSessions[same.key]=firstSession;
            app.mediaSessions[other.key]=secondSession;
            app.mediaSources[first.key]=app.mediaSources[same.key]=app.mediaSources[other.key]=source;
            app.beginPreviewFrame();
            auto a=app.requestPreview(first), b=app.requestPreview(same), c=app.requestPreview(other);
            if(!a || !b || !c || a->id!=b->id || a->id==c->id)
                throw std::runtime_error("Preview deduplication must retain browser-session isolation");
            std::cout << "All visible previews receive stable slots; deduplication respects browser sessions\n";
            return 0;
        }
        if (std::string(argv[1]) == "--history") {
            fs::create_directories(root);
            const auto queue = root / L"queue.dpapi";
            const std::string malformed = R"([{ "id":"first", "item":{} }, { "id":"bad" }, { "id":"tail", "item":{} }])";
            protectFile(queue, malformed);
            {
                App app;
                if (app.jobs.size() != 2 || app.jobs[0]->id != "first" || app.jobs[1]->id != "tail")
                    throw std::runtime_error("Valid entries around a malformed history entry must be restored");
                if (app.modalBody != "Историю очереди не удалось восстановить")
                    throw std::runtime_error("Partial recovery must be visible to the user");
            }
            const auto restored = Json::parse(unprotectFile(queue));
            if (restored.size() != 2 || restored[0]["id"] != "first" || restored[1]["id"] != "tail")
                throw std::runtime_error("Restored queue must retain both valid entries");
            bool savedOriginal = false;
            for (const auto &entry : fs::directory_iterator(root))
                if (entry.path().filename().wstring().rfind(L"queue-unreadable-", 0) == 0 &&
                    unprotectFile(entry.path()) == malformed) savedOriginal = true;
            if (!savedOriginal) throw std::runtime_error("The original malformed encrypted history must be backed up");

            // Even a blob that cannot be decrypted is preserved byte-for-byte,
            // not silently replaced by a newly encrypted empty queue.
            const std::string unreadable="not a DPAPI queue";
            writeFile(queue,unreadable);
            {
                App app;
                if(!app.jobs.empty() || app.modalBody!="Историю очереди не удалось восстановить")
                    throw std::runtime_error("Unreadable history must show recovery status");
            }
            bool savedRaw=false;
            for(const auto &entry:fs::directory_iterator(root))
                if(entry.path().filename().wstring().rfind(L"queue-unreadable-",0)==0 &&
                   readFile(entry.path())==unreadable) savedRaw=true;
            if(!savedRaw)throw std::runtime_error("Unreadable history must be preserved byte-for-byte");

            // A readable but >64 MiB encrypted history must load and survive
            // normal shutdown. Whitespace keeps the JSON itself small in RAM.
            std::string large = R"([{ "id":"large", "item":{} }])";
            large.append(size_t(64) * 1024 * 1024, ' ');
            protectFile(queue, large);
            large.clear();large.shrink_to_fit();
            {
                App app;
                if (app.jobs.size() != 1 || app.jobs[0]->id != "large")
                    throw std::runtime_error("History over the old 64 MiB read limit was not restored");
            }
            const auto recovered = Json::parse(unprotectFile(queue));
            if (recovered.size() != 1 || recovered[0]["id"] != "large")
                throw std::runtime_error("Large history was not persisted after shutdown");

            const auto limited = root / L"limited.dpapi";
            protectFile(limited, "existing");
            const auto before = readFile(limited);
            bool rejected = false;
            try { protectFile(limited, "a", 1); } catch (const std::exception &) { rejected = true; }
            if (!rejected || readFile(limited) != before)
                throw std::runtime_error("A protected output exceeding the read limit must not replace the original");
            std::cout << "Queue restores valid entries, preserves malformed data, and handles >64 MiB histories\n";
            return 0;
        }
        App app;
        app.*get(ReadyAccess{}) = true;
        auto fixture = executable().parent_path() / L"app_download_fixture.exe";
        if (!fs::is_regular_file(fixture)) throw std::runtime_error("Missing FFprobe test fixture");
        fs::copy_file(fixture, root / L"tools" / L"ffprobe.exe", fs::copy_options::overwrite_existing);
        auto job = std::make_shared<Job>();
        job->folder = utf8((root / L"job").wstring());
        job->item.category = "Видео";
        job->item.ext = "mp4";
        job->item.protocol = "http";
        job->item.url = argv[1];
        for (int attempt = 0; attempt < 2; ++attempt) {
            bool rejected = false;
            try { (app.*get(DownloadAccess{}))(job); }
            catch (const std::exception &) { rejected = true; }
            if (!rejected) throw std::runtime_error("Invalid response was accepted");
        }
        size_t quarantined = 0;
        for (const auto &entry : fs::directory_iterator(root / L"job"))
            if (entry.path().filename().wstring().rfind(L"invalid-", 0) == 0) ++quarantined;
        if (quarantined != 2) throw std::runtime_error("Both invalid files should be preserved separately");
        fs::copy_file(fixture, root / L"tools" / L"yt-dlp.exe", fs::copy_options::overwrite_existing);
        auto formatJob = std::make_shared<Job>();
        formatJob->folder = utf8((root / L"job-format").wstring());
        formatJob->assemblyInput = true;
        formatJob->item.format = true;
        formatJob->item.id = "synthetic";
        formatJob->item.category = "Видео";
        formatJob->page = argv[1];
        for (int attempt = 0; attempt < 2; ++attempt) {
            bool rejected = false;
            try { (app.*get(DownloadAccess{}))(formatJob); }
            catch (const std::exception &) { rejected = true; }
            if (!rejected) throw std::runtime_error("Invalid yt-dlp response was accepted");
        }
        if (readFile(root / L"tools" / L"attempts.txt") != "11")
            throw std::runtime_error("Retry must invoke yt-dlp again after invalid assembly input");
        quarantined = 0;
        for (const auto &entry : fs::directory_iterator(root / L"job-format"))
            if (entry.path().filename().wstring().rfind(L"invalid-", 0) == 0) ++quarantined;
        if (quarantined != 2) throw std::runtime_error("Both invalid yt-dlp outputs should be preserved");
        writeFile(root / L"job-format" / L"media.mp4", "old invalid media");
        try { (app.*get(DownloadAccess{}))(formatJob); }
        catch (const std::exception &) {}
        if (readFile(root / L"tools" / L"attempts.txt") != "111")
            throw std::runtime_error("A leftover invalid output must be quarantined before a fresh yt-dlp invocation");
        quarantined = 0;
        for (const auto &entry : fs::directory_iterator(root / L"job-format"))
            if (entry.path().filename().wstring().rfind(L"invalid-", 0) == 0) ++quarantined;
        if (quarantined != 4) throw std::runtime_error("Existing and newly invalid yt-dlp outputs should both be preserved");
        std::cout << "yt-dlp assembly retry re-downloaded and preserved invalid files\n";
        std::cout << "Both invalid responses were quarantined\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
