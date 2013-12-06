#include "scene.h"
#include "camera.h"
#include "model.h"
#include "spotlight.h"

#include "modelmanager.h"
#include "meshmanager.h"

#include "../materials/materialmanager.h"
#include "../materials/texturemanager.h"

#include "../helpers/shaders.h"

#include <QOpenGLFunctions_4_3_Core>

/**
 * @brief Constructeur paramétré
 *
 * @param parent Objet parent
 */
Scene::Scene(QObject* parent)
    : AbstractScene(parent),
      m_camera(new Camera(this)),
      m_light(new SpotLight),
      m_v(),
      m_viewCenterFixed(false),
      m_panAngle(0.0f),
      m_tiltAngle(0.0f),
      m_time(0.0f),
      m_metersToUnits(0.05f),
      m_lightMode(PerFragmentPhong),
      m_lightModeSubroutines(LightModeCount),
      m_funcs(nullptr)
{
    // Initialisation de la position et de l'orientation de la camera
    m_camera->setPosition(QVector3D(-8.0f, 6.0f, -7.0f));
    m_camera->setViewCenter(QVector3D(0.0f, 0.0f, 0.0f));
    m_camera->setUpVector(QVector3D(0.0f, 1.0f, 0.0f));

    for(int i = 1; i < LightModeCount; i++)
    {
        m_lightModeSubroutines[i] = i;
    }
}

Scene::~Scene()
{
    delete m_camera;
    m_camera = nullptr;

    delete m_light;
    m_light = nullptr;
}

void Scene::initialize()
{
    m_funcs = m_context->versionFunctions<QOpenGLFunctions_4_3_Core>();

    if( ! m_funcs )
    {
        qFatal("Requires OpenGL >= 4.3");
        exit(1);
    }

    m_funcs->initializeOpenGLFunctions();

    // Charge, compile et link le Vertex et Fragment Shader
    prepareShaders();

    glClearColor(0.39f, 0.39f, 0.39f, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    // glFrontFace(GL_CW);
    // glCullFace(GL_FRONT);
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QOpenGLShaderProgramPtr shader = m_shader->shader();
    shader->bind();
    shader->setUniformValue("texColor", 0);
    shader->setUniformValue("texNormal", 1);

//    PointLight* pointLight = dynamic_cast<PointLight*>(m_light);
//    pointLight->setUniqueColor(QVector3D(1.0f, 1.0f, 1.0f));
//    pointLight->setLinearAttenuation(0.1f);
//    pointLight->setIntensity(2.0f);

    SpotLight* spotLight = dynamic_cast<SpotLight*>(m_light);
    spotLight->setSpecularColor(1.0f, 1.0f, 1.0f);
    spotLight->setDiffuseColor(1.0f, 1.0f, 1.0f);
    spotLight->setLinearAttenuation(0.1f);
    spotLight->setIntensity(2.0f);
    spotLight->setCutOff(20.0f);

    m_modelManager = unique_ptr<AbstractModelManager>(new ModelManager(this));

    m_materialManager = make_shared<MaterialManager>(shader);
    m_textureManager  = make_shared<TextureManager>();
    m_meshManager     = make_shared<MeshManager>();

    /*
    MODELS AVAILABLE :
        - MercedesBenzSLSAMG/sls_amg.lwo
        - blackhawk/uh60.lwo
        - US_APC/apc.lwo
        - tomcat/f14d.lwo
    */

    m_model = m_modelManager->loadModel("UH60", "assets/blackhawk/uh60.lwo", shader);
}

void Scene::update(float t)
{
    const float dt = t - m_time;
    m_time = t;

    Camera::CameraTranslationOption option = m_viewCenterFixed
                                           ? Camera::DontTranslateViewCenter
                                           : Camera::TranslateViewCenter;

    m_camera->translate(m_v * dt * m_metersToUnits, option);

    if( ! qFuzzyIsNull(m_panAngle) )
    {
        m_camera->pan(m_panAngle, QVector3D(0.0f, 1.0f, 0.0f));
        m_panAngle = 0.0f;
    }

    if ( ! qFuzzyIsNull(m_tiltAngle) )
    {
        m_camera->tilt(m_tiltAngle);
        m_tiltAngle = 0.0f;
    }
}

void Scene::render(double currentTime)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set the fragment shader light mode subroutine
    m_funcs->glUniformSubroutinesuiv(GL_FRAGMENT_SHADER, 1, &m_lightModeSubroutines[m_lightMode]);

    if(currentTime > 0)
    {
        // m_object3D.rotateY(static_cast<float>(currentTime)/0.02f);
    }

    QOpenGLShaderProgramPtr shader = m_shader->shader();

    shader->bind();
    shader->setUniformValue("modelMatrix", m_object3D.modelMatrix());
    shader->setUniformValue("viewProjectionMatrix", m_camera->viewProjectionMatrix());

    m_model->render();

//    const float scale = cosf(currentTime) * 5.0f + 5.0f;
//    PointLight* pointLight = dynamic_cast<PointLight*>(m_light);
//    pointLight->setPosition(QVector3D(scale, 5.0f, scale));
//    pointLight->render(shader);

    SpotLight* spotLight = dynamic_cast<SpotLight*>(m_light);
    spotLight->setPosition(m_camera->position());
    spotLight->setDirection(m_camera->viewCenter());
    spotLight->render(shader);

    emit renderCycleDone();
}

void Scene::resize(int width, int height)
{
    glViewport(0, 0, width, height);

    if(m_camera->projectionType() == Camera::PerspectiveProjection)
    {
        float aspect = static_cast<float>(width) / static_cast<float>(height);

        m_camera->setPerspectiveProjection(m_camera->fieldOfView(),
                                           aspect,
                                           m_camera->nearPlane(),
                                           m_camera->farPlane());
    }
    else if(m_camera->projectionType() == Camera::OrthogonalProjection)
    {
        m_camera->setOrthographicProjection(m_camera->left(),
                                            m_camera->right(),
                                            m_camera->bottom(),
                                            m_camera->top(),
                                            m_camera->nearPlane(),
                                            m_camera->farPlane());
    }
}

void Scene::prepareShaders()
{
    m_shader = ShadersPtr(new Shaders);

    m_shader->setVertexShader(":/resources/shaders/per-fragment-blinn-phong.vert");
    m_shader->setFragmentShader(":/resources/shaders/per-fragment-blinn-phong.frag");

    m_shader->shader()->link();
}

void Scene::toggleFill(bool state)
{
    if(state)
    {
        glEnable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void Scene::toggleWireframe(bool state)
{
    if(state)
    {
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
}

void Scene::togglePoints(bool state)
{
    if(state)
    {
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    }
}

void Scene::togglePhong(bool state)
{
    if(state) m_lightMode = PerFragmentPhong;
}

void Scene::toggleBlinnPhong(bool state)
{
    if(state) m_lightMode = PerFragmentBlinnPhong;
}

void Scene::toggleRimLighting(bool state)
{
    if(state) m_lightMode = RimLighting;
}

void Scene::toggleAA(bool state)
{
    (state) ? glEnable(GL_MULTISAMPLE) : glDisable(GL_MULTISAMPLE);
}

Object3D* Scene::getObject()
{
    return &m_object3D;
}

Camera* Scene::getCamera()
{
    return m_camera;
}

shared_ptr<AbstractMeshManager> Scene::meshManager()
{
    return m_meshManager;
}

shared_ptr<AbstractTextureManager> Scene::textureManager()
{
    return m_textureManager;
}

shared_ptr<AbstractMaterialManager> Scene::materialManager()
{
    return m_materialManager;
}
