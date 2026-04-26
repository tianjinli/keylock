#include "ResourceManager.h"

#include <atlcore.h>
#include <shlwapi.h>

bool ResourceManager::LoadRawResource(uint32_t resource_id, const TCHAR* resource_type, void** resource_data, uint32_t* resource_size) {
  auto instance_ = GetModuleHandle(nullptr);
  HRSRC resource_handle = FindResource(instance_, MAKEINTRESOURCE(resource_id), resource_type);
  if (!resource_handle) {
    return false;
  }

  HGLOBAL resource_data_handle = LoadResource(instance_, resource_handle);
  if (!resource_data_handle) {
    return false;
  }

  void* lock_resource_data = LockResource(resource_data_handle);
  uint32_t lock_resource_size = SizeofResource(instance_, resource_handle);
  if (lock_resource_data == nullptr || lock_resource_size == 0) {
    return false;
  }
  *resource_data = lock_resource_data;
  *resource_size = lock_resource_size;
  return true;
}

bool ResourceManager::ExtractResourceToFile(uint32_t resource_id, const TCHAR* resource_type, const CString& save_file_path) {
  if (PathFileExists(save_file_path)) {
    return true;
  }

  void* resource_data = nullptr;
  uint32_t resource_size = 0;
  if (!LoadRawResource(resource_id, resource_type, &resource_data, &resource_size)) {
    return false;
  }

  std::ofstream file(save_file_path, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }

  file.write(reinterpret_cast<const char*>(resource_data), resource_size);
  return true;
}

std::unique_ptr<Gdiplus::Image> ResourceManager::LoadImageWithFallback(const CString& file_path, uint32_t resource_id, const TCHAR* resource_type) {
  // 尝试从文件加载
  std::unique_ptr<Gdiplus::Image> file_image(Gdiplus::Image::FromFile(file_path));
  if (file_image && file_image->GetLastStatus() == Gdiplus::Ok) {
    return file_image;
  }

  void* resource_data = nullptr;
  uint32_t resource_size = 0;
  if (!LoadRawResource(resource_id, resource_type, &resource_data, &resource_size)) {
    return nullptr;
  }

  // 直接用 SHCreateMemStream 包装内存，不需要 GlobalAlloc
  IStream* stream = SHCreateMemStream(reinterpret_cast<const BYTE*>(resource_data), resource_size);

  if (!stream)
    return nullptr;

  std::unique_ptr<IStream, void (*)(IStream*)> stream_ptr(stream, [](IStream* s) {
    s->Release();
  });

  auto image_ptr = std::make_unique<Gdiplus::Image>(stream_ptr.get());
  if (image_ptr && image_ptr->GetLastStatus() == Gdiplus::Ok)
    return image_ptr;

  return nullptr;
}

std::unique_ptr<CSimpleIni> ResourceManager::LoadIniWithFallback(const TCHAR* file_path, uint32_t resource_id, const TCHAR* resource_type) {
  auto ini_handle = std::make_unique<CSimpleIni>();
  SI_Error rc = SI_FAIL;

   if (PathFileExists(file_path)) {
    rc = ini_handle->LoadFile(file_path);
  }

  if (rc != SI_OK) {
    // 从资源加载默认配置
    void* resource_data = nullptr;
    uint32_t resource_size = 0;
    if (!LoadRawResource(resource_id, resource_type, &resource_data, &resource_size)) {
      return nullptr;
    }

    rc = ini_handle->LoadData((const char*) resource_data, resource_size);
    if (rc != SI_OK) {
      return nullptr;
    }
  }

  return ini_handle;
}

std::unique_ptr<Gdiplus::Bitmap> ResourceManager::ConvertImageToBitmap(Gdiplus::Image* src_image, Gdiplus::Color background_color) {
  if (!src_image)
    return nullptr;

  const int width = src_image->GetWidth();
  const int height = src_image->GetHeight();

  auto dst_bitmap = std::make_unique<Gdiplus::Bitmap>(width, height, PixelFormat32bppARGB);

  Gdiplus::Graphics graphics(dst_bitmap.get());
  graphics.Clear(background_color);
  graphics.DrawImage(src_image, 0, 0, width, height);

  return dst_bitmap;
}
