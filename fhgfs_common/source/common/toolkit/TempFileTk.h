#ifndef COMMON_TEMPFILETK_H_
#define COMMON_TEMPFILETK_H_

#include <common/storage/StorageErrors.h>

namespace TempFileTk {

FhgfsOpsErr storeTmpAndMove(const std::string& filename, const void* contents, size_t size);

FhgfsOpsErr storeTmpAndMove(const std::string& filename, const std::vector<char>& contents);
FhgfsOpsErr storeTmpAndMove(const std::string& filename, const std::string& contents);

};

#endif /* COMMON_TEMPFILETK_H_ */
