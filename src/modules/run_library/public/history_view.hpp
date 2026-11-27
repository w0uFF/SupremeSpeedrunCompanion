#pragma once
#include "src/modules/run_library/public/record_validation.hpp"
#include <algorithm>
#include <set>
#include <utility>

namespace supreme::run_library::presentation {
// Index contents are stored claims, not current archive verification or local admission.
enum class ReadState { available, absent, unavailable, invalid_data };
struct Row {
    Record indexed;
    std::optional<Annotation> annotation;
    ReadState annotation_state=ReadState::absent;
};
struct Detail { ReadState state=ReadState::unavailable; std::optional<Row> row; };
struct HistoryQuery {
    std::size_t scan_limit=200, row_limit=50;
    std::optional<Origin> origin;
    std::optional<Status> status;
    std::optional<std::string> category;
    std::optional<Comparison> time_order; // exact namespace; raw ascending elapsed, not PB
    bool favorites_only=false;
};
struct HistoryPage {
    ReadState state=ReadState::unavailable;
    std::vector<Row> rows;
    std::size_t scanned=0,annotation_errors=0;
    bool scan_limit_reached=false,row_limit_reached=false;
};
inline bool valid_origin(Origin o) noexcept{return o==Origin::local || o==Origin::imported || o==Origin::synthetic;}
inline bool valid_status(Status s) noexcept{return s==Status::finished || s==Status::restarted || s==Status::abandoned || s==Status::failed;}
inline void validate_query(const HistoryQuery& q){
    if(!q.scan_limit || q.scan_limit>1000 || !q.row_limit || q.row_limit>100 || q.row_limit>q.scan_limit ||
       (q.origin && !valid_origin(*q.origin)) || (q.status && !valid_status(*q.status)) ||
       (q.category && !key(*q.category)))throw std::invalid_argument("history query");
    if(q.time_order){validate(*q.time_order);if(q.category && *q.category!=q.time_order->category)throw std::invalid_argument("history category conflict");}
}
class History {
    const QueryRepository& repository_;
    Row row(Record r) const {
        Row out{std::move(r),{},ReadState::absent};
        try {out.annotation=repository_.annotation(out.indexed.run_id);}
        catch(const std::exception&){out.annotation_state=ReadState::unavailable;return out;}
        if(out.annotation){
            try {validate(*out.annotation);out.annotation_state=ReadState::available;}
            catch(const std::exception&){out.annotation.reset();out.annotation_state=ReadState::invalid_data;}
        }
        return out;
    }
public:
    explicit History(const QueryRepository& repository):repository_(repository){}
    Detail find(std::string_view id) const {
        validate_id(id);
        std::optional<Record> r;
        try {r=repository_.find(id);}catch(const std::exception&){return {};}
        if(!r)return {ReadState::absent,{}};
        try {validate(*r);if(r->run_id!=id)return {ReadState::invalid_data,{}};}
        catch(const std::exception&){return {ReadState::invalid_data,{}};}
        return {ReadState::available,row(std::move(*r))};
    }
    HistoryPage recent(const HistoryQuery& q={}) const {
        validate_query(q);
        std::vector<Record> records;
        try {records=repository_.recent(q.scan_limit);}catch(const std::exception&){return {};}
        HistoryPage out;out.state=ReadState::invalid_data;
        if(records.size()>q.scan_limit)return out;
        std::set<std::string> ids;
        try {for(const auto& r:records){validate(r);if(!ids.insert(r.run_id).second)return out;}}
        catch(const std::exception&){return out;}
        out.state=ReadState::available;out.scanned=records.size();out.scan_limit_reached=records.size()==q.scan_limit;
        for(auto& r:records){
            if((q.origin && r.origin!=*q.origin) || (q.status && r.status!=*q.status) ||
               (q.category && r.category!=*q.category))continue;
            if(q.time_order && (r.category!=q.time_order->category || r.profile!=q.time_order->profile ||
                r.event_semantics!=q.time_order->event_semantics))continue;
            auto item=row(std::move(r));
            if(item.annotation_state==ReadState::invalid_data || item.annotation_state==ReadState::unavailable)++out.annotation_errors;
            if(q.favorites_only && (!item.annotation || !item.annotation->favorite))continue;
            out.rows.push_back(std::move(item));
        }
        if(q.time_order){
            const auto method=q.time_order->method;
            std::sort(out.rows.begin(),out.rows.end(),[method](const Row& a,const Row& b){
                const auto x=method==Method::igt?a.indexed.igt_ms:a.indexed.rta_ns;
                const auto y=method==Method::igt?b.indexed.igt_ms:b.indexed.rta_ns;
                if(x.has_value()!=y.has_value())return x.has_value();
                if(x!=y)return x<y;
                return a.indexed.run_id<b.indexed.run_id;
            });
        }
        out.row_limit_reached=out.rows.size()>q.row_limit;
        if(out.row_limit_reached)out.rows.resize(q.row_limit);
        return out;
    }
};
}
