//
// Created by Zhang,Yan(ART) on 2019/10/19.
//

#ifndef TNT_MORPHCALCULATER_H
#define TNT_MORPHCALCULATER_H

#include "../../../third_party/cgltf/cgltf.h"
#include <vector>
#include <math/vec3.h>

namespace filament {
class MorphCalculater {
    using MorphData = std::vector<math::float3>;
public:
    MorphCalculater() = delete;
    MorphCalculater(const cgltf_primitive* prim) : mPrimitive(prim) {}
    ~MorphCalculater() {
        mPositionMorphData.clear();
        mPositionMorphData.shrink_to_fit();
        mNormalMorphData.clear();
        mNormalMorphData.shrink_to_fit();
    }
public:
    void init();
    MorphData updatePosition(const std::vector<float>& weights);
    inline size_t getMorphCount() {return mWeights.size();}
private:
    const cgltf_primitive *mPrimitive = nullptr;
    std::vector<float> mWeights;
    std::vector<MorphData> mPositionMorphData;
    std::vector<MorphData> mNormalMorphData;
    MorphData mBaseMorphData;
};

} // namespace filament

#endif //TNT_MORPHCALCULATER_H
