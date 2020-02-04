//
// Created by Zhang,Yan(ART) on 2019/10/19.
//

#include "MorphCalculater.h"
#include "../../../third_party/cgltf/cgltf.h"

namespace filament {

    void MorphCalculater::init() {
        assert(mPrimitive);
        auto* prim = mPrimitive;
        auto parseGltfData = [] (void *data, cgltf_size count) -> MorphData {
            MorphData ret;
            float* floatPtr = (float*)data;
            for (cgltf_size index = 0; index < count * 3; index = index + 3) {
                float x = floatPtr[index];
                float y = floatPtr[index + 1];
                float z = floatPtr[index + 2];
                math::float3 v(x, y, z);
                ret.push_back(v);
            }
            return ret;
        };

        for (size_t baseIndex = 0; baseIndex < prim->attributes_count; baseIndex++) {
            auto& attribute = prim->attributes[baseIndex];
            const cgltf_accessor *inputAccessor = attribute.data;
            const cgltf_size count = inputAccessor->count;
            uint8_t* bytes = (uint8_t*) inputAccessor->buffer_view->buffer->data;
            void* srcBuffer = (void*) (bytes + inputAccessor->offset + inputAccessor->buffer_view->offset);
            if (attribute.type == cgltf_attribute_type_position) {
                mBaseMorphData = parseGltfData(srcBuffer, count);
            }
        }

        const cgltf_size targetsCount = prim->targets_count;
        mWeights.resize(targetsCount);
        for (cgltf_size targetIndex = 0; targetIndex < targetsCount; targetIndex++) {
            const cgltf_morph_target &morphTarget = prim->targets[targetIndex];
            for (cgltf_size aindex = 0; aindex < morphTarget.attributes_count; aindex++) {
                const cgltf_attribute &inputAttribute = morphTarget.attributes[aindex];
                const cgltf_accessor *inputAccessor = inputAttribute.data;
                const cgltf_size count = inputAccessor->count;
                uint8_t* bytes = (uint8_t*) inputAccessor->buffer_view->buffer->data;
                void* srcBuffer = (void*) (bytes + inputAccessor->offset + inputAccessor->buffer_view->offset);
                if (inputAttribute.type == cgltf_attribute_type_normal) {
                    mNormalMorphData.push_back(parseGltfData(srcBuffer, count));
                } else if (inputAttribute.type == cgltf_attribute_type_position) {
                    mPositionMorphData.push_back(parseGltfData(srcBuffer, count));
                }
            }
        }

        //print offset data
        for (size_t i = 0; i < mPositionMorphData.size(); ++i) {
            std::cout<<"mPositionMorphData "<<i<<" points = "<<std::endl;
            for (size_t j = 0; j < mPositionMorphData[i].size(); ++j) {
                auto& point = mPositionMorphData[i][j];
                if (point.x == 0.0f && point.y == 0.0f && point.z == 0.0f) {
                    continue;
                }
                std::cout<<j<<" : "<<point.x<<", "<<point.y<<", "<<point.z<<std::endl;
            }
        }
    }

    MorphCalculater::MorphData MorphCalculater::updatePosition(const std::vector<float>& weights) {
        assert(weights.size() == mPositionMorphData.size());
        MorphData ret(mBaseMorphData.size());
        for (size_t index = 0; index < mPositionMorphData.size(); index++) {
            for (size_t pointIndex = 0; pointIndex < mPositionMorphData[0].size(); ++pointIndex) {
                ret[pointIndex] += mPositionMorphData[index][pointIndex] * weights[index];
            }
        }
        for (size_t index = 0; index < ret.size(); index++) {
            ret[index] += mBaseMorphData[index];
        }
        return ret;
    }

} // namespace filament
