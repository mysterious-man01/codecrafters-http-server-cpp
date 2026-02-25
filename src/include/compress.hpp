#include <vector>

// Compress source in gzip
std::vector<unsigned char> gzip_compress(const char* source, const int source_size);

// Decompress gzip content
std::vector<unsigned char> gzip_decompress(const char* source, const int source_size);