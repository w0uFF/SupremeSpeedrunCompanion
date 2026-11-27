#include "src/modules/recorder/public/mf_packet.hpp"
#include <iostream>
using namespace supreme::recorder;
using namespace supreme::recorder::mf;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
template<class F> bool throws(F f){try{f();return false;}catch(const std::exception&){return true;}}
constexpr std::uint32_t low=160u*96u*3u/2u,hd=1280u*720u*3u/2u;
constexpr std::uint64_t max_pts=static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
Packet make(Stream s,std::uint32_t n,std::uint64_t pts=0,std::uint64_t duration=400000){return {s,pts,duration,std::vector<std::uint8_t>(n,128)};}
bool destination(){
    std::wstring path=L"private/recording.mp4";
    Destination d(path);path.assign(L"other.mp4");CHECK(std::wstring(d.c_str())==L"private/recording.mp4");
    CHECK(throws([]{Destination bad(L"");}));
    CHECK(throws([]{Destination bad(std::wstring(L"a\0b",3));}));return true;
}
bool packets(){
    CHECK(pcm_rate==48000 && pcm_channels==2 && pcm_align==4 && pcm_samples_per_packet==1920 && pcm_packet_bytes==7680);
    auto v=make(Stream::video,low,1234567,34567);CHECK(packet_length(v,low,false)==low && v.pts==1234567 && v.duration==34567);
    CHECK(packet_length(make(Stream::video,hd),hd,false)==hd);
    CHECK(packet_length(make(Stream::audio,pcm_packet_bytes),low,true)==pcm_packet_bytes);
    CHECK(throws([&]{packet_length(make(Stream::audio,pcm_packet_bytes),low,false);}));
    CHECK(throws([&]{packet_length(make(Stream::audio,pcm_packet_bytes-1),low,true);}));
    CHECK(throws([&]{packet_length(make(Stream::video,low-1),low,false);}));
    CHECK(throws([&]{packet_length(make(Stream::video,0),0,false);}));
    CHECK(throws([&]{packet_length(make(static_cast<Stream>(9),pcm_packet_bytes),low,true);}));
    CHECK(packet_length(make(Stream::video,low,max_pts-1,1),low,false)==low);
    CHECK(throws([&]{packet_length(make(Stream::video,low,max_pts,1),low,false);}));
    CHECK(throws([&]{packet_length(make(Stream::video,low,max_pts+1,1),low,false);}));
    CHECK(throws([&]{packet_length(make(Stream::video,low,0,0),low,false);}));return true;
}
struct FakeWriter final:Sink {
    bool audio;unsigned begin_count=0,abort_count=0,finalize_count=0;
    std::thread::id thread;std::vector<Packet> written;
    explicit FakeWriter(bool a):audio(a){}
    void begin() override {thread=std::this_thread::get_id();++begin_count;}
    void write(const Packet& p) override {
        if(thread!=std::this_thread::get_id())throw std::logic_error("thread");
        (void)packet_length(p,low,audio);written.push_back(p);
    }
    void finalize() override {if(thread!=std::this_thread::get_id())throw std::logic_error("thread");++finalize_count;}
    void abort() noexcept override {++abort_count;}
};
bool worker_handoff(){
    for(bool audio:{false,true}){
        FakeWriter sink(audio);Worker worker(sink,4,low);CHECK(worker.wait_ready()==State::running);
        CHECK(worker.offer(make(Stream::video,low,23,400000))==Offer::accepted);
        CHECK(worker.offer(make(Stream::audio,pcm_packet_bytes,23,400000))==Offer::accepted);
        worker.stop();worker.join();
        CHECK(sink.thread!=std::this_thread::get_id() && sink.begin_count==1);
        CHECK(worker.state()==(audio?State::finalized:State::failed));
        CHECK(sink.finalize_count==(audio?1u:0u) && sink.abort_count==(audio?0u:1u));
        CHECK(worker.stats().written==(audio?2u:1u) && worker.stats().discarded==(audio?0u:1u));
        CHECK(sink.written[0].pts==23 && sink.written[0].duration==400000);
        if(audio)CHECK(sink.written[1].stream==Stream::audio && sink.written[1].pts==23 && sink.written[1].duration==400000);
    }
    FakeWriter sink(false);Worker worker(sink,4,low);CHECK(worker.wait_ready()==State::running);
    worker.cancel();worker.join();CHECK(worker.state()==State::cancelled && sink.abort_count==1 && !sink.finalize_count);return true;
}
int main(){
    for(auto test:{destination,packets,worker_handoff})if(!test())return 1;
    std::cout<<"B-REC-02c: 3 portable MF packet scenario groups PASS\n";return 0;
}
