#pragma once

#include <atlstr.h>
#include <windows.h>
#include <gdiplus.h>

#include <fstream>
#include <memory>
#include <vector>

#include "SimpleIni.h"

/**
 * @brief 资源管理器类
 * 负责从资源文件中提取文件到磁盘和加载资源
 */
struct ResourceManager {
  // 资源加载
  static bool LoadRawResource(uint32_t resource_id, const TCHAR* resource_type, void** resource_data, uint32_t* resource_size);

  // 资源提取
  static bool ExtractResourceToFile(uint32_t resource_id, const TCHAR* resource_type, const CString& save_file_path);

  static std::unique_ptr<Gdiplus::Image> LoadImageWithFallback(const CString& file_path, uint32_t resource_id, const TCHAR* resource_type = TEXT("PNG"));

  static std::unique_ptr<CSimpleIni> LoadIniWithFallback(const TCHAR* file_path, uint32_t resource_id, const TCHAR* resource_type = RT_RCDATA);

  static std::unique_ptr<Gdiplus::Bitmap> ConvertImageToBitmap(Gdiplus::Image* src_image, Gdiplus::Color background_color = Gdiplus::Color::Transparent);
};
