#pragma once

#include <stdint.h>
#include <stddef.h>
#include <optional>

namespace BeeGFS
{

struct FileEventReceiverNewProtocol;

FileEventReceiverNewProtocol *FileEventReceiverNewProtocolCreate(int argc, const char **argv);
void FileEventReceiverNewProtocolDestroy(FileEventReceiverNewProtocol *r);

// Tries to communicate until another event is received.
bool receive_event(FileEventReceiverNewProtocol *r);


struct Read_Event
{
   void *buffer;
   size_t size;
};

// returns reference to packet buffer allocated internally. This is only valid
// after receive_event() succeeded and is invalidated by the next receive_event().
Read_Event get_event(FileEventReceiverNewProtocol *r);

}
