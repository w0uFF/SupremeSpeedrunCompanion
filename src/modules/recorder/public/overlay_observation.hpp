#pragma once
#include "contracts/observation.hpp"
#include "src/modules/categories/public/catalog.hpp"
#include <algorithm>
#include <array>
#include <mutex>
#include <stdexcept>

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
    // Only a producer-validated loaded identity for this exact attempt. Menu
    // intent, category plan and lexical reference classification are not inputs.
    std::optional<categories::Track> validated_loaded_track;
    bool valid() const noexcept {
        if(!capture || !sequence || (!session!=!attempt) || action_count>actions.size() ||
           (origin!=Origin::unavailable && origin!=Origin::synthetic && origin!=Origin::native) ||
           static_cast<unsigned>(recording)>static_cast<unsigned>(Recording::incomplete) ||
           static_cast<unsigned>(integrity)>static_cast<unsigned>(Integrity::no_known_anomaly) ||
           (validated_loaded_track && !categories::valid(*validated_loaded_track)))return false;
        if((origin==Origin::unavailable || !attempt) && (rta_ms || igt_ms || action_count || validated_loaded_track))return false;
        for(std::size_t i=0;i<action_count;++i){
            if(!actions[i].valid())return false;
            for(std::size_t j=0;j<i;++j)if(actions[i].length==actions[j].length &&
                std::equal(actions[i].label.begin(),actions[i].label.begin()+actions[i].length,actions[j].label.begin()))return false;
        }
        return true;
    }
};
inline std::string_view track_label(const Snapshot& s) noexcept {
    if(s.origin==Origin::unavailable || !s.session || !s.attempt || !s.validated_loaded_track)return "UNKNOWN";
    for(const auto& c:categories::catalog)if(c.track && c.track==s.validated_loaded_track)return c.label;
    return "UNKNOWN";
}
// Presentation guard, not a detector or coverage evaluation. Never upgrade.
inline Integrity visible_integrity(const Snapshot& s) noexcept {
    if(s.integrity!=Integrity::no_known_anomaly)return s.integrity;
    if(s.origin==Origin::unavailable || !s.session || !s.attempt || track_label(s)=="UNKNOWN" ||
       !s.rta_ms || !s.igt_ms || !s.action_count || s.action_count>s.actions.size() ||
       s.recording==Recording::unknown || s.recording==Recording::incomplete)return Integrity::insufficient;
    for(std::size_t i=0;i<s.action_count;++i)if(!s.actions[i].value)return Integrity::insufficient;
    return s.integrity;
}
enum class Availability { missing, stale, current, ambiguous };
struct Selection { Availability availability=Availability::missing; std::optional<Snapshot> sample; };
// One history per capture token; immutable value copies, bounded synchronized access.
class History {
    struct Entry { Snapshot sample; bool tied=false,quarantined=false; };
    const std::uint64_t capture_;
    mutable std::mutex mutex_;
    std::array<Entry,256> entries_{};
    std::size_t count_=0,next_=0;
    std::uint64_t last_sequence_=0;
public:
    static constexpr std::size_t capacity=256,max_tick_group=16;
    std::uint64_t capture() const noexcept{return capture_;}
    explicit History(std::uint64_t capture):capture_(capture){if(!capture)throw std::invalid_argument("overlay capture");}
    // Producer submits ALL full replacements at one tick atomically in sequence
    // order. Exact-tick frames cannot order themselves against a tied group.
    bool publish_tick(const Snapshot* samples,std::size_t size){
        if(!samples || !size || size>max_tick_group)return false;
        for(std::size_t i=0;i<size;++i){const auto& s=samples[i];
            if(s.capture!=capture_ || !s.valid() || s.time!=samples[0].time ||
               (i && s.sequence<=samples[i-1].sequence))return false;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if(samples[0].sequence<=last_sequence_)return false;
        if(count_){auto& last=entries_[(next_+capacity-1)%capacity];
            if(samples[0].time<last.sample.time)return false;
            if(samples[0].time==last.sample.time){
                // Late extension violates the atomic group contract. Quarantine
                // until a newer full tick; do not serve the superseded identity.
                last.quarantined=true;last_sequence_=samples[size-1].sequence;return false;
            }
        }
        entries_[next_]={samples[size-1],size>1,false};last_sequence_=samples[size-1].sequence;
        next_=(next_+1)%capacity;if(count_<capacity)++count_;return true;
    }
    bool publish(const Snapshot& s){return publish_tick(&s,1);}
    Selection select(Tick time,Tick max_age) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for(std::size_t i=0;i<count_;++i){const auto& e=entries_[(next_+capacity-1-i)%capacity];const auto& s=e.sample;
            if(s.time<=time){
                if(time-s.time>max_age)return {Availability::stale,{}};
                if(e.quarantined || (e.tied && time==s.time))return {Availability::ambiguous,{}};
                auto copy=s;copy.integrity=visible_integrity(copy);return {Availability::current,copy};
            }}
        return {};
    }
};
}
