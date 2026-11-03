#include "app.hpp"
#include "language_options.hpp"
namespace ss {
static const std::vector<std::string> roles={"Видео","Аудио","Субтитры","Обложка","Главы","Метаданные"};
static Ink roleColor(int role) {
    static constexpr unsigned colors[] = {0x5599f4, 0x39bc8b, 0xc07ff2, 0xee9953, 0xd7b84e, 0x45b6c8};
    return hex(colors[role]);
}
static bool accepts(const AssemblySource &s,int role) {
    return s.role==role||(s.role==0&&s.includeAudio&&role==1&&s.stream<0);
}
void UI::assemblyProgressPanel() {
    auto processing=[](const std::shared_ptr<Job> &j){return j->state=="Сборка контейнера"||j->state=="Проверка результата"||j->state=="Объединение";};
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
    auto heading=activeNow?"Сборка контейнера":shown->state=="Готово"?"Сборка завершена":shown->state=="Отменено"?"Сборка отменена":"Ошибка сборки";
    c.text(heading,x+16,y+25,14,shown->state=="Ошибка"?hex(0xff9292):text,w-105);
    if(active>1)c.text("+"+std::to_string(active-1),x+w-84,y+25,11,muted,60);
    c.text(shown->title,x+16,y+48,11,muted,w-32);
    c.text(shown->detail,x+16,y+68,10,muted,w-32);
    Rect track{x+16,y+80,w-32,5};c.rect(track,3,edge);
    if(activeNow&&shown->percent<0){float segment=track.w*.2f,offset=(track.w-segment)*float(.5+.5*std::sin(clock*2.4));c.rect({track.x+offset,track.y,segment,track.h},3,accent);}
    else c.rect({track.x,track.y,track.w*std::clamp(float(shown->percent/100.),0.f,1.f),track.h},3,accent);
    auto id=shown->id;
    button("assembly:progress:open",{x+16,y+96,150,28},"Открыть загрузки",[this,id]{app.selectedJob=id;navigate(2);});
    if(activeNow)button("assembly:progress:cancel",{x+w-116,y+96,100,28},"Отменить",[this,id]{app.selectedJob=id;app.action("cancelJob");});
}
void UI::downloadChoicePage(Rect r) {
    title(r,"Как сохранить выбранные медиа?","Выберите способ загрузки. Состав контейнера можно настроить перед запуском.");
    float y=r.y+100,w=(r.w-20)/2;
    card({r.x,y,w,220});card({r.x+w+20,y,w,220});
    c.text("Раздельные файлы",r.x+24,y+43,21,text,w-48);
    c.text("Все выбранные медиа в одной папке.",r.x+24,y+80,12,muted,w-48);
    c.text("Исходные дорожки без объединения.",r.x+24,y+105,12,muted,w-48);
    button("mode:separate",{r.x+24,y+150,w-48,42},"Скачать раздельно",[this]{app.action("enqueue");app.downloadChoice=false;},true);
    c.text("Один контейнер",r.x+w+44,y+43,21,text,w-48);
    c.text("Роли, порядок, кодеки и качество дорожек.",r.x+w+44,y+80,12,muted,w-48);
    button("mode:manual",{r.x+w+44,y+112,w-48,40},"Открыть конструктор",[this]{cancelInput();app.openConstructor();},true);
    button("mode:back",{r.x,y+245,160,38},"Назад к обзору",[this]{app.downloadChoice=false;});
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
        cancelInput();app.draft.erase(app.draft.begin()+index);
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
    if(source.role!=role){source.role=role;source.includeAudio=false;source.primary=false;}
    if(pool){
        source.uiId=++nextTileId;
        source.metadataPending=!source.path.empty() && source.role<3;
        if(source.role==0)source.includeAudio=false;
    }
    if(existing){app.draft.erase(app.draft.begin()+index);if(before>index)--before;}
    if(before<0||before>int(app.draft.size()))before=int(app.draft.size());
    app.draft.insert(app.draft.begin()+before,source);app.draftSelection=before;draftCategory=role;
    if(pool && source.metadataPending)app.readDroppedMetadata(source);
    settlePool=false;settleIndex=before;
    return true;
}
void UI::constructorPage(Rect r) {
    dropGroups.clear();dropRows.clear();dragBefore=-1;
    for(auto &tile:app.draft)if(!tile.uiId)tile.uiId=++nextTileId;
    if(settling && GetTickCount64()-settleStart>=300)settling=false;
    c.text("Конструктор контейнера",r.x,r.y+28,22,text,r.w-430);
    button("build:back",{r.x+r.w-416,r.y+4,108,36},"К обзору",[this]{cancelInput();app.constructorOpen=false;});
    button("build:auto",{r.x+r.w-298,r.y+4,146,36},"Автосборка",[this]{
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
            source.metadataPending=!source.path.empty() && source.role<3;
            if (source.metadataPending) app.readDroppedMetadata(source);
        }
    });
    button("build:local",{r.x+r.w-142,r.y+4,142,36},"Добавить файл",[this]{app.action("importAssemblyFile");});
    float bottom=r.y+r.h-94,leftW=std::max(165.f,r.w*.24f),propW=std::max(210.f,r.w*.28f);
    float centerX=r.x+leftW+12,centerW=r.w-leftW-propW-24,propX=r.x+r.w-propW;
    poolView={r.x,r.y+85,leftW,bottom-r.y-97};
    card({r.x,r.y+54,leftW,bottom-r.y-66});
    c.text("Доступные медиа",r.x+12,r.y+76,13,text,leftW-24);
    c.push({poolView.x,poolView.y,poolView.w-16,poolView.h});
    poolTileRects.resize(app.sourcePool.size());
    int previous=-1;float py=poolView.y-poolScroll.p.value;
    // Category headings and cards are ordered together; each source keeps its own identity.
    std::vector<int> order;for(int role=0;role<6;role++)for(int i=0;i<int(app.sourcePool.size());i++)if(app.sourcePool[i].role==role)order.push_back(i);
    float total=0;
    for(int i:order){const auto &s=app.sourcePool[i];
        auto color=roleColor(s.role);
        if(previous!=s.role){c.rect({r.x+7,py,leftW-30,25},7,color.alpha(.18f));c.text(roles[s.role],r.x+12,py+19,11,text,leftW-24);py+=30;total+=30;previous=s.role;}
        float poolWidth=to("pool:w:"+std::to_string(i),leftW-30,leftW-30,4.5f,.82f);
        Rect row{r.x+7,py,poolWidth,56};py+=64;total+=64;poolTileRects[i]=row;
        if(row.y+row.h<poolView.y||row.y>poolView.y+poolView.h)continue;
        bool moving=down&&capture&&capture->id=="pool:"+std::to_string(i)&&std::hypot(mx-downX,my-downY)>9;
        if(settling&&settlePool&&settleIndex==i)settleTarget=row;
        float poolHover=hover("pool:hover:"+std::to_string(i),row);
        if(!moving && !(settling&&settlePool&&settleIndex==i)){c.border(row.scale(1+.004f*poolHover),10,color.alpha(.65f),mix(panel,color,.17f));c.text(s.name,row.x+10,row.y+22,12,text,row.w-20);
        c.text(s.path.empty()?"Из браузера · перетащите →":"На компьютере · перетащите →",row.x+10,row.y+43,10,muted,row.w-20);}
        hit("pool:"+std::to_string(i),row,[]{});
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
    for(int role=0;role<6;role++){
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
        c.text(roles[role]+" · "+std::to_string(items.size()),centerX+13,headingY+20,12,text,centerW-40);
        hit("tree:role:"+std::to_string(role),{centerX+8,headingY,centerW-32,28},[this,role]{cancelInput();draftCategory=role;app.draftSelection=-1;propertyScroll.p.target=0;advancedTrack=false;});
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
                c.text((item.primary?"По умолчанию · ":"")+item.language+(item.codec?" · перекодирование":" · исходный кодек"),row.x+10,row.y+49,10,muted,row.w-20);
            }
            hit("draft:"+std::to_string(i),row,[this,i,role]{cancelInput();draftCategory=role;app.draftSelection=i;propertyScroll.p.target=0;});
            dropRows.push_back({row.intersect(draftView),i});
        }
        if(over){Rect gap{centerX+12,cursor+slot*76,centerW-40,66};c.border(gap,10,color.alpha(.85f),color.alpha(.23f));}
        cursor+=(items.size()+(over?1:0))*76;
        Rect vacant{centerX+12,cursor,centerW-40,38};c.border(vacant,10,color.alpha(compatible?.85f:.45f),color.alpha(.08f));
        c.text("+ Перетащите сюда: "+roles[role],vacant.x+10,vacant.y+24,11,muted,vacant.w-20);cursor+=48;
    }
    draftScroll.bounds(cursor-(draftView.y+8-draftScroll.p.value),draftView.h);
    c.pop();scrollbar("scroll:draft",draftView,draftScroll);
    card({propX,r.y+54,propW,bottom-r.y-66});
    button("build:metadata",{propX+8,r.y+59,propW-16,29},"Общие метаданные",[this]{
        cancelInput();app.draftSelection=-1;propertyScroll.p.target=propertyScroll.p.value=0;
    },app.draftSelection<0);
    propertyView={propX+8,r.y+90,propW-16,bottom-r.y-106};propertyScroll.bounds(app.draftSelection<0?260:855,propertyView.h);
    c.push({propertyView.x,propertyView.y,propertyView.w-16,propertyView.h});float sy=propertyView.y-propertyScroll.p.value,x=propX+14,w=propW-44;
    int i=app.draftSelection;
    if(i>=0&&i<int(app.draft.size())){
        auto &s=app.draft[i];c.text(s.metadataPending?"Читаем метаданные…":roles[s.role],x,sy+17,12,roleColor(s.role),w);
        button("property:basic",{x,sy+30,(w-8)/2,34},"Метаданные",[this]{cancelInput();advancedTrack=false;propertyScroll.p.target=0;},!advancedTrack);
        button("property:advanced",{x+(w+8)/2,sy+30,(w-8)/2,34},"Кодирование",[this]{cancelInput();advancedTrack=true;propertyScroll.p.target=0;},advancedTrack,s.role<2);
        if(!advancedTrack || s.role>=2){
            c.text(s.role>=4?"Имя источника":"Название дорожки",x,sy+90,10,muted,w);input("track:name",{x,sy+98,w,34},s.name,"Название");
            if(s.role<3){
            c.text("Язык дорожки · ISO 639",x,sy+155,10,muted,w);
            input("track:language",{x,sy+163,w,34},s.language,"ru, rus, Русский…");
            if (supportsExtendedTrackTags(app.muxOptions.container) || !s.description.empty() || !s.artist.empty() || !s.date.empty()) {
                c.text("Описание",x,sy+220,10,muted,w);input("track:description",{x,sy+228,w,34},s.description,"Описание");
                c.text("Автор / исполнитель",x,sy+285,10,muted,w);input("track:artist",{x,sy+293,w,34},s.artist,"Автор");
                c.text("Дата выпуска (ГГГГ-ММ-ДД)",x,sy+350,10,muted,w);input("track:date",{x,sy+358,w,34},s.date,"Дата");
            }
            if (!supportsExtendedTrackTags(app.muxOptions.container))
                c.text("Описание, автор и дата: MKV и WebM",x,sy+390,10,muted,w);
            } else {
                c.text(s.role==3?"Для обложки сохраняется название":"Главы и теги берутся из файла",x,sy+170,11,muted,w);
                if(!s.language.empty()||!s.description.empty()||!s.artist.empty()||!s.date.empty())
                    button("track:clearUnsupported",{x,sy+200,w,36},"Очистить неподдерживаемые теги",[this,i]{app.draft[i].language.clear();app.draft[i].description.clear();app.draft[i].artist.clear();app.draft[i].date.clear();});
            }
            if(s.role<3){
                check("track:default",{x,sy+404,30,30},s.primary,[this,i]{auto role=app.draft[i].role;bool value=!app.draft[i].primary;for(auto &t:app.draft)if(t.role==role)t.primary=false;app.draft[i].primary=value;});
                c.text("По умолчанию",x+34,sy+424,11,text,w-34);
                if(s.role==2 && (s.forced || supportsForcedSubtitles(app.muxOptions.container))){check("track:forced",{x,sy+440,30,30},s.forced,[this,i]{app.draft[i].forced=!app.draft[i].forced;});c.text("Принудительные",x+34,sy+460,11,text,w-34);}
                if(s.role==2){
                    c.text("Кодек субтитров",x,sy+501,10,muted,w);
                    const std::vector<std::string> names={"Исходный","SRT","mov_text","WebVTT","ASS"};
                    std::vector<std::string> options;std::vector<int> ids;int selected=-1;
                    for(int n=0;n<int(names.size());++n)if(app.subtitleEncoders[n]){if(s.codec==n)selected=int(options.size());options.push_back(names[n]);ids.push_back(n);}
                    choice("track:subcodec",{x,sy+511,w,36},options,selected,[this,i,ids](int n){if(n>=0&&n<int(ids.size()))app.draft[i].codec=ids[n];});
                }
            }
        } else {
            c.text("Кодек",x,sy+90,10,muted,w);
            std::vector<std::string> codecs;std::vector<int> codecIds;int selected=-1;
            const std::vector<std::string> videoNames={"Исходный","H.264 · CPU","H.265 · CPU","VP9 · CPU","AV1 · CPU","H.264 · NVIDIA","H.265 · NVIDIA","H.264 · Intel","H.265 · Intel","H.264 · AMD","H.265 · AMD"};
            const std::vector<std::string> audioNames={"Исходный","AAC","Opus","MP3","FLAC"};
            const auto &names=s.role==0?videoNames:audioNames;
            for(int n=0;n<int(names.size());++n)if((s.role==0&&app.videoEncoders[n])||(s.role==1&&app.audioEncoders[n])){if(s.codec==n)selected=int(codecs.size());codecs.push_back(names[n]);codecIds.push_back(n);}
            choice("track:codec",{x,sy+98,w,36},codecs,selected,[this,i,codecIds](int n){if(n>=0&&n<int(codecIds.size()))app.draft[i].codec=codecIds[n];});
            c.text("Профиль качества",x,sy+155,10,muted,w);choice("track:quality",{x,sy+163,w,36},{"Высокое","Сбалансированное","Компактное"},s.quality,[this,i](int n){app.draft[i].quality=n;});
            c.text("Битрейт, кбит/с (необязательно)",x,sy+220,10,muted,w);input("track:bitrate",{x,sy+228,w,34},s.bitrate,"Автоматически");
            if(s.role==0){
                c.text("Максимальный битрейт, кбит/с",x,sy+285,10,muted,w);input("track:maxrate",{x,sy+293,w,34},s.maxrate,"Автоматически");
                c.text("Буфер, кбит",x,sy+350,10,muted,w);input("track:bufsize",{x,sy+358,w,34},s.bufsize,"Автоматически");
                c.text("Частота кадров",x,sy+415,10,muted,w);input("track:framerate",{x,sy+423,w,34},s.frameRate,"Исходная");
                c.text("Ширина, пиксели",x,sy+480,10,muted,w);input("track:width",{x,sy+488,w,34},s.width,"Исходная");
            } else {c.text("Частота дискретизации, Гц",x,sy+285,10,muted,w);input("track:samplerate",{x,sy+293,w,34},s.sampleRate,"Исходная");}
            c.text(app.videoEncodersReady?"Показаны доступные кодировщики":"Проверяем кодировщики…",x,sy+558,10,muted,w);
        }
        if((!advancedTrack || s.role>=2) && s.role<3){
            if(supportsExtendedTrackTags(app.muxOptions.container) || !s.summary.empty() || !s.synopsis.empty()){
                c.text("Краткое содержание · SUMMARY",x,sy+566,10,muted,w);
                input("track:summary",{x,sy+574,w,34},s.summary,"Краткое содержание");
                c.text("Синопсис · SYNOPSIS",x,sy+626,10,muted,w);
                input("track:synopsis",{x,sy+634,w,34},s.synopsis,"Синопсис");
            }
            if(app.muxOptions.container==1 || app.muxOptions.container==2 || !s.handlerName.empty()){
                c.text("Имя обработчика · handler_name",x,sy+686,10,muted,w);
                input("track:handler",{x,sy+694,w,34},s.handlerName,"Имя обработчика");
            }
        }
        button("track:up",{x,sy+745,(w-8)/2,34},"Выше ↑",[this,i]{cancelInput();for(int n=i-1;n>=0;n--)if(app.draft[n].role==app.draft[i].role){std::swap(app.draft[n],app.draft[i]);app.draftSelection=n;break;}});
        button("track:down",{x+(w+8)/2,sy+745,(w-8)/2,34},"Ниже ↓",[this,i]{cancelInput();for(int n=i+1;n<int(app.draft.size());n++)if(app.draft[n].role==app.draft[i].role){std::swap(app.draft[n],app.draft[i]);app.draftSelection=n;break;}});
        button("track:remove",{x,sy+790,w,36},"Убрать из контейнера",[this,i]{cancelInput();app.draft.erase(app.draft.begin()+i);app.draftSelection=-1;});
    } else {
        c.text("Метаданные итогового контейнера",x,sy+23,12,accent,w);
        c.text("Название",x,sy+55,10,muted,w);
        input("build:metadata-title",{x,sy+65,w,34},app.assemblyMetadataTitle,app.assemblyName);
        c.text("Автор / исполнитель",x,sy+123,10,muted,w);
        input("build:artist",{x,sy+133,w,34},app.assemblyArtist,"Автор");
        c.text("Описание",x,sy+191,10,muted,w);
        input("build:description",{x,sy+201,w,34},app.assemblyDescription,"Описание");
    }
    c.pop();scrollbar("scroll:property",propertyView,propertyScroll);
    input("build:name",{r.x,bottom,r.w*.36f,36},app.assemblyName,"Имя итогового файла");
    choice("build:container",{r.x+r.w*.36f+10,bottom,120,36},{"MKV","MP4","MOV","WebM"},app.muxOptions.container,[this](int n){app.muxOptions.container=n;});
    button("build:start",{r.x+r.w-202,bottom,202,36},"Скачать и собрать",[this]{cancelInput();app.action("enqueueAssembly");},true,!app.draft.empty());
    input("build:destination",{r.x,bottom+46,r.w-108,34},app.destination,"Папка сохранения");
    button("build:folder",{r.x+r.w-98,bottom+46,98,34},"Папка",[this]{app.action("folder");});
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
            auto detail=pool?(source->path.empty()?"Из браузера · перетащите →":"На компьютере · перетащите →"):
                (source->primary?"По умолчанию · ":"")+source->language+(source->codec?" · перекодирование":" · исходный кодек");
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
            c.text(settlePool?(source->path.empty()?"Из браузера":"На компьютере"):(source->language+(source->codec?" · перекодирование":" · исходный кодек")),box.x+10,box.y+std::min(box.h-10.f,49.f),10,muted,box.w-20);
        }
    }
}
}
