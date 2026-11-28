#pragma once
#include "src/modules/recorder/public/owned_video_sink.hpp"
#include <charconv>
#include <filesystem>
#include <optional>
#include <string_view>

namespace supreme::recorder::staging {
namespace fs=std::filesystem;
enum class Marker { intent, incomplete, finalized };
inline constexpr std::size_t metadata_limit=128;
inline bool valid_token(const std::string& token){return token.size()==32 && OutputIdentity{1,token,1}.valid();}
inline std::string intent(const OutputIdentity& id){
    if(!id.valid())throw std::invalid_argument("media identity");
    return "SSC-MEDIA-1\n"+id.token+"\n"+std::to_string(id.capture)+"\n"+std::to_string(id.byte_budget)+"\n";
}
inline std::optional<std::uint64_t> number(std::string_view s){
    if(s.empty())return {};
    std::uint64_t n=0;const auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    if(r.ec!=std::errc{} || r.ptr!=s.data()+s.size() || std::to_string(n)!=s)return {};
    return n;
}
inline std::optional<OutputIdentity> decode_intent(std::string_view s,const std::string& token){
    if(!valid_token(token) || s.size()>metadata_limit)return {};
    const std::string prefix="SSC-MEDIA-1\n"+token+"\n";
    if(s.substr(0,prefix.size())!=prefix)return {};
    s.remove_prefix(prefix.size());const auto split=s.find('\n');
    if(split==std::string_view::npos || split+1>=s.size() || s.back()!='\n')return {};
    const auto capture=number(s.substr(0,split));const auto budget=number(s.substr(split+1,s.size()-split-2));
    if(!capture || !budget)return {};
    OutputIdentity id{*capture,token,*budget};return id.valid()?std::optional<OutputIdentity>(std::move(id)):std::nullopt;
}
// Narrow recorder-specific store. Concurrent exclusive creation for distinct or
// colliding tokens; all other operations require the owned slot or quiescence.
// Missing files are absence, unsafe/unreadable paths are errors. No deletion.
class Store {
public:
    virtual ~Store()=default;
    virtual void reserve_slot(const OutputIdentity&)=0;
    virtual bool exists(const std::string& token) const=0;
    virtual std::optional<std::string> read(const std::string&,Marker) const=0;
    virtual void append(const std::string&,Marker,const std::string&)=0;
    virtual std::optional<std::uint64_t> media_size(const std::string&) const=0;
    virtual fs::path destination(const std::string&) const=0;
    virtual void flush_media(const std::string&)=0;
};
class CodecFactory {
public:
    virtual ~CodecFactory()=default;
    // Trusted codec, must only write the supplied new private destination.
    // Concurrent creation supported; all encoder work remains Worker-owned.
    virtual std::unique_ptr<Sink> make(const fs::path&,const VideoFormat&)=0;
};
enum class Status { absent, pending, incomplete, finalized_claim, review_required };
struct Inspection {
    Status status=Status::review_required;
    std::optional<OutputIdentity> claimed_identity;
    std::optional<std::uint64_t> observed_bytes;
};
// Explicit-token read-only recovery. Caller must quiesce writers before using
// this as a restart inventory. Pending never means resumable or finalized.
inline Inspection inspect(const Store& store,const std::string& token){
    if(!valid_token(token))throw std::invalid_argument("media token");
    try {
        if(!store.exists(token))return {Status::absent,{},{}};
        const auto raw=store.read(token,Marker::intent);
        const auto id=raw?decode_intent(*raw,token):std::nullopt;
        const auto bytes=store.media_size(token);
        const auto partial=store.read(token,Marker::incomplete);
        const auto final=store.read(token,Marker::finalized);
        const auto review=Inspection{Status::review_required,id,bytes};
        if(!id || (partial && final))return review;
        const auto canonical=intent(*id);
        if(partial){if(*partial!=canonical+"incomplete\n")return review;return {Status::incomplete,id,bytes};}
        if(final){
            if(!bytes || !*bytes || *bytes>id->byte_budget || *final!=canonical+"finalized\n"+std::to_string(*bytes)+"\n")return review;
            return {Status::finalized_claim,id,bytes};
        }
        return {Status::pending,id,bytes};
    }catch(const std::exception&){return {};}
}
class Lease final:public OutputLease {
    Store& store_;CodecFactory& factory_;const OutputIdentity id_;
    bool created_=false,terminal_=false;
    void require_pending() const {
        const auto view=inspect(store_,id_.token);
        if(view.status!=Status::pending || !view.claimed_identity || !(*view.claimed_identity==id_))
            throw std::runtime_error("media reservation changed");
    }
    friend class Provider;
    Lease(Store& store,CodecFactory& factory,OutputIdentity id):store_(store),factory_(factory),id_(std::move(id)){}
public:
    Lease(const Lease&)=delete;
    Lease& operator=(const Lease&)=delete;
    ~Lease() override {incomplete();}
    const OutputIdentity& identity() const noexcept override{return id_;}
    std::unique_ptr<Sink> make_writer(const VideoFormat& format) override {
        if(created_ || terminal_ || !format.valid())throw std::logic_error("media writer state");
        require_pending();if(store_.media_size(id_.token))throw std::runtime_error("media destination exists");
        created_=true;return factory_.make(store_.destination(id_.token),format);
    }
    std::uint64_t size_bytes() override{return store_.media_size(id_.token).value_or(0);}
    void seal() override {
        if(!created_ || terminal_)throw std::logic_error("media seal state");
        require_pending();const auto bytes=size_bytes();
        if(!bytes || bytes>id_.byte_budget)throw std::runtime_error("media seal size");
        store_.flush_media(id_.token);
        if(size_bytes()!=bytes)throw std::runtime_error("media size changed");
        store_.append(id_.token,Marker::finalized,intent(id_)+"finalized\n"+std::to_string(bytes)+"\n");
        terminal_=true;
    }
    void incomplete() noexcept override {
        if(terminal_)return;
        terminal_=true;
        try {
            // A failed seal may have left a marker. Preserve both; review on
            // conflict. Never modify a reservation whose intent no longer matches.
            if(store_.read(id_.token,Marker::intent)==std::optional<std::string>(intent(id_))){
                try {if(store_.media_size(id_.token))store_.flush_media(id_.token);}catch(...){/* Best effort only. */}
                store_.append(id_.token,Marker::incomplete,intent(id_)+"incomplete\n");
            }
        }catch(...){/* Pending or review-required survives; never delete media. */}
    }
};
class Provider final:public OutputProvider {
    Store& store_;CodecFactory& factory_;
public:
    Provider(Store& store,CodecFactory& factory):store_(store),factory_(factory){}
    std::unique_ptr<OutputLease> reserve(const OutputIdentity& id) override {
        if(!id.valid())throw std::invalid_argument("media identity");
        store_.reserve_slot(id); // atomic no-replace; partial reservation preserved on error
        store_.append(id.token,Marker::intent,intent(id));
        return std::unique_ptr<OutputLease>(new Lease(store_,factory_,id));
    }
};
}
