// Pictures in and out through Windows (WIC): any PNG / JPEG / BMP / GIF / TIFF file -> RGBA, RGBA -> PNG.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <filesystem>

#include "svrfmt/texture.h"

namespace wic_image {

using Microsoft::WRL::ComPtr;

inline ComPtr<IWICImagingFactory> Factory() {
  ComPtr<IWICImagingFactory> f;
  CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f));
  return f;
}

inline bool Load(const std::filesystem::path &file, svrfmt::Image &out) {
  auto wic = Factory();
  ComPtr<IWICBitmapDecoder> dec;
  if (!wic || FAILED(wic->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnLoad, &dec)))
    return false;
  ComPtr<IWICBitmapFrameDecode> frame;
  ComPtr<IWICFormatConverter> conv;
  if (FAILED(dec->GetFrame(0, &frame)) || FAILED(wic->CreateFormatConverter(&conv)) ||
      FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                              WICBitmapPaletteTypeCustom)))
    return false;
  UINT w = 0, h = 0;
  conv->GetSize(&w, &h);
  if (!w || !h || w > 8192 || h > 8192)
    return false;
  out.w = int(w);
  out.h = int(h);
  out.rgba.resize(size_t(w) * h * 4);
  return SUCCEEDED(conv->CopyPixels(nullptr, w * 4, UINT(out.rgba.size()), out.rgba.data()));
}

inline bool SavePng(const std::filesystem::path &file, const svrfmt::Image &img) {
  auto wic = Factory();
  ComPtr<IWICStream> stream;
  ComPtr<IWICBitmapEncoder> enc;
  ComPtr<IWICBitmapFrameEncode> frame;
  if (!wic || FAILED(wic->CreateStream(&stream)) ||
      FAILED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) ||
      FAILED(wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) ||
      FAILED(enc->Initialize(stream.Get(), WICBitmapEncoderNoCache)) ||
      FAILED(enc->CreateNewFrame(&frame, nullptr)) || FAILED(frame->Initialize(nullptr)) ||
      FAILED(frame->SetSize(UINT(img.w), UINT(img.h))))
    return false;
  WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppRGBA;
  if (FAILED(frame->SetPixelFormat(&fmt)))
    return false;
  if (fmt != GUID_WICPixelFormat32bppRGBA) {  // the encoder wants another layout: hand it converted
    ComPtr<IWICBitmap> bmp;
    ComPtr<IWICFormatConverter> conv;
    if (FAILED(wic->CreateBitmapFromMemory(UINT(img.w), UINT(img.h), GUID_WICPixelFormat32bppRGBA, UINT(img.w * 4),
                                           UINT(img.rgba.size()), const_cast<BYTE *>(img.rgba.data()), &bmp)) ||
        FAILED(wic->CreateFormatConverter(&conv)) ||
        FAILED(conv->Initialize(bmp.Get(), fmt, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom)) ||
        FAILED(frame->WriteSource(conv.Get(), nullptr)))
      return false;
  } else if (FAILED(frame->WritePixels(UINT(img.h), UINT(img.w * 4), UINT(img.rgba.size()),
                                       const_cast<BYTE *>(img.rgba.data())))) {
    return false;
  }
  return SUCCEEDED(frame->Commit()) && SUCCEEDED(enc->Commit());
}

}  // namespace wic_image
