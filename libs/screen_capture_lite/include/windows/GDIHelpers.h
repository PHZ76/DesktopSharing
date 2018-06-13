#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Dwmapi.h>
namespace SL {
    namespace Screen_Capture {
        class HDCWrapper {
        public:
            HDCWrapper() : DC(nullptr) {}
            ~HDCWrapper() { if (DC != nullptr) { DeleteDC(DC); } }
            HDC DC;
        };
        class HBITMAPWrapper {
        public:
            HBITMAPWrapper() : Bitmap(nullptr) {}
            ~HBITMAPWrapper() { if (Bitmap != nullptr) { DeleteObject(Bitmap); } }
            HBITMAP Bitmap;
        };
        struct WindowDimensions {
            RECT ClientRect;
            RECT ClientBorder;
            WINDOWPLACEMENT Placement;
        };

        inline WindowDimensions GetWindowRect(HWND hwnd) {
            WindowDimensions ret = { 0 };
            GetWindowRect(hwnd, &ret.ClientRect);
            ret.Placement.length = sizeof(WINDOWPLACEMENT);
            GetWindowPlacement(hwnd, &ret.Placement);

            RECT frame = { 0 };
            if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame)))) {

                ret.ClientBorder.left = frame.left - ret.ClientRect.left;
                ret.ClientBorder.top = frame.top - ret.ClientRect.top;
                ret.ClientBorder.right = ret.ClientRect.right - frame.right;
                ret.ClientBorder.bottom = ret.ClientRect.bottom - frame.bottom;
            }

            ret.ClientRect.bottom -= ret.ClientBorder.bottom;
            ret.ClientRect.top += ret.ClientBorder.top;
            ret.ClientRect.left += ret.ClientBorder.left;
            ret.ClientRect.right -= ret.ClientBorder.right;

            return ret;
        }
    }
}