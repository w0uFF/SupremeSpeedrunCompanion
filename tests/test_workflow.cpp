#include "src/modules/submission/public/track_coverage_claims.hpp"
#include "src/modules/submission/public/track_interval_claims.hpp"
#include "src/modules/submission/public/bound_track_claims.hpp"
#include "src/modules/submission/public/track_claims.hpp"
#include "src/modules/session/public/session.hpp"
#include "src/modules/capture_boundary/public/ledger.hpp"
#include "src/modules/player_setup/public/setup.hpp"
#include "src/modules/submission/public/draft.hpp"
#include <iostream>
#include <set>
#include <stdexcept>
using namespace supreme;
using namespace session;
using categories::find;
#define CHECK(x) do { if(!(x)) throw std::runtime_error(#x); } while(false)
static constexpr std::array<Track,9> plan={Track::alpine_easy,Track::alpine_medium,Track::alpine_hard,Track::forest_easy,Track::forest_medium,Track::forest_hard,Track::village_easy,Track::village_medium,Track::village_hard};
struct Available:player_setup::Availability { bool allows(const player_setup::Loadout& l) const override { return l.rider=="rider"&&l.board=="board"; } };
static void catalog_setup() {
 std::set<std::string_view> ids; for(auto c:categories::catalog) CHECK(ids.insert(c.id).second);
 CHECK(ids.size()==11); CHECK(find("bad")==nullptr); CHECK(categories::valid_plan(plan)); auto bad=plan;bad[8]=bad[0];CHECK(!categories::valid_plan(bad));
 Available a;using namespace player_setup; CHECK(resolve(true,Loadout{"rider","board"},a)); CHECK(!resolve(false,Loadout{"rider","board"},a)); CHECK(!resolve(true,Loadout{"bad","board"},a)); CHECK(!resolve(true,{},a));
 CHECK(capture_intent(RecordingPolicy::automatic)==CaptureIntent::arm_before_selection);CHECK(capture_intent(RecordingPolicy::ask)==CaptureIntent::await_choice);CHECK(capture_intent(RecordingPolicy::ask,false)==CaptureIntent::off);CHECK(capture_intent(RecordingPolicy::ask,true)==CaptureIntent::arm_before_selection);CHECK(capture_intent(RecordingPolicy::training,true)==CaptureIntent::off);
 CHECK(proof(false,true,true,true)==Proof::absent);CHECK(proof(true,true,true,false)==Proof::incomplete);CHECK(proof(true,true,true,true)==Proof::observed_menu_and_result);
}
static void individual() {
 Session s(1,*find("wk6y97ed"),plan);
 CHECK(s.consume({2,1,1,100,EventKind::start,plan[0],{}})==Outcome::wrong_identity);
 CHECK(s.consume({1,1,1,100,EventKind::start,plan[1],{}})==Outcome::wrong_identity);
 CHECK(s.consume({1,1,1,100,EventKind::start,plan[0],{}})==Outcome::accepted);
 CHECK(s.consume({1,1,1,100,EventKind::start,plan[0],{}})==Outcome::duplicate_or_stale);
 CHECK(s.consume({1,2,1,99,EventKind::reset_to_track,{},{}})==Outcome::invalid_event);
 CHECK(s.consume({1,2,1,150,EventKind::reset_to_track,{},{}})==Outcome::accepted);
 CHECK(s.attempts().size()==1&&s.attempts()[0].resets==1);
 CHECK(s.consume({1,3,2,200,EventKind::finish,plan[0],90})==Outcome::wrong_identity);
 CHECK(s.consume({1,3,1,200,EventKind::finish,plan[0],{}})==Outcome::invalid_event);
 CHECK(s.consume({1,3,1,200,EventKind::finish,plan[0],90})==Outcome::accepted);
 CHECK(s.snapshot().state==State::completed&&s.snapshot().session_elapsed_ms==100&&s.attempts()[0].igt_ms==90);
 CHECK(s.consume({1,4,1,201,EventKind::finish,plan[0],90})==Outcome::invalid_event);
 Session r(2,*find("wk6y97ed"),plan);CHECK(r.consume({2,1,1,100,EventKind::start,plan[0],{}})==Outcome::accepted);CHECK(r.consume({2,2,1,110,EventKind::restart_race,{},{}})==Outcome::accepted);CHECK(r.snapshot().state==State::abandoned&&!r.snapshot().session_elapsed_ms);
}
static void all_tracks() {
 Session s(3,*find("zd31pr82"),plan);std::uint64_t seq=1,t=10,id=1;
 CHECK(s.consume({3,seq++,id,t,EventKind::start,plan[0],{}})==Outcome::accepted);
 CHECK(s.consume({3,seq++,id,t+1,EventKind::restart_race,{},{}})==Outcome::accepted);
 CHECK(s.snapshot().retry_observed&&s.snapshot().completed_tracks==0);
 ++id;t+=2;
 for(auto track:plan){ CHECK(s.snapshot().next_track==track); CHECK(s.consume({3,seq++,id,t,EventKind::start,track,{}})==Outcome::accepted);auto done=Event{3,seq++,id,t+10,EventKind::finish,track,9};CHECK(s.consume(done)==Outcome::accepted);CHECK(s.consume(done)==Outcome::duplicate_or_stale); ++id;t+=20; }
 CHECK(s.snapshot().completed_tracks==9&&s.snapshot().state==State::completed&&!s.snapshot().next_track);CHECK(s.attempts().size()==10);CHECK(s.snapshot().session_elapsed_ms==172);
}
static void championship_and_limits() {
 Session c(4,*find("zdn54jqd"),plan);CHECK(c.consume({4,1,0,1,EventKind::championship_victory,{},{}})==Outcome::invalid_event);
 CHECK(c.consume({4,1,1,1,EventKind::start,plan[0],{}})==Outcome::accepted);CHECK(c.consume({4,2,0,2,EventKind::championship_victory,{},{}})==Outcome::invalid_event);CHECK(c.consume({4,2,1,2,EventKind::finish,plan[0],1})==Outcome::accepted);CHECK(c.snapshot().state==State::awaiting_race);CHECK(c.consume({4,3,0,3,EventKind::championship_victory,{},{}})==Outcome::accepted);
 Session s(5,*find("zd31pr82"),plan);for(std::uint64_t i=1;i<=Session::max_attempts;++i){CHECK(s.consume({5,2*i-1,i,2*i,EventKind::start,plan[0],{}})==Outcome::accepted);CHECK(s.consume({5,2*i,i,2*i+1,EventKind::restart_race,{},{}})==Outcome::accepted);}CHECK(s.consume({5,257,129,258,EventKind::start,plan[0],{}})==Outcome::limit_reached);CHECK(s.consume({5,257,0,258,EventKind::abandon,{},{}})==Outcome::accepted);CHECK(s.snapshot().state==State::abandoned);
}
static void submission_review() {
 using namespace submission; Draft d;CHECK(review(d,false)==Decision::incomplete_run);d.completed=true;CHECK(review(d,false)==Decision::unknown_category);d.category_id="wk6y97ed";CHECK(review(d,false)==Decision::missing_evidence);d.proof=player_setup::Proof::observed_menu_and_result;CHECK(review(d,false)==Decision::unresolved_rules);d.rules_confirmed=true;d.official_method=TimingMethod::ingame;CHECK(review(d,false)==Decision::missing_time);d.igt_ms=1000;CHECK(review(d,false)==Decision::missing_video);d.video_url="https://youtu.be/abc_DEF-123";CHECK(review(d,false)==Decision::awaiting_confirmation);CHECK(review(d,true)==Decision::offline_only);
 CHECK(!plausible_video_link("https://youtu.be.evil/a"));CHECK(!plausible_video_link("https://youtu.be/"));CHECK(!plausible_video_link("https://youtu.be/a?token=secret"));CHECK(!plausible_video_link("https://youtu.be/a\nb"));CHECK(!plausible_video_link("http://youtu.be/a"));
}
static void track_claims_review() {
 using namespace submission;
 using categories::Track;
 TrackClaims c;
 CHECK(compare_track_claims(c)==TrackClaimStatus::incomplete);
 c={Track::alpine_easy,Track::alpine_easy,Track::alpine_easy,Track::alpine_easy,std::nullopt,false,true};
 CHECK(compare_track_claims(c)==TrackClaimStatus::consistent_claims);
 c.replay_expected=true;CHECK(compare_track_claims(c)==TrackClaimStatus::incomplete);
 c.replay=Track::alpine_hard;CHECK(compare_track_claims(c)==TrackClaimStatus::conflicting);
 c.replay=Track::alpine_easy;CHECK(compare_track_claims(c)==TrackClaimStatus::consistent_claims);
 c.video_coverage_claimed=false;CHECK(compare_track_claims(c)==TrackClaimStatus::incomplete);
 c.finish=Track::forest_easy;CHECK(compare_track_claims(c)==TrackClaimStatus::conflicting);
 c.finish=static_cast<Track>(99);CHECK(compare_track_claims(c)==TrackClaimStatus::malformed);
 c.finish=Track::alpine_easy;c.video_coverage_claimed=true;
 // Claim agreement does not bypass the existing main-menu proof requirement.
 Draft d;d.completed=true;d.category_id="wk6y97ed";d.rules_confirmed=true;
 d.official_method=TimingMethod::ingame;d.igt_ms=1000;d.video_url="https://youtu.be/abc";
 CHECK(compare_track_claims(c)==TrackClaimStatus::consistent_claims);
 CHECK(review(d,true)==Decision::missing_evidence);
}
static void bound_track_claims_review() {
 using namespace submission;
 const TrackAttemptKey k{std::string(32,'a'),std::string(32,'b'),2,1};
 const BoundTrackClaim claim{k,Track::alpine_easy};
 BoundTrackClaims c{k,Track::alpine_easy,claim,claim,claim,claim,std::nullopt,false,true};
 CHECK(compare_bound_track_claims(c)==TrackClaimStatus::consistent_claims);
 c.video_label->key.attempt=1;CHECK(compare_bound_track_claims(c)==TrackClaimStatus::conflicting);
 c.video_label=claim;c.manifest->key.leg=2;CHECK(compare_bound_track_claims(c)==TrackClaimStatus::conflicting);
 c.manifest=claim;c.finish->key.session_id=std::string(32,'c');CHECK(compare_bound_track_claims(c)==TrackClaimStatus::conflicting);
 c.finish=claim;c.start->key.run_id=std::string(32,'c');CHECK(compare_bound_track_claims(c)==TrackClaimStatus::conflicting);
 c.start=claim;c.category_track=Track::alpine_hard;CHECK(compare_bound_track_claims(c)==TrackClaimStatus::conflicting);
 c.category_track=Track::alpine_easy;c.replay_expected=true;CHECK(compare_bound_track_claims(c)==TrackClaimStatus::incomplete);
 c.replay=claim;c.replay->track.reset();CHECK(compare_bound_track_claims(c)==TrackClaimStatus::incomplete);
 c.replay=claim;c.replay->key.attempt=0;CHECK(compare_bound_track_claims(c)==TrackClaimStatus::malformed);
 c.replay=claim;c.start->key.leg=2;c.finish->track=static_cast<Track>(99);
 CHECK(compare_bound_track_claims(c)==TrackClaimStatus::malformed);
 c.start=claim;c.finish=claim;c.video_coverage_claimed=false;CHECK(compare_bound_track_claims(c)==TrackClaimStatus::incomplete);
 c.video_coverage_claimed=true;CHECK(compare_bound_track_claims(c)==TrackClaimStatus::consistent_claims);
 Draft d;d.completed=true;d.category_id="wk6y97ed";d.rules_confirmed=true;
 d.official_method=TimingMethod::ingame;d.igt_ms=1000;d.video_url="https://youtu.be/abc";
 CHECK(review(d,true)==Decision::missing_evidence);
}
static void track_interval_claims_review() {
 using namespace submission;
 const TrackAttemptKey key{std::string(32,'a'),std::string(32,'b'),2,1};
 const BoundTrackClaim track{key,Track::alpine_easy};
 BoundTrackClaims claims{key,Track::alpine_easy,track,track,track,track,std::nullopt,false,true};
 const ClaimClock clock{std::string(32,'c'),10000000};
 AttemptIntervalClaim a{key,clock,1,2,3,10,20,30};
 VideoIntervalClaim v{key,clock,0,30};
 CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::consistent_claims);
 v.begin_tick=11;CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::incomplete);
 v.begin_tick=0;v.end_tick=29;CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::incomplete);
 v.end_tick=30;v.clock.frequency=1;CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::incomplete);
 v.clock=clock;v.clock.epoch=std::string(32,'d');CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::incomplete);
 v.clock=clock;v.key.attempt=1;CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::conflicting);
 v.key=key;a.finish_sequence=1;CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::malformed);
 a.finish_sequence=2;a.finish_tick=9;CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::malformed);
 a.finish_tick=20;CHECK(compare_track_interval_claims(claims,a,std::nullopt)==TrackClaimStatus::incomplete);
 claims.video_coverage_claimed=false;CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::incomplete);
 claims.video_coverage_claimed=true;CHECK(compare_track_interval_claims(claims,a,v)==TrackClaimStatus::consistent_claims);
 Draft d;d.completed=true;d.category_id="wk6y97ed";d.rules_confirmed=true;
 d.official_method=TimingMethod::ingame;d.igt_ms=1000;d.video_url="https://youtu.be/abc";
 CHECK(review(d,true)==Decision::missing_evidence);
}
static void track_coverage_claims_review() {
 using namespace submission;
 const TrackAttemptKey key{std::string(32,'a'),std::string(32,'b'),2,1};
 const BoundTrackClaim track{key,Track::alpine_easy};
 const BoundTrackClaims claims{key,Track::alpine_easy,track,track,track,track,std::nullopt,false,true};
 const ClaimClock clock{std::string(32,'c'),10000000};
 const AttemptIntervalClaim a{key,clock,2,3,4,10,20,30};
 const VideoIntervalClaim v{key,clock,0,40};
 TrackCoverageClaim c{key,clock,1,5,5,35,{{0,12},{12,25},{25,40}}};
 CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::consistent_claims);
 c.spans[1].begin_tick=13;CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::incomplete);
 c.spans[1].begin_tick=11;CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::malformed);
 c.spans[1].begin_tick=12;c.spans[2].end_tick=34;CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::incomplete);
 c.spans[2].end_tick=41;CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::malformed);
 c.spans[2].end_tick=40;c.pre_start_sequence=2;CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::malformed);
 c.pre_start_sequence=1;c.result_visible_end_tick=30;CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::malformed);
 c.result_visible_end_tick=35;c.clock.frequency=1;CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::incomplete);
 c.clock=clock;c.key.leg=2;CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::conflicting);
 c.key=key;c.spans.clear();CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::incomplete);
 c.spans.resize(max_coverage_spans+1,{0,1});CHECK(compare_track_coverage_claims(claims,a,v,c)==TrackClaimStatus::malformed);
 CHECK(compare_track_coverage_claims(claims,a,v,std::nullopt)==TrackClaimStatus::incomplete);
 Draft d;d.completed=true;d.category_id="wk6y97ed";d.rules_confirmed=true;
 d.official_method=TimingMethod::ingame;d.igt_ms=1000;d.video_url="https://youtu.be/abc";
 CHECK(review(d,true)==Decision::missing_evidence);
}
static void capture_boundaries() {
 using namespace capture_boundary;
 Ledger a(Mode::automatic,1);CHECK(a.id()==1);CHECK(!a.selection_observed(1,1));CHECK(a.acquired(10));CHECK(!a.acquired(11));CHECK(a.selection_observed(1,12));CHECK(a.begin(1,1,13));CHECK(!a.close(14,false));CHECK(a.end(1,1,End::restarted,15));CHECK(!a.close(16,false));CHECK(!a.begin(2,1,16));CHECK(a.begin(1,2,16));CHECK(a.end(1,2,End::finished,20));CHECK(!a.close(21,false));CHECK(a.result_observed(1,2,22));CHECK(a.close(23,false));CHECK(a.state()==capture_boundary::State::finalizing);CHECK(a.finalized());CHECK(!a.finalized());CHECK(a.markers().size()==2&&a.observed_menu());
 Ledger manual(Mode::manual,2);CHECK(manual.acquired(0));CHECK(manual.selection_observed(1,1));CHECK(manual.begin(1,1,2));CHECK(manual.end(1,1,End::finished,3));CHECK(!manual.close(4,false));CHECK(manual.begin(2,1,5));CHECK(!manual.observed_menu());CHECK(manual.end(2,1,End::abandoned,6));CHECK(!manual.selection_observed(2,7));CHECK(manual.close(7,true));CHECK(manual.finalized());
 Ledger failed(Mode::automatic,3);CHECK(failed.acquired(1));failed.failure();CHECK(failed.state()==capture_boundary::State::failed&&!failed.begin(3,1,2));
 Ledger old(Mode::automatic,4);CHECK(old.acquired(1));CHECK(old.begin(4,1,2));CHECK(old.end(4,1,End::restarted,3));CHECK(old.close(4,false,true));Ledger fresh(Mode::automatic,5);CHECK(fresh.acquired(4));CHECK(fresh.selection_observed(5,5));CHECK(fresh.begin(5,1,6));CHECK(old.state()==capture_boundary::State::finalizing&&fresh.state()==capture_boundary::State::acquiring);
}
int main(){try{catalog_setup();std::cout<<"catalog_setup PASS\n";individual();std::cout<<"individual PASS\n";all_tracks();std::cout<<"all_tracks PASS\n";championship_and_limits();std::cout<<"championship_and_limits PASS\n";submission_review();std::cout<<"submission_review PASS\n";track_claims_review();std::cout<<"track_claims_review PASS\n";bound_track_claims_review();std::cout<<"bound_track_claims_review PASS\n";track_interval_claims_review();std::cout<<"track_interval_claims_review PASS\n";track_coverage_claims_review();std::cout<<"track_coverage_claims_review PASS\n";capture_boundaries();std::cout<<"capture_boundaries PASS\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
