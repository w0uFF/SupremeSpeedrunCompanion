#include "src/modules/recorder/adapters/filesystem_output.hpp"
#include <atomic>
#include <array>
#include <chrono>
#include <iostream>
#include <iterator>

using namespace supreme::recorder;
namespace st=supreme::recorder::staging;
namespace fs=std::filesystem;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
template<class F> bool throws(F f){try{f();return false;}catch(const std::exception&){return true;}}
struct Scratch {
    fs::path path;
    Scratch(){
        static std::atomic<unsigned> seq{0};
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        path=fs::temp_directory_path()/("ssc-media-test-"+std::to_string(stamp)+"-"+std::to_string(++seq));
        if(!fs::create_directory(path))throw std::runtime_error("test scratch collision");
#ifndef _WIN32
        fs::permissions(path,fs::perms::owner_all,fs::perm_options::replace);
#endif
    }
    ~Scratch(){std::error_code ec;fs::remove_all(path,ec);}
    Scratch(const Scratch&)=delete;
    Scratch& operator=(const Scratch&)=delete;
};
OutputIdentity id(char ch,std::uint64_t budget=1024){return {1,std::string(32,ch),budget};}
constexpr VideoFormat format{2,2,25,1000};
void put(const fs::path& path,std::string_view bytes){
    std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));out.close();if(!out)throw std::runtime_error("test file write");
}
std::string get(const fs::path& path){
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("test file read");
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
struct FakeCodec final:Sink {
    const fs::path path;
    const unsigned fault;
    std::ofstream out;
    FakeCodec(fs::path p,unsigned f):path(std::move(p)),fault(f){}
    void begin() override {
        if(fs::exists(path))throw std::runtime_error("test codec collision");
        out.open(path,std::ios::binary|std::ios::trunc);out<<"HEADER";out.flush();if(!out || fault==1)throw std::runtime_error("test begin");
    }
    void write(const Packet& p) override {
        out.write(reinterpret_cast<const char*>(p.bytes.data()),static_cast<std::streamsize>(p.bytes.size()));out.flush();if(!out || fault==2)throw std::runtime_error("test write");
    }
    void finalize() override {
        out<<"TAIL";out.close();if(!out || fault==3)throw std::runtime_error("test finalize");
    }
    void abort() noexcept override {out.close();}
};
struct Factory final:st::CodecFactory {
    const unsigned fault;
    explicit Factory(unsigned f=0):fault(f){}
    std::unique_ptr<Sink> make(const fs::path& p,const VideoFormat&) override {
        if(fault==4)throw std::runtime_error("test factory");
        return std::make_unique<FakeCodec>(p,fault);
    }
};
// Inject failure before or after the actual filesystem operation, preserving
// the exact state left by that stage. No mocked file bytes in recovery tests.
struct FaultStore final:st::Store {
    st::FilesystemStore& real;unsigned fault=0;
    explicit FaultStore(st::FilesystemStore& r,unsigned f):real(r),fault(f){}
    void reserve_slot(const OutputIdentity& i) override {if(fault==1)throw std::runtime_error("space unavailable");real.reserve_slot(i);if(fault==2)throw std::runtime_error("reservation interrupted");}
    bool exists(const std::string& t)const override{return real.exists(t);}
    std::optional<std::string> read(const std::string& t,st::Marker m)const override{return real.read(t,m);}
    void append(const std::string& t,st::Marker m,const std::string& s)override{
        if(m==st::Marker::intent && fault==3){real.append(t,m,s.substr(0,s.size()/2));throw std::runtime_error("torn intent");}
        if(m==st::Marker::finalized && fault==4)throw std::runtime_error("seal unavailable");
        if(m==st::Marker::finalized && fault==5){real.append(t,m,s.substr(0,s.size()/2));throw std::runtime_error("torn terminal");}
        if(m==st::Marker::incomplete && fault==6)throw std::runtime_error("incomplete unavailable");
        real.append(t,m,s);
        if(m==st::Marker::finalized && fault==7)throw std::runtime_error("seal acknowledgement lost");
    }
    std::optional<std::uint64_t> media_size(const std::string& t)const override{return real.media_size(t);}
    fs::path destination(const std::string& t)const override{return real.destination(t);}
    void flush_media(const std::string& t)override{if(fault==8)throw std::runtime_error("media flush unavailable");real.flush_media(t);}
};
struct Outcome{State state;Stats stats;};
Outcome record(st::Store& store,const OutputIdentity& identity,unsigned codec_fault=0,bool cancel=false){
    Factory factory(codec_fault);st::Provider provider(store,factory);OwnedVideoSink sink(provider,identity,format);
    Worker worker(sink,4,format.frame_bytes());
    if(worker.wait_ready()==State::running){
        if(cancel)worker.cancel();
        else {if(worker.offer({Stream::video,123,400000,std::vector<std::uint8_t>(format.frame_bytes(),42)})!=Offer::accepted)throw std::runtime_error("test offer");worker.stop();}
    }
    worker.join();return {worker.state(),worker.stats()};
}
bool finalized_and_collision(){
    Scratch s;st::FilesystemStore store(s.path);auto a=id('a');
    CHECK(st::inspect(store,a.token).status==st::Status::absent);
    auto outcome=record(store,a);CHECK(outcome.state==State::finalized && outcome.stats.written==1);
    st::FilesystemStore restarted(s.path);auto first=st::inspect(restarted,a.token);
    CHECK(first.status==st::Status::finalized_claim && first.claimed_identity && *first.claimed_identity==a && first.observed_bytes==16);
    const auto media_before=get(s.path/a.token/"video.mp4");CHECK(media_before=="HEADER******TAIL");
    const auto before=store.read(a.token,st::Marker::finalized);CHECK(before==std::optional<std::string>(st::intent(a)+"finalized\n16\n"));
    CHECK(record(restarted,a).state==State::failed);CHECK(store.read(a.token,st::Marker::finalized)==before && store.media_size(a.token)==16);
    CHECK(!store.read(a.token,st::Marker::incomplete));CHECK(get(s.path/a.token/"video.mp4")==media_before);
    auto conflicting=a;conflicting.capture=9;CHECK(record(store,conflicting).state==State::failed);
    CHECK(st::inspect(restarted,a.token).claimed_identity==first.claimed_identity && get(s.path/a.token/"video.mp4")==media_before);
    put(s.path/a.token/"video.mp4","altered size");CHECK(st::inspect(restarted,a.token).status==st::Status::review_required);
    return true;
}
bool codec_and_budget(){
    Scratch s;st::FilesystemStore store(s.path);
    for(unsigned fault=1;fault<=4;++fault){auto a=id(static_cast<char>('0'+fault));auto result=record(store,a,fault);auto v=st::inspect(store,a.token);
        CHECK(result.state==State::failed && v.status==st::Status::incomplete && !store.read(a.token,st::Marker::finalized));
        CHECK(fault==4?!v.observed_bytes:v.observed_bytes.has_value() && *v.observed_bytes>0);
    }
    auto a=id('5');CHECK(record(store,a,0,true).state==State::cancelled);CHECK(st::inspect(store,a.token).status==st::Status::incomplete && store.media_size(a.token)==6);
    a=id('6',10);CHECK(record(store,a).state==State::failed);CHECK(st::inspect(store,a.token).status==st::Status::incomplete && store.media_size(a.token)==12); // overshoot preserved
    a=id('7',14);CHECK(record(store,a).state==State::failed);CHECK(st::inspect(store,a.token).status==st::Status::incomplete && store.media_size(a.token)==16); // finalize overshoot
    return true;
}
bool interrupted_and_faults(){
    Scratch s;st::FilesystemStore store(s.path);
    for(unsigned fault=1;fault<=8;++fault){auto a=id(static_cast<char>('0'+fault));FaultStore injected(store,fault);
        CHECK(record(injected,a,fault==6?2u:0u).state==State::failed);
        st::FilesystemStore reopened(s.path);auto v=st::inspect(reopened,a.token);auto twice=st::inspect(reopened,a.token);
        CHECK(v.status==twice.status && v.claimed_identity==twice.claimed_identity && v.observed_bytes==twice.observed_bytes);
        const auto expected=fault==1?st::Status::absent:(fault==2||fault==3||fault==5||fault==7)?st::Status::review_required:fault==6?st::Status::pending:st::Status::incomplete;
        CHECK(v.status==expected);
        if(fault>=4)CHECK(v.observed_bytes && *v.observed_bytes>0);
    }
    // Simulate an abrupt process exit after the intent and media were created.
    auto a=id('a');store.reserve_slot(a);store.append(a.token,st::Marker::intent,st::intent(a));put(store.destination(a.token),"unfinished");
    CHECK(st::inspect(store,a.token).status==st::Status::pending);
    CHECK(record(store,a).state==State::failed);CHECK(st::inspect(store,a.token).status==st::Status::pending && store.media_size(a.token)==10);
    return true;
}
bool metadata_and_paths(){
    Scratch s;st::FilesystemStore store(s.path);auto a=id('a');store.reserve_slot(a);
    CHECK(st::inspect(store,a.token).status==st::Status::review_required);
    CHECK(throws([&]{store.destination("../escape");}));CHECK(throws([&]{st::inspect(store,"../escape");}));
    CHECK(throws([&]{st::FilesystemStore bad(s.path/"..");}));CHECK(throws([&]{st::FilesystemStore bad(fs::path("relative"));}));
    store.append(a.token,st::Marker::intent,st::intent(id('b')));CHECK(st::inspect(store,a.token).status==st::Status::review_required);
    CHECK(throws([&]{store.append(a.token,st::Marker::intent,st::intent(a));}));
    auto b=id('b');store.reserve_slot(b);put(s.path/b.token/"intent",std::string(st::metadata_limit+1,'x'));CHECK(st::inspect(store,b.token).status==st::Status::review_required);
    auto c=id('c');store.reserve_slot(c);store.append(c.token,st::Marker::intent,st::intent(c));store.append(c.token,st::Marker::finalized,st::intent(c)+"finalized\n1\n");CHECK(st::inspect(store,c.token).status==st::Status::review_required);
    auto d=id('d');store.reserve_slot(d);store.append(d.token,st::Marker::intent,st::intent(d));put(s.path/"outside","keep");
    fs::create_hard_link(s.path/"outside",s.path/d.token/"video.mp4");CHECK(st::inspect(store,d.token).status==st::Status::review_required);CHECK(fs::file_size(s.path/"outside")==4);
#ifndef _WIN32
    auto e=id('e');fs::create_directory_symlink(s.path/d.token,s.path/e.token);CHECK(st::inspect(store,e.token).status==st::Status::review_required);CHECK(record(store,e).state==State::failed);
    fs::create_directory_symlink(s.path,s.path/"linked");CHECK(throws([&]{st::FilesystemStore bad(s.path/"linked");}));
    auto f=id('f');store.reserve_slot(f);fs::create_symlink(s.path/"outside",s.path/f.token/"intent");CHECK(st::inspect(store,f.token).status==st::Status::review_required);CHECK(throws([&]{store.append(f.token,st::Marker::intent,st::intent(f));}));
#endif
    CHECK(!st::decode_intent("SSC-MEDIA-1\n"+a.token+"\n01\n1024\n",a.token));
    CHECK(!st::decode_intent("SSC-MEDIA-1\n"+a.token+"\n18446744073709551616\n1024\n",a.token));
    CHECK(!st::decode_intent(st::intent(a)+"extra",a.token));
    auto g=id('9');store.reserve_slot(g);store.append(g.token,st::Marker::intent,st::intent(g));put(store.destination(g.token),"x");
    store.append(g.token,st::Marker::finalized,st::intent(g)+"finalized\n01\n");CHECK(st::inspect(store,g.token).status==st::Status::review_required);
    CHECK(throws([&]{store.reserve_slot({1,"../escape",1024});}));
    return true;
}
bool concurrent_reservations(){
    Scratch s;st::FilesystemStore store(s.path);Factory f;st::Provider provider(store,f);
    std::atomic<unsigned> successes{0},failures{0};std::atomic<bool> go{false};
    auto collide=[&]{while(!go)std::this_thread::yield();try{auto l=provider.reserve(id('a'));++successes;}catch(const std::exception&){++failures;}};
    std::vector<std::thread> threads;for(unsigned i=0;i<8;++i)threads.emplace_back(collide);go=true;for(auto& t:threads)t.join();
    CHECK(successes==1 && failures==7 && st::inspect(store,id('a').token).status==st::Status::incomplete);
    std::array<Outcome,2> results{};std::thread one([&]{results[0]=record(store,id('b'));}),two([&]{results[1]=record(store,id('c'));});one.join();two.join();
    CHECK(results[0].state==State::finalized && results[1].state==State::finalized);
    CHECK(st::inspect(store,id('b').token).status==st::Status::finalized_claim && st::inspect(store,id('c').token).status==st::Status::finalized_claim);
    return true;
}
int main(){for(auto test:{finalized_and_collision,codec_and_budget,interrupted_and_faults,metadata_and_paths,concurrent_reservations})if(!test())return 1;
    std::cout<<"B-REC-04: 5 persistent media scenario groups PASS\n";return 0;}
