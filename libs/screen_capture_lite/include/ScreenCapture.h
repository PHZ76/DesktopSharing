#pragma once
#include <assert.h>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#if defined(WINDOWS) || defined(WIN32)
#if defined(SC_LITE_DLL)
#define SC_LITE_EXTERN __declspec(dllexport)
#else
#define SC_LITE_EXTERN
#endif
#else
#define SC_LITE_EXTERN
#endif

namespace SL {
    namespace Screen_Capture {
        struct Point;
        struct Monitor;
        struct Window;
        struct Image;
        struct ImageBGRA {
            unsigned char B,G,R,A;
        };

        // index to self in the GetMonitors() function
        SC_LITE_EXTERN int Index(const Monitor &mointor);
        // unique identifier
        SC_LITE_EXTERN int Id(const Monitor &mointor);
        SC_LITE_EXTERN int Adapter(const Monitor &mointor);
        SC_LITE_EXTERN int OffsetX(const Monitor &mointor);
        SC_LITE_EXTERN int OffsetY(const Monitor &mointor);
        SC_LITE_EXTERN void OffsetX(Monitor &mointor, int x);
        SC_LITE_EXTERN void OffsetY(Monitor &mointor, int y);
        SC_LITE_EXTERN int OffsetX(const Window &mointor);
        SC_LITE_EXTERN int OffsetY(const Window &mointor);
        SC_LITE_EXTERN void OffsetX(Window &mointor, int x);
        SC_LITE_EXTERN void OffsetY(Window &mointor, int y);
        SC_LITE_EXTERN const char *Name(const Monitor &mointor);
        SC_LITE_EXTERN const char *Name(const Window &mointor);
        SC_LITE_EXTERN int Height(const Monitor &mointor);
        SC_LITE_EXTERN int Width(const Monitor &mointor);
        SC_LITE_EXTERN void Height(Monitor &mointor, int h);
        SC_LITE_EXTERN void Width(Monitor &mointor, int w);
        SC_LITE_EXTERN int Height(const Window &mointor);
        SC_LITE_EXTERN int Width(const Window &mointor);
        SC_LITE_EXTERN void Height(Window &mointor, int h);
        SC_LITE_EXTERN void Width(Window &mointor, int w);
        SC_LITE_EXTERN int Height(const Image &img);
        SC_LITE_EXTERN int Width(const Image &img);
        SC_LITE_EXTERN int X(const Point &p);
        SC_LITE_EXTERN int Y(const Point &p);

        // the start of the image data, this is not guarenteed to be contiguous.
        SC_LITE_EXTERN const ImageBGRA *StartSrc(const Image &img);
        SC_LITE_EXTERN const ImageBGRA *GotoNextRow(const Image &img, const ImageBGRA* current);
        SC_LITE_EXTERN bool isDataContiguous(const Image &img);
        /*
            this is the ONLY funcion for pulling data out of the Image object and is layed out here in the header so that
            users can see how to extra data and convert it to their own needed format. Initially, I included custom extract functions
            but this is beyond the scope of this library. You must copy the image data if you want to use it as the library owns the Image Data.
        */
        inline void Extract(const Image &img, unsigned char *dst, size_t dst_size)
        {
            assert(dst_size >= static_cast<size_t>(Width(img) * Height(img) * sizeof(ImageBGRA)));
            auto startdst = dst;
            auto startsrc = StartSrc(img);
            if (isDataContiguous(img)) { // no padding, the entire copy can be a single memcpy call
                memcpy(startdst, startsrc, Width(img) * Height(img) * sizeof(ImageBGRA));
            }
            else {
                for (auto i = 0; i < Height(img); i++) {
                    memcpy(startdst, startsrc, sizeof(ImageBGRA) * Width(img));
                    startdst += sizeof(ImageBGRA) * Width(img);               // advance to the next row
                    startsrc = GotoNextRow(img, startsrc); // advance to the next row
                }
            }
        }

        class Timer {
            using Clock = std::conditional<
                std::chrono::high_resolution_clock::is_steady,
                std::chrono::high_resolution_clock,
                std::chrono::steady_clock
            >::type;

            std::chrono::microseconds Duration;
            Clock::time_point Deadline;

        public: 
            template <typename Rep, typename Period>
            Timer(const std::chrono::duration<Rep, Period> &duration)
                : Duration(std::chrono::duration_cast<std::chrono::microseconds>(duration))
                , Deadline(Clock::now() + Duration)
            {}
            void start() { Deadline = Clock::now() + Duration; }
            void wait()
            {
                const auto now = Clock::now();
                if (now < Deadline) {
                    std::this_thread::sleep_for(Deadline - now);
                }
            }
            std::chrono::microseconds duration() const { return Duration;}
        };
        // will return all attached monitors
        SC_LITE_EXTERN std::vector<Monitor> GetMonitors();
        // will return all windows
        SC_LITE_EXTERN std::vector<Window> GetWindows();

        typedef std::function<void(const SL::Screen_Capture::Image &img, const Window &window)> WindowCaptureCallback;
        typedef std::function<void(const SL::Screen_Capture::Image &img, const Monitor &monitor)> ScreenCaptureCallback;
        typedef std::function<void(const SL::Screen_Capture::Image *img, const Point &point)> MouseCallback;
        typedef std::function<std::vector<Monitor>()> MonitorCallback;
        typedef std::function<std::vector<Window>()> WindowCallback;

        class SC_LITE_EXTERN IScreenCaptureManager {
        public:
            virtual ~IScreenCaptureManager() {}

            // Used by the library to determine the callback frequency
            template <class Rep, class Period> void setFrameChangeInterval(const std::chrono::duration<Rep, Period> &rel_time)
            {
                setFrameChangeInterval(std::make_shared<Timer>(rel_time));
            }
            // Used by the library to determine the callback frequency
            template <class Rep, class Period> void setMouseChangeInterval(const std::chrono::duration<Rep, Period> &rel_time)
            {
                setMouseChangeInterval(std::make_shared<Timer>(rel_time));
            }

            virtual void setFrameChangeInterval(const std::shared_ptr<Timer> &timer) = 0;
            virtual void setMouseChangeInterval(const std::shared_ptr<Timer> &timer) = 0;

            // Will pause all capturing
            virtual void pause() = 0;
            // Will return whether the library is paused
            virtual bool isPaused() const = 0;
            // Will resume all capturing if paused, otherwise has no effect
            virtual void resume() = 0;
        };

        template <typename CAPTURECALLBACK> class ICaptureConfiguration {
        public:
            virtual ~ICaptureConfiguration() {}
            // When a new frame is available the callback is invoked
            virtual std::shared_ptr<ICaptureConfiguration<CAPTURECALLBACK>> onNewFrame(const CAPTURECALLBACK &cb) = 0;
            // When a change in a frame is detected, the callback is invoked
            virtual std::shared_ptr<ICaptureConfiguration<CAPTURECALLBACK>> onFrameChanged(const CAPTURECALLBACK &cb) = 0;
            // When a mouse image changes or the mouse changes position, the callback is invoked.
            virtual std::shared_ptr<ICaptureConfiguration<CAPTURECALLBACK>> onMouseChanged(const MouseCallback &cb) = 0;
            // start capturing
            virtual std::shared_ptr<IScreenCaptureManager> start_capturing() = 0;
        };

        // the callback of windowstocapture represents the list of monitors which should be captured. Users should return the list of monitors they want
        // to be captured
        SC_LITE_EXTERN std::shared_ptr<ICaptureConfiguration<ScreenCaptureCallback>> CreateCaptureConfiguration(const MonitorCallback &monitorstocapture);
        // the callback of windowstocapture represents the list of windows which should be captured. Users should return the list of windows they want to
        // be captured
        SC_LITE_EXTERN std::shared_ptr<ICaptureConfiguration<WindowCaptureCallback>> CreateCaptureConfiguration(const WindowCallback &windowstocapture);
    } // namespace Screen_Capture
} // namespace SL
