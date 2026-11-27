#pragma once
#include "contracts/observation.hpp"
#include "src/modules/recorder/public/pixel_conversion.hpp"
#include "src/modules/recorder/public/timed_frames.hpp"
#include <array>
#include <mutex>
#include <string>

namespace supreme::recorder::overlay {
using Tick=std::uint64_t; // 100 ns from the same epoch as TimedFrame.time
struct Action {
    std::array<char,8> label{}; // producer-supplied mapped action, not physical key
    std::uint8_t length=0;
    bool analog=false;
    std::optional<std::int16_t> value; // digital 0/1; analog signed permille
    bool valid() const noexcept {
        if(!length || length>label.size())return false;
        for(std::size_t i=0;i<length;++i){const char c=label[i];if(!((c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-'))return false;}
        return !value || (analog?*value>=-1000 && *value<=1000:*value==0 || *value==1);
    }
};
enum class Recording { unknown, acquiring, stopping, incomplete };
enum class Integrity { unknown, insufficient, anomaly, violation, no_known_anomaly };
// Feature-local presentation snapshot, NOT a new engine event or verification contract.
// Full replacement on every publish, including transitions and unavailable fields.
struct Snapshot {
    std::uint64_t capture=0,sequence=0;
    Tick time=0;
    Origin origin=Origin::unavailable;
    std::uint64_t session=0,attempt=0;
    std::optional<std::uint64_t> rta_ms,igt_ms;
    std::array<Action,8> actions{};
    std::size_t action_count=0;
    Recording recording=Recording::unknown;
    Integrity integrity=Integrity::unknown;
    bool valid() const noexcept {
        if(!capture || !sequence || (!session!=!attempt) || action_count>actions.size() ||
           (origin!=Origin::unavailable && origin!=Origin::synthetic && origin!=Origin::native) ||
           static_cast<unsigned>(recording)>static_cast<unsigned>(Recording::incomplete) ||
           static_cast<unsigned>(integrity)>static_cast<unsigned>(Integrity::no_known_anomaly))return false;
        if((origin==Origin::unavailable || !attempt) && (rta_ms || igt_ms || action_count))return false;
        for(std::size_t i=0;i<action_count;++i){
            if(!actions[i].valid())return false;
            for(std::size_t j=0;j<i;++j)if(actions[i].length==actions[j].length &&
                std::equal(actions[i].label.begin(),actions[i].label.begin()+actions[i].length,actions[j].label.begin()))return false;
        }
        return true;
    }
};
enum class Availability { missing, stale, current };
struct Selection { Availability availability=Availability::missing; std::optional<Snapshot> sample; };
// One history per capture token; immutable value copies, bounded synchronized access.
class History {
    const std::uint64_t capture_;
    mutable std::mutex mutex_;
    std::array<Snapshot,256> entries_{};
    std::size_t count_=0,next_=0;
public:
    static constexpr std::size_t capacity=256;
    std::uint64_t capture() const noexcept{return capture_;}
    explicit History(std::uint64_t capture):capture_(capture){if(!capture)throw std::invalid_argument("overlay capture");}
    bool publish(const Snapshot& s){
        if(s.capture!=capture_ || !s.valid())return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if(count_){const auto& last=entries_[(next_+capacity-1)%capacity];if(s.sequence<=last.sequence || s.time<=last.time)return false;}
        entries_[next_]=s;next_=(next_+1)%capacity;if(count_<capacity)++count_;return true;
    }
    Selection select(Tick time,Tick max_age) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for(std::size_t i=0;i<count_;++i){const auto& s=entries_[(next_+capacity-1-i)%capacity];
            if(s.time<=time){if(time-s.time>max_age)return {Availability::stale,{}};return {Availability::current,s};}}
        return {};
    }
};
struct Layout {
    bool enabled=false;
    std::size_t width=480,height=128,x=8,y=8;
    Tick max_age=2500000; // provisional 250 ms, no interpolation or integrity threshold
    bool valid() const noexcept {return width>=2 && width<=640 && height>=2 && height<=160 && !(width%2) && !(height%2) && !(x%2) && !(y%2) && x<=4096 && y<=4096 && max_age && max_age<=10000000;}
};
// Bounded BGRA patch. Renderer can only write inside this surface.
class Canvas {
    const std::size_t width_,height_;
    std::vector<std::uint8_t> bytes_;
public:
    Canvas(std::size_t w,std::size_t h):width_(w),height_(h){
        if(!w || !h || w>640 || h>160)throw std::invalid_argument("overlay canvas");
        bytes_.resize(w*h*4);
    }
    std::size_t width()const noexcept{return width_;}
    std::size_t height()const noexcept{return height_;}
    const std::vector<std::uint8_t>& bytes()const noexcept{return bytes_;}
    void pixel(std::size_t x,std::size_t y,std::uint8_t gray) noexcept {
        if(x>=width_ || y>=height_)return;
        auto* p=bytes_.data()+(y*width_+x)*4;p[0]=gray;p[1]=gray;p[2]=gray;p[3]=255;
    }
};
class Renderer {
public:
    virtual ~Renderer()=default;
    // Return false if any requested content did not fit. No reference retention.
    virtual bool draw(Canvas&,const Selection&)=0;
};
class BitmapRenderer final:public Renderer {
    using Glyph=std::array<std::uint8_t,7>;
    static Glyph glyph(char c) noexcept {
        switch(c){
        case '0':return {14,17,19,21,25,17,14}; case '1':return {4,12,4,4,4,4,14};
        case '2':return {14,17,1,2,4,8,31}; case '3':return {30,1,1,14,1,1,30};
        case '4':return {2,6,10,18,31,2,2}; case '5':return {31,16,16,30,1,1,30};
        case '6':return {14,16,16,30,17,17,14}; case '7':return {31,1,2,4,8,8,8};
        case '8':return {14,17,17,14,17,17,14}; case '9':return {14,17,17,15,1,1,14};
        case 'A':return {14,17,17,31,17,17,17}; case 'B':return {30,17,17,30,17,17,30};
        case 'C':return {14,17,16,16,16,17,14}; case 'D':return {30,17,17,17,17,17,30};
        case 'E':return {31,16,16,30,16,16,31}; case 'F':return {31,16,16,30,16,16,16};
        case 'G':return {14,17,16,23,17,17,15}; case 'H':return {17,17,17,31,17,17,17};
        case 'I':return {14,4,4,4,4,4,14}; case 'J':return {7,2,2,2,18,18,12};
        case 'K':return {17,18,20,24,20,18,17}; case 'L':return {16,16,16,16,16,16,31};
        case 'M':return {17,27,21,21,17,17,17}; case 'N':return {17,25,21,19,17,17,17};
        case 'O':return {14,17,17,17,17,17,14}; case 'P':return {30,17,17,30,16,16,16};
        case 'Q':return {14,17,17,17,21,18,13}; case 'R':return {30,17,17,30,20,18,17};
        case 'S':return {15,16,16,14,1,1,30}; case 'T':return {31,4,4,4,4,4,4};
        case 'U':return {17,17,17,17,17,17,14}; case 'V':return {17,17,17,17,17,10,4};
        case 'W':return {17,17,17,21,21,21,10}; case 'X':return {17,17,10,4,10,17,17};
        case 'Y':return {17,17,10,4,4,4,4}; case 'Z':return {31,1,2,4,8,16,31};
        case '-':return {0,0,0,31,0,0,0}; case ':':return {0,4,4,0,4,4,0};
        case '/':return {1,2,2,4,8,8,16}; case '.':return {0,0,0,0,0,4,4};
        case ' ':return {}; default:return {14,17,1,2,4,0,4};
        }
    }
    static bool line(Canvas& c,std::size_t y,std::string_view text){
        if(y+7>c.height() || c.width()<4 || text.size()>(c.width()-4)/6)return false;
        std::size_t x=2;
        for(char ch:text){const auto g=glyph(ch);for(std::size_t row=0;row<7;++row)for(std::size_t col=0;col<5;++col)if(g[row]&(16u>>col))c.pixel(x+col,y+row,255);x+=6;}
        return true;
    }
    static std::string timer(const std::optional<std::uint64_t>& ms){
        if(!ms)return "UNKNOWN";
        const auto sec=*ms/1000;auto frac=std::to_string(*ms%1000+1000).substr(1);
        return std::to_string(sec)+"."+frac+" S";
    }
public:
    bool draw(Canvas& c,const Selection& v) override {
        std::size_t y=1;bool fit=true;
        auto put=[&](std::string_view t){if(!line(c,y,t)){fit=false;(void)line(c,y,"CLIPPED");}y+=8;};
        if(v.availability!=Availability::current || !v.sample){put(v.availability==Availability::stale?"OBS STALE":"OBS UNKNOWN");return fit;}
        const auto& s=*v.sample;
        put(s.origin==Origin::synthetic?"SYNTHETIC":s.origin==Origin::native?"NATIVE OBS":"OBS UNKNOWN");
        put("RTA: "+timer(s.rta_ms));put("IGT: "+timer(s.igt_ms));
        put(s.attempt?"SESSION: "+std::to_string(s.session):"SESSION UNKNOWN");
        put(s.attempt?"ATTEMPT: "+std::to_string(s.attempt):"ATTEMPT UNKNOWN");
        constexpr std::array<std::string_view,4> recording{"REC UNKNOWN","REC ACQUIRING","REC STOPPING","REC INCOMPLETE"};
        constexpr std::array<std::string_view,5> integrity{"CHECK UNKNOWN","CHECK INSUFFICIENT","CHECK ANOMALY","CHECK VIOLATION","NO KNOWN ANOMALY"};
        put(recording.at(static_cast<std::size_t>(s.recording)));put(integrity.at(static_cast<std::size_t>(s.integrity)));
        if(!s.action_count)put("INPUT UNKNOWN");
        for(std::size_t i=0;i<s.action_count;++i){const auto& a=s.actions[i];
            std::string value=a.value?(a.analog?std::to_string(*a.value)+"/1000":(*a.value?"ON":"OFF")):"UNKNOWN";
            put(std::string(a.label.data(),a.length)+": "+value);
        }
        return fit;
    }
};
enum class ComposeState { disabled, rendered, clipped, failed, invalid_frame };
struct Composition { ComposeState state; Availability availability=Availability::missing; std::optional<TimedFrame> frame; };
// Capture-lane only. Source frame is immutable; replacement is published only after success.
class Composer {
    const History& history_;
    Renderer& renderer_;
    const Layout layout_;
    const std::size_t width_,height_;
public:
    Composer(const History& h,Renderer& r,Layout l,std::size_t w,std::size_t height):history_(h),renderer_(r),layout_(l),width_(w),height_(height){
        if(!l.valid() || !w || !height || w>4096 || height>4096 || w%2 || height%2 || w*height*3/2>16u*1024u*1024u)throw std::invalid_argument("overlay layout");
    }
    Composition compose(const TimedFrame& source) const noexcept {
        if(!layout_.enabled)return {ComposeState::disabled,Availability::missing,{}};
        if(source.pixels.size()!=width_*height_*3/2)return {ComposeState::invalid_frame,Availability::missing,{}};
        if(layout_.x>=width_ || layout_.y>=height_)return {ComposeState::clipped,Availability::missing,{}};
        try {
            const auto v=history_.select(source.time,layout_.max_age);
            const auto w=std::min(layout_.width,width_-layout_.x),h=std::min(layout_.height,height_-layout_.y);
            Canvas canvas(w,h);const bool fit=renderer_.draw(canvas,v);
            auto patch=bgra_to_nv12(canvas.bytes().data(),canvas.bytes().size(),w*4,w,h);
            TimedFrame output=source;
            for(std::size_t y=0;y<h;++y)std::copy_n(patch.data()+y*w,w,output.pixels.data()+(layout_.y+y)*width_+layout_.x);
            for(std::size_t y=0;y<h/2;++y)std::copy_n(patch.data()+w*h+y*w,w,output.pixels.data()+width_*height_+(layout_.y/2+y)*width_+layout_.x);
            return {fit && w==layout_.width && h==layout_.height?ComposeState::rendered:ComposeState::clipped,v.availability,std::move(output)};
        }catch(...){return {ComposeState::failed,Availability::missing,{}};}
    }
};
}
