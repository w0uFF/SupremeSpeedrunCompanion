#include "src/modules/analysis/public/comparison.hpp"
#include <array>
#include <iostream>
using namespace supreme::analysis;
using supreme::run_library::Record;
using supreme::run_library::Insert;
using supreme::run_library::Annotation;
using supreme::playback::Kind;
#define CHECK(x) do {if(!(x)){std::cerr<<__func__<<":"<<__LINE__<<" "<<#x<<'\n';return false;}}while(false)
Subject sample(char id,Origin origin,Metric metric){
    Subject s{{{std::string(32,id),"unclassified",exchange::unclassified_profile,"events-v1",std::string(64,id),origin,Status::finished,0,100},std::string(64,id),4096},{}};
    if(!record_metric(metric))s.member.emplace(Member{metric==Metric::video_interval_100ns?Kind::video:Kind::native_replay,
        metric==Metric::video_interval_100ns?"video.mp4":"race.dat",std::string(64,id),1024});
    return s;
}
struct Repo final:Repository {
    std::array<std::optional<Record>,2> rows;
    mutable unsigned reads=0;
    unsigned writes=0;
    bool fail=false;
    Insert insert(const Record&)override{++writes;throw std::logic_error("insert");}
    std::optional<Record> find(std::string_view id)const override{++reads;if(fail)throw std::runtime_error("index");return rows.at(id==std::string(32,'a')?0u:1u);}
    std::vector<Record> recent(std::size_t)const override{throw std::logic_error("scan");}
    void annotate(std::string_view,const Annotation&)override{++writes;throw std::logic_error("annotation");}
};
struct Fake final:Source {
    std::array<Observation,2> observations{};
    mutable unsigned reads=0;
    bool fail=false;
    Observation observe(const Subject& s,Metric)const override{++reads;if(fail)throw std::runtime_error("source");return observations.at(s.archive.projection.run_id==std::string(32,'a')?0u:1u);}
};
struct Fixture {
    Metric metric;
    std::array<Subject,2> subjects;
    Repo repo;
    Fake source;
    Controller controller;
    explicit Fixture(Metric m=Metric::igt_ms):metric(m),subjects{sample('a',Origin::imported,m),sample('b',Origin::synthetic,m)},controller(repo,&source){refresh();}
    void refresh(){
        for(std::size_t i=0;i<subjects.size();++i){
            const auto& s=subjects[i];repo.rows[i].emplace(s.archive.projection);
            auto& o=source.observations[i];o.slot.state=exchange::SlotState::finalized;o.slot.archive.emplace(s.archive);
            o.member=s.member;o.evidence=Evidence::established;
            o.measurement.emplace(Measurement{s,metric,Track::alpine_easy,record_metric(metric)?s.archive.projection.event_semantics:"candidate-v1",claim(s,metric).value_or(100)});
        }
    }
    Result compare()const{return controller.compare(subjects[0],subjects[1],metric);}
    void set_value(std::size_t i,std::uint64_t v){
        if(metric==Metric::igt_ms)subjects[i].archive.projection.igt_ms=v;
        if(metric==Metric::rta_ns)subjects[i].archive.projection.rta_ns=v;
        refresh();source.observations[i].measurement->value=v;
    }
};
bool claims_and_delta(){
    for(auto metric:{Metric::igt_ms,Metric::rta_ns}){
        Fixture f(metric);f.set_value(0,0);f.set_value(1,45);auto r=f.compare();
        CHECK(r.state==State::compared && r.metric==metric && r.recorded_claims && r.values);
        CHECK(r.recorded_claims->first==0 && r.recorded_claims->second==45 && r.values->second_minus_first==45);
        CHECK(same(r.values->first.bound,f.subjects[0]) && same(r.values->second.bound,f.subjects[1]));
        const auto reversed=f.controller.compare(f.subjects[1],f.subjects[0],metric);
        CHECK(reversed.values && reversed.values->second_minus_first==-45);
        CHECK(f.repo.reads==4 && f.source.reads==4 && !f.repo.writes);
        f.set_value(0,45);CHECK(f.compare().values->second_minus_first==0);
        f.subjects[0].archive.projection.igt_ms.reset();f.subjects[0].archive.projection.rta_ns.reset();f.refresh();r=f.compare();
        CHECK(r.state==State::insufficient_evidence && r.recorded_claims && !r.recorded_claims->first && r.recorded_claims->second==45 && !r.values);
    }return true;
}
bool signed_boundaries(){
    for(auto metric:{Metric::igt_ms,Metric::rta_ns}){
        Fixture f(metric);f.set_value(0,0);f.set_value(1,signed_max);auto r=f.compare();
        CHECK(r.values && r.values->second_minus_first==std::numeric_limits<std::int64_t>::max());
        r=f.controller.compare(f.subjects[1],f.subjects[0],metric);
        CHECK(r.values && r.values->second_minus_first==-std::numeric_limits<std::int64_t>::max());
        f.source.observations[0].measurement->value=signed_max+1;CHECK(f.compare().state==State::review_required);
        f.set_value(0,signed_max+1);CHECK(f.compare().state==State::invalid_request);
    }return true;
}
bool separate_media_domains(){
    for(auto metric:{Metric::video_interval_100ns,Metric::replay_candidate_ticks}){
        Fixture f(metric);f.source.observations[0].measurement->value=0;f.source.observations[1].measurement->value=120;
        auto r=f.compare();CHECK(r.state==State::compared && !r.recorded_claims && r.values && r.values->second_minus_first==120);
        CHECK(r.values->first.metric==metric && r.values->first.bound.member && r.values->first.value==0);
        const auto limit=metric==Metric::video_interval_100ns?(6ull*60*60+1)*10000000:65535ull;
        f.source.observations[1].measurement->value=limit;CHECK(f.compare().values->second_minus_first==static_cast<std::int64_t>(limit));
        f.source.observations[1].measurement->value=limit+1;CHECK(f.compare().state==State::review_required);
        f.source.observations[1].measurement->value=120;f.source.observations[1].measurement->metric=Metric::rta_ns;CHECK(f.compare().state==State::review_required);
        f.subjects[0].member->kind=metric==Metric::video_interval_100ns?Kind::native_replay:Kind::video;CHECK(f.compare().state==State::invalid_request);
    }
    Fixture f(Metric::replay_candidate_ticks);f.subjects[0].archive.bytes=10ull*1024*1024;f.subjects[0].member->bytes=8ull*1024*1024+1;
    CHECK(f.compare().state==State::invalid_request);return true;
}
bool no_implicit_authority(){
    Fixture f;auto& a=f.source.observations[0];auto& b=f.source.observations[1];
    b.measurement->track=Track::alpine_medium;auto r=f.compare();CHECK(r.state==State::incompatible && r.recorded_claims && !r.values);
    b.measurement->track=Track::alpine_easy;b.measurement->semantics="events-v2";CHECK(f.compare().state==State::review_required);
    f.subjects[1].archive.projection.event_semantics="events-v2";f.refresh();CHECK(f.compare().state==State::incompatible);
    f.subjects[1].archive.projection.event_semantics="unknown";f.refresh();CHECK(f.compare().state==State::insufficient_evidence);
    f.subjects[1].archive.projection.event_semantics="events-v1";f.subjects[0].archive.projection.status=Status::restarted;f.subjects[0].archive.projection.igt_ms.reset();f.refresh();CHECK(f.compare().state==State::insufficient_evidence);
    f.subjects[0].archive.projection.status=Status::finished;f.subjects[0].archive.projection.igt_ms=0;f.refresh();a.measurement->value=1;CHECK(f.compare().state==State::review_required);
    f.refresh();a.measurement->track=static_cast<Track>(99);CHECK(f.compare().state==State::review_required);
    f.refresh();a.measurement->semantics=std::string(129,'x');CHECK(f.compare().state==State::review_required);
    f.refresh();b.evidence=Evidence::insufficient;b.measurement.reset();r=f.compare();CHECK(r.state==State::insufficient_evidence && r.recorded_claims && !r.values);
    Fixture media(Metric::video_interval_100ns);media.source.observations[1].measurement->semantics="other-interval";CHECK(media.compare().state==State::incompatible);
    return true;
}
bool invalid_and_stale(){
    Fixture f;auto invalid=f.subjects[0];invalid.archive.projection.origin=Origin::local;
    CHECK(f.controller.compare(invalid,f.subjects[1],f.metric).state==State::unsupported_local);
    CHECK(f.controller.compare(f.subjects[0],f.subjects[0],f.metric).state==State::invalid_request);
    CHECK(f.controller.compare(f.subjects[0],f.subjects[1],static_cast<Metric>(99)).state==State::invalid_request);
    invalid=f.subjects[0];invalid.archive.zip_sha256="bad";CHECK(f.controller.compare(invalid,f.subjects[1],f.metric).state==State::invalid_request);
    invalid=f.subjects[0];invalid.member=Member{Kind::video,"v.mp4",std::string(64,'a'),1};CHECK(f.controller.compare(invalid,f.subjects[1],f.metric).state==State::invalid_request);
    CHECK(!f.repo.reads && !f.source.reads);
    f.repo.rows[1]->rta_ns=101;CHECK(f.compare().state==State::stale_selection);f.refresh();
    f.source.observations[1].slot.archive->zip_sha256=std::string(64,'f');CHECK(f.compare().state==State::stale_selection);f.refresh();
    f.source.observations[1].slot.archive->bytes++;CHECK(f.compare().state==State::stale_selection);f.refresh();
    f.source.observations[1].measurement->bound.archive.projection.manifest_sha256=std::string(64,'f');CHECK(f.compare().state==State::stale_selection);
    Fixture media(Metric::video_interval_100ns);media.source.observations[1].member->sha256=std::string(64,'f');CHECK(media.compare().state==State::stale_selection);
    media.refresh();media.source.observations[1].measurement->bound.member->bytes++;CHECK(media.compare().state==State::stale_selection);
    return true;
}
bool missing_and_errors(){
    Fixture f;Controller disabled(f.repo);CHECK(disabled.compare(f.subjects[0],f.subjects[1],f.metric).state==State::unavailable);
    f.repo.rows[0].reset();CHECK(f.compare().state==State::missing);f.refresh();
    f.repo.fail=true;CHECK(f.compare().state==State::unavailable);f.repo.fail=false;
    f.source.fail=true;CHECK(f.compare().state==State::unavailable);f.source.fail=false;
    auto& o=f.source.observations[1];
    for(auto state:{exchange::SlotState::absent,exchange::SlotState::unavailable,exchange::SlotState::pending,exchange::SlotState::alias,exchange::SlotState::review}){
        f.refresh();o.slot.state=state;if(state!=exchange::SlotState::pending && state!=exchange::SlotState::alias)o.slot.archive.reset();o.measurement.reset();o.evidence=Evidence::unavailable;
        auto r=f.compare();CHECK(r.state==(state==exchange::SlotState::absent?State::missing:state==exchange::SlotState::unavailable?State::unavailable:State::review_required) && !r.recorded_claims && !r.values);
    }
    f.refresh();o.slot.archive.reset();CHECK(f.compare().state==State::review_required);
    f.refresh();o.evidence=Evidence::established;o.measurement.reset();CHECK(f.compare().state==State::review_required);
    for(auto e:{Evidence::unavailable,Evidence::insufficient,Evidence::unsupported,Evidence::review,static_cast<Evidence>(99)}){
        f.refresh();o.evidence=e;o.measurement.reset();auto r=f.compare();CHECK(r.recorded_claims && !r.values);
        CHECK(r.state==(e==Evidence::unavailable?State::unavailable:e==Evidence::insufficient?State::insufficient_evidence:e==Evidence::unsupported?State::unsupported:State::review_required));
    }
    f.refresh();o.evidence=Evidence::unavailable;CHECK(f.compare().state==State::review_required);
    Fixture media(Metric::video_interval_100ns);auto& missing=media.source.observations[0];missing.member.reset();missing.measurement.reset();missing.evidence=Evidence::insufficient;
    CHECK(media.compare().state==State::missing);return true;
}
bool copied_read_only(){
    Fixture f;const auto original=f.subjects[0];const auto r=f.compare();CHECK(r.values && r.state==State::compared);
    f.source.observations[0].measurement->value=999;f.repo.rows[0].reset();f.subjects[0].archive.projection.igt_ms=5;
    CHECK(r.values->first.value==0 && same(r.values->first.bound,original) && r.recorded_claims->first==0 && !f.repo.writes);
    CHECK(f.compare().state==State::missing);return true;
}
int main(){for(auto test:{claims_and_delta,signed_boundaries,separate_media_domains,no_implicit_authority,invalid_and_stale,missing_and_errors,copied_read_only})if(!test())return 1;
    std::cout<<"B-ANL-01: 7 run comparison scenario groups PASS\n";return 0;}
