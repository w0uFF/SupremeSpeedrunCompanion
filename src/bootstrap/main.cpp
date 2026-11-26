#include "src/engine_bridge/unavailable.hpp"
#include "src/modules/diagnostics/controller/controller.hpp"
#include "src/modules/diagnostics/view/text.hpp"
#include <iostream>
int main(){ supreme::UnavailableEngine source; supreme::diagnostics::Query q(source); supreme::diagnostics::Controller c(q); std::cout<<"Offline P0 diagnostic; no game access\n"<<supreme::diagnostics::render(c.refresh()); }
