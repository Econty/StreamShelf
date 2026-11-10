#pragma once
#include "platform.hpp"
#include <string>
#include <vector>
#include <utility>
#include "iso6392_data.hpp"
namespace ss {
inline const std::vector<std::pair<std::string,std::string>> &trackLanguages(){
    static const auto values=[] {
        std::vector<std::pair<std::string,std::string>> result={{"","Не указан"},{"und","Не определён"},{"rus","Русский"},{"eng","Английский"},{"ukr","Украинский"},{"bel","Белорусский"},{"ger","Немецкий"},{"fre","Французский"},{"spa","Испанский"},{"ita","Итальянский"},{"por","Португальский"},{"jpn","Японский"},{"kor","Корейский"},{"chi","Китайский"},{"ara","Арабский"},{"tur","Турецкий"},{"pol","Польский"},{"dut","Нидерландский"},{"cze","Чешский"},{"hin","Хинди"},{"kaz","Казахский"},{"uzb","Узбекский"},{"heb","Иврит"},{"swe","Шведский"},{"fin","Финский"},{"nor","Норвежский"},{"dan","Датский"},{"gre","Греческий"}};
        for(const auto &entry:isoLanguages){bool exists=false;for(const auto &item:result)if(item.first==entry.bibliographic){exists=true;break;}if(!exists)result.emplace_back(entry.bibliographic,std::string(entry.name)+" · "+entry.bibliographic);}
        return result;
    }();return values;
}
inline std::string trackLanguageLabel(const std::string &code, const std::string &fallback, bool english) {
    if (!english) return fallback;
    for (const auto &entry : isoLanguages) if (code == entry.bibliographic) return entry.name;
    return fallback;
}
inline std::string normalizedLanguage(std::string language){
    if(!language.empty()) {
        try {
            auto input=wide(language);
            for(const auto &[code,label]:trackLanguages())if(!label.empty()){
                auto name=wide(label);if(CompareStringOrdinal(input.c_str(),-1,name.c_str(),-1,TRUE)==CSTR_EQUAL)return code;
            }
            for(const auto &entry:isoLanguages){auto name=wide(entry.name);if(CompareStringOrdinal(input.c_str(),-1,name.c_str(),-1,TRUE)==CSTR_EQUAL)return entry.bibliographic;}
        } catch(...) {}
    }
    for(auto &ch:language)if(ch>='A'&&ch<='Z')ch=char(ch-'A'+'a');
    auto dash=language.find_first_of("-_");if(dash!=std::string::npos)language.resize(dash);
    if(language.empty())return "";
    for(const auto &entry:isoLanguages)if(language==entry.bibliographic || (!std::string(entry.terminologic).empty()&&language==entry.terminologic) || (!std::string(entry.twoLetter).empty()&&language==entry.twoLetter))return entry.bibliographic;
    return "und";
}
}
