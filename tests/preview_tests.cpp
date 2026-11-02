#include "platform.hpp"
#include <iostream>
int main(){
    int passed=0;
    for(const auto* name:{L"preview-wide.png",L"preview-wide.jpg"}){
        auto preview=ss::decodePreview(ss::readFile(ss::fs::path(L"tests/fixtures")/name));
        if(preview.width!=480||preview.height!=120||preview.rgba.size()!=480*120*4){std::cerr<<"FAIL thumbnail size/aspect\n";return 1;}
        if(preview.rgba[0]<180||preview.rgba[1]>50||preview.rgba[2]>50){std::cerr<<"FAIL RGBA channel order\n";return 1;}
        passed+=2;
    }
    try{ss::decodePreview("not an image");std::cerr<<"FAIL corrupt input accepted\n";return 1;}catch(...){++passed;}
    try{ss::decodePreview(std::string(9*1024*1024,'x'));std::cerr<<"FAIL excessive input accepted\n";return 1;}catch(...){++passed;}
    std::cout<<passed<<" preview decode checks passed\n";
}
