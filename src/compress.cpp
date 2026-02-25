#include "compress.hpp"
#include <zlib.h>
#include <iostream>

std::vector<unsigned char> gzip_compress(const char* source, const int source_size){
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    int init = deflateInit2(&stream,
                            Z_DEFAULT_COMPRESSION,
                            Z_DEFLATED,
                            15 + 16,
                            8,
                            Z_DEFAULT_STRATEGY);
    if(init != Z_OK){
        deflateEnd(&stream);
        return {};
    }
    
    uLong cb = 1024;
    std::vector<unsigned char> out(cb);

    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(source));
    stream.avail_in = source_size;

    stream.next_out = out.data();
    stream.avail_out = out.size();

    int ret;
    do{
        ret = deflate(&stream, Z_FINISH);
    } while(ret == Z_OK);

    if(ret != Z_STREAM_END){
        std::cerr << "An error ocurred on deflate. Code: " << ret << std::endl;
        deflateEnd(&stream);
        return {};
    }

    out.resize(stream.total_out);
    deflateEnd(&stream);
    
    return out;
}

std::vector<unsigned char> gzip_decompress(const char* source, const int source_size){
    z_stream stream{};
    if(inflateInit(&stream) != Z_OK){
        inflateEnd(&stream);
        return {};
    }

    std::vector<unsigned char> out(source_size * 2);

    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(source));
    stream.avail_in = source_size;

    stream.next_out = reinterpret_cast<Bytef*>(out.data());
    stream.avail_out = out.size();

    int ret;
    do{
        ret = inflate(&stream, Z_NO_FLUSH);
    } while(ret == Z_OK);

    if(ret != Z_STREAM_END){
        std::cerr << "An error ocurred on inflate. Code: " << ret << std::endl;
        inflateEnd(&stream);
        return {};
    }

    out.resize(stream.total_out);
    inflateEnd(&stream);

    return out;
}