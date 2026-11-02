#include "platform.hpp"
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
}
