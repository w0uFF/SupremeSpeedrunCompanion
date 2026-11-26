#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <iostream>

int wmain(int argc,wchar_t** argv) {
    if(argc!=2)return 2;
    HMODULE module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module){std::cerr<<"LoadLibraryExW failed "<<GetLastError()<<'\n';return 1;}
    auto address=GetProcAddress(module,"SSCInitialize");
    using Initialize=int (__cdecl*)();
    Initialize initialize=nullptr;
    static_assert(sizeof(initialize)==sizeof(address),"function pointer size");
    std::memcpy(&initialize,&address,sizeof(initialize));
    const bool success=initialize && initialize()==1 && initialize()==1;
    // This synthetic host may unload only because the candidate has no callbacks or owned state.
    const bool unloaded=FreeLibrary(module)!=FALSE;
    if(!success || !unloaded)return 1;
    std::cout<<"SSC inert x86 cdecl initializer exported and callable twice in independent host; no actual launcher or game\n";
    return 0;
}
