#pragma once
#include "internal/SCCommon.h"

namespace SL {
    namespace Screen_Capture {
        class NSFrameProcessor;
        struct NSFrameProcessorImpl;
        NSFrameProcessorImpl* CreateNSFrameProcessorImpl();
        void DestroyNSFrameProcessorImpl(NSFrameProcessorImpl*);
        void setMinFrameDuration(NSFrameProcessorImpl*, const std::chrono::microseconds& );
        void Pause_(NSFrameProcessorImpl*);
        void Resume_(NSFrameProcessorImpl*);
        
        DUPL_RETURN Init(NSFrameProcessorImpl* createdimpl, NSFrameProcessor* parent, const std::chrono::microseconds& );
        
        class NSFrameProcessor : public BaseFrameProcessor {
            NSFrameProcessorImpl* NSFrameProcessorImpl_ = nullptr;
            std::chrono::microseconds LastDuration;
        public:
            NSFrameProcessor();
            ~NSFrameProcessor();
            Monitor SelectedMonitor;
            void Pause();
            void Resume();
            
            DUPL_RETURN Init(std::shared_ptr<Thread_Data> data, Monitor& monitor);
            DUPL_RETURN ProcessFrame(const Monitor& curentmonitorinfo);
            DUPL_RETURN Init(std::shared_ptr<Thread_Data> data, Window& window);
            DUPL_RETURN ProcessFrame(const Window& window);
            
        };
    }
}
