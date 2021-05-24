#pragma once

#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/string_cast.hpp>

struct UniformObject;
class Object;

enum CameraType
{
    eInvalid = 0,
    eFreeLook,
    eFirstPerson,
    eThirdPerson,
    eIsometric // MOBA / Old RTS style
};

struct CameraInitStruct
{
    // ptr to view & projection matrices
    UniformObject *viewUniformObject = nullptr;
    UniformObject *projectionUniformObject = nullptr;

    float fov = -1;
    float aspect = -1;
    float nearz = -1;
    float farz = -1;

    CameraType type = eInvalid;
    Object *target = nullptr;

    glm::vec3 position = {0.0f, 0.0f, 0.0f};
};

/*
    ------------------------------
    --      camera class        --
    ------------------------------
*/

class LogHandler;
struct Camera
{
  public:
    Camera(CameraInitStruct &);
    Camera(){};
    ~Camera();
    Object *target = nullptr;
    CameraType type = CameraType::eFreeLook;
    float zoomLevel = 10.0f;

    // -- free look --
    // movement
    glm::vec3 moveAccel = {0.0f, 0.0f, 0.0f};
    static constexpr float moveStep = 0.05f;
    static constexpr float maxMoveAccel = 0.25f; // normal speed
    // static constexpr float max_move_accel = 0.5f; // maybe speed boost
    // for freelook?
    static constexpr float moveFriction = moveStep * 0.3f;

    LogHandler *ptrLogger = nullptr;

    // -- third person --
    float pitch = 40.0f;
    float targetPitch = 40.0f;

    float orbitAngle = 0.0f;
    float targetOrbit = 0.0f;
    static constexpr float orbitSmoothness = 0.7f;
    static constexpr float pitchSmoothness = 0.7f;

    // camera zoom
    float zoomAccel = 0.0f;
    static constexpr float zoomStep = 0.25f;
    static constexpr float maxZoomLevel = 20.0f;
    static constexpr float minZoomLevel = 3.4f;
    static constexpr float zoomFriction = zoomStep * 0.125f;

    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f};
    glm::vec3 front = {0.0f, 0.0f, 1.0f};

  private:
    float aspect = 0;
    float fov = 50.0f;
    float nearz = 0.1f;
    float farz = 100.0f;

    UniformObject *viewUniformObject = nullptr;
    UniformObject *projectionUniformObject = nullptr;

    static constexpr glm::vec3 DEFAULT_UP_VECTOR = {0.0f, 1.0f, 0.0f};
    static constexpr glm::vec3 DEFAULT_DOWN_VECTOR = {0.0f, -1.0f, 0.0f};
    static constexpr glm::vec3 DEFAULT_FORWARD_VECTOR = {0.0f, 0.0f, 1.0f};
    static constexpr glm::vec3 DEFAULT_BACKWARD_VECTOR = {0.0f, 0.0f, -1.0f};
    static constexpr glm::vec3 DEFAULT_LEFT_VECTOR = {-1.0f, 0.0f, 0.0f};
    static constexpr glm::vec3 DEFAULT_RIGHT_VECTOR = {1.0f, 0.0f, 0.0f};

  public:
    bool setCameraType(CameraType p_CameraType, glm::vec3 p_Position);
    // Updates view matrix
    void update(void);

    void adjustMovement(glm::vec3 p_Delta);

    // Third person
    void adjustZoom(float p_Delta);

    void adjustPitch(float p_Delta, float p_StartPitch);
    void adjustPitch(float &p_Angle);

    void adjustOrbit(float p_Delta, float p_StartOrbit);
    void adjustOrbit(float p_Delta);

    void lerpOrbit(float p_Delta);
    void lerpPitch(float p_Delta);

    void realignOrbit(void);
    void realignOrbit(float &p_Angle);

    bool checkZoom(float p_Delta);
    // free look methods
    void getFrontFace(void);
    void getFrontFace(float p_OrbitAngle);
    void move(float p_Angle, glm::vec3 p_TargetAxii, bool p_Boost = false);
    void rotate(glm::vec3 p_Delta, float p_FrameDelta);

    void moveUp(void);
    void moveUp(bool p_IsGoFast);
    void moveDown(void);
    void moveDown(bool p_IsGoFast);
    void moveLeft(void);
    void moveRight(void);
    void moveForward(void);
    void moveBackward(void);

    void updateFreelook(void);

    void updateThirdPerson(void);

    // P_TargetAxii essentially a unit vector specifying direction
    // p_OrbitAngle is the angle you wish to transform the unit vector by
    glm::vec3 rotateVector(float p_OrbitAngle, glm::vec4 p_TargetAxii);
};
