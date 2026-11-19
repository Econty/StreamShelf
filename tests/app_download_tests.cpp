#include "app.hpp"
#include "pipe_io.hpp"
#include <future>
#include <iostream>
using namespace ss;

struct DownloadMethodAccess { using type = void (App::*)(const std::shared_ptr<Job> &); friend type get(DownloadMethodAccess); };
struct DownloadLoopAccess { using type = void (App::*)(); friend type get(DownloadLoopAccess); };
struct ReadyAccess { using type = bool App::*; friend type get(ReadyAccess); };
struct StoppingAccess { using type = std::atomic_bool App::*; friend type get(StoppingAccess); };
struct SaveQueueAccess { using type = void (App::*)(); friend type get(SaveQueueAccess); };
struct SaveCredentialsAccess { using type = bool (App::*)(Job &, const JobDownloadAccess &); friend type get(SaveCredentialsAccess); };
struct RememberMediaAccess {
    using type = void (App::*)(const std::shared_ptr<Session> &, const std::string &, Session::CredentialSnapshot);
    friend type get(RememberMediaAccess);
};
struct QueueAssemblyAccess { using type = void (App::*)(); friend type get(QueueAssemblyAccess); };
struct JobAccessMethod { using type = JobDownloadAccess (App::*)(Job &, bool); friend type get(JobAccessMethod); };
struct RevokeSessionAccess {
    using type = void (App::*)(const std::shared_ptr<Session> &);
    friend type get(RevokeSessionAccess);
};
struct AnalysisCredentialsAccess {
    using type = void (App::*)(const std::shared_ptr<Session> &, const std::string &, std::atomic_bool &,
                              BrowserSource &, Session::CredentialSnapshot &, const std::function<void(const DownloadAccess &)> &);
    friend type get(AnalysisCredentialsAccess);
};
template<class Tag, typename Tag::type member> struct Access { friend typename Tag::type get(Tag) { return member; } };
template struct Access<DownloadMethodAccess, &App::download>;
template struct Access<DownloadLoopAccess, &App::downloadLoop>;
template struct Access<ReadyAccess, &App::ready>;
template struct Access<StoppingAccess, &App::stopping>;
template struct Access<SaveQueueAccess, &App::saveHistory>;
template struct Access<SaveCredentialsAccess, &App::saveDownloadAccess>;
template struct Access<RememberMediaAccess, &App::rememberMediaAccess>;
template struct Access<QueueAssemblyAccess, &App::enqueueAssembly>;
template struct Access<JobAccessMethod, &App::getDownloadAccess>;
template struct Access<RevokeSessionAccess, &App::revokeSessionAccess>;
template struct Access<AnalysisCredentialsAccess, &App::prepareAnalysisAccess>;

int main(int argc, char **argv) {
    if (argc != 2 && !(argc == 3 && (std::string(argv[1]) == "--credential-download" ||
                                     std::string(argv[1]) == "--filenames" ||
                                     std::string(argv[1]) == "--save-retry-access" ||
                                     std::string(argv[1]) == "--offline-assembly"))) return 2;
    try {
        auto root = dataRoot();
        if (root.parent_path() != executable().parent_path() || root.filename() != L"preview-data")
            throw std::runtime_error("App download test must use isolated preview data");
        if (std::string(argv[1]) == "--offline-assembly") {
            const std::string base = argv[2], page = base + "/offline-page", direct = base + "/offline-file";
            const auto cookie = Json{{"domain", "127.0.0.1"}, {"path", "/"}, {"name", "checkpoint"}, {"value", "local-fixture-value"}};
            const auto backup = root / L"offline-snapshot-fixture.dpapi";
            protectFile(root / L"cookies.dpapi", cookies(Json::array({Json{{"domain", "127.0.0.1"}, {"path", "/"},
                {"name", "unrelated-import"}, {"value", "must-not-be-sent"}}})));
            App app;
            app.*get(ReadyAccess{}) = true;
            app.destination = utf8((root / L"offline-assembly-output").wstring());
            fs::create_directories(wide(app.destination));
            const auto initialJobs = app.jobs.size();
            auto session = std::make_shared<Session>();
            session->scopedCredentialsSupported = true;
            session->updateTab({{"tabId", 4}, {"url", page}, {"sourceId", uint64_t(1)}, {"storeId", "fixture-profile"},
                                {"credentialRevision", uint64_t(1)}});
            const auto original = session->source();
            const auto record = [&](const std::string &target) {
                protectFile(backup, Json{{"url", target}, {"tabId", 4}, {"sourceId", uint64_t(1)}, {"storeId", "fixture-profile"},
                    {"credentialRevision", uint64_t(1)}, {"headers", Json::object()}, {"cookies", Json::array({cookie})}}.dump());
                Session::CredentialSnapshot snapshot{std::make_shared<std::string>(readFile(backup)), original,
                                                     session->instanceSerial, GetTickCount64()};
                (app.*get(RememberMediaAccess{}))(session, target, std::move(snapshot));
            };
            record(direct);
            session->updateCredentials({{"tabId", 4}, {"sourceId", uint64_t(1)}, {"storeId", "fixture-profile"},
                                        {"credentialRevision", uint64_t(2)}});
            record(page);
            fs::remove(backup);
            auto analysis = parseAnalysis(Json{{"title", "Offline test"},
                {"formats", Json::array({Json{{"format_id", "96"}, {"url", base + "/video"}, {"vcodec", "avc1"}, {"acodec", "none"}},
                                         Json{{"format_id", "140"}, {"url", base + "/audio"}, {"vcodec", "none"}, {"acodec", "mp4a"}}})},
                {"thumbnails", Json::array({Json{{"id", "cover"}, {"url", base + "/offline-cover"}}})}}, page);
            auto video = analysis.items[0], audio = analysis.items[1], cover = analysis.items[2];
            Media captured;
            captured.id = "network:fixture"; captured.key = "direct-fixture"; captured.url = direct; captured.page = page;
            captured.ext = "jpg"; captured.category = "Изображения"; captured.title = "Captured image";
            video.selected = captured.selected = true;
            app.media = {video, audio, cover, captured};
            for (const auto &item : app.media) {
                app.mediaSessions[item.key] = session;
                app.mediaSources[item.key] = original;
            }
            app.enqueue();
            if (app.jobs.size() != initialJobs + 2)
                throw std::runtime_error("Ordinary downloads could not queue after a cookie revision");
            auto directJob = app.jobs.back();
            session->updateTab({{"tabId", 10}, {"url", base + "/unrelated"}, {"sourceId", uint64_t(2)},
                                {"storeId", "other-profile"}, {"credentialRevision", uint64_t(0)}});
            session->alive = false;
            app.assemblyName = "Offline container";
            app.muxOptions.container = 0;
            for (int role : {0, 1, 3}) {
                AssemblySource track;
                track.media = role == 0 ? video : role == 1 ? audio : cover;
                track.role = role; track.name = track.media.id;
                app.draft.push_back(std::move(track));
            }
            (app.*get(QueueAssemblyAccess{}))();
            if (app.jobs.size() != initialJobs + 6 || !app.jobs.back()->assembly)
                throw std::runtime_error("The constructor could not queue analyzed media after browser shutdown");
            for (const auto index : {initialJobs, initialJobs + 2, initialJobs + 3}) {
                auto access = (app.*get(JobAccessMethod{}))(*app.jobs[index], false);
                if (access.target.first.find("local-fixture-value") == std::string::npos ||
                    access.target.first.find("must-not-be-sent") != std::string::npos)
                    throw std::runtime_error("The job did not retain its original analysis access");
            }
            auto coverJob = app.jobs[initialJobs + 4];
            (app.*get(DownloadMethodAccess{}))(directJob);
            (app.*get(DownloadMethodAccess{}))(coverJob);
            for (const auto &job : {directJob, coverJob})
                if (job->workerOutput.empty() || readFile(wide(job->workerOutput)) != "fixture-download")
                    throw std::runtime_error("An offline file or cover did not download");
            (app.*get(RevokeSessionAccess{}))(session);
            if (app.mediaSources.contains(video.key) || fs::exists(jobAccessPath(root, directJob->resumeAccessKey)))
                throw std::runtime_error("Explicit permission withdrawal did not clear access");
            bool refused = false;
            try { (void)(app.*get(JobAccessMethod{}))(*app.jobs[initialJobs + 2], false); }
            catch (const BrowserCredentialsChanged &) { refused = true; }
            if (!refused) throw std::runtime_error("A revoked job could still obtain saved access");
            fs::remove(root / L"cookies.dpapi");
            return 0;
        }
        if (std::string(argv[1]) == "--save-retry-access") {
            App app;
            auto job = std::make_shared<Job>();
            job->id = "stored-download";
            job->title = "Restart fixture";
            job->item.id = "network:restart-fixture";
            job->item.url = argv[2];
            job->item.category = "Изображения";
            job->item.ext = "jpg";
            job->page = "http://127.0.0.1/source";
            job->folder = utf8((root / L"restart-output").wstring());
            job->browserSourceRequired = true;
            job->resumeAccessKey = newJobAccessKey();
            const auto jar = cookies(Json::array({Json{{"domain", "127.0.0.1"}, {"hostOnly", true},
                {"path", "/restart-access"}, {"name", "checkpoint"}, {"value", "local-fixture-value"}}}));
            if (!(app.*get(SaveCredentialsAccess{}))(*job, {{jar, Json::object()}, std::nullopt}))
                throw std::runtime_error("Job access was not saved");
            app.jobs.push_back(job);
            (app.*get(SaveQueueAccess{}))();
            if (readFile(jobAccessPath(root, job->resumeAccessKey)).find("local-fixture-value") != std::string::npos ||
                unprotectFile(root / L"queue.dpapi").find("local-fixture-value") != std::string::npos)
                throw std::runtime_error("Download access was not kept separate and encrypted");
            return 0;
        }
        if (std::string(argv[1]) == "--resume-without-browser") {
            App app;
            auto found = std::find_if(app.jobs.begin(), app.jobs.end(), [](const auto &job) { return job->id == "stored-download"; });
            if (found == app.jobs.end() || (*found)->session || (*found)->resumeAccessKey.empty())
                throw std::runtime_error("Job did not restore with its persistent access reference");
            auto job = *found;
            app.*get(ReadyAccess{}) = true;
            app.selectedJob = job->id;
            app.action("retryJob");
            if (job->done || job->claimed || job->cancel)
                throw std::runtime_error("Retry still required a live browser");
            (app.*get(DownloadMethodAccess{}))(job);
            if (job->workerOutput.empty() || readFile(wide(job->workerOutput)) != "fixture-download")
                throw std::runtime_error("Restored download did not use its saved access");
            const auto checkpoint = jobAccessPath(root, job->resumeAccessKey);
            if (!fs::is_regular_file(checkpoint))
                throw std::runtime_error("Completed download did not keep the checkpoint");
            job->done = true;
            job->state = "Готово";
            app.action("removeJob");
            if (std::find(app.jobs.begin(), app.jobs.end(), job) != app.jobs.end() || fs::exists(checkpoint))
                throw std::runtime_error("Removing the job did not remove its checkpoint");
            return 0;
        }
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
            auto requestFrame = [&] {
                uint32_t size=0;
                if(!pipeio::transfer(client,&size,4,false,3000) || !size || size>=MaxFrame)
                    throw std::runtime_error("Missing credential request frame");
                std::string body(size,'\0');
                if(!pipeio::transfer(client,body.data(),size,false,3000))
                    throw std::runtime_error("Incomplete credential request frame");
                auto request=Json::parse(body);
                if(str(request,"type")!="credentials" || str(request,"url")!=target)
                    throw std::runtime_error("Unexpected credential request");
                return request;
            };
            auto reply = [&](const Json &request, bool partitioned) {
                Json response{{"type","credentialsResult"},{"requestId",str(request,"requestId")},
                    {"url",target},{"tabId",7},{"sourceId",uint64_t(1)},{"storeId","store-A"},
                    {"headers",Json::object()},
                    {"cookies",Json::array({Json{{"domain","example.test"},{"path","/"},
                        {"name","session"},{"value","synthetic"},{"secure",false},{"httpOnly",false},
                        {"partitioned",partitioned}}})}};
                if (request.contains("credentialRevision")) response["credentialRevision"] = request["credentialRevision"];
                session->acceptCredentialReply(response);
            };
            auto valid=std::async(std::launch::async,[&] { return app.credentials(session,target,cancel,&source); });
            reply(requestFrame(), false);
            if(valid.get().first.find("synthetic")==std::string::npos)
                throw std::runtime_error("Valid browser credentials were not accepted");
            auto job=std::make_shared<Job>();
            job->folder=utf8((root/L"credential-download").wstring());
            job->item.category="Видео";job->item.ext="mp4";job->item.url=target;job->page=page;
            job->session=session;job->browserSourceRequired=true;job->browserSource=source;
            auto blocked=std::async(std::launch::async,[&] {
                try { (app.*get(DownloadMethodAccess{}))(job); }
                catch(const BrowserSourceChanged &) { return true; }
                catch(const BrowserCredentialsChanged &) { return true; }
                catch(const std::exception &) { return false; }
                return false;
            });
            reply(requestFrame(), true);
            if(!blocked.get())throw std::runtime_error("Partitioned cookie did not fail closed");
            if(fs::exists(root/L"credential-download"/L"media.mp4.part"))
                throw std::runtime_error("A rejected browser snapshot started a download");
            std::cout << "Partitioned browser cookies fail closed before network I/O\n";
            session->updateCredentials({{"tabId",7},{"sourceId",uint64_t(1)},{"storeId","store-A"},{"credentialRevision",uint64_t(1)}});
            auto analysisSource = session->source();
            Session::CredentialSnapshot analysisSnapshot;
            const auto analyzeAccess = [&] {
                DownloadAccess access;
                (app.*get(AnalysisCredentialsAccess{}))(session,target,cancel,analysisSource,analysisSnapshot,
                    [&](const DownloadAccess &prepared) { access = prepared; });
                return access;
            };
            auto freshAnalysis = std::async(std::launch::async,analyzeAccess);
            const auto initial = requestFrame();
            const auto refresh = [&](const Json &request) {
                session->acceptCredentialReply({{"type","error"},{"requestId",str(request,"requestId")},
                    {"message","Credentials were refreshed. Retry the request."}});
            };
            refresh(initial);
            const auto retried = requestFrame();
            if (retried["credentialRevision"] != initial["credentialRevision"] ||
                retried["requestId"] == initial["requestId"])
                throw std::runtime_error("The analysis did not retry a same-revision refresh with a fresh request");
            session->updateCredentials({{"tabId",7},{"sourceId",uint64_t(1)},{"storeId","store-A"},{"credentialRevision",uint64_t(2)}});
            const auto rotated = requestFrame();
            if (rotated["credentialRevision"] != uint64_t(2) || rotated["requestId"] == retried["requestId"])
                throw std::runtime_error("The analysis did not request the newly confirmed revision");
            reply(rotated,false);
            if (freshAnalysis.get().first.find("synthetic") == std::string::npos ||
                analysisSource.credentialRevision != uint64_t(2) || analysisSnapshot.source.credentialRevision != uint64_t(2))
                throw std::runtime_error("The analysis did not bind to the accepted credentials");
            auto changesDuringPreparation = std::async(std::launch::async,[&] {
                int preparations = 0;
                (app.*get(AnalysisCredentialsAccess{}))(session,target,cancel,analysisSource,analysisSnapshot,
                    [&](const DownloadAccess &) {
                        if (++preparations == 1)
                            session->updateCredentials({{"tabId",7},{"sourceId",uint64_t(1)},{"storeId","store-A"},{"credentialRevision",uint64_t(3)}});
                    });
                return preparations == 2;
            });
            reply(requestFrame(),false);
            const auto duringPreparation = requestFrame();
            if (duringPreparation["credentialRevision"] != uint64_t(3))
                throw std::runtime_error("Analysis used an outdated revision after preparing the request");
            reply(duringPreparation,false);
            if (!changesDuringPreparation.get() || analysisSource.credentialRevision != uint64_t(3))
                throw std::runtime_error("Analysis did not repeat preparation after a cookie refresh");
            auto invalidAnalysis = std::async(std::launch::async,[&] {
                try { (void)analyzeAccess(); } catch (const BrowserCookieFormatUnsupported &) { return true; }
                return false;
            });
            reply(requestFrame(),true);
            if (invalidAnalysis.wait_for(std::chrono::seconds(1)) != std::future_status::ready) cancel = true;
            if (!invalidAnalysis.get()) throw std::runtime_error("The analysis retried incompatible cookies");
            auto movedAnalysis = std::async(std::launch::async,[&] {
                try { (void)analyzeAccess(); } catch (const BrowserSourceChanged &) { return true; }
                return false;
            });
            const auto moving = requestFrame();
            session->updateTab({{"tabId",7},{"url",page+"/next"},{"storeId","store-A"},{"sourceId",uint64_t(2)},{"credentialRevision",uint64_t(1)}});
            refresh(moving);
            if (movedAnalysis.wait_for(std::chrono::seconds(1)) != std::future_status::ready) cancel = true;
            if (!movedAnalysis.get()) throw std::runtime_error("The analysis did not stop after navigation");
            std::cout << "Analysis refreshes access only within the original document and stops for unsupported cookies\n";
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
            constexpr unsigned char bitmap[]={
                0x42,0x4d,58,0,0,0,0,0,0,0,54,0,0,0,
                40,0,0,0,1,0,0,0,1,0,0,0,1,0,24,0,0,0,0,0,
                4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0xff,0
            };
            auto picture=root/L"preview-cache.bmp";
            writeFile(picture,{reinterpret_cast<const char *>(bitmap),sizeof bitmap});
            auto firstSession=std::make_shared<Session>(), secondSession=std::make_shared<Session>();
            BrowserSource source{7,1,"https://example.test/watch","store",uint64_t(1)};
            Media first=visible[0];
            first.key="remote-preview";
            first.thumbnail=utf8(picture.wstring());
            Job firstJob, secondJob;
            firstJob.session=firstSession;secondJob.session=secondSession;
            firstJob.browserSource=secondJob.browserSource=source;
            app.beginPreviewFrame();
            auto a=app.requestPreview(first,&firstJob), b=app.requestPreview(first,&firstJob), c=app.requestPreview(first,&secondJob);
            if(!a || !b || !c || a->id!=b->id || a->id==c->id)
                throw std::runtime_error("Unconfirmed previews must retain browser-session isolation");
            const auto settle=[&](const std::shared_ptr<Preview> &preview) {
                const auto until=GetTickCount64()+2000;
                while(preview && !preview->done && GetTickCount64()<until){Sleep(2);app.drain();}
                if(!preview || !preview->done)throw std::runtime_error("Preview did not complete");
            };
            settle(a);settle(c);
            if(!a->image.rgba.empty() || !c->image.rgba.empty())
                throw std::runtime_error("Remote metadata was allowed to read a local thumbnail file");
            first.key="local:"+utf8(picture.wstring());
            first.thumbnail.clear();
            auto localPreview=app.requestPreview(first);
            settle(localPreview);
            if(localPreview->image.rgba.empty())
                throw std::runtime_error("A user-selected local picture no longer has a preview");
            std::cout << "All visible previews receive stable slots and respect source and path boundaries\n";
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
            try { (app.*get(DownloadMethodAccess{}))(job); }
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
            try { (app.*get(DownloadMethodAccess{}))(formatJob); }
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
        try { (app.*get(DownloadMethodAccess{}))(formatJob); }
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
