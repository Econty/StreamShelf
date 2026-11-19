#include "media_preview.hpp"
#include <wincodec.h>
#include <shlwapi.h>
namespace ss {
template<class T> struct ComPtr {T* p=nullptr;~ComPtr(){if(p)p->Release();} T** out(){return &p;} T* operator->(){return p;} };
PreviewImage decodePreview(const std::string &data) {
    if(data.empty()||data.size()>8*1024*1024)throw std::runtime_error("Изображение слишком большое или пустое");
    HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    struct Finish {HRESULT init;~Finish(){if(SUCCEEDED(init))CoUninitialize();}} finish{init};
    auto ok=[](HRESULT value){if(FAILED(value))throw std::runtime_error("Формат миниатюры не поддерживается");};
    ComPtr<IWICImagingFactory> factory;
    ok(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.out())));
    ComPtr<IStream> stream;stream.p=SHCreateMemStream(reinterpret_cast<const BYTE*>(data.data()),UINT(data.size()));
    if(!stream.p)throw std::runtime_error("Не удалось прочитать миниатюру");
    ComPtr<IWICBitmapDecoder> decoder;ok(factory->CreateDecoderFromStream(stream.p,nullptr,WICDecodeMetadataCacheOnDemand,decoder.out()));
    ComPtr<IWICBitmapFrameDecode> frame;ok(decoder->GetFrame(0,frame.out()));
    UINT w=0,h=0;ok(frame->GetSize(&w,&h));
    if(!w||!h||uint64_t(w)*h>100000000)throw std::runtime_error("Некорректный размер изображения");
    double scale=std::min({1.,480./w,270./h});
    PreviewImage result;result.width=std::max(1,int(w*scale));result.height=std::max(1,int(h*scale));
    ComPtr<IWICBitmapScaler> scaler;ok(factory->CreateBitmapScaler(scaler.out()));
    ok(scaler->Initialize(frame.p,result.width,result.height,WICBitmapInterpolationModeFant));
    ComPtr<IWICFormatConverter> converter;ok(factory->CreateFormatConverter(converter.out()));
    ok(converter->Initialize(scaler.p,GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    result.rgba.resize(size_t(result.width)*result.height*4);
    ok(converter->CopyPixels(nullptr,result.width*4,UINT(result.rgba.size()),result.rgba.data()));
    return result;
}

PreviewImage decodeMediaPreview(const fs::path &ffmpeg, const fs::path &ffprobe, const std::wstring &input,
                                bool remote, MediaPreviewKind kind, std::atomic_bool &cancel) {
    constexpr auto localMedia=L"mov,matroska,webm,avi,mpegts,mpeg,mpegvideo,mp3,ogg,flac,aac,wav,asf,aiff,amr,ac3,eac3,ape,flv";
    constexpr auto localImages=L"mov,apng,gif,webp_pipe,webp_anim,png_pipe,jpeg_pipe,bmp_pipe,tiff_pipe,jpegxl_pipe,jpegxl_anim,heif";
    const auto appendInputOptions = [&](std::vector<std::wstring> &args) {
        args.insert(args.end(), {L"-protocol_whitelist", remote ? L"http,https,tcp,tls,crypto" : L"file,pipe",
                                L"-analyzeduration", L"2500000", L"-probesize", L"2500000"});
        if (remote) args.insert(args.end(), {L"-rw_timeout", L"4000000"});
        else args.insert(args.end(), {L"-format_whitelist", kind == MediaPreviewKind::image ? localImages : localMedia});
    };
    const auto extract = [&](const std::wstring &mapping, bool waveform) -> PreviewImage {
        if (cancel) return {};
        std::vector<std::wstring> args{L"-hide_banner",L"-loglevel",L"error",L"-nostdin",L"-max_alloc",L"134217728"};
        appendInputOptions(args);
        if (waveform) args.insert(args.end(), {L"-t", L"10"});
        args.insert(args.end(), {L"-i",input});
        if (waveform) args.insert(args.end(), {L"-filter_complex",
            L"[0:a:0]aresample=8000,showwavespic=s=480x270:split_channels=0:colors=0x38bdf8[wave]", L"-map",L"[wave]"});
        else args.insert(args.end(), {L"-map",mapping,L"-vf",L"scale=480:270:force_original_aspect_ratio=decrease"});
        args.insert(args.end(), {L"-frames:v",L"1",L"-an",L"-sn",L"-dn",L"-threads",L"1",L"-c:v",L"png",L"-f",L"image2pipe",L"pipe:1"});
        auto process=runProcess(ffmpeg,args,nullptr,cancel,true);
        if (cancel || process.output.empty()) return {};
        try { return decodePreview(process.output); }
        catch (...) { return {}; }
    };
    if (!remote && kind != MediaPreviewKind::image && !ffprobe.empty() && !cancel) {
        std::vector<std::wstring> args{L"-v",L"error",L"-max_alloc",L"134217728"};
        appendInputOptions(args);
        args.insert(args.end(), {L"-show_entries",L"stream=index,codec_name,codec_type:stream_disposition=attached_pic:stream_tags=filename",
                                 L"-of",L"json",input});
        auto process=runProcess(ffprobe,args,nullptr,cancel,true);
        const auto data=Json::parse(process.output,nullptr,false);
        if (!process.code && data.is_object()) {
            int cover=-1, video=-1;
            bool hasAudio=false;
            for (const auto &stream : data.value("streams",Json::array())) {
                auto filename=str(stream.value("tags",Json::object()),"filename");
                std::transform(filename.begin(),filename.end(),filename.begin(),[](unsigned char c){return char(std::tolower(c));});
                const auto codec=str(stream,"codec_name"), type=str(stream,"codec_type");
                const auto index=int(number(stream,"index",-1));
                if (type=="audio") hasAudio=true;
                const auto disposition=stream.value("disposition",Json::object());
                const bool attached=number(disposition,"attached_pic")!=0 ||
                    ((codec=="mjpeg" || codec=="png" || codec=="webp") &&
                     (filename=="cover.jpg" || filename=="cover.jpeg" || filename=="cover.png" || filename=="cover.webp"));
                if (attached && cover < 0) cover=index;
                if (!attached && type=="video" && video < 0) video=index;
            }
            for (const auto index : {cover,video}) {
                if (index<0) continue;
                auto image=extract(L"0:"+std::to_wstring(index),false);
                if (!image.rgba.empty()) return image;
            }
            return hasAudio ? extract(L"",true) : PreviewImage{};
        }
    }
    auto image=extract(L"0:v:0",false);
    if (image.rgba.empty() && kind==MediaPreviewKind::audio && !cancel) image=extract(L"",true);
    return image;
}
}
