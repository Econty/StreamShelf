#pragma once
#include "render.hpp"
#include "extension_installer.hpp"
#include "mux_options.hpp"
#include "assembly_source.hpp"
#include "component_manager.hpp"
#include <deque>
#include <array>
#include <shared_mutex>
#include <set>
namespace ss {
struct Preview {PreviewImage image;std::string message="Загрузка превью…";bool done=false;};
struct Job {
    std::string id, title, state = "Ожидание", detail, metrics, folder, page, pair, local, output;
    Media item;
    std::shared_ptr<Session> session;
    bool mux = false;
    bool assemblyInput = false, localFile = false, conversionSource = false;
    bool singleExport = false;
    int exportFormat = 0, exportQuality = 1;
    bool assembly = false, waitingSources = false, decisionNeeded = false, prompted = false;
    std::vector<AssemblySource> sources;
    std::string assemblyMetadataTitle, assemblyArtist, assemblyDescription;
    MuxOptions muxOptions;
    std::vector<std::string> muxInputs;
    std::string muxChapters;
    std::string outputDirectory;
    // Accessed only by the worker processing this job; published on the UI thread.
    std::string workerOutput, workerCategory;
    std::atomic_bool cancel{false}, claimed{false}, done{false};
    double percent = 0;
};
class App {
    std::mutex eventsMutex;
    std::deque<std::function<void()>> events;
    struct Worker { std::thread thread; std::shared_ptr<std::atomic_bool> finished; };
    std::vector<Worker> workers;
    std::vector<std::thread> downloadWorkers;
    std::mutex jobsMutex;
    std::condition_variable jobsCv;
    bool subtitleActive=false;
    ULONGLONG subtitleResumeAt=0;
    unsigned subtitleBackoff=60;
    int active = 0;
    std::atomic_int parallel{2};
    std::atomic_bool stopping{false};
    std::shared_ptr<std::atomic_bool> terminalCancel;
    std::shared_ptr<std::atomic_bool> analysisCancel = std::make_shared<std::atomic_bool>(false);
    uint64_t analysisGeneration = 0;
    uint64_t scanGeneration = 0, silentScanStop = 0;
    std::shared_ptr<std::atomic_bool> probeCancel = std::make_shared<std::atomic_bool>(false);
    uint64_t nextScanGeneration() { *probeCancel = true; readyCv.notify_all(); probeCancel = std::make_shared<std::atomic_bool>(false); return scanGeneration = std::max(scanGeneration + 1, uint64_t(GetTickCount64()) * 100); }
    std::shared_mutex toolsMutex;
    std::mutex readyMutex;
    std::condition_variable readyCv;
    bool ready = false;
    uint64_t jobSerial = 0;
    int previewActive=0;
    struct ProbeTask { Media item; std::shared_ptr<Session> session; std::shared_ptr<std::atomic_bool> cancelled; uint64_t generation = 0; int tab = -1; int priority = 0; };
    std::mutex probeMutex;
    std::condition_variable probeCv;
    std::deque<ProbeTask> probes;
    std::vector<std::thread> probeWorkers;
    void resetCredentialSnapshots();
    void enqueueProbe(const Media &, const std::shared_ptr<Session> &, uint64_t, int);
    void probeLoop();
    void seedAndUpdate(bool);
    void scanToolComponents(bool install);
    void refreshToolFiles();
    void downloadLoop();
    void download(const std::shared_ptr<Job> &);
    void saveHistory();
    void updateAssemblies();
    void resolveAssembly(const std::shared_ptr<Job> &, bool);
    void downloadAssembly(const std::shared_ptr<Job> &);
    ProcessResult runFfmpegWithProgress(const std::shared_ptr<Job> &, const std::vector<std::wstring> &, double);

  public:
    HWND hwnd{};
    Connector connector;
    ExtensionInstaller extensionInstaller;
    bool preparingInstaller = false;
    bool componentsBusy = false, componentsScanned = false, componentsDirty = false;
    uint64_t componentsRevision = 0;
    std::string componentsActivity;
    bool componentProgressVisible = false;
    ComponentProgress componentProgress;
    std::vector<Component> components;
    std::string installerBrowserExecutable;
    std::vector<Browser> browsers;
    std::set<std::string> customBrowserPaths;
    std::vector<std::shared_ptr<Session>> sessions;
    std::vector<Media> media;
    std::array<bool,11> videoEncoders{true,false,false,false,false,false,false,false,false,false,false};
    std::array<bool,5> audioEncoders{true,false,false,false,false};
    std::array<bool,5> subtitleEncoders{true,false,false,false,false};
    std::array<bool,4> imageEncoders{true,false,false,false};
    bool videoEncodersReady = false;
    void detectVideoEncoders();
    void addLocalFiles(const std::vector<std::string> &);
    std::map<std::string, std::shared_ptr<Session>> mediaSessions;
    std::map<std::string,std::shared_ptr<Preview>> previews;
    uint64_t previewGeneration = 0;
    std::vector<std::shared_ptr<Job>> jobs;
    std::vector<std::string> logs;
    Analysis analysis;
    std::shared_ptr<Session> analysisSession;
    std::string url, title = "Медиа", destination, search, logSearch, terminalCommand,
                     status = "Подготовка инструментов…", connection = "Браузер не подключён", toast,
                     modalTitle, modalBody, modalConfirmLabel = "Продолжить";
    std::function<void()> modalConfirm;
    int browserIndex = 0, profileIndex = 0, sessionIndex = -1, category = 1, audioIndex = -1;
    std::atomic_int fragments{4};
    bool scanCommandPending = false;
    bool splashEnabled = true;
    int splashSeconds = 3;
    bool busy = false, autoUpdate = true, light = false, reduced = false, scanning = false, keepResults = false,
         exitRequested = false;
    std::string selectedJob, selectedMedia;
    std::set<std::string> selectedJobs;
    MuxOptions muxOptions;
    bool constructorOpen = false, downloadChoice = false;
    std::vector<AssemblySource> sourcePool, draft;
    int draftSelection = -1;
    bool singleExportMode = false, constructorModeManual = false, constructorFormatManual = false;
    std::string assemblyName = "Мой контейнер", assemblyMetadataTitle, assemblyArtist, assemblyDescription;
    void openConstructor(bool completed = false);
    void enqueueAssembly();
    void importAssemblyFile();
    void autoConstruct();
    void updateConstructorRecommendation();
    void readDroppedMetadata(const AssemblySource &);
    uint64_t toastSerial = 0;
    int requestedPage = -1;
    App();
    ~App();
    void start();
    void post(std::function<void()>);
    void drain();
    void work(std::function<void()>);
    void log(const std::string &);
    void notify(const std::string &);
    void error(const std::string &);
    void action(const std::string &);
    std::shared_ptr<Session> session();
    fs::path tool(const wchar_t *);
    std::pair<std::string, Json> credentials(std::shared_ptr<Session>, const std::string &,
                                             std::atomic_bool &);
    void analyze(const std::string &, bool deep = false);
    std::shared_ptr<Preview> requestPreview(const Media &);
    void enqueue();
    void refreshSessions();
    void closeRequest();
    int getParallel() const {
        return parallel;
    }
    void setParallel(int);
};
struct Hit {
    std::string id;
    Rect rect;
    std::function<void()> action;
    bool edit = false;
};
class UI {
    App &app;
    Canvas c;
    std::map<std::string, Spring> motion;
    struct LabelState {
        std::string old, current;
        Spring mix{1};
    };
    std::map<std::string, LabelState> labels;
    std::vector<Hit> hits;
    std::optional<Hit> capture;
    std::string focus;
    std::string *editing = nullptr;
    std::string suggestionField;
    std::optional<std::string> suggestionDismissedValue;
    std::vector<std::string> terminalHistory;
    size_t terminalHistoryIndex=0;
    std::string terminalDraft;
    void runTerminalCommand();
    std::vector<std::pair<std::string, std::string>> suggestions;
    Rect suggestionAnchor{};
    int suggestionIndex = 0;
    void acceptSuggestion(int);
    std::wstring logSelectionBuffer;
    size_t logAnchor=0, logCaret=0;
    std::map<std::string, size_t> carets, anchors;
    float mx = 0, my = 0, downX = 0, downY = 0, dt = .016f, width = 1440, height = 920, scale = 1;
    bool down = false, overlayInput = false;
    int page = 0, sortColumn = 0;
    bool descending = false;
    Spring nav{111}, transition{1}, appearance{0}, theme{0};
    Scroll tableScroll, mediaScroll, jobsScroll, browserScroll, logScroll, settingsScroll, aboutScroll, menuScroll,
        modalScroll;
    std::string menuId;
    std::vector<std::string> menuItems;
    Rect menuAnchor{};
    std::function<void(int)> menuSelect;
    bool menuOpen = false, modalWas = false;
    double clock = 0, toastTime = 0;
    ULONGLONG splashStarted = 0;
    uint64_t lastToast = 0;
    std::map<std::string, float> rowY;
    std::string tableSignature;
    bool cardView=true, jobDetails=false, jobsDescending=false;
    bool mediaSelectedFirst=false, jobsSelectedFirst=false;
    void collectionSurface(const std::string &, Rect, bool details, bool selected, size_t index);
    float collectionRowY(const std::string &, float);
    void selectionHeading(const std::string &, Rect, bool &);
    int jobSortColumn=0;
    std::map<std::string,unsigned> previewTextures;
    std::map<uint64_t,std::string> metadataPreviews;
    void previewTile(const Media &,Rect);
    Ink bg, panel, edge, text, muted, accent;
    float to(const std::string &, float, float initial = 0, float freq = 5, float damping = .9f);
    void hit(const std::string &, Rect, std::function<void()>, bool edit = false);
    float hover(const std::string &, Rect);
    void label(const std::string &, const std::string &, float, float, float, Ink, float max = 1e8f);
    void button(const std::string &, Rect, const std::string &, std::function<void()>, bool primary = false,
                bool enabled = true);
    void check(const std::string &, Rect, bool, std::function<void()>);
    void toggle(const std::string &, Rect, bool, std::function<void()>);
    void input(const std::string &, Rect, std::string &, const std::string &);
    void choice(const std::string &, Rect, const std::vector<std::string> &, int, std::function<void(int)>);
    void card(Rect);
    void title(Rect, const std::string &, const std::string &);
    void scrollbar(const std::string &, Rect, Scroll &);
    Scroll poolScroll, draftScroll, propertyScroll;
    Rect poolView{}, draftView{}, propertyView{};
    bool finishDraftDrag(const Hit &);
    int draftCategory = 0;
    bool advancedTrack = false;
    uint64_t nextTileId = 0;
    int dragBefore = -1, settleIndex = -1;
    bool settling = false, settlePool = false;
    ULONGLONG settleStart = 0;
    Rect settleFrom{}, settleTarget{};
    struct AutoTileFlight { Rect origin; ULONGLONG start; };
    std::map<uint64_t, AutoTileFlight> autoTileFlights;
    std::vector<Rect> poolTileRects;
    std::vector<std::pair<Rect,int>> dropGroups;
    std::vector<std::pair<Rect,int>> dropRows;
    std::string visibleAssemblyJob;
    double assemblyFinishedAt = -1;
    void assemblyProgressPanel();
    void constructorPage(Rect);
    void downloadChoicePage(Rect);
    void mediaPage(Rect);
    void startMediaAnalysis(bool deep);
    void browserPage(Rect);
    void downloadsPage(Rect);
    void logsPage(Rect);
    void settingsPage(Rect);
    void aboutPage(Rect);
    void overlays();
    std::vector<Media *> filtered();
    void cancelInput();

  public:
    UI(App &a) : app(a) {}
    void init();
    void frame(int, int, float, float dpi = 1, bool resizing = false);
    void destroy() {
        for(auto &[key,texture]:previewTextures)c.deleteImage(texture);
        c.destroy();
    }
    void navigate(int);
    bool acceptsFileDrop() const { return page == 2; }
    void mouse(float, float);
    void rightClick(float, float);
    void press();
    void release();
    void wheel(float, bool);
    void character(wchar_t);
    void key(int, bool, bool);
    void lostCapture();
    bool draggingTitle(float, float) const;
    void setDialog(const std::string &, const std::string &);
    bool finished() const {
        return app.exitRequested && appearance.value < .005f;
    }
    std::string driver() {
        return c.driver();
    }
};
} // namespace ss
