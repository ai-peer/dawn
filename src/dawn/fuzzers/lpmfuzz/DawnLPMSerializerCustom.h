
#ifndef SRC_DAWN_FUZZERS_DAWNLPMCUSTOMSERIALIZER_H_
#define SRC_DAWN_FUZZERS_DAWNLPMCUSTOMSERIALIZER_H_

#include "dawn/fuzzers/lpmfuzz/DawnLPMObjectStore.h"
#include "dawn/fuzzers/lpmfuzz/DawnLPMSerializer_autogen.h"
#include "dawn/wire/ChunkedCommandSerializer.h"
#include "dawn/wire/ObjectType_autogen.h"

namespace dawn::wire {
void GetCustomSerializedData(
    const fuzzing::Command& command,
    dawn::wire::ChunkedCommandSerializer serializer,
    ityp::array<dawn::wire::ObjectType, dawn::wire::client::DawnLPMObjectStore, 24>& gObjectStores,
    DawnLPMObjectIdProvider& provider);
}

#endif  // SRC_DAWN_FUZZERS_DAWNLPMCUSTOMSERIALIZER_H_