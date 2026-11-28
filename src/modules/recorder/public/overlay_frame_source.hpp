#pragma once
#include "src/modules/recorder/public/recorded_overlay.hpp"
#include "src/modules/recorder/public/worker_backend.hpp"

namespace supreme::recorder::overlay {
// Construct in MediaFactory::open; read/destruction on the owning capture lane.
// An enabled but clipped/failed overlay fails the capture rather than silently
// continuing without requested evidence. Overlay-free recording is explicit.
class OverlayFrameSource final:public application::FrameSource {
    std::unique_ptr<application::FrameSource> source_;
    std::shared_ptr<const History> history_;
    std::unique_ptr<Renderer> renderer_;
    Composer composer_;
    static const History& checked(const std::shared_ptr<const History>& h,std::uint64_t capture){
        if(!h || h->capture()!=capture)throw std::invalid_argument("overlay history identity");
        return *h;
    }
    static Renderer& checked(const std::unique_ptr<Renderer>& r){
        if(!r)throw std::invalid_argument("overlay renderer");
        return *r;
    }
public:
    OverlayFrameSource(std::unique_ptr<application::FrameSource> source,std::shared_ptr<const History> history,
        std::unique_ptr<Renderer> renderer,std::uint64_t capture,Layout layout,std::size_t width,std::size_t height):
        source_(std::move(source)),history_(std::move(history)),renderer_(std::move(renderer)),
        composer_(checked(history_,capture),checked(renderer_),layout,width,height){
        if(!source_)throw std::invalid_argument("overlay source");
    }
    application::Read read(std::chrono::milliseconds budget) override {
        auto result=source_->read(budget);
        if(result.kind!=application::ReadKind::frame)return result;
        auto composed=composer_.compose(result.frame);
        if(composed.state==ComposeState::disabled)return result;
        if(composed.state!=ComposeState::rendered || !composed.frame)return {application::ReadKind::error,{}};
        result.frame=std::move(*composed.frame);return result;
    }
};
}
