#pragma once
#include "../application/query.hpp"
namespace supreme::diagnostics { class Controller { const Query& query_; public: explicit Controller(const Query& q):query_(q){} Status refresh() const {return query_.execute();} }; }
