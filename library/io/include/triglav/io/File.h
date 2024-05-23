#pragma once

#include "Stream.h"
#include "Path.h"

#include <memory>
#include <string_view>
#include <vector>

namespace triglav::io {

class IFile : public ISeekableStream
{
 public:
   [[nodiscard]] virtual Result<MemorySize> file_size() = 0;
};

using IFileUPtr = std::unique_ptr<IFile>;

enum class FileOpenMode
{
   Read,
   Write,
   ReadWrite,
};

Result<IFileUPtr> open_file(const Path& path, FileOpenMode mode);
std::vector<char> read_whole_file(const Path& path);

}// namespace triglav::io