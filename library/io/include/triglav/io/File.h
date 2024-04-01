#pragma once

#include "Stream.h"

#include <vector>
#include <string_view>
#include <memory>

namespace triglav::io {    

class IFile : public ISeekableStream {
 public:
   [[nodiscard]] virtual Result<MemorySize> file_size() = 0;
};

using IFileUPtr = std::unique_ptr<IFile>; 

enum class FileOpenMode {
    Read,
    Write,
    ReadWrite,
};

Result<IFileUPtr> open_file(std::string_view path, FileOpenMode mode);
std::vector<char> read_whole_file(std::string_view name);

}