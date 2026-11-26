#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace supreme::recorder {
// BT.601 limited-range, 2x2 chroma averaging. Alpha ignored, no scaling.
inline std::vector<std::uint8_t> bgra_to_nv12(const std::uint8_t* source,std::size_t length,
                                            std::size_t stride,std::size_t width,std::size_t height) {
    if(!source || !width || !height || width%2 || height%2 || width>4096 || height>4096 ||
       stride<width*4 || stride>length || height>length/stride)
        throw std::invalid_argument("invalid BGRA layout");
    const auto plane=width*height;
    std::vector<std::uint8_t> result(plane+plane/2);
    for(std::size_t y=0;y<height;y+=2)for(std::size_t x=0;x<width;x+=2){
        int rs=0,gs=0,bs=0;
        for(std::size_t dy=0;dy<2;++dy)for(std::size_t dx=0;dx<2;++dx){
            const auto* p=source+(y+dy)*stride+(x+dx)*4;
            const int b=p[0],g=p[1],r=p[2];
            result[(y+dy)*width+x+dx]=static_cast<std::uint8_t>((66*r+129*g+25*b+128)/256+16);
            rs+=r;gs+=g;bs+=b;
        }
        const auto uv=plane+(y/2)*width+x;
        result[uv]=static_cast<std::uint8_t>((-38*rs-74*gs+112*bs+131584)/1024);
        result[uv+1]=static_cast<std::uint8_t>((112*rs-94*gs-18*bs+131584)/1024);
    }
    return result;
}
}
