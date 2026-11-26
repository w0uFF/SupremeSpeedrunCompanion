#pragma once
#include "src/modules/configuration/public/attempt_settings.hpp"
#include "src/modules/configuration/public/profile_codec.hpp"

namespace supreme::exporter {
inline bool identity(std::string_view id)noexcept{
    if(id.size()!=32)return false;
    for(char c:id)if(!((c>='0' && c<='9') || (c>='a' && c<='f')))return false;
    return true;
}
// Owns a fixed snapshot and attempt identities; no pointer to mutable session state.
// Settings member is deliberately redacted and is not a proof of observed behavior.
class AttemptBinding final {
    const std::string run_,session_;
    const configuration::AttemptSettings snapshot_;
public:
    AttemptBinding(std::string run,std::string session,const configuration::AttemptSettings& snapshot):
        run_(std::move(run)),session_(std::move(session)),snapshot_(snapshot){
        if(!identity(run_) || !identity(session_))throw std::invalid_argument("attempt evidence identity");
    }
    std::string evidence_json()const{
        namespace c=configuration::codec;
        auto values=snapshot_.get().values;
        values.package_directory.clear();
        c::Value result;
        result.object.emplace(u"schema",c::Value::integer(1));
        result.object.emplace(u"scope",c::Value::text(u"attempt_settings_snapshot"));
        result.object.emplace(u"run_id",c::Value::text(c::widen(run_)));
        result.object.emplace(u"session_id",c::Value::text(c::widen(session_)));
        const auto source=snapshot_.get().source;
        result.object.emplace(u"source",c::Value::text(source==configuration::Source::launcher?u"launcher":
            source==configuration::Source::session?u"session":u"profile"));
        result.object.emplace(u"settings",c::encode_settings(values));
        std::string out;c::emit(out,result);return out;
    }
};
}
