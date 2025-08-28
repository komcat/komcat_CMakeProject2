// =============================================================================
// UIVisionPanel_SaveImages.cpp - Windows GDI+ Implementation (Simple & Reliable)
// =============================================================================

#include "UIVisionPanel.h"
#include <sqlite3.h>
#include <filesystem>
#include <chrono>
#include <iostream>

// GDI+ includes AFTER other critical includes to prevent conflicts
#ifdef _WIN32
#define _WINSOCKAPI_   // Prevent winsock.h
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>    // Add this for COM interfaces
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;
#endif

// =============================================================================
// Helper function to get encoder CLSID
// =============================================================================
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
  UINT num = 0;
  UINT size = 0;

  ImageCodecInfo* pImageCodecInfo = NULL;
  GetImageEncodersSize(&num, &size);

  if (size == 0) return -1;

  pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
  if (pImageCodecInfo == NULL) return -1;

  GetImageEncoders(num, size, pImageCodecInfo);

  for (UINT j = 0; j < num; ++j) {
    if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
      *pClsid = pImageCodecInfo[j].Clsid;
      free(pImageCodecInfo);
      return j;
    }
  }

  free(pImageCodecInfo);
  return -1;
}

// =============================================================================
// Main guidance image saving method using Windows GDI+
// =============================================================================

bool UIVisionPanel::SaveGuidanceImageForNode(const std::string& nodeId) {
  if (!m_hasImageData || m_lastImageData.empty()) {
    std::cerr << "[UIVisionPanel] No image data available to save as guidance" << std::endl;
    return false;
  }

  if (m_imageWidth <= 0 || m_imageHeight <= 0) {
    std::cerr << "[UIVisionPanel] Invalid image dimensions: "
      << m_imageWidth << "x" << m_imageHeight << std::endl;
    return false;
  }

#ifdef _WIN32
  // Initialize GDI+
  GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;
  Status gdiplusStatus = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
  if (gdiplusStatus != Ok) {
    std::cerr << "[UIVisionPanel] Failed to initialize GDI+: " << gdiplusStatus << std::endl;
    return false;
  }

  // Create guidance images directory
  std::filesystem::path guidanceDir = "guidance_images";
  if (!std::filesystem::exists(guidanceDir)) {
    try {
      std::filesystem::create_directories(guidanceDir);
      std::cout << "[UIVisionPanel] Created guidance images directory: " << guidanceDir << std::endl;
    }
    catch (const std::exception& e) {
      std::cerr << "[UIVisionPanel] Failed to create guidance directory: " << e.what() << std::endl;
      GdiplusShutdown(gdiplusToken);
      return false;
    }
  }

  // Generate filename with timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::string filename = nodeId + "_guidance_" + std::to_string(time_t) + ".jpg";
  std::filesystem::path imagePath = guidanceDir / filename;

  bool success = false;
  Bitmap* bitmap = nullptr;

  try {
    // Determine image format
    int expectedRGBSize = m_imageWidth * m_imageHeight * 3;
    int expectedGraySize = m_imageWidth * m_imageHeight;

    std::cout << "[UIVisionPanel] Saving guidance image - Size: " << m_imageWidth << "x" << m_imageHeight
      << ", Data size: " << m_lastImageData.size() << " bytes" << std::endl;

    if (m_lastImageData.size() == expectedRGBSize) {
      // RGB Color image
      std::cout << "[UIVisionPanel] Processing RGB image..." << std::endl;

      bitmap = new Bitmap(m_imageWidth, m_imageHeight, PixelFormat24bppRGB);
      if (!bitmap) {
        std::cerr << "[UIVisionPanel] Failed to create bitmap object" << std::endl;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      // Lock bitmap for writing
      BitmapData bitmapData;
      Rect rect(0, 0, m_imageWidth, m_imageHeight);

      Status lockStatus = bitmap->LockBits(&rect, ImageLockModeWrite, PixelFormat24bppRGB, &bitmapData);
      if (lockStatus != Ok) {
        std::cerr << "[UIVisionPanel] Failed to lock RGB bitmap bits: " << lockStatus << std::endl;
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      BYTE* pixels = (BYTE*)bitmapData.Scan0;
      if (!pixels) {
        std::cerr << "[UIVisionPanel] Invalid bitmap pixel data" << std::endl;
        bitmap->UnlockBits(&bitmapData);
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      // Safely copy RGB data to bitmap
      try {
        for (int y = 0; y < m_imageHeight; y++) {
          for (int x = 0; x < m_imageWidth; x++) {
            int srcIdx = (y * m_imageWidth + x) * 3;
            int dstIdx = y * bitmapData.Stride + x * 3;

            // Bounds checking
            if (srcIdx + 2 < (int)m_lastImageData.size() &&
              dstIdx + 2 < bitmapData.Stride * m_imageHeight) {

              // Convert RGB to BGR for Windows
              pixels[dstIdx + 0] = m_lastImageData[srcIdx + 2]; // B
              pixels[dstIdx + 1] = m_lastImageData[srcIdx + 1]; // G
              pixels[dstIdx + 2] = m_lastImageData[srcIdx + 0]; // R
            }
          }
        }
      }
      catch (const std::exception& e) {
        std::cerr << "[UIVisionPanel] Exception during pixel copy: " << e.what() << std::endl;
        bitmap->UnlockBits(&bitmapData);
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      Status unlockStatus = bitmap->UnlockBits(&bitmapData);
      if (unlockStatus != Ok) {
        std::cerr << "[UIVisionPanel] Failed to unlock bitmap: " << unlockStatus << std::endl;
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      std::cout << "[UIVisionPanel] Successfully created RGB bitmap" << std::endl;

    }
    else if (m_lastImageData.size() == expectedGraySize) {
      // Grayscale image - convert to RGB for saving
      std::cout << "[UIVisionPanel] Processing grayscale image..." << std::endl;

      bitmap = new Bitmap(m_imageWidth, m_imageHeight, PixelFormat24bppRGB);
      if (!bitmap) {
        std::cerr << "[UIVisionPanel] Failed to create grayscale bitmap object" << std::endl;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      BitmapData bitmapData;
      Rect rect(0, 0, m_imageWidth, m_imageHeight);

      Status lockStatus = bitmap->LockBits(&rect, ImageLockModeWrite, PixelFormat24bppRGB, &bitmapData);
      if (lockStatus != Ok) {
        std::cerr << "[UIVisionPanel] Failed to lock grayscale bitmap bits: " << lockStatus << std::endl;
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      BYTE* pixels = (BYTE*)bitmapData.Scan0;
      if (!pixels) {
        std::cerr << "[UIVisionPanel] Invalid grayscale bitmap pixel data" << std::endl;
        bitmap->UnlockBits(&bitmapData);
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      // Safely convert grayscale to RGB
      try {
        for (int y = 0; y < m_imageHeight; y++) {
          for (int x = 0; x < m_imageWidth; x++) {
            int srcIdx = y * m_imageWidth + x;
            int dstIdx = y * bitmapData.Stride + x * 3;

            // Bounds checking
            if (srcIdx < (int)m_lastImageData.size() &&
              dstIdx + 2 < bitmapData.Stride * m_imageHeight) {

              BYTE grayValue = m_lastImageData[srcIdx];
              pixels[dstIdx + 0] = grayValue; // B
              pixels[dstIdx + 1] = grayValue; // G
              pixels[dstIdx + 2] = grayValue; // R
            }
          }
        }
      }
      catch (const std::exception& e) {
        std::cerr << "[UIVisionPanel] Exception during grayscale conversion: " << e.what() << std::endl;
        bitmap->UnlockBits(&bitmapData);
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      Status unlockStatus = bitmap->UnlockBits(&bitmapData);
      if (unlockStatus != Ok) {
        std::cerr << "[UIVisionPanel] Failed to unlock grayscale bitmap: " << unlockStatus << std::endl;
        delete bitmap;
        GdiplusShutdown(gdiplusToken);
        return false;
      }

      std::cout << "[UIVisionPanel] Successfully created grayscale bitmap" << std::endl;

    }
    else {
      std::cerr << "[UIVisionPanel] Unexpected image data size: " << m_lastImageData.size()
        << " (expected " << expectedRGBSize << " for RGB or "
        << expectedGraySize << " for grayscale)" << std::endl;
      GdiplusShutdown(gdiplusToken);
      return false;
    }

    // Save the bitmap
    if (bitmap) {
      std::cout << "[UIVisionPanel] Attempting to save bitmap..." << std::endl;

      // Get JPEG encoder CLSID
      CLSID jpegClsid;
      if (GetEncoderClsid(L"image/jpeg", &jpegClsid) >= 0) {

        // Set JPEG quality
        EncoderParameters encoderParameters;
        encoderParameters.Count = 1;
        encoderParameters.Parameter[0].Guid = EncoderQuality;
        encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
        encoderParameters.Parameter[0].NumberOfValues = 1;
        ULONG quality = 90; // High quality (0-100)
        encoderParameters.Parameter[0].Value = &quality;

        // Convert path to wide string SAFELY
        std::string pathStr = imagePath.string();
        std::wstring wImagePath;

        try {
          // Safe conversion using Windows API
          int wideCharCount = MultiByteToWideChar(CP_UTF8, 0, pathStr.c_str(), -1, NULL, 0);
          if (wideCharCount > 0) {
            wImagePath.resize(wideCharCount - 1); // -1 to exclude null terminator
            MultiByteToWideChar(CP_UTF8, 0, pathStr.c_str(), -1, &wImagePath[0], wideCharCount);
          }
          else {
            std::cerr << "[UIVisionPanel] Failed to convert path to wide string" << std::endl;
            delete bitmap;
            GdiplusShutdown(gdiplusToken);
            return false;
          }
        }
        catch (const std::exception& e) {
          std::cerr << "[UIVisionPanel] Exception during path conversion: " << e.what() << std::endl;
          delete bitmap;
          GdiplusShutdown(gdiplusToken);
          return false;
        }

        std::cout << "[UIVisionPanel] Saving to: " << imagePath.string() << std::endl;

        // Save as JPEG
        Status saveStatus = bitmap->Save(wImagePath.c_str(), &jpegClsid, &encoderParameters);
        if (saveStatus == Ok) {
          std::cout << "[UIVisionPanel] Successfully saved guidance image: " << imagePath << std::endl;
          success = true;
        }
        else {
          std::cerr << "[UIVisionPanel] Failed to save bitmap to file. Status: " << saveStatus << std::endl;
        }
      }
      else {
        std::cerr << "[UIVisionPanel] Failed to get JPEG encoder CLSID" << std::endl;
      }
    }

  }
  catch (const std::exception& e) {
    std::cerr << "[UIVisionPanel] Exception saving guidance image: " << e.what() << std::endl;
  }
  catch (...) {
    std::cerr << "[UIVisionPanel] Unknown exception saving guidance image" << std::endl;
  }

  // Cleanup
  if (bitmap) {
    delete bitmap;
    bitmap = nullptr;
  }

  // Shutdown GDI+
  GdiplusShutdown(gdiplusToken);

  if (success) {
    return UpdateGuidanceImagePath(nodeId, imagePath.string());
  }
  return false;

#else
  // Non-Windows fallback
  std::cerr << "[UIVisionPanel] Image saving not implemented for non-Windows platforms" << std::endl;
  return false;
#endif
}

// =============================================================================
// Advanced version with format options
// =============================================================================

bool UIVisionPanel::SaveGuidanceImageForNodeAdvanced(const std::string& nodeId,
  const std::string& format) {
  if (!m_hasImageData || m_lastImageData.empty()) {
    std::cerr << "[UIVisionPanel] No image data available to save as guidance" << std::endl;
    return false;
  }

  if (m_imageWidth <= 0 || m_imageHeight <= 0) {
    std::cerr << "[UIVisionPanel] Invalid image dimensions" << std::endl;
    return false;
  }

#ifdef _WIN32
  // Initialize GDI+
  GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;
  if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
    std::cerr << "[UIVisionPanel] Failed to initialize GDI+" << std::endl;
    return false;
  }

  // Create guidance images directory
  std::filesystem::path guidanceDir = "guidance_images";
  if (!std::filesystem::exists(guidanceDir)) {
    std::filesystem::create_directories(guidanceDir);
  }

  // Generate filename with timestamp and specified format
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::string filename = nodeId + "_guidance_" + std::to_string(time_t) + "." + format;
  std::filesystem::path imagePath = guidanceDir / filename;

  bool success = false;

  try {
    // Create bitmap (same logic as main method)
    int expectedRGBSize = m_imageWidth * m_imageHeight * 3;
    int expectedGraySize = m_imageWidth * m_imageHeight;

    Bitmap* bitmap = nullptr;

    if (m_lastImageData.size() == expectedRGBSize) {
      // RGB image
      bitmap = new Bitmap(m_imageWidth, m_imageHeight, PixelFormat24bppRGB);
      BitmapData bitmapData;
      Rect rect(0, 0, m_imageWidth, m_imageHeight);

      if (bitmap->LockBits(&rect, ImageLockModeWrite, PixelFormat24bppRGB, &bitmapData) == Ok) {
        BYTE* pixels = (BYTE*)bitmapData.Scan0;
        for (int y = 0; y < m_imageHeight; y++) {
          for (int x = 0; x < m_imageWidth; x++) {
            int srcIdx = (y * m_imageWidth + x) * 3;
            int dstIdx = y * bitmapData.Stride + x * 3;
            pixels[dstIdx + 0] = m_lastImageData[srcIdx + 2]; // B
            pixels[dstIdx + 1] = m_lastImageData[srcIdx + 1]; // G
            pixels[dstIdx + 2] = m_lastImageData[srcIdx + 0]; // R
          }
        }
        bitmap->UnlockBits(&bitmapData);
      }
    }
    else if (m_lastImageData.size() == expectedGraySize) {
      // Grayscale to RGB
      bitmap = new Bitmap(m_imageWidth, m_imageHeight, PixelFormat24bppRGB);
      BitmapData bitmapData;
      Rect rect(0, 0, m_imageWidth, m_imageHeight);

      if (bitmap->LockBits(&rect, ImageLockModeWrite, PixelFormat24bppRGB, &bitmapData) == Ok) {
        BYTE* pixels = (BYTE*)bitmapData.Scan0;
        for (int y = 0; y < m_imageHeight; y++) {
          for (int x = 0; x < m_imageWidth; x++) {
            int srcIdx = y * m_imageWidth + x;
            int dstIdx = y * bitmapData.Stride + x * 3;
            BYTE grayValue = m_lastImageData[srcIdx];
            pixels[dstIdx + 0] = grayValue;
            pixels[dstIdx + 1] = grayValue;
            pixels[dstIdx + 2] = grayValue;
          }
        }
        bitmap->UnlockBits(&bitmapData);
      }
    }

    if (bitmap) {
      std::wstring wImagePath = std::wstring(imagePath.string().begin(), imagePath.string().end());
      CLSID encoderClsid;

      if (format == "jpg" || format == "jpeg") {
        if (GetEncoderClsid(L"image/jpeg", &encoderClsid) >= 0) {
          EncoderParameters encoderParameters;
          encoderParameters.Count = 1;
          encoderParameters.Parameter[0].Guid = EncoderQuality;
          encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
          encoderParameters.Parameter[0].NumberOfValues = 1;
          ULONG quality = 92;
          encoderParameters.Parameter[0].Value = &quality;

          success = (bitmap->Save(wImagePath.c_str(), &encoderClsid, &encoderParameters) == Ok);
        }
      }
      else if (format == "png") {
        if (GetEncoderClsid(L"image/png", &encoderClsid) >= 0) {
          success = (bitmap->Save(wImagePath.c_str(), &encoderClsid, NULL) == Ok);
        }
      }
      else if (format == "bmp") {
        if (GetEncoderClsid(L"image/bmp", &encoderClsid) >= 0) {
          success = (bitmap->Save(wImagePath.c_str(), &encoderClsid, NULL) == Ok);
        }
      }

      if (success) {
        std::cout << "[UIVisionPanel] Saved " << format << " guidance image: " << imagePath << std::endl;
      }
      else {
        std::cerr << "[UIVisionPanel] Failed to save " << format << " image" << std::endl;
      }

      delete bitmap;
    }

  }
  catch (const std::exception& e) {
    std::cerr << "[UIVisionPanel] Exception saving " << format << " image: " << e.what() << std::endl;
  }

  GdiplusShutdown(gdiplusToken);

  if (success) {
    return UpdateGuidanceImagePath(nodeId, imagePath.string());
  }
  return false;
#else
  std::cerr << "[UIVisionPanel] Advanced image saving not implemented for non-Windows platforms" << std::endl;
  return false;
#endif
}

bool UIVisionPanel::SaveVisionResultWithOverlay(const std::string& nodeId,
  const std::string& suffix) {
  if (!m_hasImageData || m_lastImageData.empty()) {
    std::cerr << "[UIVisionPanel] No image data available for result saving" << std::endl;
    return false;
  }

  if (!m_hasResult) {
    std::cout << "[UIVisionPanel] No vision results to overlay, saving raw image" << std::endl;
    return SaveGuidanceImageForNode(nodeId);
  }

#ifdef _WIN32
  // Initialize GDI+
  GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;
  if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Ok) {
    std::cerr << "[UIVisionPanel] Failed to initialize GDI+ for result saving" << std::endl;
    return false;
  }

  bool success = false;

  try {
    // Create guidance images directory
    std::filesystem::path guidanceDir = "guidance_images";
    if (!std::filesystem::exists(guidanceDir)) {
      std::filesystem::create_directories(guidanceDir);
    }

    // Generate filename with suffix
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::string filename = nodeId + "_result_" + suffix + "_" + std::to_string(time_t) + ".jpg";
    std::filesystem::path imagePath = guidanceDir / filename;

    // Create Windows GDI+ Bitmap
    int expectedRGBSize = m_imageWidth * m_imageHeight * 3;
    int expectedGraySize = m_imageWidth * m_imageHeight;

    Bitmap* bitmap = nullptr;

    if (m_lastImageData.size() == expectedRGBSize) {
      // RGB image
      bitmap = new Bitmap(m_imageWidth, m_imageHeight, PixelFormat24bppRGB);

      BitmapData bitmapData;
      Rect rect(0, 0, m_imageWidth, m_imageHeight);

      if (bitmap->LockBits(&rect, ImageLockModeWrite, PixelFormat24bppRGB, &bitmapData) == Ok) {
        BYTE* pixels = (BYTE*)bitmapData.Scan0;

        for (int y = 0; y < m_imageHeight; y++) {
          for (int x = 0; x < m_imageWidth; x++) {
            int srcIdx = (y * m_imageWidth + x) * 3;
            int dstIdx = y * bitmapData.Stride + x * 3;

            pixels[dstIdx + 0] = m_lastImageData[srcIdx + 2]; // B
            pixels[dstIdx + 1] = m_lastImageData[srcIdx + 1]; // G
            pixels[dstIdx + 2] = m_lastImageData[srcIdx + 0]; // R
          }
        }

        bitmap->UnlockBits(&bitmapData);
      }
    }
    else if (m_lastImageData.size() == expectedGraySize) {
      // Grayscale image
      bitmap = new Bitmap(m_imageWidth, m_imageHeight, PixelFormat24bppRGB);

      BitmapData bitmapData;
      Rect rect(0, 0, m_imageWidth, m_imageHeight);

      if (bitmap->LockBits(&rect, ImageLockModeWrite, PixelFormat24bppRGB, &bitmapData) == Ok) {
        BYTE* pixels = (BYTE*)bitmapData.Scan0;

        for (int y = 0; y < m_imageHeight; y++) {
          for (int x = 0; x < m_imageWidth; x++) {
            int srcIdx = y * m_imageWidth + x;
            int dstIdx = y * bitmapData.Stride + x * 3;

            BYTE grayValue = m_lastImageData[srcIdx];
            pixels[dstIdx + 0] = grayValue;
            pixels[dstIdx + 1] = grayValue;
            pixels[dstIdx + 2] = grayValue;
          }
        }

        bitmap->UnlockBits(&bitmapData);
      }
    }

    if (bitmap) {
      // Get JPEG encoder CLSID
      CLSID jpegClsid;
      if (GetEncoderClsid(L"image/jpeg", &jpegClsid) >= 0) {

        EncoderParameters encoderParameters;
        encoderParameters.Count = 1;
        encoderParameters.Parameter[0].Guid = EncoderQuality;
        encoderParameters.Parameter[0].Type = EncoderParameterValueTypeLong;
        encoderParameters.Parameter[0].NumberOfValues = 1;
        ULONG quality = 95; // Very high quality
        encoderParameters.Parameter[0].Value = &quality;

        std::wstring wImagePath = std::wstring(imagePath.string().begin(), imagePath.string().end());

        if (bitmap->Save(wImagePath.c_str(), &jpegClsid, &encoderParameters) == Ok) {
          std::cout << "[UIVisionPanel] Saved vision result image: " << imagePath << std::endl;
          success = true;
        }
        else {
          std::cerr << "[UIVisionPanel] Failed to save result bitmap" << std::endl;
        }
      }

      delete bitmap;
    }

  }
  catch (const std::exception& e) {
    std::cerr << "[UIVisionPanel] Exception saving result image: " << e.what() << std::endl;
  }

  GdiplusShutdown(gdiplusToken);
  return success;

#else
  std::cerr << "[UIVisionPanel] SaveVisionResultWithOverlay not implemented for non-Windows platforms" << std::endl;
  return false;
#endif
}

// =============================================================================
// Database helper method to update guidance image path
// =============================================================================

bool UIVisionPanel::UpdateGuidanceImagePath(const std::string& nodeId,
  const std::string& imagePath) {
  std::string dbPath = "vision_presets.db";
  sqlite3* db = nullptr;

  int result = sqlite3_open(dbPath.c_str(), &db);
  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Cannot open database for image path update: "
      << sqlite3_errmsg(db) << std::endl;
    if (db) sqlite3_close(db);
    return false;
  }

  const char* updateSQL = R"(
        UPDATE node_preset_mappings 
        SET guidance_image_path = ?, updated_at = CURRENT_TIMESTAMP 
        WHERE node_id = ?;
    )";

  sqlite3_stmt* stmt;
  result = sqlite3_prepare_v2(db, updateSQL, -1, &stmt, nullptr);

  if (result != SQLITE_OK) {
    std::cerr << "[UIVisionPanel] Failed to prepare update statement: "
      << sqlite3_errmsg(db) << std::endl;
    sqlite3_close(db);
    return false;
  }

  sqlite3_bind_text(stmt, 1, imagePath.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, nodeId.c_str(), -1, SQLITE_STATIC);

  result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (result == SQLITE_DONE) {
    std::cout << "[UIVisionPanel] Updated guidance image path in database for node '"
      << nodeId << "'" << std::endl;

    // Refresh in-memory mappings to show updated guidance image status
    LoadNodePresetMappings();
    return true;
  }
  else {
    std::cerr << "[UIVisionPanel] Failed to update guidance image path in database" << std::endl;
    return false;
  }
}

// =============================================================================
// Method to get supported image formats
// =============================================================================

std::vector<std::string> UIVisionPanel::GetSupportedImageFormats() const {
  return { "jpg", "jpeg", "png", "bmp", "tiff", "tif" };
}

// =============================================================================
// Method to validate if image data is ready for saving
// =============================================================================

bool UIVisionPanel::IsImageReadyForSaving() const {
  if (!m_hasImageData || m_lastImageData.empty()) {
    return false;
  }

  if (m_imageWidth <= 0 || m_imageHeight <= 0) {
    return false;
  }

  int expectedRGBSize = m_imageWidth * m_imageHeight * 3;
  int expectedGraySize = m_imageWidth * m_imageHeight;

  return (m_lastImageData.size() == expectedRGBSize ||
    m_lastImageData.size() == expectedGraySize);
}