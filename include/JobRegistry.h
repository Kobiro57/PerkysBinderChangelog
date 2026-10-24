#pragma once

#include "JobsTypes.h"
#include "ResourceIds.h"

#include <cstdint>
#include <string>
#include <vector>

namespace jobs {

class JobRegistry {
public:
    struct Snapshot {
        int version = 0;
        std::vector<JobDefinition> jobs;
    };

    explicit JobRegistry(std::uint16_t resourceId = IDR_JOBS_REGISTRY);

    bool LoadEmbedded(Snapshot& snapshot) const;
    bool LoadFromJson(const std::string& json, Snapshot& snapshot) const;
    static bool Validate(const Snapshot& snapshot);

private:
    std::uint16_t resourceId_;
};

} // namespace jobs
