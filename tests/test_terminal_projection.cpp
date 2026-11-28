#include "src/modules/recorder/public/terminal_projection.hpp"
#include <iostream>
namespace h=supreme::recorder::handoff;
namespace p=h::projection;
namespace a=supreme::recorder::application;
namespace r=supreme::recorder;
namespace s=supreme::session;
namespace l=supreme::capture_boundary;
namespace st=supreme::recorder::staging;
using Id=a::Id;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
// Independent public-field equality, not codec equality or reconstructed Ledger.
template<class X,class Y> bool equal_public(const X& a,const Y& b){
    if(!h::same_request(a.binding.request,b.binding.request) || !(a.binding.output==b.binding.output) ||
       a.binding.attempts.size()!=b.binding.attempts.size() || a.accepted.size()!=b.accepted.size() ||
       a.disposition!=b.disposition || a.media.status!=b.media.status || a.media.observed_bytes!=b.media.observed_bytes ||
       a.media.claimed_identity.has_value()!=b.media.claimed_identity.has_value() ||
       (a.media.claimed_identity && !(*a.media.claimed_identity==*b.media.claimed_identity)))return false;
    for(std::size_t i=0;i<a.binding.attempts.size();++i)if(!h::same_key(a.binding.attempts[i],b.binding.attempts[i]))return false;
    for(std::size_t i=0;i<a.accepted.size();++i){
        const auto& x=a.accepted[i];const auto& y=b.accepted[i];const auto& v=x.attempt;const auto& w=y.attempt;
        if(!h::same_key(x.key,y.key) || x.rta_ns!=y.rta_ns ||
           std::tie(v.id,v.started_ms,v.track,v.state,v.end_ms,v.igt_ms,v.resets)!=std::tie(w.id,w.started_ms,w.track,w.state,w.end_ms,w.igt_ms,w.resets))return false;
    }
    const auto& x=a.report;const auto& y=b.report;
    if(!h::same_request(x.request,y.request) || x.ledger.id()!=y.ledger.id() || x.ledger.state()!=y.ledger.state() ||
       x.ledger.observed_menu()!=y.ledger.observed_menu() || x.attempts.size()!=y.attempts.size() ||
       std::tie(x.acquired_ms,x.stop_boundary_ms,x.stop_boundary_reached,x.incomplete,x.terminal_stats_consistent,x.terminal)!=
       std::tie(y.acquired_ms,y.stop_boundary_ms,y.stop_boundary_reached,y.incomplete,y.terminal_stats_consistent,y.terminal) || h::stats_fields(x.stats)!=h::stats_fields(y.stats))return false;
    for(std::size_t i=0;i<x.quality.size();++i)if(h::quality_fields(x.quality[i])!=h::quality_fields(y.quality[i]))return false;
    for(std::size_t i=0;i<x.attempts.size();++i){const auto& v=x.attempts[i];const auto& w=y.attempts[i];
        if(!(v.identity==w.identity) || std::tie(v.start_ms,v.sequence,v.state,v.marked,v.selection_observed,v.end_ms,v.result_ms)!=std::tie(w.start_ms,w.sequence,w.state,w.marked,w.selection_observed,w.end_ms,w.result_ms))return false;}
    const auto& lm=x.ledger.markers();const auto& rm=y.ledger.markers();if(lm.size()!=rm.size())return false;
    for(std::size_t i=0;i<lm.size();++i){const auto& v=lm[i];const auto& w=rm[i];if(std::tie(v.run_id,v.attempt_id,v.start_ms,v.end_ms,v.end)!=std::tie(w.run_id,w.attempt_id,w.start_ms,w.end_ms,w.end))return false;}
    if(a.completion.has_value()!=b.completion.has_value())return false;
    if(!a.completion)return true;
    const auto& c=*a.completion;const auto& d=*b.completion;
    if(std::tie(c.capture,c.reason,c.state,c.worker_state,c.high_water,c.stop_boundary_reached)!=std::tie(d.capture,d.reason,d.state,d.worker_state,d.high_water,d.stop_boundary_reached) || h::stats_fields(c.stats)!=h::stats_fields(d.stats))return false;
    for(std::size_t i=0;i<c.quality.size();++i)if(h::quality_fields(c.quality[i])!=h::quality_fields(d.quality[i]))return false;
    return true;
}
struct Backend final:a::Backend {
    bool accept=true;
    bool start(const a::Request&) noexcept override{return accept;}
    void stop(Id,Id) noexcept override{}
    void cancel(Id) noexcept override{}
};
std::string id(unsigned n,char prefix){std::string out(32,prefix);const char* digits="0123456789abcdef";out[30]=digits[(n>>4)&15];out[31]=digits[n&15];return out;}
constexpr Id maximum=std::numeric_limits<Id>::max(),signed_max=static_cast<Id>(std::numeric_limits<std::int64_t>::max());
struct Fixture {
    Backend backend;
    a::Orchestrator rec{backend};
    h::AttemptKey key{{7,1},std::string(32,'a'),std::string(32,'b')};
    h::Binding binding{{10,2,a::Mode::automatic,key.identity,0},{10,std::string(32,'c'),64},{key}};
    std::vector<h::Accepted> accepted;
    std::optional<a::Report> report;
    std::optional<a::Completion> completion;
    st::Inspection media{st::Status::finalized_claim,binding.output,16};
    bool automatic(bool late=false){
        s::Session session(7,*supreme::categories::find("wk6y97ed"),{});
        if(rec.prepare(10,2,key.identity,0)!=a::Decision::accepted || (!late && !rec.acquired(10,1)))return false;
        const auto event=[&](const s::Event& e){return session.consume(e)==s::Outcome::accepted && rec.event(10,e);};
        if(!event({7,1,1,2,s::EventKind::start,s::Track::alpine_easy,{}}) || (late && !rec.acquired(10,3)) ||
           !event({7,2,1,3,s::EventKind::reset_to_track,{},{}}) || !event({7,3,1,5,s::EventKind::finish,s::Track::alpine_easy,0}) || !rec.result_visible(10,key.identity,6))return false;
        accepted.push_back({key,session.attempts().back(),0});return true;
    }
    bool manual(unsigned n,bool truncate=false){
        binding.request={10,maximum,a::Mode::manual,{},maximum-1000};binding.attempts.clear();binding.output.byte_budget=64ull*1024*1024*1024;
        media={st::Status::finalized_claim,binding.output,binding.output.byte_budget};
        if(rec.manual_start(10,maximum,binding.request.requested_ms)!=a::Decision::accepted || !rec.acquired(10,binding.request.requested_ms+1))return false;
        for(unsigned i=0;i<n;++i){
            const Id session_id=maximum-255+i,attempt_id=maximum-i,ms=maximum-900+static_cast<Id>(i)*6;
            h::AttemptKey k{{session_id,attempt_id},id(i,'a'),id(i,'b')};
            s::Session session(session_id,*supreme::categories::find("jdzzy8xd"),{});
            const auto event=[&](const s::Event& e){return session.consume(e)==s::Outcome::accepted && rec.event(10,e);};
            if(!rec.selection(10,session_id,ms) || !event({session_id,maximum-2,attempt_id,ms+1,s::EventKind::start,s::Track::village_medium,{}}))return false;
            if(!(truncate && i+1==n)){
                if(!event({session_id,maximum-1,attempt_id,ms+2,s::EventKind::reset_to_track,{},{}}) ||
                   !event({session_id,maximum,attempt_id,ms+3,s::EventKind::finish,s::Track::village_medium,signed_max}) || !rec.result_visible(10,k.identity,ms+4))return false;
                accepted.push_back({k,session.attempts().back(),signed_max});
            }else accepted.push_back({k,session.attempts().back(),{}});
            binding.attempts.push_back(k);
        }
        return rec.manual_stop(10,maximum);
    }
    bool finish(a::BackendEnd reason=a::BackendEnd::stopped,r::State state=r::State::finalized,bool boundary=true,bool bad=false){
        a::Completion c{};c.capture=10;c.reason=reason;c.state=state;c.worker_state=state;c.high_water=2;c.stop_boundary_reached=boundary;
        c.stats.accepted=3;c.stats.written=3;
        c.quality[0].accepted=2;c.quality[1].accepted=1;
        if(!c.quality[0].written.observe(0,10000) || !c.quality[0].written.observe(20000,10000) || !c.quality[1].written.observe(5,20))return false;
        if(bad){c.stats.written=maximum;c.quality[0].written.end=0;c.quality[0].written.duration=maximum;}
        completion=c;
        if(!rec.completed(10,c.state,c.stats,c.quality,c.stop_boundary_reached))return false;
        report=rec.take(10);return report.has_value();
    }
    h::Result assembled()const{return h::assemble(binding,*report,completion,accepted,media);}
};
bool roundtrip(const h::Snapshot& snapshot){
    const auto projected=p::project(snapshot);CHECK(projected.value && projected.error==p::Error::none && equal_public(snapshot,*projected.value));
    const auto encoded=p::encode(*projected.value);CHECK(encoded.value && encoded.error==p::Error::none);
    const auto decoded=p::decode(*encoded.value);CHECK(decoded.value && equal_public(snapshot,*decoded.value));
    CHECK(p::encode(*decoded.value).value==encoded.value);return true;
}
bool tp01_exact_fields(){
    Fixture f;CHECK(f.automatic() && f.finish());const auto result=f.assembled();CHECK(result.snapshot && result.disposition==h::Disposition::reconciled && roundtrip(*result.snapshot));
    const auto value=p::project(*result.snapshot);CHECK(value.value);
    const auto bytes=p::encode(*value.value);CHECK(bytes.value);
    const std::string golden="SSC-TERMINAL-1\nbinding 10 2 automatic 7 1 0\noutput 10 "+std::string(32,'c')+" 64\nkeys 1\nkey 7 1 "+std::string(32,'a')+" "+std::string(32,'b')+
      "\nrequest 10 2 automatic 7 1 0\nreport 1 6 1 0 1 finalized\nstats 3 3 0 0 0\nvideo 2 20000 0 30000 2 0 0 0 0\naudio 1 20 5 25 1 0 0 0 0\nattempts 1\nattempt 7 1 2 3 finished 1 0 5 6\nledger 10 finalized 0\nmarkers 1\nmarker 7 1 2 5 finished\ncompletion 1\ndone 10 stopped finalized finalized 2 1\ndone_stats 3 3 0 0 0\ndone_video 2 20000 0 30000 2 0 0 0 0\ndone_audio 1 20 5 25 1 0 0 0 0\naccepted 1\nresult 7 1 "+std::string(32,'a')+" "+std::string(32,'b')+" 1 2 alpine_easy finished 5 0 1 0\nmedia finalized_claim 16 1\nclaim 10 "+std::string(32,'c')+" 64\ndisposition reconciled\n";
    CHECK(*bytes.value==golden);
    f.accepted[0].rta_ns.reset();const auto missing=f.assembled();CHECK(missing.snapshot && missing.disposition==h::Disposition::incomplete && roundtrip(*missing.snapshot));
    return true;
}
bool tp02_max_and_incomplete(){
    Fixture f;CHECK(f.manual(128) && f.finish());const auto result=f.assembled();CHECK(result.snapshot && result.disposition==h::Disposition::reconciled && roundtrip(*result.snapshot));
    auto v=p::project(*result.snapshot);CHECK(v.value && v.value->binding.attempts.size()==128 && v.value->report.attempts.size()==128 && v.value->report.ledger.entries.size()==128 && v.value->accepted.size()==128);
    // Synthetic accepted reset-count boundary; no claim of a million live events.
    for(auto& obs:v.value->accepted)obs.attempt.resets=1000000;
    auto bytes=p::encode(*v.value);CHECK(bytes.value);
    // Independent max-count fixture constructed from known input values, not codec output.
    const auto num=[](Id n){return std::to_string(n);};const std::string req=" 10 "+num(maximum)+" manual - - "+num(maximum-1000)+"\n";
    const std::string out=" 10 "+std::string(32,'c')+" 68719476736\n";
    std::string golden="SSC-TERMINAL-1\nbinding"+req+"output"+out+"keys 128\n";
    for(unsigned i=0;i<128;++i)golden+="key "+num(maximum-255+i)+" "+num(maximum-i)+" "+id(i,'a')+" "+id(i,'b')+"\n";
    golden+="request"+req+"report "+num(maximum-999)+" "+num(maximum)+" 1 0 1 finalized\nstats 3 3 0 0 0\nvideo 2 20000 0 30000 2 0 0 0 0\naudio 1 20 5 25 1 0 0 0 0\nattempts 128\n";
    for(unsigned i=0;i<128;++i){const Id ms=maximum-900+static_cast<Id>(i)*6;golden+="attempt "+num(maximum-255+i)+" "+num(maximum-i)+" "+num(ms+1)+" "+num(maximum)+" finished 1 1 "+num(ms+3)+" "+num(ms+4)+"\n";}
    golden+="ledger 10 finalized 1\nmarkers 128\n";
    for(unsigned i=0;i<128;++i){const Id ms=maximum-900+static_cast<Id>(i)*6;golden+="marker "+num(maximum-255+i)+" "+num(maximum-i)+" "+num(ms+1)+" "+num(ms+3)+" finished\n";}
    golden+="completion 1\ndone 10 stopped finalized finalized 2 1\ndone_stats 3 3 0 0 0\ndone_video 2 20000 0 30000 2 0 0 0 0\ndone_audio 1 20 5 25 1 0 0 0 0\naccepted 128\n";
    for(unsigned i=0;i<128;++i){const Id ms=maximum-900+static_cast<Id>(i)*6;golden+="result "+num(maximum-255+i)+" "+num(maximum-i)+" "+id(i,'a')+" "+id(i,'b')+" "+num(maximum-i)+" "+num(ms+1)+" village_medium finished "+num(ms+3)+" "+num(signed_max)+" 1000000 "+num(signed_max)+"\n";}
    golden+="media finalized_claim 68719476736 1\nclaim"+out+"disposition reconciled\n";
    CHECK(*bytes.value==golden);auto decoded=p::decode(golden);CHECK(decoded.value && equal_public(*v.value,*decoded.value));
    std::size_t largest=0,start=0;while(start<golden.size()){const auto end=golden.find('\n',start);CHECK(end!=std::string::npos);largest=std::max(largest,end+1-start);start=end+1;}
    CHECK(golden.size()<=p::max_bytes && largest<=p::max_line);
    std::cout<<"TP-02 max-count golden bytes="<<golden.size()<<" max_line="<<largest<<"\n";
    Fixture truncated;CHECK(truncated.manual(2,true) && truncated.finish());const auto t=truncated.assembled();CHECK(t.snapshot && t.disposition==h::Disposition::incomplete && t.snapshot->accepted.back().attempt.state==s::AttemptState::racing && t.snapshot->report.ledger.markers().back().end==l::End::abandoned && roundtrip(*t.snapshot));
    Fixture rejected;rejected.backend.accept=false;CHECK(rejected.rec.prepare(10,2,rejected.key.identity,0)==a::Decision::unavailable);rejected.report=rejected.rec.take(10);rejected.media={st::Status::absent,{},{}};const auto u=rejected.assembled();CHECK(u.snapshot && u.disposition==h::Disposition::incomplete && roundtrip(*u.snapshot));
    Fixture late;CHECK(late.automatic(true) && late.finish());const auto lresult=late.assembled();CHECK(lresult.snapshot && !lresult.snapshot->report.attempts[0].marked && roundtrip(*lresult.snapshot));
    Fixture cancel;CHECK(cancel.automatic() && cancel.rec.cancel(10,7) && cancel.finish(a::BackendEnd::cancelled,r::State::finalized,false));const auto c=cancel.assembled();CHECK(c.snapshot && c.disposition==h::Disposition::incomplete && roundtrip(*c.snapshot));
    Fixture startup;CHECK(startup.rec.prepare(10,2,startup.key.identity,0)==a::Decision::accepted);
    a::Completion no_worker{};no_worker.capture=10;no_worker.reason=a::BackendEnd::startup_error;no_worker.state=r::State::failed;
    CHECK(startup.rec.completed(10,no_worker.state,no_worker.stats,no_worker.quality,false));startup.report=startup.rec.take(10);startup.completion=no_worker;
    startup.media={st::Status::pending,startup.binding.output,{}};
    const auto no_worker_result=startup.assembled();CHECK(no_worker_result.snapshot && !no_worker_result.snapshot->completion->worker_state && roundtrip(*no_worker_result.snapshot));
    Fixture failed;s::Session live(7,*supreme::categories::find("wk6y97ed"),{});
    const s::Event failure_start{7,1,1,2,s::EventKind::start,s::Track::alpine_easy,{}};
    CHECK(failed.rec.prepare(10,2,failed.key.identity,0)==a::Decision::accepted && failed.rec.acquired(10,1) && live.consume(failure_start)==s::Outcome::accepted && failed.rec.event(10,failure_start));
    failed.accepted.push_back({failed.key,live.attempts().back(),{}});
    failed.media={st::Status::incomplete,failed.binding.output,16};
    CHECK(failed.finish(a::BackendEnd::worker_failed,r::State::failed,false));
    const auto failure=failed.assembled();CHECK(failure.snapshot && failure.disposition==h::Disposition::incomplete && !failure.snapshot->report.ledger.markers().front().end && roundtrip(*failure.snapshot));
    return true;
}
std::string replaced(std::string input,std::string_view from,std::string_view to){const auto at=input.find(from);if(at==std::string::npos)throw std::logic_error("fixture token not found");input.replace(at,from.size(),to);return input;}
bool tp03_malformed_and_mutations(){
    Fixture f;CHECK(f.automatic() && f.finish());const auto snap=f.assembled();CHECK(snap.snapshot);const auto v=p::project(*snap.snapshot);CHECK(v.value);const auto enc=p::encode(*v.value);CHECK(enc.value);const auto& good=*enc.value;
    const auto reject=[](const std::string& input){const auto result=p::decode(input);return !result.value && result.error!=p::Error::none;};
    CHECK(p::decode(replaced(good,"SSC-TERMINAL-1","SSC-TERMINAL-2")).error==p::Error::version);
    CHECK(p::decode(std::string(p::max_bytes+1,'x')).error==p::Error::limit);
    CHECK(reject(replaced(good,"keys 1","keys 129")) && reject(replaced(good,"attempts 1","attempts 129")) && reject(replaced(good,"markers 1","markers 129")) && reject(replaced(good,"accepted 1","accepted 129")));
    CHECK(reject(replaced(good,"binding 10","binding 010")) && reject(replaced(good,"automatic 7 1","automatic - 1")) && reject(replaced(good,"binding 10","binding 18446744073709551616")));
    CHECK(reject(replaced(good,"finalized 2 1\n","finalized 4294967296 1\n")) && reject(replaced(good,"finished 5 0 1 0\n","finished 5 0 4294967296 0\n")));
    CHECK(reject(replaced(good,"stats 3 3","stats  3 3")) && reject(replaced(good,"stats 3 3","stats 3 3\r")) && reject(good+"\n") && reject(good.substr(0,good.size()-1)) && reject(replaced(good,"stats 3 3",std::string(513,'x'))));
    CHECK(reject(replaced(good,"alpine_easy",std::string("\0",1))) && reject(replaced(good,"alpine_easy",std::string("\x80",1))) && reject(replaced(good,"alpine_easy","unknown")));
    // Mutate every serialized token independently; no token may be silently ignored.
    // Invalid replacement must refuse; a valid alternative must alter public values.
    std::size_t cursor=0,tokens=0,valid_changes=0;
    while(cursor<good.size()){
        const auto end=good.find_first_of(" \n",cursor);CHECK(end!=std::string::npos && end>cursor);
        const auto token=good.substr(cursor,end-cursor);auto broken=good;broken.replace(cursor,end-cursor,"~");CHECK(reject(broken));
        const std::string alternative=token=="0"?"1":token=="1"?"0":token=="2"?"3":token=="-"?"0":token=="finished"?"restarted":token=="alpine_easy"?"forest_medium":token=="reconciled"?"incomplete":token.size()==32?std::string(32,'f'):"2";
        auto changed=good;changed.replace(cursor,end-cursor,alternative);const auto decoded=p::decode(changed);
        if(decoded.value){CHECK(!equal_public(*v.value,*decoded.value));++valid_changes;}
        ++tokens;cursor=end+1;
    }
    CHECK(tokens==143 && valid_changes>0);std::cout<<"TP-03 tokens="<<tokens<<" valid changed values="<<valid_changes<<"\n";
    auto limit=*v.value;limit.completion->high_water=128;CHECK(p::encode(limit).value);
    limit.completion->high_water=129;CHECK(!p::encode(limit).value);
    CHECK(reject(replaced(good,"marker 7 1 2 5 finished","marker 7 1 2 - finished")) && reject(replaced(good,"report 1 6 1 0 1","report 1 6 2 0 1")));
    auto bad=*v.value;bad.report.attempts[0].state=static_cast<s::AttemptState>(99);CHECK(!p::encode(bad).value);
    bad=*v.value;bad.report.ledger.entries[0].run_id=99;CHECK(!p::encode(bad).value);
    bad=*v.value;bad.binding.attempts.resize(129,f.key);CHECK(p::encode(bad).error==p::Error::limit);
    Fixture manual;CHECK(manual.manual(2) && manual.finish());const auto ms=manual.assembled();CHECK(ms.snapshot);const auto mv=p::project(*ms.snapshot);CHECK(mv.value);bad=*mv.value;bad.binding.attempts[1].run_id=bad.binding.attempts[0].run_id;CHECK(!p::encode(bad).value);
    bad=*mv.value;std::swap(bad.report.ledger.entries[0],bad.report.ledger.entries[1]);CHECK(!p::encode(bad).value);
    return true;
}
bool tp04_safe_arithmetic(){
    Fixture bad;CHECK(bad.automatic() && bad.finish(a::BackendEnd::stopped,r::State::finalized,true,true));bad.media={st::Status::incomplete,bad.binding.output,maximum};
    const auto snap=bad.assembled();CHECK(snap.snapshot && snap.disposition==h::Disposition::incomplete && !snap.snapshot->report.terminal_stats_consistent && roundtrip(*snap.snapshot));
    auto value=p::project(*snap.snapshot);CHECK(value.value);const auto before=p::encode(*value.value);CHECK(before.value);auto recovered=p::decode(*before.value);CHECK(recovered.value && recovered.value->report.stats.written==maximum && recovered.value->report.quality[0].written.duration==maximum && recovered.value->media.observed_bytes==maximum);
    value.value->report.terminal_stats_consistent=true;CHECK(p::encode(*value.value).error==p::Error::arithmetic);
    value.value->report.terminal_stats_consistent=false;value.value->disposition=h::Disposition::reconciled;CHECK(p::encode(*value.value).error==p::Error::disposition);
    Fixture f;CHECK(f.automatic() && f.finish());const auto complete=f.assembled();CHECK(complete.snapshot);auto good=p::project(*complete.snapshot);CHECK(good.value);
    auto corrupt=*good.value;corrupt.report.quality[0].written.first=maximum;corrupt.completion->quality=corrupt.report.quality;CHECK(p::encode(corrupt).error==p::Error::arithmetic);
    corrupt=*good.value;corrupt.report.stats.accepted=maximum;corrupt.report.stats.written=0;corrupt.report.stats.discarded=maximum;corrupt.completion->stats=corrupt.report.stats;CHECK(p::encode(corrupt).error==p::Error::arithmetic);
    corrupt=*good.value;corrupt.report.stats.dropped_full=1;corrupt.report.quality[0].dropped_full=1;corrupt.report.quality[0].dropped_duration=1;corrupt.completion->stats=corrupt.report.stats;corrupt.completion->quality=corrupt.report.quality;CHECK(p::encode(corrupt).error==p::Error::arithmetic);
    return true;
}
int main(){for(auto test:{tp01_exact_fields,tp02_max_and_incomplete,tp03_malformed_and_mutations,tp04_safe_arithmetic})if(!test())return 1;std::cout<<"B-INT-04: TP-01..04 PASS\n";return 0;}
