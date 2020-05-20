#include "DisposalGarbageCollector.h"
#include "app/App.h"
#include "program/Program.h"
#include <common/toolkit/DisposalCleaner.h>


FhgfsOpsErr deleteFile(unsigned& unlinked, Node& owner, const std::string& entryID, const bool isMirrored) {

    const auto err = DisposalCleaner::unlinkFile(owner, entryID, isMirrored);

    if (err == FhgfsOpsErr_COMMUNICATION)
        LOG(GENERAL, ERR, "Communication error", entryID, isMirrored);
    else if (err == FhgfsOpsErr_INUSE)
        LOG(GENERAL, ERR, "File in use", entryID, isMirrored);
    else if (err != FhgfsOpsErr_SUCCESS)
        LOG(GENERAL, ERR, "Error", entryID, isMirrored, err);
    else
        (unlinked)++;

    return FhgfsOpsErr_SUCCESS;
}

void handleError(Node&, FhgfsOpsErr err) {
    LOG(GENERAL, ERR, "Disposal garbage collection run failed", err);
}

void disposalGarbageCollector() {
    LOG(GENERAL, NOTICE, "Disposal garbage collection started");
    auto app = Program::getApp();

    unsigned unlinked = 0;

    const std::vector<NodeHandle> nodes = {app->getMetaNodes()->referenceNode(app->getLocalNode().getNumID())};
    DisposalCleaner dc(*app->getMetaBuddyGroupMapper(), true);
    dc.run(nodes,
           [&unlinked] (auto&& owner, auto&& entryID, auto&& isMirrored) {
              return deleteFile(unlinked, owner, entryID, isMirrored);
           },
           handleError,
           [&app] () { return app->getGcQueue()->getSelfTerminate(); }
          );

    LOG(GENERAL, NOTICE, "Disposal garbage collection finished", unlinked);

    if(const auto wait = app->getConfig()->getTuneDisposalGCPeriod()) {
        if(auto* queue = app->getGcQueue()) {
            queue->enqueue(std::chrono::seconds(wait), disposalGarbageCollector);
        }
    }
}

