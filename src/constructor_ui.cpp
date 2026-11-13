#include "app.hpp"
#include "language_options.hpp"
#include "interface_text.hpp"
namespace ss {
static const std::vector<std::string> roles={"Видео","Аудио","Субтитры","Обложка","Главы","Метаданные","Вложения"};
static Ink roleColor(int role) {
    static constexpr unsigned colors[] = {0x5599f4, 0x39bc8b, 0xc07ff2, 0xee9953, 0xd7b84e, 0x45b6c8, 0xa5a9c7};
    return hex(colors[role]);
}
static bool accepts(const AssemblySource &s,int role) {
    return s.role==role||(s.role==0&&s.includeAudio&&role==1&&s.stream<0);
}
static std::vector<MediaInfoField> infoFor(const AssemblySource &source, bool general) {
    std::vector<MediaInfoField> result;
    if(!general && source.role==6 && source.originalStreamInfo.is_object() && !source.originalStreamInfo.empty()) {
        for(const auto &[key,label]:std::vector<std::pair<std::string,std::string>>{{"codec_type","Тип потока"},{"codec_name","Формат"},{"codec_long_name","Описание формата"}}) {
            auto value=str(source.originalStreamInfo,key.c_str());if(!value.empty())result.push_back({"Attachment",label,value,0});
        }
        if(source.originalStreamTags.is_object())for(auto it=source.originalStreamTags.begin();it!=source.originalStreamTags.end();++it)
            if(it->is_string())result.push_back({"Attachment",it.key(),it->get<std::string>(),0});
        return result;
    }
    const std::string section=general?"General":source.role==0?"Video":source.role==1?"Audio":source.role==2?"Text":"Image";
    for(const auto &field:source.mediaInfoFields)
        if(field.section==section && (general || field.ordinal==source.mediaInfoOrdinal))result.push_back(field);
    return result;
}
void UI::assemblyProgressPanel() {
    auto processing=[](const std::shared_ptr<Job> &j){return j->state=="Сборка контейнера"||j->state=="Распознавание субтитров"||j->state=="Проверка результата"||j->state=="Объединение";};
    std::shared_ptr<Job> shown;int active=0;
    for(const auto &job:app.jobs)if(processing(job)){++active;if(!shown || job->id==visibleAssemblyJob)shown=job;}
    if(shown){visibleAssemblyJob=shown->id;assemblyFinishedAt=-1;}
    else if(!visibleAssemblyJob.empty()) {
        for(const auto &job:app.jobs)if(job->id==visibleAssemblyJob){shown=job;break;}
        if(!shown){visibleAssemblyJob.clear();return;}
        if(shown->state!="Готово"&&shown->state!="Ошибка"&&shown->state!="Отменено"){visibleAssemblyJob.clear();return;}
        if(assemblyFinishedAt<0)assemblyFinishedAt=clock;
        if(clock-assemblyFinishedAt>7){visibleAssemblyJob.clear();return;}
    }
    if(!shown)return;
    float w=std::min(420.f,width-40),x=width-w-24,y=82;
    Rect box{x,y,w,136};c.glow({x-20,y-16,w+40,168},hex(0,.24f));card(box);
    hit("assembly:panel",box,[]{});
    bool activeNow=processing(shown);
    auto heading=activeNow?tr("Сборка контейнера", "Building container"):shown->state=="Готово"?tr("Сборка завершена", "Build completed"):shown->state=="Отменено"?tr("Сборка отменена", "Build cancelled"):tr("Ошибка сборки", "Build failed");
    c.text(heading,x+16,y+25,14,shown->state=="Ошибка"?hex(0xff9292):text,w-105);
    if(active>1)c.text("+"+std::to_string(active-1),x+w-84,y+25,11,muted,60);
    c.text(shown->title,x+16,y+48,11,muted,w-32);
    c.text(interfaceText(app.interfaceLanguage, shown->detail),x+16,y+68,10,muted,w-32);
    Rect track{x+16,y+80,w-32,5};c.rect(track,3,edge);
    if(activeNow&&shown->percent<0){float segment=track.w*.2f,offset=(track.w-segment)*float(.5+.5*std::sin(clock*2.4));c.rect({track.x+offset,track.y,segment,track.h},3,accent);}
    else c.rect({track.x,track.y,track.w*std::clamp(float(shown->percent/100.),0.f,1.f),track.h},3,accent);
    auto id=shown->id;
    button("assembly:progress:open",{x+16,y+96,150,28},tr("Открыть загрузки", "Open downloads"),[this,id]{app.selectedJob=id;navigate(2);});
    if(activeNow)button("assembly:progress:cancel",{x+w-116,y+96,100,28},tr("Отменить", "Cancel"),[this,id]{app.selectedJob=id;app.action("cancelJob");});
}
void UI::downloadChoicePage(Rect r) {
    title(r,tr("Как сохранить выбранные медиа?", "How should the selected media be saved?"),tr("Выберите способ загрузки. Состав контейнера можно настроить перед запуском.", "Choose a download method. You can configure the container before starting."));
    float y=r.y+100,w=(r.w-20)/2;
    card({r.x,y,w,220});card({r.x+w+20,y,w,220});
    c.text(tr("Раздельные файлы", "Separate files"),r.x+24,y+43,21,text,w-48);
    c.text(tr("Все выбранные медиа в одной папке.", "All selected media in one folder."),r.x+24,y+80,12,muted,w-48);
    c.text(tr("Исходные дорожки без объединения.", "Original tracks without merging."),r.x+24,y+105,12,muted,w-48);
    button("mode:separate",{r.x+24,y+150,w-48,42},tr("Скачать раздельно", "Download separately"),[this]{app.action("enqueue");app.downloadChoice=false;},true);
    c.text(tr("Один контейнер", "One container"),r.x+w+44,y+43,21,text,w-48);
    c.text(tr("Роли, порядок, кодеки и качество дорожек.", "Track roles, order, codecs, and quality."),r.x+w+44,y+80,12,muted,w-48);
    button("mode:manual",{r.x+w+44,y+112,w-48,40},tr("Открыть конструктор", "Open builder"),[this]{cancelInput();app.openConstructor();},true);
    button("mode:back",{r.x,y+245,160,38},tr("Назад к обзору", "Back to media"),[this]{app.downloadChoice=false;});
}
bool UI::finishDraftDrag(const Hit &old) {
    bool pool=old.id.rfind("pool:",0)==0,existing=old.id.rfind("draft:",0)==0;
    if((!pool&&!existing)||std::hypot(mx-downX,my-downY)<9)return false;
    int index=std::stoi(old.id.substr(pool?5:6));
    if(index<0||index>=int(pool?app.sourcePool.size():app.draft.size()))return true;
    AssemblySource source=pool?app.sourcePool[index]:app.draft[index];
    settling=true;settleStart=GetTickCount64();settleFrom={mx-(downX-old.rect.x),my-(downY-old.rect.y),old.rect.w,pool?56.f:66.f};
    settleTarget=old.rect;settlePool=pool;settleIndex=index;
    if(existing && poolView.contains(mx,my)) {
        auto match=std::find_if(app.sourcePool.begin(),app.sourcePool.end(),[&](const auto &item){
            return item.role==source.role && item.path==source.path && item.stream==source.stream &&
                   item.media.key==source.media.key && item.media.id==source.media.id;
        });
        int poolIndex;
        if(match==app.sourcePool.end()){auto returned=source;returned.uiId=0;returned.metadataPending=false;app.sourcePool.push_back(returned);poolIndex=int(app.sourcePool.size())-1;}
        else poolIndex=int(match-app.sourcePool.begin());
        cancelInput();app.draft.erase(app.draft.begin()+index);app.updateConstructorRecommendation();
        if(app.draftSelection==index)app.draftSelection=-1;
        else if(app.draftSelection>index)--app.draftSelection;
        settlePool=true;settleIndex=poolIndex;
        settleTarget={poolView.x+7,std::clamp(my-28.f,poolView.y,poolView.y+std::max(0.f,poolView.h-56.f)),std::max(1.f,poolView.w-30.f),56};
        return true;
    }
    int role=-1,before=-1;
    for(const auto &[rect,category]:dropGroups)if(rect.contains(mx,my))role=category;
    if(draftView.contains(mx,my))before=dragBefore;
    for(const auto &[rect,row]:dropRows)if(rect.contains(mx,my)){role=app.draft[row].role;if(before<0)before=row;}
    if(role<0)return true;
    if(!accepts(source,role)){app.notify("Этот источник не содержит медиа подходящего типа");return true;}
    cancelInput();
    if(source.role!=role){
        source.role=role;source.includeAudio=false;source.primary=false;source.exportFormat=0;source.metadataEdit=false;source.probedCodec.clear();
        if(role==3){source.stream=-1;source.metadataStream=-1;source.language.clear();source.description.clear();source.artist.clear();source.date.clear();}
    }
    if(pool){
        source.uiId=++nextTileId;
        source.metadataPending=!source.path.empty() && source.role<6;
        if(source.role==0)source.includeAudio=false;
    }
    if(existing){app.draft.erase(app.draft.begin()+index);if(before>index)--before;}
    if(before<0||before>int(app.draft.size()))before=int(app.draft.size());
    app.draft.insert(app.draft.begin()+before,source);app.draftSelection=before;draftCategory=role;app.updateConstructorRecommendation();
    if(pool && source.metadataPending)app.readDroppedMetadata(source);
    settlePool=false;settleIndex=before;
    return true;
}
void UI::constructorPage(Rect r) {
    dropGroups.clear();dropRows.clear();dragBefore=-1;
    for(auto &tile:app.draft)if(!tile.uiId)tile.uiId=++nextTileId;
    if(settling && GetTickCount64()-settleStart>=300)settling=false;
    c.text(tr("Конструктор медиа", "Media builder"),r.x,r.y+28,22,text,r.w-430);
    button("build:back",{r.x+r.w-416,r.y+4,108,36},tr("К обзору", "Back"),[this]{
        cancelInput();
        const bool hasDraft=!app.draft.empty() || app.assemblyName!=app.constructorInitialName ||
            !app.assemblyMetadataTitle.empty() || !app.assemblyArtist.empty() || !app.assemblyDescription.empty() ||
            std::any_of(app.globalTags.begin(),app.globalTags.end(),[](const auto &entry){return !entry.second.empty();});
        if(!hasDraft){app.constructorOpen=false;return;}
        app.modalTitle=tr("Закрыть конструктор?", "Close the builder?");
        app.modalBody=tr("Несохранённые дорожки и изменения будут потеряны.", "Unsaved tracks and changes will be lost.");
        app.modalConfirmLabel=tr("Закрыть", "Close");
        app.modalConfirm=[this]{app.constructorOpen=false;};
    });
    button("build:auto",{r.x+r.w-298,r.y+4,146,36},tr("Автосборка", "Auto build"),[this]{
        cancelInput();
        app.autoConstruct();
        const auto now=GetTickCount64();
        unsigned added=0;
        for (auto &source : app.draft) {
            if (source.uiId) continue;
            source.uiId=++nextTileId;
            auto match=std::find_if(app.sourcePool.begin(),app.sourcePool.end(),[&](const auto &item){
                return item.role==source.role && item.path==source.path && item.stream==source.stream &&
                    item.media.key==source.media.key && item.media.id==source.media.id;
            });
            auto index=size_t(match-app.sourcePool.begin());
            if(!app.reduced && index<poolTileRects.size()){
                auto origin=poolTileRects[index];
                origin.y=std::clamp(origin.y,poolView.y,poolView.y+std::max(0.f,poolView.h-origin.h));
                autoTileFlights[source.uiId]={origin,now+std::min(added++,12u)*35};
            }
            source.metadataPending=!source.path.empty() && source.role<6;
            if (source.metadataPending) app.readDroppedMetadata(source);
        }
    });
    button("build:local",{r.x+r.w-142,r.y+4,142,36},tr("Добавить файл", "Add file"),[this]{app.action("importAssemblyFile");});
    float bottom=r.y+r.h-94,leftW=std::max(165.f,r.w*.24f),propW=std::max(210.f,r.w*.28f);
    float centerX=r.x+leftW+12,centerW=r.w-leftW-propW-24,propX=r.x+r.w-propW;
    poolView={r.x,r.y+85,leftW,bottom-r.y-97};
    card({r.x,r.y+54,leftW,bottom-r.y-66});
    c.text(app.constructorPending?tr("Читаем потоки файлов…", "Reading file streams…"):tr("Доступные медиа", "Available media"),r.x+12,r.y+76,13,text,leftW-24);
    c.push({poolView.x,poolView.y,poolView.w-16,poolView.h});
    poolTileRects.resize(app.sourcePool.size());
    int previous=-1;float py=poolView.y-poolScroll.p.value;
    // Category headings and cards are ordered together; each source keeps its own identity.
    std::vector<int> order;for(int role=0;role<7;role++)for(int i=0;i<int(app.sourcePool.size());i++)if(app.sourcePool[i].role==role)order.push_back(i);
    float total=0;
    for(int i:order){const auto &s=app.sourcePool[i];
        auto color=roleColor(s.role);
        if(previous!=s.role){c.rect({r.x+7,py,leftW-30,25},7,color.alpha(.18f));c.text((app.singleExportMode && s.role==3)?tr("Изображения", "Images"):interfaceText(app.interfaceLanguage, roles[s.role]),r.x+12,py+19,11,text,leftW-24);py+=30;total+=30;previous=s.role;}
        float poolWidth=to("pool:w:"+std::to_string(i),leftW-30,leftW-30,4.5f,.82f);
        Rect row{r.x+7,py,poolWidth,56};py+=64;total+=64;poolTileRects[i]=row;
        if(row.y+row.h<poolView.y||row.y>poolView.y+poolView.h)continue;
        bool moving=down&&capture&&capture->id=="pool:"+std::to_string(i)&&std::hypot(mx-downX,my-downY)>9;
        if(settling&&settlePool&&settleIndex==i)settleTarget=row;
        float poolHover=hover("pool:hover:"+std::to_string(i),row);
        if(!moving && !(settling&&settlePool&&settleIndex==i)){c.border(row.scale(1+.004f*poolHover),10,color.alpha(.65f),mix(panel,color,.17f));c.text(s.name,row.x+10,row.y+22,12,text,row.w-20);
        c.text(s.path.empty()?tr("Из браузера · нажмите или перетащите", "From browser · click or drag"):tr("На компьютере · нажмите или перетащите", "Local file · click or drag"),row.x+10,row.y+43,10,muted,row.w-20);}
        hit("pool:"+std::to_string(i),row,[this,i]{
            if(i<0 || i>=int(app.sourcePool.size()))return;
            auto source=app.sourcePool[i];
            source.uiId=++nextTileId;
            source.metadataPending=!source.path.empty() && source.role<6;
            if(source.role==0)source.includeAudio=false;
            app.draft.push_back(source);app.draftSelection=int(app.draft.size())-1;draftCategory=source.role;
            app.updateConstructorRecommendation();
            if(source.metadataPending)app.readDroppedMetadata(source);
        },false,std::string(tr("Добавить источник ", "Add source "))+s.name);
    }
    poolScroll.bounds(total,poolView.h);c.pop();scrollbar("scroll:pool",poolView,poolScroll);
    // The full container structure stays expanded in one scrolling tree.
    draftView={centerX,r.y+54,centerW,bottom-r.y-66};card(draftView);
    bool dragging=down&&capture&&std::hypot(mx-downX,my-downY)>9 && (capture->id.rfind("pool:",0)==0||capture->id.rfind("draft:",0)==0);
    int movingIndex=-1;AssemblySource movingSource;
    if(dragging){bool pool=capture->id.rfind("pool:",0)==0;int n=std::stoi(capture->id.substr(pool?5:6));
        if(n>=0&&n<int(pool?app.sourcePool.size():app.draft.size())){movingSource=pool?app.sourcePool[n]:app.draft[n];if(!pool)movingIndex=n;}else dragging=false;}
    c.push({draftView.x+5,draftView.y+5,draftView.w-26,draftView.h-10});
    struct FlyingTile { Rect rect; int role; std::string name; };
    std::vector<FlyingTile> flying;
    const auto now=GetTickCount64();
    std::erase_if(autoTileFlights,[now](const auto &entry){return now>=entry.second.start+420;});
    float cursor=draftView.y+8-draftScroll.p.value;
    for(int role=0;role<7;role++){
        std::vector<int> items;for(int n=0;n<int(app.draft.size());++n)if(app.draft[n].role==role&&n!=movingIndex)items.push_back(n);
        float headingY=cursor;cursor+=30;
        float sectionBottom=cursor+items.size()*76+48;
        bool over=dragging&&draftView.contains(mx,my)&&my>=headingY&&my<sectionBottom&&accepts(movingSource,role);
        int slot=over?std::clamp(int(std::floor((my-cursor+38)/76)),0,int(items.size())):0;
        if(over){dragBefore=slot<int(items.size())?items[slot]:(items.empty()?int(app.draft.size()):items.back()+1);}
        float height=30+(items.size()+(over?1:0))*76+48;
        Rect section{centerX+6,headingY,centerW-28,height};dropGroups.push_back({section.intersect(draftView),role});
        auto color=roleColor(role);
        bool compatible=dragging&&accepts(movingSource,role);
        c.rect(section.inset(2),10,color.alpha(over?.28f:compatible?.20f:.10f));
        c.text(((app.singleExportMode && role==3)?std::string(tr("Изображения", "Images")):interfaceText(app.interfaceLanguage, roles[role]))+" · "+std::to_string(items.size()),centerX+13,headingY+20,12,text,centerW-40);
        hit("tree:role:"+std::to_string(role),{centerX+8,headingY,centerW-32,28},[this,role]{cancelInput();draftCategory=role;app.draftSelection=-1;propertyScroll.p.target=0;advancedTrack=false;},false,interfaceText(app.interfaceLanguage,roles[role]));
        for(int ordinal=0;ordinal<int(items.size());++ordinal){
            int i=items[ordinal];const auto &item=app.draft[i];auto key=std::to_string(item.uiId);
            float target=cursor+(ordinal+(over&&ordinal>=slot?1:0))*76;
            float animated=to("tile:y:"+key,target,target,4.5f,.78f),tileWidth=to("tile:w:"+key,centerW-40,centerW-40,4.5f,.82f);
            Rect row{centerX+12,animated,tileWidth,66};if(settling&&!settlePool&&settleIndex==i)settleTarget=row;
            if(auto flight=autoTileFlights.find(item.uiId);flight!=autoTileFlights.end()){
                auto start=flight->second.start;
                float t=now>start?std::min(1.f,float(now-start)/420.f):0.f;
                t=1-std::pow(1-t,3.f);
                auto from=flight->second.origin;
                Rect dest=row;
                dest.y=std::clamp(dest.y,draftView.y,draftView.y+std::max(0.f,draftView.h-dest.h));
                flying.push_back({{std::lerp(from.x,dest.x,t),std::lerp(from.y,dest.y,t),std::lerp(from.w,dest.w,t),std::lerp(from.h,dest.h,t)},role,item.name});
                continue;
            }
            if(row.y+row.h<draftView.y||row.y>draftView.y+draftView.h)continue;
            if(!(settling&&!settlePool&&settleIndex==i)){
                c.border(row,10,color.alpha(app.draftSelection==i?1.f:.60f),mix(panel,color,.20f),app.draftSelection==i?3.f:1.f);
                c.text(std::to_string(ordinal+1)+"  =  "+item.name,row.x+10,row.y+24,12,text,row.w-20);
                c.text((item.primary?tr("По умолчанию · ", "Default · "):"")+item.language+(item.codec?tr(" · перекодирование", " · transcode"):tr(" · исходный кодек", " · original codec")),row.x+10,row.y+49,10,muted,row.w-20);
            }
            hit("draft:"+std::to_string(i),row,[this,i,role]{cancelInput();draftCategory=role;app.draftSelection=i;propertyScroll.p.target=0;},false,item.name);
            dropRows.push_back({row.intersect(draftView),i});
        }
        if(over){Rect gap{centerX+12,cursor+slot*76,centerW-40,66};c.border(gap,10,color.alpha(.85f),color.alpha(.23f));}
        cursor+=(items.size()+(over?1:0))*76;
        Rect vacant{centerX+12,cursor,centerW-40,38};c.border(vacant,10,color.alpha(compatible?.85f:.45f),color.alpha(.08f));
        c.text(tr("+ Перетащите сюда: ", "+ Drag here: ")+interfaceText(app.interfaceLanguage, roles[role]),vacant.x+10,vacant.y+24,11,muted,vacant.w-20);cursor+=48;
    }
    draftScroll.bounds(cursor-(draftView.y+8-draftScroll.p.value),draftView.h);
    c.pop();scrollbar("scroll:draft",draftView,draftScroll);
    card({propX,r.y+54,propW,bottom-r.y-66});
    button("build:metadata",{propX+8,r.y+59,propW-16,29},tr("Общие метаданные", "Global metadata"),[this]{
        cancelInput();app.draftSelection=-1;propertyScroll.p.target=propertyScroll.p.value=0;
    },app.draftSelection<0);
    propertyView={propX+8,r.y+90,propW-16,bottom-r.y-106};
    float propertyContent=app.draftSelection<0?260.f:(app.singleExportMode?1100.f:890.f);
    if(app.singleExportMode && app.draftSelection>=0 && app.draftSelection<int(app.draft.size())) {
        const auto &item=app.draft[app.draftSelection];
        if(item.role==5 && item.metadataEdit)propertyContent=std::max(propertyContent,820.f+float(item.metadataTree.size())*34.f);
    }
    c.push({propertyView.x,propertyView.y,propertyView.w-16,propertyView.h});float sy=propertyView.y-propertyScroll.p.value,x=propX+14,w=propW-44;
    int i=app.draftSelection;
    float metadataTailY=0;
    if(i>=0&&i<int(app.draft.size())){
        auto &s=app.draft[i];c.text(s.metadataPending?tr("Читаем метаданные…", "Reading metadata…"):(app.singleExportMode && s.role==3?tr("Изображение", "Image"):interfaceText(app.interfaceLanguage, roles[s.role])),x,sy+17,12,roleColor(s.role),w);
        if(app.singleExportMode) {
            std::vector<std::string> formats={tr("Исходный", "Original")};
            if(s.role==0)formats={tr("Исходный", "Original"),"MKV · H.264","MP4 · H.264","WebM · VP9"};
            else if(s.role==1)formats={tr("Исходный", "Original"),"MP3","M4A · AAC","FLAC"};
            else if(s.role==3)formats={tr("Исходный", "Original"),"PNG","JPG","WebP"};
            else if(s.role==2)formats={tr("Исходный", "Original"),"SRT","WebVTT","ASS","SRT · OCR"};
            else if(s.role==4)formats={tr("Исходный", "Original"),"JSON","FFmetadata"};
            else if(s.role==5)formats={tr("Исходный", "Original"),tr("Текст · TXT", "Text · TXT")};
            if(s.exportFormat<0||s.exportFormat>=int(formats.size()))s.exportFormat=0;
            c.text(tr("Формат", "Format"),x,sy+60,10,muted,w);
            choice("export:format",{x,sy+68,w,36},formats,s.exportFormat,[this,i](int n){app.draft[i].exportFormat=n;});
            if((s.role==3 || s.role<2) && s.exportFormat>0) {
                c.text(tr("Качество / сжатие", "Quality / compression"),x,sy+132,10,muted,w);
                choice("export:quality",{x,sy+140,w,36},{tr("Высокое", "High"),tr("Среднее", "Medium"),tr("Компактное", "Compact")},s.exportQuality,[this,i](int n){app.draft[i].exportQuality=n;});
            }
            if(s.role==1 && s.exportFormat>0)
                c.text(s.exportFormat==3?tr("Перекодирование в FLAC без потерь", "Lossless FLAC conversion"):tr("Перекодирование с потерей качества", "Lossy conversion"),x,sy+210,10,muted,w);
            if(s.role==2 && s.exportFormat==4) {
                c.text(tr("Путь к tesseract.exe", "Path to tesseract.exe"),x,sy+132,10,muted,w);input("export:ocr-path",{x,sy+140,w-44,34},s.ocrExecutable,tr("Полный путь к Tesseract", "Full path to Tesseract"));
                auto sourceId=s.uiId;
                button("export:ocr-browse",{x+w-38,sy+140,38,34},"…",[this,sourceId]{
                    app.work([this,sourceId]{auto path=pickFile(app.hwnd);if(path.empty())return;app.post([this,sourceId,path]{
                        if(!app.constructorOpen)return;
                        auto it=std::find_if(app.draft.begin(),app.draft.end(),[&](const auto &item){return item.uiId==sourceId;});
                        if(it!=app.draft.end())it->ocrExecutable=path;
                    });});
                });
                c.text(tr("Языки Tesseract", "Tesseract languages"),x,sy+197,10,muted,w);input("export:ocr-language",{x,sy+205,w,34},s.ocrLanguage,tr("eng или rus+eng", "eng or rus+eng"));
                c.text(tr("После распознавания проверьте текст и время", "Check the text and timing after recognition"),x,sy+280,10,muted,w);
            }
            if(s.role==5) {
                button("export:metadata-edit",{x,sy+195,w,34},s.metadataEdit?tr("Отменить редактирование", "Cancel editing"):tr("Редактировать JSON", "Edit JSON"),[this,i]{
                    auto &source=app.draft[i];
                    if(source.metadataEdit){source.metadataEdit=false;source.metadataOverrides.clear();source.metadataRemoved.clear();source.metadataPathEdits=Json::array();source.metadataTree.clear();source.metadataTreeReady=false;return;}
                    try {
                        std::string content;
                        if(!source.path.empty())content=readFile(wide(source.path),4*1024*1024);
                        else content=dataSourceContent(source.media);
                        auto data=Json::parse(content,nullptr,false);
                        if(!data.is_object())throw std::runtime_error(tr("Редактирование доступно для JSON-объекта", "Editing requires a JSON object"));
                        source.metadataEditTitle=str(data,"title");source.metadataEditArtist=str(data,"artist");source.metadataEditDescription=str(data,"description");source.metadataEdit=true;source.metadataTreeReady=false;
                    } catch(const std::exception &e){app.notify(e.what());}
                },s.metadataEdit,!s.metadataPending);
                if(s.metadataEdit && !s.metadataPending) {
                    c.text(tr("Название", "Title"),x,sy+257,10,muted,w);input("export:meta-title",{x,sy+265,w,34},s.metadataEditTitle,tr("Название", "Title"));
                    c.text(tr("Автор / исполнитель", "Author / artist"),x,sy+322,10,muted,w);input("export:meta-artist",{x,sy+330,w,34},s.metadataEditArtist,tr("Автор", "Author"));
                    c.text(tr("Описание", "Description"),x,sy+387,10,muted,w);input("export:meta-description",{x,sy+395,w,34},s.metadataEditDescription,tr("Описание", "Description"));
                    c.text(tr("Поле или путь JSON", "JSON field or path"),x,sy+465,10,muted,w);input("export:meta-key",{x,sy+473,w,34},s.metadataFieldKey,tr("Имя или /объект/поле", "Name or /object/field"));
                    c.text(tr("Значение в формате JSON", "JSON value"),x,sy+530,10,muted,w);input("export:meta-value",{x,sy+538,w,34},s.metadataFieldValue,"\"текст\", 42, true или null");
                    auto stage=[this,i](bool remove) {
                        try {
                            auto &source=app.draft[i];auto candidate=source;
                            stageMetadataField(candidate,candidate.metadataFieldKey,candidate.metadataFieldValue,remove);
                            std::string content=!source.path.empty()?readFile(wide(source.path),4*1024*1024):dataSourceContent(source.media);
                            (void)editedMetadataJson(content,candidate);
                            source.metadataOverrides=std::move(candidate.metadataOverrides);
                            source.metadataRemoved=std::move(candidate.metadataRemoved);
                            source.metadataPathEdits=std::move(candidate.metadataPathEdits);source.metadataTreeReady=false;
                            app.notify(remove?tr("Поле будет удалено", "Field will be removed"):tr("Изменение поля добавлено", "Field change added"));
                        } catch(const std::exception &e){app.notify(e.what());}
                    };
                    button("export:meta-apply",{x,sy+585,w,34},tr("Применить значение", "Apply value"),[stage]{stage(false);});
                    button("export:meta-delete",{x,sy+625,w,34},tr("Удалить поле", "Remove field"),[stage]{stage(true);});
                    c.text(tr("Изменений: ", "Changes: ")+std::to_string(s.metadataOverrides.size()+s.metadataRemoved.size()+s.metadataPathEdits.size()),x,sy+685,10,muted,w);
                    if(!s.metadataTreeReady) {
                        try {
                            std::string content=!s.path.empty()?readFile(wide(s.path),4*1024*1024):dataSourceContent(s.media);
                            s.metadataTree=makeMetadataTree(Json::parse(editedMetadataJson(content,s)));
                        } catch(...) {s.metadataTree.clear();}
                        s.metadataTreeReady=true;
                    }
                    c.text(tr("Поля JSON · выберите путь", "JSON fields · select a path"),x,sy+725,10,muted,w);
                    for(size_t n=0;n<s.metadataTree.size();++n) {
                        const auto item=s.metadataTree[n];
                        button("export:meta-tree:"+std::to_string(s.uiId)+":"+std::to_string(n),{x,sy+738+float(n)*34,w,30},item.path,[this,i,item]{
                            auto &target=app.draft[i];target.metadataFieldKey=item.path;target.metadataFieldValue=item.value;
                            propertyScroll.p.target=std::min(440.f,propertyScroll.maximum);
                            if(!item.editable)app.notify(tr("Выберите вложенное поле или введите новое значение JSON.", "Select a nested field or enter a new JSON value."));
                        });
                    }
                    if(s.metadataTree.size()>=128)c.text(tr("Показаны первые 128 полей", "Showing the first 128 fields"),x,sy+738+float(s.metadataTree.size())*34+20,10,muted,w);
                }
            }
            if((s.role==5 && !s.metadataEdit) || s.role==4) {
                auto found=metadataPreviews.find(s.uiId);
                if(found==metadataPreviews.end()) {
                    std::string preview;
                    try {if(!s.path.empty())preview=readFile(wide(s.path),4*1024*1024);else if(s.media.category=="Данные")preview=dataSourceContent(s.media);}catch(...){}
                    if(preview.size()>1200)preview.resize(1200);
                    if(metadataPreviews.size()>=64)metadataPreviews.erase(metadataPreviews.begin());
                    found=metadataPreviews.emplace(s.uiId,std::move(preview)).first;
                }
                c.text(tr("Предпросмотр данных", "Data preview"),x,sy+(s.role==5?255:215),10,muted,w);
                c.text(found->second.empty()?tr("Предпросмотр недоступен", "Preview unavailable"):found->second,x,sy+(s.role==5?282:242),10,text,w);
            }
            if(s.role<4)c.text(tr("Настройки применяются только к этому материалу", "Settings apply only to this item"),x,sy+330,10,muted,w);
        } else {
        button("property:basic",{x,sy+30,(w-8)/2,34},tr("Метаданные", "Metadata"),[this]{cancelInput();advancedTrack=false;propertyScroll.p.target=0;},!advancedTrack);
        button("property:advanced",{x+(w+8)/2,sy+30,(w-8)/2,34},tr("Кодирование", "Encoding"),[this]{cancelInput();advancedTrack=true;propertyScroll.p.target=0;},advancedTrack,s.role<2);
        if(!advancedTrack || s.role>=2){
            float y=75;
            auto field=[&](const std::string &id,const std::string &label,std::string &value,const std::string &placeholder) {
                c.text(label,x,sy+y+10,10,muted,w);
                input(id,{x,sy+y+16,w,34},value,placeholder);y+=57;
            };
            field("track:name",s.role>=4?tr("Имя источника", "Source name"):tr("Название дорожки", "Track title"),s.name,tr("Название", "Title"));
            if(s.role<3){
                field("track:language",tr("Язык дорожки · ISO 639", "Track language · ISO 639"),s.language,tr("ru, rus, Русский…", "en, eng, English…"));
                if(supportsExtendedTrackTags(app.muxOptions.container) || !s.description.empty() || !s.artist.empty() || !s.date.empty()) {
                    field("track:description",tr("Описание", "Description"),s.description,tr("Описание", "Description"));
                    field("track:artist",tr("Автор / исполнитель", "Author / artist"),s.artist,tr("Автор", "Author"));
                    field("track:date",tr("Дата выпуска (ГГГГ-ММ-ДД)", "Release date (YYYY-MM-DD)"),s.date,tr("Дата", "Date"));
                }
                if(!supportsExtendedTrackTags(app.muxOptions.container)) {
                    c.text(tr("Описание, автор и дата: MKV и WebM", "Description, author and date: MKV and WebM"),x,sy+y+12,10,muted,w);y+=24;
                }
                check("track:default",{x,sy+y,30,30},s.primary,[this,i]{auto role=app.draft[i].role;bool value=!app.draft[i].primary;for(auto &t:app.draft)if(t.role==role)t.primary=false;app.draft[i].primary=value;},tr("По умолчанию", "Default"));
                c.text(tr("По умолчанию", "Default"),x+34,sy+y+20,11,text,w-34);y+=37;
                if(s.role==2 && (s.forced || supportsForcedSubtitles(app.muxOptions.container))){
                    check("track:forced",{x,sy+y,30,30},s.forced,[this,i]{app.draft[i].forced=!app.draft[i].forced;},tr("Принудительные субтитры", "Forced subtitles"));
                    c.text(tr("Принудительные", "Forced"),x+34,sy+y+20,11,text,w-34);y+=37;
                }
                if(s.role==2){
                    c.text(tr("Кодек субтитров", "Subtitle codec"),x,sy+y+10,10,muted,w);
                    const std::vector<std::string> names={tr("Исходный", "Original"),"SRT","mov_text","WebVTT","ASS"};
                    std::vector<std::string> options;std::vector<int> ids;int selected=-1;
                    for(int n=0;n<int(names.size());++n)if(app.subtitleEncoders[n]){if(s.codec==n)selected=int(options.size());options.push_back(names[n]);ids.push_back(n);}
                    choice("track:subcodec",{x,sy+y+16,w,36},options,selected,[this,i,ids](int n){if(n>=0&&n<int(ids.size()))app.draft[i].codec=ids[n];});y+=59;
                }
                if(supportsExtendedTrackTags(app.muxOptions.container) || !s.summary.empty() || !s.synopsis.empty()){
                    field("track:summary",tr("Краткое содержание · SUMMARY", "Summary · SUMMARY"),s.summary,tr("Краткое содержание", "Summary"));
                    field("track:synopsis",tr("Синопсис · SYNOPSIS", "Synopsis · SYNOPSIS"),s.synopsis,tr("Синопсис", "Synopsis"));
                }
                if(app.muxOptions.container==1 || app.muxOptions.container==2 || !s.handlerName.empty())
                    field("track:handler",tr("Имя обработчика · handler_name", "Handler name · handler_name"),s.handlerName,tr("Имя обработчика", "Handler name"));
            } else {
                if(s.role==3) {
                    c.text(tr("Формат обложки", "Cover format"),x,sy+y+10,10,muted,w);
                    choice("track:coverFormat",{x,sy+y+16,w,36},{"PNG","JPG"},s.coverFormat,[this,i](int n){app.draft[i].coverFormat=n;});y+=59;
                    c.text(tr("Исходник: выбранное изображение", "Original: selected image"),x,sy+y+12,10,muted,w);y+=25;
                } else {c.text(s.role==6?tr("Вложение берётся из исходного файла", "Attachment comes from the source file"):tr("Главы и теги берутся из файла", "Chapters and tags come from the file"),x,sy+y+12,11,muted,w);y+=25;}
                if(!s.language.empty()||!s.description.empty()||!s.artist.empty()||!s.date.empty()) {
                    button("track:clearUnsupported",{x,sy+y,w,36},tr("Очистить неподдерживаемые теги", "Clear unsupported tags"),[this,i]{app.draft[i].language.clear();app.draft[i].description.clear();app.draft[i].artist.clear();app.draft[i].date.clear();});y+=43;
                }
            }
            metadataTailY=y;
        } else {
            c.text(tr("Кодек", "Codec"),x,sy+90,10,muted,w);
            std::vector<std::string> codecs;std::vector<int> codecIds;int selected=-1;
            const std::vector<std::string> videoNames={tr("Исходный", "Original"),"H.264 · CPU","H.265 · CPU","VP9 · CPU","AV1 · CPU","H.264 · NVIDIA","H.265 · NVIDIA","H.264 · Intel","H.265 · Intel","H.264 · AMD","H.265 · AMD"};
            const std::vector<std::string> audioNames={tr("Исходный", "Original"),"AAC","Opus","MP3","FLAC"};
            const auto &names=s.role==0?videoNames:audioNames;
            for(int n=0;n<int(names.size());++n)if((s.role==0&&app.videoEncoders[n])||(s.role==1&&app.audioEncoders[n])){if(s.codec==n)selected=int(codecs.size());codecs.push_back(names[n]);codecIds.push_back(n);}
            choice("track:codec",{x,sy+98,w,36},codecs,selected,[this,i,codecIds](int n){if(n>=0&&n<int(codecIds.size()))app.draft[i].codec=codecIds[n];});
            if(s.role==1)c.text(s.codec==0?tr("Без перекодирования", "No transcoding"):s.codec==4?tr("FLAC: без потерь", "FLAC: lossless"):tr("Перекодирование с потерей качества", "Lossy conversion"),x,sy+135,10,muted,w);
            c.text(tr("Профиль качества", "Quality profile"),x,sy+155,10,muted,w);choice("track:quality",{x,sy+163,w,36},{tr("Высокое", "High"),tr("Сбалансированное", "Balanced"),tr("Компактное", "Compact")},s.quality,[this,i](int n){app.draft[i].quality=n;});
            c.text(tr("Битрейт, кбит/с (необязательно)", "Bitrate, kbps (optional)"),x,sy+220,10,muted,w);input("track:bitrate",{x,sy+228,w,34},s.bitrate,tr("Автоматически", "Automatic"));
            if(s.role==0){
                c.text(tr("Максимальный битрейт, кбит/с", "Maximum bitrate, kbps"),x,sy+285,10,muted,w);input("track:maxrate",{x,sy+293,w,34},s.maxrate,tr("Автоматически", "Automatic"));
                c.text(tr("Буфер, кбит", "Buffer, kbit"),x,sy+350,10,muted,w);input("track:bufsize",{x,sy+358,w,34},s.bufsize,tr("Автоматически", "Automatic"));
                c.text(tr("Частота кадров", "Frame rate"),x,sy+415,10,muted,w);input("track:framerate",{x,sy+423,w,34},s.frameRate,tr("Исходная", "Original"));
                c.text(tr("Ширина, пиксели", "Width, pixels"),x,sy+480,10,muted,w);input("track:width",{x,sy+488,w,34},s.width,tr("Исходная", "Original"));
            } else {c.text(tr("Частота дискретизации, Гц", "Sample rate, Hz"),x,sy+285,10,muted,w);input("track:samplerate",{x,sy+293,w,34},s.sampleRate,tr("Исходная", "Original"));}
            c.text(app.videoEncodersReady?tr("Показаны доступные кодировщики", "Available encoders shown"):tr("Проверяем кодировщики…", "Checking encoders…"),x,sy+558,10,muted,w);
        }
        }
        auto orderControls=[this,i,x,w,sy](float y) {
            button("track:up",{x,sy+y,(w-8)/2,34},tr("Выше ↑", "Move up ↑"),[this,i]{cancelInput();for(int n=i-1;n>=0;n--)if(app.draft[n].role==app.draft[i].role){std::swap(app.draft[n],app.draft[i]);app.draftSelection=n;break;}});
            button("track:down",{x+(w+8)/2,sy+y,(w-8)/2,34},tr("Ниже ↓", "Move down ↓"),[this,i]{cancelInput();for(int n=i+1;n<int(app.draft.size());n++)if(app.draft[n].role==app.draft[i].role){std::swap(app.draft[n],app.draft[i]);app.draftSelection=n;break;}});
            button("track:remove",{x,sy+y+45,w,36},tr("Убрать", "Remove"),[this,i]{cancelInput();app.draft.erase(app.draft.begin()+i);app.draftSelection=-1;app.updateConstructorRecommendation();});
        };
        float removeY=app.singleExportMode?355.f:790.f;
        if(app.singleExportMode && s.role==5 && s.metadataEdit)removeY=std::max(removeY,780.f+float(s.metadataTree.size())*34.f);
        if(!app.singleExportMode && (!advancedTrack || s.role>=2)) {
            float y=metadataTailY+10;
            if(s.role==5 && s.metadataEdit && !s.metadataPending) {
                c.text(tr("Название", "Title"),x,sy+y+15,10,muted,w);input("source:title",{x,sy+y+22,w,34},s.metadataEditTitle,tr("Название", "Title"));y+=65;
                c.text(tr("Автор / исполнитель", "Author / artist"),x,sy+y+15,10,muted,w);input("source:artist",{x,sy+y+22,w,34},s.metadataEditArtist,tr("Автор", "Author"));y+=65;
                c.text(tr("Описание", "Description"),x,sy+y+15,10,muted,w);input("source:description",{x,sy+y+22,w,34},s.metadataEditDescription,tr("Описание", "Description"));y+=65;
            }
            if(s.role<3 || (s.role==5 && s.metadataEdit)) {
                c.text(s.role==5?tr("Дополнительные теги источника", "Additional source tags"):tr("Дополнительные теги дорожки", "Additional track tags"),x,sy+y,11,accent,w); y+=12;
                for(auto &[key,value]:s.extraTags) {
                    c.text(key,x,sy+y+15,10,muted,w);
                    input("extra:track:"+std::to_string(s.uiId)+":"+key,{x,sy+y+22,w,34},value,tr("Значение", "Value"));y+=65;
                }
                input("extra:track-name",{x,sy+y+10,w-80,34},s.extraTagName,tr("Имя тега", "Tag name"));
                button("extra:track-add",{x+w-74,sy+y+10,74,34},tr("Добавить", "Add"),[this,i]{
                    auto &source=app.draft[i];
                    if(!metadataKeyAllowed(source.extraTagName)){app.notify(tr("Введите допустимое имя нового тега", "Enter a valid tag name"));return;}
                    for(const auto &[key,value]:source.extraTags)if(_stricmp(key.c_str(),source.extraTagName.c_str())==0){app.notify(tr("Такой тег уже существует", "That tag already exists"));return;}
                    source.extraTags[source.extraTagName]="";source.extraTagName.clear();cancelInput();
                });y+=65;
            }
            if(s.role==4 && s.chaptersReady) {
                c.text(tr("Главы · время в секундах", "Chapters · time in seconds"),x,sy+y+15,10,accent,w);y+=25;
                for(size_t n=0;n<s.chapterEdits.size();++n) {
                    auto &chapter=s.chapterEdits[n];auto id=std::to_string(s.uiId)+":"+std::to_string(n);
                    c.text(tr("Глава ", "Chapter ")+std::to_string(n+1),x,sy+y+13,10,muted,w);
                    input("chapter:title:"+id,{x,sy+y+20,w,34},chapter.title,tr("Название", "Title"));
                    c.text(tr("Начало, с", "Start, s"),x,sy+y+76,10,muted,w);
                    input("chapter:start:"+id,{x,sy+y+83,w,34},chapter.start,"0");
                    c.text(tr("Конец, с", "End, s"),x,sy+y+139,10,muted,w);
                    input("chapter:end:"+id,{x,sy+y+146,w,34},chapter.end,"0");y+=190;
                }
            }
            c.text(tr("Свойства исходного файла · только чтение", "Source file properties · read only"),x,sy+y+15,10,accent,w);y+=27;
            auto fields=infoFor(s,false);
            if(fields.empty()) { c.text(s.metadataPending?tr("Читаем сведения…", "Reading information…"):tr("MediaInfo или сведения файла недоступны", "MediaInfo or file information unavailable"),x,sy+y+15,10,muted,w); y+=35; }
            for(const auto &field:fields) {
                c.text(interfaceText(app.interfaceLanguage, field.name),x,sy+y+13,10,muted,w);
                c.text(field.value,x,sy+y+32,11,text,w);y+=51;
            }
            orderControls(y+10);
            propertyContent=y+105;
        } else if(app.singleExportMode) {
            button("track:remove",{x,sy+removeY,w,36},tr("Убрать", "Remove"),[this,i]{cancelInput();app.draft.erase(app.draft.begin()+i);app.draftSelection=-1;app.updateConstructorRecommendation();});
            propertyContent=removeY+55;
        } else orderControls(745.f);
    } else {
        c.text(tr("Метаданные итогового контейнера", "Output container metadata"),x,sy+23,12,accent,w);
        c.text(tr("Название", "Title"),x,sy+55,10,muted,w);
        input("build:metadata-title",{x,sy+65,w,34},app.assemblyMetadataTitle,app.assemblyName);
        c.text(tr("Автор / исполнитель", "Author / artist"),x,sy+112,10,muted,w);
        input("build:artist",{x,sy+122,w,34},app.assemblyArtist,tr("Автор", "Author"));
        c.text(tr("Описание", "Description"),x,sy+169,10,muted,w);
        input("build:description",{x,sy+179,w,34},app.assemblyDescription,tr("Описание", "Description"));
        float y=230;
        c.text(tr("Дополнительные общие теги", "Additional global tags"),x,sy+y,11,accent,w);y+=12;
        for(auto &[key,value]:app.globalTags) {
            c.text(key,x,sy+y+15,10,muted,w);
            input("extra:global:"+key,{x,sy+y+22,w,34},value,tr("Значение", "Value"));y+=65;
        }
        input("extra:global-name",{x,sy+y+10,w-80,34},app.globalTagName,tr("Имя тега", "Tag name"));
        button("extra:global-add",{x+w-74,sy+y+10,74,34},tr("Добавить", "Add"),[this]{
            if(!metadataKeyAllowed(app.globalTagName)){app.notify(tr("Введите допустимое имя нового тега", "Enter a valid tag name"));return;}
            for(const auto &[key,value]:app.globalTags)if(_stricmp(key.c_str(),app.globalTagName.c_str())==0){app.notify(tr("Такой тег уже существует", "That tag already exists"));return;}
            app.globalTags[app.globalTagName]="";app.globalTagName.clear();cancelInput();
        });y+=70;
        c.text(tr("Свойства исходного файла · только чтение", "Source file properties · read only"),x,sy+y+15,10,accent,w);y+=27;
        for(const auto &source:app.draft)if(!source.mediaInfoFields.empty()) {
            for(const auto &field:infoFor(source,true)) {
                c.text(interfaceText(app.interfaceLanguage, field.name),x,sy+y+13,10,muted,w);
                c.text(field.value,x,sy+y+32,11,text,w);y+=51;
            }
            break;
        }
        propertyContent=y+20;
    }
    c.pop();propertyScroll.bounds(propertyContent,propertyView.h);scrollbar("scroll:property",propertyView,propertyScroll);
    std::string compatibility;
    if(app.singleExportMode) {
        for(const auto &source:app.draft) {
            int role=source.role,format=source.exportFormat;bool available=true;
            if(format<=0)continue;
            if(role==2 && format!=4 && isBitmapSubtitleCodec(source.probedCodec)){compatibility=tr("Графические субтитры: выберите исходный формат или распознавание текста.", "Bitmap subtitles: choose the original format or text recognition.");break;}
            if(role==0 && format<=3)available=app.videoEncoders[format==3?3:1];
            else if(role==1 && format<=3)available=app.audioEncoders[format==1?3:format==2?1:4];
            else if(role==2 && format<=3)available=app.subtitleEncoders[format==1?1:format==2?3:4];
            else if(role==2 && format==4)available=!source.ocrExecutable.empty() && fs::path(wide(source.ocrExecutable)).is_absolute();
            else if(role==3 && format<=3)available=app.imageEncoders[format];
            if(!available){compatibility=role==2 && format==4?tr("Укажите полный путь к установленному Tesseract.", "Enter the full path to the installed Tesseract."):app.videoEncodersReady?tr("Преобразователь недоступен; выберите исходный или другой формат.", "Converter unavailable; choose the original or another format."):tr("Проверяем доступные преобразователи…", "Checking available converters…");break;}
        }
    }
    else if(!app.singleExportMode && !app.draft.empty()) {
        bool playable=false,cover=false,badAudio=false;int audio=0;
        for(const auto &source:app.draft) {
            playable|=source.role<2;cover|=source.role==3;audio+=source.role==1;
            if(app.muxOptions.container>=4 && (source.role==0 || source.role==2))badAudio=true;
        }
        if(!playable)compatibility=tr("Добавьте видео или аудио либо выберите отдельное сохранение.", "Add video or audio, or choose separate export.");
        else if(cover && app.muxOptions.container==3)compatibility=tr("Обложка недоступна в WebM; выберите MKV или MP4.", "Cover art is unavailable in WebM; choose MKV or MP4.");
        else if(app.muxOptions.container>=4 && (badAudio || audio!=1))compatibility=tr("Для аудиоформата нужна одна аудиодорожка без видео и субтитров.", "Audio output requires one audio track without video or subtitles.");
        if(compatibility.empty())for(const auto &source:app.draft) {
            auto conflict=assemblyCodecConflict(app.muxOptions.container,source);
            if(conflict.empty() && source.codec==0 && !source.probedCodec.empty())conflict=assemblyProbedCodecConflict(app.muxOptions.container,source.role,source.probedCodec);
            if(conflict.empty() && source.codec==0 && source.role<2)
                conflict=assemblyCopiedCodecConflict(app.muxOptions.container,source.role,source.role==0?source.media.vcodec:source.media.acodec);
            if(!conflict.empty()){compatibility=conflict;break;}
        }
    }
    if(!compatibility.empty())c.text(interfaceText(app.interfaceLanguage, compatibility),centerX,bottom-5,10,hex(0xffae70),centerW);
    input("build:name",{r.x,bottom,r.w*.29f,36},app.assemblyName,tr("Имя итогового файла", "Output file name"));
    choice("build:mode",{r.x+r.w*.29f+8,bottom,126,36},{tr("Сборка", "Build"),tr("Отдельно", "Separate")},app.singleExportMode?1:0,[this](int n){app.singleExportMode=n==1;app.constructorModeManual=true;});
    if(!app.singleExportMode)choice("build:container",{r.x+r.w*.29f+142,bottom,110,36},{"MKV","MP4","MOV","WebM","MP3","M4A","FLAC"},app.muxOptions.container,[this](int n){app.muxOptions.container=n;app.constructorFormatManual=true;});
    button("build:start",{r.x+r.w-202,bottom,202,36},app.singleExportMode?tr("Сохранить файл", "Save file"):tr("Скачать и собрать", "Download and build"),[this]{cancelInput();app.action("enqueueAssembly");},true,!app.draft.empty());
    input("build:destination",{r.x,bottom+46,r.w-108,34},app.destination,tr("Папка сохранения", "Destination folder"));
    button("build:folder",{r.x+r.w-98,bottom+46,98,34},tr("Папка", "Folder"),[this]{app.action("folder");});
    if(down&&capture&&std::hypot(mx-downX,my-downY)>9){
        const AssemblySource *source=nullptr; bool pool=false;
        if(capture->id.rfind("pool:",0)==0){int n=std::stoi(capture->id.substr(5));if(n>=0&&n<int(app.sourcePool.size())){source=&app.sourcePool[n];pool=true;}}
        if(capture->id.rfind("draft:",0)==0){int n=std::stoi(capture->id.substr(6));if(n>=0&&n<int(app.draft.size()))source=&app.draft[n];}
        if(source){
            Rect moving{mx-(downX-capture->rect.x),my-(downY-capture->rect.y),capture->rect.w,pool?56.f:66.f};
            auto color=roleColor(source->role);
            c.glow({moving.x-8,moving.y-6,moving.w+16,moving.h+16},color.alpha(.16f));
            c.border(moving,10,color,mix(panel,color,.22f),2);
            c.text(source->name,moving.x+10,moving.y+22,12,text,moving.w-20);
            auto detail=pool?(source->path.empty()?tr("Из браузера · перетащите →", "From browser · drag →"):tr("На компьютере · перетащите →", "Local file · drag →")):
                (source->primary?tr("По умолчанию · ", "Default · "):"")+source->language+(source->codec?tr(" · перекодирование", " · transcode"):tr(" · исходный кодек", " · original codec"));
            c.text(detail,moving.x+10,moving.y+(pool?43:49),10,muted,moving.w-20);
        }
    }
    for(const auto &tile:flying){
        auto color=roleColor(tile.role);
        c.border(tile.rect,10,color,mix(panel,color,.25f),2);
        c.text(tile.name,tile.rect.x+10,tile.rect.y+28,12,text,tile.rect.w-20);
    }
    if(settling){
        const AssemblySource *source=nullptr;
        if(settlePool&&settleIndex>=0&&settleIndex<int(app.sourcePool.size()))source=&app.sourcePool[settleIndex];
        if(!settlePool&&settleIndex>=0&&settleIndex<int(app.draft.size()))source=&app.draft[settleIndex];
        if(source){float t=std::clamp(float(GetTickCount64()-settleStart)/300.f,0.f,1.f);t=1-std::pow(1-t,3.f);
            auto blend=[t](float a,float b){return a+(b-a)*t;};Rect box{blend(settleFrom.x,settleTarget.x),blend(settleFrom.y,settleTarget.y),blend(settleFrom.w,settleTarget.w),blend(settleFrom.h,settleTarget.h)};
            auto color=roleColor(source->role);
            c.border(box,10,color.alpha(1-.4f*t),mix(panel,color,.20f),2-t);c.text(source->name,box.x+10,box.y+22,12,text,box.w-20);
            c.text(settlePool?(source->path.empty()?tr("Из браузера", "From browser"):tr("На компьютере", "Local file")):(source->language+(source->codec?tr(" · перекодирование", " · transcode"):tr(" · исходный кодек", " · original codec"))),box.x+10,box.y+std::min(box.h-10.f,49.f),10,muted,box.w-20);
        }
    }
}
}
