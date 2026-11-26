#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace supreme::run_library {
enum class Origin { local, imported, synthetic };
enum class Status { finished, restarted, abandoned, failed };
enum class Method { igt, rta };
struct Record {
    std::string run_id, category, profile, event_semantics, manifest_sha256;
    Origin origin; Status status;
    std::optional<std::uint64_t> igt_ms, rta_ns;
};
struct Annotation { std::string name; bool favorite; };
enum class Insert { inserted, identical, conflict };
// Storage errors are exceptions, never silently converted to absence or conflict.
class Repository {
public:
    virtual ~Repository()=default;
    virtual Insert insert(const Record&)=0;
    virtual std::optional<Record> find(std::string_view id) const=0;
    virtual std::vector<Record> recent(std::size_t limit) const=0;
    virtual void annotate(std::string_view id,const Annotation&)=0;
};
inline bool comparable(const Record& r,const Record& reference,Method method) noexcept {
    return (method==Method::igt || method==Method::rta) && r.origin==Origin::local && r.status==Status::finished &&
        r.category==reference.category && r.profile==reference.profile && r.event_semantics==reference.event_semantics &&
        (method==Method::igt?r.igt_ms.has_value():r.rta_ns.has_value());
}
}
