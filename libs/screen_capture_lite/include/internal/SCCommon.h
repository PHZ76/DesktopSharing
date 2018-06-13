#pragma once
#include "ScreenCapture.h"
#include <atomic>
#include <thread>

// this is INTERNAL DO NOT USE!
namespace SL {
    namespace Screen_Capture { 
        struct Point {
            int x;
            int y;
        };
        struct Monitor {
            int Id = INT32_MAX;
            int Index = INT32_MAX;
            int Adapter = INT32_MAX;
            int Height = 0;
            int Width = 0;
            int OriginalHeight = 0;
            int OriginalWidth = 0;
            // Offsets are the number of pixels that a monitor can be from the origin. For example, users can shuffle their
            // monitors around so this affects their offset.
            int OffsetX = 0;
            int OffsetY = 0;
            int OriginalOffsetX = 0;
            int OriginalOffsetY = 0;
            char Name[128] = { 0 };
            float Scaling = 1.0f;
        };

        struct Window {
            size_t Handle;
            Point Position;
            
            Point Size;
            // Name will always be lower case. It is converted to lower case internally by the library for comparisons
            char Name[128] = { 0 };
        };
        struct ImageRect {
            ImageRect() : ImageRect(0, 0, 0, 0) {}
            ImageRect(int l, int t, int r, int b) :left(l), top(t), right(r), bottom(b) {}
            int left;
            int top;
            int right;
            int bottom;
            bool Contains(const ImageRect &a) const { return left <= a.left && right >= a.right && top <= a.top && bottom >= a.bottom; }
        }; 
        struct Image {
            ImageRect Bounds;
            int BytesToNextRow = 0;
            bool isContiguous = false;
            // alpha is always unused and might contain garbage
            const ImageBGRA *Data = nullptr;
        };

        inline bool operator==(const ImageRect &a, const ImageRect &b)
        {
            return b.left == a.left && b.right == a.right && b.top == a.top && b.bottom == a.bottom;
        }    
        int Height(const ImageRect &rect);
        int Width(const ImageRect &rect);
        const ImageRect &Rect(const Image &img);

        template <typename F, typename M, typename W> struct CaptureData {
            std::shared_ptr<Timer> FrameTimer;
            F OnNewFrame;
            F OnFrameChanged;
            std::shared_ptr<Timer> MouseTimer;
            M OnMouseChanged;
            W getThingsToWatch;
        };
        struct CommonData {
            // Used to indicate abnormal error condition
            std::atomic<bool> UnexpectedErrorEvent;
            // Used to indicate a transition event occurred e.g. PnpStop, PnpStart, mode change, TDR, desktop switch and the application needs to recreate
            // the duplication interface
            std::atomic<bool> ExpectedErrorEvent;
            // Used to signal to threads to exit
            std::atomic<bool> TerminateThreadsEvent;
            std::atomic<bool> Paused;
        };
        struct Thread_Data {

            CaptureData<ScreenCaptureCallback, MouseCallback, MonitorCallback> ScreenCaptureData;
            CaptureData<WindowCaptureCallback, MouseCallback, WindowCallback> WindowCaptureData;
            CommonData CommonData_;
        };

        class BaseFrameProcessor {
        public:
            std::shared_ptr<Thread_Data> Data;
            std::unique_ptr<unsigned char[]> ImageBuffer;
            int ImageBufferSize = 0;
            bool FirstRun = true;
        };

        enum DUPL_RETURN { DUPL_RETURN_SUCCESS = 0, DUPL_RETURN_ERROR_EXPECTED = 1, DUPL_RETURN_ERROR_UNEXPECTED = 2 };
        Monitor CreateMonitor(int index, int id, int h, int w, int ox, int oy, const std::string &n, float scale);
        Monitor CreateMonitor(int index, int id, int adapter, int h, int w, int ox, int oy, const std::string &n, float scale);
        SC_LITE_EXTERN bool isMonitorInsideBounds(const std::vector<Monitor> &monitors, const Monitor &monitor);
        SC_LITE_EXTERN Image CreateImage(const ImageRect &imgrect, int rowpadding, const ImageBGRA *data);
        // this function will copy data from the src into the dst. The only requirement is that src must not be larger than dst, but it can be smaller
        // void Copy(const Image& dst, const Image& src);

        SC_LITE_EXTERN std::vector<ImageRect> GetDifs(const Image &oldimg, const Image &newimg);
        template <class F, class T, class C> void ProcessCapture(const F &data, T &base, const C &mointor,
            const unsigned char * startsrc,
            int srcrowstride
        ) {
            ImageRect imageract;
            imageract.left = 0;
            imageract.top = 0;
            imageract.bottom = Height(mointor);
            imageract.right = Width(mointor); 
            const auto sizeofimgbgra = static_cast<int>(sizeof(ImageBGRA));
            const auto startimgsrc = reinterpret_cast<const ImageBGRA*>(startsrc);
            auto dstrowstride = sizeofimgbgra * Width(mointor);
            if (data.OnNewFrame) {//each frame we still let the caller know if asked for
                auto wholeimg = CreateImage(imageract,  srcrowstride, startimgsrc);
                wholeimg.isContiguous = dstrowstride == srcrowstride;
                data.OnNewFrame(wholeimg, mointor);
            }
            if (data.OnFrameChanged) {//difs are needed!
                if (base.FirstRun) {
                    // first time through, just send the whole image
                    auto wholeimg = CreateImage(imageract,  srcrowstride, startimgsrc);
                    wholeimg.isContiguous = dstrowstride == srcrowstride;
                    data.OnFrameChanged(wholeimg, mointor);
                    base.FirstRun = false;
                }
                else {
                    // user wants difs, lets do it!
                    auto newimg = CreateImage(imageract,  srcrowstride - dstrowstride, startimgsrc);
                    auto oldimg = CreateImage(imageract,  0, reinterpret_cast<const ImageBGRA*>(base.ImageBuffer.get()));
                    auto imgdifs = GetDifs(oldimg, newimg);

                    for (auto &r : imgdifs) {
                        auto leftoffset = r.left * sizeofimgbgra;
                        auto thisstartsrc = startsrc +  leftoffset + (r.top * srcrowstride);

                        auto difimg = CreateImage(r, srcrowstride, reinterpret_cast<const ImageBGRA*>(thisstartsrc));
                        difimg.isContiguous = false;
                        data.OnFrameChanged(difimg, mointor);
                    }
                }
                auto startdst = base.ImageBuffer.get();
                if (dstrowstride == srcrowstride) { // no need for multiple calls, there is no padding here
                    memcpy(startdst, startsrc, dstrowstride * Height(mointor));
                }
                else {
                    for (auto i = 0; i < Height(mointor); i++) {
                        memcpy(startdst + (i * dstrowstride), startsrc + (i * srcrowstride), dstrowstride);
                    }
                }
            }
        }
    } // namespace Screen_Capture
} // namespace SL
