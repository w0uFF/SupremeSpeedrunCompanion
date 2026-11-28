#include "src/modules/recorder/public/terminal_handoff.hpp"
#include <iostream>
namespace h=supreme::recorder::handoff;
namespace a=supreme::recorder::application;
namespace r=supreme::recorder;
namespace s=supreme::session;
namespace st=supreme::recorder::staging;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
struct Backend final:a::Backend {
    bool accept=true;
    unsigned starts=0,stops=0,cancels=0;
    bool start(const a::Request&) noexcept override{++starts;return accept;}
    void stop(a::Id,a::Id) noexcept override{++stops;}
    void cancel(a::Id) noexcept override{++cancels;}
};
struct Store final:st::Store {
    r::OutputIdentity id;
    std::optional<std::string> raw,final,partial;
    std::optional<std::uint64_t> bytes;
    bool present=true;
    mutable unsigned reads=0;
    explicit Store(r::OutputIdentity v):id(std::move(v)),raw(st::intent(id)),final(st::intent(id)+"finalized\n16\n"),bytes(16){}
    void reserve_slot(const r::OutputIdentity&)override{throw std::logic_error("reserve");}
    bool exists(const std::string& token)const override{++reads;if(token!=id.token)throw std::logic_error("token");return present;}
    std::optional<std::string> read(const std::string&,st::Marker m)const override{
        ++reads;return m==st::Marker::intent?raw:m==st::Marker::finalized?final:partial;
    }
    void append(const std::string&,st::Marker,const std::string&)override{throw std::logic_error("append");}
    std::optional<std::uint64_t> media_size(const std::string&)const override{++reads;return bytes;}
    st::fs::path destination(const std::string&)const override{throw std::logic_error("destination");}
    void flush_media(const std::string&)override{throw std::logic_error("flush");}
};
struct Fixture {
    const h::AttemptKey key{{7,1},std::string(32,'a'),std::string(32,'b')};
    h::Binding binding{{10,2,a::Mode::automatic,key.identity,0},{10,std::string(32,'c'),64},{key}};
    Backend backend;
    a::Orchestrator recorder{backend};
    s::Session session{7,*supreme::categories::find("wk6y97ed"),{}};
    Store store{binding.output};
    std::optional<a::Report> report;
    std::optional<a::Completion> completion;
    std::vector<h::Accepted> accepted;
    bool event(const s::Event& e){return session.consume(e)==s::Outcome::accepted && recorder.event(10,e);}
    bool start(){return recorder.prepare(10,2,key.identity,0)==a::Decision::accepted && recorder.acquired(10,1) &&
        event({7,1,1,2,s::EventKind::start,s::Track::alpine_easy,{}});}
    bool finish(){
        if(!event({7,2,1,3,s::EventKind::reset_to_track,{},{}}) ||
           !event({7,3,1,5,s::EventKind::finish,s::Track::alpine_easy,0}) ||
           !recorder.result_visible(10,key.identity,6))return false;
        accepted.push_back({key,session.attempts().back(),0});return true;
    }
    bool complete(a::BackendEnd reason=a::BackendEnd::stopped,r::State disposition=r::State::finalized,
                  std::optional<r::State> actual=r::State::finalized,bool boundary=true){
        a::Completion c{};c.capture=10;c.reason=reason;c.state=disposition;c.worker_state=actual;c.stop_boundary_reached=boundary;
        c.stats.accepted=2;c.stats.written=2;c.quality[0].accepted=2;
        if(!c.quality[0].written.observe(0,10000)||!c.quality[0].written.observe(10000,10000))return false;
        c.high_water=1;completion.emplace(c);
        if(!recorder.completed(10,c.state,c.stats,c.quality,c.stop_boundary_reached))return false;
        report=recorder.take(10);return report.has_value();
    }
    h::Result assemble()const{return h::assemble(binding,*report,completion,accepted,st::inspect(store,binding.output.token));}
};
bool complete_and_recover(){
    Fixture f;CHECK(f.start() && f.finish() && f.complete());
    auto result=f.assemble();CHECK(result.disposition==h::Disposition::reconciled && result.snapshot);
    const auto& snap=*result.snapshot;
    CHECK(snap.report.terminal==r::State::finalized && snap.completion && snap.completion->high_water==1);
    CHECK(snap.accepted.size()==1 && snap.accepted[0].attempt.igt_ms==0 && snap.accepted[0].rta_ns==0 && snap.accepted[0].attempt.resets==1);
    CHECK(snap.media.status==st::Status::finalized_claim && snap.media.observed_bytes==16);
    CHECK(!f.recorder.take(10) && f.backend.starts==1 && f.backend.stops==1 && !f.backend.cancels);
    const auto reads=f.store.reads;
    CHECK(h::recover(snap,&f.store,false)==h::Recovery::not_quiescent && f.store.reads==reads);
    CHECK(h::recover(snap,nullptr,true)==h::Recovery::unavailable);
    CHECK(h::recover(snap,&f.store,true)==h::Recovery::matching_claim);
    f.store.bytes=17;CHECK(h::recover(snap,&f.store,true)==h::Recovery::changed_or_review);
    f.store.bytes=16;f.store.partial=st::intent(f.store.id)+"incomplete\n";CHECK(h::recover(snap,&f.store,true)==h::Recovery::changed_or_review);
    f.store.partial.reset();f.accepted[0].rta_ns=99;CHECK(snap.accepted[0].rta_ns==0);
    auto altered=snap;altered.disposition=h::Disposition::incomplete;CHECK(h::recover(altered,&f.store,true)==h::Recovery::changed_or_review);
    return true;
}
bool late_finalization_and_failure(){
    Fixture f;CHECK(f.start() && f.finish() && f.recorder.cancel(10,7));
    CHECK(f.complete(a::BackendEnd::cancelled,r::State::finalized,r::State::finalized,false));
    auto result=f.assemble();CHECK(result.snapshot && result.disposition==h::Disposition::incomplete);
    CHECK(result.snapshot->report.incomplete && result.snapshot->media.status==st::Status::finalized_claim && result.snapshot->completion->reason==a::BackendEnd::cancelled);
    CHECK(result.snapshot->completion->worker_state==r::State::finalized && !result.snapshot->completion->stop_boundary_reached);
    CHECK(h::recover(*result.snapshot,&f.store,true)==h::Recovery::incomplete);
    Fixture failed;CHECK(failed.start() && failed.finish());
    failed.store.final.reset();failed.store.partial=st::intent(failed.store.id)+"incomplete\n";
    CHECK(failed.complete(a::BackendEnd::worker_failed,r::State::failed,r::State::failed,false));
    result=failed.assemble();CHECK(result.snapshot && result.disposition==h::Disposition::incomplete && result.snapshot->completion->worker_state==r::State::failed);
    return true;
}
bool missing_and_rejection(){
    Fixture f;f.backend.accept=false;
    CHECK(f.recorder.prepare(10,2,f.key.identity,0)==a::Decision::unavailable);
    f.report=f.recorder.take(10);CHECK(f.report);f.store.present=false;
    auto result=f.assemble();CHECK(result.snapshot && result.disposition==h::Disposition::incomplete && !result.snapshot->completion && result.snapshot->accepted.empty());
    CHECK(h::recover(*result.snapshot,&f.store,true)==h::Recovery::incomplete);
    Fixture g;CHECK(g.start() && g.finish() && g.complete());
    g.accepted.clear();result=g.assemble();CHECK(result.snapshot && result.disposition==h::Disposition::incomplete && result.snapshot->accepted.empty());
    g.accepted.push_back({g.key,g.session.attempts().back(),{}});result=g.assemble();CHECK(result.snapshot && result.disposition==h::Disposition::incomplete && !result.snapshot->accepted[0].rta_ns);
    g.store.final.reset();result=g.assemble();CHECK(result.snapshot && result.snapshot->media.status==st::Status::pending && result.disposition==h::Disposition::incomplete);
    return true;
}
bool mismatch_and_bounds(){
    Fixture f;CHECK(f.start() && f.finish() && f.complete());
    auto binding=f.binding;binding.output.capture=11;CHECK(h::assemble(binding,*f.report,f.completion,f.accepted,st::inspect(f.store,f.binding.output.token)).disposition==h::Disposition::review_required);
    binding=f.binding;binding.request.target=3;CHECK(h::assemble(binding,*f.report,f.completion,f.accepted,st::inspect(f.store,f.binding.output.token)).disposition==h::Disposition::review_required);
    const auto actual=f.completion;f.completion->stats.written++;CHECK(!f.assemble().snapshot);f.completion=actual;
    f.completion->high_water=129;CHECK(!f.assemble().snapshot);f.completion=actual;
    f.completion->worker_state=r::State::running;CHECK(!f.assemble().snapshot);f.completion=actual;
    f.completion->capture=11;CHECK(!f.assemble().snapshot);f.completion=actual;
    const auto saved=f.accepted;f.accepted[0].key.run_id=std::string(32,'d');CHECK(!f.assemble().snapshot);f.accepted=saved;
    f.accepted[0].attempt.end_ms=7;CHECK(!f.assemble().snapshot);f.accepted=saved;
    f.accepted[0].attempt.track=static_cast<s::Track>(99);CHECK(!f.assemble().snapshot);f.accepted=saved;
    f.accepted[0].rta_ns=std::numeric_limits<std::uint64_t>::max();CHECK(!f.assemble().snapshot);f.accepted=saved;
    f.store.raw=st::intent({11,f.store.id.token,64});CHECK(!f.assemble().snapshot);
    binding=f.binding;binding.attempts.resize(129,f.key);CHECK(!h::valid(binding));
    return true;
}
bool manual_multi_attempt(){
    Fixture f;f.binding.request.mode=a::Mode::manual;f.binding.request.attempt.reset();
    CHECK(f.recorder.manual_start(10,2,0)==a::Decision::accepted && f.recorder.acquired(10,1));
    CHECK(f.event({7,1,1,2,s::EventKind::start,s::Track::alpine_easy,{}}) && f.finish());
    const h::AttemptKey next{{8,1},std::string(32,'d'),std::string(32,'e')};
    s::Session another(8,*supreme::categories::find("n2yzw5ek"),{});
    const s::Event start{8,1,1,10,s::EventKind::start,s::Track::alpine_medium,{}};
    CHECK(another.consume(start)==s::Outcome::accepted && f.recorder.event(10,start));
    f.binding.attempts.push_back(next);f.accepted.push_back({next,another.attempts().back(),{}});
    CHECK(f.recorder.manual_stop(10,11) && f.complete());
    const auto result=f.assemble();CHECK(result.snapshot && result.disposition==h::Disposition::incomplete && result.snapshot->accepted.size()==2);
    CHECK(result.snapshot->accepted[1].attempt.state==s::AttemptState::racing && !result.snapshot->accepted[1].attempt.end_ms && !result.snapshot->accepted[1].attempt.igt_ms);
    CHECK(result.snapshot->report.ledger.markers().back().end==supreme::capture_boundary::End::abandoned);
    auto collision=f.binding;collision.attempts[1].run_id=f.key.run_id;CHECK(!h::valid(collision));
    collision=f.binding;collision.attempts[1].session_id=f.key.session_id;CHECK(!h::valid(collision));
    return true;
}
struct Journal final:h::Journal {
    std::optional<h::Snapshot> stored;
    unsigned appends=0;
    bool fail_read=false,lose_ack=false,omit_write=false;
    std::optional<h::Snapshot> find(const std::string&) const override{
        if(fail_read)throw std::runtime_error("journal unavailable");
        return stored;
    }
    void append_new(const h::Snapshot& snapshot) override{
        ++appends;if(stored)throw std::logic_error("conflict");
        if(!omit_write)stored.emplace(snapshot);
        if(lose_ack)throw std::runtime_error("lost acknowledgement");
    }
};
bool retention_and_conflict(){
    Fixture f;CHECK(f.start() && f.finish() && f.complete());
    const auto result=f.assemble();CHECK(result.snapshot);const auto& snap=*result.snapshot;
    CHECK(h::retain(snap,nullptr)==h::Retention::unavailable);
    Journal j;j.fail_read=true;CHECK(h::retain(snap,&j)==h::Retention::unavailable && !j.appends);
    j.fail_read=false;j.lose_ack=true;CHECK(h::retain(snap,&j)==h::Retention::uncertain && j.appends==1 && j.stored);
    CHECK(h::retain(snap,&j)==h::Retention::unchanged && j.appends==1);
    auto changed=snap;changed.completion->high_water=2;
    CHECK(h::retain(changed,&j)==h::Retention::conflict && j.appends==1);
    changed=snap;changed.accepted[0].rta_ns=1;CHECK(h::retain(changed,&j)==h::Retention::conflict);
    changed=snap;changed.disposition=h::Disposition::incomplete;CHECK(h::retain(changed,&j)==h::Retention::invalid);
    Journal absent;absent.omit_write=true;CHECK(h::retain(snap,&absent)==h::Retention::uncertain && absent.appends==1);
    Journal clean;CHECK(h::retain(snap,&clean)==h::Retention::retained && clean.appends==1);
    CHECK(h::same_snapshot(*clean.stored,snap));
    return true;
}
int main(){for(auto test:{complete_and_recover,late_finalization_and_failure,missing_and_rejection,mismatch_and_bounds,manual_multi_attempt,retention_and_conflict})if(!test())return 1;
    std::cout<<"B-INT-02: 6 terminal handoff scenario groups PASS\n";return 0;}
