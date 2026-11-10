#include "assembly_source.hpp"
#include "language_options.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace ss;
int main() {
    assert(trackLanguageLabel("eng", "Английский", true) == "English");
    assert(trackLanguageLabel("eng", "Английский", false) == "Английский");
    assert(normalizedLanguage("English") == "eng");
    assert(metadataKeyAllowed("ALBUM_ARTIST"));
    assert(metadataKeyAllowed("Автор музыки"));
    assert(!metadataKeyAllowed("title"));
    assert(!metadataKeyAllowed("bad=name"));
    assert(metadataTagMatches(Json{{"GENRE","Rock"}},"genre","Rock"));
    assert(!metadataTagMatches(Json{{"GENRE","Rock"}},"genre","Jazz"));
    assert(metadataTagMatches(Json::object(),"genre",""));
    assert(metadataTagMatchesAny(Json{{"name","Video"}},{"title","name"},"Video"));
    assert(!metadataTagMatchesAny(Json{{"name","Video"}},{"title","name"},""));
    std::map<std::string,std::string> old={{"genre","Rock"}};
    assert(!metadataTagChanged(old,"genre","Rock"));
    assert(metadataTagChanged(old,"genre",""));
    assert(!metadataTagChanged(old,"newtag",""));
    AssemblySource source;source.role=4;source.chaptersReady=true;
    source.chapterEdits={{"1.25","3.50","Глава 1",Json{{"comment","line one\nline two"}}}};
    source.extraTags={{"genre","Jazz"}};source.originalExtraTags=old;source.originalKnownTags={{"title","Original"}};
    auto saved=assemblyFrom(assemblyJson(source));
    assert(saved.chapterEdits.size()==1 && saved.chapterEdits[0].title=="Глава 1");
    assert(saved.extraTags==source.extraTags && saved.originalExtraTags==old && saved.originalKnownTags==source.originalKnownTags);
    auto upper=chaptersFromProbe(Json{{"chapters",Json::array({Json{{"start_time","0"},{"end_time","2"},{"tags",{{"TITLE","Uppercase"},{"comment","Note"}}}}})}});
    assert(str(upper[0],"title")=="Uppercase" && upper[0]["tags"]["comment"]=="Note");
    auto chapters=editedChapters(saved);
    assert(number(chapters[0],"start_time")==1.25);
    assert(chapters[0]["tags"]["comment"]=="line one\nline two");
    assert(chaptersToFfmetadata(chapters).find("comment=line one\\\nline two")!=std::string::npos);
    saved.chapterEdits[0].end="1.0";bool rejected=false;
    try {(void)editedChapters(saved);}catch(...){rejected=true;}assert(rejected);
    saved.chapterEdits[0].end="4oops";rejected=false;
    try {(void)editedChapters(saved);}catch(...){rejected=true;}assert(rejected);
    Media media;media.title="Sample";media.key="local:sample";
    Json probe={{"streams",Json::array({
        Json{{"index",0},{"codec_type","video"},{"codec_name","h264"},{"disposition",{{"default",1}}}},
        Json{{"index",1},{"codec_type","audio"},{"codec_name","aac"},{"tags",{{"language","rus"}}}},
        Json{{"index",2},{"codec_type","subtitle"},{"codec_name","subrip"},{"disposition",{{"forced",1}}}},
        Json{{"index",3},{"codec_type","video"},{"codec_name","png"},{"disposition",{{"attached_pic",1}}},{"tags",{{"comment","cover comment"}}}},
        Json{{"index",4},{"codec_type","attachment"},{"codec_name","ttf"},{"tags",{{"filename","font.ttf"}}}}
    })},{"chapters",Json::array({Json{{"start_time",0},{"end_time",3}}})},{"format",{{"tags",{{"title","Sample"}}}}}};
    auto sources=assemblySourcesFromProbe(media,"sample.mkv",probe);
    assert(sources.size()==7);
    for(int i=0;i<5;++i)assert(sources[i].stream==i);
    assert(sources[0].role==0 && sources[0].primary && sources[0].embeddedKind=="stream");
    assert(copiedStreamDifference(Json{{"codec_name","h264"},{"width",1920}},Json{{"codec_name","h264"},{"width",1920}}).empty());
    assert(copiedStreamDifference(Json{{"codec_name","h264"},{"width",1920}},Json{{"codec_name","h264"},{"width",1280}})=="width");
    assert(copiedStreamDifference(Json{{"side_data_list",Json::array({Json{{"side_data_type","Display Matrix"},{"rotation",90}}})}},Json{{"side_data_list",Json::array({Json{{"side_data_type","Display Matrix"},{"rotation",0}}})}})=="Display Matrix");
    assert(sources[1].role==1 && sources[1].language=="rus" && sources[1].media.acodec=="aac");
    assert(sources[2].role==2 && sources[2].forced);
    assert(sources[3].role==3 && sources[3].embeddedKind=="cover");
    assert(sources[4].role==6 && sources[4].embeddedKind=="attachment");
    assert(sources[5].role==4 && sources[5].embeddedKind=="chapters");
    assert(sources[6].role==5 && sources[6].embeddedKind=="metadata");
    auto generated=assemblyFrom(assemblyJson(sources[5]));
    assert(generated.name=="Главы · Sample");
    auto englishSources=assemblySourcesFromProbe(media,"sample.mkv",probe,"en");
    assert(englishSources[5].name=="Chapters · Sample");
    assert(generated.name=="Главы · Sample"); // Creating an English view does not change saved content.
    generated.name="My own chapters";
    assert(assemblyFrom(assemblyJson(generated)).name=="My own chapters");
    AssemblySource audio;setGeneratedAssemblyName(audio,"optional_audio","Sample","en");
    assert(audio.name=="Sample · audio (if available)");
    assert(assemblyFrom(assemblyJson(audio)).name=="Sample · audio (if available)");
    auto legacy=assemblyFrom(Json{{"name","Моё название"}});
    assert(legacy.name=="Моё название");
    auto coverCopy=assemblyFrom(assemblyJson(sources[3]));
    assert(coverCopy.embeddedKind=="cover" && coverCopy.originalCoverTags["comment"]=="cover comment");
    probe["programs"]=Json::array({Json{{"program_id",1}}});
    auto withProgram=assemblySourcesFromProbe(media,"sample.ts",probe);
    assert(std::any_of(withProgram.begin(),withProgram.end(),[](const auto &item){return item.role==6 && item.embeddedKind=="programs";}));
    Json imageProbe={{"streams",Json::array({Json{{"index",0},{"codec_type","video"},{"codec_name","mjpeg"}}})},{"format",{{"format_name","jpeg_pipe"}}}};
    auto image=assemblySourcesFromProbe(media,"image.jpg",imageProbe);
    assert(image.size()==1 && image[0].role==3 && image[0].coverFormat==1 && image[0].embeddedKind.empty());
    std::cout<<"Metadata editor checks passed\n";
}
