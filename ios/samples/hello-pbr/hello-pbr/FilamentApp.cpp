/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FilamentApp.h"

#include <filament/Material.h>
#include <filament/Viewport.h>
#include <filamat/MaterialBuilder.h>
#include <filament/TextureSampler.h>

#include <filameshio/MeshReader.h>

#include <utils/Path.h>

#include <image/KtxUtility.h>
#include <stb_image.h>

#include <sstream>
#include <iostream>

// This file is generated via the "Run Script" build phase and contains the mesh, material, and IBL
// textures this app uses.
#include "resources.h"

using namespace filament;
using namespace filamesh;
using namespace filamat;
using namespace utils;

static const Material* g_material;
static MeshReader::MaterialRegistry g_materialInstances;

void FilamentApp::initialize() {
#if FILAMENT_APP_USE_OPENGL
    engine = Engine::create(filament::Engine::Backend::OPENGL);
#elif FILAMENT_APP_USE_METAL
    engine = Engine::create(filament::Engine::Backend::METAL);
#endif
    swapChain = engine->createSwapChain(nativeLayer);
    renderer = engine->createRenderer();
    scene = engine->createScene();
    camera = engine->createCamera();

    filaView = engine->createView();

    Texture* normal = loadMap(engine, std::string(path + "/normal.png").c_str(), false);
    Texture* basecolor = loadMap(engine, std::string(path + "/basecolor.png").c_str());
    Texture* roughness = loadMap(engine, std::string(path + "/roughness.png").c_str(), false);
    if (!normal || !basecolor || !roughness) {
        std::cout << "can't load texture" << std::endl;
        return;
    }
    
    image::KtxBundle* iblBundle = new image::KtxBundle(RESOURCES_VENETIAN_CROSSROADS_2K_IBL_DATA,
            RESOURCES_VENETIAN_CROSSROADS_2K_IBL_SIZE);
    filament::math::float3 harmonics[9];
    iblBundle->getSphericalHarmonics(harmonics);
    app.iblTexture = image::ktx::createTexture(engine, iblBundle, false);

    image::KtxBundle* skyboxBundle = new image::KtxBundle(RESOURCES_VENETIAN_CROSSROADS_2K_SKYBOX_DATA,
            RESOURCES_VENETIAN_CROSSROADS_2K_SKYBOX_SIZE);
    app.skyboxTexture = image::ktx::createTexture(engine, skyboxBundle, false);

    app.skybox = Skybox::Builder()
        .environment(app.skyboxTexture)
        .build(*engine);

    app.indirectLight = IndirectLight::Builder()
        .reflections(app.iblTexture)
        .irradiance(3, harmonics)
        .intensity(30000)
        .build(*engine);
    scene->setIndirectLight(app.indirectLight);
    scene->setSkybox(app.skybox);

    app.sun = EntityManager::get().create();
    LightManager::Builder(LightManager::Type::SUN)
        .castShadows(true)
        // The direction is calibrated to match the IBL's sun.
        .direction({0.548267, -0.473983, -0.689016})
        .build(*engine, app.sun);
    scene->addEntity(app.sun);

    MaterialBuilder::init();
    MaterialBuilder builder = MaterialBuilder()
            .name("DefaultMaterial")
            .require(VertexAttribute::UV0)
            .parameter(MaterialBuilder::SamplerType::SAMPLER_2D, "normalMap")
            .parameter(MaterialBuilder::SamplerType::SAMPLER_2D, "basecolorMap")
            .parameter(MaterialBuilder::SamplerType::SAMPLER_2D, "roughnessMap")
            .material(R"SHADER(
                void material(inout MaterialInputs material) {
                    vec2 uv = getUV0() * 2.0;
                    material.normal = texture(materialParams_normalMap, uv).xyz * 2.0 - 1.0;
                    prepareMaterial(material);

                    vec3 baseColor = texture(materialParams_basecolorMap, uv).rgb;
                    float luma = dot(baseColor, vec3(0.2126, 0.7152, 0.0722));

                    material.baseColor.rgb = baseColor;
                    material.roughness = texture(materialParams_roughnessMap, uv).r;
                    material.sheenColor = vec3(luma) * 0.5;
                }
            )SHADER")
            .shading(Shading::CLOTH)
            .platform(MaterialBuilder::Platform::MOBILE)
            .optimization(MaterialBuilder::Optimization::NONE)
            .targetApi(MaterialBuilder::TargetApi::OPENGL);
    
    Package pkg = builder.build();
    g_material = Material::Builder().package(pkg.getData(), pkg.getSize())
            .build(*engine);
    const utils::CString defaultMaterialName("DefaultMaterial");
    g_materialInstances.registerMaterialInstance(defaultMaterialName, g_material->createInstance());

    TextureSampler sampler(TextureSampler::MinFilter::LINEAR_MIPMAP_LINEAR,
            TextureSampler::MagFilter::LINEAR, TextureSampler::WrapMode::REPEAT);
    sampler.setAnisotropy(8.0f);
    g_materialInstances.getMaterialInstance(defaultMaterialName)->setParameter("normalMap", normal, sampler);
    g_materialInstances.getMaterialInstance(defaultMaterialName)->setParameter("basecolorMap", basecolor, sampler);
    g_materialInstances.getMaterialInstance(defaultMaterialName)->setParameter("roughnessMap", roughness, sampler);
    
    utils::Path p(path + "/filacloth");
    MeshReader::Mesh mesh = MeshReader::loadMeshFromFile(engine, p, g_materialInstances);
    
//    MeshReader::Mesh mesh = MeshReader::loadMeshFromBuffer(engine, RESOURCES_MATERIAL_SPHERE_DATA,
//            nullptr, nullptr, g_materialInstances.getMaterialInstance(defaultMaterialName));
//    g_materialInstances.getMaterialInstance(defaultMaterialName)->setParameter("baseColor", RgbType::sRGB, {0.71f, 0.0f, 0.0f});

    app.renderable = mesh.renderable;
    scene->addEntity(app.renderable);
    auto& rcm = engine->getRenderableManager();
    rcm.setCastShadows(rcm.getInstance(app.renderable), true);

    filaView->setScene(scene);
    filaView->setCamera(camera);
    filaView->setViewport(Viewport(0, 0, width, height));

    const uint32_t w = filaView->getViewport().width;
    const uint32_t h = filaView->getViewport().height;
    const float aspect = (float) w / h;
    cameraManipulator.setCamera(camera);
    cameraManipulator.setViewport(w, h);
    cameraManipulator.lookAt(filament::math::double3(0, 0, 3), filament::math::double3(0, 0, 0));
    camera->setProjection(60, aspect, 0.1, 10);
}

void FilamentApp::render() {
    if (renderer->beginFrame(swapChain)) {
        renderer->render(filaView);
        renderer->endFrame();
    }
}

void FilamentApp::pan(float deltaX, float deltaY) {
    cameraManipulator.rotate(filament::math::double2(deltaX, -deltaY), 10);
}

FilamentApp::~FilamentApp() {
    engine->destroy(app.materialInstance);
    engine->destroy(app.mat);
    engine->destroy(app.indirectLight);
    engine->destroy(app.iblTexture);
    engine->destroy(app.skyboxTexture);
    engine->destroy(app.skybox);
    engine->destroy(app.renderable);
    engine->destroy(app.sun);

    engine->destroy(renderer);
    engine->destroy(scene);
    engine->destroy(filaView);
    engine->destroy(camera);
    engine->destroy(swapChain);
    engine->destroy(&engine);
}

Texture* FilamentApp::loadMap(Engine* engine, const char* name, bool sRGB) {
    Path path(name);
    if (path.exists()) {
        int w, h, n;
        unsigned char* data = stbi_load(path.getAbsolutePath().c_str(), &w, &h, &n, 3);
        if (data != nullptr) {
            Texture* map = Texture::Builder()
                    .width(uint32_t(w))
                    .height(uint32_t(h))
                    .levels(0xff)
                    .format(sRGB ? Texture::InternalFormat::SRGB8 : Texture::InternalFormat::RGB8)
                    .build(*engine);
            Texture::PixelBufferDescriptor buffer(data, size_t(w * h * 3),
                    Texture::Format::RGB, Texture::Type::UBYTE,
                    (Texture::PixelBufferDescriptor::Callback) &stbi_image_free);
            map->setImage(*engine, 0, std::move(buffer));
//            map->generateMipmaps(*engine);
            return map;
        } else {
            std::cout << "The map " << path << " could not be loaded" << std::endl;
        }
    } else {
        std::cout << "The map " << path << " does not exist" << std::endl;
    }
    return nullptr;
}
